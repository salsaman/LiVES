
// interface.c
// LiVES
// (c) G. Finch 2003 - 2015 <salsaman@gmail.com>
// Released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

// TODO - use lives_widget_showall where poss.
// and don't forget lives_box_pack_end (doh)
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

// functions called in multitrack.c
extern void multitrack_preview_clicked(LiVESButton *, livespointer user_data);
extern void mt_change_disp_tracks_ok(LiVESButton *, livespointer user_data);


void add_suffix_check(LiVESBox *box, const char *ext) {
  char *ltext;

  LiVESWidget *checkbutton;

  if (ext==NULL) ltext=lives_strdup(_("Let LiVES set the _file extension"));
  else ltext=lives_strdup_printf(_("Let LiVES set the _file extension (.%s)"),ext);
  checkbutton=lives_standard_check_button_new(ltext,TRUE,box,NULL);
  lives_free(ltext);
  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(checkbutton), mainw->fx1_bool);
  lives_signal_connect_after(LIVES_GUI_OBJECT(checkbutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                             LIVES_GUI_CALLBACK(on_boolean_toggled),
                             &mainw->fx1_bool);

}



static LiVESWidget *add_deinterlace_checkbox(LiVESBox *for_deint) {
  LiVESWidget *hbox=lives_hbox_new(FALSE, 0);
  LiVESWidget *checkbutton = lives_standard_check_button_new(_("Apply _Deinterlace"),TRUE,LIVES_BOX(hbox),NULL);

  if (LIVES_IS_HBOX(for_deint)) {
    LiVESWidget *filler;
    lives_box_pack_start(for_deint, hbox, FALSE, FALSE, widget_opts.packing_width);
    lives_box_reorder_child(for_deint, hbox, 0);
    filler=add_fill_to_box(LIVES_BOX(for_deint));
    if (filler!=NULL) lives_box_reorder_child(for_deint, filler, 1);
  } else lives_box_pack_start(for_deint, hbox, FALSE, FALSE, widget_opts.packing_height);

  lives_widget_set_can_focus_and_default(checkbutton);
  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(checkbutton), mainw->open_deint);
  lives_signal_connect_after(LIVES_GUI_OBJECT(checkbutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                             LIVES_GUI_CALLBACK(on_boolean_toggled),
                             &mainw->open_deint);
  lives_widget_set_tooltip_text(checkbutton,_("If this is set, frames will be deinterlaced as they are imported."));

  lives_widget_show_all(LIVES_WIDGET(for_deint));

  return hbox;
}


static void pv_sel_changed(LiVESFileChooser *chooser, livespointer user_data) {
  LiVESSList *slist=lives_file_chooser_get_filenames(chooser);
  LiVESWidget *pbutton=(LiVESWidget *)user_data;

  if (slist==NULL||lives_slist_nth_data(slist,0)==NULL||lives_slist_length(slist)>1||
      !(lives_file_test((char *)lives_slist_nth_data(slist,0),LIVES_FILE_TEST_IS_REGULAR))) {
    lives_widget_set_sensitive(pbutton,FALSE);
  } else lives_widget_set_sensitive(pbutton,TRUE);

  if (slist!=NULL) {
    lives_list_free_strings((LiVESList *)slist);
    lives_slist_free(slist);
  }
}





void widget_add_preview(LiVESWidget *widget, LiVESBox *for_preview, LiVESBox *for_button, LiVESBox *for_deint, int preview_type) {
  LiVESWidget *preview_button=NULL;
  LiVESWidget *fs_label;

  mainw->fs_playframe = lives_frame_new(NULL);
  mainw->fs_playalign = lives_alignment_new(0.,0.,1.,1.);
  mainw->fs_playarea = lives_event_box_new();

  if (preview_type==LIVES_PREVIEW_TYPE_VIDEO_AUDIO||preview_type==LIVES_PREVIEW_TYPE_RANGE) {
    lives_container_set_border_width(LIVES_CONTAINER(mainw->fs_playframe), widget_opts.border_width);

    widget_opts.justify=LIVES_JUSTIFY_RIGHT;
    fs_label = lives_standard_label_new(_("Preview"));
    widget_opts.justify=LIVES_JUSTIFY_DEFAULT;
    lives_frame_set_label_widget(LIVES_FRAME(mainw->fs_playframe), fs_label);

    lives_box_pack_start(for_preview, mainw->fs_playframe, FALSE, FALSE, 0);

    lives_widget_set_size_request(mainw->fs_playarea, DEFAULT_FRAME_HSIZE, DEFAULT_FRAME_VSIZE);

    lives_container_add(LIVES_CONTAINER(mainw->fs_playframe), mainw->fs_playalign);
    lives_container_add(LIVES_CONTAINER(mainw->fs_playalign), mainw->fs_playarea);

    lives_widget_set_bg_color(mainw->fs_playarea, LIVES_WIDGET_STATE_NORMAL, &palette->black);
    lives_widget_set_bg_color(mainw->fs_playframe, LIVES_WIDGET_STATE_NORMAL, &palette->black);
    lives_widget_set_bg_color(mainw->fs_playalign, LIVES_WIDGET_STATE_NORMAL, &palette->black);

  }


  if (preview_type==LIVES_PREVIEW_TYPE_VIDEO_AUDIO) {
    preview_button = lives_button_new_with_mnemonic(_("Click here to _Preview any selected video, image or audio file"));
  } else if (preview_type==LIVES_PREVIEW_TYPE_AUDIO_ONLY) {
    preview_button = lives_button_new_with_mnemonic(_("Click here to _Preview any selected audio file"));
  } else if (preview_type==LIVES_PREVIEW_TYPE_RANGE) {
    preview_button = lives_button_new_with_mnemonic(_("Click here to _Preview the video"));
  }


  lives_box_pack_start(for_button, preview_button, FALSE, FALSE, widget_opts.packing_width);

  if (preview_type==LIVES_PREVIEW_TYPE_VIDEO_AUDIO||preview_type==LIVES_PREVIEW_TYPE_RANGE) {
    add_deinterlace_checkbox(for_deint);
  }

  lives_signal_connect(LIVES_GUI_OBJECT(preview_button), LIVES_WIDGET_CLICKED_SIGNAL,
                       LIVES_GUI_CALLBACK(on_fs_preview_clicked),
                       LIVES_INT_TO_POINTER(preview_type));

  if (LIVES_IS_FILE_CHOOSER(widget)) {
    lives_widget_set_sensitive(preview_button,FALSE);

    lives_signal_connect(LIVES_GUI_OBJECT(widget), LIVES_WIDGET_SELECTION_CHANGED_SIGNAL,
                         LIVES_GUI_CALLBACK(pv_sel_changed),
                         (livespointer)preview_button);
  }

}


xprocess *create_processing(const char *text) {

  LiVESWidget *dialog_vbox;
  LiVESWidget *vbox2;
  LiVESWidget *vbox3;
  LiVESWidget *dialog_action_area;
  LiVESWidget *details_arrow;

  LiVESAccelGroup *accel_group=LIVES_ACCEL_GROUP(lives_accel_group_new());

  xprocess *procw=(xprocess *)(lives_malloc(sizeof(xprocess)));

  char tmp_label[256];

  widget_opts.non_modal=TRUE;
  procw->processing = lives_standard_dialog_new(_("LiVES: - Processing..."),FALSE,-1,-1);
  widget_opts.non_modal=FALSE;

  lives_window_add_accel_group(LIVES_WINDOW(procw->processing), accel_group);

  if (prefs->show_gui) {
    if (mainw->multitrack==NULL) lives_window_set_transient_for(LIVES_WINDOW(procw->processing),LIVES_WINDOW(mainw->LiVES));
    else lives_window_set_transient_for(LIVES_WINDOW(procw->processing),LIVES_WINDOW(mainw->multitrack->window));
  }

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(procw->processing));
  lives_widget_show(dialog_vbox);

  vbox2 = lives_vbox_new(FALSE, 0);
  lives_widget_show(vbox2);
  lives_box_pack_start(LIVES_BOX(dialog_vbox), vbox2, TRUE, TRUE, 0);

  vbox3 = lives_vbox_new(FALSE, 0);
  lives_widget_show(vbox3);
  lives_box_pack_start(LIVES_BOX(vbox2), vbox3, TRUE, TRUE, 0);

  lives_snprintf(tmp_label,256,"%s...\n",text);
  procw->label = lives_standard_label_new(tmp_label);
  lives_widget_show(procw->label);

  lives_box_pack_start(LIVES_BOX(vbox3), procw->label, TRUE, TRUE, 0);

  procw->progressbar = lives_progress_bar_new();
  lives_widget_show(procw->progressbar);
  lives_box_pack_start(LIVES_BOX(vbox3), procw->progressbar, FALSE, FALSE, 0);
  if (palette->style&STYLE_1) {
    lives_widget_set_fg_color(procw->progressbar, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
  }

  widget_opts.justify=LIVES_JUSTIFY_CENTER;
  if (mainw->internal_messaging&&mainw->rte!=0) {
    procw->label2 = lives_standard_label_new(_("\n\nPlease Wait\n\nRemember to switch off effects (ctrl-0) afterwards !"));
  }
#ifdef RT_AUDIO
  else if (mainw->jackd_read!=NULL||mainw->pulsed_read!=NULL) procw->label2 = lives_label_new("");
#endif
  else procw->label2=lives_standard_label_new(_("\nPlease Wait"));
  widget_opts.justify=LIVES_JUSTIFY_DEFAULT;

  lives_widget_show(procw->label2);

  lives_box_pack_start(LIVES_BOX(vbox3), procw->label2, FALSE, FALSE, 0);

  widget_opts.justify=LIVES_JUSTIFY_CENTER;
  procw->label3 = lives_standard_label_new(PROCW_STRETCHER);
  lives_widget_show(procw->label3);
  lives_box_pack_start(LIVES_BOX(vbox3), procw->label3, FALSE, FALSE, 0);
  widget_opts.justify=LIVES_JUSTIFY_DEFAULT;

  if (mainw->iochan!=NULL) {
    // add "show details" arrow
    boolean woat=widget_opts.apply_theme;
    widget_opts.apply_theme=FALSE;
    procw->scrolledwindow = lives_standard_scrolled_window_new(ENC_DETAILS_WIN_H, ENC_DETAILS_WIN_V, LIVES_WIDGET(mainw->optextview));
    widget_opts.apply_theme=woat;

    details_arrow=lives_standard_expander_new(_("Show Details"),FALSE,LIVES_BOX(vbox3),procw->scrolledwindow);

    lives_widget_show_all(details_arrow);

  }


  lives_widget_show_all(vbox3);

  dialog_action_area = lives_dialog_get_action_area(LIVES_DIALOG(procw->processing));
  lives_button_box_set_layout(LIVES_BUTTON_BOX(dialog_action_area), LIVES_BUTTONBOX_END);

  procw->stop_button = lives_button_new_with_mnemonic(_("_Enough"));
  procw->preview_button = lives_button_new_with_mnemonic(_("_Preview"));

  if (cfile->nokeep) procw->pause_button = lives_button_new_with_mnemonic(_("Paus_e"));
  else procw->pause_button = lives_button_new_with_mnemonic(_("Pause/_Enough"));

  lives_dialog_add_action_widget(LIVES_DIALOG(procw->processing), procw->preview_button, 1);
  lives_widget_hide(procw->preview_button);
  lives_widget_set_can_focus_and_default(procw->preview_button);

  lives_dialog_add_action_widget(LIVES_DIALOG(procw->processing), procw->pause_button, 0);
  lives_widget_hide(procw->pause_button);
  lives_widget_set_can_focus_and_default(procw->pause_button);


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
      lives_dialog_add_action_widget(LIVES_DIALOG(procw->processing), procw->stop_button, 0);
      lives_widget_show(procw->stop_button);
      lives_widget_set_can_focus_and_default(procw->stop_button);
    }
  }

  procw->cancel_button = lives_button_new_with_mnemonic(_("_Cancel"));
  lives_dialog_add_action_widget(LIVES_DIALOG(procw->processing), procw->cancel_button, LIVES_RESPONSE_CANCEL);
  lives_widget_set_can_focus_and_default(procw->cancel_button);

  lives_widget_add_accelerator(procw->cancel_button, LIVES_WIDGET_CLICKED_SIGNAL, accel_group,
                               LIVES_KEY_Escape, (LiVESXModifierType)0, (LiVESAccelFlags)0);

  lives_signal_connect(LIVES_GUI_OBJECT(procw->stop_button), LIVES_WIDGET_CLICKED_SIGNAL,
                       LIVES_GUI_CALLBACK(on_stop_clicked),
                       NULL);

  lives_signal_connect(LIVES_GUI_OBJECT(procw->pause_button), LIVES_WIDGET_CLICKED_SIGNAL,
                       LIVES_GUI_CALLBACK(on_effects_paused),
                       NULL);

  if (mainw->multitrack!=NULL&&mainw->multitrack->is_rendering) {
    lives_signal_connect(LIVES_GUI_OBJECT(procw->preview_button), LIVES_WIDGET_CLICKED_SIGNAL,
                         LIVES_GUI_CALLBACK(multitrack_preview_clicked),
                         mainw->multitrack);
  } else {
    lives_signal_connect(LIVES_GUI_OBJECT(procw->preview_button), LIVES_WIDGET_CLICKED_SIGNAL,
                         LIVES_GUI_CALLBACK(on_preview_clicked),
                         NULL);
  }

  lives_signal_connect(LIVES_GUI_OBJECT(procw->cancel_button), LIVES_WIDGET_CLICKED_SIGNAL,
                       LIVES_GUI_CALLBACK(on_cancel_keep_button_clicked),
                       NULL);


  return procw;
}


#define TB_WIDTH 200
#define TB_HEIGHT_VID 80
#define TB_HEIGHT_AUD 50

static LiVESWidget *vid_text_view_new(void) {
  LiVESWidget *textview=lives_text_view_new();

  if (palette->style&STYLE_1) {
    lives_widget_set_base_color(textview, LIVES_WIDGET_STATE_NORMAL, &palette->info_base);
    lives_widget_set_text_color(textview, LIVES_WIDGET_STATE_NORMAL, &palette->info_text);
  }

  lives_widget_set_size_request(textview, TB_WIDTH, TB_HEIGHT_VID);
  lives_text_view_set_editable(LIVES_TEXT_VIEW(textview), FALSE);
  lives_text_view_set_justification(LIVES_TEXT_VIEW(textview), LIVES_JUSTIFY_CENTER);
  lives_text_view_set_cursor_visible(LIVES_TEXT_VIEW(textview), FALSE);

  return textview;
}

static LiVESWidget *aud_text_view_new(void) {
  LiVESWidget *textview=lives_text_view_new();

  if (palette->style&STYLE_1) {
    lives_widget_set_base_color(textview, LIVES_WIDGET_STATE_NORMAL, &palette->info_base);
    lives_widget_set_text_color(textview, LIVES_WIDGET_STATE_NORMAL, &palette->info_text);
  }

  lives_widget_set_size_request(textview, TB_WIDTH, TB_HEIGHT_AUD);
  lives_text_view_set_editable(LIVES_TEXT_VIEW(textview), FALSE);
  lives_text_view_set_justification(LIVES_TEXT_VIEW(textview), LIVES_JUSTIFY_CENTER);
  lives_text_view_set_cursor_visible(LIVES_TEXT_VIEW(textview), FALSE);

  return textview;
}


lives_clipinfo_t *create_clip_info_window(int audio_channels, boolean is_mt) {
  LiVESWidget *dialog_vbox;
  LiVESWidget *dialog_action_area;
  LiVESWidget *table;
  LiVESWidget *label;
  LiVESWidget *vidframe;
  LiVESWidget *laudframe;
  LiVESWidget *raudframe;
  LiVESWidget *okbutton;

  LiVESAccelGroup *accel_group;

  lives_clipinfo_t *filew=(lives_clipinfo_t *)(lives_malloc(sizeof(lives_clipinfo_t)));

  char *title;

  if (mainw->multitrack==NULL)
    title=lives_strdup_printf(_("LiVES: - %s"),cfile->name);
  else
    title=lives_strdup(_("LiVES: - Multitrack details"));

  filew->dialog = lives_standard_dialog_new(title,FALSE,-1,-1);
  lives_free(title);

  accel_group = LIVES_ACCEL_GROUP(lives_accel_group_new());
  lives_window_add_accel_group(LIVES_WINDOW(filew->dialog), accel_group);

  if (prefs->show_gui) {
    if (mainw->multitrack==NULL) lives_window_set_transient_for(LIVES_WINDOW(filew->dialog),LIVES_WINDOW(mainw->LiVES));
    else lives_window_set_transient_for(LIVES_WINDOW(filew->dialog),LIVES_WINDOW(mainw->multitrack->window));
  }

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(filew->dialog));

  if (cfile->frames>0||is_mt) {
    vidframe = lives_frame_new(NULL);

    lives_box_pack_start(LIVES_BOX(dialog_vbox), vidframe, TRUE, TRUE, 0);
    if (palette->style&STYLE_1) {
      lives_widget_set_bg_color(vidframe, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
    }

    table=lives_table_new(3,4,TRUE);

    lives_table_set_col_spacings(LIVES_TABLE(table), widget_opts.packing_width*4);
    lives_table_set_row_spacings(LIVES_TABLE(table), widget_opts.packing_height);
    lives_container_set_border_width(LIVES_CONTAINER(table), widget_opts.border_width);

    lives_container_add(LIVES_CONTAINER(vidframe), table);

    label = lives_standard_label_new(_("Format"));
    lives_table_attach(LIVES_TABLE(table), label, 0, 1, 0, 1,
                       (LiVESAttachOptions)(LIVES_FILL|LIVES_EXPAND),
                       (LiVESAttachOptions)(0), 0, 0);

    label = lives_standard_label_new(_("Frame size"));
    lives_table_attach(LIVES_TABLE(table), label, 0, 1, 1, 2,
                       (LiVESAttachOptions)(LIVES_FILL|LIVES_EXPAND),
                       (LiVESAttachOptions)(0), 0, 0);

    if (!is_mt) label = lives_standard_label_new(_("File size"));
    else label = lives_standard_label_new(_("Byte size"));
    lives_table_attach(LIVES_TABLE(table), label, 0, 1, 2, 3,
                       (LiVESAttachOptions)(LIVES_FILL|LIVES_EXPAND),
                       (LiVESAttachOptions)(0), 0, 0);

    label = lives_standard_label_new(_("FPS"));
    lives_table_attach(LIVES_TABLE(table), label, 2, 3, 0, 1,
                       (LiVESAttachOptions)(LIVES_FILL|LIVES_EXPAND),
                       (LiVESAttachOptions)(0), 0, 0);

    if (!is_mt) label = lives_standard_label_new(_("Frames"));
    else label = lives_standard_label_new(_("Events"));
    lives_table_attach(LIVES_TABLE(table), label, 2, 3, 1, 2,
                       (LiVESAttachOptions)(LIVES_FILL|LIVES_EXPAND),
                       (LiVESAttachOptions)(0), 0, 0);

    label = lives_standard_label_new(_("Total time"));
    lives_table_attach(LIVES_TABLE(table), label, 2, 3, 2, 3,
                       (LiVESAttachOptions)(LIVES_FILL|LIVES_EXPAND),
                       (LiVESAttachOptions)(0), 0, 0);

    filew->textview24 = vid_text_view_new();
    lives_table_attach(LIVES_TABLE(table), filew->textview24, 1, 2, 0, 1,
                       (LiVESAttachOptions)(LIVES_FILL|LIVES_EXPAND),
                       (LiVESAttachOptions)(0), 0, 0);

    filew->textview25 = vid_text_view_new();
    lives_table_attach(LIVES_TABLE(table), filew->textview25, 3, 4, 0, 1,
                       (LiVESAttachOptions)(LIVES_FILL|LIVES_EXPAND),
                       (LiVESAttachOptions)(0), 0, 0);

    filew->textview26 = vid_text_view_new();
    lives_table_attach(LIVES_TABLE(table), filew->textview26, 1, 2, 1, 2,
                       (LiVESAttachOptions)(LIVES_FILL|LIVES_EXPAND),
                       (LiVESAttachOptions)(0), 0, 0);

    filew->textview27 = vid_text_view_new();
    lives_table_attach(LIVES_TABLE(table), filew->textview27, 3, 4, 1, 2,
                       (LiVESAttachOptions)(LIVES_FILL|LIVES_EXPAND),
                       (LiVESAttachOptions)(0), 0, 0);

    filew->textview28 = vid_text_view_new();
    lives_table_attach(LIVES_TABLE(table), filew->textview28, 3, 4, 2, 3,
                       (LiVESAttachOptions)(LIVES_FILL|LIVES_EXPAND),
                       (LiVESAttachOptions)(0), 0, 0);

    filew->textview29 = vid_text_view_new();
    lives_table_attach(LIVES_TABLE(table), filew->textview29, 1, 2, 2, 3,
                       (LiVESAttachOptions)(LIVES_FILL|LIVES_EXPAND),
                       (LiVESAttachOptions)(0), 0, 0);


    label = lives_standard_label_new(_("Video"));
    lives_frame_set_label_widget(LIVES_FRAME(vidframe), label);
  }

  if (audio_channels>0) {
    laudframe = lives_frame_new(NULL);

    lives_box_pack_start(LIVES_BOX(dialog_vbox), laudframe, TRUE, TRUE, 0);
    if (palette->style&STYLE_1) {
      lives_widget_set_bg_color(laudframe, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
    }

    if (audio_channels>1) {
      label = lives_standard_label_new(_("Left Audio"));
    } else {
      label = lives_standard_label_new(_("Audio"));
    }

    lives_frame_set_label_widget(LIVES_FRAME(laudframe), label);

    table=lives_table_new(1,4,TRUE);

    lives_table_set_col_spacings(LIVES_TABLE(table), widget_opts.packing_width*4);
    lives_table_set_row_spacings(LIVES_TABLE(table), widget_opts.packing_height);
    lives_container_set_border_width(LIVES_CONTAINER(table), widget_opts.border_width);

    lives_container_add(LIVES_CONTAINER(laudframe), table);

    if (!is_mt) {
      label = lives_standard_label_new(_("Total time"));
      lives_table_attach(LIVES_TABLE(table), label, 0, 1, 0, 1,
                         (LiVESAttachOptions)(LIVES_FILL|LIVES_EXPAND),
                         (LiVESAttachOptions)(0), 0, 0);

      filew->textview_ltime = aud_text_view_new();
      lives_table_attach(LIVES_TABLE(table), filew->textview_ltime, 1, 2, 0, 1,
                         (LiVESAttachOptions)(LIVES_FILL|LIVES_EXPAND),
                         (LiVESAttachOptions)(0), 0, 0);
    }

    label = lives_standard_label_new(_("Rate/size"));
    lives_table_attach(LIVES_TABLE(table), label, 2, 3, 0, 1,
                       (LiVESAttachOptions)(LIVES_FILL|LIVES_EXPAND),
                       (LiVESAttachOptions)(0), 0, 0);

    filew->textview_lrate = aud_text_view_new();
    lives_table_attach(LIVES_TABLE(table), filew->textview_lrate, 3, 4, 0, 1,
                       (LiVESAttachOptions)(LIVES_FILL|LIVES_EXPAND),
                       (LiVESAttachOptions)(0), 0, 0);


    if (audio_channels>1) {
      raudframe = lives_frame_new(NULL);

      lives_box_pack_start(LIVES_BOX(dialog_vbox), raudframe, TRUE, TRUE, 0);
      if (palette->style&STYLE_1) {
        lives_widget_set_bg_color(raudframe, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
      }

      label = lives_standard_label_new(_("Right audio"));
      lives_frame_set_label_widget(LIVES_FRAME(raudframe), label);

      table=lives_table_new(1,4,TRUE);

      lives_table_set_col_spacings(LIVES_TABLE(table), widget_opts.packing_width*4);
      lives_table_set_row_spacings(LIVES_TABLE(table), widget_opts.packing_height);
      lives_container_set_border_width(LIVES_CONTAINER(table), widget_opts.border_width);

      lives_container_add(LIVES_CONTAINER(raudframe), table);

      if (!is_mt) {
        label = lives_standard_label_new(_("Total time"));
        lives_table_attach(LIVES_TABLE(table), label, 0, 1, 0, 1,
                           (LiVESAttachOptions)(LIVES_FILL|LIVES_EXPAND),
                           (LiVESAttachOptions)(0), 0, 0);

        filew->textview_rtime = aud_text_view_new();
        lives_table_attach(LIVES_TABLE(table), filew->textview_rtime, 1, 2, 0, 1,
                           (LiVESAttachOptions)(LIVES_FILL|LIVES_EXPAND),
                           (LiVESAttachOptions)(0), 0, 0);
      }

      label = lives_standard_label_new(_("Rate/size"));
      lives_table_attach(LIVES_TABLE(table), label, 2, 3, 0, 1,
                         (LiVESAttachOptions)(LIVES_FILL|LIVES_EXPAND),
                         (LiVESAttachOptions)(0), 0, 0);

      filew->textview_rrate = aud_text_view_new();
      lives_table_attach(LIVES_TABLE(table), filew->textview_rrate, 3, 4, 0, 1,
                         (LiVESAttachOptions)(LIVES_FILL|LIVES_EXPAND),
                         (LiVESAttachOptions)(0), 0, 0);

    }
  }

  dialog_action_area = lives_dialog_get_action_area(LIVES_DIALOG(filew->dialog));
  lives_button_box_set_layout(LIVES_BUTTON_BOX(dialog_action_area), LIVES_BUTTONBOX_SPREAD);

  okbutton = lives_button_new_from_stock(LIVES_STOCK_OK);
  lives_dialog_add_action_widget(LIVES_DIALOG(filew->dialog), okbutton, LIVES_RESPONSE_OK);
  lives_widget_set_can_focus_and_default(okbutton);
  lives_widget_grab_default(okbutton);

  lives_widget_set_size_request(okbutton,DEF_BUTTON_WIDTH*4,-1);

  lives_signal_connect(LIVES_GUI_OBJECT(okbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                       LIVES_GUI_CALLBACK(lives_general_button_clicked),
                       filew);

  accel_group = LIVES_ACCEL_GROUP(lives_accel_group_new());
  lives_window_add_accel_group(LIVES_WINDOW(filew->dialog), accel_group);

  lives_widget_add_accelerator(okbutton, LIVES_WIDGET_CLICKED_SIGNAL, accel_group,
                               LIVES_KEY_Escape, (LiVESXModifierType)0, (LiVESAccelFlags)0);


  lives_widget_show_all(filew->dialog);

  return filew;
}


static void on_resizecb_toggled(LiVESToggleButton *t, livespointer user_data) {
  LiVESWidget *cb=(LiVESWidget *)user_data;

  if (!lives_toggle_button_get_active(t)) {
    lives_widget_set_sensitive(cb,FALSE);
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(cb),FALSE);
  } else {
    lives_widget_set_sensitive(cb,TRUE);
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(cb),prefs->enc_letterbox);
  }
}




LiVESWidget *create_encoder_prep_dialog(const char *text1, const char *text2, boolean opt_resize) {
  LiVESWidget *dialog;
  LiVESWidget *dialog_vbox;
  LiVESWidget *dialog_action_area;
  LiVESWidget *cancelbutton;
  LiVESWidget *okbutton;
  LiVESWidget *checkbutton=NULL;
  LiVESWidget *checkbutton2;
  LiVESWidget *label;
  LiVESWidget *hbox;

  char *labeltext,*tmp,*tmp2;

  dialog = lives_standard_dialog_new(_("LiVES: - Encoding options"),FALSE,-1,-1);

  if (prefs->show_gui) {
    lives_window_set_transient_for(LIVES_WINDOW(dialog),LIVES_WINDOW(mainw->LiVES));
  }

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(dialog));

  widget_opts.justify=LIVES_JUSTIFY_CENTER;
  label = lives_standard_label_new(text1);
  widget_opts.justify=LIVES_JUSTIFY_DEFAULT;
  lives_box_pack_start(LIVES_BOX(dialog_vbox), label, TRUE, TRUE, 0);

  if (opt_resize) {
    if (text2!=NULL) labeltext=lives_strdup(_("<------------- (Check the box to re_size as suggested)"));
    else labeltext=lives_strdup(_("<------------- (Check the box to use the _size recommendation)"));

    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, FALSE, FALSE, widget_opts.packing_width);

    checkbutton = lives_standard_check_button_new(labeltext,TRUE,LIVES_BOX(hbox),NULL);

    lives_free(labeltext);

    lives_signal_connect_after(LIVES_GUI_OBJECT(checkbutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                               LIVES_GUI_CALLBACK(on_boolean_toggled),
                               &mainw->fx1_bool);

  } else if (text2==NULL) mainw->fx1_bool=TRUE;

  if (text2!=NULL&&(mainw->fx1_bool||opt_resize)) {

    hbox = lives_hbox_new(FALSE, 0);
    if (capable->has_composite&&capable->has_convert) {
      // only offer this if we have "composite" and "convert" - for now... TODO ****
      lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, FALSE, FALSE, widget_opts.packing_height);
    }

    checkbutton2 = lives_standard_check_button_new
                   ((tmp=lives_strdup(_("Use _letterboxing to maintain aspect ratio (optional)"))),TRUE,LIVES_BOX(hbox),
                    (tmp2=lives_strdup(_("Draw black rectangles either above or to the sides of the image, to prevent it from stretching."))));

    lives_free(tmp);
    lives_free(tmp2);

    if (opt_resize) {
      lives_widget_set_sensitive(checkbutton2,FALSE);
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(checkbutton2),FALSE);
    } else lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(checkbutton2),prefs->enc_letterbox);

    lives_signal_connect_after(LIVES_GUI_OBJECT(checkbutton2), LIVES_WIDGET_TOGGLED_SIGNAL,
                               LIVES_GUI_CALLBACK(on_boolean_toggled),
                               &prefs->enc_letterbox);

    if (opt_resize)
      lives_signal_connect_after(LIVES_GUI_OBJECT(checkbutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                                 LIVES_GUI_CALLBACK(on_resizecb_toggled),
                                 checkbutton2);

  }

  dialog_action_area = lives_dialog_get_action_area(LIVES_DIALOG(dialog));
  lives_button_box_set_layout(LIVES_BUTTON_BOX(dialog_action_area), LIVES_BUTTONBOX_END);

  if (text2!=NULL) {
    label = lives_standard_label_new(text2);
    lives_box_pack_start(LIVES_BOX(dialog_vbox), label, TRUE, TRUE, 0);
    cancelbutton = lives_button_new_from_stock(LIVES_STOCK_CANCEL);
    okbutton = lives_button_new_from_stock(LIVES_STOCK_OK);
  } else {
    cancelbutton = lives_button_new_with_mnemonic(_("Keep _my settings"));
    okbutton = lives_button_new_with_mnemonic(_("Use _recommended settings"));
  }

  lives_dialog_add_action_widget(LIVES_DIALOG(dialog), cancelbutton, LIVES_RESPONSE_CANCEL);
  lives_widget_set_can_focus_and_default(cancelbutton);

  lives_dialog_add_action_widget(LIVES_DIALOG(dialog), okbutton, LIVES_RESPONSE_OK);

  lives_widget_set_can_focus_and_default(okbutton);
  lives_widget_grab_default(okbutton);

  lives_widget_show_all(dialog);
  return dialog;
}


// Information/error dialog

// the type of message box here is with a single OK button
// if 2 or more buttons (e.g. OK/CANCEL, YES/NO, ABORT/RETRY/CANCEL) are needed, use create_message_dialog() in dialogs.c

LiVESWidget *create_info_error_dialog(lives_dialog_t info_type, const char *text, LiVESWindow *transient, int mask, boolean is_blocking) {
  LiVESWidget *dialog;

  if (!prefs->show_gui) {
    transient=NULL;
  } else {
    if (mainw->multitrack==NULL) transient=LIVES_WINDOW(mainw->LiVES);
    else transient=LIVES_WINDOW(mainw->multitrack->window);
  }

  dialog=create_message_dialog(info_type,text,transient,mask,is_blocking);
  return dialog;
}



text_window *create_text_window(const char *title, const char *text, LiVESTextBuffer *textbuffer) {
  // general text window
  LiVESWidget *dialog_vbox;
  LiVESWidget *scrolledwindow;
  LiVESWidget *dialog_action_area;
  LiVESWidget *okbutton;

  char *mytitle=lives_strdup(title);
  char *mytext=NULL;
  char *tmp;

  boolean woat;

  if (text!=NULL) mytext=lives_strdup(text);

  textwindow=(text_window *)lives_malloc(sizeof(text_window));

  textwindow->dialog = lives_standard_dialog_new((tmp=lives_strconcat("LiVES: - ",mytitle,NULL)),FALSE,DEF_DIALOG_WIDTH, DEF_DIALOG_HEIGHT);
  lives_free(tmp);

  if (prefs->show_gui) {
    lives_window_set_transient_for(LIVES_WINDOW(textwindow->dialog),mainw->multitrack==NULL?
                                   LIVES_WINDOW(mainw->LiVES):LIVES_WINDOW(mainw->multitrack->window));
  }

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(textwindow->dialog));

  if (textbuffer!=NULL) textwindow->textview = lives_text_view_new_with_buffer(textbuffer);
  else textwindow->textview = lives_text_view_new();

  woat=widget_opts.apply_theme;
  widget_opts.apply_theme=FALSE;
  scrolledwindow = lives_standard_scrolled_window_new(RFX_WINSIZE_H, RFX_WINSIZE_V, textwindow->textview);
  widget_opts.apply_theme=woat;

  lives_box_pack_start(LIVES_BOX(dialog_vbox), scrolledwindow, TRUE, TRUE, 0);

  lives_text_view_set_editable(LIVES_TEXT_VIEW(textwindow->textview), FALSE);
  lives_text_view_set_cursor_visible(LIVES_TEXT_VIEW(textwindow->textview), FALSE);

  if (palette->style&STYLE_1) {
    lives_widget_set_base_color(textwindow->textview, LIVES_WIDGET_STATE_NORMAL, &palette->info_base);
    lives_widget_set_text_color(textwindow->textview, LIVES_WIDGET_STATE_NORMAL, &palette->info_text);
    lives_widget_set_bg_color(lives_bin_get_child(LIVES_BIN(scrolledwindow)), LIVES_WIDGET_STATE_NORMAL, &palette->info_base);
  }

  if (mytext!=NULL) {
    lives_text_view_set_text(LIVES_TEXT_VIEW(textwindow->textview), mytext, -1);
  }

  if (mytext!=NULL||mainw->iochan!=NULL) {
    dialog_action_area = lives_dialog_get_action_area(LIVES_DIALOG(textwindow->dialog));
    lives_button_box_set_layout(LIVES_BUTTON_BOX(dialog_action_area), LIVES_BUTTONBOX_END);

    okbutton = lives_button_new_with_mnemonic(_("_Close Window"));

    LiVESWidget *savebutton = lives_button_new_with_mnemonic(_("_Save to file"));
    lives_dialog_add_action_widget(LIVES_DIALOG(textwindow->dialog), savebutton, LIVES_RESPONSE_YES);
    lives_dialog_add_action_widget(LIVES_DIALOG(textwindow->dialog), okbutton, LIVES_RESPONSE_OK);

    lives_signal_connect(LIVES_GUI_OBJECT(savebutton), LIVES_WIDGET_CLICKED_SIGNAL,
                         LIVES_GUI_CALLBACK(on_save_textview_clicked),
                         textwindow->textview);

    lives_signal_connect(LIVES_GUI_OBJECT(okbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                         LIVES_GUI_CALLBACK(lives_general_button_clicked),
                         textwindow);

  }

  if (mytitle!=NULL) lives_free(mytitle);
  if (mytext!=NULL) lives_free(mytext);

  if (prefs->show_gui)
    lives_widget_show_all(textwindow->dialog);

  return textwindow;
}



_insertw *create_insert_dialog(void) {
  LiVESWidget *dialog_vbox;
  LiVESWidget *hbox1;
  LiVESWidget *hbox;
  LiVESWidget *table;
  LiVESWidget *radiobutton;
  LiVESWidget *vseparator;
  LiVESWidget *dialog_action_area;
  LiVESWidget *cancelbutton;
  LiVESWidget *okbutton;

  LiVESSList *radiobutton1_group = NULL;
  LiVESSList *radiobutton2_group = NULL;

  LiVESAccelGroup *accel_group=LIVES_ACCEL_GROUP(lives_accel_group_new());

  char *tmp,*tmp2;

  _insertw *insertw=(_insertw *)(lives_malloc(sizeof(_insertw)));

  insertw->insert_dialog = lives_standard_dialog_new(_("LiVES: - Insert"),FALSE,-1,-1);

  lives_window_add_accel_group(LIVES_WINDOW(insertw->insert_dialog), accel_group);

  if (prefs->show_gui) {
    lives_window_set_transient_for(LIVES_WINDOW(insertw->insert_dialog),LIVES_WINDOW(mainw->LiVES));
  }

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(insertw->insert_dialog));

  hbox1 = lives_hbox_new(FALSE, 0);

  lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox1, TRUE, TRUE, widget_opts.packing_height);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(hbox1), hbox, FALSE, FALSE, widget_opts.packing_width);

  insertw->spinbutton_times = lives_standard_spin_button_new(_("_Number of times to insert"),
                              TRUE,1.,1.,10000.,1.,10.,0.,LIVES_BOX(hbox),NULL);

  add_fill_to_box(LIVES_BOX(hbox1));

  hbox = lives_hbox_new(FALSE, 0);

  lives_box_pack_start(LIVES_BOX(hbox1), hbox, FALSE, FALSE, widget_opts.packing_width);

  insertw->fit_checkbutton = lives_standard_check_button_new(_("_Insert to fit audio"),TRUE,LIVES_BOX(hbox),NULL);

  lives_widget_set_sensitive(LIVES_WIDGET(insertw->fit_checkbutton),cfile->achans>0&&clipboard->achans==0);

  add_hsep_to_box(LIVES_BOX(dialog_vbox));

  table = lives_table_new(2, 3, FALSE);
  lives_box_pack_start(LIVES_BOX(dialog_vbox), table, TRUE, TRUE, widget_opts.packing_height);
  lives_table_set_col_spacings(LIVES_TABLE(table), widget_opts.packing_width*4);
  lives_table_set_row_spacings(LIVES_TABLE(table), widget_opts.packing_height);


  hbox = lives_hbox_new(FALSE, 0);

  radiobutton=lives_standard_radio_button_new((tmp=lives_strdup(_("Insert _before selection"))),
              TRUE,radiobutton1_group,LIVES_BOX(hbox),
              (tmp2=lives_strdup(_("Insert clipboard before selected frames"))));

  lives_free(tmp);
  lives_free(tmp2);

  radiobutton1_group = lives_radio_button_get_group(LIVES_RADIO_BUTTON(radiobutton));

  lives_table_attach(LIVES_TABLE(table), hbox, 0, 1, 0, 1,
                     (LiVESAttachOptions)(LIVES_FILL),
                     (LiVESAttachOptions)(0), 0, 0);

  if (cfile->frames==0) {
    lives_widget_set_sensitive(radiobutton, FALSE);
  }

  hbox = lives_hbox_new(FALSE, 0);

  radiobutton=lives_standard_radio_button_new((tmp=lives_strdup(_("Insert _after selection"))),
              TRUE,radiobutton1_group,LIVES_BOX(hbox),
              (tmp2=lives_strdup(_("Insert clipboard after selected frames"))));

  lives_table_attach(LIVES_TABLE(table), hbox, 0, 1, 1, 2,
                     (LiVESAttachOptions)(LIVES_FILL),
                     (LiVESAttachOptions)(0), 0, 0);

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(radiobutton),mainw->insert_after);


  hbox = lives_hbox_new(FALSE, 0);

  insertw->with_sound=lives_standard_radio_button_new(_("Insert _with sound"),
                      TRUE,radiobutton2_group,LIVES_BOX(hbox),NULL);
  radiobutton2_group = lives_radio_button_get_group(LIVES_RADIO_BUTTON(insertw->with_sound));


  lives_table_attach(LIVES_TABLE(table), hbox, 2, 3, 0, 1,
                     (LiVESAttachOptions)(LIVES_FILL),
                     (LiVESAttachOptions)(0), 0, 0);

  if (cfile->achans==0&&clipboard->achans==0) lives_widget_set_sensitive(insertw->with_sound,FALSE);

  hbox = lives_hbox_new(FALSE, 0);

  insertw->without_sound=lives_standard_radio_button_new(_("Insert with_out sound"),
                         TRUE,radiobutton2_group,LIVES_BOX(hbox),NULL);

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(insertw->without_sound),
                                 !((cfile->achans>0||clipboard->achans>0)&&mainw->ccpd_with_sound));

  lives_table_attach(LIVES_TABLE(table), hbox, 2, 3, 1, 2,
                     (LiVESAttachOptions)(LIVES_FILL),
                     (LiVESAttachOptions)(0), 0, 0);

  lives_widget_set_sensitive(insertw->with_sound, clipboard->achans>0||cfile->achans>0);
  lives_widget_set_sensitive(insertw->without_sound, clipboard->achans>0||cfile->achans>0);

  vseparator = lives_vseparator_new();
  lives_table_attach(LIVES_TABLE(table), vseparator, 1, 2, 0, 1,
                     (LiVESAttachOptions)(LIVES_EXPAND | LIVES_FILL),
                     (LiVESAttachOptions)(LIVES_FILL), 0, 0);

  vseparator = lives_vseparator_new();
  lives_table_attach(LIVES_TABLE(table), vseparator, 1, 2, 1, 2,
                     (LiVESAttachOptions)(LIVES_FILL),
                     (LiVESAttachOptions)(LIVES_FILL), 0, 0);

  dialog_action_area = lives_dialog_get_action_area(LIVES_DIALOG(insertw->insert_dialog));

  lives_button_box_set_layout(LIVES_BUTTON_BOX(dialog_action_area), LIVES_BUTTONBOX_END);

  cancelbutton = lives_button_new_from_stock(LIVES_STOCK_CANCEL);
  lives_dialog_add_action_widget(LIVES_DIALOG(insertw->insert_dialog), cancelbutton, LIVES_RESPONSE_CANCEL);

  okbutton = lives_button_new_from_stock(LIVES_STOCK_OK);
  lives_dialog_add_action_widget(LIVES_DIALOG(insertw->insert_dialog), okbutton, LIVES_RESPONSE_OK);
  lives_widget_set_can_focus_and_default(okbutton);
  lives_widget_grab_default(okbutton);
  lives_widget_grab_focus(okbutton);

  lives_signal_connect(LIVES_GUI_OBJECT(insertw->with_sound), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(on_insertwsound_toggled),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(radiobutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(on_boolean_toggled),
                       &mainw->insert_after);
  lives_signal_connect(LIVES_GUI_OBJECT(cancelbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                       LIVES_GUI_CALLBACK(lives_general_button_clicked),
                       insertw);
  lives_signal_connect(LIVES_GUI_OBJECT(okbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                       LIVES_GUI_CALLBACK(on_insert_activate),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(insertw->fit_checkbutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(on_insfitaudio_toggled),
                       NULL);
  lives_signal_connect_after(LIVES_GUI_OBJECT(insertw->spinbutton_times), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                             LIVES_GUI_CALLBACK(on_spin_value_changed),
                             LIVES_INT_TO_POINTER(1));

  lives_widget_add_accelerator(cancelbutton, LIVES_WIDGET_CLICKED_SIGNAL, accel_group,
                               LIVES_KEY_Escape, (LiVESXModifierType)0, (LiVESAccelFlags)0);

  lives_widget_add_accelerator(okbutton, LIVES_WIDGET_CLICKED_SIGNAL, accel_group,
                               LIVES_KEY_Return, (LiVESXModifierType)0, (LiVESAccelFlags)0);

  lives_widget_show_all(insertw->insert_dialog);

  return insertw;
}





LiVESWidget *create_opensel_dialog(void) {
  LiVESWidget *opensel_dialog;
  LiVESWidget *dialog_vbox;
  LiVESWidget *vbox;
  LiVESWidget *table;
  LiVESWidget *label;
  LiVESWidget *spinbutton;
  LiVESWidget *dialog_action_area;
  LiVESWidget *cancelbutton;
  LiVESWidget *okbutton;

  boolean no_gui=widget_opts.no_gui;

  widget_opts.no_gui=TRUE; // work around bugs in gtk+
  opensel_dialog = lives_standard_dialog_new(_("LiVES: - Open Selection"),FALSE,-1,-1);
  widget_opts.no_gui=no_gui;

  if (prefs->show_gui) {
    if (mainw->multitrack==NULL) lives_window_set_transient_for(LIVES_WINDOW(opensel_dialog),LIVES_WINDOW(mainw->LiVES));
    else lives_window_set_transient_for(LIVES_WINDOW(opensel_dialog),LIVES_WINDOW(mainw->multitrack->window));
  }


  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(opensel_dialog));

  vbox = lives_vbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(dialog_vbox), vbox, TRUE, TRUE, 0);

  table = lives_table_new(2, 2, FALSE);
  lives_box_pack_start(LIVES_BOX(vbox), table, TRUE, TRUE, widget_opts.packing_height);

  lives_table_set_row_spacings(LIVES_TABLE(table), widget_opts.packing_height*2);

  label = lives_standard_label_new(_("Selection start time (sec)"));
  lives_table_attach(LIVES_TABLE(table), label, 0, 1, 0, 1,
                     (LiVESAttachOptions)(LIVES_EXPAND | LIVES_FILL),
                     (LiVESAttachOptions)(0), 0, 0);
  lives_label_set_halignment(LIVES_LABEL(label), 0.5);

  label = lives_standard_label_new(_("Number of frames to open"));
  lives_table_attach(LIVES_TABLE(table), label, 0, 1, 1, 2,
                     (LiVESAttachOptions)(LIVES_EXPAND | LIVES_FILL),
                     (LiVESAttachOptions)(0), 0, 0);
  lives_label_set_halignment(LIVES_LABEL(label), 0.5);

  spinbutton = lives_standard_spin_button_new(NULL, FALSE, 0., 0., 1000000000., 1., 10., 2, NULL, NULL);

  lives_signal_connect_after(LIVES_GUI_OBJECT(spinbutton), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                             LIVES_GUI_CALLBACK(on_spin_value_changed),
                             LIVES_INT_TO_POINTER(1));

  lives_table_attach(LIVES_TABLE(table), spinbutton, 1, 2, 0, 1,
                     (LiVESAttachOptions)(LIVES_EXPAND | LIVES_FILL),
                     (LiVESAttachOptions)(LIVES_EXPAND), widget_opts.packing_height*4+2, 0);


  spinbutton = lives_standard_spin_button_new(NULL,FALSE,1000.,1.,(double)LIVES_MAXINT, 1., 10., 0, NULL, NULL);

  lives_signal_connect_after(LIVES_GUI_OBJECT(spinbutton), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                             LIVES_GUI_CALLBACK(on_spin_value_changed),
                             LIVES_INT_TO_POINTER(2));

  lives_table_attach(LIVES_TABLE(table), spinbutton, 1, 2, 1, 2,
                     (LiVESAttachOptions)(LIVES_EXPAND | LIVES_FILL),
                     (LiVESAttachOptions)(LIVES_EXPAND), widget_opts.packing_height*4+2, 0);

  dialog_action_area = lives_dialog_get_action_area(LIVES_DIALOG(opensel_dialog));
  lives_button_box_set_layout(LIVES_BUTTON_BOX(dialog_action_area), LIVES_BUTTONBOX_END);

  cancelbutton = lives_button_new_from_stock(LIVES_STOCK_CANCEL);
  lives_dialog_add_action_widget(LIVES_DIALOG(opensel_dialog), cancelbutton, LIVES_RESPONSE_CANCEL);

  okbutton = lives_button_new_from_stock(LIVES_STOCK_OK);
  lives_dialog_add_action_widget(LIVES_DIALOG(opensel_dialog), okbutton, LIVES_RESPONSE_OK);
  lives_widget_set_can_focus_and_default(okbutton);
  lives_widget_grab_default(okbutton);

  widget_add_preview(opensel_dialog, LIVES_BOX(dialog_vbox), LIVES_BOX(dialog_vbox), LIVES_BOX(dialog_vbox), LIVES_PREVIEW_TYPE_RANGE);

  lives_signal_connect(LIVES_GUI_OBJECT(cancelbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                       LIVES_GUI_CALLBACK(on_cancel_opensel_clicked),
                       NULL);

  lives_signal_connect(LIVES_GUI_OBJECT(okbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                       LIVES_GUI_CALLBACK(on_opensel_range_ok_clicked),
                       NULL);


  lives_widget_show_all(opensel_dialog);

  return opensel_dialog;
}





_entryw *create_location_dialog(int type) {
  // type 1 is open location
  // type 2 is open youtube: - 3 fields:= URL, directory, file name

  LiVESWidget *dialog_vbox;
  LiVESWidget *dialog_action_area;
  LiVESWidget *cancelbutton;
  LiVESWidget *okbutton;
  LiVESWidget *label;
  LiVESWidget *checkbutton;
  LiVESWidget *hbox;
  LiVESWidget *buttond;

  _entryw *locw=(_entryw *)(lives_malloc(sizeof(_entryw)));

  LiVESAccelGroup *accel_group=LIVES_ACCEL_GROUP(lives_accel_group_new());

  char *title,*tmp,*tmp2;

  if (type==1)
    title=lives_strdup(_("LiVES: - Open Location"));
  else
    title=lives_strdup(_("LiVES: - Open Youtube Clip"));

  locw->dialog = lives_standard_dialog_new(title,FALSE,-1,-1);

  lives_free(title);

  lives_window_add_accel_group(LIVES_WINDOW(locw->dialog), accel_group);

  if (prefs->show_gui) {
    if (mainw->multitrack==NULL) lives_window_set_transient_for(LIVES_WINDOW(locw->dialog),LIVES_WINDOW(mainw->LiVES));
    else lives_window_set_transient_for(LIVES_WINDOW(locw->dialog),LIVES_WINDOW(mainw->multitrack->window));
  }

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(locw->dialog));

  widget_opts.justify=LIVES_JUSTIFY_CENTER;

  if (type==1) {
    label = lives_standard_label_new(
              _("\n\nTo open a stream, you must make sure that you have the correct libraries compiled in mplayer.\nAlso make sure you have set your bandwidth in Preferences|Streaming\n\n"));
  } else {
    label = lives_standard_label_new(
              _("\n\nTo open a clip from Youtube, LiVES will first download it with youtube-dl.\nPlease make sure you have the latest version of that tool installed.\n\n"));

    lives_box_pack_start(LIVES_BOX(dialog_vbox), label, FALSE, FALSE, 0);

    label=lives_standard_label_new(_("Enter the URL of the clip below.\nE.g: http://www.youtube.com/watch?v=WCR6f6WzjP8\n\n"));

  }

  widget_opts.justify=LIVES_JUSTIFY_DEFAULT;

  lives_box_pack_start(LIVES_BOX(dialog_vbox), label, FALSE, FALSE, 0);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, TRUE, TRUE, widget_opts.packing_height*2);

  locw->entry = lives_standard_entry_new(type==1?_("URL : "):_("Youtube URL : "),FALSE,"",79,32768,LIVES_BOX(hbox),NULL);

  if (type==1) {
    hbox=lives_hbox_new(FALSE, 0);
    checkbutton = lives_standard_check_button_new((tmp=lives_strdup(_("Do not send bandwidth information"))),
                  TRUE,LIVES_BOX(hbox),
                  (tmp2=lives_strdup(_("Try this setting if you are having problems getting a stream"))));

    lives_free(tmp);
    lives_free(tmp2);

    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(checkbutton),prefs->no_bandwidth);

    lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, FALSE, FALSE, widget_opts.packing_height*2);

    lives_signal_connect(LIVES_GUI_OBJECT(checkbutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                         LIVES_GUI_CALLBACK(on_boolean_toggled),
                         &prefs->no_bandwidth);

    add_deinterlace_checkbox(LIVES_BOX(dialog_vbox));

  }

  if (type==2) {
    hbox=lives_hbox_new(FALSE, 0);

    lives_box_pack_start(LIVES_BOX(dialog_vbox),hbox,TRUE,FALSE,widget_opts.packing_height);

    locw->dir_entry = lives_standard_entry_new(_("Download to _Directory : "),TRUE,mainw->vid_dl_dir,
                      72.*widget_opts.scale,PATH_MAX,LIVES_BOX(hbox),NULL);

    lives_entry_set_editable(LIVES_ENTRY(locw->dir_entry),FALSE);
    lives_entry_set_max_length(LIVES_ENTRY(locw->dir_entry),PATH_MAX);

    // add dir, with filechooser button
    buttond = lives_standard_file_button_new(TRUE,NULL);
    lives_label_set_mnemonic_widget(LIVES_LABEL(widget_opts.last_label),buttond);
    lives_box_pack_start(LIVES_BOX(hbox),buttond,FALSE,FALSE,widget_opts.packing_width);


    add_fill_to_box(LIVES_BOX(hbox));

    hbox=lives_hbox_new(FALSE, 0);

    lives_box_pack_start(LIVES_BOX(dialog_vbox),hbox,TRUE,FALSE,widget_opts.packing_height);

    locw->name_entry = lives_standard_entry_new(_("Download _File Name : "),TRUE,"",
                       74.*widget_opts.scale,PATH_MAX,LIVES_BOX(hbox),NULL);

    lives_signal_connect(buttond, LIVES_WIDGET_CLICKED_SIGNAL, LIVES_GUI_CALLBACK(on_filesel_button_clicked), (livespointer)locw->dir_entry);

    label=lives_standard_label_new(_(".webm"));

    lives_box_pack_start(LIVES_BOX(hbox),label,FALSE,FALSE,widget_opts.packing_width);
  }



  dialog_action_area = lives_dialog_get_action_area(LIVES_DIALOG(locw->dialog));
  lives_widget_show(dialog_action_area);
  lives_button_box_set_layout(LIVES_BUTTON_BOX(dialog_action_area), LIVES_BUTTONBOX_END);

  cancelbutton = lives_button_new_from_stock(LIVES_STOCK_CANCEL);
  lives_widget_show(cancelbutton);
  lives_dialog_add_action_widget(LIVES_DIALOG(locw->dialog), cancelbutton, LIVES_RESPONSE_CANCEL);
  lives_widget_set_can_focus_and_default(cancelbutton);

  okbutton = lives_button_new_from_stock(LIVES_STOCK_OK);
  lives_widget_show(okbutton);
  lives_dialog_add_action_widget(LIVES_DIALOG(locw->dialog), okbutton, LIVES_RESPONSE_OK);
  lives_widget_set_can_focus_and_default(okbutton);
  lives_widget_grab_default(okbutton);


  lives_signal_connect(LIVES_GUI_OBJECT(cancelbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                       LIVES_GUI_CALLBACK(lives_general_button_clicked),
                       locw);

  if (type==1)
    lives_signal_connect(LIVES_GUI_OBJECT(okbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                         LIVES_GUI_CALLBACK(on_location_select),
                         NULL);

  else if (type==2)
    lives_signal_connect(LIVES_GUI_OBJECT(okbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                         LIVES_GUI_CALLBACK(on_utube_select),
                         NULL);


  lives_widget_add_accelerator(cancelbutton, LIVES_WIDGET_CLICKED_SIGNAL, accel_group,
                               LIVES_KEY_Escape, (LiVESXModifierType)0, (LiVESAccelFlags)0);

  lives_widget_show_all(locw->dialog);

  return locw;
}

#define RW_ENTRY_DISPWIDTH 40

_entryw *create_rename_dialog(int type) {
  // type 1 = rename clip in menu
  // type 2 = save clip set
  // type 3 = reload clip set
  // type 4 = save clip set from mt
  // type 5 = save clip set for project export

  // type 6 = initial tempdir

  // type 7 = rename track in mt

  LiVESWidget *dialog_vbox;
  LiVESWidget *hbox;
  LiVESWidget *label;
  LiVESWidget *dialog_action_area;
  LiVESWidget *cancelbutton;
  LiVESWidget *okbutton;
  LiVESWidget *set_combo;
  LiVESWidget *dirbutton1;
  LiVESWidget *dirimage1;

  LiVESAccelGroup *accel_group=LIVES_ACCEL_GROUP(lives_accel_group_new());

  char *title=NULL;

  _entryw *renamew=(_entryw *)(lives_malloc(sizeof(_entryw)));

  renamew->setlist=NULL;

  if (type==1) {
    title=lives_strdup(_("LiVES: - Rename Clip"));
  } else if (type==2||type==4||type==5) {
    title=lives_strdup(_("LiVES: - Enter Set Name to Save as"));
  } else if (type==3) {
    title=lives_strdup(_("LiVES: - Enter a Set Name to Reload"));
  } else if (type==6) {
    title=lives_strdup(_("LiVES: - Choose a Working Directory"));
  } else if (type==7) {
    title=lives_strdup(_("LiVES: - Rename Current Track"));
  }

  renamew->dialog = lives_standard_dialog_new(title,FALSE,-1,-1);

  lives_window_add_accel_group(LIVES_WINDOW(renamew->dialog), accel_group);

  if (prefs->show_gui) {
    if (mainw->multitrack==NULL) {
      if (mainw->is_ready) {
        lives_window_set_transient_for(LIVES_WINDOW(renamew->dialog),LIVES_WINDOW(mainw->LiVES));
      }
    } else lives_window_set_transient_for(LIVES_WINDOW(renamew->dialog),LIVES_WINDOW(mainw->multitrack->window));
  }

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(renamew->dialog));

  if (type==4) {
    label = lives_standard_label_new
            (_("You need to enter a name for the current clip set.\nThis will allow you reload the layout with the same clips later.\nPlease enter the set name you wish to use.\nLiVES will remind you to save the clip set later when you try to exit.\n"));
    lives_box_pack_start(LIVES_BOX(dialog_vbox), label, FALSE, FALSE, 0);
  }

  if (type==5) {
    label = lives_standard_label_new
            (_("In order to export this project, you must enter a name for this clip set.\nThis will also be used for the project name.\n"));
    lives_box_pack_start(LIVES_BOX(dialog_vbox), label, FALSE, FALSE, 0);
  }


  if (type==6) {
    label = lives_standard_label_new
            (_("Welcome to LiVES !\nThis startup wizard will guide you through the\ninitial install so that you can get the most from this application.\n"));
    lives_box_pack_start(LIVES_BOX(dialog_vbox), label, FALSE, FALSE, 0);

    label = lives_standard_label_new
            (_("\nFirst of all you need to choose a working directory for LiVES.\nThis should be a directory with plenty of disk space available.\n"));
    lives_box_pack_start(LIVES_BOX(dialog_vbox), label, FALSE, FALSE, 0);
  }


  hbox = lives_hbox_new(FALSE, 0);

  if (type==3) {
    lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, TRUE, FALSE, widget_opts.packing_height*4);
  } else if (type!=6&&type!=7&&type!=1) {
    lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, TRUE, TRUE, widget_opts.packing_height*2);
  } else {
    lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, TRUE, TRUE, widget_opts.packing_height*4);
  }


  if (type==1||type==7) {
    label = lives_standard_label_new(_("New name "));
  } else if (type==2||type==3||type==4||type==5) {
    label = lives_standard_label_new(_("Set name "));
  } else {
    label = lives_standard_label_new("");
  }

  lives_box_pack_start(LIVES_BOX(hbox), label, FALSE, FALSE, widget_opts.packing_width*4);


  if (type==3) {

    set_combo=lives_combo_new();

    renamew->setlist=get_set_list(prefs->tmpdir,TRUE);

    lives_combo_populate(LIVES_COMBO(set_combo),renamew->setlist);

    renamew->entry=lives_combo_get_entry(LIVES_COMBO(set_combo));

    if (strlen(prefs->ar_clipset_name)) {
      // set default to our auto-reload clipset
      lives_entry_set_text(LIVES_ENTRY(renamew->entry),prefs->ar_clipset_name);
    }

    lives_box_pack_start(LIVES_BOX(hbox), set_combo, TRUE, TRUE, 0);

    lives_entry_set_completion_from_list(LIVES_ENTRY(renamew->entry),renamew->setlist);

  } else {
    char *tmp;
    renamew->entry = lives_entry_new();
    lives_entry_set_max_length(LIVES_ENTRY(renamew->entry),type==6?PATH_MAX:type==7?16:128);
    if (type==2&&strlen(mainw->set_name)) {
      lives_entry_set_text(LIVES_ENTRY(renamew->entry),(tmp=F2U8(mainw->set_name)));
      lives_free(tmp);
    }
    if (type==6) {
      char *tmpdir;
      if (prefs->startup_phase==-1) tmpdir=lives_build_filename(capable->home_dir,LIVES_TMP_NAME,NULL);
      else tmpdir=lives_strdup(prefs->tmpdir);
      lives_entry_set_text(LIVES_ENTRY(renamew->entry),(tmp=F2U8(tmpdir)));
      lives_free(tmp);
      lives_free(tmpdir);
    }
    lives_box_pack_start(LIVES_BOX(hbox), renamew->entry, TRUE, TRUE, 0);
  }


  if (type==6) {
    dirbutton1 = lives_button_new();

    dirimage1 = lives_image_new_from_stock(LIVES_STOCK_OPEN, LIVES_ICON_SIZE_BUTTON);

    lives_container_add(LIVES_CONTAINER(dirbutton1), dirimage1);

    lives_box_pack_start(LIVES_BOX(hbox), dirbutton1, FALSE, TRUE, widget_opts.packing_width);
    lives_signal_connect(dirbutton1, LIVES_WIDGET_CLICKED_SIGNAL, LIVES_GUI_CALLBACK(on_filesel_complex_clicked),renamew->entry);

  }


  lives_entry_set_activates_default(LIVES_ENTRY(renamew->entry), TRUE);
  lives_entry_set_width_chars(LIVES_ENTRY(renamew->entry),RW_ENTRY_DISPWIDTH);

  dialog_action_area = lives_dialog_get_action_area(LIVES_DIALOG(renamew->dialog));

  lives_button_box_set_layout(LIVES_BUTTON_BOX(dialog_action_area), LIVES_BUTTONBOX_END);

  cancelbutton = lives_button_new_from_stock(LIVES_STOCK_CANCEL);

  if (!(type==4&&!mainw->interactive)) {
    lives_dialog_add_action_widget(LIVES_DIALOG(renamew->dialog), cancelbutton, LIVES_RESPONSE_CANCEL);
    lives_widget_set_can_focus_and_default(cancelbutton);

    lives_widget_add_accelerator(cancelbutton, LIVES_WIDGET_CLICKED_SIGNAL, accel_group,
                                 LIVES_KEY_Escape, (LiVESXModifierType)0, (LiVESAccelFlags)0);
  }

  if (type==6) {
    okbutton = lives_button_new_from_stock(LIVES_STOCK_GO_FORWARD);
    lives_button_set_label(LIVES_BUTTON(okbutton),_("_Next"));
  } else okbutton = lives_button_new_from_stock(LIVES_STOCK_OK);

  lives_dialog_add_action_widget(LIVES_DIALOG(renamew->dialog), okbutton, LIVES_RESPONSE_OK);
  lives_widget_set_can_focus_and_default(okbutton);
  lives_widget_grab_default(okbutton);

  if (type!=4&&type!=2&&type!=5&&type!=3) {
    lives_signal_connect(LIVES_GUI_OBJECT(cancelbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                         LIVES_GUI_CALLBACK(lives_general_button_clicked),
                         renamew);
  }

  if (type==1) {
    lives_signal_connect(LIVES_GUI_OBJECT(okbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                         LIVES_GUI_CALLBACK(on_rename_set_name),
                         NULL);
  }

  lives_widget_add_accelerator(cancelbutton, LIVES_WIDGET_CLICKED_SIGNAL, accel_group,
                               LIVES_KEY_Escape, (LiVESXModifierType)0, (LiVESAccelFlags)0);

  lives_widget_show_all(renamew->dialog);

  lives_widget_grab_focus(renamew->entry);

  return renamew;
}


void on_liveinp_advanced_clicked(LiVESButton *button, livespointer user_data) {
  lives_tvcardw_t *tvcardw=(lives_tvcardw_t *)(user_data);

  tvcardw->use_advanced=!tvcardw->use_advanced;

  if (tvcardw->use_advanced) {
    lives_widget_show(tvcardw->adv_vbox);
    lives_button_set_label(LIVES_BUTTON(tvcardw->advbutton),_("Use def_aults"));
  } else {
    lives_button_set_label(LIVES_BUTTON(tvcardw->advbutton),_("_Advanced"));
    lives_window_resize(LIVES_WINDOW(lives_widget_get_toplevel(tvcardw->adv_vbox)),4,40);
    lives_widget_hide(tvcardw->adv_vbox);
  }

  lives_widget_queue_resize(lives_widget_get_parent(tvcardw->adv_vbox));

}


static void rb_tvcarddef_toggled(LiVESToggleButton *tbut, livespointer user_data) {
  lives_tvcardw_t *tvcardw=(lives_tvcardw_t *)(user_data);

  if (!lives_toggle_button_get_active(tbut)) {
    lives_widget_set_sensitive(tvcardw->spinbuttonw,TRUE);
    lives_widget_set_sensitive(tvcardw->spinbuttonh,TRUE);
    lives_widget_set_sensitive(tvcardw->spinbuttonf,TRUE);
  } else {
    lives_widget_set_sensitive(tvcardw->spinbuttonw,FALSE);
    lives_widget_set_sensitive(tvcardw->spinbuttonh,FALSE);
    lives_widget_set_sensitive(tvcardw->spinbuttonf,FALSE);
  }


}


static void after_dialog_combo_changed(LiVESWidget *combo, livespointer user_data) {
  LiVESList *list=(LiVESList *)user_data;
  char *etext=lives_combo_get_active_text(LIVES_COMBO(combo));
  mainw->fx1_val=lives_list_strcmp_index(list,etext);
  lives_free(etext);
}


LiVESWidget *create_combo_dialog(int type, livespointer user_data) {
  // create a dialog with combo box selector

  // type 1 == 1 combo box

  LiVESWidget *combo_dialog;
  LiVESWidget *dialog_vbox;
  LiVESWidget *label;
  LiVESWidget *combo;

  char *label_text=NULL,*title=NULL;

  LiVESList *list=(LiVESList *)user_data;

  if (type==1) {
    title=lives_strdup(_("LiVES:- Select input device"));
  }

  combo_dialog = lives_standard_dialog_new(title,TRUE,-1,-1);
  if (title!=NULL) lives_free(title);

  if (prefs->show_gui) {
    if (type==1) {
      lives_window_set_transient_for(LIVES_WINDOW(combo_dialog),LIVES_WINDOW(mainw->LiVES));
    } else {
      lives_window_set_transient_for(LIVES_WINDOW(combo_dialog),LIVES_WINDOW(mainw->multitrack->window));
    }
  }

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(combo_dialog));

  if (type==1) {
    label_text=lives_strdup(_("Select input device:"));
  }

  label = lives_standard_label_new(label_text);
  if (label_text!=NULL) lives_free(label_text);

  lives_box_pack_start(LIVES_BOX(dialog_vbox), label, TRUE, TRUE, 0);

  combo = lives_combo_new();

  lives_entry_set_width_chars(LIVES_ENTRY(lives_combo_get_entry(LIVES_COMBO(combo))), 64);

  lives_combo_populate(LIVES_COMBO(combo),list);

  lives_combo_set_active_index(LIVES_COMBO(combo), 0);

  lives_signal_connect_after(LIVES_WIDGET_OBJECT(combo), LIVES_WIDGET_CHANGED_SIGNAL, LIVES_GUI_CALLBACK(after_dialog_combo_changed), list);

  lives_box_pack_start(LIVES_BOX(dialog_vbox), combo, TRUE, TRUE, widget_opts.packing_height*2);

  if (type==1) {
    add_deinterlace_checkbox(LIVES_BOX(dialog_vbox));
  }

  if (prefs->show_gui)
    lives_widget_show_all(combo_dialog);

  return combo_dialog;
}


LiVESWidget *create_cdtrack_dialog(int type, livespointer user_data) {
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

  LiVESWidget *cd_dialog;
  LiVESWidget *dialog_vbox;
  LiVESWidget *hbox;
  LiVESWidget *spinbutton;
  LiVESWidget *dialog_action_area;
  LiVESWidget *cancelbutton;
  LiVESWidget *okbutton;

  LiVESAccelGroup *accel_group=LIVES_ACCEL_GROUP(lives_accel_group_new());

  LiVESSList *radiobutton_group=NULL;

  char *label_text=NULL,*title;


  if (type==0) {
    title=lives_strdup(_("LiVES:- Load CD Track"));
  } else if (type==1) {
    title=lives_strdup(_("LiVES:- Select DVD Title/Chapter"));
  } else if (type==2) {
    title=lives_strdup(_("LiVES:- Select VCD Title"));
  } else if (type==3) {
    title=lives_strdup(_("LiVES:- Change Maximum Visible Tracks"));
  } else {
    title=lives_strdup(_("LiVES:- Device details"));
  }

  cd_dialog = lives_standard_dialog_new(title,FALSE,-1,-1);
  lives_free(title);

  //lives_window_set_default_size (LIVES_WINDOW (cd_dialog), 300, 240);

  if (prefs->show_gui) {
    if (type==0||type==1||type==2||type==4||type==5) {
      lives_window_set_transient_for(LIVES_WINDOW(cd_dialog),LIVES_WINDOW(mainw->LiVES));
    } else {
      lives_window_set_transient_for(LIVES_WINDOW(cd_dialog),LIVES_WINDOW(mainw->multitrack->window));
    }
  }

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(cd_dialog));

  hbox = lives_hbox_new(FALSE, widget_opts.packing_width*5);
  lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, TRUE, TRUE, widget_opts.packing_height);

  if (type==0) {
    label_text=lives_strdup_printf(_("Track to load (from %s)"),prefs->cdplay_device);
  } else if (type==1) {
    label_text=lives_strdup(_("DVD Title"));
  } else if (type==2) {
    label_text=lives_strdup(_("VCD Title"));
  } else if (type==3) {
    label_text=lives_strdup(_("Maximum number of tracks to display"));
  } else if (type==4) {
    label_text=lives_strdup(_("Device:        /dev/video"));
  } else if (type==5) {
    label_text=lives_strdup(_("Device:        fw:"));
  }


  if (type==0||type==1||type==2) {
    spinbutton = lives_standard_spin_button_new(label_text,FALSE, mainw->fx1_val,
                 1., 256., 1., 10., 0,
                 LIVES_BOX(hbox),NULL);
  } else if (type==3) {
    spinbutton = lives_standard_spin_button_new(label_text,FALSE, mainw->fx1_val,
                 4., 8., 1., 1.,0,
                 LIVES_BOX(hbox),NULL);
  } else {
    spinbutton = lives_standard_spin_button_new(label_text,FALSE, 0.,
                 0., 31., 1., 1., 0,
                 LIVES_BOX(hbox),NULL);
  }

  lives_free(label_text);

  lives_signal_connect_after(LIVES_GUI_OBJECT(spinbutton), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                             LIVES_GUI_CALLBACK(on_spin_value_changed),
                             LIVES_INT_TO_POINTER(1));


  add_fill_to_box(LIVES_BOX(hbox));

  if (type==1||type==4) {

    hbox = lives_hbox_new(FALSE, widget_opts.packing_width*5);
    lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, TRUE, TRUE, widget_opts.packing_height);

    if (type==1) {
      spinbutton = lives_standard_spin_button_new(_("Chapter  "), FALSE, mainw->fx2_val,
                   1., 1024., 1., 10., 0,
                   LIVES_BOX(hbox),NULL);
    } else {
      spinbutton = lives_standard_spin_button_new(_("Channel  "), FALSE, 1.,
                   1., 69., 1., 1., 0,
                   LIVES_BOX(hbox),NULL);

    }

    lives_signal_connect_after(LIVES_GUI_OBJECT(spinbutton), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                               LIVES_GUI_CALLBACK(on_spin_value_changed),
                               LIVES_INT_TO_POINTER(2));


    if (type==1) {
      hbox = lives_hbox_new(FALSE, widget_opts.packing_width*5);
      lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, TRUE, TRUE, widget_opts.packing_height);

      spinbutton = lives_standard_spin_button_new(_("Audio ID  "), FALSE, mainw->fx3_val,
                   128., 159., 1., 1., 0,
                   LIVES_BOX(hbox),NULL);

      lives_signal_connect_after(LIVES_GUI_OBJECT(spinbutton), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                                 LIVES_GUI_CALLBACK(on_spin_value_changed),
                                 LIVES_INT_TO_POINTER(3));

    }
  }

  if (type==4||type==5) {
    hbox=add_deinterlace_checkbox(LIVES_BOX(dialog_vbox));
    add_fill_to_box(LIVES_BOX(hbox));
  }


  if (type==4) {
    LiVESList *dlist=NULL;
    LiVESList *olist=NULL;

    tvcardw=(lives_tvcardw_t *)lives_malloc(sizeof(lives_tvcardw_t));
    tvcardw->use_advanced=FALSE;

    dlist=lives_list_append(dlist,(livespointer)"autodetect");
    dlist=lives_list_append(dlist,(livespointer)"v4l2");
    dlist=lives_list_append(dlist,(livespointer)"v4l");
    dlist=lives_list_append(dlist,(livespointer)"bsdbt848");
    dlist=lives_list_append(dlist,(livespointer)"dummy");

    olist=lives_list_append(olist,(livespointer)"autodetect");
    olist=lives_list_append(olist,(livespointer)"yv12");
    olist=lives_list_append(olist,(livespointer)"rgb32");
    olist=lives_list_append(olist,(livespointer)"rgb24");
    olist=lives_list_append(olist,(livespointer)"rgb16");
    olist=lives_list_append(olist,(livespointer)"rgb15");
    olist=lives_list_append(olist,(livespointer)"uyvy");
    olist=lives_list_append(olist,(livespointer)"yuy2");
    olist=lives_list_append(olist,(livespointer)"i420");


    lives_box_set_spacing(LIVES_BOX(dialog_vbox),widget_opts.packing_height*2);

    hbox = lives_hbox_new(FALSE, widget_opts.packing_width*5);

    lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, FALSE, FALSE, widget_opts.packing_height*2);

    add_fill_to_box(LIVES_BOX(hbox));

    tvcardw->advbutton = lives_button_new_with_mnemonic(_("_Advanced"));

    lives_box_pack_start(LIVES_BOX(hbox), tvcardw->advbutton, TRUE, TRUE, widget_opts.packing_width*4);

    add_fill_to_box(LIVES_BOX(hbox));


    tvcardw->adv_vbox = lives_vbox_new(FALSE, widget_opts.packing_width*5);
    lives_box_pack_start(LIVES_BOX(dialog_vbox), tvcardw->adv_vbox, TRUE, TRUE, widget_opts.packing_height*2);


    // add input, width, height, fps, driver and outfmt


    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(tvcardw->adv_vbox), hbox, TRUE, FALSE, 0);

    tvcardw->spinbuttoni = lives_standard_spin_button_new(_("Input number"),FALSE,
                           0.,0.,16.,1.,1.,0,
                           LIVES_BOX(hbox),NULL);


    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(tvcardw->adv_vbox), hbox, TRUE, FALSE, 0);

    tvcardw->radiobuttond = lives_standard_radio_button_new(_("Use default width, height and FPS"),FALSE,
                            radiobutton_group,LIVES_BOX(hbox),NULL);
    radiobutton_group = lives_radio_button_get_group(LIVES_RADIO_BUTTON(tvcardw->radiobuttond));

    lives_signal_connect_after(LIVES_GUI_OBJECT(tvcardw->radiobuttond), LIVES_WIDGET_TOGGLED_SIGNAL,
                               LIVES_GUI_CALLBACK(rb_tvcarddef_toggled),
                               (livespointer)tvcardw);

    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(tvcardw->adv_vbox), hbox, TRUE, FALSE, 0);

    lives_standard_radio_button_new(NULL,FALSE,
                                    radiobutton_group,LIVES_BOX(hbox),NULL);

    tvcardw->spinbuttonw = lives_standard_spin_button_new(_("Width"),FALSE,
                           640.,4.,4096.,2.,2.,0,
                           LIVES_BOX(hbox),NULL);

    lives_widget_set_sensitive(tvcardw->spinbuttonw,FALSE);

    tvcardw->spinbuttonh = lives_standard_spin_button_new(_("Height"),FALSE,
                           480.,4.,4096.,2.,2.,0,
                           LIVES_BOX(hbox),NULL);

    lives_widget_set_sensitive(tvcardw->spinbuttonh,FALSE);

    tvcardw->spinbuttonf = lives_standard_spin_button_new(_("FPS"),FALSE,
                           25., 1., FPS_MAX, 1., 10., 3,
                           LIVES_BOX(hbox),NULL);

    lives_widget_set_sensitive(tvcardw->spinbuttonf,FALSE);

    hbox = lives_hbox_new(FALSE, 0);

    tvcardw->combod = lives_standard_combo_new(_("_Driver"),TRUE,dlist,LIVES_BOX(hbox),NULL);
    lives_combo_set_active_index(LIVES_COMBO(tvcardw->combod), 0);

    tvcardw->comboo = lives_standard_combo_new(_("_Output format"),TRUE,olist,LIVES_BOX(hbox),NULL);


    lives_widget_show_all(hbox);
    lives_box_pack_start(LIVES_BOX(tvcardw->adv_vbox), hbox, TRUE, FALSE, 0);

    lives_signal_connect(LIVES_GUI_OBJECT(tvcardw->advbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                         LIVES_GUI_CALLBACK(on_liveinp_advanced_clicked),
                         tvcardw);

    lives_widget_hide(tvcardw->adv_vbox);

    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(cd_dialog),"tvcard_data",tvcardw);

  }

  dialog_action_area = lives_dialog_get_action_area(LIVES_DIALOG(cd_dialog));
  lives_button_box_set_layout(LIVES_BUTTON_BOX(dialog_action_area), LIVES_BUTTONBOX_END);

  cancelbutton = lives_button_new_from_stock(LIVES_STOCK_CANCEL);
  lives_dialog_add_action_widget(LIVES_DIALOG(cd_dialog), cancelbutton, LIVES_RESPONSE_CANCEL);

  okbutton = lives_button_new_from_stock(LIVES_STOCK_OK);
  lives_dialog_add_action_widget(LIVES_DIALOG(cd_dialog), okbutton, LIVES_RESPONSE_OK);
  lives_widget_set_can_focus_and_default(okbutton);

  lives_widget_grab_default(okbutton);

  lives_widget_add_accelerator(cancelbutton, LIVES_WIDGET_CLICKED_SIGNAL, accel_group,
                               LIVES_KEY_Escape, (LiVESXModifierType)0, (LiVESAccelFlags)0);


  lives_widget_add_accelerator(okbutton, LIVES_WIDGET_CLICKED_SIGNAL, accel_group,
                               LIVES_KEY_Return, (LiVESXModifierType)0, (LiVESAccelFlags)0);

  if (type!=4&&type!=5) {
    lives_signal_connect(LIVES_GUI_OBJECT(cancelbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                         LIVES_GUI_CALLBACK(lives_general_button_clicked),
                         NULL);
  }

  if (type==0) {
    lives_signal_connect(LIVES_GUI_OBJECT(okbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                         LIVES_GUI_CALLBACK(on_load_cdtrack_ok_clicked),
                         NULL);
  } else if (type==1||type==2)  {
    lives_signal_connect(LIVES_GUI_OBJECT(okbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                         LIVES_GUI_CALLBACK(on_load_vcd_ok_clicked),
                         LIVES_INT_TO_POINTER(type));
  } else if (type==3)  {
    lives_signal_connect(LIVES_GUI_OBJECT(okbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                         LIVES_GUI_CALLBACK(mt_change_disp_tracks_ok),
                         user_data);
  }

  lives_window_add_accel_group(LIVES_WINDOW(cd_dialog), accel_group);

  lives_widget_show_all(cd_dialog);

  if (type==4) lives_widget_hide(tvcardw->adv_vbox);

  return cd_dialog;
}



static void rb_aud_sel_pressed(LiVESButton *button, livespointer user_data) {
  aud_dialog_t *audd=(aud_dialog_t *)user_data;
  audd->is_sel=!audd->is_sel;
  lives_widget_set_sensitive(audd->time_spin,!audd->is_sel);
}




aud_dialog_t *create_audfade_dialog(int type) {
  // type 0 = fade in
  // type 1 = fade out

  LiVESWidget *dialog_vbox;
  LiVESWidget *hbox;
  LiVESWidget *rb_time;
  LiVESWidget *rb_sel;
  LiVESWidget *label;

  char *label_text=NULL,*label_text2=NULL,*title;

  double max;

  LiVESSList *radiobutton_group = NULL;

  aud_dialog_t *audd=(aud_dialog_t *)lives_malloc(sizeof(aud_dialog_t));

  if (type==0) {
    title=lives_strdup(_("LiVES:- Fade Audio In"));
  } else {
    title=lives_strdup(_("LiVES:- Fade Audio Out"));
  }

  audd->dialog = lives_standard_dialog_new(title,TRUE,-1,-1);
  lives_free(title);

  if (prefs->show_gui) {
    lives_window_set_transient_for(LIVES_WINDOW(audd->dialog),LIVES_WINDOW(mainw->LiVES));
  }

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(audd->dialog));

  hbox = lives_hbox_new(FALSE, TB_HEIGHT_AUD);
  lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, TRUE, TRUE, widget_opts.packing_height);

  if (type==0) {
    label_text=lives_strdup(_("Fade in over  "));
    label_text2=lives_strdup(_("first"));
  } else if (type==1) {
    label_text=lives_strdup(_("Fade out over  "));
    label_text2=lives_strdup(_("last"));
  }


  label = lives_standard_label_new(label_text);
  if (label_text!=NULL) lives_free(label_text);

  lives_box_pack_start(LIVES_BOX(hbox), label, FALSE, FALSE, 0);

  hbox = lives_hbox_new(FALSE, widget_opts.packing_width*5);
  lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, TRUE, TRUE, widget_opts.packing_height);

  rb_time=lives_standard_radio_button_new(label_text2,FALSE,radiobutton_group,
                                          LIVES_BOX(hbox),NULL);

  radiobutton_group = lives_radio_button_get_group(LIVES_RADIO_BUTTON(rb_time));
  if (label_text2!=NULL) lives_free(label_text2);


  max=cfile->laudio_time;

  widget_opts.swap_label=TRUE;
  audd->time_spin = lives_standard_spin_button_new(_("seconds."),FALSE,
                    max/2.>DEF_AUD_FADE_SECS?DEF_AUD_FADE_SECS:max/2., .1, max, 1., 10., 2,
                    LIVES_BOX(hbox),NULL);
  widget_opts.swap_label=FALSE;

  hbox = lives_hbox_new(FALSE, widget_opts.packing_width*5);
  lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, TRUE, TRUE, widget_opts.packing_height);

  rb_sel=lives_standard_radio_button_new(_("selection"),FALSE,radiobutton_group,LIVES_BOX(hbox),NULL);

  audd->is_sel=FALSE;

  if ((cfile->end-1.)/cfile->fps>cfile->laudio_time) {
    // if selection is longer than audio time, we cannot use sel len
    lives_widget_set_sensitive(rb_sel,FALSE);
  } else {
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(rb_sel),TRUE);
    rb_aud_sel_pressed(LIVES_BUTTON(rb_sel),(livespointer)audd);
  }

  lives_signal_connect_after(LIVES_GUI_OBJECT(rb_sel), LIVES_WIDGET_TOGGLED_SIGNAL,
                             LIVES_GUI_CALLBACK(rb_aud_sel_pressed),
                             (livespointer)audd);


  add_fill_to_box(LIVES_BOX(hbox));


  lives_widget_show_all(audd->dialog);

  return audd;
}





_commentsw *create_comments_dialog(lives_clip_t *sfile, char *filename) {
  LiVESWidget *dialog_vbox;
  LiVESWidget *table;
  LiVESWidget *label;
  LiVESWidget *vbox;
  LiVESWidget *hbox;
  LiVESWidget *buttond;

  _commentsw *commentsw=(_commentsw *)(lives_malloc(sizeof(_commentsw)));

  commentsw->comments_dialog = lives_standard_dialog_new(_("LiVES: - File Comments (optional)"),TRUE,-1,-1);

  if (prefs->show_gui) {
    lives_window_set_transient_for(LIVES_WINDOW(commentsw->comments_dialog),LIVES_WINDOW(mainw->LiVES));
  }

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(commentsw->comments_dialog));

  table = lives_table_new(4, 2, FALSE);
  lives_container_set_border_width(LIVES_CONTAINER(table), widget_opts.border_width);

  lives_table_set_row_spacings(LIVES_TABLE(table), widget_opts.packing_height*2);

  lives_box_pack_start(LIVES_BOX(dialog_vbox), table, TRUE, TRUE, widget_opts.packing_height);

  label = lives_standard_label_new(_("Title/Name : "));

  lives_table_attach(LIVES_TABLE(table), label, 0, 1, 0, 1,
                     (LiVESAttachOptions)(LIVES_FILL),
                     (LiVESAttachOptions)(0), 0, 0);
  lives_label_set_halignment(LIVES_LABEL(label), 0.5);

  label = lives_standard_label_new(_("Author/Artist : "));

  lives_table_attach(LIVES_TABLE(table), label, 0, 1, 1, 2,
                     (LiVESAttachOptions)(LIVES_FILL),
                     (LiVESAttachOptions)(0), 0, 0);
  lives_label_set_halignment(LIVES_LABEL(label), 0.5);

  label = lives_standard_label_new(_("Comments : "));

  lives_table_attach(LIVES_TABLE(table), label, 0, 1, 3, 4,
                     (LiVESAttachOptions)(LIVES_FILL),
                     (LiVESAttachOptions)(0), 0, 0);

  lives_label_set_halignment(LIVES_LABEL(label), 0.5);

  commentsw->title_entry = lives_standard_entry_new(NULL,FALSE,cfile->title,80,-1,NULL,NULL);

  lives_table_attach(LIVES_TABLE(table), commentsw->title_entry, 1, 2, 0, 1,
                     (LiVESAttachOptions)(LIVES_EXPAND | LIVES_FILL),
                     (LiVESAttachOptions)(LIVES_EXPAND), 0, 0);

  commentsw->author_entry = lives_standard_entry_new(NULL,FALSE,cfile->author,80,-1,NULL,NULL);

  lives_table_attach(LIVES_TABLE(table), commentsw->author_entry, 1, 2, 1, 2,
                     (LiVESAttachOptions)(LIVES_EXPAND | LIVES_FILL),
                     (LiVESAttachOptions)(LIVES_EXPAND), 0, 0);

  commentsw->comment_entry = lives_standard_entry_new(NULL,FALSE,cfile->comment,80,250,NULL,NULL);

  lives_table_attach(LIVES_TABLE(table), commentsw->comment_entry, 1, 2, 3, 4,
                     (LiVESAttachOptions)(LIVES_EXPAND | LIVES_FILL),
                     (LiVESAttachOptions)(LIVES_EXPAND), 0, 0);

  if (sfile!=NULL) {
    // options
    vbox = lives_vbox_new(FALSE, 0);

    lives_standard_expander_new(_("_Options"),TRUE,LIVES_BOX(dialog_vbox),vbox);

    add_fill_to_box(LIVES_BOX(vbox));

    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

    commentsw->subt_checkbutton = lives_standard_check_button_new(_("Save _subtitles to file"),TRUE,LIVES_BOX(hbox),NULL);

    if (sfile->subt==NULL) {
      lives_widget_set_sensitive(commentsw->subt_checkbutton,FALSE);
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(commentsw->subt_checkbutton),FALSE);
    } else lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(commentsw->subt_checkbutton),TRUE);


    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

    commentsw->subt_entry=lives_standard_entry_new(_("Subtitle file"),FALSE,NULL,32,-1,LIVES_BOX(hbox),NULL);

    buttond = lives_button_new_with_mnemonic(_("Browse..."));

    lives_signal_connect(buttond, LIVES_WIDGET_CLICKED_SIGNAL,LIVES_GUI_CALLBACK(on_save_subs_activate),
                         (livespointer)commentsw->subt_entry);

    lives_box_pack_start(LIVES_BOX(hbox), buttond, FALSE, FALSE, widget_opts.packing_width);

    add_fill_to_box(LIVES_BOX(vbox));


    if (sfile->subt==NULL) {
      lives_widget_set_sensitive(commentsw->subt_entry,FALSE);
      lives_widget_set_sensitive(buttond,FALSE);
    } else {
      char xfilename[512];
      char *osubfname=NULL;

      lives_snprintf(xfilename,512,"%s",filename);
      get_filename(xfilename,FALSE); // strip extension
      switch (sfile->subt->type) {
      case SUBTITLE_TYPE_SRT:
        osubfname=lives_strdup_printf("%s.srt",xfilename);
        break;

      case SUBTITLE_TYPE_SUB:
        osubfname=lives_strdup_printf("%s.sub",xfilename);
        break;

      default:
        break;
      }
      lives_entry_set_text(LIVES_ENTRY(commentsw->subt_entry),osubfname);
      mainw->subt_save_file=osubfname; // assign instead of free
    }
  }

  lives_widget_show_all(commentsw->comments_dialog);

  return commentsw;
}


char last_good_folder[PATH_MAX];

static void chooser_check_dir(LiVESFileChooser *chooser, livespointer user_data) {
  char *cwd=lives_get_current_dir();
  char *new_dir;

#ifdef GUI_GTK
  new_dir=gtk_file_chooser_get_current_folder(chooser);
#endif
#ifdef GUI_QT
  QFileDialog *qchooser = static_cast<QFileDialog *>(chooser);
  new_dir = qchooser->directory().path().toLocal8Bit().data();
#endif

  if (!strcmp(new_dir,last_good_folder)) return;

  if (lives_chdir(new_dir,TRUE)) {
    lives_free(cwd);
#ifdef GUI_GTK
    gtk_file_chooser_set_current_folder(chooser,last_good_folder);
#endif
#ifdef GUI_QT
    qchooser->setDirectory(last_good_folder);
#endif
    do_dir_perm_access_error(new_dir);
    lives_free(new_dir);
    return;
  }
  lives_snprintf(last_good_folder,PATH_MAX,"%s",new_dir);
  lives_chdir(cwd,FALSE);
  lives_free(new_dir);
  lives_free(cwd);

}




char *choose_file(const char *dir, const char *fname, char **const filt, LiVESFileChooserAction act,
                  const char *title, LiVESWidget *extra_widget) {
  // new style file chooser

  // in/out values are in utf8 encoding

  LiVESWidget *chooser;

  char *mytitle;
  char *filename=NULL;

  int response;
  register int i;


  if (title==NULL) {
    if (act==LIVES_FILE_CHOOSER_ACTION_SELECT_DEVICE) {
      mytitle=lives_strdup(_("LiVES: - choose a device"));
      act=LIVES_FILE_CHOOSER_ACTION_OPEN;
    } else if (act==LIVES_FILE_CHOOSER_ACTION_SELECT_FOLDER) {
      mytitle=lives_strdup(_("LiVES: - choose a directory"));
    } else {
      mytitle=lives_strdup(_("LiVES: - choose a file"));
    }
  } else mytitle=lives_strdup(title);



#ifdef GUI_GTK

  if (act!=LIVES_FILE_CHOOSER_ACTION_SAVE) {
    if (mainw->interactive)
      chooser=gtk_file_chooser_dialog_new(mytitle,LIVES_WINDOW(mainw->LiVES),(LiVESFileChooserAction)act,
                                          LIVES_STOCK_LABEL_CANCEL, LIVES_RESPONSE_CANCEL,
                                          LIVES_STOCK_LABEL_OPEN, LIVES_RESPONSE_ACCEPT,
                                          NULL);
    else
      chooser=gtk_file_chooser_dialog_new(mytitle,LIVES_WINDOW(mainw->LiVES),(LiVESFileChooserAction)act,
                                          LIVES_STOCK_LABEL_OPEN, LIVES_RESPONSE_ACCEPT,
                                          NULL);
  } else {
    chooser=gtk_file_chooser_dialog_new(mytitle,LIVES_WINDOW(mainw->LiVES),(LiVESFileChooserAction)act,
                                        LIVES_STOCK_LABEL_CANCEL, LIVES_RESPONSE_CANCEL,
                                        LIVES_STOCK_LABEL_SAVE, LIVES_RESPONSE_ACCEPT,
                                        NULL);

  }


  gtk_file_chooser_set_local_only(LIVES_FILE_CHOOSER(chooser),TRUE);


  if (mainw->is_ready && palette->style&STYLE_1) {
    lives_widget_set_bg_color(chooser, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
    set_child_colour(chooser,FALSE);
  }

  if (dir!=NULL) {
    gtk_file_chooser_set_current_folder(LIVES_FILE_CHOOSER(chooser),dir);
    gtk_file_chooser_add_shortcut_folder(LIVES_FILE_CHOOSER(chooser),dir,NULL);
  }

  if (filt!=NULL) {
    GtkFileFilter *filter=gtk_file_filter_new();
    for (i=0; filt[i]!=NULL; i++) gtk_file_filter_add_pattern(filter,filt[i]);
    gtk_file_chooser_set_filter(LIVES_FILE_CHOOSER(chooser),filter);
    if (fname==NULL&&i==1&&act==LIVES_FILE_CHOOSER_ACTION_SAVE)
      gtk_file_chooser_set_current_name(LIVES_FILE_CHOOSER(chooser),filt[0]); //utf-8
  }

  if (fname!=NULL) {
    gtk_file_chooser_set_current_name(LIVES_FILE_CHOOSER(chooser),fname); // utf-8
    if (fname!=NULL&&dir!=NULL) {
      char *ffname=lives_build_filename(dir,fname,NULL);
      gtk_file_chooser_select_filename(LIVES_FILE_CHOOSER(chooser),ffname); // must be dir and file
      lives_free(ffname);
    }
  }

  if (extra_widget!=NULL && extra_widget!=mainw->LiVES) gtk_file_chooser_set_extra_widget(LIVES_FILE_CHOOSER(chooser),extra_widget);

#endif

#ifdef GUI_QT
  LiVESFileChooser *fchooser=new LiVESFileChooser();
  QFileDialog *qchooser = static_cast<QFileDialog *>(fchooser);
  qchooser->setModal(true);
  qchooser->setWindowTitle(QString::fromUtf8(mytitle));

  //  LiVESWidget *aarea = chooser->get_action_area();

  if (act!=LIVES_FILE_CHOOSER_ACTION_SAVE) {
    // open, select folder, create folder, select device
    //aarea->setStandardButtons(QDialogButtonBox::Open | QDialogButtonBox::Cancel);
    qchooser->setAcceptMode(QFileDialog::AcceptOpen);
  } else {
    //aarea->setStandardButtons(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    qchooser->setAcceptMode(QFileDialog::AcceptSave);
  }


  if (act == LIVES_FILE_CHOOSER_ACTION_SELECT_FOLDER || act == LIVES_FILE_CHOOSER_ACTION_CREATE_FOLDER) {
    qchooser->setFileMode(QFileDialog::Directory);
    qchooser->setOptions(QFileDialog::ShowDirsOnly);
  }

  if (act == LIVES_FILE_CHOOSER_ACTION_SELECT_FOLDER || act == LIVES_FILE_CHOOSER_ACTION_OPEN) {
    qchooser->setFileMode(QFileDialog::ExistingFile);
    qchooser->setOptions(QFileDialog::ReadOnly);
  }


  if (mainw->is_ready && palette->style&STYLE_1) {
    lives_widget_set_bg_color(fchooser, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
    //set_child_colour(chooser,FALSE);
  }

  if (dir!=NULL) {
    qchooser->setDirectory(QString::fromUtf8(dir));
    QList<QUrl> urls = qchooser->sidebarUrls();
    urls.append(QString::fromUtf8(dir));
    qchooser->setSidebarUrls(urls);
  }


  if (filt!=NULL) {
    QStringList filter;
    for (i=0; filt[i]!=NULL; i++) filter.append(QString::fromUtf8(filt[i]));

    qchooser->setNameFilters(filter);

    if (fname==NULL&&i==1&&act==LIVES_FILE_CHOOSER_ACTION_SAVE)
      qchooser->setDefaultSuffix(QString::fromUtf8(filt[0]));
  }

  // TODO
  //if (extra_widget!=NULL) gtk_file_chooser_set_extra_widget(fchooser,extra_widget);

  chooser = static_cast<LiVESWidget *>(fchooser);

#endif


  lives_container_set_border_width(LIVES_CONTAINER(chooser), widget_opts.border_width);

  if (prefs->show_gui) {
    if (mainw->multitrack==NULL) lives_window_set_transient_for(LIVES_WINDOW(chooser),LIVES_WINDOW(mainw->LiVES));
    else lives_window_set_transient_for(LIVES_WINDOW(chooser),LIVES_WINDOW(mainw->multitrack->window));
  }

  lives_signal_connect(chooser, LIVES_WIDGET_CURRENT_FOLDER_CHANGED_SIGNAL, LIVES_GUI_CALLBACK(chooser_check_dir), NULL);

  lives_widget_grab_focus(chooser);

  lives_widget_show_all(chooser);

  lives_window_center(LIVES_WINDOW(chooser));

  lives_window_set_modal(LIVES_WINDOW(chooser), TRUE);

  memset(last_good_folder,0,1);

  if (extra_widget==mainw->LiVES) {
    return (char *)chooser; // kludge to allow custom adding of extra widgets
  }

rundlg:

  if ((response=lives_dialog_run(LIVES_DIALOG(chooser)))!=LIVES_RESPONSE_CANCEL) {
    char *tmp;
    filename=lives_filename_to_utf8((tmp=lives_file_chooser_get_filename(LIVES_FILE_CHOOSER(chooser))),-1,NULL,NULL,NULL);
    lives_free(tmp);
  }

  if (filename!=NULL&&act==LIVES_FILE_CHOOSER_ACTION_SAVE) {
    if (!check_file(filename,TRUE)) {
      lives_free(filename);
      filename=NULL;
      goto rundlg;
    }
  }

  lives_free(mytitle);

  lives_widget_destroy(chooser);

  return filename;

}



LiVESWidget *choose_file_with_preview(const char *dir, const char *title, int preview_type) {
  // preview_type 1 - video and audio open (single - opensel)
  //LIVES_FILE_SELECTION_VIDEO_AUDIO

  // preview type 2 - import audio
  // LIVES_FILE_SELECTION_AUDIO_ONLY

  // preview_type 3 - video and audio open (multiple)
  //LIVES_FILE_SELECTION_VIDEO_AUDIO_MULTI

  // type 4
  // LIVES_FILE_SELECTION_VIDEO_RANGE

  LiVESWidget *chooser;

  chooser=(LiVESWidget *)choose_file(dir,NULL,NULL,LIVES_FILE_CHOOSER_ACTION_OPEN,title,mainw->LiVES);

  if (preview_type==LIVES_FILE_SELECTION_VIDEO_AUDIO_MULTI) {
#ifdef GUI_GTK
    gtk_file_chooser_set_select_multiple(LIVES_FILE_CHOOSER(chooser),TRUE);
#endif
#ifdef GUI_QT
    QFileDialog *qchooser = static_cast<QFileDialog *>(static_cast<LiVESFileChooser *>(chooser));
    qchooser->setFileMode(QFileDialog::ExistingFiles);
#endif
  }

  widget_add_preview(chooser,LIVES_BOX(lives_dialog_get_content_area(LIVES_DIALOG(chooser))),
                     LIVES_BOX(lives_dialog_get_content_area(LIVES_DIALOG(chooser))),
                     LIVES_BOX(lives_dialog_get_action_area(LIVES_DIALOG(chooser))),
                     (preview_type==LIVES_FILE_SELECTION_VIDEO_AUDIO||
                      preview_type==LIVES_FILE_SELECTION_VIDEO_AUDIO_MULTI)?LIVES_PREVIEW_TYPE_VIDEO_AUDIO:
                     LIVES_PREVIEW_TYPE_AUDIO_ONLY);

  if (prefs->fileselmax) {
    lives_window_set_resizable(LIVES_WINDOW(chooser),TRUE);
    lives_window_maximize(LIVES_WINDOW(chooser));
    lives_widget_queue_draw(chooser);
    lives_widget_context_update();
  }

  lives_widget_show_all(chooser);

  return chooser;
}






//cancel/discard/save dialog
_entryw *create_cds_dialog(int type) {
  // values for type are:
  // 0 == leave multitrack, user pref is warn when leave multitrack
  // 1 == exit from LiVES, or save set
  // 2 == ?
  // 3 == wipe layout confirmation
  // 4 == prompt for render after recording / viewing in mt


  LiVESWidget *dialog_vbox;
  LiVESWidget *dialog_action_area;
  LiVESWidget *cancelbutton;
  LiVESWidget *discardbutton;
  LiVESWidget *savebutton;
  LiVESWidget *label=NULL;
  LiVESWidget *hbox;
  LiVESAccelGroup *accel_group;

  _entryw *cdsw=(_entryw *)(lives_malloc(sizeof(_entryw)));

  cdsw->warn_checkbutton=NULL;

  cdsw->dialog = lives_standard_dialog_new(_("LiVES: - Cancel/Discard/Save"),FALSE,-1,-1);

  accel_group = LIVES_ACCEL_GROUP(lives_accel_group_new());
  lives_window_add_accel_group(LIVES_WINDOW(cdsw->dialog), accel_group);

  if (prefs->show_gui) {
    if (mainw->multitrack==NULL) lives_window_set_transient_for(LIVES_WINDOW(cdsw->dialog),LIVES_WINDOW(mainw->LiVES));
    else lives_window_set_transient_for(LIVES_WINDOW(cdsw->dialog),LIVES_WINDOW(mainw->multitrack->window));
  }

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(cdsw->dialog));

  widget_opts.justify=LIVES_JUSTIFY_CENTER;
  if (type==0) {
    if (strlen(mainw->multitrack->layout_name)==0) {
      label = lives_standard_label_new(
                _("You are about to leave multitrack mode.\nThe current layout has not been saved.\nWhat would you like to do ?\n"));
    } else {
      label = lives_standard_label_new(
                _("You are about to leave multitrack mode.\nThe current layout has been changed since the last save.\nWhat would you like to do ?\n"));
    }
  } else if (type==1) {
    if (!mainw->only_close) label = lives_standard_label_new(
                                        _("You are about to exit LiVES.\nThe current clip set can be saved.\nWhat would you like to do ?\n"));
    else label = lives_standard_label_new(_("The current clip set has not been saved.\nWhat would you like to do ?\n"));
  } else if (type==2||type==3) {
    if ((mainw->multitrack!=NULL&&mainw->multitrack->changed)||(mainw->stored_event_list!=NULL&&mainw->stored_event_list_changed)) {
      label = lives_standard_label_new(_("The current layout has not been saved.\nWhat would you like to do ?\n"));
    } else {
      label = lives_standard_label_new(_("The current layout has *not* been changed since it was last saved.\nWhat would you like to do ?\n"));
    }
  } else if (type==4) {
    if (mainw->multitrack!=NULL&&mainw->multitrack->changed) {
      label = lives_standard_label_new(
                _("The current layout contains generated frames and cannot be retained.\nYou may wish to render it before exiting multitrack mode.\n"));
    } else {
      label = lives_standard_label_new(
                _("You are about to leave multitrack mode.\nThe current layout contains generated frames and cannot be retained.\nWhat do you wish to do ?"));
    }
  }
  widget_opts.justify=LIVES_JUSTIFY_DEFAULT;

  lives_box_pack_start(LIVES_BOX(dialog_vbox), label, TRUE, TRUE, 0);

  if (type==1) {
    LiVESWidget *checkbutton;

    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

    cdsw->entry = lives_standard_entry_new(_("Clip set _name"),TRUE,strlen(mainw->set_name)?mainw->set_name:"",
                                           32.*widget_opts.scale,128.*widget_opts.scale,LIVES_BOX(hbox),NULL);

    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

    checkbutton = lives_standard_check_button_new(_("_Auto reload next time"),TRUE,LIVES_BOX(hbox),NULL);

    if ((type==0&&prefs->ar_layout)||(type==1&&!mainw->only_close)) {
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(checkbutton),TRUE);
      if (type==1) prefs->ar_clipset=TRUE;
      else prefs->ar_layout=TRUE;
    } else {
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(checkbutton),FALSE);
      if (type==1) prefs->ar_clipset=FALSE;
      else prefs->ar_layout=FALSE;
    }

    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(checkbutton),"cdsw",(livespointer)cdsw);

    lives_signal_connect(LIVES_GUI_OBJECT(checkbutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                         LIVES_GUI_CALLBACK(on_autoreload_toggled),
                         LIVES_INT_TO_POINTER(type));
  }

  if (type==0&&!(prefs->warning_mask&WARN_MASK_EXIT_MT)) {
    add_warn_check(LIVES_BOX(dialog_vbox),WARN_MASK_EXIT_MT);
  }

  dialog_action_area = lives_dialog_get_action_area(LIVES_DIALOG(cdsw->dialog));
  lives_button_box_set_layout(LIVES_BUTTON_BOX(dialog_action_area), LIVES_BUTTONBOX_END);

  cancelbutton = lives_button_new_from_stock(LIVES_STOCK_CANCEL);
  lives_dialog_add_action_widget(LIVES_DIALOG(cdsw->dialog), cancelbutton, LIVES_RESPONSE_CANCEL);
  lives_widget_add_accelerator(cancelbutton, LIVES_WIDGET_CLICKED_SIGNAL, accel_group,
                               LIVES_KEY_Escape, (LiVESXModifierType)0, (LiVESAccelFlags)0);

  discardbutton = lives_button_new_from_stock(LIVES_STOCK_DELETE);

  lives_dialog_add_action_widget(LIVES_DIALOG(cdsw->dialog), discardbutton, 1+(type==2));
  lives_button_set_use_underline(LIVES_BUTTON(discardbutton),TRUE);
  if ((type==0&&strlen(mainw->multitrack->layout_name)==0)||type==3||type==4)
    lives_button_set_label(LIVES_BUTTON(discardbutton),_("_Wipe layout"));
  else if (type==0) lives_button_set_label(LIVES_BUTTON(discardbutton),_("_Ignore changes"));
  else if (type==1) lives_button_set_label(LIVES_BUTTON(discardbutton),_("_Delete clip set"));
  else if (type==2) lives_button_set_label(LIVES_BUTTON(discardbutton),_("_Delete layout"));

  savebutton = lives_button_new_from_stock(LIVES_STOCK_SAVE);
  lives_button_set_use_underline(LIVES_BUTTON(savebutton),TRUE);
  if (type==0||type==3) lives_button_set_label(LIVES_BUTTON(savebutton),_("_Save layout"));
  else if (type==1) lives_button_set_label(LIVES_BUTTON(savebutton),_("_Save clip set"));
  else if (type==2) lives_button_set_label(LIVES_BUTTON(savebutton),_("_Wipe layout"));
  if (type!=4) lives_dialog_add_action_widget(LIVES_DIALOG(cdsw->dialog), savebutton, 2-(type==2));
  lives_widget_set_can_focus_and_default(savebutton);
  if (type==1||type==2)lives_widget_grab_default(savebutton);

  lives_widget_show_all(cdsw->dialog);

  if (type==1) {
    lives_widget_grab_focus(cdsw->entry);
  }

  if (!mainw->interactive) lives_widget_set_sensitive(cancelbutton,FALSE);

  return cdsw;
}




void do_layout_recover_dialog(void) {
  if (!do_yesno_dialog(_("\nLiVES has detected a multitrack layout from a previous session.\nWould you like to try and recover it ?\n")))
    recover_layout_cancelled(TRUE);
  else recover_layout();
}


static void flip_cdisk_bit(LiVESToggleButton *t, livespointer user_data) {
  uint32_t bitmask=LIVES_POINTER_TO_INT(user_data);
  prefs->clear_disk_opts^=bitmask;
}


LiVESWidget *create_cleardisk_advanced_dialog(void) {
  LiVESWidget *dialog;
  LiVESWidget *dialog_vbox;
  LiVESWidget *scrollw;
  LiVESWidget *vbox;
  LiVESWidget *hbox;
  LiVESWidget *checkbutton;
  LiVESWidget *okbutton;
  LiVESWidget *resetbutton;

  boolean woat=widget_opts.apply_theme;

  char *tmp,*tmp2;

  dialog = lives_standard_dialog_new(_("LiVES: - Disk Recovery Options"),FALSE,-1,-1);

  if (prefs->show_gui) {
    if (mainw->multitrack==NULL) lives_window_set_transient_for(LIVES_WINDOW(dialog),LIVES_WINDOW(mainw->LiVES));
    else lives_window_set_transient_for(LIVES_WINDOW(dialog),LIVES_WINDOW(mainw->multitrack->window));
  }

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(dialog));

  vbox = lives_vbox_new(FALSE, 0);
  lives_container_set_border_width(LIVES_CONTAINER(vbox), widget_opts.border_width*2);

  widget_opts.apply_theme=FALSE;
  scrollw = lives_standard_scrolled_window_new(450.*widget_opts.scale,300.*widget_opts.scale,vbox);
  widget_opts.apply_theme=woat;

  lives_container_add(LIVES_CONTAINER(dialog_vbox), scrollw);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, TRUE, widget_opts.packing_height);

  checkbutton = lives_standard_check_button_new((tmp=lives_strdup(_("Delete _Orphaned Clips"))),TRUE,LIVES_BOX(hbox),
                (tmp2=lives_strdup(_("Delete any clips which are not currently loaded or part of a set"))));

  lives_free(tmp);
  lives_free(tmp2);

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(checkbutton), !(prefs->clear_disk_opts & LIVES_CDISK_LEAVE_ORPHAN_SETS));

  lives_signal_connect_after(LIVES_GUI_OBJECT(checkbutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                             LIVES_GUI_CALLBACK(flip_cdisk_bit),
                             LIVES_INT_TO_POINTER(LIVES_CDISK_LEAVE_ORPHAN_SETS));

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, TRUE, widget_opts.packing_height);

  checkbutton = lives_standard_check_button_new(_("Clear _Backup Files from Closed Clips"),TRUE,LIVES_BOX(hbox),NULL);

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(checkbutton), !(prefs->clear_disk_opts & LIVES_CDISK_LEAVE_BFILES));

  lives_signal_connect_after(LIVES_GUI_OBJECT(checkbutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                             LIVES_GUI_CALLBACK(flip_cdisk_bit),
                             LIVES_INT_TO_POINTER(LIVES_CDISK_LEAVE_BFILES));

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, TRUE, widget_opts.packing_height);

  checkbutton = lives_standard_check_button_new(_("Remove Sets which have _Layouts but no Clips"),TRUE,LIVES_BOX(hbox),NULL);

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(checkbutton),
                                 (prefs->clear_disk_opts & LIVES_CDISK_REMOVE_ORPHAN_LAYOUTS));

  lives_signal_connect_after(LIVES_GUI_OBJECT(checkbutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                             LIVES_GUI_CALLBACK(flip_cdisk_bit),
                             LIVES_INT_TO_POINTER(LIVES_CDISK_REMOVE_ORPHAN_LAYOUTS));

  resetbutton = lives_button_new_from_stock(LIVES_STOCK_REFRESH);
  lives_dialog_add_action_widget(LIVES_DIALOG(dialog), resetbutton, LIVES_RESPONSE_RETRY);
  lives_button_set_label(LIVES_BUTTON(resetbutton),_("_Reset to Defaults"));

  okbutton = lives_button_new_from_stock(LIVES_STOCK_OK);
  lives_dialog_add_action_widget(LIVES_DIALOG(dialog), okbutton, LIVES_RESPONSE_OK);

  lives_widget_set_can_focus_and_default(okbutton);
  lives_widget_grab_default(okbutton);
  lives_button_set_label(LIVES_BUTTON(okbutton),_("_Accept"));

  return dialog;

}


LiVESTextView *create_output_textview(void) {
  LiVESWidget *textview=lives_text_view_new();
  lives_text_view_set_editable(LIVES_TEXT_VIEW(textview), FALSE);

  if (palette->style&STYLE_1) {
    lives_widget_set_base_color(textview, LIVES_WIDGET_STATE_NORMAL, &palette->info_base);
    lives_widget_set_text_color(textview, LIVES_WIDGET_STATE_NORMAL, &palette->info_text);
  }

  lives_object_ref_sink(textview);
  lives_object_ref(textview);
  return LIVES_TEXT_VIEW(textview);
}

