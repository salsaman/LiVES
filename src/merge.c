// merge.c
// LiVES (lives-exe)
// (c) G. Finch 2003 - 2012 (salsaman@gmail.com)
// Released under the GPL 3 or later
// see file ../COPYING for licensing details

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#include "main.h"
#include "callbacks.h"
#include "merge.h"
#include "paramwindow.h"
#include "effects.h"
#include "resample.h"
#include "support.h"

_merge_opts* merge_opts;

void create_merge_dialog (void) {
  GtkWidget *dialog_vbox;
  GtkWidget *vbox;
  GSList *radiobutton_align_group = NULL;
  GSList *radiobutton_insdrop_group = NULL;
  GtkWidget *align_start_button;
  GtkWidget *align_end_button;
  GtkWidget *hbox;
  GtkWidget *label;
  GtkWidget *fit_button;
  GtkWidget *hseparator;
  GtkWidget *transition_combo;
  GObject *spinbutton_adj;
  GtkWidget *dialog_action_area;
  GtkWidget *cancelbutton;
  GtkWidget *okbutton;

  int i,idx=0;
  lives_rfx_t *rfx;

  gint cb_frames=clipboard->frames;
  gchar *txt;

  gint defstart=0;

  merge_opts=(_merge_opts*)(g_malloc(sizeof(_merge_opts)));
  merge_opts->list_to_rfx_index=(int *)g_malloc (sizint*(mainw->num_rendered_effects_builtin+
							 mainw->num_rendered_effects_custom+
							 mainw->num_rendered_effects_test));
  merge_opts->trans_list=NULL;

  for (i=0;i<mainw->num_rendered_effects_builtin+mainw->num_rendered_effects_custom+mainw->num_rendered_effects_test;i++) {
    if ((rfx=&mainw->rendered_fx[i])->num_in_channels==2) {
      if (i==mainw->last_transition_idx) defstart=idx;
      merge_opts->list_to_rfx_index[idx++]=i;
      if (rfx->status==RFX_STATUS_CUSTOM) {
	merge_opts->trans_list = g_list_append (merge_opts->trans_list,g_strconcat ( _(rfx->menu_text)," (custom)",NULL));
      }
      else if (rfx->status==RFX_STATUS_TEST) {
	merge_opts->trans_list = g_list_append (merge_opts->trans_list,g_strconcat ( _(rfx->menu_text)," (test)",NULL));
      }
      else {
	merge_opts->trans_list = g_list_append (merge_opts->trans_list, g_strdup(_(rfx->menu_text)));
      }
    }
  }

  if (!idx) {
    do_rendered_fx_dialog();
    g_free (merge_opts->list_to_rfx_index);
    g_free (merge_opts);
    return;
  }



  merge_opts->merge_dialog = gtk_dialog_new ();
  gtk_container_set_border_width (GTK_CONTAINER (merge_opts->merge_dialog), 10);
  gtk_window_set_title (GTK_WINDOW (merge_opts->merge_dialog), _("LiVES: - Merge"));
  gtk_window_set_position (GTK_WINDOW (merge_opts->merge_dialog), GTK_WIN_POS_CENTER_ALWAYS);
  gtk_window_set_modal (GTK_WINDOW (merge_opts->merge_dialog), TRUE);
  gtk_window_set_default_size (GTK_WINDOW(merge_opts->merge_dialog), 720, -1);
  gtk_dialog_set_has_separator(GTK_DIALOG(merge_opts->merge_dialog),FALSE);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg (merge_opts->merge_dialog, GTK_STATE_NORMAL, &palette->normal_back);
  }

  if (!prefs->show_gui) {
    gtk_window_set_transient_for(GTK_WINDOW(merge_opts->merge_dialog),GTK_WINDOW(mainw->LiVES));
  }

  dialog_vbox = lives_dialog_get_content_area(GTK_DIALOG(merge_opts->merge_dialog));
  gtk_widget_show (dialog_vbox);

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (vbox);
  gtk_box_pack_start (GTK_BOX (dialog_vbox), vbox, TRUE, TRUE, 0);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 19);

  txt=g_strdup_printf(_ ("Merge Clipboard [ %d Frames ]       With Selection [ %d Frames ]"),clipboard->frames,cfile->end-cfile->start+1);
  if (prefs->ins_resample&&clipboard->fps!=cfile->fps) {
    cb_frames=count_resampled_frames(clipboard->frames,clipboard->fps,cfile->fps);
    if (!(cb_frames==clipboard->frames)) {
      g_free(txt);
      txt=g_strdup_printf(_ ("Merge Clipboard [ %d Frames (resampled) ]       With Selection [ %d Frames ]"),cb_frames,cfile->end-cfile->start+1);
    }
  }

  label = gtk_label_new (txt);
  g_free(txt);
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg (label, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  label = gtk_label_new ("");
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);

  align_start_button = gtk_radio_button_new (NULL);

  gtk_widget_show (align_start_button);
  gtk_box_pack_start (GTK_BOX (hbox),align_start_button, TRUE, FALSE, 0);
  gtk_radio_button_set_group (GTK_RADIO_BUTTON (align_start_button), radiobutton_align_group);
  radiobutton_align_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (align_start_button));

  label = gtk_label_new_with_mnemonic ( _("Align _Starts"));
  gtk_widget_show (label);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label),align_start_button);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg (label, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  label = gtk_label_new ("");
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);

  align_end_button = gtk_radio_button_new (radiobutton_align_group);
  gtk_widget_show (align_end_button);
  gtk_box_pack_start (GTK_BOX (hbox), align_end_button, TRUE, FALSE, 0);
  radiobutton_align_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (align_end_button));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg (align_end_button, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(align_end_button),!mainw->last_transition_align_start);
  

  label = gtk_label_new_with_mnemonic ( _("Align _Ends"));
  gtk_widget_show (label);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label),align_end_button);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg (label, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 22);

  spinbutton_adj = (GObject *)gtk_adjustment_new (1, 1, (gint)((cfile->end-cfile->start+1)/cb_frames), 1, 10, 0);
  merge_opts->spinbutton_loops = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_adj), 1, 0);
  merge_opts->ins_frame_button = gtk_radio_button_new (NULL);
  merge_opts->drop_frame_button = gtk_radio_button_new (NULL);
  fit_button = gtk_check_button_new ();

  gtk_spin_button_set_value (GTK_SPIN_BUTTON (merge_opts->spinbutton_loops),mainw->last_transition_loops);

  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(fit_button),mainw->last_transition_loop_to_fit);

  gtk_widget_set_sensitive(merge_opts->spinbutton_loops,!mainw->last_transition_loop_to_fit);

  if ((cfile->end-cfile->start+1)<cb_frames) {
    // hide loop controls if selection is smaller than clipboard
    label = gtk_label_new (_("What to do with extra clipboard frames -"));
    gtk_widget_show (label);
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg (label, GTK_STATE_NORMAL, &palette->normal_fore);
    }

    gtk_widget_show (merge_opts->ins_frame_button);
    gtk_box_pack_start (GTK_BOX (hbox), merge_opts->ins_frame_button, TRUE, FALSE, 0);
    label = gtk_label_new_with_mnemonic (_("_Insert Frames"));
    gtk_widget_show (label);
    gtk_label_set_mnemonic_widget (GTK_LABEL (label),merge_opts->ins_frame_button);
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg (label, GTK_STATE_NORMAL, &palette->normal_fore);
    }

    gtk_radio_button_set_group (GTK_RADIO_BUTTON (merge_opts->ins_frame_button), radiobutton_insdrop_group);
    radiobutton_insdrop_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (merge_opts->ins_frame_button));

    gtk_widget_show (merge_opts->drop_frame_button);
    gtk_box_pack_start (GTK_BOX (hbox), merge_opts->drop_frame_button, TRUE, FALSE, 0);

    label = gtk_label_new_with_mnemonic (_("_Drop Frames"));
    gtk_widget_show (label);
    gtk_label_set_mnemonic_widget (GTK_LABEL (label),merge_opts->drop_frame_button);
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg (label, GTK_STATE_NORMAL, &palette->normal_fore);
    }
    gtk_radio_button_set_group (GTK_RADIO_BUTTON (merge_opts->drop_frame_button), radiobutton_insdrop_group);
    radiobutton_insdrop_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (merge_opts->drop_frame_button));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(merge_opts->drop_frame_button),!mainw->last_transition_ins_frames);
  }
  else if ((cfile->end-cfile->start+1)>cb_frames) {
    
    label = gtk_label_new_with_mnemonic (_("Number of Times to Loop Clipboard"));
    gtk_widget_show (label);
    gtk_label_set_mnemonic_widget (GTK_LABEL (label),merge_opts->spinbutton_loops);
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
    gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg (label, GTK_STATE_NORMAL, &palette->normal_fore);
    }
    gtk_widget_show (merge_opts->spinbutton_loops);
    gtk_box_pack_start (GTK_BOX (hbox), merge_opts->spinbutton_loops, TRUE, FALSE, 0);

    gtk_widget_show (fit_button);
    gtk_box_pack_start (GTK_BOX (hbox), fit_button, FALSE, FALSE, 0);
    label = gtk_label_new_with_mnemonic (_ ("_Loop Clipboard to Fit Selection"));
    gtk_label_set_mnemonic_widget (GTK_LABEL (label),fit_button);
    gtk_widget_show (label);
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg (label, GTK_STATE_NORMAL, &palette->normal_fore);
    }
  }
  else {
    gtk_widget_hide(hbox);
  }

  hseparator = gtk_hseparator_new ();
  gtk_widget_show (hseparator);
  gtk_box_pack_start (GTK_BOX (vbox), hseparator, FALSE, TRUE, 0);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 10);

  label = gtk_label_new_with_mnemonic (_("_Transition Method:"));
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg (label, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  transition_combo = gtk_combo_new ();
  combo_set_popdown_strings (GTK_COMBO (transition_combo), merge_opts->trans_list);
  gtk_box_pack_start (GTK_BOX (hbox), transition_combo, TRUE, FALSE, 0);
  gtk_widget_show(transition_combo);
  merge_opts->trans_entry=(GtkWidget*)(GTK_ENTRY((GTK_COMBO(transition_combo))->entry));
  gtk_entry_set_text (GTK_ENTRY (merge_opts->trans_entry),(gchar *)g_list_nth_data (merge_opts->trans_list,defstart));
  gtk_editable_set_editable (GTK_EDITABLE(merge_opts->trans_entry),FALSE);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label),merge_opts->trans_entry);

  mainw->last_transition_idx=merge_opts->list_to_rfx_index[defstart];

  hseparator = gtk_hseparator_new ();
  gtk_widget_show (hseparator);
  gtk_box_pack_start (GTK_BOX (vbox), hseparator, FALSE, TRUE, 0);

  do_onchange_init(rfx);

  // now the dynamic part...
  merge_opts->param_vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER(merge_opts->param_vbox), 10);

  gtk_box_pack_start (GTK_BOX (vbox), merge_opts->param_vbox, TRUE, TRUE, 0);

  rfx=&mainw->rendered_fx[mainw->last_transition_idx];
  make_param_box(GTK_VBOX (merge_opts->param_vbox), rfx);
  gtk_widget_show_all (merge_opts->param_vbox);
  // done !


  dialog_action_area = GTK_DIALOG (merge_opts->merge_dialog)->action_area;
  gtk_widget_show (dialog_action_area);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog_action_area), GTK_BUTTONBOX_END);

  cancelbutton = gtk_button_new_from_stock ("gtk-cancel");
  gtk_widget_show (cancelbutton);
  gtk_dialog_add_action_widget (GTK_DIALOG (merge_opts->merge_dialog), cancelbutton, GTK_RESPONSE_CANCEL);
  GTK_WIDGET_SET_FLAGS (cancelbutton, GTK_CAN_DEFAULT);

  okbutton = gtk_button_new_from_stock ("gtk-ok");
  gtk_widget_show (okbutton);
  gtk_dialog_add_action_widget (GTK_DIALOG (merge_opts->merge_dialog), okbutton, GTK_RESPONSE_OK);
  GTK_WIDGET_SET_FLAGS (okbutton, GTK_CAN_DEFAULT);
 



  g_signal_connect (GTK_OBJECT (cancelbutton), "clicked",
                      G_CALLBACK (on_merge_cancel_clicked),
                      rfx);

  g_signal_connect (GTK_OBJECT (okbutton), "clicked",
                      G_CALLBACK (on_merge_ok_clicked),
                      rfx);


  g_signal_connect (GTK_OBJECT(merge_opts->trans_entry),"changed",G_CALLBACK (on_trans_method_changed),NULL);


  g_signal_connect (GTK_OBJECT (align_start_button), "toggled",
		    G_CALLBACK (on_align_start_end_toggled),
		    rfx);


  if ((cfile->end-cfile->start+1)>cb_frames) {
    g_signal_connect (GTK_OBJECT (fit_button), "toggled",
		      G_CALLBACK (on_fit_toggled),
		      NULL);
  }
  else if ((cfile->end-cfile->start+1)<cb_frames) {
    merge_opts->ins_frame_function=g_signal_connect (GTK_OBJECT (merge_opts->ins_frame_button), "toggled",G_CALLBACK (on_ins_frames_toggled),NULL);
  }

  g_signal_connect_after (GTK_OBJECT (merge_opts->spinbutton_loops), "value_changed",
			  G_CALLBACK (after_spinbutton_loops_changed),
			  NULL);
}




void on_trans_method_changed (GtkWidget *entry, gpointer user_data) {
  int idx;
  lives_rfx_t *rfx;

  if (!strlen (gtk_entry_get_text (GTK_ENTRY (entry)))) return;

  for (idx=0;strcmp((char *)g_list_nth_data (merge_opts->trans_list,idx),
		    (char *)gtk_entry_get_text(GTK_ENTRY (entry)));idx++);

  mainw->last_transition_idx=merge_opts->list_to_rfx_index[idx];
  rfx=&mainw->rendered_fx[mainw->last_transition_idx];

  gtk_container_foreach (GTK_CONTAINER(merge_opts->param_vbox),bang,NULL);
  on_paramwindow_cancel_clicked (NULL,rfx);

  do_onchange_init(rfx);

  make_param_box(GTK_VBOX (merge_opts->param_vbox), rfx);
  gtk_widget_show_all (merge_opts->param_vbox);
  merge_opts->align_start=!merge_opts->align_start;
  on_align_start_end_toggled (NULL,NULL);
}



void bang (GtkWidget *widget, gpointer null) {
  gtk_widget_destroy (widget);
}



void
on_merge_activate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  create_merge_dialog();

  merge_opts->loop_to_fit=mainw->last_transition_loop_to_fit;
  merge_opts->ins_frames=mainw->last_transition_ins_frames;
  merge_opts->align_start=!mainw->last_transition_align_start;

  gtk_widget_show (merge_opts->merge_dialog);
  on_align_start_end_toggled (NULL,NULL);
}



void
on_merge_cancel_clicked                   (GtkButton       *button,
					   gpointer         user_data)
{
  lives_rfx_t *rfx=(lives_rfx_t *)user_data;
  on_paramwindow_cancel_clicked (NULL,rfx);
  mainw->last_transition_loops=gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (merge_opts->spinbutton_loops));
  gtk_widget_destroy(merge_opts->merge_dialog);
  while (g_main_context_iteration (NULL,FALSE));
  mainw->last_transition_loop_to_fit=merge_opts->loop_to_fit;
  mainw->last_transition_ins_frames=merge_opts->ins_frames;
  mainw->last_transition_align_start=merge_opts->align_start;
  g_list_free_strings (merge_opts->trans_list);
  g_list_free (merge_opts->trans_list);
  g_free (merge_opts->list_to_rfx_index);
  g_free(merge_opts);
}





void
on_merge_ok_clicked                   (GtkButton       *button,
				       gpointer         user_data)
{
  gchar *msg;
  gint start,end;

  gint cb_start=1;
  gboolean cb_video_change=FALSE;
  gint current_file=mainw->current_file;
  gint old_frames=clipboard->frames;

  // save original values in case we cancel
  gint oundo_start=cfile->undo_start;
  gint oundo_end=cfile->undo_end;
  gint cb_end,excess_frames;
  gint times_to_loop=1;

  lives_rfx_t *rfx;

  mainw->last_transition_loops=gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (merge_opts->spinbutton_loops));
  mainw->last_transition_loop_to_fit=merge_opts->loop_to_fit;
  mainw->last_transition_ins_frames=merge_opts->ins_frames;
  mainw->last_transition_align_start=merge_opts->align_start;

  rfx=&mainw->rendered_fx[mainw->last_transition_idx];

  if (cfile->fps!=clipboard->fps) {
    if (!do_clipboard_fps_warning()) {
      on_paramwindow_cancel_clicked (NULL,rfx);
      gtk_widget_destroy(merge_opts->merge_dialog);
      while (g_main_context_iteration(NULL,FALSE));
      g_list_free (merge_opts->trans_list);
      g_free (merge_opts->list_to_rfx_index);
      g_free(merge_opts);
      return;
    }
  }

  times_to_loop=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(merge_opts->spinbutton_loops));

  on_paramwindow_cancel_clicked (NULL,rfx);
  gtk_widget_destroy(merge_opts->merge_dialog);
  while (g_main_context_iteration(NULL,FALSE));
  g_list_free (merge_opts->trans_list);
  g_free (merge_opts->list_to_rfx_index);
  g_free(merge_opts);

  // if pref is set, resample clipboard video
  if (prefs->ins_resample&&cfile->fps!=clipboard->fps) {
    if (!resample_clipboard(cfile->fps)) return;
  }

  if ((cfile->end-cfile->start+1)<=clipboard->frames) {
    times_to_loop=1;
  }

  msg=g_strdup(_ ("Merging clipboard with selection..."));
  d_print(msg);
  g_free(msg);

  excess_frames=clipboard->frames-(cfile->end-cfile->start+1);
  if (excess_frames<0) excess_frames=0;

  cfile->insert_start=0;
  cfile->insert_end=0;

  cfile->redoable=FALSE;
  gtk_widget_set_sensitive(mainw->redo,FALSE);

  mainw->fx2_bool=FALSE;

  // dummy values - used for 'fit to audio' in insert
  mainw->fx1_bool=FALSE;
  mainw->fx1_val=1;  // times to insert

  // insert pre-frames
  if (!mainw->last_transition_align_start&&excess_frames>0&&mainw->last_transition_ins_frames) {
    mainw->insert_after=FALSE;
    msg=g_strdup_printf(_ ("inserting %d extra frame(s) before merge\n"),excess_frames);
    d_print(msg);
    g_free(msg);
  
    // fx1_start and fx2_start indicate the clipboard start/end values, fx2_bool is insert_with_audio
    // TODO - allow this to be cancelled
    mainw->fx1_start=1;
    mainw->fx2_start=excess_frames;
    mainw->fx2_bool=FALSE;
    on_insert_activate(NULL,NULL);
    if (mainw->error) {
      d_print_failed();
      return;
    }
    if (mainw->cancelled) {
      return;
    }
  }

  // these should be the original values, maybe shifted if frames were inserted
  
  // now the merge section
  mainw->effects_paused=FALSE;

  // There are 6 possibilities:

  //               align starts                                       align ends
  //
  //               cb_start   cb_end                                 cb_start     cb_end
  // cb <= sel        1       cb_frames*ttl                            1          cb_frames*ttl
  // cb > sel         1       end-start+1                              excess_frames          cb_frames
  // l2f              1       end-start+1                              1          end-start+1


  // number of frames to merge, must be <= selection length
  if (!mainw->last_transition_loop_to_fit) {
    cb_end=(clipboard->frames-excess_frames)*times_to_loop;
  }
  else {
    //loop_to_fit_audio
    cb_end=cfile->end-cfile->start+1;
  } 


  // here we use undo_start and undo_end to mark the merged section,
  // insert_start and insert_end to mark the inserted section (if any)
  if (mainw->last_transition_align_start) {
    cfile->undo_start=cfile->start;
    cfile->undo_end=cfile->start+(cb_end*times_to_loop)-1;
  }
  else {
    cfile->undo_start=cfile->end-(cb_end*times_to_loop)+1;
    cfile->undo_end=cfile->end;
    cb_start=excess_frames+1;
    if (mainw->last_transition_loop_to_fit) {
      // make sure last frames are lined up
      cb_start-=cb_end;
      cb_end=(int)((cb_end+clipboard->frames-1)/clipboard->frames)*clipboard->frames;
      cb_start+=cb_end;
    }
  }

  if (!mainw->last_transition_loop_to_fit) {
    if (mainw->last_transition_align_start) {
      cfile->progress_end=cb_end-cb_start+cfile->start+excess_frames*!mainw->last_transition_align_start;
      cfile->progress_start=cfile->start;
    }
    else {
      cfile->progress_end=cfile->end;
      cfile->progress_start=cfile->end-cb_end+1;
    }
  }
  else {
    cfile->progress_start=cfile->start;
    cfile->progress_end=cfile->end;
  }

  // do the actual merge
  if (!do_effect (rfx,FALSE)) {
    // cancelled
    // delete pre-inserted frames
      if (!mainw->last_transition_align_start&&excess_frames>0) {
	start=cfile->start;
	end=cfile->end;
	cfile->start=cfile->insert_start;
	cfile->end=cfile->insert_end;
	on_delete_activate(NULL,NULL);
	// reset to original values
	cfile->start=start;
	cfile->end=end;
      }
      
      cfile->undo_start=oundo_start;
      cfile->undo_end=oundo_end;
      cfile->insert_start=cfile->insert_end=0;

      if (cb_video_change) {
	mainw->current_file=0;
	on_undo_activate(NULL,NULL);
	switch_to_file(0,current_file);
      }
      set_undoable (NULL, FALSE);

      if (!mainw->last_transition_align_start&&excess_frames>0) {
	// we've added and now deleted frames, we need to redraw everything...
	cfile->start-=excess_frames;
	cfile->end-=excess_frames;
	switch_to_file(mainw->current_file,mainw->current_file);
      }
      else {
	sensitize();
	get_play_times();
      }
      return;
  }

  // insert any post frames
  if (mainw->last_transition_align_start&&excess_frames>0&&mainw->last_transition_ins_frames) {
    mainw->insert_after=TRUE;
    msg=g_strdup_printf(_ ("now inserting %d extra frame(s)\n"),excess_frames);
    d_print(msg);
    g_free(msg);
    
      
    // fx1_start and fx2_start hold the clipboard start/end values
    mainw->fx1_start=clipboard->frames-excess_frames+1;
    mainw->fx2_start=clipboard->frames;

    on_insert_activate(NULL,GINT_TO_POINTER (1));
  }
  
  if (excess_frames==0||!mainw->last_transition_ins_frames) {
    d_print_done();
  }
  else {
    d_print(_ ("Merge done.\n"));
  }
  
  if (cb_video_change) {
    clipboard->old_frames=old_frames;
    mainw->current_file=0;
    on_undo_activate(NULL,NULL);
    mainw->current_file=current_file;
  }
  
  cfile->undo_action=UNDO_MERGE;
  // can get overwritten by undo insert
  set_undoable (_ (rfx->menu_text),TRUE);
  
  if (cfile->insert_start==0) {
    cfile->insert_start=cfile->undo_start;
    cfile->insert_end=cfile->undo_end;
  }
  if (cfile->undo_end>cfile->insert_end) {
    cfile->insert_end=cfile->undo_end;
  }
  if (cfile->undo_start<cfile->insert_start) {
    cfile->insert_start=cfile->undo_start;
  }
  
  switch_to_file (mainw->current_file,mainw->current_file);
}


void
after_spinbutton_loops_changed           (GtkSpinButton   *spinbutton,
					  gpointer user_data) {
  setmergealign ();
}


void
on_align_start_end_toggled (GtkToggleButton *togglebutton, gpointer user_data) {
  merge_opts->align_start=!merge_opts->align_start;
  setmergealign ();
}


void
on_fit_toggled (GtkToggleButton *togglebutton, gpointer user_data) {

  merge_opts->loop_to_fit=!merge_opts->loop_to_fit;
  gtk_widget_set_sensitive(merge_opts->spinbutton_loops,!merge_opts->loop_to_fit);
  setmergealign();
}


void
on_ins_frames_toggled (GtkToggleButton *togglebutton, gpointer user_data) {
  merge_opts->ins_frames=!merge_opts->ins_frames;
}
