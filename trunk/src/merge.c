// merge.c
// LiVES (lives-exe)
// (c) G. Finch 2003 - 2015 (salsaman@gmail.com)
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

_merge_opts *merge_opts;

void create_merge_dialog(void) {
  lives_rfx_t *rfx;

  LiVESWidget *dialog_vbox;
  LiVESWidget *vbox;
  LiVESWidget *align_start_button;
  LiVESWidget *align_end_button;
  LiVESWidget *hbox;
  LiVESWidget *label;
  LiVESWidget *fit_button;
  LiVESWidget *transition_combo;
  LiVESWidget *dialog_action_area;
  LiVESWidget *cancelbutton;
  LiVESWidget *okbutton;

  LiVESSList *radiobutton_align_group = NULL;
  LiVESSList *radiobutton_insdrop_group = NULL;

  LiVESList *retvals;

  LiVESAccelGroup *accel_group;

  char *txt;

  int idx=0;

  int scrw,scrh,width,height;

  int cb_frames=clipboard->frames;
  int defstart=0;

  register int i;

  merge_opts=(_merge_opts *)(lives_malloc(sizeof(_merge_opts)));
  merge_opts->list_to_rfx_index=(int *)lives_malloc(sizint*(mainw->num_rendered_effects_builtin+
                                mainw->num_rendered_effects_custom+
                                mainw->num_rendered_effects_test));
  merge_opts->trans_list=NULL;

  merge_opts->spinbutton_loops=NULL;

  for (i=0; i<mainw->num_rendered_effects_builtin+mainw->num_rendered_effects_custom+mainw->num_rendered_effects_test; i++) {
    if ((rfx=&mainw->rendered_fx[i])->num_in_channels==2) {
      if (i==mainw->last_transition_idx) defstart=idx;
      merge_opts->list_to_rfx_index[idx++]=i;
      if (rfx->status==RFX_STATUS_CUSTOM) {
        merge_opts->trans_list = lives_list_append(merge_opts->trans_list,lives_strconcat(_(rfx->menu_text)," (custom)",NULL));
      } else if (rfx->status==RFX_STATUS_TEST) {
        merge_opts->trans_list = lives_list_append(merge_opts->trans_list,lives_strconcat(_(rfx->menu_text)," (test)",NULL));
      } else {
        merge_opts->trans_list = lives_list_append(merge_opts->trans_list, lives_strdup(_(rfx->menu_text)));
      }
    }
  }

  if (!idx) {
    do_rendered_fx_dialog();
    lives_free(merge_opts->list_to_rfx_index);
    lives_free(merge_opts);
    return;
  }

  if (prefs->gui_monitor!=0) {
    scrw=mainw->mgeom[prefs->gui_monitor-1].width;
    scrh=mainw->mgeom[prefs->gui_monitor-1].height;
  } else {
    scrw=mainw->scr_width;
    scrh=mainw->scr_height;
  }

  height=scrh-SCR_HEIGHT_SAFETY;
  width=scrw-SCR_WIDTH_SAFETY;

  merge_opts->merge_dialog = lives_standard_dialog_new(_("LiVES: - Merge"),FALSE,width,height);

  accel_group = LIVES_ACCEL_GROUP(lives_accel_group_new());
  lives_window_add_accel_group(LIVES_WINDOW(merge_opts->merge_dialog), accel_group);

  if (prefs->show_gui) {
    lives_window_set_transient_for(LIVES_WINDOW(merge_opts->merge_dialog),LIVES_WINDOW(mainw->LiVES));
  }

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(merge_opts->merge_dialog));

  vbox = lives_vbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(dialog_vbox), vbox, TRUE, TRUE, 0);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height*2);

  txt=lives_strdup_printf(_("Merge Clipboard [ %d Frames ]       With Selection [ %d Frames ]"),clipboard->frames,cfile->end-cfile->start+1);
  if (prefs->ins_resample&&clipboard->fps!=cfile->fps) {
    cb_frames=count_resampled_frames(clipboard->frames,clipboard->fps,cfile->fps);
    if (!(cb_frames==clipboard->frames)) {
      lives_free(txt);
      txt=lives_strdup_printf(_("Merge Clipboard [ %d Frames (resampled) ]       With Selection [ %d Frames ]"),cb_frames,
                              cfile->end-cfile->start+1);
    }
  }

  label = lives_standard_label_new(txt);
  lives_free(txt);

  lives_box_pack_start(LIVES_BOX(hbox), label, FALSE, FALSE, 0);

  add_fill_to_box(LIVES_BOX(hbox));

  align_start_button = lives_standard_radio_button_new(_("Align _Starts"),TRUE,radiobutton_align_group,LIVES_BOX(hbox),NULL);
  radiobutton_align_group = lives_radio_button_get_group(LIVES_RADIO_BUTTON(align_start_button));

  add_fill_to_box(LIVES_BOX(hbox));

  align_end_button = lives_standard_radio_button_new(_("Align _Ends"),TRUE,radiobutton_align_group,LIVES_BOX(hbox),NULL);

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(align_end_button),!mainw->last_transition_align_start);



  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, TRUE, widget_opts.packing_height*2);

  if ((cfile->end-cfile->start+1)<cb_frames) {
    // hide loop controls if selection is smaller than clipboard
    label = lives_standard_label_new(_("What to do with extra clipboard frames -"));
    lives_box_pack_start(LIVES_BOX(hbox), label, FALSE, FALSE, 0);

    merge_opts->ins_frame_button=lives_standard_radio_button_new(_("_Insert Frames"),TRUE,radiobutton_insdrop_group,LIVES_BOX(hbox),NULL);
    radiobutton_insdrop_group = lives_radio_button_get_group(LIVES_RADIO_BUTTON(merge_opts->ins_frame_button));

    merge_opts->ins_frame_function=lives_signal_connect(LIVES_GUI_OBJECT(merge_opts->ins_frame_button),
                                   LIVES_WIDGET_TOGGLED_SIGNAL,LIVES_GUI_CALLBACK(on_ins_frames_toggled),NULL);

    merge_opts->drop_frame_button=lives_standard_radio_button_new(_("_Drop Frames"),TRUE,radiobutton_insdrop_group,LIVES_BOX(hbox),NULL);

    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(merge_opts->drop_frame_button),!mainw->last_transition_ins_frames);
  } else if ((cfile->end-cfile->start+1)>cb_frames) {
    merge_opts->spinbutton_loops = lives_standard_spin_button_new
                                   (_("Number of Times to Loop Clipboard"),FALSE,1.,1.,
                                    (int)((cfile->end-cfile->start+1)/cb_frames), 1., 10., 0, LIVES_BOX(hbox), NULL);


    lives_spin_button_set_value(LIVES_SPIN_BUTTON(merge_opts->spinbutton_loops),mainw->last_transition_loops);
    lives_widget_set_sensitive(merge_opts->spinbutton_loops,!mainw->last_transition_loop_to_fit);

    lives_signal_connect_after(LIVES_GUI_OBJECT(merge_opts->spinbutton_loops), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                               LIVES_GUI_CALLBACK(after_spinbutton_loops_changed),
                               NULL);

    fit_button = lives_standard_check_button_new(_("_Loop Clipboard to Fit Selection"),TRUE,LIVES_BOX(hbox),NULL);
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(fit_button),mainw->last_transition_loop_to_fit);

    lives_signal_connect(LIVES_GUI_OBJECT(fit_button), LIVES_WIDGET_TOGGLED_SIGNAL,
                         LIVES_GUI_CALLBACK(on_fit_toggled),
                         NULL);
  }

  add_hsep_to_box(LIVES_BOX(vbox));

  hbox = lives_hbox_new(TRUE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

  transition_combo = lives_standard_combo_new(_("_Transition Method:"),TRUE,merge_opts->trans_list,LIVES_BOX(hbox),NULL);

  lives_combo_set_active_index(LIVES_COMBO(transition_combo),defstart);

  mainw->last_transition_idx=merge_opts->list_to_rfx_index[defstart];

  add_hsep_to_box(LIVES_BOX(vbox));


  // now the dynamic part...
  merge_opts->param_vbox = lives_vbox_new(FALSE, 0);
  lives_container_set_border_width(LIVES_CONTAINER(merge_opts->param_vbox), widget_opts.border_width>>1);

  lives_box_pack_start(LIVES_BOX(vbox), merge_opts->param_vbox, TRUE, TRUE, 0);

  rfx=&mainw->rendered_fx[mainw->last_transition_idx];
  mainw->overflow_height=900;
  make_param_box(LIVES_VBOX(merge_opts->param_vbox), rfx);
  mainw->overflow_height=0;
  lives_widget_show_all(merge_opts->param_vbox);

  retvals=do_onchange_init(rfx);

  if (retvals!=NULL) {
    // now apply visually anything we got from onchange_init
    //param_demarshall (rfx,retvals,TRUE,TRUE);
    lives_list_free_strings(retvals);
    lives_list_free(retvals);
  }

  // done !


  dialog_action_area = lives_dialog_get_action_area(LIVES_DIALOG(merge_opts->merge_dialog));
  lives_button_box_set_layout(LIVES_BUTTON_BOX(dialog_action_area), LIVES_BUTTONBOX_END);

  cancelbutton = lives_button_new_from_stock(LIVES_STOCK_CANCEL,NULL);
  lives_dialog_add_action_widget(LIVES_DIALOG(merge_opts->merge_dialog), cancelbutton, LIVES_RESPONSE_CANCEL);
  lives_widget_set_can_focus(cancelbutton,TRUE);

  okbutton = lives_button_new_from_stock(LIVES_STOCK_OK,NULL);
  lives_dialog_add_action_widget(LIVES_DIALOG(merge_opts->merge_dialog), okbutton, LIVES_RESPONSE_OK);
  lives_widget_set_can_focus_and_default(okbutton);
  lives_widget_grab_default(okbutton);

  lives_signal_connect(LIVES_GUI_OBJECT(cancelbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                       LIVES_GUI_CALLBACK(on_merge_cancel_clicked),
                       rfx);


  lives_widget_add_accelerator(cancelbutton, LIVES_WIDGET_CLICKED_SIGNAL, accel_group,
                               LIVES_KEY_Escape, (LiVESXModifierType)0, (LiVESAccelFlags)0);


  lives_signal_connect(LIVES_GUI_OBJECT(okbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                       LIVES_GUI_CALLBACK(on_merge_ok_clicked),
                       rfx);

  lives_signal_connect(LIVES_GUI_OBJECT(transition_combo),LIVES_WIDGET_CHANGED_SIGNAL,LIVES_GUI_CALLBACK(on_trans_method_changed),NULL);


  lives_signal_connect(LIVES_GUI_OBJECT(align_start_button), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(on_align_start_end_toggled),
                       rfx);


  if (prefs->show_gui) {
    lives_widget_show_all(merge_opts->merge_dialog);
  }
}

static void bang(LiVESWidget *widget, livespointer null) {
  lives_widget_destroy(widget);
}


void on_trans_method_changed(LiVESCombo *combo, livespointer user_data) {
  lives_rfx_t *rfx;

  LiVESList *retvals;

  char *txt=lives_combo_get_active_text(combo);

  int idx;

  if (!strlen(txt)) {
    lives_free(txt);
    return;
  }

  rfx=&mainw->rendered_fx[mainw->last_transition_idx];

  lives_container_foreach(LIVES_CONTAINER(merge_opts->param_vbox),bang,NULL);
  on_paramwindow_cancel_clicked(NULL,rfx);

  idx=lives_list_strcmp_index(merge_opts->trans_list,txt);

  lives_free(txt);

  mainw->last_transition_idx=merge_opts->list_to_rfx_index[idx];
  rfx=&mainw->rendered_fx[mainw->last_transition_idx];

  make_param_box(LIVES_VBOX(merge_opts->param_vbox), rfx);
  lives_widget_show_all(merge_opts->param_vbox);

  retvals=do_onchange_init(rfx);

  if (retvals!=NULL) {
    // now apply visually anything we got from onchange_init
    param_demarshall(rfx,retvals,TRUE,TRUE);
    lives_list_free_strings(retvals);
    lives_list_free(retvals);
  }

  merge_opts->align_start=!merge_opts->align_start;
  on_align_start_end_toggled(NULL,NULL);
}




void on_merge_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  create_merge_dialog();

  merge_opts->loop_to_fit=mainw->last_transition_loop_to_fit;
  merge_opts->ins_frames=mainw->last_transition_ins_frames;
  merge_opts->align_start=!mainw->last_transition_align_start;

  on_align_start_end_toggled(NULL,NULL);
}



void on_merge_cancel_clicked(LiVESButton *button, livespointer user_data) {
  lives_rfx_t *rfx=(lives_rfx_t *)user_data;
  on_paramwindow_cancel_clicked(NULL,rfx);
  if (merge_opts->spinbutton_loops!=NULL)
    mainw->last_transition_loops=lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(merge_opts->spinbutton_loops));
  lives_widget_destroy(merge_opts->merge_dialog);
  lives_widget_context_update();
  mainw->last_transition_loop_to_fit=merge_opts->loop_to_fit;
  mainw->last_transition_ins_frames=merge_opts->ins_frames;
  mainw->last_transition_align_start=merge_opts->align_start;
  lives_list_free_strings(merge_opts->trans_list);
  lives_list_free(merge_opts->trans_list);
  lives_free(merge_opts->list_to_rfx_index);
  lives_free(merge_opts);
}





void on_merge_ok_clicked(LiVESButton *button, livespointer user_data) {
  int start,end;

  int cb_start=1;
  boolean cb_video_change=FALSE;
  int current_file=mainw->current_file;
  int old_frames=clipboard->frames;

  // save original values in case we cancel
  int oundo_start=cfile->undo_start;
  int oundo_end=cfile->undo_end;
  int cb_end,excess_frames;
  int times_to_loop=1;

  lives_rfx_t *rfx;

  if (merge_opts->spinbutton_loops!=NULL)
    mainw->last_transition_loops=lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(merge_opts->spinbutton_loops));

  mainw->last_transition_loop_to_fit=merge_opts->loop_to_fit;
  mainw->last_transition_ins_frames=merge_opts->ins_frames;
  mainw->last_transition_align_start=merge_opts->align_start;

  rfx=&mainw->rendered_fx[mainw->last_transition_idx];

  if (cfile->fps!=clipboard->fps) {
    if (!do_clipboard_fps_warning()) {
      on_paramwindow_cancel_clicked(NULL,rfx);
      lives_widget_destroy(merge_opts->merge_dialog);
      lives_widget_context_update();
      lives_list_free(merge_opts->trans_list);
      lives_free(merge_opts->list_to_rfx_index);
      lives_free(merge_opts);
      return;
    }
  }

  if (merge_opts->spinbutton_loops!=NULL)
    times_to_loop=lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(merge_opts->spinbutton_loops));
  else
    times_to_loop=1;

  on_paramwindow_cancel_clicked(NULL,rfx);
  lives_widget_destroy(merge_opts->merge_dialog);
  lives_widget_context_update();
  lives_list_free(merge_opts->trans_list);
  lives_free(merge_opts->list_to_rfx_index);
  lives_free(merge_opts);

  // if pref is set, resample clipboard video
  if (prefs->ins_resample&&cfile->fps!=clipboard->fps) {
    if (!resample_clipboard(cfile->fps)) return;
  }

  if ((cfile->end-cfile->start+1)<=clipboard->frames) {
    times_to_loop=1;
  }

  d_print(_("Merging clipboard with selection..."));

  excess_frames=clipboard->frames-(cfile->end-cfile->start+1);
  if (excess_frames<0) excess_frames=0;

  cfile->insert_start=0;
  cfile->insert_end=0;

  cfile->redoable=FALSE;
  lives_widget_set_sensitive(mainw->redo,FALSE);

  mainw->fx2_bool=FALSE;

  // dummy values - used for 'fit to audio' in insert
  mainw->fx1_bool=FALSE;
  mainw->fx1_val=1;  // times to insert

  // insert pre-frames
  if (!mainw->last_transition_align_start&&excess_frames>0&&mainw->last_transition_ins_frames) {
    mainw->insert_after=FALSE;
    d_print(P_("inserting %d extra frame before merge\n","inserting %d extra frames before merge\n",excess_frames),excess_frames);

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
  } else {
    //loop_to_fit_audio
    cb_end=cfile->end-cfile->start+1;
  }


  // here we use undo_start and undo_end to mark the merged section,
  // insert_start and insert_end to mark the inserted section (if any)
  if (mainw->last_transition_align_start) {
    cfile->undo_start=cfile->start;
    cfile->undo_end=cfile->start+(cb_end*times_to_loop)-1;
  } else {
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
    } else {
      cfile->progress_end=cfile->end;
      cfile->progress_start=cfile->end-cb_end+1;
    }
  } else {
    cfile->progress_start=cfile->start;
    cfile->progress_end=cfile->end;
  }

  // do the actual merge
  if (!do_effect(rfx,FALSE)) {
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
    set_undoable(NULL, FALSE);

    if (!mainw->last_transition_align_start&&excess_frames>0) {
      // we've added and now deleted frames, we need to redraw everything...
      cfile->start-=excess_frames;
      cfile->end-=excess_frames;
      switch_to_file(mainw->current_file,mainw->current_file);
    } else {
      sensitize();
      get_play_times();
    }
    return;
  }

  // insert any post frames
  if (mainw->last_transition_align_start&&excess_frames>0&&mainw->last_transition_ins_frames) {
    mainw->insert_after=TRUE;
    d_print(P_("now inserting %d extra frame\n","now inserting %d extra frames\n",excess_frames),excess_frames);

    // fx1_start and fx2_start hold the clipboard start/end values
    mainw->fx1_start=clipboard->frames-excess_frames+1;
    mainw->fx2_start=clipboard->frames;

    on_insert_activate(NULL,LIVES_INT_TO_POINTER(1));
  }

  if (excess_frames==0||!mainw->last_transition_ins_frames) {
    d_print_done();
  } else {
    d_print(_("Merge done.\n"));
  }

  if (cb_video_change) {
    clipboard->old_frames=old_frames;
    mainw->current_file=0;
    on_undo_activate(NULL,NULL);
    mainw->current_file=current_file;
  }

  cfile->undo_action=UNDO_MERGE;
  // can get overwritten by undo insert
  set_undoable(_(rfx->menu_text),TRUE);

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

  switch_to_file(mainw->current_file,mainw->current_file);
}


void after_spinbutton_loops_changed(LiVESSpinButton *spinbutton, livespointer user_data) {
  setmergealign();
}


void on_align_start_end_toggled(LiVESToggleButton *togglebutton, livespointer user_data) {
  merge_opts->align_start=!merge_opts->align_start;
  setmergealign();
}


void on_fit_toggled(LiVESToggleButton *togglebutton, livespointer user_data) {
  merge_opts->loop_to_fit=!merge_opts->loop_to_fit;
  lives_widget_set_sensitive(merge_opts->spinbutton_loops,!merge_opts->loop_to_fit);
  setmergealign();
}


void on_ins_frames_toggled(LiVESToggleButton *togglebutton, livespointer user_data) {
  merge_opts->ins_frames=!merge_opts->ins_frames;
}
