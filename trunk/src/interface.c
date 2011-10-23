// interface.c
// LiVES
// (c) G. Finch 2003 - 2010 <salsaman@xs4all.nl>
// Released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

// TODO - use gtk_widget_showall where poss.
// and don't forget gtk_box_pack_end (doh)
// and just use label instead of labelnn, etc.

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include "main.h"
#include "callbacks.h"
#include "interface.h"
#include "merge.h"
#include "support.h"

// functions called in multitrack.c
extern void multitrack_preview_clicked  (GtkButton *, gpointer user_data);
extern void mt_change_disp_tracks_ok (GtkButton *, gpointer user_data);

static gulong arrow_id; // works around a known bug in gobject


void add_suffix_check(GtkBox *box) {

  GtkWidget *hbox;
  GtkWidget *checkbutton = gtk_check_button_new ();
  GtkWidget *eventbox=gtk_event_box_new();
  GtkWidget *label=gtk_label_new_with_mnemonic (_ ("Let LiVES set the _file extension"));

  gtk_label_set_mnemonic_widget (GTK_LABEL (label),checkbutton);

  gtk_container_add(GTK_CONTAINER(eventbox),label);
  g_signal_connect (GTK_OBJECT (eventbox), "button_press_event",
		    G_CALLBACK (label_act_toggle),
		    checkbutton);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg (eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  
  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (box), hbox, FALSE, FALSE, 10);
  gtk_container_set_border_width (GTK_CONTAINER(hbox), 10);

  gtk_box_pack_start (GTK_BOX (hbox), checkbutton, FALSE, FALSE, 10);
  gtk_box_pack_start (GTK_BOX (hbox), eventbox, FALSE, FALSE, 10);
  GTK_WIDGET_SET_FLAGS (checkbutton, GTK_CAN_DEFAULT|GTK_CAN_FOCUS);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbutton), mainw->fx1_bool);
  g_signal_connect_after (GTK_OBJECT (checkbutton), "toggled",
			  G_CALLBACK (on_boolean_toggled),
			  &mainw->fx1_bool);
  
}



GtkWidget*
create_fileselection (const gchar *title, gint preview_type, gpointer free_on_cancel)
{
  // 1 - video and audio open
  // preview type 2 is export audio
  // preview_type 3 was save_file, no longer used (deprecated)


  GtkWidget *fileselection;
  GtkWidget *ok_button;
  GtkWidget *cancel_button;

  gchar titles[256];
  g_snprintf(titles,256,"LiVES: - %s",title);

  fileselection = gtk_file_selection_new (titles);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(fileselection, GTK_STATE_NORMAL, &palette->normal_back);
  }

  gtk_container_set_border_width (GTK_CONTAINER (fileselection), 10);
  gtk_window_set_position (GTK_WINDOW (fileselection), GTK_WIN_POS_CENTER_ALWAYS);
  gtk_window_set_modal (GTK_WINDOW (fileselection), TRUE);

  if (prefs->show_gui) {
    if (mainw->multitrack==NULL) gtk_window_set_transient_for(GTK_WINDOW(fileselection),GTK_WINDOW(mainw->LiVES));
    else gtk_window_set_transient_for(GTK_WINDOW(fileselection),GTK_WINDOW(mainw->multitrack->window));
  }

  if (prefs->gui_monitor!=0) {
   gint xcen=mainw->mgeom[prefs->gui_monitor-1].x+(mainw->mgeom[prefs->gui_monitor-1].width-fileselection->allocation.width)/2;
   gint ycen=mainw->mgeom[prefs->gui_monitor-1].y+(mainw->mgeom[prefs->gui_monitor-1].height-fileselection->allocation.height)/2;
   gtk_window_move(GTK_WINDOW(fileselection),xcen,ycen);
  }

  if (prefs->fileselmax) {
    gtk_window_maximize (GTK_WINDOW(fileselection));
  }

  ok_button = GTK_FILE_SELECTION (fileselection)->ok_button;
  GTK_WIDGET_SET_FLAGS (ok_button, GTK_CAN_DEFAULT);

  cancel_button = GTK_FILE_SELECTION (fileselection)->cancel_button;
  GTK_WIDGET_SET_FLAGS (cancel_button, GTK_CAN_DEFAULT);

  gtk_widget_grab_focus (fileselection);
  
  if (preview_type==1||preview_type==2) widget_add_preview(GTK_BOX (GTK_FILE_SELECTION(fileselection)->main_vbox),GTK_BOX (GTK_FILE_SELECTION (fileselection)->action_area),GTK_BOX(GTK_FILE_SELECTION(fileselection)->main_vbox),preview_type);
  g_signal_connect (cancel_button, "clicked",G_CALLBACK (on_cancel_button1_clicked),free_on_cancel);

  gtk_widget_show_all (fileselection);
  return fileselection;
}



static GtkWidget *add_deinterlace_checkbox(GtkBox *for_deint) {
  GtkWidget *hbox=gtk_hbox_new (FALSE, 0);
  GtkWidget *checkbutton = gtk_check_button_new ();
  GtkWidget *eventbox=gtk_event_box_new();
  GtkWidget *label=gtk_label_new_with_mnemonic (_ ("Apply Deinterlace"));

  mainw->open_deint=FALSE;

  gtk_label_set_mnemonic_widget (GTK_LABEL (label),checkbutton);
  gtk_container_add(GTK_CONTAINER(eventbox),label);
  g_signal_connect (GTK_OBJECT (eventbox), "button_press_event",
		    G_CALLBACK (label_act_toggle),
		    checkbutton);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg (eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  
  gtk_box_pack_start (for_deint, hbox, FALSE, FALSE, 10);
  gtk_box_pack_start (GTK_BOX (hbox), checkbutton, FALSE, FALSE, 10);
  gtk_box_pack_start (GTK_BOX (hbox), eventbox, FALSE, FALSE, 10);
  GTK_WIDGET_SET_FLAGS (checkbutton, GTK_CAN_DEFAULT|GTK_CAN_FOCUS);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbutton), mainw->open_deint);
  g_signal_connect_after (GTK_OBJECT (checkbutton), "toggled",
			  G_CALLBACK (on_boolean_toggled),
			  &mainw->open_deint);
  gtk_tooltips_set_tip (mainw->tooltips, checkbutton,_("If this is set, frames will be deinterlaced as they are imported."), NULL);

  gtk_widget_show_all(hbox);

  return hbox;
}



void
widget_add_preview(GtkBox *for_preview, GtkBox *for_button, GtkBox *for_deint, gint preview_type) {
  // preview type 1 - video and audio, fileselector
  // preview type 2 - audio only, fileselector
  // preview type 3 - range preview


  GtkWidget *preview_button=NULL;
  GtkWidget *fs_label;

  mainw->fs_playframe = gtk_frame_new (NULL);
  mainw->fs_playarea = gtk_socket_new ();

  if (preview_type==1||preview_type==3) {

    gtk_widget_show (mainw->fs_playframe);

    gtk_container_set_border_width (GTK_CONTAINER(mainw->fs_playframe), 10);
    gtk_frame_set_shadow_type (GTK_FRAME(mainw->fs_playframe), GTK_SHADOW_IN);
    if (palette->style&STYLE_1) {
      gtk_widget_modify_bg (mainw->fs_playframe, GTK_STATE_NORMAL, &palette->normal_back);
    }

    fs_label = gtk_label_new (_ ("Preview"));
    gtk_widget_show (fs_label);
    gtk_frame_set_label_widget (GTK_FRAME (mainw->fs_playframe), fs_label);
    gtk_label_set_justify (GTK_LABEL (fs_label), GTK_JUSTIFY_RIGHT);

    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg(fs_label, GTK_STATE_NORMAL, &palette->normal_fore);
    }
    
    gtk_box_pack_start (for_preview, mainw->fs_playframe, FALSE, FALSE, 0);
    gtk_widget_set_size_request (mainw->fs_playarea, DEFAULT_FRAME_HSIZE, DEFAULT_FRAME_VSIZE);

    gtk_container_add (GTK_CONTAINER (mainw->fs_playframe), mainw->fs_playarea);

    if (palette->style&STYLE_1) {
      gtk_widget_modify_bg (mainw->fs_playarea, GTK_STATE_NORMAL, &palette->normal_back);
    }
    gtk_widget_show(mainw->fs_playarea);
   }

  if (preview_type==1) {
    preview_button = gtk_button_new_with_mnemonic (_ ("Click here to _Preview any selected video, image or audio file"));
  }
  else if (preview_type==2) {
    preview_button = gtk_button_new_with_mnemonic (_ ("Click here to _Preview any selected audio file"));
  }
  else if (preview_type==3) {
    preview_button = gtk_button_new_with_mnemonic (_ ("Click here to _Preview the video"));
  }
  gtk_widget_show (preview_button);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(preview_button, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }

  gtk_box_pack_start (for_button, preview_button, TRUE, TRUE, 0);


  if (preview_type==1||preview_type==3) {
    add_deinterlace_checkbox(for_deint);
  }

  g_signal_connect (GTK_OBJECT (preview_button), "clicked",
		    G_CALLBACK (on_fs_preview_clicked),
		    GINT_TO_POINTER (preview_type));

}



static gboolean procdets_pressed (GtkWidget *ahbox, GdkEventButton *event, gpointer user_data) {
  GtkWidget *arrow=(GtkWidget *)user_data;
  gboolean expanded=!(g_object_get_data(G_OBJECT(arrow),"expanded"));
  GtkWidget *hbox=gtk_widget_get_parent(GTK_WIDGET(arrow));

  gtk_widget_destroy(arrow);

  // remove this signal because its user_data is invalid
  if (g_signal_handler_is_connected (ahbox, arrow_id)) {
    g_signal_handler_disconnect (ahbox, arrow_id);
  }

  arrow = gtk_arrow_new (expanded?GTK_ARROW_DOWN:GTK_ARROW_RIGHT, GTK_SHADOW_OUT);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(arrow, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  arrow_id=g_signal_connect (GTK_OBJECT (ahbox), "button_press_event",
			     G_CALLBACK (procdets_pressed),
			     arrow);

  gtk_box_pack_end (GTK_BOX (hbox), arrow, FALSE, FALSE, 10);
  gtk_widget_show(arrow);

  g_object_set_data(G_OBJECT(arrow),"expanded",GINT_TO_POINTER(expanded));

  if (expanded) gtk_widget_show(cfile->proc_ptr->scrolledwindow);
  else gtk_widget_hide(cfile->proc_ptr->scrolledwindow);

  return FALSE;
}



process * create_processing (const gchar *text) {

  GtkWidget *dialog_vbox1;
  GtkWidget *vbox2;
  GtkWidget *vbox3;
  GtkWidget *dialog_action_area1;
  process *procw=(process*)(g_malloc(sizeof(process)));
  gchar tmp_label[256];

  GtkWidget *hbox;
  GtkWidget *ahbox;
  GtkWidget *label;
  GtkWidget *details_arrow;

  procw->processing = gtk_dialog_new ();
  gtk_container_set_border_width (GTK_CONTAINER (procw->processing), 10);
  gtk_window_add_accel_group (GTK_WINDOW (procw->processing), mainw->accel_group);

  gtk_window_set_resizable (GTK_WINDOW (procw->processing), FALSE);
  
  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(procw->processing, GTK_STATE_NORMAL, &palette->normal_back);
  }
  gtk_window_set_title (GTK_WINDOW (procw->processing), _("LiVES: - Processing..."));
  if (mainw->multitrack==NULL) gtk_window_set_transient_for(GTK_WINDOW(procw->processing),GTK_WINDOW(mainw->LiVES));
  else gtk_window_set_transient_for(GTK_WINDOW(procw->processing),GTK_WINDOW(mainw->multitrack->window));
  gtk_window_set_position (GTK_WINDOW (procw->processing), GTK_WIN_POS_CENTER_ALWAYS);

  dialog_vbox1 = GTK_DIALOG (procw->processing)->vbox;
  gtk_widget_show (dialog_vbox1);

  gtk_dialog_set_has_separator(GTK_DIALOG(procw->processing),FALSE);

  vbox2 = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (vbox2);
  gtk_box_pack_start (GTK_BOX (dialog_vbox1), vbox2, TRUE, TRUE, 0);

  vbox3 = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (vbox3);
  gtk_box_pack_start (GTK_BOX (vbox2), vbox3, TRUE, TRUE, 0);

  g_snprintf(tmp_label,256,"%s...\n",text);
  procw->label = gtk_label_new (tmp_label);
  gtk_widget_show (procw->label);
  gtk_widget_set_size_request (procw->label, PROG_LABEL_WIDTH, -1);

  gtk_box_pack_start (GTK_BOX (vbox3), procw->label, TRUE, TRUE, 0);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(procw->label, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  gtk_label_set_justify (GTK_LABEL (procw->label), GTK_JUSTIFY_LEFT);

  procw->progressbar = gtk_progress_bar_new ();
  gtk_widget_show (procw->progressbar);
  gtk_box_pack_start (GTK_BOX (vbox3), procw->progressbar, FALSE, FALSE, 0);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(procw->progressbar, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  if (mainw->internal_messaging&&mainw->rte!=0) {
    procw->label2 = gtk_label_new (_("\n\nPlease Wait\n\nRemember to switch off effects (ctrl-0) afterwards !"));
  }
#ifdef RT_AUDIO
  else if (mainw->jackd_read!=NULL||mainw->pulsed_read!=NULL) procw->label2 = gtk_label_new ("");
#endif
  else procw->label2=gtk_label_new (_("\nPlease Wait"));

  gtk_widget_show (procw->label2);
  gtk_widget_set_size_request (procw->label2, PROG_LABEL_WIDTH, -1);

  gtk_box_pack_start (GTK_BOX (vbox3), procw->label2, FALSE, FALSE, 0);

  gtk_label_set_justify (GTK_LABEL (procw->label2), GTK_JUSTIFY_CENTER);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(procw->label2, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  procw->label3 = gtk_label_new (NULL);
  gtk_widget_show (procw->label3);
  gtk_box_pack_start (GTK_BOX (vbox3), procw->label3, FALSE, FALSE, 0);
  gtk_widget_set_size_request (procw->label3, PROG_LABEL_WIDTH, -1);

  gtk_label_set_justify (GTK_LABEL (procw->label3), GTK_JUSTIFY_CENTER);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(procw->label3, GTK_STATE_NORMAL, &palette->normal_fore);
  }


  if (mainw->iochan!=NULL) {
    // add "show details" arrow

    ahbox=gtk_event_box_new();
    gtk_box_pack_start (GTK_BOX (vbox3), ahbox, FALSE, FALSE, 10);

    hbox = gtk_hbox_new (FALSE, 0);
    gtk_container_add (GTK_CONTAINER (ahbox), hbox);

    label=gtk_label_new("");
    gtk_box_pack_end (GTK_BOX (hbox), label, TRUE, TRUE, 10);

    details_arrow = gtk_arrow_new (GTK_ARROW_RIGHT, GTK_SHADOW_OUT);

    arrow_id=g_signal_connect (GTK_OBJECT (ahbox), "button_press_event",
			       G_CALLBACK (procdets_pressed),
			       details_arrow);

    g_object_set_data(G_OBJECT(details_arrow),"expanded",GINT_TO_POINTER(FALSE));

    label=gtk_label_new (_ ("Show details"));
    gtk_box_pack_end (GTK_BOX (hbox), label, FALSE, FALSE, 10);
    gtk_box_pack_end (GTK_BOX (hbox), details_arrow, FALSE, FALSE, 10);

    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
      gtk_widget_modify_fg(details_arrow, GTK_STATE_NORMAL, &palette->normal_fore);
      gtk_widget_modify_bg(ahbox, GTK_STATE_NORMAL, &palette->normal_back);
    }

    gtk_widget_show_all(ahbox);

    procw->scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
    gtk_widget_set_size_request (procw->scrolledwindow, PROG_LABEL_WIDTH, 200);
    gtk_box_pack_start (GTK_BOX (vbox3), procw->scrolledwindow, TRUE, TRUE, 0);
    gtk_container_add (GTK_CONTAINER (procw->scrolledwindow), (GtkWidget *)mainw->optextview);
  }


  dialog_action_area1 = GTK_DIALOG (procw->processing)->action_area;
  gtk_widget_show (dialog_action_area1);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog_action_area1), GTK_BUTTONBOX_END);

  procw->stop_button = gtk_button_new_with_mnemonic (_ ("_Enough"));
  procw->preview_button = gtk_button_new_with_mnemonic (_ ("_Preview"));

  if (cfile->nokeep) procw->pause_button = gtk_button_new_with_mnemonic (_ ("Paus_e"));
  else procw->pause_button = gtk_button_new_with_mnemonic (_ ("Pause/_Enough"));

  gtk_dialog_add_action_widget (GTK_DIALOG (procw->processing), procw->preview_button, 1);
  gtk_widget_hide(procw->preview_button);
  GTK_WIDGET_SET_FLAGS (procw->preview_button, GTK_CAN_DEFAULT);
    
  gtk_dialog_add_action_widget (GTK_DIALOG (procw->processing), procw->pause_button, 0);
  gtk_widget_hide(procw->pause_button);
  GTK_WIDGET_SET_FLAGS (procw->pause_button, GTK_CAN_DEFAULT);


  if (mainw->current_file>-1) {
    if (cfile->opening_loc
#ifdef ENABLE_JACK
	||mainw->jackd_read!=NULL
#endif
#ifdef HAVE_PULSE_AUDIO
	||mainw->pulsed_read!=NULL
#endif
	) {
      // the "enough" button for opening
      gtk_dialog_add_action_widget (GTK_DIALOG (procw->processing), procw->stop_button, 0);
      gtk_widget_show(procw->stop_button);
      GTK_WIDGET_SET_FLAGS (procw->stop_button, GTK_CAN_DEFAULT);
    }
  }
  
  procw->cancel_button = gtk_button_new_with_mnemonic (_ ("_Cancel"));
  gtk_dialog_add_action_widget (GTK_DIALOG (procw->processing), procw->cancel_button, GTK_RESPONSE_CANCEL);
  GTK_WIDGET_SET_FLAGS (procw->cancel_button, GTK_CAN_DEFAULT);

  gtk_widget_add_accelerator (procw->cancel_button, "activate", mainw->accel_group,
                              GDK_Escape, 0, 0);

  g_signal_connect (GTK_OBJECT (procw->stop_button), "clicked",
		    G_CALLBACK (on_stop_clicked),
		    NULL);

  g_signal_connect (GTK_OBJECT (procw->pause_button), "clicked",
                      G_CALLBACK (on_effects_paused),
                      NULL);

  if (mainw->multitrack!=NULL&&mainw->multitrack->is_rendering) {
    g_signal_connect (GTK_OBJECT (procw->preview_button), "clicked",
                      G_CALLBACK (multitrack_preview_clicked),
                      mainw->multitrack);
  }
  else {
    g_signal_connect (GTK_OBJECT (procw->preview_button), "clicked",
                      G_CALLBACK (on_preview_clicked),
                      NULL);
  }

  g_signal_connect (GTK_OBJECT (procw->cancel_button), "clicked",
                      G_CALLBACK (on_button3_clicked),
                      NULL);

  g_signal_connect (GTK_OBJECT (procw->processing), "delete_event",
                      G_CALLBACK (return_true),
                      NULL);


  return procw;
}



fileinfo*
create_info_window (gint audio_channels, gboolean is_mt) {
  GtkWidget *vbox5;
  GtkWidget *frame4;
  GtkWidget *fixed3;
  GtkWidget *fixed5;
  GtkWidget *fixed6;
  GtkWidget *label45;
  GtkWidget *label46;
  GtkWidget *label47;
  GtkWidget *label48;
  GtkWidget *label44;
  GtkWidget *label43;
  GtkWidget *label40;
  GtkWidget *label50;
  GtkWidget *label51;
  GtkWidget *label52;
  GtkWidget *label53;
  GtkWidget *frame5;
  GtkWidget *left;
  GtkWidget *frame6;
  GtkWidget *right;
  GtkWidget *button8;

  GtkAccelGroup *accel_group;

  fileinfo *filew=(fileinfo*)(g_malloc(sizeof(fileinfo)));

  filew->info_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_position (GTK_WINDOW (filew->info_window), GTK_WIN_POS_CENTER_ALWAYS);
  gtk_window_set_modal (GTK_WINDOW (filew->info_window), TRUE);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(filew->info_window, GTK_STATE_NORMAL, &palette->normal_back);
  }

  vbox5 = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (vbox5);
  gtk_container_add (GTK_CONTAINER (filew->info_window), vbox5);

  if (prefs->show_gui) {
    if (mainw->fs&&mainw->sep_win&&mainw->playing_file>-1) {
      gtk_window_set_transient_for(GTK_WINDOW(filew->info_window),GTK_WINDOW(mainw->play_window));
    }
    else {
      gtk_window_set_transient_for(GTK_WINDOW(filew->info_window),GTK_WINDOW(mainw->LiVES));
    }
  }

  if (cfile->frames>0||is_mt) {
    frame4 = gtk_frame_new (NULL);
    gtk_widget_set_size_request (frame4, 800, 340);
    gtk_widget_show (frame4);
    gtk_box_pack_start (GTK_BOX (vbox5), frame4, TRUE, TRUE, 0);
    if (palette->style&STYLE_1) {
      gtk_widget_modify_bg(frame4, GTK_STATE_NORMAL, &palette->normal_back);
    }

    fixed3 = gtk_fixed_new ();
    gtk_widget_show (fixed3);
    gtk_container_add (GTK_CONTAINER (frame4), fixed3);
    
    filew->textview24 = gtk_text_view_new ();
    gtk_widget_show (filew->textview24);
    gtk_fixed_put (GTK_FIXED (fixed3), filew->textview24, 180, 48);
    gtk_widget_set_size_request (filew->textview24, 180, 80);
    gtk_text_view_set_editable (GTK_TEXT_VIEW (filew->textview24), FALSE);
    gtk_text_view_set_justification (GTK_TEXT_VIEW (filew->textview24), GTK_JUSTIFY_CENTER);
    gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (filew->textview24), FALSE);
    
    filew->textview25 = gtk_text_view_new ();
    gtk_widget_show (filew->textview25);
    gtk_fixed_put (GTK_FIXED (fixed3), filew->textview25, 580, 48);
    gtk_widget_set_size_request (filew->textview25, 180, 80);
    gtk_text_view_set_editable (GTK_TEXT_VIEW (filew->textview25), FALSE);
    gtk_text_view_set_justification (GTK_TEXT_VIEW (filew->textview25), GTK_JUSTIFY_CENTER);
    gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (filew->textview25), FALSE);
    
    filew->textview26 = gtk_text_view_new ();
    gtk_widget_show (filew->textview26);
    gtk_fixed_put (GTK_FIXED (fixed3), filew->textview26, 180, 136);
    gtk_widget_set_size_request (filew->textview26, 180, 80);
    gtk_text_view_set_editable (GTK_TEXT_VIEW (filew->textview26), FALSE);
    gtk_text_view_set_justification (GTK_TEXT_VIEW (filew->textview26), GTK_JUSTIFY_CENTER);
    gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (filew->textview26), FALSE);
    
    filew->textview27 = gtk_text_view_new ();
    gtk_widget_show (filew->textview27);
    gtk_fixed_put (GTK_FIXED (fixed3), filew->textview27, 580, 136);
    gtk_widget_set_size_request (filew->textview27, 180, 80);
    gtk_text_view_set_editable (GTK_TEXT_VIEW (filew->textview27), FALSE);
    gtk_text_view_set_justification (GTK_TEXT_VIEW (filew->textview27), GTK_JUSTIFY_CENTER);
    gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (filew->textview27), FALSE);
    
    filew->textview28 = gtk_text_view_new ();
    gtk_widget_show (filew->textview28);
    gtk_fixed_put (GTK_FIXED (fixed3), filew->textview28, 580, 224);
    gtk_widget_set_size_request (filew->textview28, 180, 80);
    gtk_text_view_set_editable (GTK_TEXT_VIEW (filew->textview28), FALSE);
    gtk_text_view_set_justification (GTK_TEXT_VIEW (filew->textview28), GTK_JUSTIFY_CENTER);
    gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (filew->textview28), FALSE);
    
    filew->textview29 = gtk_text_view_new ();
    gtk_widget_show (filew->textview29);
    gtk_fixed_put (GTK_FIXED (fixed3), filew->textview29, 180, 224);
    gtk_widget_set_size_request (filew->textview29, 180, 80);
    gtk_text_view_set_editable (GTK_TEXT_VIEW (filew->textview29), FALSE);
    gtk_text_view_set_justification (GTK_TEXT_VIEW (filew->textview29), GTK_JUSTIFY_CENTER);
    gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (filew->textview29), FALSE);
    
    label45 = gtk_label_new (_("Format"));
    gtk_widget_show (label45);
    gtk_fixed_put (GTK_FIXED (fixed3), label45, 44, 64);
    gtk_widget_set_size_request (label45, 125, 20);
    gtk_label_set_justify (GTK_LABEL (label45), GTK_JUSTIFY_LEFT);
    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg(label45, GTK_STATE_NORMAL, &palette->normal_fore);
    }

    label46 = gtk_label_new (_("Frame size"));
    gtk_widget_show (label46);
    gtk_fixed_put (GTK_FIXED (fixed3), label46, 18, 152);
    gtk_widget_set_size_request (label46, 145, 20);
    gtk_label_set_justify (GTK_LABEL (label46), GTK_JUSTIFY_LEFT);
    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg(label46, GTK_STATE_NORMAL, &palette->normal_fore);
    }

    if (!is_mt) label47 = gtk_label_new (_("File size"));
    else label47 = gtk_label_new (_("Byte size"));
    gtk_widget_show (label47);
    gtk_fixed_put (GTK_FIXED (fixed3), label47, 18, 240);
    gtk_widget_set_size_request (label47, 145, 20);
    gtk_label_set_justify (GTK_LABEL (label47), GTK_JUSTIFY_LEFT);
    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg(label47, GTK_STATE_NORMAL, &palette->normal_fore);
    }

    label48 = gtk_label_new (_("Total time"));
    gtk_widget_show (label48);
    gtk_fixed_put (GTK_FIXED (fixed3), label48, 450, 240);
    gtk_widget_set_size_request (label48, 145, 20);
    gtk_label_set_justify (GTK_LABEL (label48), GTK_JUSTIFY_LEFT);
    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg(label48, GTK_STATE_NORMAL, &palette->normal_fore);
    }

    label44 = gtk_label_new (_("FPS"));
    gtk_widget_show (label44);
    gtk_fixed_put (GTK_FIXED (fixed3), label44, 476, 64);
    gtk_widget_set_size_request (label44, 120, 20);
    gtk_label_set_justify (GTK_LABEL (label44), GTK_JUSTIFY_LEFT);
    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg(label44, GTK_STATE_NORMAL, &palette->normal_fore);
    }

    if (!is_mt) label43 = gtk_label_new (_("Frames"));
    else label43 = gtk_label_new (_("Events"));
    gtk_widget_show (label43);
    gtk_fixed_put (GTK_FIXED (fixed3), label43, 476, 152);
    gtk_widget_set_size_request (label43, 125, 20);
    gtk_label_set_justify (GTK_LABEL (label43), GTK_JUSTIFY_LEFT);
    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg(label43, GTK_STATE_NORMAL, &palette->normal_fore);
    }

    label40 = gtk_label_new (_("Video"));
    gtk_widget_show (label40);
    gtk_frame_set_label_widget (GTK_FRAME (frame4), label40);
    gtk_label_set_justify (GTK_LABEL (label40), GTK_JUSTIFY_LEFT);
    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg(label40, GTK_STATE_NORMAL, &palette->normal_fore);
    }
  }

  if (audio_channels>0) {
    frame5 = gtk_frame_new (NULL);
    gtk_widget_set_size_request (frame5, 700, 140);
    gtk_widget_show (frame5);
    gtk_box_pack_start (GTK_BOX (vbox5), frame5, TRUE, TRUE, 0);
    if (palette->style&STYLE_1) {
      gtk_widget_modify_bg(frame5, GTK_STATE_NORMAL, &palette->normal_back);
    }

    if (audio_channels>1) {
      left = gtk_label_new (_("Left Audio"));
    }
    else {
      left = gtk_label_new (_("Audio"));
    }

    gtk_widget_show (left);
    gtk_frame_set_label_widget (GTK_FRAME (frame5), left);
    gtk_label_set_justify (GTK_LABEL (left), GTK_JUSTIFY_LEFT);
    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg(left, GTK_STATE_NORMAL, &palette->normal_fore);
    }

    fixed5 = gtk_fixed_new ();
    gtk_widget_show (fixed5);
    gtk_container_add (GTK_CONTAINER (frame5), fixed5);

    if (!is_mt) {
      filew->textview_ltime = gtk_text_view_new ();
      gtk_widget_show (filew->textview_ltime);
      gtk_fixed_put (GTK_FIXED (fixed5), filew->textview_ltime, 580, 16);
      gtk_widget_set_size_request (filew->textview_ltime, 180, 50);
      gtk_text_view_set_editable (GTK_TEXT_VIEW (filew->textview_ltime), FALSE);
      gtk_text_view_set_justification (GTK_TEXT_VIEW (filew->textview_ltime), GTK_JUSTIFY_CENTER);
      gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (filew->textview_ltime), FALSE);
      
      label50 = gtk_label_new (_("Total time"));
      gtk_widget_show (label50);
      gtk_fixed_put (GTK_FIXED (fixed5), label50, 450, 32);
      gtk_widget_set_size_request (label50, 145, 16);
      gtk_label_set_justify (GTK_LABEL (label50), GTK_JUSTIFY_LEFT);
      if (palette->style&STYLE_1) {
	gtk_widget_modify_fg(label50, GTK_STATE_NORMAL, &palette->normal_fore);
      }
    }

    filew->textview_lrate = gtk_text_view_new ();
    gtk_widget_show (filew->textview_lrate);
    gtk_fixed_put (GTK_FIXED (fixed5), filew->textview_lrate, 180, 16);
    gtk_widget_set_size_request (filew->textview_lrate, 180, 50);
    gtk_text_view_set_editable (GTK_TEXT_VIEW (filew->textview_lrate), FALSE);
    gtk_text_view_set_justification (GTK_TEXT_VIEW (filew->textview_lrate), GTK_JUSTIFY_CENTER);
    gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (filew->textview_lrate), FALSE);
    
    label52 = gtk_label_new (_("Rate/size"));
    gtk_widget_show (label52);
    gtk_fixed_put (GTK_FIXED (fixed5), label52, 30, 32);
    gtk_widget_set_size_request (label52, 130, 16);
    gtk_label_set_justify (GTK_LABEL (label52), GTK_JUSTIFY_LEFT);
    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg(label52, GTK_STATE_NORMAL, &palette->normal_fore);
    }


    if (audio_channels>1) {
      frame6 = gtk_frame_new (NULL);
      gtk_widget_set_size_request (frame6, 600, 100);
      gtk_widget_show (frame6);
      gtk_box_pack_start (GTK_BOX (vbox5), frame6, TRUE, TRUE, 0);
      if (palette->style&STYLE_1) {
	gtk_widget_modify_bg(frame6, GTK_STATE_NORMAL, &palette->normal_back);
      }

      fixed6 = gtk_fixed_new ();
      gtk_widget_show (fixed6);
      gtk_container_add (GTK_CONTAINER (frame6), fixed6);
      
      if (!is_mt) {
	filew->textview_rtime = gtk_text_view_new ();
	gtk_widget_show (filew->textview_rtime);
	gtk_fixed_put (GTK_FIXED (fixed6), filew->textview_rtime, 580, 16);
	gtk_widget_set_size_request (filew->textview_rtime, 180, 50);
	gtk_text_view_set_editable (GTK_TEXT_VIEW (filew->textview_rtime), FALSE);
	gtk_text_view_set_justification (GTK_TEXT_VIEW (filew->textview_rtime), GTK_JUSTIFY_CENTER);
	gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (filew->textview_rtime), FALSE);
	
	label51 = gtk_label_new (_("Total time"));
	gtk_widget_show (label51);
	gtk_fixed_put (GTK_FIXED (fixed6), label51, 450, 32);
	gtk_widget_set_size_request (label51, 145, 16);
	gtk_label_set_justify (GTK_LABEL (label51), GTK_JUSTIFY_LEFT);
	if (palette->style&STYLE_1) {
	  gtk_widget_modify_fg(label51, GTK_STATE_NORMAL, &palette->normal_fore);
	}
      }

      filew->textview_rrate = gtk_text_view_new ();
      gtk_widget_show (filew->textview_rrate);
      gtk_fixed_put (GTK_FIXED (fixed6), filew->textview_rrate, 180, 16);
      gtk_widget_set_size_request (filew->textview_rrate, 180, 50);
      gtk_text_view_set_editable (GTK_TEXT_VIEW (filew->textview_rrate), FALSE);
      gtk_text_view_set_justification (GTK_TEXT_VIEW (filew->textview_rrate), GTK_JUSTIFY_CENTER);
      gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (filew->textview_rrate), FALSE);
      
      label53 = gtk_label_new (_("Rate/size"));
      gtk_widget_show (label53);
      gtk_fixed_put (GTK_FIXED (fixed6), label53, 30, 32);
      gtk_widget_set_size_request (label53, 130, 16);
      gtk_label_set_justify (GTK_LABEL (label53), GTK_JUSTIFY_LEFT);
      if (palette->style&STYLE_1) {
	gtk_widget_modify_fg(label53, GTK_STATE_NORMAL, &palette->normal_fore);
      }
      
      right = gtk_label_new (_("Right audio"));
      gtk_widget_show (right);
      gtk_frame_set_label_widget (GTK_FRAME (frame6), right);
      gtk_label_set_justify (GTK_LABEL (right), GTK_JUSTIFY_LEFT);
      if (palette->style&STYLE_1) {
	gtk_widget_modify_fg(right, GTK_STATE_NORMAL, &palette->normal_fore);
      }
    }
  }

  button8 = gtk_button_new_from_stock ("gtk-ok");
  gtk_widget_show (button8);
  gtk_box_pack_end (GTK_BOX (vbox5), button8, FALSE, FALSE, 0);
  gtk_widget_set_size_request (button8, -1, 78);
  gtk_container_set_border_width (GTK_CONTAINER (button8), 12);
  gtk_button_set_relief (GTK_BUTTON (button8), GTK_RELIEF_NONE);
  GTK_WIDGET_SET_FLAGS (button8, GTK_CAN_DEFAULT);
  gtk_widget_grab_default (button8);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(button8, GTK_STATE_SELECTED|GTK_STATE_ACTIVE, &palette->white);
  }

  g_signal_connect (GTK_OBJECT (button8), "clicked",
                      G_CALLBACK (on_cancel_button1_clicked),
                      filew);

  g_signal_connect (GTK_OBJECT (filew->info_window), "delete_event",
                      G_CALLBACK (on_cancel_button1_clicked_del),
                      filew);

  accel_group = GTK_ACCEL_GROUP(gtk_accel_group_new ());
  gtk_window_add_accel_group (GTK_WINDOW (filew->info_window), accel_group);

  gtk_widget_add_accelerator (button8, "activate", accel_group,
                              GDK_Escape, 0, 0);


  return filew;
}





GtkWidget*
create_encoder_prep_dialog (const gchar *text1, const gchar *text2, gboolean opt_resize)
{
  GtkWidget *dialog;
  GtkWidget *dialog_vbox;
  GtkWidget *dialog_action_area;
  GtkWidget *cancelbutton;
  GtkWidget *okbutton;
  GtkWidget *checkbutton;
  GtkWidget *label;
  GtkWidget *hbox;

  dialog = gtk_dialog_new ();
  gtk_window_set_title (GTK_WINDOW (dialog), _("LiVES: - Encoding options"));
  if (palette->style&STYLE_1) {
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog),FALSE);
    gtk_widget_modify_bg(dialog, GTK_STATE_NORMAL, &palette->normal_back);
  }

  if (prefs->show_gui) {
    gtk_window_set_transient_for(GTK_WINDOW(dialog),GTK_WINDOW(mainw->LiVES));
  }

  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER_ALWAYS);
  gtk_window_set_default_size (GTK_WINDOW (dialog), 450, 300);

  dialog_vbox = GTK_DIALOG (dialog)->vbox;

  label = gtk_label_new (text1);
  gtk_box_pack_start (GTK_BOX (dialog_vbox), label, TRUE, TRUE, 0);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_CENTER);
  gtk_label_set_line_wrap (GTK_LABEL (label), FALSE);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  if (opt_resize) {      
    checkbutton = gtk_check_button_new ();
    if (text2!=NULL) label=gtk_label_new_with_mnemonic (_ ("<------------- (Check the box to resize as suggested)"));
    else label=gtk_label_new_with_mnemonic (_ ("<------------- (Check the box to use the size recommendation)"));
    gtk_label_set_mnemonic_widget (GTK_LABEL (label),checkbutton);
    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    }
    hbox = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (hbox), checkbutton, TRUE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, FALSE, 10);
    GTK_WIDGET_SET_FLAGS (checkbutton, GTK_CAN_DEFAULT|GTK_CAN_FOCUS);
    gtk_box_pack_start (GTK_BOX (dialog_vbox), hbox, FALSE, FALSE, 10);
    g_signal_connect (GTK_OBJECT (checkbutton), "toggled",
		      G_CALLBACK (on_boolean_toggled),
		      &mainw->fx1_bool);
  }
  else if (text2==NULL) mainw->fx1_bool=TRUE;

  dialog_action_area = GTK_DIALOG (dialog)->action_area;
  gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog_action_area), GTK_BUTTONBOX_END);

  if (text2!=NULL) {
    label = gtk_label_new (text2);
    gtk_box_pack_start (GTK_BOX (dialog_vbox), label, TRUE, TRUE, 0);
    gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_CENTER);
    gtk_label_set_line_wrap (GTK_LABEL (label), FALSE);
    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    }
    cancelbutton = gtk_button_new_from_stock ("gtk-cancel");
    okbutton = gtk_button_new_from_stock ("gtk-ok");
  }
  else {
    cancelbutton = gtk_button_new_with_mnemonic (_("Keep _my settings"));
    okbutton = gtk_button_new_with_mnemonic (_("Use _recommended settings"));
  }

  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), cancelbutton, GTK_RESPONSE_CANCEL);
  GTK_WIDGET_SET_FLAGS (cancelbutton, GTK_CAN_DEFAULT);

  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), okbutton, GTK_RESPONSE_OK);

  GTK_WIDGET_SET_FLAGS (okbutton, GTK_CAN_DEFAULT);
  gtk_widget_grab_default (okbutton);

  gtk_widget_show_all (dialog);
  return dialog;
}


// Information/error dialog
GtkWidget*
create_dialog3 (const gchar *text, gboolean is_blocking, gint mask) {
  GtkWidget *dialog3;
  GtkWidget *dialog_vbox3;
  GtkWidget *info_text;
  GtkWidget *dialog_action_area3;
  GtkWidget *info_ok_button;
  GtkWidget *details_button;
  GtkWidget *checkbutton;
  GtkWidget *label;
  GtkWidget *hbox;
  GtkWidget *eventbox;
  gchar *form_text;
  
  dialog3 = gtk_dialog_new ();
  gtk_window_set_title (GTK_WINDOW (dialog3), "LiVES");
  if (is_blocking) gtk_window_set_modal (GTK_WINDOW (dialog3), TRUE);
  gtk_window_set_position (GTK_WINDOW (dialog3), GTK_WIN_POS_CENTER_ALWAYS);
  gtk_window_set_default_size (GTK_WINDOW (dialog3), 350, 200);
  if (mainw!=NULL) {
    if (palette->style&STYLE_1) {
      gtk_widget_modify_bg(dialog3, GTK_STATE_NORMAL, &palette->normal_back);
      gtk_dialog_set_has_separator(GTK_DIALOG(dialog3),FALSE);
    }
  }

  dialog_vbox3 = GTK_DIALOG (dialog3)->vbox;
  gtk_widget_show (dialog_vbox3);
  
  form_text=g_strdup_printf("\n\n%s",text);

  info_text = gtk_label_new (form_text);
  g_free(form_text);

  if (mainw!=NULL&&palette->style&STYLE_1) {
    gtk_widget_modify_fg(info_text, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  gtk_label_set_selectable (GTK_LABEL (info_text), TRUE);

  gtk_widget_show (info_text);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (dialog_vbox3), hbox, FALSE, FALSE, 10);

  gtk_box_pack_start (GTK_BOX (hbox), info_text, FALSE, FALSE, 20);
  gtk_label_set_justify (GTK_LABEL (info_text), GTK_JUSTIFY_CENTER);
  gtk_label_set_line_wrap (GTK_LABEL (info_text), FALSE);
  
  if (mask>0) {
    checkbutton = gtk_check_button_new ();
    eventbox=gtk_event_box_new();
    label=gtk_label_new_with_mnemonic (_("Do _not show this warning any more\n(can be turned back on from Preferences/Warnings)"));
    gtk_container_add (GTK_CONTAINER (eventbox), label);
    g_signal_connect (GTK_OBJECT (eventbox), "button_press_event",
		      G_CALLBACK (label_act_toggle),
		      checkbutton);
    gtk_label_set_mnemonic_widget (GTK_LABEL (label),checkbutton);
    if (palette->style&STYLE_1&&mainw!=NULL) {
      gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
      gtk_widget_modify_bg (eventbox, GTK_STATE_NORMAL, &palette->normal_back);
    }
    hbox = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (dialog_vbox3), hbox, FALSE, FALSE, 10);
    gtk_box_pack_start (GTK_BOX (hbox), checkbutton, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (hbox), eventbox, FALSE, FALSE, 10);
    gtk_widget_show_all (hbox);
    GTK_WIDGET_SET_FLAGS (checkbutton, GTK_CAN_DEFAULT|GTK_CAN_FOCUS);
    g_signal_connect (GTK_OBJECT (checkbutton), "toggled",
                      G_CALLBACK (on_warn_mask_toggled),
                      GINT_TO_POINTER(mask));
  }

  dialog_action_area3 = GTK_DIALOG (dialog3)->action_area;
  gtk_widget_show (dialog_action_area3);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog_action_area3), GTK_BUTTONBOX_END);


  if (mainw->iochan!=NULL) {
    details_button = gtk_button_new_with_mnemonic(_("Show _Details"));
    gtk_widget_show (details_button);
    gtk_dialog_add_action_widget (GTK_DIALOG (dialog3), details_button, GTK_RESPONSE_YES);
    g_signal_connect (GTK_OBJECT (details_button), "clicked",
		      G_CALLBACK (on_details_button_clicked),
		      NULL);
  }
  
  info_ok_button = gtk_button_new_from_stock ("gtk-ok");
  gtk_widget_show (info_ok_button);
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog3), info_ok_button, GTK_RESPONSE_OK);


  if (mainw->iochan==NULL) {
    GTK_WIDGET_SET_FLAGS (info_ok_button, GTK_CAN_DEFAULT|GTK_CAN_FOCUS);
    gtk_widget_grab_focus (info_ok_button);
    gtk_widget_grab_default (info_ok_button);
  }

  g_signal_connect (GTK_OBJECT (info_ok_button), "clicked",
		    G_CALLBACK (on_info_ok_button_clicked2),
		    GINT_TO_POINTER (mask));

  return dialog3;
}



text_window *create_text_window (const gchar *title, const gchar *text, GtkTextBuffer *textbuffer) {
  // general text window
  GtkWidget *dialog_vbox;
  GtkWidget *scrolledwindow;
  GtkWidget *dialog_action_area;
  GtkWidget *okbutton;
  gchar *mytitle=g_strdup(title);
  gchar *mytext=NULL;
  gchar *tmp;

  if (text!=NULL) mytext=g_strdup(text);

  textwindow=g_malloc(sizeof(text_window));

  textwindow->dialog = gtk_dialog_new ();
  gtk_window_set_title (GTK_WINDOW (textwindow->dialog), (tmp=g_strconcat ("LiVES: - ",mytitle,NULL)));
  g_free(tmp);
  gtk_window_set_position (GTK_WINDOW (textwindow->dialog), GTK_WIN_POS_CENTER_ALWAYS);
  gtk_window_set_modal (GTK_WINDOW (textwindow->dialog), TRUE);
  gtk_window_set_default_size (GTK_WINDOW (textwindow->dialog), 600, 400);

  if (prefs->show_gui) {
    gtk_window_set_transient_for(GTK_WINDOW(textwindow->dialog),mainw->multitrack==NULL?GTK_WINDOW(mainw->LiVES):GTK_WINDOW(mainw->multitrack->window));
  }

  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(textwindow->dialog, GTK_STATE_NORMAL, &palette->normal_back);
    gtk_dialog_set_has_separator(GTK_DIALOG(textwindow->dialog),FALSE);
  }

  dialog_vbox = GTK_DIALOG (textwindow->dialog)->vbox;
  gtk_widget_show (dialog_vbox);

  scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_show (scrolledwindow);
  gtk_box_pack_start (GTK_BOX (dialog_vbox), scrolledwindow, TRUE, TRUE, 0);
  
  if (mainw->iochan!=NULL) {
    textwindow->textview=GTK_WIDGET(mainw->optextview);
  }
  else {
    if (textbuffer!=NULL) textwindow->textview = gtk_text_view_new_with_buffer(textbuffer);
    else textwindow->textview = gtk_text_view_new ();
    gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (textwindow->textview), GTK_WRAP_WORD);
  }

  gtk_widget_show (textwindow->textview);
  gtk_container_add (GTK_CONTAINER (scrolledwindow), textwindow->textview);

  gtk_text_view_set_editable (GTK_TEXT_VIEW (textwindow->textview), FALSE);
  gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (textwindow->textview), FALSE);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_base(textwindow->textview, GTK_STATE_NORMAL, &palette->info_base);
    gtk_widget_modify_text(textwindow->textview, GTK_STATE_NORMAL, &palette->info_text);
  }

  if (mytext!=NULL) {
    gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (textwindow->textview)), mytext, -1);
  }

  if (mytext!=NULL||mainw->iochan!=NULL) {
    dialog_action_area = GTK_DIALOG (textwindow->dialog)->action_area;
    gtk_widget_show (dialog_action_area);
    gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog_action_area), GTK_BUTTONBOX_END);

    okbutton = gtk_button_new_with_mnemonic (_("_Close Window"));
    gtk_widget_show (okbutton);

    if (mainw->iochan!=NULL) {
      GtkWidget *savebutton = gtk_button_new_with_mnemonic (_("_Save to file"));
      gtk_widget_show (savebutton);
      gtk_dialog_add_action_widget (GTK_DIALOG (textwindow->dialog), savebutton, GTK_RESPONSE_YES);
      gtk_dialog_add_action_widget (GTK_DIALOG (textwindow->dialog), okbutton, GTK_RESPONSE_OK);
    
      g_signal_connect (GTK_OBJECT (savebutton), "clicked",
			G_CALLBACK (on_save_textview_clicked),
			textwindow->textview);
    }
    else {
      gtk_dialog_add_action_widget (GTK_DIALOG (textwindow->dialog), okbutton, GTK_RESPONSE_OK);
      gtk_widget_set_size_request (okbutton, 586, 78);
      GTK_WIDGET_SET_FLAGS (okbutton, GTK_CAN_DEFAULT);
      gtk_widget_grab_default (okbutton);
      gtk_widget_grab_focus (okbutton);
      gtk_container_set_border_width (GTK_CONTAINER (okbutton), 12);
    }

    
    g_signal_connect (GTK_OBJECT (okbutton), "clicked",
		      G_CALLBACK (on_cancel_button1_clicked),
		      textwindow);
    
  }

  if (mytitle!=NULL) g_free(mytitle);
  if (mytext!=NULL) g_free(mytext);

  return textwindow;
}




void add_to_winmenu(void) {
  // TODO - change to add_to_clipmenu, move to gui.c
  GtkWidget *active_image;
  gchar *tmp;

  cfile->menuentry = gtk_image_menu_item_new_with_label(cfile->clip_type!=CLIP_TYPE_VIDEODEV?(tmp=g_path_get_basename(cfile->name)):(tmp=g_strdup(cfile->name)));
  g_free(tmp);
#ifdef HAVE_GTK_VERSION216
  gtk_image_menu_item_set_always_show_image(GTK_IMAGE_MENU_ITEM(cfile->menuentry),TRUE);
#endif
  gtk_widget_show (cfile->menuentry);
  gtk_container_add (GTK_CONTAINER (mainw->winmenu), cfile->menuentry);

  gtk_widget_set_sensitive (cfile->menuentry, TRUE);
  g_signal_connect (GTK_OBJECT (cfile->menuentry), "activate",
                      G_CALLBACK (switch_clip_activate),
                      NULL);

  if (!cfile->opening&&(cfile->clip_type==CLIP_TYPE_DISK||cfile->clip_type==CLIP_TYPE_FILE)) {
    active_image = gtk_image_new_from_stock ("gtk-cancel", GTK_ICON_SIZE_MENU);
  }
  else {
    active_image = gtk_image_new_from_stock ("gtk-yes", GTK_ICON_SIZE_MENU);
  }
  gtk_widget_show (active_image);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (cfile->menuentry), active_image);
  if (cfile->clip_type==CLIP_TYPE_DISK||cfile->clip_type==CLIP_TYPE_FILE) mainw->clips_available++;
  mainw->cliplist = g_list_append (mainw->cliplist, GINT_TO_POINTER (mainw->current_file));
  cfile->old_frames=cfile->frames;
  cfile->ratio_fps=check_for_ratio_fps(cfile->fps);
}




void
remove_from_winmenu(void) {
  gtk_container_remove(GTK_CONTAINER(mainw->winmenu), cfile->menuentry);
  gtk_widget_destroy(cfile->menuentry);
  mainw->cliplist=g_list_remove (mainw->cliplist, GINT_TO_POINTER (mainw->current_file));
  if (cfile->clip_type==CLIP_TYPE_DISK||cfile->clip_type==CLIP_TYPE_FILE) {
    mainw->clips_available--;
    if (prefs->crash_recovery) rewrite_recovery_file();
  }

}


_insertw*
create_insert_dialog (void)
{
  GtkWidget *dialog_vbox3;
  GtkWidget *hbox15;
  GtkWidget *hbox;
  GtkWidget *eventbox;
  GtkWidget *label;
  GtkObject *spinbutton_times_adj;
  GtkWidget *table2;
  GtkWidget *radiobutton1;
  GSList *radiobutton1_group = NULL;
  GSList *radiobutton2_group = NULL;
  GtkWidget *radiobutton4;
  GtkWidget *vseparator1;
  GtkWidget *vseparator2;
  GtkWidget *hseparator1;
  GtkWidget *dialog_action_area3;
  GtkWidget *cancelbutton1;
  GtkWidget *okbutton1;
  GtkAccelGroup *accel_group=GTK_ACCEL_GROUP(gtk_accel_group_new ());

  _insertw *insertw=(_insertw*)(g_malloc(sizeof(_insertw)));

  insertw->insert_dialog = gtk_dialog_new ();
  gtk_window_add_accel_group (GTK_WINDOW (insertw->insert_dialog), accel_group);
  gtk_window_set_title (GTK_WINDOW (insertw->insert_dialog), _("LiVES: - Insert"));
  gtk_window_set_position (GTK_WINDOW (insertw->insert_dialog), GTK_WIN_POS_CENTER_ALWAYS);
  gtk_window_set_modal (GTK_WINDOW (insertw->insert_dialog), TRUE);
  gtk_window_set_default_size (GTK_WINDOW (insertw->insert_dialog), 300, 200);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg (insertw->insert_dialog, GTK_STATE_NORMAL, &palette->normal_back);
  }

  if (prefs->show_gui) {
    gtk_window_set_transient_for(GTK_WINDOW(insertw->insert_dialog),GTK_WINDOW(mainw->LiVES));
  }

  dialog_vbox3 = GTK_DIALOG (insertw->insert_dialog)->vbox;
  gtk_widget_show (dialog_vbox3);

  hbox15 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox15);
  gtk_box_pack_start (GTK_BOX (dialog_vbox3), hbox15, TRUE, TRUE, 0);


  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox15), hbox, FALSE, FALSE, 10);
  label=gtk_label_new_with_mnemonic(_("_Number of times to insert"));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 10);

  spinbutton_times_adj = gtk_adjustment_new (1, 1, 10000, 1, 10, 0);
  insertw->spinbutton_times = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_times_adj), 1, 0);
  gtk_spin_button_set_update_policy (GTK_SPIN_BUTTON (insertw->spinbutton_times),GTK_UPDATE_IF_VALID);
  gtk_widget_show (insertw->spinbutton_times);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label),insertw->spinbutton_times);
  gtk_box_pack_start (GTK_BOX (hbox), insertw->spinbutton_times, TRUE, FALSE, 0);
  gtk_entry_set_activates_default (GTK_ENTRY ((GtkEntry *)&(GTK_SPIN_BUTTON (insertw->spinbutton_times)->entry)), TRUE);

  gtk_widget_show_all(hbox);


  add_fill_to_box(GTK_BOX(hbox15));

  insertw->fit_checkbutton = gtk_check_button_new ();
  eventbox=gtk_event_box_new();
  label=gtk_label_new_with_mnemonic (_("_Insert to fit audio"));
  gtk_label_set_mnemonic_widget (GTK_LABEL (label),insertw->fit_checkbutton);

  gtk_container_add(GTK_CONTAINER(eventbox),label);
  g_signal_connect (GTK_OBJECT (eventbox), "button_press_event",
		    G_CALLBACK (label_act_toggle),
		    insertw->fit_checkbutton);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg (eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox15), hbox, FALSE, FALSE, 10);

  gtk_box_pack_start (GTK_BOX (hbox), insertw->fit_checkbutton, FALSE, FALSE, 5);
  gtk_box_pack_start (GTK_BOX (hbox), eventbox, FALSE, FALSE, 5);
  GTK_WIDGET_SET_FLAGS (insertw->fit_checkbutton, GTK_CAN_DEFAULT|GTK_CAN_FOCUS);

  gtk_widget_set_sensitive(GTK_WIDGET(insertw->fit_checkbutton),cfile->achans>0&&clipboard->achans==0);

  gtk_widget_show_all(hbox);


  hseparator1 = gtk_hseparator_new ();
  gtk_widget_show (hseparator1);
  gtk_box_pack_start (GTK_BOX (dialog_vbox3), hseparator1, TRUE, TRUE, 0);

  table2 = gtk_table_new (2, 3, FALSE);
  gtk_widget_show (table2);
  gtk_box_pack_start (GTK_BOX (dialog_vbox3), table2, TRUE, TRUE, 0);
  gtk_table_set_col_spacings (GTK_TABLE (table2), 42);
  gtk_table_set_row_spacings (GTK_TABLE (table2), 10);


  hbox = gtk_hbox_new (FALSE, 0);

  radiobutton1=gtk_radio_button_new(NULL);
  gtk_radio_button_set_group (GTK_RADIO_BUTTON (radiobutton1), radiobutton1_group);
  radiobutton1_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radiobutton1));

  gtk_box_pack_start (GTK_BOX (hbox), radiobutton1, FALSE, FALSE, 10);

  label=gtk_label_new_with_mnemonic (_ ("Insert _before selection"));
  gtk_label_set_mnemonic_widget (GTK_LABEL (label),radiobutton1);

  eventbox=gtk_event_box_new();
  gtk_container_add(GTK_CONTAINER(eventbox),label);
  gtk_tooltips_set_tip (mainw->tooltips, eventbox, _("Insert clipboard before selected frames"), NULL);
  gtk_tooltips_copy(radiobutton1,eventbox);
  g_signal_connect (GTK_OBJECT (eventbox), "button_press_event",
		    G_CALLBACK (label_act_toggle),
		    radiobutton1);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg (eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  gtk_box_pack_start (GTK_BOX (hbox), eventbox, FALSE, FALSE, 10);

  gtk_widget_show_all (hbox);

  gtk_table_attach (GTK_TABLE (table2), hbox, 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  if (cfile->frames==0) {
    gtk_widget_set_sensitive (radiobutton1, FALSE);
  }



  hbox = gtk_hbox_new (FALSE, 0);

  radiobutton4=gtk_radio_button_new(NULL);
  gtk_radio_button_set_group (GTK_RADIO_BUTTON (radiobutton4), radiobutton1_group);
  radiobutton1_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radiobutton4));

  gtk_box_pack_start (GTK_BOX (hbox), radiobutton4, FALSE, FALSE, 10);

  label=gtk_label_new_with_mnemonic (_ ("Insert _after selection"));
  gtk_label_set_mnemonic_widget (GTK_LABEL (label),radiobutton4);

  eventbox=gtk_event_box_new();
  gtk_container_add(GTK_CONTAINER(eventbox),label);
  gtk_tooltips_set_tip (mainw->tooltips, eventbox, _("Insert clipboard after selected frames"), NULL);
  gtk_tooltips_copy(radiobutton4,eventbox);
  g_signal_connect (GTK_OBJECT (eventbox), "button_press_event",
		    G_CALLBACK (label_act_toggle),
		    radiobutton4);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg (eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  gtk_box_pack_start (GTK_BOX (hbox), eventbox, FALSE, FALSE, 10);

  gtk_widget_show_all (hbox);

  gtk_table_attach (GTK_TABLE (table2), hbox, 0, 1, 1, 2,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(radiobutton4),mainw->insert_after);




  hbox = gtk_hbox_new (FALSE, 0);

  insertw->with_sound=gtk_radio_button_new(NULL);
  gtk_radio_button_set_group (GTK_RADIO_BUTTON (insertw->with_sound), radiobutton2_group);
  radiobutton2_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (insertw->with_sound));

  gtk_box_pack_start (GTK_BOX (hbox), insertw->with_sound, FALSE, FALSE, 10);

  label=gtk_label_new_with_mnemonic (_ ("Insert _with sound"));
  gtk_label_set_mnemonic_widget (GTK_LABEL (label),insertw->with_sound);

  eventbox=gtk_event_box_new();
  gtk_container_add(GTK_CONTAINER(eventbox),label);
  g_signal_connect (GTK_OBJECT (eventbox), "button_press_event",
		    G_CALLBACK (label_act_toggle),
		    radiobutton1);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg (eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  gtk_box_pack_start (GTK_BOX (hbox), eventbox, FALSE, FALSE, 10);

  gtk_widget_show_all (hbox);

  gtk_table_attach (GTK_TABLE (table2), hbox, 2, 3, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);



  if (cfile->achans==0&&clipboard->achans==0) gtk_widget_set_sensitive(insertw->with_sound,FALSE);

  hbox = gtk_hbox_new (FALSE, 0);

  insertw->without_sound=gtk_radio_button_new(NULL);

  gtk_radio_button_set_group (GTK_RADIO_BUTTON (insertw->without_sound), radiobutton2_group);
  radiobutton2_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (insertw->without_sound));
  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(insertw->without_sound),!((cfile->achans>0||clipboard->achans>0)&&mainw->ccpd_with_sound));


  gtk_box_pack_start (GTK_BOX (hbox), insertw->without_sound, FALSE, FALSE, 10);

  label=gtk_label_new_with_mnemonic (_ ("Insert with_out sound"));
  gtk_label_set_mnemonic_widget (GTK_LABEL (label),insertw->without_sound);

  eventbox=gtk_event_box_new();
  gtk_container_add(GTK_CONTAINER(eventbox),label);
  g_signal_connect (GTK_OBJECT (eventbox), "button_press_event",
		    G_CALLBACK (label_act_toggle),
		    insertw->without_sound);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg (eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  gtk_box_pack_start (GTK_BOX (hbox), eventbox, FALSE, FALSE, 10);

  gtk_widget_show_all (hbox);

  gtk_table_attach (GTK_TABLE (table2), hbox, 2, 3, 1, 2,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  gtk_widget_set_sensitive (insertw->with_sound, clipboard->achans>0||cfile->achans>0);
  gtk_widget_set_sensitive (insertw->without_sound, clipboard->achans>0||cfile->achans>0);

  vseparator1 = gtk_vseparator_new ();
  gtk_widget_show (vseparator1);
  gtk_table_attach (GTK_TABLE (table2), vseparator1, 1, 2, 0, 1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (GTK_FILL), 0, 0);

  vseparator2 = gtk_vseparator_new ();
  gtk_widget_show (vseparator2);
  gtk_table_attach (GTK_TABLE (table2), vseparator2, 1, 2, 1, 2,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (GTK_FILL), 0, 0);

  dialog_action_area3 = GTK_DIALOG (insertw->insert_dialog)->action_area;
  gtk_widget_show (dialog_action_area3);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog_action_area3), GTK_BUTTONBOX_END);

  cancelbutton1 = gtk_button_new_from_stock ("gtk-cancel");
  gtk_widget_show (cancelbutton1);
  gtk_dialog_add_action_widget (GTK_DIALOG (insertw->insert_dialog), cancelbutton1, GTK_RESPONSE_CANCEL);
  GTK_WIDGET_SET_FLAGS (cancelbutton1, GTK_CAN_FOCUS);

  okbutton1 = gtk_button_new_from_stock ("gtk-ok");
  gtk_widget_show (okbutton1);
  gtk_dialog_add_action_widget (GTK_DIALOG (insertw->insert_dialog), okbutton1, GTK_RESPONSE_OK);
  GTK_WIDGET_SET_FLAGS (okbutton1, GTK_CAN_DEFAULT|GTK_CAN_FOCUS);
  gtk_widget_grab_default(okbutton1);
  gtk_widget_grab_focus(okbutton1);

  g_signal_connect (GTK_OBJECT (insertw->with_sound), "toggled",
                      G_CALLBACK (on_insertwsound_toggled),
                      NULL);
  g_signal_connect (GTK_OBJECT (radiobutton4), "toggled",
                      G_CALLBACK (on_boolean_toggled),
                      &mainw->insert_after);
  g_signal_connect (GTK_OBJECT (cancelbutton1), "clicked",
                      G_CALLBACK (on_cancel_button1_clicked),
                      insertw);
  g_signal_connect (GTK_OBJECT (okbutton1), "clicked",
                      G_CALLBACK (on_insert_activate),
                      NULL);
  g_signal_connect (GTK_OBJECT (insertw->fit_checkbutton), "toggled",
		    G_CALLBACK (on_insfitaudio_toggled),
		    NULL);
  g_signal_connect_after (GTK_OBJECT (insertw->spinbutton_times), "value_changed",
                      G_CALLBACK (on_spin_value_changed),
                      GINT_TO_POINTER (1));
  g_signal_connect (GTK_OBJECT (insertw->insert_dialog), "delete_event",
                      G_CALLBACK (return_true),
                      NULL);

  gtk_widget_add_accelerator (cancelbutton1, "activate", accel_group,
                              GDK_Escape, 0, 0);


  return insertw;
}





GtkWidget*
create_opensel_dialog (void)
{
  GtkWidget *opensel_dialog;
  GtkWidget *dialog_vbox9;
  GtkWidget *vbox15;
  GtkWidget *table5;
  GtkWidget *label46;
  GtkWidget *label47;
  GtkObject *spinbutton23_adj;
  GtkWidget *spinbutton23;
  GtkObject *spinbutton24_adj;
  GtkWidget *spinbutton24;
  GtkWidget *dialog_action_area9;
  GtkWidget *cancelbutton7;
  GtkWidget *okbutton6;

  opensel_dialog = gtk_dialog_new ();
  gtk_window_set_title (GTK_WINDOW (opensel_dialog), _("LiVES: - Open Selection"));
  gtk_window_set_position (GTK_WINDOW (opensel_dialog), GTK_WIN_POS_CENTER_ALWAYS);
  gtk_window_set_modal (GTK_WINDOW (opensel_dialog), TRUE);

  if (prefs->show_gui) {
    if (mainw->multitrack==NULL) gtk_window_set_transient_for(GTK_WINDOW(opensel_dialog),GTK_WINDOW(mainw->LiVES));
    else gtk_window_set_transient_for(GTK_WINDOW(opensel_dialog),GTK_WINDOW(mainw->multitrack->window));
  }

  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(opensel_dialog, GTK_STATE_NORMAL, &palette->normal_back);
    gtk_dialog_set_has_separator(GTK_DIALOG(opensel_dialog),FALSE);
  }

  gtk_container_set_border_width (GTK_CONTAINER (opensel_dialog), 10);
  gtk_window_set_default_size (GTK_WINDOW (opensel_dialog), 300, 200);

  dialog_vbox9 = GTK_DIALOG (opensel_dialog)->vbox;
  gtk_widget_show (dialog_vbox9);

  vbox15 = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (vbox15);
  gtk_box_pack_start (GTK_BOX (dialog_vbox9), vbox15, TRUE, TRUE, 0);

  table5 = gtk_table_new (2, 2, FALSE);
  gtk_widget_show (table5);
  gtk_box_pack_start (GTK_BOX (vbox15), table5, TRUE, TRUE, 12);

  gtk_table_set_row_spacings (GTK_TABLE (table5), 20);
  label46 = gtk_label_new (_("    Selection start time (sec)"));
  gtk_widget_show (label46);
  gtk_table_attach (GTK_TABLE (table5), label46, 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label46), 0, 0.5);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label46, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  label47 = gtk_label_new (_("    Number of frames to open"));
  gtk_widget_show (label47);
  gtk_table_attach (GTK_TABLE (table5), label47, 0, 1, 1, 2,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label47), 0, 0.5);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label47, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  spinbutton23_adj = gtk_adjustment_new (0, 0, G_MAXINT, 1, 10, 0);
  spinbutton23 = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton23_adj), 1, 2);
  gtk_widget_show (spinbutton23);
  gtk_table_attach (GTK_TABLE (table5), spinbutton23, 1, 2, 0, 1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (GTK_EXPAND), 42, 0);
  gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spinbutton23), TRUE);

  spinbutton24_adj = gtk_adjustment_new (1000, 1, G_MAXINT, 1, 10, 0);
  spinbutton24 = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton24_adj), 1, 0);
  gtk_widget_show (spinbutton24);
  gtk_table_attach (GTK_TABLE (table5), spinbutton24, 1, 2, 1, 2,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (GTK_EXPAND), 42, 0);
  gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spinbutton24), TRUE);


  dialog_action_area9 = GTK_DIALOG (opensel_dialog)->action_area;
  gtk_widget_show (dialog_action_area9);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog_action_area9), GTK_BUTTONBOX_END);

  cancelbutton7 = gtk_button_new_from_stock ("gtk-cancel");
  gtk_widget_show (cancelbutton7);
  gtk_dialog_add_action_widget (GTK_DIALOG (opensel_dialog), cancelbutton7, GTK_RESPONSE_CANCEL);
  GTK_WIDGET_SET_FLAGS (cancelbutton7, GTK_CAN_DEFAULT);

  okbutton6 = gtk_button_new_from_stock ("gtk-ok");
  gtk_widget_show (okbutton6);
  gtk_dialog_add_action_widget (GTK_DIALOG (opensel_dialog), okbutton6, GTK_RESPONSE_OK);
  GTK_WIDGET_SET_FLAGS (okbutton6, GTK_CAN_DEFAULT);
  gtk_widget_grab_default(okbutton6);

  widget_add_preview (GTK_BOX (dialog_vbox9), GTK_BOX (dialog_vbox9), GTK_BOX(dialog_vbox9), 3);

  g_signal_connect_after (GTK_OBJECT (spinbutton23), "value_changed",
                            G_CALLBACK (on_spin_value_changed),
                            GINT_TO_POINTER (1));
  g_signal_connect_after (GTK_OBJECT (spinbutton24), "value_changed",
                            G_CALLBACK (on_spin_value_changed),
                            GINT_TO_POINTER (2));
  g_signal_connect (GTK_OBJECT (cancelbutton7), "clicked",
                      G_CALLBACK (on_cancel_opensel_clicked),
                      NULL);
  g_signal_connect (GTK_OBJECT (okbutton6), "clicked",
                      G_CALLBACK (on_opensel_range_ok_clicked),
                      NULL);

   return opensel_dialog;
}




// TODO - move all prefs stuff into prefs.c


_entryw*
create_location_dialog (void)
{
  GtkWidget *dialog_vbox13;
  GtkWidget *hbox19;
  GtkWidget *label83;
  GtkWidget *dialog_action_area13;
  GtkWidget *cancelbutton11;
  GtkWidget *okbutton10;
  GtkWidget *label;
  GtkWidget *checkbutton;
  GtkWidget *eventbox;
  GtkWidget *hbox;

  _entryw *locw=(_entryw*)(g_malloc(sizeof(_entryw)));

  locw->dialog = gtk_dialog_new ();
  if (palette->style&STYLE_1) {
    gtk_dialog_set_has_separator(GTK_DIALOG(locw->dialog),FALSE);
    gtk_widget_modify_bg (locw->dialog, GTK_STATE_NORMAL, &palette->normal_back);
  }

  if (prefs->show_gui) {
    if (mainw->multitrack==NULL) gtk_window_set_transient_for(GTK_WINDOW(locw->dialog),GTK_WINDOW(mainw->LiVES));
    else gtk_window_set_transient_for(GTK_WINDOW(locw->dialog),GTK_WINDOW(mainw->multitrack->window));
  }

  gtk_window_set_position (GTK_WINDOW (locw->dialog), GTK_WIN_POS_CENTER_ALWAYS);
  gtk_window_set_modal (GTK_WINDOW (locw->dialog), TRUE);

  gtk_window_set_default_size (GTK_WINDOW (locw->dialog), 300, 200);
  
  gtk_container_set_border_width (GTK_CONTAINER (locw->dialog), 10);
  gtk_window_set_title (GTK_WINDOW (locw->dialog), _("LiVES: - Open Location"));

  dialog_vbox13 = GTK_DIALOG (locw->dialog)->vbox;
  gtk_widget_show (dialog_vbox13);

  label = gtk_label_new (_("\n\nTo open a stream, you must make sure that you have the correct libraries compiled in mplayer.\nAlso make sure you have set your bandwidth in Preferences|Streaming\n\n"));
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (dialog_vbox13), label, FALSE, FALSE, 0);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_CENTER);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg (label, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  hbox19 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox19);
  gtk_box_pack_start (GTK_BOX (dialog_vbox13), hbox19, TRUE, TRUE, 0);

  label83 = gtk_label_new (_ ("URL : "));
  gtk_widget_show (label83);
  gtk_box_pack_start (GTK_BOX (hbox19), label83, FALSE, FALSE, 0);
  gtk_label_set_justify (GTK_LABEL (label83), GTK_JUSTIFY_LEFT);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg (label83, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  locw->entry = gtk_entry_new_with_max_length (32768);
  gtk_widget_show (locw->entry);
  gtk_box_pack_start (GTK_BOX (hbox19), locw->entry, TRUE, TRUE, 0);
  gtk_entry_set_activates_default (GTK_ENTRY (locw->entry), TRUE);

  hbox=gtk_hbox_new (FALSE, 0);
  checkbutton = gtk_check_button_new ();
  eventbox=gtk_event_box_new();
  label=gtk_label_new_with_mnemonic (_("Do not send bandwidth information"));
  gtk_tooltips_set_tip (mainw->tooltips, checkbutton,_("Try this setting if you are having problems getting a stream"), NULL);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbutton),prefs->no_bandwidth);

  gtk_label_set_mnemonic_widget (GTK_LABEL (label),checkbutton);
  gtk_container_add(GTK_CONTAINER(eventbox),label);
  g_signal_connect (GTK_OBJECT (eventbox), "button_press_event",
		    G_CALLBACK (label_act_toggle),
		    checkbutton);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg (eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  
  gtk_box_pack_start (GTK_BOX(dialog_vbox13), hbox, FALSE, FALSE, 10);
  gtk_box_pack_start (GTK_BOX (hbox), checkbutton, FALSE, FALSE, 10);
  gtk_box_pack_start (GTK_BOX (hbox), eventbox, FALSE, FALSE, 10);
  GTK_WIDGET_SET_FLAGS (checkbutton, GTK_CAN_DEFAULT|GTK_CAN_FOCUS);

  g_signal_connect (GTK_OBJECT (checkbutton), "toggled",
		    G_CALLBACK (on_boolean_toggled),
		    &prefs->no_bandwidth);

  gtk_widget_show_all(hbox);

  add_deinterlace_checkbox(GTK_BOX(dialog_vbox13));

  dialog_action_area13 = GTK_DIALOG (locw->dialog)->action_area;
  gtk_widget_show (dialog_action_area13);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog_action_area13), GTK_BUTTONBOX_END);

  cancelbutton11 = gtk_button_new_from_stock ("gtk-cancel");
  gtk_widget_show (cancelbutton11);
  gtk_dialog_add_action_widget (GTK_DIALOG (locw->dialog), cancelbutton11, GTK_RESPONSE_CANCEL);
  GTK_WIDGET_SET_FLAGS (cancelbutton11, GTK_CAN_DEFAULT);

  okbutton10 = gtk_button_new_from_stock ("gtk-ok");
  gtk_widget_show (okbutton10);
  gtk_dialog_add_action_widget (GTK_DIALOG (locw->dialog), okbutton10, GTK_RESPONSE_OK);
  GTK_WIDGET_SET_FLAGS (okbutton10, GTK_CAN_DEFAULT);
  gtk_widget_grab_default (okbutton10);


  g_signal_connect (GTK_OBJECT (cancelbutton11), "clicked",
		    G_CALLBACK (on_cancel_button1_clicked),
		    locw);

  g_signal_connect (GTK_OBJECT (okbutton10), "clicked",
		    G_CALLBACK (on_location_select),
		    NULL);

  g_signal_connect (GTK_OBJECT (locw->dialog), "delete_event",
                      G_CALLBACK (return_true),
                      NULL);

  return locw;
}



_entryw* create_rename_dialog (gint type) {
  // type 1 = rename clip in menu
  // type 2 = save clip set
  // type 3 = reload clip set
  // type 4 = save clip set from mt
  // type 5 = save clip set for project export

  // type 6 = initial tempdir


  GtkWidget *dialog_vbox13;
  GtkWidget *hbox19;
  GtkWidget *label;
  GtkWidget *dialog_action_area13;
  GtkWidget *cancelbutton;
  GtkWidget *okbutton;
  GtkWidget *set_combo;
  GtkWidget *dirbutton1;
  GtkWidget *dirimage1;

  _entryw *renamew=(_entryw*)(g_malloc(sizeof(_entryw)));

  renamew->setlist=NULL;

  renamew->dialog = gtk_dialog_new ();

  gtk_window_set_position (GTK_WINDOW (renamew->dialog), GTK_WIN_POS_CENTER_ALWAYS);

  if (prefs->show_gui) {
    if (mainw->multitrack==NULL) {
      if (mainw->is_ready) {
	gtk_window_set_transient_for(GTK_WINDOW(renamew->dialog),GTK_WINDOW(mainw->LiVES));
      }
    }
    else gtk_window_set_transient_for(GTK_WINDOW(renamew->dialog),GTK_WINDOW(mainw->multitrack->window));
  }

  gtk_window_set_modal (GTK_WINDOW (renamew->dialog), TRUE);

  if (palette->style&STYLE_1) {
    gtk_dialog_set_has_separator(GTK_DIALOG(renamew->dialog),FALSE);
    gtk_widget_modify_bg (renamew->dialog, GTK_STATE_NORMAL, &palette->normal_back);
  }

  if (type!=6) gtk_window_set_default_size (GTK_WINDOW (renamew->dialog), 500, 200);

  gtk_container_set_border_width (GTK_CONTAINER (renamew->dialog), 10);

  dialog_vbox13 = GTK_DIALOG (renamew->dialog)->vbox;
  gtk_widget_show (dialog_vbox13);

  if (type==4) {
    label = gtk_label_new (_("You need to enter a name for the current clip set.\nThis will allow you reload the layout with the same clips later.\nPlease enter the set name you wish to use.\nLiVES will remind you to save the clip set later when you try to exit.\n"));
    gtk_widget_show (label);
    gtk_box_pack_start (GTK_BOX (dialog_vbox13), label, FALSE, FALSE, 0);
    gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg (label, GTK_STATE_NORMAL, &palette->normal_fore);
    }
  }

  if (type==5) {
    label = gtk_label_new (_("In order to export this project, you must enter a name for this clip set.\nThis will also be used for the project name.\n"));
    gtk_widget_show (label);
    gtk_box_pack_start (GTK_BOX (dialog_vbox13), label, FALSE, FALSE, 0);
    gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg (label, GTK_STATE_NORMAL, &palette->normal_fore);
    }
  }


  if (type==6) {
    label = gtk_label_new (_("Welcome to LiVES !\nThis startup wizard will guide you through the\ninitial install so that you can get the most from this application.\n"));
    gtk_widget_show (label);
    gtk_box_pack_start (GTK_BOX (dialog_vbox13), label, FALSE, FALSE, 0);
    gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_CENTER);
    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg (label, GTK_STATE_NORMAL, &palette->normal_fore);
    }
    label = gtk_label_new (_("\nFirst of all you need to choose a working directory for LiVES.\nThis should be a directory with plenty of disk space available.\nNote, on many systems, /tmp is cleared on shutdown, so it may be inadvisable to create it in /tmp.\n"));
    gtk_widget_show (label);
    gtk_box_pack_start (GTK_BOX (dialog_vbox13), label, FALSE, FALSE, 0);
    gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg (label, GTK_STATE_NORMAL, &palette->normal_fore);
    }
  }


  hbox19 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox19);
  if (type!=6) {
    gtk_box_pack_start (GTK_BOX (dialog_vbox13), hbox19, TRUE, TRUE, 0);
  }
  else {
    gtk_box_pack_start (GTK_BOX (dialog_vbox13), hbox19, TRUE, TRUE, 40);
  }

  if (type==1) {
    gtk_window_set_title (GTK_WINDOW (renamew->dialog), _("LiVES: - Rename Clip"));
  }
  else if (type==2||type==3||type==4||type==5) {
    gtk_window_set_title (GTK_WINDOW (renamew->dialog), _("LiVES: - Enter Set Name"));
  }
  else if (type==6) {
    gtk_window_set_title (GTK_WINDOW (renamew->dialog), _("LiVES: - Choose a Working Directory"));
  }

  if (type==1) {
    label = gtk_label_new (_("New name "));
  }
  else if (type==2||type==3||type==4||type==5) {
    label = gtk_label_new (_("Set name "));
  }
  else {
    label = gtk_label_new ("");
  }
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (hbox19), label, FALSE, FALSE, 0);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg (label, GTK_STATE_NORMAL, &palette->normal_fore);
  }


  if (type==3) {
    GtkListStore *store;
    GtkEntryCompletion *completion;
    GList *xlist;

    set_combo=gtk_combo_new();
    renamew->setlist=get_set_list(prefs->tmpdir);
    combo_set_popdown_strings(GTK_COMBO(set_combo),renamew->setlist);
    renamew->entry=(GTK_COMBO(set_combo))->entry;
    gtk_widget_show (set_combo);
    gtk_box_pack_start (GTK_BOX (hbox19), set_combo, TRUE, TRUE, 0);

    xlist=renamew->setlist;
    store = gtk_list_store_new (1, G_TYPE_STRING);

    while (xlist != NULL) {
      GtkTreeIter iter;
      gtk_list_store_append (store, &iter);
      gtk_list_store_set (store, &iter, 0, (gchar *)xlist->data, -1);
      xlist=xlist->next;
    }
    
    completion = gtk_entry_completion_new ();
    gtk_entry_completion_set_model (completion, (GtkTreeModel *)store);
    gtk_entry_completion_set_text_column (completion, 0);
    gtk_entry_completion_set_inline_completion (completion, TRUE);
    gtk_entry_completion_set_popup_set_width (completion, TRUE);
    gtk_entry_completion_set_popup_completion (completion, TRUE);
    gtk_entry_completion_set_popup_single_match(completion,FALSE);
    gtk_entry_set_completion (GTK_ENTRY (renamew->entry), completion);
  }
  else {
    renamew->entry = gtk_entry_new_with_max_length (type==6?255:128);
    if (type==2&&strlen (mainw->set_name)) {
      gtk_entry_set_text (GTK_ENTRY (renamew->entry),mainw->set_name);
    }
    if (type==6) {
      gchar *tmpdir;
      if (prefs->startup_phase==-1) tmpdir=g_strdup_printf("%s/livestmp/",capable->home_dir);
      else tmpdir=g_strdup(prefs->tmpdir);
      gtk_entry_set_text (GTK_ENTRY (renamew->entry),tmpdir);
      g_free(tmpdir);
    }
    gtk_box_pack_start (GTK_BOX (hbox19), renamew->entry, TRUE, TRUE, 0);
    gtk_widget_show (renamew->entry);
  }


  if (type==6) {
    dirbutton1 = gtk_button_new ();
    gtk_widget_show (dirbutton1);
    
    dirimage1 = gtk_image_new_from_stock ("gtk-open", GTK_ICON_SIZE_BUTTON);
    gtk_widget_show (dirimage1);

    gtk_container_add (GTK_CONTAINER (dirbutton1), dirimage1);

    gtk_box_pack_start (GTK_BOX (hbox19), dirbutton1, FALSE, TRUE, 10);
    g_signal_connect(dirbutton1, "clicked", G_CALLBACK (on_filesel_complex_clicked),renamew->entry);

  }


  gtk_entry_set_activates_default (GTK_ENTRY (renamew->entry), TRUE);

  dialog_action_area13 = GTK_DIALOG (renamew->dialog)->action_area;
  gtk_widget_show (dialog_action_area13);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog_action_area13), GTK_BUTTONBOX_END);

  cancelbutton = gtk_button_new_from_stock ("gtk-cancel");
  gtk_widget_show (cancelbutton);
  gtk_dialog_add_action_widget (GTK_DIALOG (renamew->dialog), cancelbutton, GTK_RESPONSE_CANCEL);
  GTK_WIDGET_SET_FLAGS (cancelbutton, GTK_CAN_DEFAULT);

  if (type==6) {
    okbutton = gtk_button_new_from_stock ("gtk-go-forward");
    gtk_button_set_label(GTK_BUTTON(okbutton),_("_Next"));
  }
  else okbutton = gtk_button_new_from_stock ("gtk-ok");
  gtk_widget_show (okbutton);
  gtk_dialog_add_action_widget (GTK_DIALOG (renamew->dialog), okbutton, GTK_RESPONSE_OK);
  GTK_WIDGET_SET_FLAGS (okbutton, GTK_CAN_DEFAULT);
  gtk_widget_grab_default (okbutton);


  if (type!=4&&type!=2&&type!=5) {
    g_signal_connect (GTK_OBJECT (cancelbutton), "clicked",
		      G_CALLBACK (on_cancel_button1_clicked),
		      renamew);
  }
  else {
    g_signal_connect (GTK_OBJECT (cancelbutton), "clicked",
		      G_CALLBACK (response_cancel),
		      renamew);
  }
  if (type==1) {
    g_signal_connect (GTK_OBJECT (okbutton), "clicked",
		      G_CALLBACK (on_rename_set_name),
		      NULL);
  }
  else if (type==3) {
    g_signal_connect (GTK_OBJECT (okbutton), "clicked",
		      G_CALLBACK (on_load_set_ok),
		      GINT_TO_POINTER(FALSE));
  }
  else if (type==4||type==2||type==5||type==6) {
    g_signal_connect (GTK_OBJECT (okbutton), "clicked",
		      G_CALLBACK (response_ok),
		      NULL);
  }

  g_signal_connect (GTK_OBJECT (renamew->dialog), "delete_event",
                      G_CALLBACK (return_true),
                      NULL);

  return renamew;
}


void on_liveinp_advanced_clicked (GtkButton *button, gpointer user_data) {
  tvcardw_t *tvcardw=(tvcardw_t *)(user_data);

  tvcardw->use_advanced=!tvcardw->use_advanced;

  if (tvcardw->use_advanced) {
    gtk_widget_show(tvcardw->adv_vbox);
    gtk_button_set_label(GTK_BUTTON(tvcardw->advbutton),_("Use def_aults"));
  }
  else {
    gtk_button_set_label(GTK_BUTTON(tvcardw->advbutton),_("_Advanced"));
    gtk_window_resize(GTK_WINDOW(gtk_widget_get_toplevel(tvcardw->adv_vbox)),4,40);
    gtk_widget_hide(tvcardw->adv_vbox);
  }

  gtk_widget_queue_resize(tvcardw->adv_vbox->parent);

}


static void rb_tvcarddef_toggled(GtkToggleButton *tbut, gpointer user_data) {
  tvcardw_t *tvcardw=(tvcardw_t *)(user_data);

  if (!gtk_toggle_button_get_active(tbut)) {
    gtk_widget_set_sensitive(tvcardw->spinbuttonw,TRUE);
    gtk_widget_set_sensitive(tvcardw->spinbuttonh,TRUE);
    gtk_widget_set_sensitive(tvcardw->spinbuttonf,TRUE);
  }
  else {
    gtk_widget_set_sensitive(tvcardw->spinbuttonw,FALSE);
    gtk_widget_set_sensitive(tvcardw->spinbuttonh,FALSE);
    gtk_widget_set_sensitive(tvcardw->spinbuttonf,FALSE);
  }


}


static void after_dialog_combo_changed (GtkWidget *combo, gpointer user_data) {
  GList *list=(GList *)user_data;
  gchar *etext=gtk_combo_box_get_active_text(GTK_COMBO_BOX(combo));
  mainw->fx1_val=lives_list_index(list,etext);
}


GtkWidget *create_combo_dialog (gint type, gpointer user_data) {
  // create a dialog with combo box selector

  // type 1 == 1 combo box

  GtkWidget *combo_dialog;
  GtkWidget *dialog_vbox;
  GtkWidget *label;
  GtkWidget *combo;
  GtkWidget *okbutton;
  GtkWidget *cancelbutton;
  GtkWidget *dialog_action_area;

  gchar *label_text=NULL;

  GList *list=(GList *)user_data;

  GtkAccelGroup *accel_group=GTK_ACCEL_GROUP(gtk_accel_group_new ());

  combo_dialog = gtk_dialog_new ();
  if (type==1) {
    gtk_window_set_title (GTK_WINDOW (combo_dialog), _("LiVES:- Select input device"));
  }

  gtk_window_set_position (GTK_WINDOW (combo_dialog), GTK_WIN_POS_CENTER_ALWAYS);
  gtk_window_set_modal (GTK_WINDOW (combo_dialog), TRUE);

  gtk_container_set_border_width (GTK_CONTAINER (combo_dialog), 20);
  //gtk_window_set_default_size (GTK_WINDOW (combo_dialog), 300, 200);

  if (prefs->show_gui) {
    if (type==1) {
      gtk_window_set_transient_for(GTK_WINDOW(combo_dialog),GTK_WINDOW(mainw->LiVES));
    }
    else {
      gtk_window_set_transient_for(GTK_WINDOW(combo_dialog),GTK_WINDOW(mainw->multitrack->window));
    }
  }

  if (palette->style&STYLE_1) {
    gtk_dialog_set_has_separator(GTK_DIALOG(combo_dialog),FALSE);
    gtk_widget_modify_bg (combo_dialog, GTK_STATE_NORMAL, &palette->normal_back);
  }

  dialog_vbox = GTK_DIALOG (combo_dialog)->vbox;
  gtk_widget_show (dialog_vbox);

  if (type==1) {
    label_text=g_strdup(_("Select input device:"));
  }

  label = gtk_label_new (label_text);
  if (label_text!=NULL) g_free(label_text);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg (label, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  gtk_box_pack_start (GTK_BOX (dialog_vbox), label, TRUE, TRUE, 0);
  gtk_widget_show (label);

  combo = gtk_combo_box_new_text();

  populate_combo_box(GTK_COMBO_BOX(combo), list);
  gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);

  g_signal_connect_after (G_OBJECT (combo), "changed", G_CALLBACK (after_dialog_combo_changed), list);

  gtk_box_pack_start (GTK_BOX (dialog_vbox), combo, TRUE, TRUE, 20);
  gtk_widget_show (combo);

  if (type==1) {
    add_deinterlace_checkbox(GTK_BOX(dialog_vbox));
  }

  dialog_action_area = GTK_DIALOG (combo_dialog)->action_area;
  gtk_widget_show (dialog_action_area);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog_action_area), GTK_BUTTONBOX_END);

  cancelbutton = gtk_button_new_from_stock ("gtk-cancel");
  gtk_widget_show (cancelbutton);
  gtk_dialog_add_action_widget (GTK_DIALOG (combo_dialog), cancelbutton, GTK_RESPONSE_CANCEL);
  GTK_WIDGET_SET_FLAGS (cancelbutton, GTK_CAN_DEFAULT);

  gtk_widget_add_accelerator (cancelbutton, "activate", accel_group,
                              GDK_Escape, 0, 0);

  okbutton = gtk_button_new_from_stock ("gtk-ok");
  gtk_widget_show (okbutton);
  gtk_dialog_add_action_widget (GTK_DIALOG (combo_dialog), okbutton, GTK_RESPONSE_OK);
  GTK_WIDGET_SET_FLAGS (okbutton, GTK_CAN_DEFAULT);
  gtk_widget_grab_default (okbutton);

  gtk_window_add_accel_group (GTK_WINDOW (combo_dialog), accel_group);

  return combo_dialog;


}


GtkWidget*
create_cdtrack_dialog (gint type, gpointer user_data)
{
  // general purpose dialog with label and up to 2 spinbuttons

  // type 0 = cd track
  // type 1 = dvd title/chapter/aid
  // type 2 = vcd title -- do we need chapter as well ?
  // type 3 = number of tracks in mt


  // type 4 = TV card (device and channel)
  // type 5 = fw card

  // TODO - add pref for dvd/vcd device

  // TODO - for CD make this nicer - get track names

  GtkWidget *cd_dialog;
  GtkWidget *dialog_vbox;
  GtkWidget *hbox;
  GtkWidget *hbox17;
  GtkWidget *hbox17b;
  GtkWidget *label;
  GtkWidget *label62=NULL;
  GtkWidget *label62b;
  GtkObject *spinbutton35_adj=NULL;
  GtkWidget *spinbutton35;
  GtkObject *spinbutton36_adj;
  GtkWidget *spinbutton36=NULL;
  GtkObject *spinbutton36b_adj;
  GtkWidget *spinbutton36b;
  GtkWidget *radiobutton;
  GtkWidget *eventbox;
  GtkWidget *dialog_action_area;
  GtkWidget *cancelbutton;
  GtkWidget *okbutton;
  GtkObject *spinbutton_adj;

  gchar *label_text=NULL;
  GtkAccelGroup *accel_group=GTK_ACCEL_GROUP(gtk_accel_group_new ());
  
  GSList *radiobutton_group=NULL;

  cd_dialog = gtk_dialog_new ();
  if (type==0) {
    gtk_window_set_title (GTK_WINDOW (cd_dialog), _("LiVES:- Load CD Track"));
  }
  else if (type==1) {
    gtk_window_set_title (GTK_WINDOW (cd_dialog), _("LiVES:- Select DVD Title/Chapter"));
  }
  else if (type==2) {
    gtk_window_set_title (GTK_WINDOW (cd_dialog), _("LiVES:- Select VCD Title"));
  }
  else if (type==3) {
    gtk_window_set_title (GTK_WINDOW (cd_dialog), _("LiVES:- Change Maximum Visible Tracks"));
  }
  else if (type==4||type==5) {
    gtk_window_set_title (GTK_WINDOW (cd_dialog), _("LiVES:- Device details"));
  }

  gtk_window_set_position (GTK_WINDOW (cd_dialog), GTK_WIN_POS_CENTER_ALWAYS);
  gtk_window_set_modal (GTK_WINDOW (cd_dialog), TRUE);

  gtk_container_set_border_width (GTK_CONTAINER (cd_dialog), 10);
  gtk_window_set_default_size (GTK_WINDOW (cd_dialog), 300, 200);

  if (prefs->show_gui) {
    if (type==0||type==1||type==2||type==4||type==5) {
      gtk_window_set_transient_for(GTK_WINDOW(cd_dialog),GTK_WINDOW(mainw->LiVES));
    }
    else {
      gtk_window_set_transient_for(GTK_WINDOW(cd_dialog),GTK_WINDOW(mainw->multitrack->window));
    }
  }

  if (palette->style&STYLE_1) {
    gtk_dialog_set_has_separator(GTK_DIALOG(cd_dialog),FALSE);
    gtk_widget_modify_bg (cd_dialog, GTK_STATE_NORMAL, &palette->normal_back);
  }

  dialog_vbox = GTK_DIALOG (cd_dialog)->vbox;
  gtk_widget_show (dialog_vbox);

  hbox = gtk_hbox_new (FALSE, 50);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (dialog_vbox), hbox, TRUE, TRUE, 0);

  if (type==0) {
    label_text=g_strdup_printf(_("Track to load (from %s)"),prefs->cdplay_device);
  }
  else if (type==1) {
    label_text=g_strdup(_("DVD Title"));
  }
  else if (type==2) {
    label_text=g_strdup(_("VCD Title"));
  }
  else if (type==3) {
    label_text=g_strdup(_("Maximum number of tracks to display"));
  }
  else if (type==4) {
    label_text=g_strdup(_("Device:        /dev/video"));
  }
  else if (type==5) {
    label_text=g_strdup(_("Device:        fw:"));
  }

  label = gtk_label_new (label_text);
  g_free(label_text);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg (label, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);

  if (type==0||type==1||type==2) {
    spinbutton35_adj = gtk_adjustment_new (mainw->fx1_val, 1., 256., 1., 10., 0.);
  }
  else if (type==3) {
    spinbutton35_adj = gtk_adjustment_new (mainw->fx1_val, 4., 8., 1., 1., 0.);
  }
  else if (type==4||type==5) {
    spinbutton35_adj = gtk_adjustment_new (0., 0., 7., 1., 1., 0.);
  }

  spinbutton35 = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton35_adj), 1, 0);
  gtk_widget_show (spinbutton35);
  gtk_box_pack_start (GTK_BOX (hbox), spinbutton35, FALSE, TRUE, 0);
  gtk_entry_set_activates_default (GTK_ENTRY ((GtkEntry *)&(GTK_SPIN_BUTTON (spinbutton35)->entry)), TRUE);

  add_fill_to_box(GTK_BOX(hbox));

  if (type==1||type==4) {

    hbox17 = gtk_hbox_new (FALSE, 50);

    if (type==1) spinbutton36_adj = gtk_adjustment_new (mainw->fx2_val, 1, 1024, 1, 10, 0);
    else spinbutton36_adj = gtk_adjustment_new (1, 1, 69, 1, 1, 0);

    gtk_widget_show (hbox17);
    gtk_box_pack_start (GTK_BOX (dialog_vbox), hbox17, TRUE, TRUE, 0);

    if (type==1) label62=gtk_label_new(_("Chapter  "));
    else if (type==4) label62=gtk_label_new(_("Channel  "));
    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg (label62, GTK_STATE_NORMAL, &palette->normal_fore);
    }

    gtk_widget_show (label62);
    gtk_box_pack_start (GTK_BOX (hbox17), label62, FALSE, FALSE, 0);
    gtk_label_set_justify (GTK_LABEL (label62), GTK_JUSTIFY_LEFT);

    spinbutton36 = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton36_adj), 1, 0);
    gtk_widget_show (spinbutton36);
    gtk_box_pack_start (GTK_BOX (hbox17), spinbutton36, FALSE, TRUE, 0);
    gtk_entry_set_activates_default (GTK_ENTRY ((GtkEntry *)&(GTK_SPIN_BUTTON (spinbutton36)->entry)), TRUE);


    if (type==1) {
      hbox17b = gtk_hbox_new (FALSE, 50);
      if (type==1) spinbutton36b_adj = gtk_adjustment_new (mainw->fx3_val, 128, 159, 1, 1, 0);
      spinbutton36b = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton36b_adj), 1, 0);
      gtk_entry_set_activates_default (GTK_ENTRY ((GtkEntry *)&(GTK_SPIN_BUTTON (spinbutton36b)->entry)), TRUE);
      
      gtk_widget_show (hbox17b);
      gtk_box_pack_start (GTK_BOX (dialog_vbox), hbox17b, TRUE, TRUE, 0);
      
      label62b=gtk_label_new(_("Audio ID  "));
      if (palette->style&STYLE_1) {
	gtk_widget_modify_fg (label62b, GTK_STATE_NORMAL, &palette->normal_fore);
      }
      
      gtk_widget_show (label62b);
      gtk_box_pack_start (GTK_BOX (hbox17b), label62b, FALSE, FALSE, 0);
      gtk_label_set_justify (GTK_LABEL (label62b), GTK_JUSTIFY_LEFT);
      
      gtk_widget_show (spinbutton36b);
      gtk_box_pack_start (GTK_BOX (hbox17b), spinbutton36b, FALSE, TRUE, 0);
    }
  }

  if (type==4||type==5) {
    hbox=add_deinterlace_checkbox(GTK_BOX(dialog_vbox));
    add_fill_to_box(GTK_BOX(hbox));
  }


  if (type==4) {
    GList *dlist=NULL;
    GList *olist=NULL;

    tvcardw_t *tvcardw=(tvcardw_t *)g_malloc(sizeof(tvcardw_t));
    tvcardw->use_advanced=FALSE;

    dlist=g_list_append(dlist,"autodetect");
    dlist=g_list_append(dlist,"v4l2");
    dlist=g_list_append(dlist,"v4l");
    dlist=g_list_append(dlist,"bsdbt848");
    dlist=g_list_append(dlist,"dummy");

    olist=g_list_append(olist,"autodetect");
    olist=g_list_append(olist,"yv12");
    olist=g_list_append(olist,"rgb32");
    olist=g_list_append(olist,"rgb24");
    olist=g_list_append(olist,"rgb16");
    olist=g_list_append(olist,"rgb15");
    olist=g_list_append(olist,"uyvy");
    olist=g_list_append(olist,"yuy2");
    olist=g_list_append(olist,"i420");


    gtk_box_set_spacing(GTK_BOX(dialog_vbox),20);

    hbox = gtk_hbox_new (FALSE, 50);
    gtk_widget_show (hbox);
    gtk_box_pack_start (GTK_BOX (dialog_vbox), hbox, FALSE, FALSE, 20);
    
    add_fill_to_box(GTK_BOX(hbox));

    tvcardw->advbutton = gtk_button_new_with_mnemonic (_("_Advanced"));
    gtk_widget_show (tvcardw->advbutton);
    gtk_box_pack_start (GTK_BOX (hbox), tvcardw->advbutton, TRUE, TRUE, 40);
    
    add_fill_to_box(GTK_BOX(hbox));


    tvcardw->adv_vbox = gtk_vbox_new (FALSE, 50);
    gtk_widget_show (tvcardw->adv_vbox);
    gtk_box_pack_start (GTK_BOX (dialog_vbox), tvcardw->adv_vbox, TRUE, TRUE, 20);
    

    // add input, width, height, fps, driver and outfmt


    hbox = gtk_hbox_new (FALSE, 0);
    label=gtk_label_new_with_mnemonic(_("Input number"));
    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    }
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 10);
    
    spinbutton_adj = gtk_adjustment_new (0.,0.,16.,1.,1.,0.);
    tvcardw->spinbuttoni = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_adj),1,0);
    gtk_entry_set_activates_default (GTK_ENTRY ((GtkEntry *)&(GTK_SPIN_BUTTON (tvcardw->spinbuttoni)->entry)), TRUE);
    
    gtk_label_set_mnemonic_widget (GTK_LABEL (label), tvcardw->spinbuttoni);
    gtk_box_pack_start (GTK_BOX (hbox), tvcardw->spinbuttoni, TRUE, FALSE, 0);

    gtk_box_pack_start (GTK_BOX (tvcardw->adv_vbox), hbox, TRUE, FALSE, 0);
    gtk_widget_show_all (hbox);


    hbox = gtk_hbox_new (FALSE, 0);

    tvcardw->radiobuttond = gtk_radio_button_new (NULL);
    gtk_radio_button_set_group (GTK_RADIO_BUTTON (tvcardw->radiobuttond), radiobutton_group);
    radiobutton_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (tvcardw->radiobuttond));
    
    gtk_box_pack_start (GTK_BOX (hbox), tvcardw->radiobuttond, FALSE, FALSE, 10);
    
    label=gtk_label_new_with_mnemonic (_ ("Use default width, height and FPS"));
    gtk_label_set_mnemonic_widget (GTK_LABEL (label),tvcardw->radiobuttond);
    
    eventbox=gtk_event_box_new();
    gtk_container_add(GTK_CONTAINER(eventbox),label);

    g_signal_connect (GTK_OBJECT (eventbox), "button_press_event",
		      G_CALLBACK (label_act_toggle),
		      tvcardw->radiobuttond);
    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
      gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
      gtk_widget_modify_bg (eventbox, GTK_STATE_NORMAL, &palette->normal_back);
    }
    gtk_box_pack_start (GTK_BOX (hbox), eventbox, FALSE, FALSE, 10);
    

    g_signal_connect_after (GTK_OBJECT (tvcardw->radiobuttond), "toggled",
			    G_CALLBACK (rb_tvcarddef_toggled),
			    (gpointer)tvcardw);



    gtk_box_pack_start (GTK_BOX (tvcardw->adv_vbox), hbox, TRUE, FALSE, 0);
    gtk_widget_show_all (hbox);



    hbox = gtk_hbox_new (FALSE, 0);

    radiobutton = gtk_radio_button_new (radiobutton_group);
    
    gtk_box_pack_start (GTK_BOX (hbox), radiobutton, FALSE, FALSE, 10);
    
    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg (hbox, GTK_STATE_NORMAL, &palette->normal_fore);
      gtk_widget_modify_bg (hbox, GTK_STATE_NORMAL, &palette->normal_back);
    }


    label=gtk_label_new_with_mnemonic(_("Width"));
    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    }
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 10);
    
    spinbutton_adj = gtk_adjustment_new (640.,4.,4096.,2.,2.,0.);
    tvcardw->spinbuttonw = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_adj),1,0);
    gtk_entry_set_activates_default (GTK_ENTRY ((GtkEntry *)&(GTK_SPIN_BUTTON (tvcardw->spinbuttonw)->entry)), TRUE);
    
    gtk_label_set_mnemonic_widget (GTK_LABEL (label), tvcardw->spinbuttonw);
    gtk_box_pack_start (GTK_BOX (hbox), tvcardw->spinbuttonw, TRUE, FALSE, 0);

    gtk_widget_set_sensitive(tvcardw->spinbuttonw,FALSE);

    label=gtk_label_new_with_mnemonic(_("Height"));
    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    }
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 10);
    
    spinbutton_adj = gtk_adjustment_new (480.,4.,4096.,2.,2.,0.);
    tvcardw->spinbuttonh = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_adj),1,0);
    gtk_entry_set_activates_default (GTK_ENTRY ((GtkEntry *)&(GTK_SPIN_BUTTON (tvcardw->spinbuttonh)->entry)), TRUE);
    
    gtk_label_set_mnemonic_widget (GTK_LABEL (label), tvcardw->spinbuttonh);
    gtk_box_pack_start (GTK_BOX (hbox), tvcardw->spinbuttonh, TRUE, FALSE, 0);
    gtk_widget_set_sensitive(tvcardw->spinbuttonh,FALSE);
    

    label=gtk_label_new_with_mnemonic(_("FPS"));
    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    }
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 10);
    
    spinbutton_adj = gtk_adjustment_new (25., 1., FPS_MAX, 1., 10., 0.);
    tvcardw->spinbuttonf = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_adj),1,3);
    gtk_entry_set_activates_default (GTK_ENTRY ((GtkEntry *)&(GTK_SPIN_BUTTON (tvcardw->spinbuttonf)->entry)), TRUE);
    
    gtk_label_set_mnemonic_widget (GTK_LABEL (label), tvcardw->spinbuttonf);
    gtk_box_pack_start (GTK_BOX (hbox), tvcardw->spinbuttonf, TRUE, FALSE, 0);
    gtk_widget_set_sensitive(tvcardw->spinbuttonf,FALSE);

    gtk_box_pack_start (GTK_BOX (tvcardw->adv_vbox), hbox, TRUE, FALSE, 0);
    gtk_widget_show_all (hbox);


    hbox = gtk_hbox_new (FALSE, 0);

    tvcardw->combod = gtk_combo_new ();
 
    label = gtk_label_new_with_mnemonic (_("Driver"));
    gtk_label_set_mnemonic_widget (GTK_LABEL (label),GTK_COMBO(tvcardw->combod)->entry);
 
    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    }

    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 10);
    gtk_box_pack_start (GTK_BOX (hbox), tvcardw->combod, FALSE, TRUE, 10);
    gtk_editable_set_editable (GTK_EDITABLE((GTK_COMBO (tvcardw->combod))->entry),FALSE);
   
    combo_set_popdown_strings (GTK_COMBO (tvcardw->combod), dlist);
    gtk_entry_set_activates_default(GTK_ENTRY((GTK_COMBO(tvcardw->combod))->entry),TRUE);




    tvcardw->comboo = gtk_combo_new ();
 
    label = gtk_label_new_with_mnemonic (_("Output format"));
    gtk_label_set_mnemonic_widget (GTK_LABEL (label),GTK_COMBO(tvcardw->comboo)->entry);
 
    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    }

    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 10);
    gtk_box_pack_start (GTK_BOX (hbox), tvcardw->comboo, FALSE, TRUE, 10);
    gtk_editable_set_editable (GTK_EDITABLE((GTK_COMBO (tvcardw->comboo))->entry),FALSE);
    gtk_entry_set_activates_default(GTK_ENTRY((GTK_COMBO(tvcardw->comboo))->entry),TRUE);
   
    combo_set_popdown_strings (GTK_COMBO (tvcardw->comboo), olist);


    gtk_widget_show_all (hbox);
    gtk_box_pack_start (GTK_BOX (tvcardw->adv_vbox), hbox, TRUE, FALSE, 0);








    g_signal_connect (GTK_OBJECT (tvcardw->advbutton), "clicked",
		      G_CALLBACK (on_liveinp_advanced_clicked),
		      tvcardw);


    gtk_widget_hide(tvcardw->adv_vbox);

    g_object_set_data(G_OBJECT(cd_dialog),"tvcard_data",tvcardw);

  }

  dialog_action_area = GTK_DIALOG (cd_dialog)->action_area;
  gtk_widget_show (dialog_action_area);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog_action_area), GTK_BUTTONBOX_END);

  cancelbutton = gtk_button_new_from_stock ("gtk-cancel");
  gtk_widget_show (cancelbutton);
  gtk_dialog_add_action_widget (GTK_DIALOG (cd_dialog), cancelbutton, GTK_RESPONSE_CANCEL);
  GTK_WIDGET_SET_FLAGS (cancelbutton, GTK_CAN_DEFAULT);

  gtk_widget_add_accelerator (cancelbutton, "activate", accel_group,
                              GDK_Escape, 0, 0);

  okbutton = gtk_button_new_from_stock ("gtk-ok");
  gtk_widget_show (okbutton);
  gtk_dialog_add_action_widget (GTK_DIALOG (cd_dialog), okbutton, GTK_RESPONSE_OK);
  GTK_WIDGET_SET_FLAGS (okbutton, GTK_CAN_DEFAULT);
  gtk_widget_grab_default (okbutton);
  
  g_signal_connect_after (GTK_OBJECT (spinbutton35), "value_changed",
			  G_CALLBACK (on_spin_value_changed),
			  GINT_TO_POINTER (1));

  if (type==4) {
    g_signal_connect_after (GTK_OBJECT (spinbutton36), "value_changed",
			    G_CALLBACK (on_spin_value_changed),
			    GINT_TO_POINTER (2));
  }

  if (type==1) {
    g_signal_connect_after (GTK_OBJECT (spinbutton36), "value_changed",
			    G_CALLBACK (on_spin_value_changed),
			    GINT_TO_POINTER (3));
  }

  if (type!=4&&type!=5) {
    g_signal_connect (GTK_OBJECT (cancelbutton), "clicked",
		      G_CALLBACK (on_cancel_button1_clicked),
		      NULL);
  }

  if (type==0) {
    g_signal_connect (GTK_OBJECT (okbutton), "clicked",
		      G_CALLBACK (on_load_cdtrack_ok_clicked),
		      NULL);
  }
  else if (type==1||type==2)  {
    g_signal_connect (GTK_OBJECT (okbutton), "clicked",
		      G_CALLBACK (on_load_vcd_ok_clicked),
		      GINT_TO_POINTER (type));
  }
  else if (type==3)  {
    g_signal_connect (GTK_OBJECT (okbutton), "clicked",
		      G_CALLBACK (mt_change_disp_tracks_ok),
		      user_data);
  }


  gtk_window_add_accel_group (GTK_WINDOW (cd_dialog), accel_group);


  return cd_dialog;
}



static void rb_aud_sel_pressed (GtkButton *button,
				gpointer user_data) {
  
  aud_dialog_t *audd=(aud_dialog_t *)user_data;
  audd->is_sel=!audd->is_sel;
  gtk_widget_set_sensitive(audd->time_spin,audd->is_sel);

}




aud_dialog_t *create_audfade_dialog (gint type) {
  // type 0 = fade in
  // type 1 = fade out

  GtkWidget *dialog_vbox;
  GtkWidget *hbox;
  GtkWidget *rb_time;
  GtkWidget *rb_sel;
  GtkWidget *label61;
  GtkWidget *label62;
  GtkObject *spinbutton35_adj;
  GtkWidget *dialog_action_area11;
  GtkWidget *cancelbutton9;
  GtkWidget *okbutton8;
  gchar *label_text=NULL,*label_text2=NULL;
  gdouble max;
  GSList *radiobutton_group = NULL;

  aud_dialog_t *audd=(aud_dialog_t *)g_malloc(sizeof(aud_dialog_t));

  audd->is_sel=FALSE;

  audd->dialog = gtk_dialog_new ();
  if (type==0) {
    gtk_window_set_title (GTK_WINDOW (audd->dialog), _("LiVES:- Fade Audio In"));
  }
  else if (type==1) {
    gtk_window_set_title (GTK_WINDOW (audd->dialog), _("LiVES:- Fade Audio Out"));
  }

  gtk_window_set_position (GTK_WINDOW (audd->dialog), GTK_WIN_POS_CENTER_ALWAYS);
  gtk_window_set_modal (GTK_WINDOW (audd->dialog), TRUE);

  gtk_container_set_border_width (GTK_CONTAINER (audd->dialog), 10);
  gtk_window_set_default_size (GTK_WINDOW (audd->dialog), 300, 200);

  if (prefs->show_gui) {
    gtk_window_set_transient_for(GTK_WINDOW(audd->dialog),GTK_WINDOW(mainw->LiVES));
  }

  if (palette->style&STYLE_1) {
    gtk_dialog_set_has_separator(GTK_DIALOG(audd->dialog),FALSE);
    gtk_widget_modify_bg (audd->dialog, GTK_STATE_NORMAL, &palette->normal_back);
  }

  dialog_vbox = GTK_DIALOG (audd->dialog)->vbox;
  gtk_widget_show (dialog_vbox);

  hbox = gtk_hbox_new (FALSE, 50);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (dialog_vbox), hbox, TRUE, TRUE, 0);

  if (type==0) {
    label_text=g_strdup(_("Fade in over  "));
    label_text2=g_strdup(_("first"));
  }
  else if (type==1) {
    label_text=g_strdup(_("Fade out over  "));
    label_text2=g_strdup(_("last"));
  }

  label61 = gtk_label_new (label_text);
  if (label_text!=NULL) g_free(label_text);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg (label61, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  gtk_widget_show (label61);
  gtk_box_pack_start (GTK_BOX (hbox), label61, FALSE, FALSE, 0);
  gtk_label_set_justify (GTK_LABEL (label61), GTK_JUSTIFY_LEFT);

  rb_time=gtk_radio_button_new(radiobutton_group);
  gtk_widget_show (rb_time);
  radiobutton_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (rb_time));

  gtk_box_pack_start (GTK_BOX (hbox), rb_time, FALSE, TRUE, 0);

  label61 = gtk_label_new (label_text2);
  if (label_text2!=NULL) g_free(label_text2);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg (label61, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  gtk_widget_show (label61);
  gtk_box_pack_start (GTK_BOX (hbox), label61, FALSE, FALSE, 0);
  gtk_label_set_justify (GTK_LABEL (label61), GTK_JUSTIFY_LEFT);

  max=cfile->laudio_time/2.;
  if (max>MAX_FADE_SECS) max=MAX_FADE_SECS;

  spinbutton35_adj = gtk_adjustment_new (10, .1, max, 1, 10, 0);
  audd->time_spin = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton35_adj), 1, 1);
  gtk_widget_show (audd->time_spin);
  gtk_box_pack_start (GTK_BOX (hbox), audd->time_spin, FALSE, TRUE, 0);

  label62=gtk_label_new(_("seconds."));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg (label62, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  gtk_widget_show (label62);
  gtk_box_pack_start (GTK_BOX (hbox), label62, FALSE, FALSE, 10);
  gtk_label_set_justify (GTK_LABEL (label62), GTK_JUSTIFY_LEFT);

  hbox = gtk_hbox_new (FALSE, 50);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (dialog_vbox), hbox, TRUE, TRUE, 0);

  add_fill_to_box(GTK_BOX(hbox));


  rb_sel=gtk_radio_button_new(radiobutton_group);
  gtk_widget_show (rb_sel);

  gtk_box_pack_start (GTK_BOX (hbox), rb_sel, FALSE, TRUE, 0);

  g_signal_connect_after (GTK_OBJECT (rb_sel), "toggled",
			  G_CALLBACK (rb_aud_sel_pressed),
			  (gpointer)audd);

  label62=gtk_label_new(_("selection."));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg (label62, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  gtk_widget_show (label62);
  gtk_box_pack_start (GTK_BOX (hbox), label62, FALSE, FALSE, 10);
  gtk_label_set_justify (GTK_LABEL (label62), GTK_JUSTIFY_LEFT);

  add_fill_to_box(GTK_BOX(hbox));

  dialog_action_area11 = GTK_DIALOG (audd->dialog)->action_area;
  gtk_widget_show (dialog_action_area11);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog_action_area11), GTK_BUTTONBOX_END);

  cancelbutton9 = gtk_button_new_from_stock ("gtk-cancel");
  gtk_widget_show (cancelbutton9);
  gtk_dialog_add_action_widget (GTK_DIALOG (audd->dialog), cancelbutton9, GTK_RESPONSE_CANCEL);
  GTK_WIDGET_SET_FLAGS (cancelbutton9, GTK_CAN_DEFAULT);

  okbutton8 = gtk_button_new_from_stock ("gtk-ok");
  gtk_widget_show (okbutton8);
  gtk_dialog_add_action_widget (GTK_DIALOG (audd->dialog), okbutton8, GTK_RESPONSE_OK);
  GTK_WIDGET_SET_FLAGS (okbutton8, GTK_CAN_DEFAULT);
  gtk_widget_grab_default (okbutton8);


  if ((cfile->end-1.)/cfile->fps>cfile->laudio_time) {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(rb_sel),FALSE);
    gtk_widget_set_sensitive(rb_sel,FALSE);
  }
  
  return audd;
}




_xranw *
create_rp_dialog (void)
{
  GtkWidget *dialog_vbox14;
  GtkWidget *vbox19;
  GtkWidget *hbox21;
  GtkWidget *label89;
  GtkObject *spinbutton50_adj;
  GtkWidget *hbox20;
  GtkWidget *label88;
  GtkWidget *button13;
  GtkWidget *image413;
  GtkWidget *hbox22;
  GtkWidget *eventbox;
  GtkWidget *hbox;
  GtkWidget *label;
  GtkWidget *label90;
  GtkObject *spinbutton51_adj;
  GtkWidget *hbox23;
  GtkWidget *label91;
  GtkObject *spinbutton52_adj;
  GtkWidget *dialog_action_area14;
  GtkWidget *cancelbutton12;
  GtkWidget *okbutton11;

  _xranw *xranw=(_xranw*)(g_malloc(sizeof(_xranw)));

  xranw->rp_dialog = gtk_dialog_new ();
  gtk_window_set_title (GTK_WINDOW (xranw->rp_dialog), _("LiVES: - Play Random Music"));

  dialog_vbox14 = GTK_DIALOG (xranw->rp_dialog)->vbox;
  gtk_widget_show (dialog_vbox14);

  if (prefs->show_gui) {
    gtk_window_set_transient_for(GTK_WINDOW(xranw->rp_dialog),GTK_WINDOW(mainw->LiVES));
  }

  if (palette->style&STYLE_1) {
    gtk_dialog_set_has_separator(GTK_DIALOG(xranw->rp_dialog),FALSE);
    gtk_widget_modify_bg (xranw->rp_dialog, GTK_STATE_NORMAL, &palette->normal_back);
  }

  gtk_window_set_position (GTK_WINDOW (xranw->rp_dialog), GTK_WIN_POS_CENTER_ALWAYS);
  gtk_window_set_modal (GTK_WINDOW (xranw->rp_dialog), TRUE);

  gtk_container_set_border_width (GTK_CONTAINER (xranw->rp_dialog), 10);
  gtk_window_set_default_size (GTK_WINDOW (xranw->rp_dialog), 600, 300);

  vbox19 = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (vbox19);
  gtk_box_pack_start (GTK_BOX (dialog_vbox14), vbox19, TRUE, TRUE, 0);

  hbox21 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox21);
  gtk_box_pack_start (GTK_BOX (vbox19), hbox21, TRUE, TRUE, 0);

  label89 = gtk_label_new (_("Number of Tracks Requested:    "));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg (label89, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  gtk_widget_show (label89);
  gtk_box_pack_start (GTK_BOX (hbox21), label89, FALSE, FALSE, 0);
  gtk_label_set_justify (GTK_LABEL (label89), GTK_JUSTIFY_LEFT);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg (label89, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  spinbutton50_adj = gtk_adjustment_new (10, 1, 100, 1, 10, 0);
  xranw->numtracks = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton50_adj), 1, 0);
  gtk_widget_show (xranw->numtracks);
  gtk_box_pack_start (GTK_BOX (hbox21), xranw->numtracks, FALSE, FALSE, 0);
  gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (xranw->numtracks), TRUE);
  gtk_spin_button_set_update_policy (GTK_SPIN_BUTTON (xranw->numtracks), GTK_UPDATE_IF_VALID);

  hbox20 = gtk_hbox_new (FALSE, 16);
  gtk_widget_show (hbox20);
  gtk_box_pack_start (GTK_BOX (vbox19), hbox20, TRUE, TRUE, 0);

  label88 = gtk_label_new (_("Directory:"));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg (label88, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  gtk_widget_show (label88);
  gtk_box_pack_start (GTK_BOX (hbox20), label88, FALSE, FALSE, 0);
  gtk_label_set_justify (GTK_LABEL (label88), GTK_JUSTIFY_LEFT);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg (label88, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  xranw->dir = gtk_entry_new ();
  gtk_widget_show (xranw->dir);
  gtk_box_pack_start (GTK_BOX (hbox20), xranw->dir, TRUE, TRUE, 0);
  gtk_entry_set_text(GTK_ENTRY(xranw->dir),mainw->audio_dir);

  button13 = gtk_button_new ();
  gtk_widget_show (button13);
  gtk_box_pack_start (GTK_BOX (hbox20), button13, FALSE, FALSE, 0);

  image413 = gtk_image_new_from_stock ("gtk-open", GTK_ICON_SIZE_BUTTON);
  gtk_widget_show (image413);
  gtk_container_add (GTK_CONTAINER (button13), image413);

  xranw->subdir_check = gtk_check_button_new ();
  eventbox=gtk_event_box_new();
  label=gtk_label_new_with_mnemonic (_("Include subdirectories"));
  gtk_label_set_mnemonic_widget (GTK_LABEL (label),xranw->subdir_check);

  gtk_container_add(GTK_CONTAINER(eventbox),label);
  g_signal_connect (GTK_OBJECT (eventbox), "button_press_event",
		    G_CALLBACK (label_act_toggle),
		    xranw->subdir_check);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg (eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  
  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox19), hbox, FALSE, FALSE, 10);

  gtk_box_pack_start (GTK_BOX (hbox), xranw->subdir_check, FALSE, FALSE, 10);
  gtk_box_pack_start (GTK_BOX (hbox), eventbox, FALSE, FALSE, 10);
  GTK_WIDGET_SET_FLAGS (xranw->subdir_check, GTK_CAN_DEFAULT|GTK_CAN_FOCUS);
  gtk_widget_show_all(hbox);

  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(xranw->subdir_check),TRUE);

  hbox22 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox22);
  gtk_box_pack_start (GTK_BOX (vbox19), hbox22, TRUE, TRUE, 0);

  label90 = gtk_label_new (_("Minimum File Size (MB)         "));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg (label90, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  gtk_widget_show (label90);
  gtk_box_pack_start (GTK_BOX (hbox22), label90, FALSE, FALSE, 0);
  gtk_label_set_justify (GTK_LABEL (label90), GTK_JUSTIFY_LEFT);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg (label90, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  spinbutton51_adj = gtk_adjustment_new (2, 0, 100, 1, 10, 0);
  xranw->minsize = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton51_adj), 1, 0);
  gtk_widget_show (xranw->minsize);
  gtk_box_pack_start (GTK_BOX (hbox22),xranw->minsize, FALSE, FALSE, 10);

  hbox23 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox23);
  gtk_box_pack_start (GTK_BOX (vbox19), hbox23, TRUE, TRUE, 0);

  label91 = gtk_label_new (_("Maximum File Size (MB)        "));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg (label91, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  gtk_widget_show (label91);
  gtk_box_pack_start (GTK_BOX (hbox23), label91, FALSE, FALSE, 0);
  gtk_label_set_justify (GTK_LABEL (label91), GTK_JUSTIFY_LEFT);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg (label91, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  spinbutton52_adj = gtk_adjustment_new (10, 0, 100, 1, 10, 0);
  xranw->maxsize = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton52_adj), 1, 0);
  gtk_widget_show (xranw->maxsize);
  gtk_box_pack_start (GTK_BOX (hbox23), xranw->maxsize, FALSE, FALSE, 10);

  dialog_action_area14 = GTK_DIALOG (xranw->rp_dialog)->action_area;
  gtk_widget_show (dialog_action_area14);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog_action_area14), GTK_BUTTONBOX_END);

  cancelbutton12 = gtk_button_new_from_stock ("gtk-cancel");
  gtk_widget_show (cancelbutton12);
  gtk_dialog_add_action_widget (GTK_DIALOG (xranw->rp_dialog), cancelbutton12, GTK_RESPONSE_CANCEL);
  GTK_WIDGET_SET_FLAGS (cancelbutton12, GTK_CAN_DEFAULT);

  okbutton11 = gtk_button_new_from_stock ("gtk-ok");
  gtk_widget_show (okbutton11);
  gtk_dialog_add_action_widget (GTK_DIALOG (xranw->rp_dialog), okbutton11, GTK_RESPONSE_OK);
  GTK_WIDGET_SET_FLAGS (okbutton11, GTK_CAN_DEFAULT);
  gtk_widget_grab_default (okbutton11);

  g_signal_connect(okbutton11, "clicked", G_CALLBACK (on_xmms_ran_ok_clicked),NULL);
  g_signal_connect(cancelbutton12, "clicked", G_CALLBACK (on_cancel_button1_clicked),xranw);
  g_signal_connect(button13, "clicked", G_CALLBACK (on_filesel_simple_clicked),xranw->dir);
  g_signal_connect (GTK_OBJECT (xranw->rp_dialog), "delete_event",
                      G_CALLBACK (return_true),
                      NULL);

  return xranw;
}


_commentsw* create_comments_dialog (file *sfile, gchar *filename) {
  GtkWidget *dialog_vbox;
  GtkWidget *table;
  GtkWidget *label;
  GtkWidget *expander;
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *eventbox;
  GtkWidget *dialog_action_area;
  GtkWidget *okbutton;
  GtkWidget *cancelbutton;
  GtkWidget *buttond;

  _commentsw *commentsw=(_commentsw*)(g_malloc(sizeof(_commentsw)));

  gint bheight=200;

  if (sfile!=NULL) bheight=350;

  commentsw->comments_dialog = gtk_dialog_new ();
  gtk_container_set_border_width (GTK_CONTAINER (commentsw->comments_dialog), 10);
  gtk_window_set_title (GTK_WINDOW (commentsw->comments_dialog), _("LiVES: - File Comments (optional)"));
  gtk_window_set_position (GTK_WINDOW (commentsw->comments_dialog), GTK_WIN_POS_CENTER_ALWAYS);
  gtk_window_set_modal (GTK_WINDOW (commentsw->comments_dialog), TRUE);
  if (prefs->show_gui) {
    gtk_window_set_transient_for(GTK_WINDOW(commentsw->comments_dialog),GTK_WINDOW(mainw->LiVES));
  }
  if (palette->style&STYLE_1) {
    gtk_dialog_set_has_separator(GTK_DIALOG(commentsw->comments_dialog),FALSE);
    gtk_widget_modify_bg (commentsw->comments_dialog, GTK_STATE_NORMAL, &palette->normal_back);
  }

  gtk_window_set_default_size (GTK_WINDOW (commentsw->comments_dialog), 600, bheight);

  dialog_vbox = GTK_DIALOG (commentsw->comments_dialog)->vbox;
  gtk_widget_show (dialog_vbox);

  table = gtk_table_new (4, 2, FALSE);
  gtk_widget_show (table);
  gtk_box_pack_start (GTK_BOX (dialog_vbox), table, TRUE, TRUE, 10);

  label = gtk_label_new (_("Title/Name : "));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

  label = gtk_label_new (_("Author/Artist : "));
  gtk_widget_show (label);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 1, 2,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

  label = gtk_label_new (_("Comments : "));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 3, 4,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

  commentsw->title_entry = gtk_entry_new ();
  gtk_widget_show (commentsw->title_entry);
  gtk_table_attach (GTK_TABLE (table), commentsw->title_entry, 1, 2, 0, 1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (GTK_EXPAND), 0, 0);
  gtk_entry_set_text (GTK_ENTRY (commentsw->title_entry),cfile->title);

  commentsw->author_entry = gtk_entry_new ();
  gtk_widget_show (commentsw->author_entry);
  gtk_table_attach (GTK_TABLE (table), commentsw->author_entry, 1, 2, 1, 2,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (GTK_EXPAND), 0, 0);
  gtk_entry_set_text (GTK_ENTRY (commentsw->author_entry),cfile->author);

  commentsw->comment_entry = gtk_entry_new ();
  gtk_widget_show (commentsw->comment_entry);
  gtk_table_attach (GTK_TABLE (table), commentsw->comment_entry, 1, 2, 3, 4,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (GTK_EXPAND), 0, 0);
  gtk_entry_set_max_length (GTK_ENTRY (commentsw->comment_entry), 250);

  gtk_entry_set_text (GTK_ENTRY (commentsw->comment_entry),cfile->comment);

  if (sfile!=NULL) {

    expander=gtk_expander_new_with_mnemonic(_("_Options"));
    gtk_widget_show (expander);
  
    if (palette->style&STYLE_1) {
      label=gtk_expander_get_label_widget(GTK_EXPANDER(expander));
      gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    }

    gtk_box_pack_start (GTK_BOX (dialog_vbox), expander, TRUE, TRUE, 0);

    vbox = gtk_vbox_new (FALSE, 0);
    gtk_widget_show (vbox);
    gtk_container_add (GTK_CONTAINER (expander), vbox);

    // options

    commentsw->subt_checkbutton = gtk_check_button_new ();
    //if (param->desc!=NULL) gtk_tooltips_set_tip (mainw->tooltips, checkbutton, param->desc, NULL);
    eventbox=gtk_event_box_new();
    //gtk_tooltips_copy(eventbox,checkbutton);
    label=gtk_label_new_with_mnemonic (_("Save subtitles to file"));
    gtk_label_set_mnemonic_widget (GTK_LABEL (label),commentsw->subt_checkbutton);
    
    gtk_container_add(GTK_CONTAINER(eventbox),label);
    g_signal_connect (GTK_OBJECT (eventbox), "button_press_event",
		      G_CALLBACK (label_act_toggle),
		      commentsw->subt_checkbutton);
    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
      gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
      gtk_widget_modify_bg (eventbox, GTK_STATE_NORMAL, &palette->normal_back);
    }

    if (sfile->subt==NULL) {
      gtk_widget_set_sensitive(commentsw->subt_checkbutton,FALSE);
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(commentsw->subt_checkbutton),FALSE);
    }
    else gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(commentsw->subt_checkbutton),TRUE);

    hbox = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 10);
    gtk_box_pack_start (GTK_BOX (hbox), commentsw->subt_checkbutton, FALSE, FALSE, 10);
    gtk_box_pack_start (GTK_BOX (hbox), eventbox, FALSE, FALSE, 10);
    gtk_widget_show_all (hbox);


    hbox = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 10);
    commentsw->subt_entry=gtk_entry_new();
    gtk_entry_set_width_chars(GTK_ENTRY(commentsw->subt_entry),32);
    label = gtk_label_new_with_mnemonic (_("Subtitle file"));

    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    }

    buttond = gtk_button_new_with_mnemonic(_("Browse..."));

    g_signal_connect (buttond, "clicked",G_CALLBACK (on_save_subs_activate),
    		      (gpointer)commentsw->subt_entry);

    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 10);
    gtk_box_pack_start (GTK_BOX (hbox), commentsw->subt_entry, FALSE, FALSE, 10);
    gtk_box_pack_start (GTK_BOX (hbox), buttond, FALSE, FALSE, 10);
    gtk_widget_show_all (hbox);

    if (sfile->subt==NULL) {
      gtk_widget_set_sensitive(commentsw->subt_entry,FALSE);
      gtk_widget_set_sensitive(buttond,FALSE);
    }
    else {
      gchar xfilename[512];
      gchar *osubfname=NULL;

      g_snprintf(xfilename,512,"%s",filename);
      get_filename(xfilename,FALSE); // strip extension
      switch (sfile->subt->type) {
      case SUBTITLE_TYPE_SRT:
	osubfname=g_strdup_printf("%s.srt",xfilename);
	break;

      case SUBTITLE_TYPE_SUB:
	osubfname=g_strdup_printf("%s.sub",xfilename);
	break;

      default:
	break;
      }
      gtk_entry_set_text(GTK_ENTRY(commentsw->subt_entry),osubfname);
      mainw->subt_save_file=osubfname; // assign instead of free
    }
  }

  dialog_action_area = GTK_DIALOG (commentsw->comments_dialog)->action_area;
  gtk_widget_show (dialog_action_area);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog_action_area), GTK_BUTTONBOX_END);

  cancelbutton = gtk_button_new_from_stock ("gtk-cancel");
  gtk_widget_show (cancelbutton);
  gtk_dialog_add_action_widget (GTK_DIALOG (commentsw->comments_dialog), cancelbutton, GTK_RESPONSE_CANCEL);
  GTK_WIDGET_SET_FLAGS (cancelbutton, GTK_CAN_DEFAULT);

  okbutton = gtk_button_new_from_stock ("gtk-ok");
  gtk_widget_show (okbutton);
  gtk_dialog_add_action_widget (GTK_DIALOG (commentsw->comments_dialog), okbutton, GTK_RESPONSE_OK);

  GTK_WIDGET_SET_FLAGS (okbutton, GTK_CAN_DEFAULT|GTK_CAN_FOCUS);
  gtk_widget_grab_focus (okbutton);
  gtk_widget_grab_default (okbutton);

  return commentsw;
}





gchar *choose_file(gchar *dir, gchar *fname, gchar **filt, GtkFileChooserAction act, GtkWidget *extra_widget) {
  // new style file chooser

  // in/out values are in utf8 encoding

  GList *children;

  GtkWidget *chooser;
  GtkFileFilter *filter;
  gchar *filename;
  int i;
  GtkResponseType response;
  gchar *title;
  gchar *tmp;

  if (act==GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER) {
    title=g_strdup(_("LiVES: choose a directory"));
  }
  else {
    title=g_strdup(_("LiVES: choose a file"));
  }

  if (act!=GTK_FILE_CHOOSER_ACTION_SAVE) 
    chooser=gtk_file_chooser_dialog_new(title,GTK_WINDOW(mainw->LiVES),act,GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
					NULL);

  else {
    chooser=gtk_file_chooser_dialog_new(title,GTK_WINDOW(mainw->LiVES),act,GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
					NULL);
  }


  g_free(title);

  gtk_file_chooser_set_local_only(GTK_FILE_CHOOSER(chooser),TRUE);
  if (dir!=NULL) {
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(chooser),(tmp=g_filename_from_utf8(dir,-1,NULL,NULL,NULL)));
    gtk_file_chooser_add_shortcut_folder(GTK_FILE_CHOOSER(chooser),tmp,NULL);
    g_free(tmp);
  }
#if GLIB_CHECK_VERSION(2,8,0)
  gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(chooser),TRUE);
#endif
  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(chooser, GTK_STATE_NORMAL, &palette->normal_back);
    children=gtk_container_get_children(GTK_CONTAINER(chooser));
    while (children!=NULL) {
      GtkWidget *child=children->data;
      gtk_widget_modify_fg(child, GTK_STATE_NORMAL, &palette->normal_fore);
      children=children->next;
    }
  }

  if (filt!=NULL) {
    filter=gtk_file_filter_new();
    gtk_file_filter_add_pattern(filter,filt[0]);
    for (i=1;filt[i]!=NULL;i++) gtk_file_filter_add_pattern(filter,filt[i]);
    gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(chooser),filter);
    if (fname!=NULL) gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(chooser),fname);
    else if (i==1&&act==GTK_FILE_CHOOSER_ACTION_SAVE) gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(chooser),filt[0]);
  }
  else {
    if (fname!=NULL&&dir!=NULL) {
      gchar *ffname=g_strconcat(dir,fname,NULL);
      gtk_file_chooser_set_current_folder_uri (GTK_FILE_CHOOSER(chooser), dir);
      gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER(chooser), fname);
      gtk_file_chooser_select_filename(GTK_FILE_CHOOSER(chooser),ffname);
      g_free(ffname);
    }
  }

  if (extra_widget!=NULL) gtk_file_chooser_set_extra_widget(GTK_FILE_CHOOSER(chooser),extra_widget);

  gtk_widget_show(chooser);

  if ((response=gtk_dialog_run(GTK_DIALOG(chooser)))==GTK_RESPONSE_CANCEL) {
    gtk_widget_destroy(chooser);
    return NULL;
  }

  filename=g_filename_to_utf8((tmp=gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(chooser))),-1,NULL,NULL,NULL);
  g_free(tmp);

  gtk_widget_destroy(chooser);

  return filename;

}






//cancel/discard/save dialog
_entryw* create_cds_dialog (gint type) {
  GtkWidget *dialog_vbox;
  GtkWidget *dialog_action_area;
  GtkWidget *cancelbutton;
  GtkWidget *discardbutton;
  GtkWidget *savebutton;
  GtkWidget *label=NULL;
  GtkWidget *hbox;
  GtkAccelGroup *accel_group;

  _entryw *cdsw=(_entryw*)(g_malloc(sizeof(_entryw)));

  cdsw->warn_checkbutton=NULL;

  cdsw->dialog = gtk_dialog_new ();
  accel_group = GTK_ACCEL_GROUP(gtk_accel_group_new ());
  gtk_window_add_accel_group (GTK_WINDOW (cdsw->dialog), accel_group);
  gtk_window_set_title (GTK_WINDOW (cdsw->dialog), _("LiVES: - Cancel/Discard/Save"));
  if (palette->style&STYLE_1) {
    gtk_dialog_set_has_separator(GTK_DIALOG(cdsw->dialog),FALSE);
    gtk_widget_modify_bg(cdsw->dialog, GTK_STATE_NORMAL, &palette->normal_back);
  }

  gtk_window_set_modal (GTK_WINDOW (cdsw->dialog), TRUE);
  gtk_window_set_position (GTK_WINDOW (cdsw->dialog), GTK_WIN_POS_CENTER_ALWAYS);
  gtk_window_set_default_size (GTK_WINDOW (cdsw->dialog), 350, 200);

  if (prefs->show_gui) {
    if (mainw->multitrack==NULL) gtk_window_set_transient_for(GTK_WINDOW(cdsw->dialog),GTK_WINDOW(mainw->LiVES));
    else gtk_window_set_transient_for(GTK_WINDOW(cdsw->dialog),GTK_WINDOW(mainw->multitrack->window));
  }

  dialog_vbox = GTK_DIALOG (cdsw->dialog)->vbox;
  gtk_widget_show (dialog_vbox);

  if (type==0) {
    if (strlen(mainw->multitrack->layout_name)==0) {
      label = gtk_label_new (_("You are about to leave multitrack mode.\nThe current layout has not been saved.\nWhat would you like to do ?\n"));
    }
    else {
      label = gtk_label_new (_("You are about to leave multitrack mode.\nThe current layout has been changed since the last save.\nWhat would you like to do ?\n"));
    }
  }
  else if (type==1) {
    if (!mainw->only_close) label = gtk_label_new (_("You are about to exit LiVES.\nThe current clip set can be saved.\nWhat would you like to do ?\n"));
    else label = gtk_label_new (_("The current clip set has not been saved.\nWhat would you like to do ?\n"));
  }
  else if (type==2||type==3) {
    if ((mainw->multitrack!=NULL&&mainw->multitrack->changed)||(mainw->stored_event_list!=NULL&&mainw->stored_event_list_changed)) {
      label = gtk_label_new (_("The current layout has not been saved.\nWhat would you like to do ?\n"));
    }
    else {
      label = gtk_label_new (_("The current layout has not been changed since it was last saved.\nWhat would you like to do ?\n"));
    }
  }
  else if (type==4) {
    if (mainw->multitrack!=NULL&&mainw->multitrack->changed) {
      label = gtk_label_new (_("The current layout contains generated frames and cannot be retained.\nYou may wish to render it before exiting multitrack mode.\n"));
    }
    else {
      label = gtk_label_new (_("You are about to leave multitrack mode.\nThe current layout contains generated frames and cannot be retained.\nWhat do you wish to do ?"));
    }
  }

  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (dialog_vbox), label, TRUE, TRUE, 0);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_CENTER);
  gtk_label_set_line_wrap (GTK_LABEL (label), FALSE);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  if (type==1) {
    GtkWidget *eventbox,*checkbutton;

    hbox = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (dialog_vbox), hbox, FALSE, FALSE, 10);
    
    cdsw->entry = gtk_entry_new_with_max_length (128);
    if (strlen (mainw->set_name)) {
      gtk_entry_set_text (GTK_ENTRY (cdsw->entry),mainw->set_name);
    }
    
    label=gtk_label_new_with_mnemonic (_("Clip set _name"));
    gtk_label_set_mnemonic_widget (GTK_LABEL (label),cdsw->entry);
    if (palette->style&STYLE_1&&mainw!=NULL) {
      gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    }
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 10);
    gtk_box_pack_start (GTK_BOX (hbox), cdsw->entry, FALSE, TRUE, 0);
    gtk_entry_set_width_chars (GTK_ENTRY (cdsw->entry),32);
    gtk_entry_set_activates_default(GTK_ENTRY(cdsw->entry),TRUE);
    gtk_widget_show_all (hbox);

    checkbutton = gtk_check_button_new ();
    label=gtk_label_new_with_mnemonic (_("_Auto reload next time"));

    eventbox=gtk_event_box_new();
    gtk_container_add (GTK_CONTAINER (eventbox), label);
    g_signal_connect (GTK_OBJECT (eventbox), "button_press_event",
		      G_CALLBACK (label_act_toggle),
		      checkbutton);
    gtk_label_set_mnemonic_widget (GTK_LABEL (label),checkbutton);
    if (palette->style&STYLE_1&&mainw!=NULL) {
      gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
      gtk_widget_modify_bg (eventbox, GTK_STATE_NORMAL, &palette->normal_back);
    }
    hbox = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (dialog_vbox), hbox, FALSE, FALSE, 10);
    gtk_box_pack_start (GTK_BOX (hbox), checkbutton, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (hbox), eventbox, FALSE, FALSE, 10);
    gtk_widget_show_all (hbox);
    GTK_WIDGET_SET_FLAGS (checkbutton, GTK_CAN_DEFAULT|GTK_CAN_FOCUS);
    
    if ((type==0&&prefs->ar_layout)||(type==1&&!mainw->only_close)) {
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbutton),TRUE);
      if (type==1) prefs->ar_clipset=TRUE;
      else prefs->ar_layout=TRUE;
    }
    else {
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbutton),FALSE);
      if (type==1) prefs->ar_clipset=FALSE;
      else prefs->ar_layout=FALSE;
    }
    
    g_object_set_data(G_OBJECT(checkbutton),"cdsw",(gpointer)cdsw);
    
    g_signal_connect (GTK_OBJECT (checkbutton), "toggled",
		      G_CALLBACK (on_autoreload_toggled),
		      GINT_TO_POINTER(type));
  }

  if (type==0&&!(prefs->warning_mask&WARN_MASK_EXIT_MT)) {
    add_warn_check(GTK_BOX(dialog_vbox),WARN_MASK_EXIT_MT);
  }

  dialog_action_area = GTK_DIALOG (cdsw->dialog)->action_area;
  gtk_widget_show (dialog_action_area);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog_action_area), GTK_BUTTONBOX_END);

  cancelbutton = gtk_button_new_from_stock ("gtk-cancel");
  gtk_widget_show (cancelbutton);
  gtk_dialog_add_action_widget (GTK_DIALOG (cdsw->dialog), cancelbutton, 0);
  gtk_widget_add_accelerator (cancelbutton, "activate", accel_group,
                              GDK_Escape, 0, 0);

  discardbutton = gtk_button_new_from_stock ("gtk-delete");
  gtk_widget_show (discardbutton);
  gtk_dialog_add_action_widget (GTK_DIALOG (cdsw->dialog), discardbutton, 1+(type==2));
  gtk_button_set_use_stock(GTK_BUTTON(discardbutton),FALSE);
  gtk_button_set_use_underline(GTK_BUTTON(discardbutton),TRUE);
  if ((type==0&&strlen(mainw->multitrack->layout_name)==0)||type==3||type==4) gtk_button_set_label(GTK_BUTTON(discardbutton),_("_Wipe layout"));
  else if (type==0) gtk_button_set_label(GTK_BUTTON(discardbutton),_("_Ignore changes"));
  else if (type==1) gtk_button_set_label(GTK_BUTTON(discardbutton),_("_Delete clip set"));
  else if (type==2) gtk_button_set_label(GTK_BUTTON(discardbutton),_("_Delete layout"));

  savebutton = gtk_button_new_from_stock ("gtk-save");
  gtk_widget_show (savebutton);
  gtk_button_set_use_stock(GTK_BUTTON(savebutton),FALSE);
  gtk_button_set_use_underline(GTK_BUTTON(savebutton),TRUE);
  if (type==0||type==3) gtk_button_set_label(GTK_BUTTON(savebutton),_("_Save layout"));
  else if (type==1) gtk_button_set_label(GTK_BUTTON(savebutton),_("_Save clip set"));
  else if (type==2) gtk_button_set_label(GTK_BUTTON(savebutton),_("_Wipe layout"));
  if (type!=4) gtk_dialog_add_action_widget (GTK_DIALOG (cdsw->dialog), savebutton, 2-(type==2));
  GTK_WIDGET_SET_FLAGS (savebutton, GTK_CAN_DEFAULT);
  if (type==1||type==2)gtk_widget_grab_default(savebutton);

  return cdsw;
}




void do_layout_recover_dialog(void) {
  GtkWidget *label;
  GtkWidget *dialog_vbox;
  GtkWidget *okbutton;
  GtkWidget *cancelbutton;

  GtkWidget *mdialog=gtk_dialog_new ();

  gtk_window_set_title (GTK_WINDOW (mdialog), _("LiVES: recover layout ?"));
  gtk_window_set_position (GTK_WINDOW (mdialog), GTK_WIN_POS_CENTER_ALWAYS);
  gtk_window_set_default_size (GTK_WINDOW (mdialog), 450, 200);

  if (prefs->show_gui) {
    gtk_window_set_transient_for(GTK_WINDOW(mdialog),GTK_WINDOW(mainw->LiVES));
  }

  if (palette->style&STYLE_1) {
    gtk_dialog_set_has_separator(GTK_DIALOG(mdialog),FALSE);
    gtk_widget_modify_bg(mdialog, GTK_STATE_NORMAL, &palette->normal_back);
  }

  dialog_vbox = GTK_DIALOG (mdialog)->vbox;
  
  label = gtk_label_new (_("\nLiVES has detected a multitrack layout from a previous session.\nWould you like to try and recover it ?\n"));
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_CENTER);

  if (mainw!=NULL&&palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  gtk_container_add (GTK_CONTAINER (dialog_vbox), label);


  cancelbutton = gtk_button_new_from_stock ("gtk-cancel");
  gtk_widget_show (cancelbutton);
  gtk_dialog_add_action_widget (GTK_DIALOG (mdialog), cancelbutton, GTK_RESPONSE_CANCEL);
  GTK_WIDGET_SET_FLAGS (cancelbutton, GTK_CAN_FOCUS);

  okbutton = gtk_button_new_from_stock ("gtk-ok");
  gtk_widget_show (okbutton);
  gtk_dialog_add_action_widget (GTK_DIALOG (mdialog), okbutton, GTK_RESPONSE_OK);
  GTK_WIDGET_SET_FLAGS (okbutton, GTK_CAN_DEFAULT|GTK_CAN_FOCUS);
  gtk_widget_grab_default(okbutton);
  gtk_widget_grab_focus(okbutton);

  g_signal_connect (cancelbutton, "clicked",G_CALLBACK (recover_layout_cancelled),NULL);

  g_signal_connect (okbutton, "clicked",G_CALLBACK (recover_layout),NULL);

  gtk_widget_show_all(mdialog);
}




GtkTextView *create_output_textview(void) {
  GtkWidget *textview=gtk_text_view_new();
  gtk_text_view_set_editable (GTK_TEXT_VIEW (textview), FALSE);
  g_object_ref(textview);
  gtk_widget_show(textview);
  return GTK_TEXT_VIEW(textview);
}

