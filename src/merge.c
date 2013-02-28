// merge.c
// LiVES (lives-exe)
// (c) G. Finch 2003 - 2013 (salsaman@gmail.com)
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
  lives_rfx_t *rfx;

  GtkWidget *dialog_vbox;
  GtkWidget *vbox;
  GtkWidget *align_start_button;
  GtkWidget *align_end_button;
  GtkWidget *hbox;
  GtkWidget *label;
  GtkWidget *fit_button;
  GtkWidget *transition_combo;
  GtkWidget *dialog_action_area;
  GtkWidget *cancelbutton;
  GtkWidget *okbutton;

  GSList *radiobutton_align_group = NULL;
  GSList *radiobutton_insdrop_group = NULL;

  GtkAccelGroup *accel_group;

  gchar *txt;

  int idx=0;

  gint cb_frames=clipboard->frames;
  gint defstart=0;

  register int i;

  merge_opts=(_merge_opts*)(g_malloc(sizeof(_merge_opts)));
  merge_opts->list_to_rfx_index=(int *)g_malloc (sizint*(mainw->num_rendered_effects_builtin+
							 mainw->num_rendered_effects_custom+
							 mainw->num_rendered_effects_test));
  merge_opts->trans_list=NULL;

  merge_opts->spinbutton_loops=NULL;

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

  merge_opts->merge_dialog = lives_standard_dialog_new (_("LiVES: - Merge"),FALSE);

  accel_group = GTK_ACCEL_GROUP(gtk_accel_group_new ());
  gtk_window_add_accel_group (GTK_WINDOW (merge_opts->merge_dialog), accel_group);

  if (prefs->show_gui) {
    gtk_window_set_transient_for(GTK_WINDOW(merge_opts->merge_dialog),GTK_WINDOW(mainw->LiVES));
  }

  dialog_vbox = lives_dialog_get_content_area(GTK_DIALOG(merge_opts->merge_dialog));

  vbox = lives_vbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (dialog_vbox), vbox, TRUE, TRUE, 0);

  hbox = lives_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 19);

  txt=g_strdup_printf(_ ("Merge Clipboard [ %d Frames ]       With Selection [ %d Frames ]"),clipboard->frames,cfile->end-cfile->start+1);
  if (prefs->ins_resample&&clipboard->fps!=cfile->fps) {
    cb_frames=count_resampled_frames(clipboard->frames,clipboard->fps,cfile->fps);
    if (!(cb_frames==clipboard->frames)) {
      g_free(txt);
      txt=g_strdup_printf(_ ("Merge Clipboard [ %d Frames (resampled) ]       With Selection [ %d Frames ]"),cb_frames,cfile->end-cfile->start+1);
    }
  }

  label = lives_standard_label_new (txt);
  g_free(txt);

  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

  add_fill_to_box(LIVES_BOX(hbox));

  align_start_button = lives_standard_radio_button_new (_("Align _Starts"),TRUE,radiobutton_align_group,LIVES_BOX(hbox),NULL);
  radiobutton_align_group = lives_radio_button_get_group (LIVES_RADIO_BUTTON (align_start_button));

  add_fill_to_box(LIVES_BOX(hbox));

  align_end_button = lives_standard_radio_button_new (_("Align _Ends"),TRUE,radiobutton_align_group,LIVES_BOX(hbox),NULL);

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(align_end_button),!mainw->last_transition_align_start);



  hbox = lives_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 22);

  if ((cfile->end-cfile->start+1)<cb_frames) {
    // hide loop controls if selection is smaller than clipboard
    label = lives_standard_label_new (_("What to do with extra clipboard frames -"));
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

    merge_opts->ins_frame_button=lives_standard_radio_button_new(_("_Insert Frames"),TRUE,radiobutton_insdrop_group,LIVES_BOX(hbox),NULL);
    radiobutton_insdrop_group = lives_radio_button_get_group (LIVES_RADIO_BUTTON (merge_opts->ins_frame_button));

    merge_opts->ins_frame_function=g_signal_connect (GTK_OBJECT (merge_opts->ins_frame_button),
						     "toggled",G_CALLBACK (on_ins_frames_toggled),NULL);

    merge_opts->drop_frame_button=lives_standard_radio_button_new(_("_Drop Frames"),TRUE,radiobutton_insdrop_group,LIVES_BOX(hbox),NULL);

    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(merge_opts->drop_frame_button),!mainw->last_transition_ins_frames);
  }
  else if ((cfile->end-cfile->start+1)>cb_frames) {
    merge_opts->spinbutton_loops = lives_standard_spin_button_new 
      (_("Number of Times to Loop Clipboard"),FALSE,1.,1.,
       (int)((cfile->end-cfile->start+1)/cb_frames), 1., 10., 0, LIVES_BOX(hbox), NULL);
    

    gtk_spin_button_set_value (GTK_SPIN_BUTTON (merge_opts->spinbutton_loops),mainw->last_transition_loops);
    gtk_widget_set_sensitive(merge_opts->spinbutton_loops,!mainw->last_transition_loop_to_fit);

    g_signal_connect_after (GTK_OBJECT (merge_opts->spinbutton_loops), "value_changed",
			    G_CALLBACK (after_spinbutton_loops_changed),
			    NULL);

    fit_button = lives_standard_check_button_new (_("_Loop Clipboard to Fit Selection"),TRUE,LIVES_BOX(hbox),NULL);
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(fit_button),mainw->last_transition_loop_to_fit);

    g_signal_connect (GTK_OBJECT (fit_button), "toggled",
		      G_CALLBACK (on_fit_toggled),
		      NULL);
  }

  add_hsep_to_box(LIVES_BOX(vbox),FALSE);
  
  hbox = lives_hbox_new (TRUE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 10);

  transition_combo = lives_standard_combo_new (_("_Transition Method:"),TRUE,merge_opts->trans_list,LIVES_BOX(hbox),NULL);

  gtk_combo_box_set_active(GTK_COMBO_BOX(transition_combo),defstart);

  mainw->last_transition_idx=merge_opts->list_to_rfx_index[defstart];

  add_hsep_to_box(LIVES_BOX(vbox),FALSE);


  do_onchange_init(rfx);

  // now the dynamic part...
  merge_opts->param_vbox = lives_vbox_new (FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER(merge_opts->param_vbox), 10);

  gtk_box_pack_start (GTK_BOX (vbox), merge_opts->param_vbox, TRUE, TRUE, 0);

  rfx=&mainw->rendered_fx[mainw->last_transition_idx];
  make_param_box(GTK_VBOX (merge_opts->param_vbox), rfx);

  // done !


  dialog_action_area = lives_dialog_get_action_area(LIVES_DIALOG (merge_opts->merge_dialog));
  gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog_action_area), GTK_BUTTONBOX_END);

  cancelbutton = gtk_button_new_from_stock ("gtk-cancel");
  gtk_dialog_add_action_widget (GTK_DIALOG (merge_opts->merge_dialog), cancelbutton, GTK_RESPONSE_CANCEL);
  lives_widget_set_can_focus (cancelbutton,TRUE);

  okbutton = gtk_button_new_from_stock ("gtk-ok");
  gtk_dialog_add_action_widget (GTK_DIALOG (merge_opts->merge_dialog), okbutton, GTK_RESPONSE_OK);
  lives_widget_set_can_focus_and_default (okbutton);
  gtk_widget_grab_default (okbutton);

  g_signal_connect (GTK_OBJECT (cancelbutton), "clicked",
                      G_CALLBACK (on_merge_cancel_clicked),
                      rfx);


  gtk_widget_add_accelerator (cancelbutton, "activate", accel_group,
                              LIVES_KEY_Escape,  (GdkModifierType)0, (GtkAccelFlags)0);


  g_signal_connect (GTK_OBJECT (okbutton), "clicked",
                      G_CALLBACK (on_merge_ok_clicked),
                      rfx);
  
  g_signal_connect (GTK_OBJECT(transition_combo),"changed",G_CALLBACK (on_trans_method_changed),NULL);
  

  g_signal_connect (GTK_OBJECT (align_start_button), "toggled",
		    G_CALLBACK (on_align_start_end_toggled),
		    rfx);


  if (prefs->show_gui) {
    gtk_widget_show_all(merge_opts->merge_dialog);
  }
}

static void bang (GtkWidget *widget, gpointer null) {
  gtk_widget_destroy (widget);
}


void on_trans_method_changed (GtkComboBox *combo, gpointer user_data) {
  int idx;
  lives_rfx_t *rfx;
  char *txt=lives_combo_get_active_text (combo);

  if (!strlen (txt)) {
    g_free(txt);
    return;
  }

  idx=lives_list_index(merge_opts->trans_list,txt);

  g_free(txt);

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




void
on_merge_activate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  create_merge_dialog();

  merge_opts->loop_to_fit=mainw->last_transition_loop_to_fit;
  merge_opts->ins_frames=mainw->last_transition_ins_frames;
  merge_opts->align_start=!mainw->last_transition_align_start;

  on_align_start_end_toggled (NULL,NULL);
}



void
on_merge_cancel_clicked                   (GtkButton       *button,
					   gpointer         user_data)
{
  lives_rfx_t *rfx=(lives_rfx_t *)user_data;
  on_paramwindow_cancel_clicked (NULL,rfx);
  if (merge_opts->spinbutton_loops!=NULL) 
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
  boolean cb_video_change=FALSE;
  gint current_file=mainw->current_file;
  gint old_frames=clipboard->frames;

  // save original values in case we cancel
  gint oundo_start=cfile->undo_start;
  gint oundo_end=cfile->undo_end;
  gint cb_end,excess_frames;
  gint times_to_loop=1;

  lives_rfx_t *rfx;

  if (merge_opts->spinbutton_loops!=NULL) 
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

  if (merge_opts->spinbutton_loops!=NULL) 
    times_to_loop=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(merge_opts->spinbutton_loops));
  else 
    times_to_loop=1;

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
