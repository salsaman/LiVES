// interface.c
// LiVES
// (c) G. Finch 2003 - 2013 <salsaman@gmail.com>
// Released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

// TODO - use gtk_widget_showall where poss.
// and don't forget gtk_box_pack_end (doh)
// and just use label instead of labelnn, etc.


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
#include "paramwindow.h" // TODO - remove when we have lives_standard_scrolledwindow_new()

// functions called in multitrack.c
extern void multitrack_preview_clicked  (GtkButton *, gpointer user_data);
extern void mt_change_disp_tracks_ok (GtkButton *, gpointer user_data);

static gulong arrow_id; // works around a known bug in gobject


void add_suffix_check(GtkBox *box, const gchar *ext) {
  gchar *ltext;

  GtkWidget *checkbutton;

  if (ext==NULL) ltext=g_strdup (_ ("Let LiVES set the _file extension"));
  else ltext=g_strdup_printf(_ ("Let LiVES set the _file extension (.%s)"),ext);
  checkbutton=lives_standard_check_button_new(ltext,TRUE,box,NULL);
  g_free(ltext);
  lives_toggle_button_set_active (LIVES_TOGGLE_BUTTON (checkbutton), mainw->fx1_bool);
  g_signal_connect_after (GTK_OBJECT (checkbutton), "toggled",
			  G_CALLBACK (on_boolean_toggled),
			  &mainw->fx1_bool);
  
}



static GtkWidget *add_deinterlace_checkbox(GtkBox *for_deint) {
  GtkWidget *hbox=lives_hbox_new (FALSE, 0);
  GtkWidget *checkbutton = lives_standard_check_button_new (_ ("Apply _Deinterlace"),TRUE,LIVES_BOX(hbox),NULL);

  if (LIVES_IS_HBOX(for_deint)) {
    GtkWidget *filler;
    gtk_box_pack_start (for_deint, hbox, FALSE, FALSE, widget_opts.packing_width);
    gtk_box_reorder_child(for_deint, hbox, 0);
    filler=add_fill_to_box(LIVES_BOX(for_deint));
    if (filler!=NULL) gtk_box_reorder_child(for_deint, filler, 1);
  }
  else gtk_box_pack_start (for_deint, hbox, FALSE, FALSE, widget_opts.packing_height);

  lives_widget_set_can_focus_and_default (checkbutton);
  lives_toggle_button_set_active (LIVES_TOGGLE_BUTTON (checkbutton), mainw->open_deint);
  g_signal_connect_after (GTK_OBJECT (checkbutton), "toggled",
			  G_CALLBACK (on_boolean_toggled),
			  &mainw->open_deint);
  gtk_widget_set_tooltip_text( checkbutton,_("If this is set, frames will be deinterlaced as they are imported."));

  gtk_widget_show_all(GTK_WIDGET(for_deint));

  return hbox;
}


static void pv_sel_changed(GtkFileChooser *chooser, gpointer user_data) {
  GSList *slist=gtk_file_chooser_get_filenames (chooser);
  GtkWidget *pbutton=(GtkWidget *)user_data;

  if (slist==NULL||slist->data==NULL||g_slist_length(slist)>1||!(g_file_test((gchar *)slist->data,G_FILE_TEST_IS_REGULAR))) {
    gtk_widget_set_sensitive(pbutton,FALSE);
  }
  else gtk_widget_set_sensitive(pbutton,TRUE);
  
  if (slist!=NULL) {
    g_list_free_strings((GList *)slist);
    g_slist_free(slist);
  }
}





void widget_add_preview(GtkWidget *widget, LiVESBox *for_preview, LiVESBox *for_button, LiVESBox *for_deint, int preview_type) {
  // preview type 1 - video and audio, fileselector
  // preview type 2 - audio only, fileselector
  // preview type 3 - range preview

  GtkWidget *preview_button=NULL;
  GtkWidget *fs_label;

  mainw->fs_playframe = gtk_frame_new (NULL);
  mainw->fs_playalign = gtk_alignment_new (0.,0.,1.,1.);
  mainw->fs_playarea = gtk_event_box_new ();

  if (preview_type==1||preview_type==3) {

    gtk_widget_show (mainw->fs_playframe);
    gtk_widget_show (mainw->fs_playalign);

    gtk_container_set_border_width (GTK_CONTAINER(mainw->fs_playframe), widget_opts.border_width);

    widget_opts.justify=LIVES_JUSTIFY_RIGHT;
    fs_label = lives_standard_label_new (_ ("Preview"));
    widget_opts.justify=LIVES_JUSTIFY_DEFAULT;
    gtk_widget_show (fs_label);
    gtk_frame_set_label_widget (GTK_FRAME (mainw->fs_playframe), fs_label);

    gtk_box_pack_start (for_preview, mainw->fs_playframe, FALSE, FALSE, 0);
    gtk_widget_set_size_request (mainw->fs_playarea, DEFAULT_FRAME_HSIZE, DEFAULT_FRAME_VSIZE);

    gtk_container_add (GTK_CONTAINER (mainw->fs_playframe), mainw->fs_playalign);
    gtk_container_add (GTK_CONTAINER (mainw->fs_playalign), mainw->fs_playarea);

    lives_widget_set_bg_color (mainw->fs_playarea, GTK_STATE_NORMAL, &palette->black);
    lives_widget_set_bg_color (mainw->fs_playframe, GTK_STATE_NORMAL, &palette->black);
    lives_widget_set_bg_color (mainw->fs_playalign, GTK_STATE_NORMAL, &palette->black);

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
    lives_widget_set_bg_color(preview_button, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }

  gtk_box_pack_start (for_button, preview_button, FALSE, FALSE, widget_opts.packing_width);

  if (preview_type==1||preview_type==3) {
    add_deinterlace_checkbox(for_deint);
  }


  g_signal_connect (GTK_OBJECT (preview_button), "clicked",
		    G_CALLBACK (on_fs_preview_clicked),
		    GINT_TO_POINTER (preview_type));

  if (GTK_IS_FILE_CHOOSER(widget)) {
    gtk_widget_set_sensitive(preview_button,FALSE);
    
    g_signal_connect (GTK_OBJECT (widget), "selection_changed",
		      G_CALLBACK (pv_sel_changed),
		      (gpointer)preview_button);
  }

}


static boolean procdets_pressed (GtkWidget *ahbox, GdkEventButton *event, gpointer user_data) {
  GtkWidget *arrow=(GtkWidget *)user_data;
  boolean expanded=!(g_object_get_data(G_OBJECT(arrow),"expanded"));
  GtkWidget *hbox=lives_widget_get_parent(arrow);

  gtk_widget_destroy(arrow);

  // remove this signal because its user_data is invalid
  if (g_signal_handler_is_connected (ahbox, arrow_id)) {
    g_signal_handler_disconnect (ahbox, arrow_id);
  }

  arrow = gtk_arrow_new (expanded?GTK_ARROW_DOWN:GTK_ARROW_RIGHT, GTK_SHADOW_OUT);
  if (palette->style&STYLE_1) {
    lives_widget_set_fg_color(arrow, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  arrow_id=g_signal_connect (GTK_OBJECT (ahbox), "button_press_event",
			     G_CALLBACK (procdets_pressed),
			     arrow);

  gtk_box_pack_end (GTK_BOX (hbox), arrow, FALSE, FALSE, widget_opts.packing_width);
  gtk_widget_show(arrow);

  g_object_set_data(G_OBJECT(arrow),"expanded",GINT_TO_POINTER(expanded));

  if (expanded) gtk_widget_show(cfile->proc_ptr->scrolledwindow);
  else gtk_widget_hide(cfile->proc_ptr->scrolledwindow);

  return FALSE;
}



xprocess * create_processing (const gchar *text) {

  GtkWidget *dialog_vbox;
  GtkWidget *vbox2;
  GtkWidget *vbox3;
  GtkWidget *dialog_action_area;
  GtkWidget *hbox;
  GtkWidget *ahbox;
  GtkWidget *label;
  GtkWidget *details_arrow;

  GtkAccelGroup *accel_group=GTK_ACCEL_GROUP(gtk_accel_group_new ());

  xprocess *procw=(xprocess*)(g_malloc(sizeof(xprocess)));

  gchar tmp_label[256];

  widget_opts.non_modal=TRUE;
  procw->processing = lives_standard_dialog_new (_("LiVES: - Processing..."),FALSE);
  widget_opts.non_modal=FALSE;

  gtk_window_add_accel_group (GTK_WINDOW (procw->processing), accel_group);

  if (prefs->show_gui) {
    if (mainw->multitrack==NULL) gtk_window_set_transient_for(GTK_WINDOW(procw->processing),GTK_WINDOW(mainw->LiVES));
    else gtk_window_set_transient_for(GTK_WINDOW(procw->processing),GTK_WINDOW(mainw->multitrack->window));
  }

  dialog_vbox = lives_dialog_get_content_area(GTK_DIALOG(procw->processing));
  gtk_widget_show (dialog_vbox);

  vbox2 = lives_vbox_new (FALSE, 0);
  gtk_widget_show (vbox2);
  gtk_box_pack_start (GTK_BOX (dialog_vbox), vbox2, TRUE, TRUE, 0);

  vbox3 = lives_vbox_new (FALSE, 0);
  gtk_widget_show (vbox3);
  gtk_box_pack_start (GTK_BOX (vbox2), vbox3, TRUE, TRUE, 0);

  g_snprintf(tmp_label,256,"%s...\n",text);
  procw->label = lives_standard_label_new (tmp_label);
  gtk_widget_show (procw->label);

  gtk_box_pack_start (GTK_BOX (vbox3), procw->label, TRUE, TRUE, 0);

  procw->progressbar = gtk_progress_bar_new ();
  gtk_widget_show (procw->progressbar);
  gtk_box_pack_start (GTK_BOX (vbox3), procw->progressbar, FALSE, FALSE, 0);
  if (palette->style&STYLE_1) {
    lives_widget_set_fg_color(procw->progressbar, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  widget_opts.justify=LIVES_JUSTIFY_CENTER;
  if (mainw->internal_messaging&&mainw->rte!=0) {
    procw->label2 = lives_standard_label_new (_("\n\nPlease Wait\n\nRemember to switch off effects (ctrl-0) afterwards !"));
  }
#ifdef RT_AUDIO
  else if (mainw->jackd_read!=NULL||mainw->pulsed_read!=NULL) procw->label2 = gtk_label_new ("");
#endif
  else procw->label2=lives_standard_label_new (_("\nPlease Wait"));
  widget_opts.justify=LIVES_JUSTIFY_DEFAULT;

  gtk_widget_show (procw->label2);

  gtk_box_pack_start (GTK_BOX (vbox3), procw->label2, FALSE, FALSE, 0);

  widget_opts.justify=LIVES_JUSTIFY_CENTER;
  procw->label3 = lives_standard_label_new (PROCW_STRETCHER);
  gtk_widget_show (procw->label3);
  gtk_box_pack_start (GTK_BOX (vbox3), procw->label3, FALSE, FALSE, 0);
  widget_opts.justify=LIVES_JUSTIFY_DEFAULT;

  if (mainw->iochan!=NULL) {
    // add "show details" arrow

    ahbox=gtk_event_box_new();
    gtk_box_pack_start (GTK_BOX (vbox3), ahbox, FALSE, FALSE, widget_opts.packing_height);

    hbox = lives_hbox_new (FALSE, 0);
    gtk_container_add (GTK_CONTAINER (ahbox), hbox);

    label=gtk_label_new("");
    gtk_box_pack_end (GTK_BOX (hbox), label, TRUE, TRUE, widget_opts.packing_width);

    details_arrow = gtk_arrow_new (GTK_ARROW_RIGHT, GTK_SHADOW_OUT);

    arrow_id=g_signal_connect (GTK_OBJECT (ahbox), "button_press_event",
			       G_CALLBACK (procdets_pressed),
			       details_arrow);

    g_object_set_data(G_OBJECT(details_arrow),"expanded",GINT_TO_POINTER(FALSE));

    label=lives_standard_label_new (_ ("Show details"));
    gtk_box_pack_end (GTK_BOX (hbox), label, TRUE, FALSE, widget_opts.packing_width);
    gtk_box_pack_end (GTK_BOX (hbox), details_arrow, TRUE, FALSE, widget_opts.packing_width);

    if (palette->style&STYLE_1) {
      lives_widget_set_fg_color(details_arrow, GTK_STATE_NORMAL, &palette->normal_fore);
      lives_widget_set_bg_color(ahbox, GTK_STATE_NORMAL, &palette->normal_back);
    }

    gtk_widget_show_all(ahbox);

    procw->scrolledwindow = lives_standard_scrolled_window_new (ENC_DETAILS_WIN_H, ENC_DETAILS_WIN_V,
								LIVES_WIDGET(mainw->optextview), FALSE);

    gtk_box_pack_start (GTK_BOX (vbox3), procw->scrolledwindow, TRUE, TRUE, 0);
  }


  dialog_action_area = lives_dialog_get_action_area(LIVES_DIALOG (procw->processing));
  gtk_widget_show (dialog_action_area);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog_action_area), GTK_BUTTONBOX_END);

  procw->stop_button = gtk_button_new_with_mnemonic (_ ("_Enough"));
  procw->preview_button = gtk_button_new_with_mnemonic (_ ("_Preview"));

  if (cfile->nokeep) procw->pause_button = gtk_button_new_with_mnemonic (_ ("Paus_e"));
  else procw->pause_button = gtk_button_new_with_mnemonic (_ ("Pause/_Enough"));

  gtk_dialog_add_action_widget (GTK_DIALOG (procw->processing), procw->preview_button, 1);
  gtk_widget_hide(procw->preview_button);
  lives_widget_set_can_focus_and_default (procw->preview_button);
    
  gtk_dialog_add_action_widget (GTK_DIALOG (procw->processing), procw->pause_button, 0);
  gtk_widget_hide(procw->pause_button);
  lives_widget_set_can_focus_and_default (procw->pause_button);


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
      lives_widget_set_can_focus_and_default (procw->stop_button);
    }
  }
  
  procw->cancel_button = gtk_button_new_with_mnemonic (_ ("_Cancel"));
  gtk_dialog_add_action_widget (GTK_DIALOG (procw->processing), procw->cancel_button, GTK_RESPONSE_CANCEL);
  lives_widget_set_can_focus_and_default (procw->cancel_button);

  gtk_widget_add_accelerator (procw->cancel_button, "activate", accel_group,
                              LIVES_KEY_Escape, (GdkModifierType)0, (GtkAccelFlags)0);

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
                      G_CALLBACK (on_cancel_keep_button_clicked),
                      NULL);

  g_signal_connect (GTK_OBJECT (procw->processing), "delete_event",
                      G_CALLBACK (return_true),
                      NULL);


  return procw;
}



lives_clipinfo_t *create_clip_info_window (int audio_channels, boolean is_mt) {
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

  lives_clipinfo_t *filew=(lives_clipinfo_t *)(g_malloc(sizeof(lives_clipinfo_t)));

  filew->info_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_position (GTK_WINDOW (filew->info_window), GTK_WIN_POS_CENTER_ALWAYS);
  gtk_window_set_modal (GTK_WINDOW (filew->info_window), TRUE);

  if (palette->style&STYLE_1) {
    lives_widget_set_bg_color(filew->info_window, GTK_STATE_NORMAL, &palette->normal_back);
  }

  vbox5 = lives_vbox_new (FALSE, 0);
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
      lives_widget_set_bg_color(frame4, GTK_STATE_NORMAL, &palette->normal_back);
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
    
    label45 = lives_standard_label_new (_("Format"));
    gtk_widget_show (label45);
    gtk_fixed_put (GTK_FIXED (fixed3), label45, 44, 64);
    gtk_widget_set_size_request (label45, 125, 20);

    label46 = lives_standard_label_new (_("Frame size"));
    gtk_widget_show (label46);
    gtk_fixed_put (GTK_FIXED (fixed3), label46, 18, 152);
    gtk_widget_set_size_request (label46, 145, 20);

    if (!is_mt) label47 = lives_standard_label_new (_("File size"));
    else label47 = lives_standard_label_new (_("Byte size"));
    gtk_widget_show (label47);
    gtk_fixed_put (GTK_FIXED (fixed3), label47, 18, 240);
    gtk_widget_set_size_request (label47, 145, 20);

    label48 = lives_standard_label_new (_("Total time"));
    gtk_widget_show (label48);
    gtk_fixed_put (GTK_FIXED (fixed3), label48, 450, 240);
    gtk_widget_set_size_request (label48, 145, 20);

    label44 = lives_standard_label_new (_("FPS"));
    gtk_widget_show (label44);
    gtk_fixed_put (GTK_FIXED (fixed3), label44, 476, 64);
    gtk_widget_set_size_request (label44, 120, 20);

    if (!is_mt) label43 = lives_standard_label_new (_("Frames"));
    else label43 = lives_standard_label_new (_("Events"));
    gtk_widget_show (label43);
    gtk_fixed_put (GTK_FIXED (fixed3), label43, 476, 152);
    gtk_widget_set_size_request (label43, 125, 20);

    label40 = lives_standard_label_new (_("Video"));
    gtk_widget_show (label40);
    gtk_frame_set_label_widget (GTK_FRAME (frame4), label40);
  }

  if (audio_channels>0) {
    frame5 = gtk_frame_new (NULL);
    gtk_widget_set_size_request (frame5, 700, 140);
    gtk_widget_show (frame5);
    gtk_box_pack_start (GTK_BOX (vbox5), frame5, TRUE, TRUE, 0);
    if (palette->style&STYLE_1) {
      lives_widget_set_bg_color(frame5, GTK_STATE_NORMAL, &palette->normal_back);
    }

    if (audio_channels>1) {
      left = lives_standard_label_new (_("Left Audio"));
    }
    else {
      left = lives_standard_label_new (_("Audio"));
    }

    gtk_widget_show (left);
    gtk_frame_set_label_widget (GTK_FRAME (frame5), left);

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
      
      label50 = lives_standard_label_new (_("Total time"));
      gtk_widget_show (label50);
      gtk_fixed_put (GTK_FIXED (fixed5), label50, 450, 32);
      gtk_widget_set_size_request (label50, 145, 16);
    }

    filew->textview_lrate = gtk_text_view_new ();
    gtk_widget_show (filew->textview_lrate);
    gtk_fixed_put (GTK_FIXED (fixed5), filew->textview_lrate, 180, 16);
    gtk_widget_set_size_request (filew->textview_lrate, 180, 50);
    gtk_text_view_set_editable (GTK_TEXT_VIEW (filew->textview_lrate), FALSE);
    gtk_text_view_set_justification (GTK_TEXT_VIEW (filew->textview_lrate), GTK_JUSTIFY_CENTER);
    gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (filew->textview_lrate), FALSE);
    
    label52 = lives_standard_label_new (_("Rate/size"));
    gtk_widget_show (label52);
    gtk_fixed_put (GTK_FIXED (fixed5), label52, 30, 32);
    gtk_widget_set_size_request (label52, 130, 16);

    if (audio_channels>1) {
      frame6 = gtk_frame_new (NULL);
      gtk_widget_set_size_request (frame6, 600, 100);
      gtk_widget_show (frame6);
      gtk_box_pack_start (GTK_BOX (vbox5), frame6, TRUE, TRUE, 0);
      if (palette->style&STYLE_1) {
	lives_widget_set_bg_color(frame6, GTK_STATE_NORMAL, &palette->normal_back);
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
	
	label51 = lives_standard_label_new (_("Total time"));
	gtk_widget_show (label51);
	gtk_fixed_put (GTK_FIXED (fixed6), label51, 450, 32);
	gtk_widget_set_size_request (label51, 145, 16);
      }

      filew->textview_rrate = gtk_text_view_new ();
      gtk_widget_show (filew->textview_rrate);
      gtk_fixed_put (GTK_FIXED (fixed6), filew->textview_rrate, 180, 16);
      gtk_widget_set_size_request (filew->textview_rrate, 180, 50);
      gtk_text_view_set_editable (GTK_TEXT_VIEW (filew->textview_rrate), FALSE);
      gtk_text_view_set_justification (GTK_TEXT_VIEW (filew->textview_rrate), GTK_JUSTIFY_CENTER);
      gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (filew->textview_rrate), FALSE);
      
      label53 = lives_standard_label_new (_("Rate/size"));
      gtk_widget_show (label53);
      gtk_fixed_put (GTK_FIXED (fixed6), label53, 30, 32);
      gtk_widget_set_size_request (label53, 130, 16);
      
      right = lives_standard_label_new (_("Right audio"));
      gtk_widget_show (right);
      gtk_frame_set_label_widget (GTK_FRAME (frame6), right);
    }
  }

  button8 = gtk_button_new_from_stock ("gtk-ok");
  gtk_widget_show (button8);
  gtk_box_pack_end (GTK_BOX (vbox5), button8, FALSE, FALSE, 0);
  gtk_widget_set_size_request (button8, -1, 78);
  gtk_container_set_border_width (GTK_CONTAINER (button8), 12);
  gtk_button_set_relief (GTK_BUTTON (button8), GTK_RELIEF_NONE);
  lives_widget_set_can_focus_and_default (button8);
  gtk_widget_grab_default (button8);

  g_signal_connect (GTK_OBJECT (button8), "clicked",
                      G_CALLBACK (lives_general_button_clicked),
                      filew);

  g_signal_connect (GTK_OBJECT (filew->info_window), "delete_event",
                      G_CALLBACK (lives_general_delete_event),
                      filew);

  accel_group = GTK_ACCEL_GROUP(gtk_accel_group_new ());
  gtk_window_add_accel_group (GTK_WINDOW (filew->info_window), accel_group);

  gtk_widget_add_accelerator (button8, "activate", accel_group,
                              LIVES_KEY_Escape, (GdkModifierType)0, (GtkAccelFlags)0);


  return filew;
}


static void on_resizecb_toggled (GtkToggleButton *t, gpointer user_data) {
  GtkWidget *cb=(GtkWidget *)user_data;

  if (!lives_toggle_button_get_active(t)) {
    gtk_widget_set_sensitive(cb,FALSE);
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(cb),FALSE);
  }
  else {
    gtk_widget_set_sensitive(cb,TRUE);
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(cb),prefs->enc_letterbox);
  }
}




GtkWidget* create_encoder_prep_dialog (const gchar *text1, const gchar *text2, boolean opt_resize) {
  GtkWidget *dialog;
  GtkWidget *dialog_vbox;
  GtkWidget *dialog_action_area;
  GtkWidget *cancelbutton;
  GtkWidget *okbutton;
  GtkWidget *checkbutton=NULL;
  GtkWidget *checkbutton2;
  GtkWidget *label;
  GtkWidget *hbox;

  gchar *labeltext,*tmp,*tmp2;

  dialog = lives_standard_dialog_new (_("LiVES: - Encoding options"),FALSE);

  if (prefs->show_gui) {
    gtk_window_set_transient_for(GTK_WINDOW(dialog),GTK_WINDOW(mainw->LiVES));
  }

  dialog_vbox = lives_dialog_get_content_area(GTK_DIALOG(dialog));

  widget_opts.justify=LIVES_JUSTIFY_CENTER;
  label = lives_standard_label_new (text1);
  widget_opts.justify=LIVES_JUSTIFY_DEFAULT;
  gtk_box_pack_start (GTK_BOX (dialog_vbox), label, TRUE, TRUE, 0);

  if (opt_resize) {      
    if (text2!=NULL) labeltext=g_strdup (_("<------------- (Check the box to re_size as suggested)"));
    else labeltext=g_strdup (_("<------------- (Check the box to use the _size recommendation)"));

    hbox = lives_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (dialog_vbox), hbox, FALSE, FALSE, widget_opts.packing_width);

    checkbutton = lives_standard_check_button_new (labeltext,TRUE,LIVES_BOX(hbox),NULL);

    g_free(labeltext);
    
    g_signal_connect_after (GTK_OBJECT (checkbutton), "toggled",
			    G_CALLBACK (on_boolean_toggled),
			    &mainw->fx1_bool);

  }
  else if (text2==NULL) mainw->fx1_bool=TRUE;

  if (text2!=NULL&&(mainw->fx1_bool||opt_resize)) {

    hbox = lives_hbox_new (FALSE, 0);
    if (capable->has_composite&&capable->has_convert) {
      // only offer this if we have "composite" and "convert" - for now... TODO ****
      gtk_box_pack_start (GTK_BOX (dialog_vbox), hbox, FALSE, FALSE, widget_opts.packing_height);
    }

    checkbutton2 = lives_standard_check_button_new 
      ((tmp=g_strdup(_("Use _letterboxing to maintain aspect ratio (optional)"))),TRUE,LIVES_BOX(hbox),
       (tmp2=g_strdup(_("Draw black rectangles either above or to the sides of the image, to prevent it from stretching."))));

    g_free(tmp); g_free(tmp2);

    if (opt_resize) {
      gtk_widget_set_sensitive(checkbutton2,FALSE);
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(checkbutton2),FALSE);
    }
    else lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(checkbutton2),prefs->enc_letterbox);
    
    g_signal_connect_after (GTK_OBJECT (checkbutton2), "toggled",
			    G_CALLBACK (on_boolean_toggled),
			    &prefs->enc_letterbox);

    if (opt_resize) 
      g_signal_connect_after (GTK_OBJECT (checkbutton), "toggled",
			      G_CALLBACK (on_resizecb_toggled),
			      checkbutton2);

  }

  dialog_action_area = lives_dialog_get_action_area(LIVES_DIALOG (dialog));
  gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog_action_area), GTK_BUTTONBOX_END);

  if (text2!=NULL) {
    label = lives_standard_label_new (text2);
    gtk_box_pack_start (GTK_BOX (dialog_vbox), label, TRUE, TRUE, 0);
    cancelbutton = gtk_button_new_from_stock ("gtk-cancel");
    okbutton = gtk_button_new_from_stock ("gtk-ok");
  }
  else {
    cancelbutton = gtk_button_new_with_mnemonic (_("Keep _my settings"));
    okbutton = gtk_button_new_with_mnemonic (_("Use _recommended settings"));
  }

  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), cancelbutton, GTK_RESPONSE_CANCEL);
  lives_widget_set_can_focus_and_default (cancelbutton);

  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), okbutton, GTK_RESPONSE_OK);

  lives_widget_set_can_focus_and_default (okbutton);
  gtk_widget_grab_default (okbutton);

  gtk_widget_show_all (dialog);
  return dialog;
}


// Information/error dialog
GtkWidget* create_info_error_dialog (const gchar *text, boolean is_blocking, int mask) {
  GtkWidget *dialog;
  GtkWidget *dialog_vbox;
  GtkWidget *info_text;
  GtkWidget *dialog_action_area;
  GtkWidget *info_ok_button;
  GtkWidget *details_button;
  GtkWidget *checkbutton;
  GtkWidget *hbox;
  gchar *form_text;
  gchar *textx;

  GtkAccelGroup *accel_group=GTK_ACCEL_GROUP(gtk_accel_group_new ());

  //dialog = lives_standard_dialog_new (_("LiVES"),FALSE);
  dialog = gtk_message_dialog_new (NULL,(GtkDialogFlags)0,
				   mask==0?GTK_MESSAGE_ERROR:GTK_MESSAGE_WARNING,GTK_BUTTONS_NONE,"%s","");
  gtk_window_set_title (GTK_WINDOW (dialog), _("LiVES"));
  
  gtk_window_set_deletable(GTK_WINDOW(dialog), FALSE);
  gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);

  gtk_container_set_border_width (GTK_CONTAINER (dialog), widget_opts.border_width*2);

  gtk_window_add_accel_group (GTK_WINDOW (dialog), accel_group);

  if (mainw!=NULL&&mainw->is_ready&&palette->style&STYLE_1) {
    lives_dialog_set_has_separator(GTK_DIALOG(dialog),FALSE);
    lives_widget_set_bg_color(dialog, GTK_STATE_NORMAL, &palette->normal_back);
  }

  dialog_vbox = lives_dialog_get_content_area(GTK_DIALOG(dialog));

  textx=insert_newlines(text,MAX_MSG_WIDTH_CHARS);

  form_text=g_strdup_printf("\n\n%s",textx);

  widget_opts.justify=LIVES_JUSTIFY_CENTER;
  info_text = lives_standard_label_new (form_text);
  widget_opts.justify=LIVES_JUSTIFY_DEFAULT;
  g_free(form_text);
  g_free(textx);

  gtk_label_set_selectable (GTK_LABEL (info_text), TRUE);

  hbox = lives_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (dialog_vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

  gtk_box_pack_start (GTK_BOX (hbox), info_text, FALSE, FALSE, 20);
  
  if (mask>0) {
    hbox = lives_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (dialog_vbox), hbox, FALSE, FALSE, widget_opts.packing_height);
    checkbutton = lives_standard_check_button_new (
						   _("Do _not show this warning any more\n(can be turned back on from Preferences/Warnings)"),
						   TRUE,LIVES_BOX(hbox),NULL);
    lives_widget_set_can_focus_and_default (checkbutton);
    g_signal_connect (GTK_OBJECT (checkbutton), "toggled",
                      G_CALLBACK (on_warn_mask_toggled),
                      GINT_TO_POINTER(mask));
  }

  dialog_action_area = lives_dialog_get_action_area(LIVES_DIALOG (dialog));
  gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog_action_area), GTK_BUTTONBOX_END);

  if (mainw->iochan!=NULL) {
    details_button = gtk_button_new_with_mnemonic(_("Show _Details"));
    gtk_dialog_add_action_widget (GTK_DIALOG (dialog), details_button, GTK_RESPONSE_YES);
    g_signal_connect (GTK_OBJECT (details_button), "clicked",
		      G_CALLBACK (on_details_button_clicked),
		      NULL);
  }
  
  info_ok_button = gtk_button_new_from_stock ("gtk-ok");
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), info_ok_button, GTK_RESPONSE_OK);


  if (mainw->iochan==NULL) {
    lives_widget_set_can_focus_and_default (info_ok_button);
    gtk_widget_grab_focus (info_ok_button);
    gtk_widget_grab_default (info_ok_button);
  }

  g_signal_connect (GTK_OBJECT (info_ok_button), "clicked",
		    G_CALLBACK (lives_general_button_clicked),
		    NULL);

  gtk_widget_add_accelerator (info_ok_button, "activate", accel_group,
			      LIVES_KEY_Return, (GdkModifierType)0, (GtkAccelFlags)0);



  gtk_widget_show_all(dialog);
  if (is_blocking) gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);

  if (prefs->present) {
    gtk_window_present (GTK_WINDOW (dialog));
    gdk_window_raise (lives_widget_get_xwindow(dialog));
  }

  return dialog;
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

  textwindow=(text_window *)g_malloc(sizeof(text_window));

  textwindow->dialog = lives_standard_dialog_new ((tmp=g_strconcat ("LiVES: - ",mytitle,NULL)),FALSE);
  g_free(tmp);

  if (prefs->show_gui) {
    gtk_window_set_transient_for(GTK_WINDOW(textwindow->dialog),mainw->multitrack==NULL?
				 GTK_WINDOW(mainw->LiVES):GTK_WINDOW(mainw->multitrack->window));
  }

  dialog_vbox = lives_dialog_get_content_area(GTK_DIALOG(textwindow->dialog));

  scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
  gtk_box_pack_start (GTK_BOX (dialog_vbox), scrolledwindow, TRUE, TRUE, 0);
  gtk_widget_set_size_request (scrolledwindow, RFX_WINSIZE_H, RFX_WINSIZE_V);
  
  if (mainw->iochan!=NULL) {
    textwindow->textview=GTK_WIDGET(mainw->optextview);
  }
  else {
    if (textbuffer!=NULL) textwindow->textview = gtk_text_view_new_with_buffer(textbuffer);
    else textwindow->textview = gtk_text_view_new ();
    gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (textwindow->textview), GTK_WRAP_WORD);
  }

  gtk_container_add (GTK_CONTAINER (scrolledwindow), textwindow->textview);

  gtk_text_view_set_editable (GTK_TEXT_VIEW (textwindow->textview), FALSE);
  gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (textwindow->textview), FALSE);

  if (palette->style&STYLE_1) {
    lives_widget_set_base_color(textwindow->textview, GTK_STATE_NORMAL, &palette->info_base);
    lives_widget_set_text_color(textwindow->textview, GTK_STATE_NORMAL, &palette->info_text);
  }

  if (mytext!=NULL) {
    gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (textwindow->textview)), mytext, -1);
  }

  if (mytext!=NULL||mainw->iochan!=NULL) {
    dialog_action_area = lives_dialog_get_action_area(LIVES_DIALOG (textwindow->dialog));
    gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog_action_area), GTK_BUTTONBOX_END);

    okbutton = gtk_button_new_with_mnemonic (_("_Close Window"));

    GtkWidget *savebutton = gtk_button_new_with_mnemonic (_("_Save to file"));
    gtk_dialog_add_action_widget (GTK_DIALOG (textwindow->dialog), savebutton, GTK_RESPONSE_YES);
    gtk_dialog_add_action_widget (GTK_DIALOG (textwindow->dialog), okbutton, GTK_RESPONSE_OK);
    
    g_signal_connect (GTK_OBJECT (savebutton), "clicked",
		      G_CALLBACK (on_save_textview_clicked),
		      textwindow->textview);
    
    g_signal_connect (GTK_OBJECT (okbutton), "clicked",
		      G_CALLBACK (lives_general_button_clicked),
		      textwindow);
    
  }

  if (mytitle!=NULL) g_free(mytitle);
  if (mytext!=NULL) g_free(mytext);

  if (prefs->show_gui)
    gtk_widget_show_all(textwindow->dialog);

  return textwindow;
}




void add_to_winmenu(void) {
  // TODO - change to add_to_clipmenu, move to gui.c
  GtkWidget *active_image;
  gchar *tmp;

  cfile->menuentry = gtk_image_menu_item_new_with_label(cfile->clip_type!=CLIP_TYPE_VIDEODEV?
							(tmp=g_path_get_basename(cfile->name)):
							(tmp=g_strdup(cfile->name)));
  g_free(tmp);

  lives_image_menu_item_set_always_show_image(LIVES_IMAGE_MENU_ITEM(cfile->menuentry),TRUE);

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




void remove_from_winmenu(void) {
  gtk_container_remove(GTK_CONTAINER(mainw->winmenu), cfile->menuentry);
  if (LIVES_IS_WIDGET(cfile->menuentry))
    gtk_widget_destroy(cfile->menuentry);
  mainw->cliplist=g_list_remove (mainw->cliplist, GINT_TO_POINTER (mainw->current_file));
  if (cfile->clip_type==CLIP_TYPE_DISK||cfile->clip_type==CLIP_TYPE_FILE) {
    mainw->clips_available--;
    if (prefs->crash_recovery) rewrite_recovery_file();
  }

}


_insertw* create_insert_dialog (void) {
  GtkWidget *dialog_vbox;
  GtkWidget *hbox1;
  GtkWidget *hbox;
  GtkWidget *table;
  GtkWidget *radiobutton;
  GtkWidget *vseparator;
  GtkWidget *dialog_action_area;
  GtkWidget *cancelbutton;
  GtkWidget *okbutton;

  GSList *radiobutton1_group = NULL;
  GSList *radiobutton2_group = NULL;

  GtkAccelGroup *accel_group=GTK_ACCEL_GROUP(gtk_accel_group_new ());

  gchar *tmp,*tmp2;

  _insertw *insertw=(_insertw*)(g_malloc(sizeof(_insertw)));

  insertw->insert_dialog = lives_standard_dialog_new (_("LiVES: - Insert"),FALSE);

  gtk_window_add_accel_group (GTK_WINDOW (insertw->insert_dialog), accel_group);

  if (prefs->show_gui) {
    gtk_window_set_transient_for(GTK_WINDOW(insertw->insert_dialog),GTK_WINDOW(mainw->LiVES));
  }

  dialog_vbox = lives_dialog_get_content_area(GTK_DIALOG(insertw->insert_dialog));

  hbox1 = lives_hbox_new (FALSE, 0);

  gtk_box_pack_start (GTK_BOX (dialog_vbox), hbox1, TRUE, TRUE, widget_opts.packing_height);

  hbox = lives_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox1), hbox, FALSE, FALSE, widget_opts.packing_width);

  insertw->spinbutton_times = lives_standard_spin_button_new(_("_Number of times to insert"),
							     TRUE,1.,1.,10000.,1.,10.,0.,LIVES_BOX(hbox),NULL);

  add_fill_to_box(GTK_BOX(hbox1));

  hbox = lives_hbox_new (FALSE, 0);

  gtk_box_pack_start (GTK_BOX (hbox1), hbox, FALSE, FALSE, widget_opts.packing_width);

  insertw->fit_checkbutton = lives_standard_check_button_new (_("_Insert to fit audio"),TRUE,LIVES_BOX(hbox),NULL);

  gtk_widget_set_sensitive(GTK_WIDGET(insertw->fit_checkbutton),cfile->achans>0&&clipboard->achans==0);

  add_hsep_to_box (LIVES_BOX (dialog_vbox));

  table = gtk_table_new (2, 3, FALSE);
  gtk_box_pack_start (GTK_BOX (dialog_vbox), table, TRUE, TRUE, widget_opts.packing_height);
  gtk_table_set_col_spacings (GTK_TABLE (table), widget_opts.packing_height*4+2);
  gtk_table_set_row_spacings (GTK_TABLE (table), widget_opts.packing_width);


  hbox = lives_hbox_new (FALSE, 0);

  radiobutton=lives_standard_radio_button_new((tmp=g_strdup(_ ("Insert _before selection"))),
					      TRUE,radiobutton1_group,LIVES_BOX(hbox),
					      (tmp2=g_strdup(_("Insert clipboard before selected frames"))));

  g_free(tmp); g_free(tmp2);

  radiobutton1_group = lives_radio_button_get_group (LIVES_RADIO_BUTTON (radiobutton));

  gtk_table_attach (GTK_TABLE (table), hbox, 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  if (cfile->frames==0) {
    gtk_widget_set_sensitive (radiobutton, FALSE);
  }

  hbox = lives_hbox_new (FALSE, 0);

  radiobutton=lives_standard_radio_button_new((tmp=g_strdup(_("Insert _after selection"))),
					      TRUE,radiobutton1_group,LIVES_BOX(hbox),
					      (tmp2=g_strdup(_("Insert clipboard after selected frames"))));

  gtk_table_attach (GTK_TABLE (table), hbox, 0, 1, 1, 2,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(radiobutton),mainw->insert_after);


  hbox = lives_hbox_new (FALSE, 0);

  insertw->with_sound=lives_standard_radio_button_new(_("Insert _with sound"),
						      TRUE,radiobutton2_group,LIVES_BOX(hbox),NULL);
  radiobutton2_group = lives_radio_button_get_group (LIVES_RADIO_BUTTON (insertw->with_sound));


  gtk_table_attach (GTK_TABLE (table), hbox, 2, 3, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  if (cfile->achans==0&&clipboard->achans==0) gtk_widget_set_sensitive(insertw->with_sound,FALSE);

  hbox = lives_hbox_new (FALSE, 0);

  insertw->without_sound=lives_standard_radio_button_new(_("Insert with_out sound"),
							 TRUE,radiobutton2_group,LIVES_BOX(hbox),NULL);

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(insertw->without_sound),
				 !((cfile->achans>0||clipboard->achans>0)&&mainw->ccpd_with_sound));

  gtk_table_attach (GTK_TABLE (table), hbox, 2, 3, 1, 2,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  gtk_widget_set_sensitive (insertw->with_sound, clipboard->achans>0||cfile->achans>0);
  gtk_widget_set_sensitive (insertw->without_sound, clipboard->achans>0||cfile->achans>0);

  vseparator = lives_vseparator_new ();
  gtk_table_attach (GTK_TABLE (table), vseparator, 1, 2, 0, 1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (GTK_FILL), 0, 0);

  vseparator = lives_vseparator_new ();
  gtk_table_attach (GTK_TABLE (table), vseparator, 1, 2, 1, 2,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (GTK_FILL), 0, 0);

  dialog_action_area = lives_dialog_get_action_area(LIVES_DIALOG (insertw->insert_dialog));

  gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog_action_area), GTK_BUTTONBOX_END);

  cancelbutton = gtk_button_new_from_stock ("gtk-cancel");
  gtk_dialog_add_action_widget (GTK_DIALOG (insertw->insert_dialog), cancelbutton, GTK_RESPONSE_CANCEL);

  okbutton = gtk_button_new_from_stock ("gtk-ok");
  gtk_dialog_add_action_widget (GTK_DIALOG (insertw->insert_dialog), okbutton, GTK_RESPONSE_OK);
  lives_widget_set_can_focus_and_default (okbutton);
  gtk_widget_grab_default(okbutton);
  gtk_widget_grab_focus(okbutton);

  g_signal_connect (GTK_OBJECT (insertw->with_sound), "toggled",
		    G_CALLBACK (on_insertwsound_toggled),
		    NULL);
  g_signal_connect (GTK_OBJECT (radiobutton), "toggled",
		    G_CALLBACK (on_boolean_toggled),
		    &mainw->insert_after);
  g_signal_connect (GTK_OBJECT (cancelbutton), "clicked",
		    G_CALLBACK (lives_general_button_clicked),
		    insertw);
  g_signal_connect (GTK_OBJECT (okbutton), "clicked",
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

  gtk_widget_add_accelerator (cancelbutton, "activate", accel_group,
                              LIVES_KEY_Escape,  (GdkModifierType)0, (GtkAccelFlags)0);

  gtk_widget_add_accelerator (okbutton, "activate", accel_group,
                              LIVES_KEY_Return,  (GdkModifierType)0, (GtkAccelFlags)0);

  gtk_widget_show_all(insertw->insert_dialog);

  return insertw;
}





GtkWidget *create_opensel_dialog (void) {
  GtkWidget *opensel_dialog;
  GtkWidget *dialog_vbox;
  GtkWidget *vbox;
  GtkWidget *table;
  GtkWidget *label;
  GtkWidget *spinbutton;
  GtkWidget *dialog_action_area;
  GtkWidget *cancelbutton;
  GtkWidget *okbutton;

  opensel_dialog = lives_standard_dialog_new (_("LiVES: - Open Selection"),FALSE);

  if (prefs->show_gui) {
    if (mainw->multitrack==NULL) gtk_window_set_transient_for(GTK_WINDOW(opensel_dialog),GTK_WINDOW(mainw->LiVES));
    else gtk_window_set_transient_for(GTK_WINDOW(opensel_dialog),GTK_WINDOW(mainw->multitrack->window));
  }


  dialog_vbox = lives_dialog_get_content_area(GTK_DIALOG(opensel_dialog));
 
  vbox = lives_vbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (dialog_vbox), vbox, TRUE, TRUE, 0);

  table = gtk_table_new (2, 2, FALSE);
  gtk_box_pack_start (GTK_BOX (vbox), table, TRUE, TRUE, 12);

  gtk_table_set_row_spacings (GTK_TABLE (table), 20);

  label = lives_standard_label_new (_("    Selection start time (sec)"));
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

  label = lives_standard_label_new (_("    Number of frames to open"));
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 1, 2,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

  spinbutton = lives_standard_spin_button_new (NULL, FALSE, 0., 0., 1000000000., 1., 10., 2, NULL, NULL);

  g_signal_connect_after (GTK_OBJECT (spinbutton), "value_changed",
                            G_CALLBACK (on_spin_value_changed),
                            GINT_TO_POINTER (1));

  gtk_table_attach (GTK_TABLE (table), spinbutton, 1, 2, 0, 1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (GTK_EXPAND), widget_opts.packing_height*4+2, 0);


  spinbutton = lives_standard_spin_button_new (NULL,FALSE,1000.,1.,(double)G_MAXINT, 1., 10., 0, NULL, NULL);

  g_signal_connect_after (GTK_OBJECT (spinbutton), "value_changed",
			  G_CALLBACK (on_spin_value_changed),
			  GINT_TO_POINTER (2));

  gtk_table_attach (GTK_TABLE (table), spinbutton, 1, 2, 1, 2,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (GTK_EXPAND), widget_opts.packing_height*4+2, 0);

  dialog_action_area = lives_dialog_get_action_area(LIVES_DIALOG (opensel_dialog));
  gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog_action_area), GTK_BUTTONBOX_END);

  cancelbutton = gtk_button_new_from_stock ("gtk-cancel");
  gtk_dialog_add_action_widget (GTK_DIALOG (opensel_dialog), cancelbutton, GTK_RESPONSE_CANCEL);

  okbutton = gtk_button_new_from_stock ("gtk-ok");
  gtk_dialog_add_action_widget (GTK_DIALOG (opensel_dialog), okbutton, GTK_RESPONSE_OK);
  lives_widget_set_can_focus_and_default (okbutton);
  gtk_widget_grab_default(okbutton);

  widget_add_preview (opensel_dialog, GTK_BOX (dialog_vbox), GTK_BOX (dialog_vbox), GTK_BOX(dialog_vbox), 3);

  g_signal_connect (GTK_OBJECT (cancelbutton), "clicked",
		    G_CALLBACK (on_cancel_opensel_clicked),
		    NULL);
  g_signal_connect (GTK_OBJECT (okbutton), "clicked",
		    G_CALLBACK (on_opensel_range_ok_clicked),
		    NULL);

  gtk_widget_show_all(opensel_dialog);

  return opensel_dialog;
}





_entryw* create_location_dialog (int type) {
  // type 1 is open location
  // type 2 is open youtube: - 3 fields:= URL, directory, file name

  GtkWidget *dialog_vbox;
  GtkWidget *dialog_action_area;
  GtkWidget *cancelbutton;
  GtkWidget *okbutton;
  GtkWidget *label;
  GtkWidget *checkbutton;
  GtkWidget *hbox;
  GtkWidget *buttond;

  _entryw *locw=(_entryw*)(g_malloc(sizeof(_entryw)));

  GtkAccelGroup *accel_group=GTK_ACCEL_GROUP(gtk_accel_group_new ());

  gchar *title,*tmp,*tmp2;

  if (type==1) 
    title=g_strdup(_("LiVES: - Open Location"));
  else 
    title=g_strdup(_("LiVES: - Open Youtube Clip"));

  locw->dialog = lives_standard_dialog_new (title,FALSE);

  g_free(title);

  gtk_window_add_accel_group (GTK_WINDOW (locw->dialog), accel_group);

  if (prefs->show_gui) {
    if (mainw->multitrack==NULL) gtk_window_set_transient_for(GTK_WINDOW(locw->dialog),GTK_WINDOW(mainw->LiVES));
    else gtk_window_set_transient_for(GTK_WINDOW(locw->dialog),GTK_WINDOW(mainw->multitrack->window));
  }

  dialog_vbox = lives_dialog_get_content_area(GTK_DIALOG(locw->dialog));

  widget_opts.justify=LIVES_JUSTIFY_CENTER;

  if (type==1) {
    label = lives_standard_label_new (_("\n\nTo open a stream, you must make sure that you have the correct libraries compiled in mplayer.\nAlso make sure you have set your bandwidth in Preferences|Streaming\n\n"));
  }
  else { 
    label = lives_standard_label_new (_("\n\nTo open a clip from Youtube, LiVES will first download it with youtube-dl.\nPlease make sure you have the latest version of that tool installed.\n\n"));

    gtk_box_pack_start (GTK_BOX (dialog_vbox), label, FALSE, FALSE, 0);
    
    label=lives_standard_label_new(_("Enter the URL of the clip below.\nE.g: http://www.youtube.com/watch?v=WCR6f6WzjP8\n\n"));

  }

  widget_opts.justify=LIVES_JUSTIFY_DEFAULT;

  gtk_box_pack_start (GTK_BOX (dialog_vbox), label, FALSE, FALSE, 0);

  hbox = lives_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (dialog_vbox), hbox, TRUE, TRUE, widget_opts.packing_height*2);

  locw->entry = lives_standard_entry_new (type==1?_ ("URL : "):_ ("Youtube URL : "),FALSE,"",79,32768,LIVES_BOX(hbox),NULL);

  if (type==1) {
    hbox=lives_hbox_new (FALSE, 0);
    checkbutton = lives_standard_check_button_new ((tmp=g_strdup(_("Do not send bandwidth information"))),
						   TRUE,LIVES_BOX(hbox),
						   (tmp2=g_strdup(_("Try this setting if you are having problems getting a stream"))));

    g_free(tmp); g_free(tmp2);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbutton),prefs->no_bandwidth);
    
    gtk_box_pack_start (GTK_BOX(dialog_vbox), hbox, FALSE, FALSE, widget_opts.packing_height*2);
    
    g_signal_connect (GTK_OBJECT (checkbutton), "toggled",
		      G_CALLBACK (on_boolean_toggled),
		      &prefs->no_bandwidth);
    
    add_deinterlace_checkbox(GTK_BOX(dialog_vbox));

  }

  if (type==2) {
    hbox=lives_hbox_new (FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(dialog_vbox),hbox,TRUE,FALSE,widget_opts.packing_height);

    locw->dir_entry = lives_standard_entry_new (_("Download to _Directory : "),TRUE,"",72,PATH_MAX,LIVES_BOX(hbox),NULL);

    // add dir, with filechooser button
    buttond = gtk_file_chooser_button_new(_("Download Directory..."),GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(buttond),mainw->vid_save_dir);
    gtk_box_pack_start(GTK_BOX(hbox),buttond,TRUE,FALSE,0);

    add_fill_to_box (GTK_BOX (hbox));
    //gtk_file_chooser_button_set_width_chars(GTK_FILE_CHOOSER_BUTTON(buttond),16);
    
    g_signal_connect (GTK_FILE_CHOOSER(buttond), "selection-changed",G_CALLBACK (on_fileread_clicked),
    		      (gpointer)locw->dir_entry);


    hbox=lives_hbox_new (FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(dialog_vbox),hbox,TRUE,FALSE,widget_opts.packing_height);

    locw->name_entry = lives_standard_entry_new (_("Download _File Name : "),TRUE,"",74,32768,LIVES_BOX(hbox),NULL);

    label=lives_standard_label_new (_(".webm"));

    gtk_box_pack_start(GTK_BOX(hbox),label,FALSE,FALSE,widget_opts.packing_width);
   }



  dialog_action_area = lives_dialog_get_action_area(LIVES_DIALOG (locw->dialog));
  gtk_widget_show (dialog_action_area);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog_action_area), GTK_BUTTONBOX_END);

  cancelbutton = gtk_button_new_from_stock ("gtk-cancel");
  gtk_widget_show (cancelbutton);
  gtk_dialog_add_action_widget (GTK_DIALOG (locw->dialog), cancelbutton, GTK_RESPONSE_CANCEL);
  lives_widget_set_can_focus_and_default (cancelbutton);

  okbutton = gtk_button_new_from_stock ("gtk-ok");
  gtk_widget_show (okbutton);
  gtk_dialog_add_action_widget (GTK_DIALOG (locw->dialog), okbutton, GTK_RESPONSE_OK);
  lives_widget_set_can_focus_and_default (okbutton);
  gtk_widget_grab_default (okbutton);


  g_signal_connect (GTK_OBJECT (cancelbutton), "clicked",
		    G_CALLBACK (lives_general_button_clicked),
		    locw);

  if (type==1) 
    g_signal_connect (GTK_OBJECT (okbutton), "clicked",
		      G_CALLBACK (on_location_select),
		      NULL);

  else if (type==2) 
    g_signal_connect (GTK_OBJECT (okbutton), "clicked",
		      G_CALLBACK (on_utube_select),
		      NULL);

  g_signal_connect (GTK_OBJECT (locw->dialog), "delete_event",
                      G_CALLBACK (return_true),
                      NULL);

  gtk_widget_add_accelerator (cancelbutton, "activate", accel_group,
                              LIVES_KEY_Escape, (GdkModifierType)0, (GtkAccelFlags)0);

  gtk_widget_show_all(locw->dialog);

  return locw;
}

#define RW_ENTRY_DISPWIDTH 40

_entryw* create_rename_dialog (int type) {
  // type 1 = rename clip in menu
  // type 2 = save clip set
  // type 3 = reload clip set
  // type 4 = save clip set from mt
  // type 5 = save clip set for project export

  // type 6 = initial tempdir

  // type 7 = rename track in mt

  GtkWidget *dialog_vbox;
  GtkWidget *hbox;
  GtkWidget *label;
  GtkWidget *dialog_action_area;
  GtkWidget *cancelbutton;
  GtkWidget *okbutton;
  GtkWidget *set_combo;
  GtkWidget *dirbutton1;
  GtkWidget *dirimage1;

  GtkAccelGroup *accel_group=GTK_ACCEL_GROUP(gtk_accel_group_new ());

  gchar *title=NULL;

  _entryw *renamew=(_entryw*)(g_malloc(sizeof(_entryw)));

  renamew->setlist=NULL;

  if (type==1) {
    title=g_strdup(_("LiVES: - Rename Clip"));
  }
  else if (type==2||type==4||type==5) {
    title=g_strdup(_("LiVES: - Enter Set Name"));
  }
  else if (type==3) {
    title=g_strdup(_("LiVES: - Enter a Set Name to Reload"));
  }
  else if (type==6) {
    title=g_strdup(_("LiVES: - Choose a Working Directory"));
  }
  else if (type==7) {
    title=g_strdup(_("LiVES: - Rename Current Track"));
  }

  renamew->dialog = lives_standard_dialog_new (title,FALSE);

  gtk_window_add_accel_group (GTK_WINDOW (renamew->dialog), accel_group);

  if (prefs->show_gui) {
    if (mainw->multitrack==NULL) {
      if (mainw->is_ready) {
	gtk_window_set_transient_for(GTK_WINDOW(renamew->dialog),GTK_WINDOW(mainw->LiVES));
      }
    }
    else gtk_window_set_transient_for(GTK_WINDOW(renamew->dialog),GTK_WINDOW(mainw->multitrack->window));
  }

  dialog_vbox = lives_dialog_get_content_area(GTK_DIALOG(renamew->dialog));

  if (type==4) {
    label = lives_standard_label_new 
      (_("You need to enter a name for the current clip set.\nThis will allow you reload the layout with the same clips later.\nPlease enter the set name you wish to use.\nLiVES will remind you to save the clip set later when you try to exit.\n"));
    gtk_box_pack_start (GTK_BOX (dialog_vbox), label, FALSE, FALSE, 0);
  }

  if (type==5) {
    label = lives_standard_label_new 
      (_("In order to export this project, you must enter a name for this clip set.\nThis will also be used for the project name.\n"));
    gtk_box_pack_start (GTK_BOX (dialog_vbox), label, FALSE, FALSE, 0);
  }


  if (type==6) {
    label = lives_standard_label_new 
      (_("Welcome to LiVES !\nThis startup wizard will guide you through the\ninitial install so that you can get the most from this application.\n"));
    gtk_box_pack_start (GTK_BOX (dialog_vbox), label, FALSE, FALSE, 0);

    label = lives_standard_label_new 
      (_("\nFirst of all you need to choose a working directory for LiVES.\nThis should be a directory with plenty of disk space available.\n"));
    gtk_box_pack_start (GTK_BOX (dialog_vbox), label, FALSE, FALSE, 0);
  }


  hbox = lives_hbox_new (FALSE, 0);

  if (type==3) {
    gtk_box_pack_start (GTK_BOX (dialog_vbox), hbox, TRUE, FALSE, widget_opts.packing_height*4);
  }
  else if (type!=6&&type!=7&&type!=1) {
    gtk_box_pack_start (GTK_BOX (dialog_vbox), hbox, TRUE, TRUE, widget_opts.packing_height*2);
  }
  else {
    gtk_box_pack_start (GTK_BOX (dialog_vbox), hbox, TRUE, TRUE, widget_opts.packing_height*4);
  }


  if (type==1||type==7) {
    label = lives_standard_label_new (_("New name "));
  }
  else if (type==2||type==3||type==4||type==5) {
    label = lives_standard_label_new (_("Set name "));
  }
  else {
    label = lives_standard_label_new ("");
  }

  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, widget_opts.packing_width*4);


  if (type==3) {
    GtkListStore *store;
    GtkEntryCompletion *completion;
    GList *xlist;

    set_combo=lives_combo_new();

    renamew->setlist=get_set_list(prefs->tmpdir);

    lives_combo_populate(LIVES_COMBO(set_combo),renamew->setlist);

    renamew->entry=lives_combo_get_entry(LIVES_COMBO(set_combo));

    if (strlen(prefs->ar_clipset_name)) {
      // set default to our auto-reload clipset
      gtk_entry_set_text(GTK_ENTRY(renamew->entry),prefs->ar_clipset_name);
    }

    gtk_box_pack_start (GTK_BOX (hbox), set_combo, TRUE, TRUE, 0);

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
    renamew->entry = gtk_entry_new();
    gtk_entry_set_max_length (GTK_ENTRY(renamew->entry),type==6?PATH_MAX:type==7?16:128);
    if (type==2&&strlen (mainw->set_name)) {
      gtk_entry_set_text (GTK_ENTRY (renamew->entry),mainw->set_name);
    }
    if (type==6) {
      gchar *tmpdir;
      if (prefs->startup_phase==-1) tmpdir=g_build_filename(capable->home_dir,LIVES_TMP_NAME,NULL);
      else tmpdir=g_strdup(prefs->tmpdir);
      gtk_entry_set_text (GTK_ENTRY (renamew->entry),tmpdir);
      g_free(tmpdir);
    }
    gtk_box_pack_start (GTK_BOX (hbox), renamew->entry, TRUE, TRUE, 0);
  }


  if (type==6) {
    dirbutton1 = gtk_button_new ();
    
    dirimage1 = gtk_image_new_from_stock ("gtk-open", GTK_ICON_SIZE_BUTTON);

    gtk_container_add (GTK_CONTAINER (dirbutton1), dirimage1);

    gtk_box_pack_start (GTK_BOX (hbox), dirbutton1, FALSE, TRUE, widget_opts.packing_width);
    g_signal_connect(dirbutton1, "clicked", G_CALLBACK (on_filesel_complex_clicked),renamew->entry);

  }


  gtk_entry_set_activates_default (GTK_ENTRY (renamew->entry), TRUE);
  gtk_entry_set_width_chars (GTK_ENTRY (renamew->entry),RW_ENTRY_DISPWIDTH);

  dialog_action_area = lives_dialog_get_action_area(LIVES_DIALOG (renamew->dialog));

  gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog_action_area), GTK_BUTTONBOX_END);

  cancelbutton = gtk_button_new_from_stock ("gtk-cancel");

  gtk_dialog_add_action_widget (GTK_DIALOG (renamew->dialog), cancelbutton, GTK_RESPONSE_CANCEL);
  lives_widget_set_can_focus_and_default (cancelbutton);

  gtk_widget_add_accelerator (cancelbutton, "activate", accel_group,
			      LIVES_KEY_Escape, (GdkModifierType)0, (GtkAccelFlags)0);

  if (type==6) {
    okbutton = gtk_button_new_from_stock ("gtk-go-forward");
    gtk_button_set_label(GTK_BUTTON(okbutton),_("_Next"));
  }
  else okbutton = gtk_button_new_from_stock ("gtk-ok");

  gtk_dialog_add_action_widget (GTK_DIALOG (renamew->dialog), okbutton, GTK_RESPONSE_OK);
  lives_widget_set_can_focus_and_default (okbutton);
  gtk_widget_grab_default (okbutton);

  gtk_widget_grab_focus (renamew->entry);


  if (type!=4&&type!=2&&type!=5) {
    g_signal_connect (GTK_OBJECT (cancelbutton), "clicked",
		      G_CALLBACK (lives_general_button_clicked),
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


  gtk_widget_add_accelerator (cancelbutton, "activate", accel_group,
                              LIVES_KEY_Escape, (GdkModifierType)0, (GtkAccelFlags)0);

  gtk_widget_show_all(renamew->dialog);

  return renamew;
}


void on_liveinp_advanced_clicked (GtkButton *button, gpointer user_data) {
  lives_tvcardw_t *tvcardw=(lives_tvcardw_t *)(user_data);

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

  gtk_widget_queue_resize(lives_widget_get_parent(tvcardw->adv_vbox));

}


static void rb_tvcarddef_toggled(GtkToggleButton *tbut, gpointer user_data) {
  lives_tvcardw_t *tvcardw=(lives_tvcardw_t *)(user_data);

  if (!lives_toggle_button_get_active(tbut)) {
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
  gchar *etext=lives_combo_get_active_text(LIVES_COMBO(combo));
  mainw->fx1_val=lives_list_index(list,etext);
  g_free(etext);
}


GtkWidget *create_combo_dialog (int type, gpointer user_data) {
  // create a dialog with combo box selector

  // type 1 == 1 combo box

  GtkWidget *combo_dialog;
  GtkWidget *dialog_vbox;
  GtkWidget *label;
  GtkWidget *combo;

  gchar *label_text=NULL,*title=NULL;

  GList *list=(GList *)user_data;

  if (type==1) {
    title=g_strdup(_("LiVES:- Select input device"));
  }

  combo_dialog = lives_standard_dialog_new (title,TRUE);
  if (title!=NULL) g_free(title);

  if (prefs->show_gui) {
    if (type==1) {
      gtk_window_set_transient_for(GTK_WINDOW(combo_dialog),GTK_WINDOW(mainw->LiVES));
    }
    else {
      gtk_window_set_transient_for(GTK_WINDOW(combo_dialog),GTK_WINDOW(mainw->multitrack->window));
    }
  }

  dialog_vbox = lives_dialog_get_content_area(GTK_DIALOG(combo_dialog));

  if (type==1) {
    label_text=g_strdup(_("Select input device:"));
  }

  label = lives_standard_label_new (label_text);
  if (label_text!=NULL) g_free(label_text);

  gtk_box_pack_start (GTK_BOX (dialog_vbox), label, TRUE, TRUE, 0);

  combo = lives_combo_new();

  gtk_entry_set_width_chars (GTK_ENTRY (lives_combo_get_entry(LIVES_COMBO(combo))), 64);

  lives_combo_populate(LIVES_COMBO(combo),list);

  gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);

  g_signal_connect_after (G_OBJECT (combo), "changed", G_CALLBACK (after_dialog_combo_changed), list);

  gtk_box_pack_start (GTK_BOX (dialog_vbox), combo, TRUE, TRUE, 20);

  if (type==1) {
    add_deinterlace_checkbox(GTK_BOX(dialog_vbox));
  }

  if (prefs->show_gui)
    gtk_widget_show_all(combo_dialog);

  return combo_dialog;
}


GtkWidget* create_cdtrack_dialog (int type, gpointer user_data) {
  // general purpose dialog with label and up to 2 spinbuttons

  // type 0 = cd track
  // type 1 = dvd title/chapter/aid
  // type 2 = vcd title -- do we need chapter as well ?
  // type 3 = number of tracks in mt


  // type 4 = TV card (device and channel)
  // type 5 = fw card

  // TODO - add pref for dvd/vcd device

  // TODO - for CD make this nicer - get track names
  lives_tvcardw_t *tvcardw=NULL;

  GtkWidget *cd_dialog;
  GtkWidget *dialog_vbox;
  GtkWidget *hbox;
  GtkWidget *spinbutton;
  GtkWidget *dialog_action_area;
  GtkWidget *cancelbutton;
  GtkWidget *okbutton;

  GtkAccelGroup *accel_group=GTK_ACCEL_GROUP(gtk_accel_group_new ());
  
  GSList *radiobutton_group=NULL;

  gchar *label_text=NULL,*title;
 

  if (type==0) {
    title=g_strdup(_("LiVES:- Load CD Track"));
  }
  else if (type==1) {
    title=g_strdup(_("LiVES:- Select DVD Title/Chapter"));
  }
  else if (type==2) {
    title=g_strdup(_("LiVES:- Select VCD Title"));
  }
  else if (type==3) {
    title=g_strdup(_("LiVES:- Change Maximum Visible Tracks"));
  }
  else {
    title=g_strdup(_("LiVES:- Device details"));
  }

  cd_dialog = lives_standard_dialog_new (title,FALSE);
  g_free(title);

  //gtk_window_set_default_size (GTK_WINDOW (cd_dialog), 300, 240);

  if (prefs->show_gui) {
    if (type==0||type==1||type==2||type==4||type==5) {
      gtk_window_set_transient_for(GTK_WINDOW(cd_dialog),GTK_WINDOW(mainw->LiVES));
    }
    else {
      gtk_window_set_transient_for(GTK_WINDOW(cd_dialog),GTK_WINDOW(mainw->multitrack->window));
    }
  }

  dialog_vbox = lives_dialog_get_content_area(GTK_DIALOG(cd_dialog));

  hbox = lives_hbox_new (FALSE, widget_opts.packing_width*5);
  gtk_box_pack_start (GTK_BOX (dialog_vbox), hbox, TRUE, TRUE, widget_opts.packing_height);

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


  if (type==0||type==1||type==2) {
    spinbutton = lives_standard_spin_button_new (label_text,FALSE, mainw->fx1_val,
						 1., 256., 1., 10., 0,
						 LIVES_BOX(hbox),NULL);
  }
  else if (type==3) {
    spinbutton = lives_standard_spin_button_new (label_text,FALSE, mainw->fx1_val,
						 4., 8., 1., 1.,0,
						 LIVES_BOX(hbox),NULL);
  }
  else {
    spinbutton = lives_standard_spin_button_new (label_text,FALSE, 0.,
						 0., 31., 1., 1., 0, 
						 LIVES_BOX(hbox),NULL);
  }

  g_free(label_text);

  g_signal_connect_after (GTK_OBJECT (spinbutton), "value_changed",
			  G_CALLBACK (on_spin_value_changed),
			  GINT_TO_POINTER (1));


  add_fill_to_box(GTK_BOX(hbox));

  if (type==1||type==4) {

    hbox = lives_hbox_new (FALSE, widget_opts.packing_width*5);
    gtk_box_pack_start (GTK_BOX (dialog_vbox), hbox, TRUE, TRUE, widget_opts.packing_height);

    if (type==1) {
      spinbutton = lives_standard_spin_button_new (_("Chapter  "), FALSE, mainw->fx2_val,
						   1., 1024., 1., 10., 0, 
						   LIVES_BOX(hbox),NULL);
    }
    else {
      spinbutton = lives_standard_spin_button_new (_("Channel  "), FALSE, 1.,
						   1., 69., 1., 1., 0, 
						   LIVES_BOX(hbox),NULL);

    }

    g_signal_connect_after (GTK_OBJECT (spinbutton), "value_changed",
			    G_CALLBACK (on_spin_value_changed),
			    GINT_TO_POINTER (2));


    if (type==1) {
      hbox = lives_hbox_new (FALSE, widget_opts.packing_width*5);
      gtk_box_pack_start (GTK_BOX (dialog_vbox), hbox, TRUE, TRUE, widget_opts.packing_height);
      
      spinbutton = lives_standard_spin_button_new (_("Audio ID  "), FALSE, mainw->fx3_val,
						   128., 159., 1., 1., 0, 
						   LIVES_BOX(hbox),NULL);

      g_signal_connect_after (GTK_OBJECT (spinbutton), "value_changed",
			      G_CALLBACK (on_spin_value_changed),
			      GINT_TO_POINTER (3));

    }
  }

  if (type==4||type==5) {
    hbox=add_deinterlace_checkbox(GTK_BOX(dialog_vbox));
    add_fill_to_box(GTK_BOX(hbox));
  }


  if (type==4) {
    GList *dlist=NULL;
    GList *olist=NULL;

    tvcardw=(lives_tvcardw_t *)g_malloc(sizeof(lives_tvcardw_t));
    tvcardw->use_advanced=FALSE;

    dlist=g_list_append(dlist,(gpointer)"autodetect");
    dlist=g_list_append(dlist,(gpointer)"v4l2");
    dlist=g_list_append(dlist,(gpointer)"v4l");
    dlist=g_list_append(dlist,(gpointer)"bsdbt848");
    dlist=g_list_append(dlist,(gpointer)"dummy");

    olist=g_list_append(olist,(gpointer)"autodetect");
    olist=g_list_append(olist,(gpointer)"yv12");
    olist=g_list_append(olist,(gpointer)"rgb32");
    olist=g_list_append(olist,(gpointer)"rgb24");
    olist=g_list_append(olist,(gpointer)"rgb16");
    olist=g_list_append(olist,(gpointer)"rgb15");
    olist=g_list_append(olist,(gpointer)"uyvy");
    olist=g_list_append(olist,(gpointer)"yuy2");
    olist=g_list_append(olist,(gpointer)"i420");


    lives_box_set_spacing(GTK_BOX(dialog_vbox),widget_opts.packing_height*2);

    hbox = lives_hbox_new (FALSE, widget_opts.packing_width*5);

    gtk_box_pack_start (GTK_BOX (dialog_vbox), hbox, FALSE, FALSE, widget_opts.packing_height*2);
    
    add_fill_to_box(GTK_BOX(hbox));

    tvcardw->advbutton = gtk_button_new_with_mnemonic (_("_Advanced"));

    gtk_box_pack_start (GTK_BOX (hbox), tvcardw->advbutton, TRUE, TRUE, widget_opts.packing_width*4);
    
    add_fill_to_box(GTK_BOX(hbox));


    tvcardw->adv_vbox = lives_vbox_new (FALSE, widget_opts.packing_width*5);
    gtk_box_pack_start (GTK_BOX (dialog_vbox), tvcardw->adv_vbox, TRUE, TRUE, widget_opts.packing_height*2);
    

    // add input, width, height, fps, driver and outfmt


    hbox = lives_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (tvcardw->adv_vbox), hbox, TRUE, FALSE, 0);

    tvcardw->spinbuttoni = lives_standard_spin_button_new (_("Input number"),FALSE,
							   0.,0.,16.,1.,1.,0,
							   LIVES_BOX(hbox),NULL);
							   

    hbox = lives_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (tvcardw->adv_vbox), hbox, TRUE, FALSE, 0);

    tvcardw->radiobuttond = lives_standard_radio_button_new (_("Use default width, height and FPS"),FALSE,
							     radiobutton_group,LIVES_BOX(hbox),NULL);
    radiobutton_group = lives_radio_button_get_group (LIVES_RADIO_BUTTON (tvcardw->radiobuttond));
    
    g_signal_connect_after (GTK_OBJECT (tvcardw->radiobuttond), "toggled",
			    G_CALLBACK (rb_tvcarddef_toggled),
			    (gpointer)tvcardw);

    hbox = lives_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (tvcardw->adv_vbox), hbox, TRUE, FALSE, 0);

    lives_standard_radio_button_new (NULL,FALSE,
				     radiobutton_group,LIVES_BOX(hbox),NULL);
    
    tvcardw->spinbuttonw = lives_standard_spin_button_new (_("Width"),FALSE,
							   640.,4.,4096.,2.,2.,0,
							   LIVES_BOX(hbox),NULL);

    gtk_widget_set_sensitive(tvcardw->spinbuttonw,FALSE);

    tvcardw->spinbuttonh = lives_standard_spin_button_new (_("Height"),FALSE,
							   480.,4.,4096.,2.,2.,0,
							   LIVES_BOX(hbox),NULL);

    gtk_widget_set_sensitive(tvcardw->spinbuttonh,FALSE);
    
    tvcardw->spinbuttonf = lives_standard_spin_button_new (_("FPS"),FALSE,
							   25., 1., FPS_MAX, 1., 10., 3,
							   LIVES_BOX(hbox),NULL);

    gtk_widget_set_sensitive(tvcardw->spinbuttonf,FALSE);

    hbox = lives_hbox_new (FALSE, 0);

    tvcardw->combod = lives_standard_combo_new (_("_Driver"),TRUE,dlist,LIVES_BOX(hbox),NULL);
    gtk_combo_box_set_active(GTK_COMBO_BOX(tvcardw->combod), 0);

    tvcardw->comboo = lives_standard_combo_new (_("_Output format"),TRUE,olist,LIVES_BOX(hbox),NULL);


    gtk_widget_show_all (hbox);
    gtk_box_pack_start (GTK_BOX (tvcardw->adv_vbox), hbox, TRUE, FALSE, 0);

    g_signal_connect (GTK_OBJECT (tvcardw->advbutton), "clicked",
		      G_CALLBACK (on_liveinp_advanced_clicked),
		      tvcardw);

    gtk_widget_hide(tvcardw->adv_vbox);

    g_object_set_data(G_OBJECT(cd_dialog),"tvcard_data",tvcardw);

  }

  dialog_action_area = lives_dialog_get_action_area(LIVES_DIALOG (cd_dialog));
  gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog_action_area), GTK_BUTTONBOX_END);

  cancelbutton = gtk_button_new_from_stock ("gtk-cancel");
  gtk_dialog_add_action_widget (GTK_DIALOG (cd_dialog), cancelbutton, GTK_RESPONSE_CANCEL);

  gtk_widget_add_accelerator (cancelbutton, "activate", accel_group,
                              LIVES_KEY_Escape, (GdkModifierType)0, (GtkAccelFlags)0);

  okbutton = gtk_button_new_from_stock ("gtk-ok");
  gtk_dialog_add_action_widget (GTK_DIALOG (cd_dialog), okbutton, GTK_RESPONSE_OK);
  lives_widget_set_can_focus_and_default (okbutton);
  gtk_widget_grab_default (okbutton);
  
  gtk_widget_add_accelerator (okbutton, "activate", accel_group,
                              LIVES_KEY_Return, (GdkModifierType)0, (GtkAccelFlags)0);

  if (type!=4&&type!=5) {
    g_signal_connect (GTK_OBJECT (cancelbutton), "clicked",
		      G_CALLBACK (lives_general_button_clicked),
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

  gtk_widget_show_all(cd_dialog);

  if (type==4) gtk_widget_hide(tvcardw->adv_vbox);

  return cd_dialog;
}



static void rb_aud_sel_pressed (GtkButton *button, gpointer user_data) {
  aud_dialog_t *audd=(aud_dialog_t *)user_data;
  audd->is_sel=!audd->is_sel;
  gtk_widget_set_sensitive(audd->time_spin,!audd->is_sel);
}




aud_dialog_t *create_audfade_dialog (gint type) {
  // type 0 = fade in
  // type 1 = fade out

  GtkWidget *dialog_vbox;
  GtkWidget *hbox;
  GtkWidget *rb_time;
  GtkWidget *rb_sel;
  GtkWidget *label;

  gchar *label_text=NULL,*label_text2=NULL,*title;

  double max;

  GSList *radiobutton_group = NULL;

  aud_dialog_t *audd=(aud_dialog_t *)g_malloc(sizeof(aud_dialog_t));

  if (type==0) {
    title=g_strdup(_("LiVES:- Fade Audio In"));
  }
  else {
    title=g_strdup(_("LiVES:- Fade Audio Out"));
  }

  audd->dialog = lives_standard_dialog_new (title,TRUE);
  g_free(title);

  if (prefs->show_gui) {
    gtk_window_set_transient_for(GTK_WINDOW(audd->dialog),GTK_WINDOW(mainw->LiVES));
  }

  dialog_vbox = lives_dialog_get_content_area(GTK_DIALOG(audd->dialog));

  hbox = lives_hbox_new (FALSE, 50);
  gtk_box_pack_start (GTK_BOX (dialog_vbox), hbox, TRUE, TRUE, widget_opts.packing_height);

  if (type==0) {
    label_text=g_strdup(_("Fade in over  "));
    label_text2=g_strdup(_("first"));
  }
  else if (type==1) {
    label_text=g_strdup(_("Fade out over  "));
    label_text2=g_strdup(_("last"));
  }


  label = lives_standard_label_new (label_text);
  if (label_text!=NULL) g_free(label_text);

  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

  hbox = lives_hbox_new (FALSE, widget_opts.packing_width*5);
  gtk_box_pack_start (GTK_BOX (dialog_vbox), hbox, TRUE, TRUE, widget_opts.packing_height);

  rb_time=lives_standard_radio_button_new(label_text2,FALSE,radiobutton_group,
					  LIVES_BOX(hbox),NULL);

  radiobutton_group = lives_radio_button_get_group (LIVES_RADIO_BUTTON (rb_time));
  if (label_text2!=NULL) g_free(label_text2);


  max=cfile->laudio_time;

  widget_opts.swap_label=TRUE;
  audd->time_spin = lives_standard_spin_button_new (_("seconds."),FALSE,
						    max/2.>DEF_AUD_FADE_SECS?DEF_AUD_FADE_SECS:max/2., .1, max, 1., 10., 2,
						    LIVES_BOX(hbox),NULL);
  widget_opts.swap_label=FALSE;

  hbox = lives_hbox_new (FALSE, widget_opts.packing_width*5);
  gtk_box_pack_start (GTK_BOX (dialog_vbox), hbox, TRUE, TRUE, widget_opts.packing_height);

  rb_sel=lives_standard_radio_button_new(_("selection"),FALSE,radiobutton_group,LIVES_BOX(hbox),NULL);

  audd->is_sel=FALSE;

  if ((cfile->end-1.)/cfile->fps>cfile->laudio_time) {
    // if selection is longer than audio time, we cannot use sel len
    gtk_widget_set_sensitive(rb_sel,FALSE);
  }
  else {
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(rb_sel),TRUE);
    rb_aud_sel_pressed(GTK_BUTTON(rb_sel),(gpointer)audd);
  }

  g_signal_connect_after (GTK_OBJECT (rb_sel), "toggled",
			  G_CALLBACK (rb_aud_sel_pressed),
			  (gpointer)audd);


  add_fill_to_box(GTK_BOX(hbox));


  gtk_widget_show_all(audd->dialog);
  
  return audd;
}





_commentsw* create_comments_dialog (file *sfile, gchar *filename) {
  GtkWidget *dialog_vbox;
  GtkWidget *table;
  GtkWidget *label;
  GtkWidget *expander;
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *buttond;

  _commentsw *commentsw=(_commentsw*)(g_malloc(sizeof(_commentsw)));

  commentsw->comments_dialog = lives_standard_dialog_new (_("LiVES: - File Comments (optional)"),TRUE);

  if (prefs->show_gui) {
    gtk_window_set_transient_for(GTK_WINDOW(commentsw->comments_dialog),GTK_WINDOW(mainw->LiVES));
  }

  dialog_vbox = lives_dialog_get_content_area(GTK_DIALOG(commentsw->comments_dialog));

  table = gtk_table_new (4, 2, FALSE);
  gtk_container_set_border_width(GTK_CONTAINER(table), widget_opts.border_width);

  gtk_table_set_row_spacings(GTK_TABLE(table), 20);

  gtk_box_pack_start (GTK_BOX (dialog_vbox), table, TRUE, TRUE, widget_opts.packing_height);

  label = lives_standard_label_new (_("Title/Name : "));

  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

  label = lives_standard_label_new (_("Author/Artist : "));

  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 1, 2,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

  label = lives_standard_label_new (_("Comments : "));

  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 3, 4,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

  commentsw->title_entry = lives_standard_entry_new (NULL,FALSE,cfile->title,80,-1,NULL,NULL);

  gtk_table_attach (GTK_TABLE (table), commentsw->title_entry, 1, 2, 0, 1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (GTK_EXPAND), 0, 0);

  commentsw->author_entry = lives_standard_entry_new (NULL,FALSE,cfile->author,80,-1,NULL,NULL);

  gtk_table_attach (GTK_TABLE (table), commentsw->author_entry, 1, 2, 1, 2,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (GTK_EXPAND), 0, 0);

  commentsw->comment_entry = lives_standard_entry_new (NULL,FALSE,cfile->comment,80,250,NULL,NULL);

  gtk_table_attach (GTK_TABLE (table), commentsw->comment_entry, 1, 2, 3, 4,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (GTK_EXPAND), 0, 0);

  if (sfile!=NULL) {

    expander=gtk_expander_new_with_mnemonic(_("_Options"));
  
    if (palette->style&STYLE_1) {
      label=gtk_expander_get_label_widget(GTK_EXPANDER(expander));
      lives_widget_set_fg_color(label, GTK_STATE_NORMAL, &palette->normal_fore);
      lives_widget_set_fg_color(label, GTK_STATE_PRELIGHT, &palette->normal_fore);
      lives_widget_set_fg_color(expander, GTK_STATE_PRELIGHT, &palette->normal_fore);
      lives_widget_set_bg_color(expander, GTK_STATE_PRELIGHT, &palette->normal_back);
      lives_widget_set_fg_color(expander, GTK_STATE_NORMAL, &palette->normal_fore);
      lives_widget_set_bg_color(expander, GTK_STATE_NORMAL, &palette->normal_back);
    }

    gtk_box_pack_start (GTK_BOX (dialog_vbox), expander, TRUE, TRUE, 0);

    vbox = lives_vbox_new (FALSE, 0);
    gtk_widget_show (vbox);

    add_fill_to_box(LIVES_BOX(vbox));

    gtk_container_add (GTK_CONTAINER (expander), vbox);

    // options

    hbox = lives_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

    commentsw->subt_checkbutton = lives_standard_check_button_new (_("Save _subtitles to file"),TRUE,LIVES_BOX(hbox),NULL);

    if (sfile->subt==NULL) {
      gtk_widget_set_sensitive(commentsw->subt_checkbutton,FALSE);
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(commentsw->subt_checkbutton),FALSE);
    }
    else lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(commentsw->subt_checkbutton),TRUE);


    hbox = lives_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

    commentsw->subt_entry=lives_standard_entry_new(_("Subtitle file"),FALSE,NULL,32,-1,LIVES_BOX(hbox),NULL);

    buttond = gtk_button_new_with_mnemonic(_("Browse..."));

    g_signal_connect (buttond, "clicked",G_CALLBACK (on_save_subs_activate),
    		      (gpointer)commentsw->subt_entry);

    gtk_box_pack_start (GTK_BOX (hbox), buttond, FALSE, FALSE, widget_opts.packing_width);
    gtk_widget_show_all (hbox);

    add_fill_to_box(LIVES_BOX(vbox));


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

  gtk_widget_show_all(commentsw->comments_dialog);

  return commentsw;
}


static void set_child_colour(GtkWidget *widget, gpointer data) {
  if (GTK_IS_BUTTON(widget)) return;
  if (GTK_IS_CONTAINER(widget)) {
    gtk_container_forall(GTK_CONTAINER(widget),set_child_colour,NULL);
    return;
  }

  if (GTK_IS_LABEL(widget)) {
    lives_widget_set_fg_color(widget, GTK_STATE_NORMAL, &palette->normal_fore);
  }
}



gchar *choose_file(gchar *dir, gchar *fname, gchar **filt, GtkFileChooserAction act, const char *title, GtkWidget *extra_widget) {
  // new style file chooser

  // in/out values are in utf8 encoding

  GtkWidget *chooser;
  GtkFileFilter *filter;

  gchar *filename=NULL;
  gchar *mytitle;
  gchar *tmp;

  boolean did_check;

  gint response;

  register int i;

  if (title==NULL) {
    if (act==GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER) {
      mytitle=g_strdup(_("LiVES: choose a directory"));
    }
    else {
      mytitle=g_strdup(_("LiVES: choose a file"));
    }
  }
  else mytitle=g_strdup(title);

  if (act!=GTK_FILE_CHOOSER_ACTION_SAVE) 
    chooser=gtk_file_chooser_dialog_new(mytitle,GTK_WINDOW(mainw->LiVES),act,GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
					NULL);

  else {
    chooser=gtk_file_chooser_dialog_new(mytitle,GTK_WINDOW(mainw->LiVES),act,GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
					NULL);
  }


  g_free(mytitle);

  gtk_file_chooser_set_local_only(GTK_FILE_CHOOSER(chooser),TRUE);

  if (dir!=NULL) {
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(chooser),(tmp=g_filename_from_utf8(dir,-1,NULL,NULL,NULL)));
    gtk_file_chooser_add_shortcut_folder(GTK_FILE_CHOOSER(chooser),tmp,NULL);
    g_free(tmp);
  }

  did_check=lives_file_chooser_set_do_overwrite_confirmation(LIVES_FILE_CHOOSER(chooser),TRUE);

  if (palette->style&STYLE_1) {
    lives_widget_set_bg_color(chooser, GTK_STATE_NORMAL, &palette->normal_back);
    gtk_container_forall(GTK_CONTAINER(chooser),set_child_colour,NULL);
  }

  if (dir!=NULL) gtk_file_chooser_set_current_folder_uri (GTK_FILE_CHOOSER(chooser), dir);
  if (fname!=NULL) gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(chooser),fname);

  if (filt!=NULL) {
    filter=gtk_file_filter_new();
    gtk_file_filter_add_pattern(filter,filt[0]);
    for (i=1;filt[i]!=NULL;i++) gtk_file_filter_add_pattern(filter,filt[i]);
    gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(chooser),filter);
    if (fname==NULL&&i==1&&act==GTK_FILE_CHOOSER_ACTION_SAVE) gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(chooser),filt[0]);
  }
  else {
    if (fname!=NULL&&dir!=NULL) {
      gchar *ffname=g_build_filename(dir,fname,NULL);
      gtk_file_chooser_select_filename(GTK_FILE_CHOOSER(chooser),ffname);
      g_free(ffname);
    }
  }

  gtk_container_set_border_width (GTK_CONTAINER (chooser), widget_opts.border_width);
  gtk_window_set_position (GTK_WINDOW (chooser), GTK_WIN_POS_CENTER_ALWAYS);
  gtk_window_set_modal (GTK_WINDOW (chooser), TRUE);

  if (prefs->show_gui) {
    if (mainw->multitrack==NULL) gtk_window_set_transient_for(GTK_WINDOW(chooser),GTK_WINDOW(mainw->LiVES));
    else gtk_window_set_transient_for(GTK_WINDOW(chooser),GTK_WINDOW(mainw->multitrack->window));
  }

  if (prefs->gui_monitor!=0) {
    int xcen=mainw->mgeom[prefs->gui_monitor-1].x+(mainw->mgeom[prefs->gui_monitor-1].width
						   -lives_widget_get_allocation_width(chooser))/2;
   int ycen=mainw->mgeom[prefs->gui_monitor-1].y+(mainw->mgeom[prefs->gui_monitor-1].height-
						  lives_widget_get_allocation_height(chooser))/2;
   gtk_window_move(GTK_WINDOW(chooser),xcen,ycen);
  }

  gtk_widget_show(chooser);

  gtk_widget_grab_focus (chooser);

  if (extra_widget==mainw->LiVES) return (gchar *)chooser; // kludge to allow custom adding of extra widgets

  if (extra_widget!=NULL) gtk_file_chooser_set_extra_widget(GTK_FILE_CHOOSER(chooser),extra_widget);
  
  if ((response=gtk_dialog_run(GTK_DIALOG(chooser)))!=GTK_RESPONSE_CANCEL) {
    filename=g_filename_to_utf8((tmp=gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(chooser))),-1,NULL,NULL,NULL);
    g_free(tmp);
  }

  if (!did_check && act==GTK_FILE_CHOOSER_ACTION_SAVE) {
    if (!check_file(filename,TRUE)) {
      g_free(filename);
      filename=NULL;
    }
  }

  gtk_widget_destroy(chooser);

  return filename;

}


static void chooser_response(GtkDialog *dialog, gint response, gpointer user_data) {
  int type=GPOINTER_TO_INT(user_data);

  if (response!=GTK_RESPONSE_CANCEL) {
    switch (type) {
    case 1:
      on_ok_filesel_open_clicked(GTK_FILE_CHOOSER(dialog),NULL);
      break;
    case 2:
      on_open_new_audio_clicked(GTK_FILE_CHOOSER(dialog),NULL);
      break;
    case 3:
      on_ok_file_open_clicked(GTK_FILE_CHOOSER(dialog),NULL);
      break;
    case 4:
      //on_xmms_ok_clicked(GTK_FILE_CHOOSER(dialog),NULL);
      break;
    case 5:
      on_ok_append_audio_clicked(GTK_FILE_CHOOSER(dialog),NULL);
      break;
    default:
      break;
    }
  }
  else on_cancel_button1_clicked(GTK_WIDGET(dialog),NULL);
}




void choose_file_with_preview (gchar *dir, const gchar *title, int preview_type) {
  // preview_type 1 - video and audio open (single - opensel)
  // preview type 2 - import audio
  // preview_type 3 - video and audio open (multiple)
  // type 4 xmms (deprecated)
  // type 5 append audio

  GtkWidget *chooser;

  gchar titles[256];

  g_snprintf(titles,256,_("LiVES: - %s"),title);

  chooser=(GtkWidget *)choose_file(dir,NULL,NULL,GTK_FILE_CHOOSER_ACTION_OPEN,titles,mainw->LiVES);
  
  if (preview_type==3) gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(chooser),TRUE);

  widget_add_preview(chooser,LIVES_BOX(lives_dialog_get_content_area(LIVES_DIALOG(chooser))),
		     LIVES_BOX(lives_dialog_get_content_area(LIVES_DIALOG(chooser))),
		     LIVES_BOX(lives_dialog_get_action_area(LIVES_DIALOG(chooser))),
		     preview_type==3?1:preview_type>3?2:preview_type);

  if (prefs->fileselmax) {
    gtk_window_set_resizable (GTK_WINDOW(chooser),TRUE);
    gtk_window_maximize (GTK_WINDOW(chooser));
    gtk_widget_queue_draw(chooser);
    lives_widget_context_update();
  }

  g_signal_connect (chooser, "response", G_CALLBACK (chooser_response), GINT_TO_POINTER(preview_type));
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

  cdsw->dialog = lives_standard_dialog_new (_("LiVES: - Cancel/Discard/Save"),FALSE);

  accel_group = GTK_ACCEL_GROUP(gtk_accel_group_new ());
  gtk_window_add_accel_group (GTK_WINDOW (cdsw->dialog), accel_group);

  if (prefs->show_gui) {
    if (mainw->multitrack==NULL) gtk_window_set_transient_for(GTK_WINDOW(cdsw->dialog),GTK_WINDOW(mainw->LiVES));
    else gtk_window_set_transient_for(GTK_WINDOW(cdsw->dialog),GTK_WINDOW(mainw->multitrack->window));
  }

  dialog_vbox = lives_dialog_get_content_area(GTK_DIALOG(cdsw->dialog));

  widget_opts.justify=LIVES_JUSTIFY_CENTER;
  if (type==0) {
    if (strlen(mainw->multitrack->layout_name)==0) {
      label = lives_standard_label_new (_("You are about to leave multitrack mode.\nThe current layout has not been saved.\nWhat would you like to do ?\n"));
    }
    else {
      label = lives_standard_label_new (_("You are about to leave multitrack mode.\nThe current layout has been changed since the last save.\nWhat would you like to do ?\n"));
    }
  }
  else if (type==1) {
    if (!mainw->only_close) label = lives_standard_label_new (_("You are about to exit LiVES.\nThe current clip set can be saved.\nWhat would you like to do ?\n"));
    else label = lives_standard_label_new (_("The current clip set has not been saved.\nWhat would you like to do ?\n"));
  }
  else if (type==2||type==3) {
    if ((mainw->multitrack!=NULL&&mainw->multitrack->changed)||(mainw->stored_event_list!=NULL&&mainw->stored_event_list_changed)) {
      label = lives_standard_label_new (_("The current layout has not been saved.\nWhat would you like to do ?\n"));
    }
    else {
      label = lives_standard_label_new (_("The current layout has not been changed since it was last saved.\nWhat would you like to do ?\n"));
    }
  }
  else if (type==4) {
    if (mainw->multitrack!=NULL&&mainw->multitrack->changed) {
      label = lives_standard_label_new (_("The current layout contains generated frames and cannot be retained.\nYou may wish to render it before exiting multitrack mode.\n"));
    }
    else {
      label = lives_standard_label_new (_("You are about to leave multitrack mode.\nThe current layout contains generated frames and cannot be retained.\nWhat do you wish to do ?"));
    }
  }
  widget_opts.justify=LIVES_JUSTIFY_DEFAULT;

  gtk_box_pack_start (GTK_BOX (dialog_vbox), label, TRUE, TRUE, 0);

  if (type==1) {
    GtkWidget *checkbutton;

    hbox = lives_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (dialog_vbox), hbox, FALSE, FALSE, widget_opts.packing_height);
    
    cdsw->entry = lives_standard_entry_new (_("Clip set _name"),TRUE,strlen (mainw->set_name)?mainw->set_name:"",32,128,LIVES_BOX(hbox),NULL);

    hbox = lives_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (dialog_vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

    checkbutton = lives_standard_check_button_new (_("_Auto reload next time"),TRUE,LIVES_BOX(hbox),NULL);

    if ((type==0&&prefs->ar_layout)||(type==1&&!mainw->only_close)) {
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(checkbutton),TRUE);
      if (type==1) prefs->ar_clipset=TRUE;
      else prefs->ar_layout=TRUE;
    }
    else {
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(checkbutton),FALSE);
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

  dialog_action_area = lives_dialog_get_action_area(LIVES_DIALOG (cdsw->dialog));
  gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog_action_area), GTK_BUTTONBOX_END);

  cancelbutton = gtk_button_new_from_stock ("gtk-cancel");
  gtk_dialog_add_action_widget (GTK_DIALOG (cdsw->dialog), cancelbutton, 0);
  gtk_widget_add_accelerator (cancelbutton, "activate", accel_group,
                              LIVES_KEY_Escape, (GdkModifierType)0, (GtkAccelFlags)0);

  discardbutton = gtk_button_new_from_stock ("gtk-delete");
  gtk_dialog_add_action_widget (GTK_DIALOG (cdsw->dialog), discardbutton, 1+(type==2));
  gtk_button_set_use_stock(GTK_BUTTON(discardbutton),FALSE);
  gtk_button_set_use_underline(GTK_BUTTON(discardbutton),TRUE);
  if ((type==0&&strlen(mainw->multitrack->layout_name)==0)||type==3||type==4) gtk_button_set_label(GTK_BUTTON(discardbutton),_("_Wipe layout"));
  else if (type==0) gtk_button_set_label(GTK_BUTTON(discardbutton),_("_Ignore changes"));
  else if (type==1) gtk_button_set_label(GTK_BUTTON(discardbutton),_("_Delete clip set"));
  else if (type==2) gtk_button_set_label(GTK_BUTTON(discardbutton),_("_Delete layout"));

  savebutton = gtk_button_new_from_stock ("gtk-save");
  gtk_button_set_use_stock(GTK_BUTTON(savebutton),FALSE);
  gtk_button_set_use_underline(GTK_BUTTON(savebutton),TRUE);
  if (type==0||type==3) gtk_button_set_label(GTK_BUTTON(savebutton),_("_Save layout"));
  else if (type==1) gtk_button_set_label(GTK_BUTTON(savebutton),_("_Save clip set"));
  else if (type==2) gtk_button_set_label(GTK_BUTTON(savebutton),_("_Wipe layout"));
  if (type!=4) gtk_dialog_add_action_widget (GTK_DIALOG (cdsw->dialog), savebutton, 2-(type==2));
  lives_widget_set_can_focus_and_default (savebutton);
  if (type==1||type==2)gtk_widget_grab_default(savebutton);

  gtk_widget_show_all(cdsw->dialog);

  return cdsw;
}




void do_layout_recover_dialog(void) {
  GtkWidget *label;
  GtkWidget *dialog_vbox;
  GtkWidget *okbutton;
  GtkWidget *cancelbutton;

  GtkWidget *mdialog=lives_standard_dialog_new (_("LiVES: recover layout ?"),FALSE);


  if (prefs->show_gui) {
    gtk_window_set_transient_for(GTK_WINDOW(mdialog),GTK_WINDOW(mainw->LiVES));
  }

  dialog_vbox = lives_dialog_get_content_area(GTK_DIALOG(mdialog));
  
  widget_opts.justify=LIVES_JUSTIFY_CENTER;
  label = lives_standard_label_new 
    (_("\nLiVES has detected a multitrack layout from a previous session.\nWould you like to try and recover it ?\n"));
  widget_opts.justify=LIVES_JUSTIFY_DEFAULT;


  gtk_container_add (GTK_CONTAINER (dialog_vbox), label);


  cancelbutton = gtk_button_new_from_stock ("gtk-cancel");
  gtk_widget_show (cancelbutton);
  gtk_dialog_add_action_widget (GTK_DIALOG (mdialog), cancelbutton, GTK_RESPONSE_CANCEL);
  lives_widget_set_can_focus_and_default (cancelbutton);

  okbutton = gtk_button_new_from_stock ("gtk-ok");
  gtk_widget_show (okbutton);
  gtk_dialog_add_action_widget (GTK_DIALOG (mdialog), okbutton, GTK_RESPONSE_OK);
  lives_widget_set_can_focus_and_default (okbutton);
  gtk_widget_grab_default(okbutton);
  gtk_widget_grab_focus(okbutton);

  g_signal_connect (cancelbutton, "clicked",G_CALLBACK (recover_layout_cancelled),NULL);

  g_signal_connect (okbutton, "clicked",G_CALLBACK (recover_layout),NULL);

  gtk_widget_add_accelerator (cancelbutton, "activate", mainw->accel_group,
                              LIVES_KEY_Escape, (GdkModifierType)0, (GtkAccelFlags)0);

  gtk_widget_show_all(mdialog);
}


static void flip_cdisk_bit (GtkToggleButton *t, gpointer user_data) {
  guint32 bitmask=GPOINTER_TO_INT(user_data);
  prefs->clear_disk_opts^=bitmask;
}


GtkWidget *create_cleardisk_advanced_dialog(void) {
  GtkWidget *dialog;
  GtkWidget *dialog_vbox;
  GtkWidget *scrollw;
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *checkbutton;
  GtkWidget *okbutton;
  GtkWidget *resetbutton;

  gchar *tmp,*tmp2;

  dialog = lives_standard_dialog_new (_("LiVES: - Disk Recovery Options"),FALSE);

  if (prefs->show_gui) {
    if (mainw->multitrack==NULL) gtk_window_set_transient_for(GTK_WINDOW(dialog),GTK_WINDOW(mainw->LiVES));
    else gtk_window_set_transient_for(GTK_WINDOW(dialog),GTK_WINDOW(mainw->multitrack->window));
  }

  dialog_vbox = lives_dialog_get_content_area(GTK_DIALOG(dialog));

  scrollw = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_set_size_request (scrollw, 450, 300);

  gtk_container_add (GTK_CONTAINER (dialog_vbox), scrollw);
   
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrollw), GTK_POLICY_AUTOMATIC, 
				  GTK_POLICY_AUTOMATIC);


  vbox = lives_vbox_new (FALSE, 0);

  gtk_container_set_border_width (GTK_CONTAINER (vbox), 20);

  gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scrollw), vbox);
  
  // Apply theme background to scrolled window
  if (palette->style&STYLE_1) {
    lives_widget_set_fg_color(lives_bin_get_child(LIVES_BIN(scrollw)), GTK_STATE_NORMAL, &palette->normal_fore);
    lives_widget_set_bg_color(lives_bin_get_child(LIVES_BIN(scrollw)), GTK_STATE_NORMAL, &palette->normal_back);
  }

  hbox = lives_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, widget_opts.packing_height);

  checkbutton = lives_standard_check_button_new((tmp=g_strdup(_("Delete _Orphaned Clips"))),TRUE,LIVES_BOX(hbox),
						(tmp2=g_strdup(_("Delete any clips which are not currently loaded or part of a set"))));

  g_free(tmp); g_free(tmp2);

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(checkbutton), !(prefs->clear_disk_opts & LIVES_CDISK_LEAVE_ORPHAN_SETS));

  g_signal_connect_after (GTK_OBJECT (checkbutton), "toggled",
			  G_CALLBACK (flip_cdisk_bit),
			  GINT_TO_POINTER(LIVES_CDISK_LEAVE_ORPHAN_SETS));

  hbox = lives_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, widget_opts.packing_height);

  checkbutton = lives_standard_check_button_new(_("Clear _Backup Files from Closed Clips"),TRUE,LIVES_BOX(hbox),NULL);

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(checkbutton), !(prefs->clear_disk_opts & LIVES_CDISK_LEAVE_BFILES));

  g_signal_connect_after (GTK_OBJECT (checkbutton), "toggled",
			  G_CALLBACK (flip_cdisk_bit),
			  GINT_TO_POINTER(LIVES_CDISK_LEAVE_BFILES));

  hbox = lives_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, widget_opts.packing_height);

  checkbutton = lives_standard_check_button_new(_("Remove Sets which have _Layouts but no Clips"),TRUE,LIVES_BOX(hbox),NULL);

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(checkbutton), 
				 (prefs->clear_disk_opts & LIVES_CDISK_REMOVE_ORPHAN_LAYOUTS));
  
  g_signal_connect_after (GTK_OBJECT (checkbutton), "toggled",
			  G_CALLBACK (flip_cdisk_bit),
			  GINT_TO_POINTER(LIVES_CDISK_REMOVE_ORPHAN_LAYOUTS));

  resetbutton = gtk_button_new_from_stock ("gtk-refresh");
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), resetbutton, LIVES_RETRY);
  gtk_button_set_label(GTK_BUTTON(resetbutton),_("_Reset to Defaults"));

  okbutton = gtk_button_new_from_stock ("gtk-ok");
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), okbutton, GTK_RESPONSE_OK);

  lives_widget_set_can_focus_and_default (okbutton);
  gtk_widget_grab_default (okbutton);
  gtk_button_set_label(GTK_BUTTON(okbutton),_("_Accept"));

  return dialog;

}


GtkTextView *create_output_textview(void) {
  GtkWidget *textview=gtk_text_view_new();
  gtk_text_view_set_editable (GTK_TEXT_VIEW (textview), FALSE);

  if (palette->style&STYLE_1) {
    lives_widget_set_base_color(textview, GTK_STATE_NORMAL, &palette->info_base);
    lives_widget_set_text_color(textview, GTK_STATE_NORMAL, &palette->info_text);
  }

  g_object_ref(textview);
  gtk_widget_show(textview);
  return GTK_TEXT_VIEW(textview);
}

