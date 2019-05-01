// interface.c
// LiVES
// (c) G. Finch 2003 - 2018 <salsaman+lives@gmail.com>
// Released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#include "main.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include "callbacks.h"
#include "interface.h"
#include "merge.h"
#include "support.h"
#include "omc-learn.h" // for OSC_NOTIFY mapping

// functions called in multitrack.c
extern void multitrack_preview_clicked(LiVESButton *, livespointer user_data);
extern void mt_change_disp_tracks_ok(LiVESButton *, livespointer user_data);

void add_suffix_check(LiVESBox *box, const char *ext) {
  char *ltext;

  LiVESWidget *checkbutton;

  if (ext == NULL) ltext = lives_strdup(_("Let LiVES set the _file extension"));
  else ltext = lives_strdup_printf(_("Let LiVES set the _file extension (.%s)"), ext);
  checkbutton = lives_standard_check_button_new(ltext, mainw->fx1_bool, box, NULL);
  lives_free(ltext);
  lives_signal_connect_after(LIVES_GUI_OBJECT(checkbutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                             LIVES_GUI_CALLBACK(on_boolean_toggled),
                             &mainw->fx1_bool);
}


static LiVESWidget *add_deinterlace_checkbox(LiVESBox *for_deint) {
  char *tmp, *tmp2;
  LiVESWidget *hbox = lives_hbox_new(FALSE, 0);
  LiVESWidget *checkbutton = lives_standard_check_button_new((tmp = lives_strdup(_("Apply _Deinterlace"))), mainw->open_deint,
                             LIVES_BOX(hbox),
                             (tmp2 = lives_strdup(_("If this is set, frames will be deinterlaced as they are imported."))));
  lives_free(tmp);
  lives_free(tmp2);

  if (LIVES_IS_HBOX(for_deint)) {
    LiVESWidget *filler;
    lives_box_pack_start(for_deint, hbox, FALSE, FALSE, widget_opts.packing_width);
    lives_box_reorder_child(for_deint, hbox, 0);
    filler = add_fill_to_box(LIVES_BOX(for_deint));
    if (filler != NULL) lives_box_reorder_child(for_deint, filler, 1);
  } else lives_box_pack_start(for_deint, hbox, FALSE, FALSE, widget_opts.packing_height);

  lives_widget_set_can_focus_and_default(checkbutton);
  lives_signal_connect_after(LIVES_GUI_OBJECT(checkbutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                             LIVES_GUI_CALLBACK(on_boolean_toggled),
                             &mainw->open_deint);

  lives_widget_show_all(LIVES_WIDGET(for_deint));

  return hbox;
}


static void pv_sel_changed(LiVESFileChooser *chooser, livespointer user_data) {
  LiVESSList *slist = lives_file_chooser_get_filenames(chooser);
  LiVESWidget *pbutton = (LiVESWidget *)user_data;

  if (slist == NULL || lives_slist_nth_data(slist, 0) == NULL || lives_slist_length(slist) > 1 ||
      !(lives_file_test((char *)lives_slist_nth_data(slist, 0), LIVES_FILE_TEST_IS_REGULAR))) {
    lives_widget_set_sensitive(pbutton, FALSE);
  } else lives_widget_set_sensitive(pbutton, TRUE);

  lives_slist_free_all(&slist);
}


void widget_add_preview(LiVESWidget *widget, LiVESBox *for_preview, LiVESBox *for_button, LiVESBox *for_deint, int preview_type) {
  LiVESWidget *preview_button = NULL;

  if (preview_type == LIVES_PREVIEW_TYPE_VIDEO_AUDIO || preview_type == LIVES_PREVIEW_TYPE_RANGE ||
      preview_type == LIVES_PREVIEW_TYPE_IMAGE_ONLY) {
    mainw->fs_playframe = lives_standard_frame_new(_("Preview"), 0.5, FALSE);
    mainw->fs_playalign = lives_alignment_new(0., 0., 1., 1.);
    mainw->fs_playarea = lives_event_box_new();

    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(mainw->fs_playarea), "pixbuf", NULL);

    lives_container_set_border_width(LIVES_CONTAINER(mainw->fs_playframe), widget_opts.border_width);

    lives_box_pack_start(for_preview, mainw->fs_playframe, FALSE, FALSE, 0);

    lives_widget_set_size_request(mainw->fs_playarea, DEFAULT_FRAME_HSIZE, DEFAULT_FRAME_VSIZE);

    lives_container_add(LIVES_CONTAINER(mainw->fs_playframe), mainw->fs_playalign);
    lives_container_add(LIVES_CONTAINER(mainw->fs_playalign), mainw->fs_playarea);

    lives_widget_set_bg_color(mainw->fs_playarea, LIVES_WIDGET_STATE_NORMAL, &palette->black);
    lives_widget_set_bg_color(mainw->fs_playframe, LIVES_WIDGET_STATE_NORMAL, &palette->black);
    lives_widget_set_bg_color(mainw->fs_playalign, LIVES_WIDGET_STATE_NORMAL, &palette->black);

  } else mainw->fs_playframe = mainw->fs_playalign = mainw->fs_playarea = NULL; // AUDIO_ONLY

  if (preview_type == LIVES_PREVIEW_TYPE_VIDEO_AUDIO) {
    preview_button = lives_standard_button_new_with_label(_("Click here to _Preview any selected video, image or audio file"));
  } else if (preview_type == LIVES_PREVIEW_TYPE_AUDIO_ONLY) {
    preview_button = lives_standard_button_new_with_label(_("Click here to _Preview any selected audio file"));
  } else if (preview_type == LIVES_PREVIEW_TYPE_RANGE) {
    preview_button = lives_standard_button_new_with_label(_("Click here to _Preview the video"));
  } else {
    preview_button = lives_standard_button_new_with_label(_("Click here to _Preview the file"));
  }

  lives_box_pack_start(for_button, preview_button, FALSE, FALSE, widget_opts.packing_width);

  if (preview_type == LIVES_PREVIEW_TYPE_VIDEO_AUDIO || preview_type == LIVES_PREVIEW_TYPE_RANGE) {
    add_deinterlace_checkbox(for_deint);
  }

  lives_signal_connect(LIVES_GUI_OBJECT(preview_button), LIVES_WIDGET_CLICKED_SIGNAL,
                       LIVES_GUI_CALLBACK(on_fs_preview_clicked),
                       LIVES_INT_TO_POINTER(preview_type));

  if (LIVES_IS_FILE_CHOOSER(widget)) {
    lives_widget_set_sensitive(preview_button, FALSE);

    lives_signal_connect(LIVES_GUI_OBJECT(widget), LIVES_WIDGET_SELECTION_CHANGED_SIGNAL,
                         LIVES_GUI_CALLBACK(pv_sel_changed),
                         (livespointer)preview_button);
  }
}


xprocess *create_processing(const char *text) {
  LiVESWidget *dialog_vbox;
  LiVESWidget *hbox;
  LiVESWidget *vbox2;
  LiVESWidget *vbox3;

  LiVESAccelGroup *accel_group = LIVES_ACCEL_GROUP(lives_accel_group_new());

  xprocess *procw = (xprocess *)(lives_malloc(sizeof(xprocess)));

  char tmp_label[256];

  boolean no_gui = widget_opts.no_gui;

  widget_opts.no_gui = TRUE; // work around bugs in gtk+
  widget_opts.non_modal = TRUE;
  procw->processing = lives_standard_dialog_new(_("Processing..."), FALSE, -1, -1);
  widget_opts.non_modal = FALSE;
  widget_opts.no_gui = no_gui;

  if (prefs->gui_monitor != 0) {
    lives_window_set_screen(LIVES_WINDOW(procw->processing), mainw->mgeom[prefs->gui_monitor - 1].screen);
  }

  lives_window_add_accel_group(LIVES_WINDOW(procw->processing), accel_group);

  if (prefs->show_gui) {
    lives_window_set_transient_for(LIVES_WINDOW(procw->processing), LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET));
  }

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(procw->processing));

  vbox2 = lives_vbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(dialog_vbox), vbox2, TRUE, TRUE, 0);

  vbox3 = lives_vbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox2), vbox3, TRUE, TRUE, 0);

  lives_snprintf(tmp_label, 256, "%s...\n", text);
  procw->label = lives_standard_label_new(tmp_label);

  lives_box_pack_start(LIVES_BOX(vbox3), procw->label, TRUE, TRUE, 0);

  procw->progressbar = lives_progress_bar_new();
  lives_box_pack_start(LIVES_BOX(vbox3), procw->progressbar, FALSE, FALSE, 0);
  if (palette->style & STYLE_1) {
    lives_widget_set_fg_color(procw->progressbar, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
  }

  widget_opts.justify = LIVES_JUSTIFY_CENTER;
  if (mainw->internal_messaging && mainw->rte != 0) {
    procw->label2 = lives_standard_label_new(_("\n\nPlease Wait\n\nRemember to switch off effects (ctrl-0) afterwards !"));
  } else procw->label2 = lives_standard_label_new(_("\nPlease Wait"));
  widget_opts.justify = LIVES_JUSTIFY_DEFAULT;

  lives_box_pack_start(LIVES_BOX(vbox3), procw->label2, FALSE, FALSE, 0);

  procw->label3 = lives_standard_label_new("");
  lives_box_pack_start(LIVES_BOX(vbox3), procw->label3, FALSE, FALSE, 0);

  hbox = lives_hbox_new(FALSE, 0);
  add_fill_to_box(LIVES_BOX(hbox));
  add_fill_to_box(LIVES_BOX(hbox));
  add_fill_to_box(LIVES_BOX(hbox));
  lives_box_pack_start(LIVES_BOX(vbox3), hbox, FALSE, FALSE, 0);

  if (mainw->iochan != NULL) {
    // add "show details" arrow
    boolean woat = widget_opts.apply_theme;
    widget_opts.apply_theme = FALSE;
    widget_opts.expand = LIVES_EXPAND_EXTRA;
    procw->scrolledwindow = lives_standard_scrolled_window_new(ENC_DETAILS_WIN_H, ENC_DETAILS_WIN_V, LIVES_WIDGET(mainw->optextview));
    widget_opts.expand = LIVES_EXPAND_DEFAULT;
    widget_opts.apply_theme = woat;
    lives_standard_expander_new(_("Show Details"), LIVES_BOX(vbox3), procw->scrolledwindow);
  }

  procw->stop_button = lives_standard_button_new_with_label(_("_Enough")); // used only for open location and for audio recording
  procw->preview_button = lives_standard_button_new_with_label(_("_Preview"));

  if (cfile->nokeep) procw->pause_button = lives_standard_button_new_with_label(_("Paus_e"));
  else procw->pause_button = lives_standard_button_new_with_label(_("Pause/_Enough"));

  lives_dialog_add_action_widget(LIVES_DIALOG(procw->processing), procw->preview_button, 1);
  lives_widget_hide(procw->preview_button);
  lives_widget_set_can_focus_and_default(procw->preview_button);

  lives_dialog_add_action_widget(LIVES_DIALOG(procw->processing), procw->pause_button, 0);
  lives_widget_hide(procw->pause_button);
  lives_widget_set_can_focus_and_default(procw->pause_button);

  if (mainw->current_file > -1) {
    if (cfile->opening_loc
#ifdef ENABLE_JACK
        || mainw->jackd_read != NULL
#endif
#ifdef HAVE_PULSE_AUDIO
        || mainw->pulsed_read != NULL
#endif
       ) {
      // the "enough" button for opening

      // show buttons
      lives_dialog_add_action_widget(LIVES_DIALOG(procw->processing), procw->stop_button, 0);
      lives_widget_set_can_focus_and_default(procw->stop_button);
    }
  }

  procw->cancel_button = lives_standard_button_new_with_label(_("_Cancel"));
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

  if (mainw->multitrack != NULL && mainw->multitrack->is_rendering) {
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

  if (mainw->show_procd) lives_widget_show_all(procw->processing);
  lives_widget_hide(procw->preview_button);
  lives_widget_hide(procw->pause_button);
  lives_widget_hide(procw->stop_button);

  return procw;
}


static LiVESWidget *vid_text_view_new(void) {
  LiVESWidget *textview;
  widget_opts.justify = LIVES_JUSTIFY_CENTER;
  textview = lives_standard_text_view_new(NULL, NULL);
  widget_opts.justify = widget_opts.default_justify;
  lives_widget_set_size_request(textview, TB_WIDTH, TB_HEIGHT_VID);
  if (palette->style & STYLE_3) {
    lives_widget_set_bg_color(textview, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
  }
  return textview;
}


static LiVESWidget *aud_text_view_new(void) {
  LiVESWidget *textview;
  widget_opts.justify = LIVES_JUSTIFY_CENTER;
  textview = lives_standard_text_view_new(NULL, NULL);
  widget_opts.justify = widget_opts.default_justify;
  lives_widget_set_size_request(textview, TB_WIDTH, TB_HEIGHT_AUD);
  if (palette->style & STYLE_3) {
    lives_widget_set_bg_color(textview, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
  }
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

  lives_clipinfo_t *filew = (lives_clipinfo_t *)(lives_malloc(sizeof(lives_clipinfo_t)));

  char *title;

  if (mainw->multitrack == NULL)
    title = lives_strdup(_(cfile->name));
  else
    title = lives_strdup(_("Multitrack Details"));

  filew->dialog = lives_standard_dialog_new(title, FALSE, -1, -1);
  lives_free(title);

  lives_signal_handlers_disconnect_by_func(filew->dialog, return_true, NULL);

  accel_group = LIVES_ACCEL_GROUP(lives_accel_group_new());
  lives_window_add_accel_group(LIVES_WINDOW(filew->dialog), accel_group);

  if (prefs->show_gui) {
    lives_window_set_transient_for(LIVES_WINDOW(filew->dialog), LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET));
  }

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(filew->dialog));

  if (cfile->frames > 0 || is_mt) {
    vidframe = lives_standard_frame_new(_("Video"), 0., FALSE);

    lives_box_pack_start(LIVES_BOX(dialog_vbox), vidframe, TRUE, TRUE, widget_opts.packing_height);

    table = lives_table_new(3, 4, TRUE);

    lives_table_set_column_homogeneous(LIVES_TABLE(table), FALSE);

    lives_table_set_col_spacings(LIVES_TABLE(table), widget_opts.packing_width * 4);
    lives_table_set_row_spacings(LIVES_TABLE(table), widget_opts.packing_height);
    lives_container_set_border_width(LIVES_CONTAINER(table), widget_opts.border_width);

    lives_container_add(LIVES_CONTAINER(vidframe), table);

    label = lives_standard_label_new(_("Format"));
    if (palette->style & STYLE_3) {
      lives_widget_set_bg_color(label, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
    }
    lives_label_set_hpadding(LIVES_LABEL(label), 4);
    lives_table_attach(LIVES_TABLE(table), label, 0, 1, 0, 1,
                       (LiVESAttachOptions)(LIVES_FILL | LIVES_EXPAND),
                       (LiVESAttachOptions)(0), 0, 0);

    label = lives_standard_label_new(_("Frame size"));
    if (palette->style & STYLE_3) {
      lives_widget_set_bg_color(label, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
    }
    lives_label_set_hpadding(LIVES_LABEL(label), 4);
    lives_table_attach(LIVES_TABLE(table), label, 0, 1, 1, 2,
                       (LiVESAttachOptions)(LIVES_FILL | LIVES_EXPAND),
                       (LiVESAttachOptions)(0), 0, 0);

    if (!is_mt) label = lives_standard_label_new(_("File size"));
    else label = lives_standard_label_new(_("Byte size"));
    if (palette->style & STYLE_3) {
      lives_widget_set_bg_color(label, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
    }
    lives_label_set_hpadding(LIVES_LABEL(label), 4);
    lives_table_attach(LIVES_TABLE(table), label, 0, 1, 2, 3,
                       (LiVESAttachOptions)(LIVES_FILL | LIVES_EXPAND),
                       (LiVESAttachOptions)(0), 0, 0);

    label = lives_standard_label_new(_("FPS"));
    if (palette->style & STYLE_3) {
      lives_widget_set_bg_color(label, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
    }
    lives_label_set_hpadding(LIVES_LABEL(label), 4);
    lives_table_attach(LIVES_TABLE(table), label, 2, 3, 0, 1,
                       (LiVESAttachOptions)(LIVES_FILL | LIVES_EXPAND),
                       (LiVESAttachOptions)(0), 0, 0);

    if (!is_mt) label = lives_standard_label_new(_("Frames"));
    else label = lives_standard_label_new(_("Events"));
    if (palette->style & STYLE_3) {
      lives_widget_set_bg_color(label, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
    }
    lives_label_set_hpadding(LIVES_LABEL(label), 4);
    lives_table_attach(LIVES_TABLE(table), label, 2, 3, 1, 2,
                       (LiVESAttachOptions)(LIVES_FILL | LIVES_EXPAND),
                       (LiVESAttachOptions)(0), 0, 0);

    label = lives_standard_label_new(_("Total time"));
    if (palette->style & STYLE_3) {
      lives_widget_set_bg_color(label, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
    }
    lives_label_set_hpadding(LIVES_LABEL(label), 4);
    lives_table_attach(LIVES_TABLE(table), label, 2, 3, 2, 3,
                       (LiVESAttachOptions)(LIVES_FILL | LIVES_EXPAND),
                       (LiVESAttachOptions)(0), 0, 0);

    filew->textview_type = vid_text_view_new();
    lives_table_attach(LIVES_TABLE(table), filew->textview_type, 1, 2, 0, 1,
                       (LiVESAttachOptions)(LIVES_FILL | LIVES_EXPAND),
                       (LiVESAttachOptions)(0), 0, 0);

    filew->textview_fps = vid_text_view_new();
    lives_table_attach(LIVES_TABLE(table), filew->textview_fps, 3, 4, 0, 1,
                       (LiVESAttachOptions)(LIVES_FILL | LIVES_EXPAND),
                       (LiVESAttachOptions)(0), 0, 0);

    filew->textview_size = vid_text_view_new();
    lives_table_attach(LIVES_TABLE(table), filew->textview_size, 1, 2, 1, 2,
                       (LiVESAttachOptions)(LIVES_FILL | LIVES_EXPAND),
                       (LiVESAttachOptions)(0), 0, 0);

    filew->textview_frames = vid_text_view_new();
    lives_table_attach(LIVES_TABLE(table), filew->textview_frames, 3, 4, 1, 2,
                       (LiVESAttachOptions)(LIVES_FILL | LIVES_EXPAND),
                       (LiVESAttachOptions)(0), 0, 0);

    filew->textview_vtime = vid_text_view_new();
    lives_table_attach(LIVES_TABLE(table), filew->textview_vtime, 3, 4, 2, 3,
                       (LiVESAttachOptions)(LIVES_FILL | LIVES_EXPAND),
                       (LiVESAttachOptions)(0), 0, 0);

    filew->textview_fsize = vid_text_view_new();
    lives_table_attach(LIVES_TABLE(table), filew->textview_fsize, 1, 2, 2, 3,
                       (LiVESAttachOptions)(LIVES_FILL | LIVES_EXPAND),
                       (LiVESAttachOptions)(0), 0, 0);

  }

  if (audio_channels > 0) {
    char *tmp;

    if (audio_channels > 1) tmp = lives_strdup(_("Left Audio"));
    else tmp = lives_strdup(_("Audio"));

    laudframe = lives_standard_frame_new(tmp, 0., FALSE);
    lives_free(tmp);

    lives_box_pack_start(LIVES_BOX(dialog_vbox), laudframe, TRUE, TRUE, widget_opts.packing_height);

    table = lives_table_new(1, 4, TRUE);

    lives_table_set_col_spacings(LIVES_TABLE(table), widget_opts.packing_width * 4);
    lives_table_set_row_spacings(LIVES_TABLE(table), widget_opts.packing_height);
    lives_container_set_border_width(LIVES_CONTAINER(table), widget_opts.border_width);

    lives_container_add(LIVES_CONTAINER(laudframe), table);

    if (!is_mt) {
      label = lives_standard_label_new(_("Total time"));
      if (palette->style & STYLE_3) {
        lives_widget_set_bg_color(label, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
      }
      lives_table_attach(LIVES_TABLE(table), label, 0, 1, 0, 1,
                         (LiVESAttachOptions)(0),
                         (LiVESAttachOptions)(0), 0, 0);

      filew->textview_ltime = aud_text_view_new();
      lives_table_attach(LIVES_TABLE(table), filew->textview_ltime, 1, 2, 0, 1,
                         (LiVESAttachOptions)(0),
                         (LiVESAttachOptions)(0), 0, 0);
    }

    label = lives_standard_label_new(_("Rate/size"));
    if (palette->style & STYLE_3) {
      lives_widget_set_bg_color(label, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
    }
    lives_label_set_hpadding(LIVES_LABEL(label), 4);
    lives_table_attach(LIVES_TABLE(table), label, 2, 3, 0, 1,
                       (LiVESAttachOptions)(0),
                       (LiVESAttachOptions)(0), 0, 0);

    filew->textview_lrate = aud_text_view_new();
    lives_table_attach(LIVES_TABLE(table), filew->textview_lrate, 3, 4, 0, 1,
                       (LiVESAttachOptions)(0),
                       (LiVESAttachOptions)(0), 0, 0);

    if (audio_channels > 1) {
      raudframe = lives_standard_frame_new(_("Right Audio"), 0., FALSE);

      lives_box_pack_start(LIVES_BOX(dialog_vbox), raudframe, TRUE, TRUE, widget_opts.packing_height);

      table = lives_table_new(1, 4, TRUE);

      lives_table_set_col_spacings(LIVES_TABLE(table), widget_opts.packing_width * 4);
      lives_table_set_row_spacings(LIVES_TABLE(table), widget_opts.packing_height);
      lives_container_set_border_width(LIVES_CONTAINER(table), widget_opts.border_width);

      lives_container_add(LIVES_CONTAINER(raudframe), table);

      if (!is_mt) {
        label = lives_standard_label_new(_("Total time"));
        if (palette->style & STYLE_3) {
          lives_widget_set_bg_color(label, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
        }
        lives_label_set_hpadding(LIVES_LABEL(label), 4);
        lives_table_attach(LIVES_TABLE(table), label, 0, 1, 0, 1,
                           (LiVESAttachOptions)(0),
                           (LiVESAttachOptions)(0), 0, 0);

        filew->textview_rtime = aud_text_view_new();
        lives_table_attach(LIVES_TABLE(table), filew->textview_rtime, 1, 2, 0, 1,
                           (LiVESAttachOptions)(0),
                           (LiVESAttachOptions)(0), 0, 0);
      }

      label = lives_standard_label_new(_("Rate/size"));
      if (palette->style & STYLE_3) {
        lives_widget_set_bg_color(label, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
      }
      lives_label_set_hpadding(LIVES_LABEL(label), 4);
      lives_table_attach(LIVES_TABLE(table), label, 2, 3, 0, 1,
                         (LiVESAttachOptions)(0),
                         (LiVESAttachOptions)(0), 0, 0);

      filew->textview_rrate = aud_text_view_new();
      lives_table_attach(LIVES_TABLE(table), filew->textview_rrate, 3, 4, 0, 1,
                         (LiVESAttachOptions)(0),
                         (LiVESAttachOptions)(0), 0, 0);

    }
  }

  dialog_action_area = lives_dialog_get_action_area(LIVES_DIALOG(filew->dialog));

  okbutton = lives_standard_button_new_from_stock(LIVES_STOCK_OK, NULL);
  lives_dialog_add_action_widget(LIVES_DIALOG(filew->dialog), okbutton, LIVES_RESPONSE_OK);

  lives_widget_set_can_focus_and_default(okbutton);
  lives_widget_grab_default_special(okbutton);

  if (LIVES_IS_BUTTON_BOX(dialog_action_area)) lives_button_box_set_layout(LIVES_BUTTON_BOX(dialog_action_area), LIVES_BUTTONBOX_SPREAD);

  lives_signal_connect(LIVES_GUI_OBJECT(okbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                       LIVES_GUI_CALLBACK(lives_general_button_clicked),
                       filew);

  accel_group = LIVES_ACCEL_GROUP(lives_accel_group_new());
  lives_window_add_accel_group(LIVES_WINDOW(filew->dialog), accel_group);

  lives_widget_add_accelerator(okbutton, LIVES_WIDGET_CLICKED_SIGNAL, accel_group,
                               LIVES_KEY_Escape, (LiVESXModifierType)0, (LiVESAccelFlags)0);

  lives_widget_show_all(filew->dialog);

  lives_widget_set_size_request(okbutton, DEF_BUTTON_WIDTH * 4, -1);

  lives_widget_context_update();

  return filew;
}


static void on_resizecb_toggled(LiVESToggleButton *t, livespointer user_data) {
  LiVESWidget *cb = (LiVESWidget *)user_data;

  if (!lives_toggle_button_get_active(t)) {
    lives_widget_set_sensitive(cb, FALSE);
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(cb), FALSE);
  } else {
    lives_widget_set_sensitive(cb, TRUE);
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(cb), prefs->enc_letterbox);
  }
}


LiVESWidget *create_encoder_prep_dialog(const char *text1, const char *text2, boolean opt_resize) {
  LiVESWidget *dialog;
  LiVESWidget *dialog_vbox;
  LiVESWidget *cancelbutton;
  LiVESWidget *okbutton;
  LiVESWidget *checkbutton = NULL;
  LiVESWidget *checkbutton2;
  LiVESWidget *label;
  LiVESWidget *hbox;

  char *labeltext, *tmp, *tmp2;

  dialog = create_question_dialog(_("Encoding Options"), text1, LIVES_WINDOW(mainw->LiVES));
  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(dialog));

  if (opt_resize) {
    if (text2 != NULL) labeltext = lives_strdup(_("<------------- (Check the box to re_size as suggested)"));
    else labeltext = lives_strdup(_("<------------- (Check the box to use the _size recommendation)"));

    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, FALSE, FALSE, widget_opts.packing_width);

    checkbutton = lives_standard_check_button_new(labeltext, FALSE, LIVES_BOX(hbox), NULL);

    lives_free(labeltext);

    lives_signal_connect_after(LIVES_GUI_OBJECT(checkbutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                               LIVES_GUI_CALLBACK(on_boolean_toggled),
                               &mainw->fx1_bool);

  } else if (text2 == NULL) mainw->fx1_bool = TRUE;

  if (text2 != NULL && (mainw->fx1_bool || opt_resize)) {
    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

    checkbutton2 = lives_standard_check_button_new
                   ((tmp = lives_strdup(_("Use _letterboxing to maintain aspect ratio (optional)"))), FALSE, LIVES_BOX(hbox),
                    (tmp2 = lives_strdup(_("Draw black rectangles either above or to the sides of the image, to prevent it from stretching."))));

    lives_free(tmp);
    lives_free(tmp2);

    if (opt_resize) {
      lives_widget_set_sensitive(checkbutton2, FALSE);
    } else lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(checkbutton2), prefs->enc_letterbox);

    lives_signal_connect_after(LIVES_GUI_OBJECT(checkbutton2), LIVES_WIDGET_TOGGLED_SIGNAL,
                               LIVES_GUI_CALLBACK(on_boolean_toggled),
                               &prefs->enc_letterbox);

    if (opt_resize)
      lives_signal_connect_after(LIVES_GUI_OBJECT(checkbutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                                 LIVES_GUI_CALLBACK(on_resizecb_toggled),
                                 checkbutton2);

  }

  if (text2 != NULL) {
    label = lives_standard_label_new(text2);
    lives_box_pack_start(LIVES_BOX(dialog_vbox), label, TRUE, TRUE, 0);
    cancelbutton = lives_standard_button_new_from_stock(LIVES_STOCK_CANCEL, NULL);
    okbutton = lives_standard_button_new_from_stock(LIVES_STOCK_OK, NULL);
  } else {
    cancelbutton = lives_standard_button_new_with_label(_("Keep _my settings"));
    okbutton = lives_standard_button_new_with_label(_("Use _recommended settings"));
  }

  lives_dialog_add_action_widget(LIVES_DIALOG(dialog), cancelbutton, LIVES_RESPONSE_CANCEL);
  lives_widget_set_can_focus_and_default(cancelbutton);

  lives_dialog_add_action_widget(LIVES_DIALOG(dialog), okbutton, LIVES_RESPONSE_OK);

  lives_widget_set_can_focus_and_default(okbutton);
  lives_widget_grab_default_special(okbutton);

  lives_widget_show_all(dialog);
  return dialog;
}

// Information/error dialog

// the type of message box here is with a single OK button
// if 2 or more buttons (e.g. OK/CANCEL, YES/NO, ABORT/RETRY/CANCEL) are needed, use create_message_dialog() in dialogs.c

LiVESWidget *create_info_error_dialog(lives_dialog_t info_type, const char *text, LiVESWindow *transient, int mask, boolean is_blocking) {
  LiVESWidget *dialog;

  if (!prefs->show_gui) {
    transient = NULL;
  } else {
    transient = LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET);
  }

  dialog = create_message_dialog(info_type, text, transient, mask, is_blocking);
  return dialog;
}


text_window *create_text_window(const char *title, const char *text, LiVESTextBuffer *textbuffer) {
  // general text window
  LiVESWidget *dialog_vbox;
  LiVESWidget *scrolledwindow;
  LiVESWidget *okbutton;

  boolean woat;

  textwindow = (text_window *)lives_malloc(sizeof(text_window));

  textwindow->dialog = lives_standard_dialog_new(title, FALSE, DEF_DIALOG_WIDTH, DEF_DIALOG_HEIGHT);

  if (prefs->show_gui) {
    lives_window_set_transient_for(LIVES_WINDOW(textwindow->dialog), LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET));
  }

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(textwindow->dialog));

  textwindow->textview = textwindow->table = NULL;

  if (textbuffer != NULL || text != NULL) textwindow->textview = lives_standard_text_view_new(text, textbuffer);

  woat = widget_opts.apply_theme;
  widget_opts.apply_theme = FALSE;

  if (textwindow->textview != NULL) {
    scrolledwindow = lives_standard_scrolled_window_new(RFX_WINSIZE_H, RFX_WINSIZE_V, textwindow->textview);
    if (palette->style & STYLE_1) {
      lives_widget_set_bg_color(lives_bin_get_child(LIVES_BIN(scrolledwindow)), LIVES_WIDGET_STATE_NORMAL, &palette->info_base);
    }

  } else {
    textwindow->table = lives_table_new(1, 1, FALSE);
    scrolledwindow = lives_standard_scrolled_window_new(RFX_WINSIZE_H, RFX_WINSIZE_V, textwindow->table);
    if (palette->style & STYLE_1) {
      lives_widget_set_bg_color(textwindow->table, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
      lives_widget_set_fg_color(textwindow->table, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
      lives_widget_set_bg_color(lives_bin_get_child(LIVES_BIN(scrolledwindow)), LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
    }

  }

  widget_opts.apply_theme = woat;

  lives_box_pack_start(LIVES_BOX(dialog_vbox), scrolledwindow, TRUE, TRUE, 0);

  if (text != NULL || mainw->iochan != NULL || textwindow->table != NULL) {
    LiVESWidget *savebutton;

    okbutton = lives_standard_button_new_from_stock(LIVES_STOCK_CLOSE, _("_Close Window"));

    savebutton = lives_standard_button_new_from_stock(LIVES_STOCK_SAVE, _("_Save to file"));

    if (textwindow->table == NULL) {
      lives_dialog_add_action_widget(LIVES_DIALOG(textwindow->dialog), savebutton, LIVES_RESPONSE_YES);
      lives_signal_connect(LIVES_GUI_OBJECT(savebutton), LIVES_WIDGET_CLICKED_SIGNAL,
                           LIVES_GUI_CALLBACK(on_save_textview_clicked),
                           textwindow->textview);
    }

    lives_dialog_add_action_widget(LIVES_DIALOG(textwindow->dialog), okbutton, LIVES_RESPONSE_OK);

    lives_signal_connect(LIVES_GUI_OBJECT(okbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                         LIVES_GUI_CALLBACK(lives_general_button_clicked),
                         textwindow);

  }

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
  LiVESWidget *cancelbutton;
  LiVESWidget *okbutton;

  LiVESSList *radiobutton1_group = NULL;
  LiVESSList *radiobutton2_group = NULL;

  LiVESAccelGroup *accel_group = LIVES_ACCEL_GROUP(lives_accel_group_new());

  char *tmp, *tmp2;

  _insertw *insertw = (_insertw *)(lives_malloc(sizeof(_insertw)));

  insertw->insert_dialog = lives_standard_dialog_new(_("Insert"), FALSE, -1, -1);
  lives_signal_handlers_disconnect_by_func(insertw->insert_dialog, return_true, NULL);

  lives_window_add_accel_group(LIVES_WINDOW(insertw->insert_dialog), accel_group);

  if (prefs->show_gui) {
    lives_window_set_transient_for(LIVES_WINDOW(insertw->insert_dialog), LIVES_WINDOW(mainw->LiVES));
  }

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(insertw->insert_dialog));

  hbox1 = lives_hbox_new(FALSE, 0);

  lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox1, TRUE, TRUE, widget_opts.packing_height);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(hbox1), hbox, FALSE, FALSE, widget_opts.packing_width);

  insertw->spinbutton_times = lives_standard_spin_button_new(_("_Number of times to insert"),
                              1., 1., 10000., 1., 10., 0., LIVES_BOX(hbox), NULL);

  lives_widget_grab_focus(insertw->spinbutton_times);

  add_fill_to_box(LIVES_BOX(hbox1));

  hbox = lives_hbox_new(FALSE, 0);

  lives_box_pack_start(LIVES_BOX(hbox1), hbox, FALSE, FALSE, widget_opts.packing_width);

  insertw->fit_checkbutton = lives_standard_check_button_new(_("_Insert to fit audio"), FALSE, LIVES_BOX(hbox), NULL);

  lives_widget_set_sensitive(LIVES_WIDGET(insertw->fit_checkbutton), cfile->achans > 0 && clipboard->achans == 0);

  add_hsep_to_box(LIVES_BOX(dialog_vbox));

  table = lives_table_new(2, 3, FALSE);
  lives_box_pack_start(LIVES_BOX(dialog_vbox), table, TRUE, TRUE, widget_opts.packing_height);
  lives_table_set_col_spacings(LIVES_TABLE(table), widget_opts.packing_width * 4);
  lives_table_set_row_spacings(LIVES_TABLE(table), widget_opts.packing_height * 2);

  hbox = lives_hbox_new(FALSE, 0);

  radiobutton = lives_standard_radio_button_new((tmp = lives_strdup(_("Insert _before selection"))),
                &radiobutton1_group, LIVES_BOX(hbox),
                (tmp2 = lives_strdup(_("Insert clipboard before selected frames"))));

  lives_free(tmp);
  lives_free(tmp2);

  lives_table_attach(LIVES_TABLE(table), hbox, 0, 1, 0, 1,
                     (LiVESAttachOptions)(LIVES_FILL),
                     (LiVESAttachOptions)(0), 0, 0);

  if (cfile->frames == 0) {
    lives_widget_set_sensitive(radiobutton, FALSE);
  }

  hbox = lives_hbox_new(FALSE, 0);

  radiobutton = lives_standard_radio_button_new((tmp = lives_strdup(_("Insert _after selection"))),
                &radiobutton1_group, LIVES_BOX(hbox),
                (tmp2 = lives_strdup(_("Insert clipboard after selected frames"))));

  lives_table_attach(LIVES_TABLE(table), hbox, 0, 1, 1, 2,
                     (LiVESAttachOptions)(LIVES_FILL),
                     (LiVESAttachOptions)(0), 0, 0);

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(radiobutton), mainw->insert_after);

  hbox = lives_hbox_new(FALSE, 0);

  if (clipboard->achans == 0)
    insertw->with_sound = lives_standard_radio_button_new(_("Insert _with silence"),
                          &radiobutton2_group, LIVES_BOX(hbox), NULL);
  else
    insertw->with_sound = lives_standard_radio_button_new(_("Insert _with sound"),
                          &radiobutton2_group, LIVES_BOX(hbox), NULL);

  lives_table_attach(LIVES_TABLE(table), hbox, 2, 3, 0, 1,
                     (LiVESAttachOptions)(LIVES_FILL),
                     (LiVESAttachOptions)(0), 0, 0);

  if (cfile->achans == 0 && clipboard->achans == 0) lives_widget_set_sensitive(insertw->with_sound, FALSE);

  hbox = lives_hbox_new(FALSE, 0);

  insertw->without_sound = lives_standard_radio_button_new(_("Insert with_out sound"),
                           &radiobutton2_group, LIVES_BOX(hbox), NULL);

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(insertw->without_sound),
                                 !((cfile->achans > 0 || clipboard->achans > 0) && mainw->ccpd_with_sound));

  lives_table_attach(LIVES_TABLE(table), hbox, 2, 3, 1, 2,
                     (LiVESAttachOptions)(LIVES_FILL),
                     (LiVESAttachOptions)(0), 0, 0);

  lives_widget_set_sensitive(insertw->with_sound, clipboard->achans > 0 || cfile->achans > 0);
  lives_widget_set_sensitive(insertw->without_sound, clipboard->achans > 0 || cfile->achans > 0);

  vseparator = lives_vseparator_new();
  lives_table_attach(LIVES_TABLE(table), vseparator, 1, 2, 0, 1,
                     (LiVESAttachOptions)(LIVES_EXPAND | LIVES_FILL),
                     (LiVESAttachOptions)(LIVES_FILL), 0, 0);

  vseparator = lives_vseparator_new();
  lives_table_attach(LIVES_TABLE(table), vseparator, 1, 2, 1, 2,
                     (LiVESAttachOptions)(LIVES_FILL),
                     (LiVESAttachOptions)(LIVES_FILL), 0, 0);

  add_fill_to_box(LIVES_BOX(dialog_vbox));

  cancelbutton = lives_standard_button_new_from_stock(LIVES_STOCK_CANCEL, NULL);
  lives_dialog_add_action_widget(LIVES_DIALOG(insertw->insert_dialog), cancelbutton, LIVES_RESPONSE_CANCEL);

  okbutton = lives_standard_button_new_from_stock(LIVES_STOCK_OK, NULL);
  lives_dialog_add_action_widget(LIVES_DIALOG(insertw->insert_dialog), okbutton, LIVES_RESPONSE_OK);
  lives_widget_set_can_focus_and_default(okbutton);
  lives_widget_grab_default_special(okbutton);

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
  LiVESWidget *cancelbutton;
  LiVESWidget *okbutton;

  boolean no_gui = widget_opts.no_gui;

  widget_opts.no_gui = TRUE; // work around bugs in gtk+
  opensel_dialog = lives_standard_dialog_new(_("Open Selection"), FALSE, -1, -1);
  widget_opts.no_gui = no_gui;

  if (prefs->show_gui) {
    lives_window_set_transient_for(LIVES_WINDOW(opensel_dialog), LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET));
  }

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(opensel_dialog));

  vbox = lives_vbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(dialog_vbox), vbox, TRUE, TRUE, 0);

  table = lives_table_new(2, 2, FALSE);
  lives_box_pack_start(LIVES_BOX(vbox), table, TRUE, TRUE, widget_opts.packing_height);

  lives_table_set_row_spacings(LIVES_TABLE(table), widget_opts.packing_height * 2);

  label = lives_standard_label_new(_("Selection start time (sec)"));
  lives_table_attach(LIVES_TABLE(table), label, 0, 1, 0, 1,
                     (LiVESAttachOptions)(LIVES_EXPAND | LIVES_FILL),
                     (LiVESAttachOptions)(0), 0, 0);
  lives_label_set_halignment(LIVES_LABEL(label), 0.);

  label = lives_standard_label_new(_("Number of frames to open"));
  lives_table_attach(LIVES_TABLE(table), label, 0, 1, 1, 2,
                     (LiVESAttachOptions)(LIVES_EXPAND | LIVES_FILL),
                     (LiVESAttachOptions)(0), 0, 0);
  lives_label_set_halignment(LIVES_LABEL(label), 0.);

  spinbutton = lives_standard_spin_button_new(NULL, 0., 0., 1000000000., 1., 10., 2, NULL, NULL);

  lives_signal_connect_after(LIVES_GUI_OBJECT(spinbutton), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                             LIVES_GUI_CALLBACK(on_spin_value_changed),
                             LIVES_INT_TO_POINTER(1));

  lives_table_attach(LIVES_TABLE(table), spinbutton, 1, 2, 0, 1,
                     (LiVESAttachOptions)(LIVES_EXPAND | LIVES_FILL),
                     (LiVESAttachOptions)(LIVES_EXPAND), widget_opts.packing_height * 4 + 2, 0);

  spinbutton = lives_standard_spin_button_new(NULL, 1000., 1., (double)LIVES_MAXINT, 1., 10., 0, NULL, NULL);

  lives_signal_connect_after(LIVES_GUI_OBJECT(spinbutton), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                             LIVES_GUI_CALLBACK(on_spin_value_changed),
                             LIVES_INT_TO_POINTER(2));

  lives_table_attach(LIVES_TABLE(table), spinbutton, 1, 2, 1, 2,
                     (LiVESAttachOptions)(LIVES_EXPAND | LIVES_FILL),
                     (LiVESAttachOptions)(LIVES_EXPAND), widget_opts.packing_height * 4 + 2, 0);

  cancelbutton = lives_standard_button_new_from_stock(LIVES_STOCK_CANCEL, NULL);
  lives_dialog_add_action_widget(LIVES_DIALOG(opensel_dialog), cancelbutton, LIVES_RESPONSE_CANCEL);

  okbutton = lives_standard_button_new_from_stock(LIVES_STOCK_OK, NULL);
  lives_dialog_add_action_widget(LIVES_DIALOG(opensel_dialog), okbutton, LIVES_RESPONSE_OK);
  lives_widget_set_can_focus_and_default(okbutton);
  lives_widget_grab_default_special(okbutton);

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
  LiVESWidget *cancelbutton;
  LiVESWidget *okbutton;
  LiVESWidget *label;
  LiVESWidget *checkbutton;
  LiVESWidget *hbox;
  LiVESWidget *buttond;

  _entryw *locw = (_entryw *)(lives_malloc(sizeof(_entryw)));

  LiVESAccelGroup *accel_group = LIVES_ACCEL_GROUP(lives_accel_group_new());

  char *title, *tmp, *tmp2;

  if (type == 1)
    title = lives_strdup(_("Open Location"));
  else
    title = lives_strdup(_("Open Youtube Clip"));

  locw->dialog = lives_standard_dialog_new(title, FALSE, -1, -1);
  lives_signal_handlers_disconnect_by_func(locw->dialog, return_true, NULL);

  lives_free(title);

  lives_window_add_accel_group(LIVES_WINDOW(locw->dialog), accel_group);

  if (prefs->show_gui) {
    lives_window_set_transient_for(LIVES_WINDOW(locw->dialog), LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET));
  }

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(locw->dialog));

  widget_opts.justify = LIVES_JUSTIFY_CENTER;

  if (type == 1) {
    label = lives_standard_label_new(
              _("\n\nTo open a stream, you must make sure that you have the correct libraries compiled in mplayer (or mpv).\n"
                "Also make sure you have set your bandwidth in Preferences|Streaming\n\n"));
  } else {
    label = lives_standard_label_new(
              _("\n\nTo open a clip from Youtube, LiVES will first download it with youtube-dl.\n"
                "Please make sure you have the latest version of that tool installed.\n\n"));

    lives_box_pack_start(LIVES_BOX(dialog_vbox), label, FALSE, FALSE, 0);

    label = lives_standard_label_new(_("Enter the URL of the clip below.\nE.g: http://www.youtube.com/watch?v=WCR6f6WzjP8\n\n"));

  }

  widget_opts.justify = LIVES_JUSTIFY_DEFAULT;

  lives_box_pack_start(LIVES_BOX(dialog_vbox), label, FALSE, FALSE, 0);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, TRUE, TRUE, widget_opts.packing_height * 2);

  locw->entry = lives_standard_entry_new(type == 1 ? _("URL : ") : _("Youtube URL : "), "", STD_ENTRY_WIDTH, 32768, LIVES_BOX(hbox),
                                         NULL);

  add_fill_to_box(LIVES_BOX(hbox));

  if (type == 1) {
    hbox = lives_hbox_new(FALSE, 0);
    checkbutton = lives_standard_check_button_new((tmp = lives_strdup(_("Do not send bandwidth information"))),
                  prefs->no_bandwidth, LIVES_BOX(hbox),
                  (tmp2 = lives_strdup(_("Try this setting if you are having problems getting a stream"))));

    lives_free(tmp);
    lives_free(tmp2);

    lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, FALSE, FALSE, widget_opts.packing_height * 2);

    lives_signal_connect(LIVES_GUI_OBJECT(checkbutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                         LIVES_GUI_CALLBACK(on_boolean_toggled),
                         &prefs->no_bandwidth);

    add_deinterlace_checkbox(LIVES_BOX(dialog_vbox));
  }

  if (type == 2) {
    hbox = lives_hbox_new(FALSE, 0);

    lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, TRUE, FALSE, widget_opts.packing_height * 2);

    locw->dir_entry = lives_standard_entry_new(_("Download to _Directory : "), mainw->vid_dl_dir,
                      STD_ENTRY_WIDTH, PATH_MAX, LIVES_BOX(hbox), NULL);

    lives_entry_set_editable(LIVES_ENTRY(locw->dir_entry), FALSE);

    // add dir, with filechooser button
    buttond = lives_standard_file_button_new(TRUE, NULL);
    lives_label_set_mnemonic_widget(LIVES_LABEL(widget_opts.last_label), buttond);
    lives_box_pack_start(LIVES_BOX(hbox), buttond, FALSE, FALSE, widget_opts.packing_width / 2);

    add_fill_to_box(LIVES_BOX(hbox));

    hbox = lives_hbox_new(FALSE, 0);

    lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, TRUE, FALSE, widget_opts.packing_height * 2);

    locw->name_entry = lives_standard_entry_new(_("Download _File Name : "), "",
                       -1, PATH_MAX, LIVES_BOX(hbox), NULL);

    lives_signal_connect(buttond, LIVES_WIDGET_CLICKED_SIGNAL, LIVES_GUI_CALLBACK(on_filesel_button_clicked), (livespointer)locw->dir_entry);

    label = lives_standard_label_new("." LIVES_FILE_EXT_MP4);

    lives_box_pack_start(LIVES_BOX(hbox), label, FALSE, FALSE, 0);

    add_fill_to_box(LIVES_BOX(hbox));

    hbox = lives_hbox_new(FALSE, 0);

    lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, TRUE, FALSE, widget_opts.packing_height * 2);
  }

  cancelbutton = lives_standard_button_new_from_stock(LIVES_STOCK_CANCEL, NULL);
  lives_dialog_add_action_widget(LIVES_DIALOG(locw->dialog), cancelbutton, LIVES_RESPONSE_CANCEL);
  lives_widget_set_can_focus_and_default(cancelbutton);

  okbutton = lives_standard_button_new_from_stock(LIVES_STOCK_OK, NULL);
  lives_dialog_add_action_widget(LIVES_DIALOG(locw->dialog), okbutton, LIVES_RESPONSE_OK);
  lives_widget_set_can_focus_and_default(okbutton);
  lives_widget_grab_default_special(okbutton);

  lives_signal_connect(LIVES_GUI_OBJECT(cancelbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                       LIVES_GUI_CALLBACK(lives_general_button_clicked),
                       locw);

  if (type == 1)
    lives_signal_connect(LIVES_GUI_OBJECT(okbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                         LIVES_GUI_CALLBACK(on_location_select),
                         NULL);

  else if (type == 2)
    lives_signal_connect(LIVES_GUI_OBJECT(okbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                         LIVES_GUI_CALLBACK(on_utube_select),
                         NULL);

  lives_widget_add_accelerator(cancelbutton, LIVES_WIDGET_CLICKED_SIGNAL, accel_group,
                               LIVES_KEY_Escape, (LiVESXModifierType)0, (LiVESAccelFlags)0);

  lives_widget_show_all(locw->dialog);

  return locw;
}


_entryw *create_rename_dialog(int type) {
  // type 1 = rename clip in menu
  // type 2 = save clip set
  // type 3 = reload clip set
  // type 4 = save clip set from mt
  // type 5 = save clip set for project export

  // type 6 = initial workdir

  // type 7 = rename track in mt

  // type 8 = export theme

  LiVESWidget *dialog_vbox;
  LiVESWidget *hbox;
  LiVESWidget *label;
  LiVESWidget *cancelbutton;
  LiVESWidget *okbutton;
  LiVESWidget *checkbutton;
  LiVESWidget *set_combo;
  LiVESWidget *dirbutton1;
  LiVESWidget *dirimage1;

  LiVESAccelGroup *accel_group = LIVES_ACCEL_GROUP(lives_accel_group_new());

  char *title = NULL;

  _entryw *renamew = (_entryw *)(lives_malloc(sizeof(_entryw)));

  renamew->setlist = NULL;

  if (type == 1) {
    title = lives_strdup(_("Rename Clip"));
  } else if (type == 2 || type == 4 || type == 5) {
    title = lives_strdup(_("Enter Set Name to Save as"));
  } else if (type == 3) {
    title = lives_strdup(_("Enter a Set Name to Reload"));
  } else if (type == 6) {
    title = lives_strdup(_("Choose a Working Directory"));
  } else if (type == 7) {
    title = lives_strdup(_("Rename Current Track"));
  } else if (type == 8) {
    title = lives_strdup(_("Enter a Name for Your Theme"));
  }

  renamew->dialog = lives_standard_dialog_new(title, FALSE, -1, -1);
  lives_free(title);
  lives_signal_handlers_disconnect_by_func(renamew->dialog, return_true, NULL);

  lives_window_add_accel_group(LIVES_WINDOW(renamew->dialog), accel_group);

  if (prefs->show_gui) {
    if (mainw->is_ready) {
      lives_window_set_transient_for(LIVES_WINDOW(renamew->dialog), LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET));
    }
  }

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(renamew->dialog));

  if (type == 4) {
    label = lives_standard_label_new
            (_("You need to enter a name for the current clip set.\nThis will allow you reload the layout with the same clips later.\n"
               "Please enter the set name you wish to use.\nLiVES will remind you to save the clip set later when you try to exit.\n"));
    lives_box_pack_start(LIVES_BOX(dialog_vbox), label, FALSE, FALSE, 0);
  }

  if (type == 5) {
    label = lives_standard_label_new
            (_("In order to export this project, you must enter a name for this clip set.\nThis will also be used for the project name.\n"));
    lives_box_pack_start(LIVES_BOX(dialog_vbox), label, FALSE, FALSE, 0);
  }

  if (type == 6) {
    label = lives_standard_label_new
            (_("Welcome to LiVES !\nThis startup wizard will guide you through the\ninitial install so that you can get the most from this application.\n"));
    lives_box_pack_start(LIVES_BOX(dialog_vbox), label, FALSE, FALSE, 0);

    label = lives_standard_label_new
            (_("\nFirst of all you need to choose a working directory for LiVES.\nThis should be a directory with plenty of disk space available.\n"));
    lives_box_pack_start(LIVES_BOX(dialog_vbox), label, FALSE, FALSE, 0);
  }

  hbox = lives_hbox_new(FALSE, 0);

  if (type == 3) {
    lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, TRUE, FALSE, widget_opts.packing_height * 4);
  } else if (type == 2 || type == 4 || type == 5) {
    lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, TRUE, TRUE, widget_opts.packing_height * 2);
  } else {
    lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, TRUE, TRUE, widget_opts.packing_height * 4);
  }

  if (type == 1 || type == 7) {
    label = lives_standard_label_new(_("New name "));
  } else if (type == 2 || type == 3 || type == 4 || type == 5) {
    label = lives_standard_label_new(_("Set name "));
  } else if (type == 8) {
    label = lives_standard_label_new(_("Theme name "));
  } else {
    label = lives_standard_label_new("");
  }

  lives_box_pack_start(LIVES_BOX(hbox), label, FALSE, FALSE, widget_opts.packing_width * 4);

  if (type == 3) {
    set_combo = lives_combo_new();

    renamew->setlist = get_set_list(prefs->workdir, TRUE);

    lives_combo_populate(LIVES_COMBO(set_combo), renamew->setlist);

    renamew->entry = lives_combo_get_entry(LIVES_COMBO(set_combo));

    if (strlen(prefs->ar_clipset_name)) {
      // set default to our auto-reload clipset
      lives_entry_set_text(LIVES_ENTRY(renamew->entry), prefs->ar_clipset_name);
    }

    lives_box_pack_start(LIVES_BOX(hbox), set_combo, TRUE, TRUE, 0);

    lives_entry_set_completion_from_list(LIVES_ENTRY(renamew->entry), renamew->setlist);
  } else {
    char *tmp;
    renamew->entry = lives_entry_new();
    lives_entry_set_max_length(LIVES_ENTRY(renamew->entry), type == 6 ? PATH_MAX : type == 7 ? 16 : 128);
    if (type == 2 && strlen(mainw->set_name)) {
      lives_entry_set_text(LIVES_ENTRY(renamew->entry), (tmp = F2U8(mainw->set_name)));
      lives_free(tmp);
    }
    if (type == 6) {
      char *workdir;
      if (prefs->startup_phase == -1) workdir = lives_build_filename(capable->home_dir, LIVES_WORK_NAME, NULL);
      else workdir = lives_strdup(prefs->workdir);
      lives_entry_set_text(LIVES_ENTRY(renamew->entry), (tmp = F2U8(workdir)));
      lives_free(tmp);
      lives_free(workdir);
    }
    lives_box_pack_start(LIVES_BOX(hbox), renamew->entry, TRUE, TRUE, 0);
  }

  if (type == 6) {
    dirbutton1 = lives_standard_button_new();

    dirimage1 = lives_image_new_from_stock(LIVES_STOCK_OPEN, LIVES_ICON_SIZE_BUTTON);

    lives_container_add(LIVES_CONTAINER(dirbutton1), dirimage1);

    lives_box_pack_start(LIVES_BOX(hbox), dirbutton1, FALSE, TRUE, widget_opts.packing_width);
    lives_signal_connect(dirbutton1, LIVES_WIDGET_CLICKED_SIGNAL, LIVES_GUI_CALLBACK(on_filesel_complex_clicked), renamew->entry);
  }

  if (type == 8) {
    mainw->fx1_bool = FALSE;
    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, TRUE, TRUE, widget_opts.packing_height * 4);

    checkbutton = lives_standard_check_button_new(_("Save extended colors"), FALSE, LIVES_BOX(hbox), NULL);

    lives_signal_connect_after(LIVES_GUI_OBJECT(checkbutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                               LIVES_GUI_CALLBACK(on_boolean_toggled),
                               &mainw->fx1_bool);
  }

  lives_entry_set_activates_default(LIVES_ENTRY(renamew->entry), TRUE);
  lives_entry_set_width_chars(LIVES_ENTRY(renamew->entry), RW_ENTRY_DISPWIDTH);

  cancelbutton = lives_standard_button_new_from_stock(LIVES_STOCK_CANCEL, NULL);

  if (!(type == 4 && !mainw->interactive)) {
    lives_dialog_add_action_widget(LIVES_DIALOG(renamew->dialog), cancelbutton, LIVES_RESPONSE_CANCEL);
    lives_widget_set_can_focus_and_default(cancelbutton);

    lives_widget_add_accelerator(cancelbutton, LIVES_WIDGET_CLICKED_SIGNAL, accel_group,
                                 LIVES_KEY_Escape, (LiVESXModifierType)0, (LiVESAccelFlags)0);
  }

  if (type == 6) {
    okbutton = lives_standard_button_new_from_stock(LIVES_STOCK_GO_FORWARD, _("_Next"));
  } else okbutton = lives_standard_button_new_from_stock(LIVES_STOCK_OK, NULL);

  lives_dialog_add_action_widget(LIVES_DIALOG(renamew->dialog), okbutton, LIVES_RESPONSE_OK);
  lives_widget_set_can_focus_and_default(okbutton);
  lives_widget_grab_default_special(okbutton);

  if (type != 3) {
    lives_signal_connect(LIVES_GUI_OBJECT(cancelbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                         LIVES_GUI_CALLBACK(lives_general_button_clicked),
                         renamew);
  }

  if (type == 1) {
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
  lives_tvcardw_t *tvcardw = (lives_tvcardw_t *)(user_data);

  tvcardw->use_advanced = !tvcardw->use_advanced;

  if (tvcardw->use_advanced) {
    lives_widget_show(tvcardw->adv_vbox);
    lives_button_set_label(LIVES_BUTTON(tvcardw->advbutton), _("Use def_aults"));
  } else {
    lives_button_set_label(LIVES_BUTTON(tvcardw->advbutton), _("_Advanced"));
    lives_window_resize(LIVES_WINDOW(lives_widget_get_toplevel(tvcardw->adv_vbox)), 4, 40);
    lives_widget_hide(tvcardw->adv_vbox);
  }

  lives_widget_queue_resize(lives_widget_get_parent(tvcardw->adv_vbox));
}


static void rb_tvcarddef_toggled(LiVESToggleButton *tbut, livespointer user_data) {
  lives_tvcardw_t *tvcardw = (lives_tvcardw_t *)(user_data);

  if (!lives_toggle_button_get_active(tbut)) {
    lives_widget_set_sensitive(tvcardw->spinbuttonw, TRUE);
    lives_widget_set_sensitive(tvcardw->spinbuttonh, TRUE);
    lives_widget_set_sensitive(tvcardw->spinbuttonf, TRUE);
  } else {
    lives_widget_set_sensitive(tvcardw->spinbuttonw, FALSE);
    lives_widget_set_sensitive(tvcardw->spinbuttonh, FALSE);
    lives_widget_set_sensitive(tvcardw->spinbuttonf, FALSE);
  }
}


static void after_dialog_combo_changed(LiVESWidget *combo, livespointer user_data) {
  LiVESList *list = (LiVESList *)user_data;
  char *etext = lives_combo_get_active_text(LIVES_COMBO(combo));
  mainw->fx1_val = lives_list_strcmp_index(list, etext);
  lives_free(etext);
}


LiVESWidget *create_combo_dialog(int type, livespointer user_data) {
  // create a dialog with combo box selector

  // type 1 == unicap device

  // afterwards, mainw->fx1_val points to index selected

  LiVESWidget *combo_dialog;
  LiVESWidget *dialog_vbox;
  LiVESWidget *label;
  LiVESWidget *combo;

  char *label_text = NULL, *title = NULL;

  LiVESList *list = (LiVESList *)user_data;

  if (type == 1) {
    title = lives_strdup(_("Select input device"));
  }

  combo_dialog = lives_standard_dialog_new(title, TRUE, -1, -1);
  if (title != NULL) lives_free(title);

  if (prefs->show_gui) {
    lives_window_set_transient_for(LIVES_WINDOW(combo_dialog), LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET));
  }

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(combo_dialog));

  if (type == 1) {
    label_text = lives_strdup(_("Select input device:"));
  }

  label = lives_standard_label_new(label_text);
  if (label_text != NULL) lives_free(label_text);

  lives_box_pack_start(LIVES_BOX(dialog_vbox), label, TRUE, TRUE, 0);

  combo = lives_combo_new();

  lives_entry_set_width_chars(LIVES_ENTRY(lives_combo_get_entry(LIVES_COMBO(combo))), 64);

  lives_combo_populate(LIVES_COMBO(combo), list);

  lives_combo_set_active_index(LIVES_COMBO(combo), 0);

  lives_signal_connect_after(LIVES_WIDGET_OBJECT(combo), LIVES_WIDGET_CHANGED_SIGNAL, LIVES_GUI_CALLBACK(after_dialog_combo_changed), list);

  lives_box_pack_start(LIVES_BOX(dialog_vbox), combo, TRUE, TRUE, widget_opts.packing_height * 2);

  if (type == 1) {
    add_deinterlace_checkbox(LIVES_BOX(dialog_vbox));
  }

  if (prefs->show_gui)
    lives_widget_show_all(combo_dialog);

  return combo_dialog;
}


LiVESWidget *create_cdtrack_dialog(int type, livespointer user_data) {
  // general purpose device dialog with label and up to 2 spinbuttons

  // type 0 = cd track
  // type 1 = dvd title/chapter/aid
  // type 2 = vcd title -- do we need chapter as well ?

  // type 3 = number of tracks in mt

  // type 4 = TV card (device and channel)
  // type 5 = fw card

  // TODO - for CD make this nicer - get track names

  lives_tvcardw_t *tvcardw = NULL;

  LiVESWidget *cd_dialog;
  LiVESWidget *dialog_vbox;
  LiVESWidget *hbox;
  LiVESWidget *spinbutton;
  LiVESWidget *cancelbutton;
  LiVESWidget *okbutton;

  LiVESAccelGroup *accel_group = LIVES_ACCEL_GROUP(lives_accel_group_new());

  LiVESSList *radiobutton_group = NULL;

  char *label_text = NULL, *title;

  int ph_mult = 4;

  if (type == LIVES_DEVICE_CD) {
    title = lives_strdup(_("Load CD Track"));
  } else if (type == LIVES_DEVICE_DVD) {
    title = lives_strdup(_("Select DVD Title/Chapter"));
  } else if (type == LIVES_DEVICE_VCD) {
    title = lives_strdup(_("Select VCD Title"));
  } else if (type == LIVES_DEVICE_INTERNAL) {
    title = lives_strdup(_("Change Maximum Visible Tracks"));
  } else {
    title = lives_strdup(_("Device details"));
  }

  cd_dialog = lives_standard_dialog_new(title, FALSE, -1, -1);
  lives_free(title);
  lives_signal_handlers_disconnect_by_func(cd_dialog, return_true, NULL);

  if (prefs->show_gui) {
    lives_window_set_transient_for(LIVES_WINDOW(cd_dialog), LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET));
  }

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(cd_dialog));

  if (type == LIVES_DEVICE_DVD || type == LIVES_DEVICE_TV_CARD) ph_mult = 2;

  hbox = lives_hbox_new(FALSE, widget_opts.packing_width * 5);
  lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, TRUE, TRUE, widget_opts.packing_height * ph_mult);

  if (type == LIVES_DEVICE_CD) {
    label_text = lives_strdup_printf(_("Track to load (from %s)"), prefs->cdplay_device);
  } else if (type == LIVES_DEVICE_DVD) {
    label_text = lives_strdup(_("DVD Title"));
  } else if (type == LIVES_DEVICE_VCD) {
    label_text = lives_strdup(_("VCD Title"));
  } else if (type == LIVES_DEVICE_INTERNAL) {
    label_text = lives_strdup(_("Maximum number of tracks to display"));
  } else if (type == LIVES_DEVICE_TV_CARD) {
    label_text = lives_strdup(_("Device:        /dev/video"));
  } else if (type == LIVES_DEVICE_FW_CARD) {
    label_text = lives_strdup(_("Device:        fw:"));
  }

  widget_opts.mnemonic_label = FALSE;
  if (type == LIVES_DEVICE_CD || type == LIVES_DEVICE_DVD || type == LIVES_DEVICE_VCD) {
    spinbutton = lives_standard_spin_button_new(label_text, mainw->fx1_val,
                 1., 256., 1., 10., 0,
                 LIVES_BOX(hbox), NULL);
  } else if (type == LIVES_DEVICE_INTERNAL) {
    spinbutton = lives_standard_spin_button_new(label_text, mainw->fx1_val,
                 5., 15., 1., 1., 0,
                 LIVES_BOX(hbox), NULL);
  } else {
    spinbutton = lives_standard_spin_button_new(label_text, 0.,
                 0., 31., 1., 1., 0,
                 LIVES_BOX(hbox), NULL);
  }
  widget_opts.mnemonic_label = TRUE;

  lives_free(label_text);

  lives_signal_connect_after(LIVES_GUI_OBJECT(spinbutton), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                             LIVES_GUI_CALLBACK(on_spin_value_changed),
                             LIVES_INT_TO_POINTER(1));

  add_fill_to_box(LIVES_BOX(hbox));

  if (type == LIVES_DEVICE_DVD || type == LIVES_DEVICE_TV_CARD) {
    hbox = lives_hbox_new(FALSE, widget_opts.packing_width * 5);
    lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, TRUE, TRUE, widget_opts.packing_height * ph_mult);

    if (type == LIVES_DEVICE_DVD) {
      spinbutton = lives_standard_spin_button_new(_("Chapter  "), mainw->fx2_val,
                   1., 1024., 1., 10., 0,
                   LIVES_BOX(hbox), NULL);
    } else {
      spinbutton = lives_standard_spin_button_new(_("Channel  "), 1.,
                   1., 69., 1., 1., 0,
                   LIVES_BOX(hbox), NULL);

    }

    lives_signal_connect_after(LIVES_GUI_OBJECT(spinbutton), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                               LIVES_GUI_CALLBACK(on_spin_value_changed),
                               LIVES_INT_TO_POINTER(2));

    if (type == LIVES_DEVICE_DVD) {
      hbox = lives_hbox_new(FALSE, widget_opts.packing_width * 5);
      lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, TRUE, TRUE, widget_opts.packing_height * ph_mult);

      spinbutton = lives_standard_spin_button_new(_("Audio ID  "), mainw->fx3_val,
                   DVD_AUDIO_CHAN_MIN, DVD_AUDIO_CHAN_MAX, 1., 1., 0,
                   LIVES_BOX(hbox), NULL);

      lives_signal_connect_after(LIVES_GUI_OBJECT(spinbutton), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                                 LIVES_GUI_CALLBACK(on_spin_value_changed),
                                 LIVES_INT_TO_POINTER(3));

    }
  }

  if (type == LIVES_DEVICE_TV_CARD || type == LIVES_DEVICE_FW_CARD) {
    hbox = add_deinterlace_checkbox(LIVES_BOX(dialog_vbox));
    add_fill_to_box(LIVES_BOX(hbox));
  }

  if (type == LIVES_DEVICE_TV_CARD) {
    LiVESList *dlist = NULL;
    LiVESList *olist = NULL;
    char const *str;
    char *tvcardtypes[] = LIVES_TV_CARD_TYPES;
    register int i;

    tvcardw = (lives_tvcardw_t *)lives_malloc(sizeof(lives_tvcardw_t));
    tvcardw->use_advanced = FALSE;

    for (i = 0; (str = tvcardtypes[i]) != NULL; i++) {
      dlist = lives_list_append(dlist, (livespointer)tvcardtypes[i]);
    }

    lives_box_set_spacing(LIVES_BOX(dialog_vbox), widget_opts.packing_height * 2);

    hbox = lives_hbox_new(FALSE, widget_opts.packing_width * 5);

    lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, FALSE, FALSE, widget_opts.packing_height * 2);

    add_fill_to_box(LIVES_BOX(hbox));

    tvcardw->advbutton = lives_standard_button_new_from_stock(LIVES_STOCK_PREFERENCES, _("_Advanced"));

    lives_box_pack_start(LIVES_BOX(hbox), tvcardw->advbutton, TRUE, TRUE, widget_opts.packing_width * 4);

    add_fill_to_box(LIVES_BOX(hbox));

    tvcardw->adv_vbox = lives_vbox_new(FALSE, widget_opts.packing_width * 5);
    lives_box_pack_start(LIVES_BOX(dialog_vbox), tvcardw->adv_vbox, TRUE, TRUE, widget_opts.packing_height * 2);

    // add input, width, height, fps, driver and outfmt

    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(tvcardw->adv_vbox), hbox, TRUE, FALSE, 0);

    tvcardw->spinbuttoni = lives_standard_spin_button_new(_("Input number"),
                           0., 0., 16., 1., 1., 0,
                           LIVES_BOX(hbox), NULL);

    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(tvcardw->adv_vbox), hbox, TRUE, FALSE, 0);

    tvcardw->radiobuttond = lives_standard_radio_button_new(_("Use default width, height and FPS"),
                            &radiobutton_group, LIVES_BOX(hbox), NULL);

    lives_signal_connect_after(LIVES_GUI_OBJECT(tvcardw->radiobuttond), LIVES_WIDGET_TOGGLED_SIGNAL,
                               LIVES_GUI_CALLBACK(rb_tvcarddef_toggled),
                               (livespointer)tvcardw);

    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(tvcardw->adv_vbox), hbox, TRUE, FALSE, 0);

    lives_standard_radio_button_new(NULL, &radiobutton_group, LIVES_BOX(hbox), NULL);

    tvcardw->spinbuttonw = lives_standard_spin_button_new(_("Width"),
                           640., 4., 4096., 4., 16., 0,
                           LIVES_BOX(hbox), NULL);

    lives_widget_set_sensitive(tvcardw->spinbuttonw, FALSE);

    tvcardw->spinbuttonh = lives_standard_spin_button_new(_("Height"),
                           480., 4., 4096., 4., 16., 0,
                           LIVES_BOX(hbox), NULL);

    lives_widget_set_sensitive(tvcardw->spinbuttonh, FALSE);

    tvcardw->spinbuttonf = lives_standard_spin_button_new(_("FPS"),
                           25., 1., FPS_MAX, 1., 10., 3,
                           LIVES_BOX(hbox), NULL);

    lives_widget_set_sensitive(tvcardw->spinbuttonf, FALSE);

    hbox = lives_hbox_new(FALSE, 0);

    tvcardw->combod = lives_standard_combo_new(_("_Driver"), dlist, LIVES_BOX(hbox), NULL);
    lives_combo_set_active_index(LIVES_COMBO(tvcardw->combod), 0);

    tvcardw->comboo = lives_standard_combo_new(_("_Output format"), olist, LIVES_BOX(hbox), NULL);

    lives_widget_show_all(hbox);
    lives_box_pack_start(LIVES_BOX(tvcardw->adv_vbox), hbox, TRUE, FALSE, 0);

    lives_signal_connect(LIVES_GUI_OBJECT(tvcardw->advbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                         LIVES_GUI_CALLBACK(on_liveinp_advanced_clicked),
                         tvcardw);

    lives_widget_hide(tvcardw->adv_vbox);

    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(cd_dialog), "tvcard_data", tvcardw);
  }

  add_fill_to_box(LIVES_BOX(dialog_vbox));

  cancelbutton = lives_standard_button_new_from_stock(LIVES_STOCK_CANCEL, NULL);
  lives_dialog_add_action_widget(LIVES_DIALOG(cd_dialog), cancelbutton, LIVES_RESPONSE_CANCEL);

  okbutton = lives_standard_button_new_from_stock(LIVES_STOCK_OK, NULL);
  lives_dialog_add_action_widget(LIVES_DIALOG(cd_dialog), okbutton, LIVES_RESPONSE_OK);
  lives_widget_set_can_focus_and_default(okbutton);

  lives_widget_grab_default_special(okbutton);

  lives_widget_add_accelerator(cancelbutton, LIVES_WIDGET_CLICKED_SIGNAL, accel_group,
                               LIVES_KEY_Escape, (LiVESXModifierType)0, (LiVESAccelFlags)0);

  lives_widget_add_accelerator(okbutton, LIVES_WIDGET_CLICKED_SIGNAL, accel_group,
                               LIVES_KEY_Return, (LiVESXModifierType)0, (LiVESAccelFlags)0);

  if (type != LIVES_DEVICE_TV_CARD && type != LIVES_DEVICE_FW_CARD) {
    lives_signal_connect(LIVES_GUI_OBJECT(cancelbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                         LIVES_GUI_CALLBACK(lives_general_button_clicked),
                         NULL);
  }

  if (type == LIVES_DEVICE_CD) {
    lives_signal_connect(LIVES_GUI_OBJECT(okbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                         LIVES_GUI_CALLBACK(on_load_cdtrack_ok_clicked),
                         NULL);
  } else if (type == LIVES_DEVICE_DVD || type == LIVES_DEVICE_VCD)  {
    lives_signal_connect(LIVES_GUI_OBJECT(okbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                         LIVES_GUI_CALLBACK(on_load_vcd_ok_clicked),
                         LIVES_INT_TO_POINTER(type));
  } else if (type == LIVES_DEVICE_INTERNAL)  {
    lives_signal_connect(LIVES_GUI_OBJECT(okbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                         LIVES_GUI_CALLBACK(mt_change_disp_tracks_ok),
                         user_data);
  }

  lives_window_add_accel_group(LIVES_WINDOW(cd_dialog), accel_group);

  lives_widget_show_all(cd_dialog);

  if (type == LIVES_DEVICE_TV_CARD) lives_widget_hide(tvcardw->adv_vbox);

  return cd_dialog;
}


static void rb_aud_sel_pressed(LiVESButton *button, livespointer user_data) {
  aud_dialog_t *audd = (aud_dialog_t *)user_data;
  audd->is_sel = !audd->is_sel;
  lives_widget_set_sensitive(audd->time_spin, !audd->is_sel);
}


aud_dialog_t *create_audfade_dialog(int type) {
  // type 0 = fade in
  // type 1 = fade out

  LiVESWidget *dialog_vbox;
  LiVESWidget *hbox;
  LiVESWidget *rb_sel;
  LiVESWidget *label;

  char *label_text = NULL, *label_text2 = NULL, *title;

  double max;

  LiVESSList *radiobutton_group = NULL;

  aud_dialog_t *audd = (aud_dialog_t *)lives_malloc(sizeof(aud_dialog_t));

  if (type == 0) {
    title = lives_strdup(_("Fade Audio In"));
  } else {
    title = lives_strdup(_("Fade Audio Out"));
  }

  audd->dialog = lives_standard_dialog_new(title, TRUE, -1, -1);
  lives_signal_handlers_disconnect_by_func(audd->dialog, return_true, NULL);
  lives_free(title);

  if (prefs->show_gui) {
    lives_window_set_transient_for(LIVES_WINDOW(audd->dialog), LIVES_WINDOW(mainw->LiVES));
  }

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(audd->dialog));

  hbox = lives_hbox_new(FALSE, TB_HEIGHT_AUD);
  lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, TRUE, TRUE, widget_opts.packing_height);

  if (type == 0) {
    label_text = lives_strdup(_("Fade in over  "));
    label_text2 = lives_strdup(_("first"));
  } else if (type == 1) {
    label_text = lives_strdup(_("Fade out over  "));
    label_text2 = lives_strdup(_("last"));
  }

  label = lives_standard_label_new(label_text);
  if (label_text != NULL) lives_free(label_text);

  lives_box_pack_start(LIVES_BOX(hbox), label, FALSE, FALSE, 0);

  hbox = lives_hbox_new(FALSE, widget_opts.packing_width * 5);
  lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, TRUE, TRUE, widget_opts.packing_height);

  lives_standard_radio_button_new(label_text2, &radiobutton_group, LIVES_BOX(hbox), NULL);

  if (label_text2 != NULL) lives_free(label_text2);

  max = cfile->laudio_time;

  widget_opts.swap_label = TRUE;
  audd->time_spin = lives_standard_spin_button_new(_("seconds."),
                    max / 2. > DEF_AUD_FADE_SECS ? DEF_AUD_FADE_SECS : max / 2., .1, max, 1., 10., 2,
                    LIVES_BOX(hbox), NULL);
  widget_opts.swap_label = FALSE;

  hbox = lives_hbox_new(FALSE, widget_opts.packing_width * 5);
  lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, TRUE, TRUE, widget_opts.packing_height);

  rb_sel = lives_standard_radio_button_new(_("selection"), &radiobutton_group, LIVES_BOX(hbox), NULL);

  audd->is_sel = FALSE;

  if ((cfile->end - 1.) / cfile->fps > cfile->laudio_time) {
    // if selection is longer than audio time, we cannot use sel len
    lives_widget_set_sensitive(rb_sel, FALSE);
  } else {
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(rb_sel), TRUE);
    rb_aud_sel_pressed(LIVES_BUTTON(rb_sel), (livespointer)audd);
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

  char *extrabit, *title;

  _commentsw *commentsw = (_commentsw *)(lives_malloc(sizeof(_commentsw)));

  if (filename != NULL) extrabit = lives_strdup(_(" (Optional)"));
  else extrabit = lives_strdup("");

  title = lives_strdup_printf(_("File Comments%s"), extrabit);

  commentsw->comments_dialog = lives_standard_dialog_new(title, TRUE, -1, -1);
  lives_free(title);
  lives_free(extrabit);
  lives_signal_handlers_disconnect_by_func(commentsw->comments_dialog, return_true, NULL);

  if (prefs->show_gui) {
    lives_window_set_transient_for(LIVES_WINDOW(commentsw->comments_dialog), LIVES_WINDOW(mainw->LiVES));
  }

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(commentsw->comments_dialog));

  if (filename != NULL) {
    label = lives_standard_label_new((extrabit = lives_strdup_printf(_("File Name: %s"), filename)));
    lives_free(extrabit);
    lives_box_pack_start(LIVES_BOX(dialog_vbox), label, TRUE, TRUE, widget_opts.packing_height);
  }

  table = lives_table_new(4, 2, FALSE);
  lives_container_set_border_width(LIVES_CONTAINER(table), widget_opts.border_width);

  lives_table_set_row_spacings(LIVES_TABLE(table), widget_opts.packing_height * 2);

  lives_box_pack_start(LIVES_BOX(dialog_vbox), table, TRUE, TRUE, widget_opts.packing_height);

  label = lives_standard_label_new(_("Title/Name : "));

  lives_table_attach(LIVES_TABLE(table), label, 0, 1, 0, 1,
                     (LiVESAttachOptions)(LIVES_FILL),
                     (LiVESAttachOptions)(0), 0, 0);
  lives_label_set_halignment(LIVES_LABEL(label), 0.);

  label = lives_standard_label_new(_("Author/Artist : "));

  lives_table_attach(LIVES_TABLE(table), label, 0, 1, 1, 2,
                     (LiVESAttachOptions)(LIVES_FILL),
                     (LiVESAttachOptions)(0), 0, 0);
  lives_label_set_halignment(LIVES_LABEL(label), 0.);

  label = lives_standard_label_new(_("Comments : "));

  lives_table_attach(LIVES_TABLE(table), label, 0, 1, 3, 4,
                     (LiVESAttachOptions)(LIVES_FILL),
                     (LiVESAttachOptions)(0), 0, 0);

  lives_label_set_halignment(LIVES_LABEL(label), 0.);

  commentsw->title_entry = lives_standard_entry_new(NULL, cfile->title, STD_ENTRY_WIDTH, -1, NULL, NULL);

  lives_table_attach(LIVES_TABLE(table), commentsw->title_entry, 1, 2, 0, 1,
                     (LiVESAttachOptions)(LIVES_EXPAND | LIVES_FILL),
                     (LiVESAttachOptions)(LIVES_EXPAND), 0, 0);

  commentsw->author_entry = lives_standard_entry_new(NULL, cfile->author, STD_ENTRY_WIDTH, -1, NULL, NULL);

  lives_table_attach(LIVES_TABLE(table), commentsw->author_entry, 1, 2, 1, 2,
                     (LiVESAttachOptions)(LIVES_EXPAND | LIVES_FILL),
                     (LiVESAttachOptions)(LIVES_EXPAND), 0, 0);

  commentsw->comment_entry = lives_standard_entry_new(NULL, cfile->comment, STD_ENTRY_WIDTH, 250, NULL, NULL);

  lives_table_attach(LIVES_TABLE(table), commentsw->comment_entry, 1, 2, 3, 4,
                     (LiVESAttachOptions)(LIVES_EXPAND | LIVES_FILL),
                     (LiVESAttachOptions)(LIVES_EXPAND), 0, 0);

  if (filename != NULL) {
    // options
    vbox = lives_vbox_new(FALSE, 0);

    add_fill_to_box(LIVES_BOX(vbox));

    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

    commentsw->subt_checkbutton = lives_standard_check_button_new(_("Save _subtitles to file"), FALSE, LIVES_BOX(hbox), NULL);

    if (sfile->subt == NULL) {
      lives_widget_set_sensitive(commentsw->subt_checkbutton, FALSE);
    } else lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(commentsw->subt_checkbutton), TRUE);

    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

    commentsw->subt_entry = lives_standard_entry_new(_("Subtitle file"), NULL, SHORT_ENTRY_WIDTH, -1, LIVES_BOX(hbox), NULL);

    buttond = lives_standard_button_new_with_label(_("Browse..."));

    lives_signal_connect(buttond, LIVES_WIDGET_CLICKED_SIGNAL, LIVES_GUI_CALLBACK(on_save_subs_activate),
                         (livespointer)commentsw->subt_entry);

    lives_box_pack_start(LIVES_BOX(hbox), buttond, FALSE, FALSE, widget_opts.packing_width);

    add_fill_to_box(LIVES_BOX(vbox));

    if (sfile->subt == NULL) {
      lives_widget_set_sensitive(commentsw->subt_entry, FALSE);
      lives_widget_set_sensitive(buttond, FALSE);
    } else {
      char xfilename[512];
      char *osubfname = NULL;

      lives_snprintf(xfilename, 512, "%s", filename);
      get_filename(xfilename, FALSE); // strip extension
      switch (sfile->subt->type) {
      case SUBTITLE_TYPE_SRT:
        osubfname = lives_strdup_printf("%s.%s", xfilename, LIVES_FILE_EXT_SRT);
        break;

      case SUBTITLE_TYPE_SUB:
        osubfname = lives_strdup_printf("%s.%s", xfilename, LIVES_FILE_EXT_SUB);
        break;

      default:
        break;
      }
      lives_entry_set_text(LIVES_ENTRY(commentsw->subt_entry), osubfname);
      mainw->subt_save_file = osubfname; // assign instead of free
    }

    lives_widget_set_size_request(vbox, ENC_DETAILS_WIN_H, ENC_DETAILS_WIN_V);
    lives_widget_context_update();
    lives_standard_expander_new(_("_Options"), LIVES_BOX(dialog_vbox), vbox);
  }

  lives_widget_show_all(commentsw->comments_dialog);

  return commentsw;
}


static char last_good_folder[PATH_MAX];

static void chooser_check_dir(LiVESFileChooser *chooser, livespointer user_data) {
  char *cwd = lives_get_current_dir();
  char *new_dir;

#ifdef GUI_GTK
  new_dir = gtk_file_chooser_get_current_folder(chooser);
#endif
#ifdef GUI_QT
  QFileDialog *qchooser = static_cast<QFileDialog *>(chooser);
  new_dir = qchooser->directory().path().toLocal8Bit().data();
#endif

  if (!strcmp(new_dir, last_good_folder)) return;

  if (lives_chdir(new_dir, TRUE)) {
    lives_free(cwd);
#ifdef GUI_GTK
    gtk_file_chooser_set_current_folder(chooser, last_good_folder);
#endif
#ifdef GUI_QT
    qchooser->setDirectory(last_good_folder);
#endif
    do_dir_perm_access_error(new_dir);
    lives_free(new_dir);
    return;
  }
  lives_snprintf(last_good_folder, PATH_MAX, "%s", new_dir);
  lives_chdir(cwd, FALSE);
  lives_free(new_dir);
  lives_free(cwd);
}


LIVES_INLINE void chooser_response(LiVESWidget *widget, int response, livespointer udata) {
  mainw->fc_buttonresponse = response;
}


char *choose_file(const char *dir, const char *fname, char **const filt, LiVESFileChooserAction act,
                  const char *title, LiVESWidget *extra_widget) {
  // new style file chooser

  // in/out values are in utf8 encoding

  LiVESWidget *chooser;

  char *mytitle;
  char *filename = NULL;

  int response;
  register int i;

  if (title == NULL) {
    if (act == LIVES_FILE_CHOOSER_ACTION_SELECT_DEVICE) {
      mytitle = lives_strdup_printf(_("%sChoose a Device"), widget_opts.title_prefix);
      act = LIVES_FILE_CHOOSER_ACTION_OPEN;
    } else if (act == LIVES_FILE_CHOOSER_ACTION_SELECT_FOLDER) {
      mytitle = lives_strdup_printf(_("%sChoose a Directory"), widget_opts.title_prefix);
    } else {
      mytitle = lives_strdup_printf(_("%sChoose a File"), widget_opts.title_prefix);
    }
  } else mytitle = lives_strdup_printf("%s%s", widget_opts.title_prefix, title);

#ifdef GUI_GTK

  if (act != LIVES_FILE_CHOOSER_ACTION_SAVE) {
    if (mainw->interactive)
      chooser = gtk_file_chooser_dialog_new(mytitle, LIVES_WINDOW(mainw->LiVES), (LiVESFileChooserAction)act,
                                            LIVES_STOCK_LABEL_CANCEL, LIVES_RESPONSE_CANCEL,
                                            LIVES_STOCK_LABEL_OPEN, LIVES_RESPONSE_ACCEPT,
                                            NULL);
    else
      chooser = gtk_file_chooser_dialog_new(mytitle, LIVES_WINDOW(mainw->LiVES), (LiVESFileChooserAction)act,
                                            LIVES_STOCK_LABEL_OPEN, LIVES_RESPONSE_ACCEPT,
                                            NULL);
  } else {
    chooser = gtk_file_chooser_dialog_new(mytitle, LIVES_WINDOW(mainw->LiVES), (LiVESFileChooserAction)act,
                                          LIVES_STOCK_LABEL_CANCEL, LIVES_RESPONSE_CANCEL,
                                          LIVES_STOCK_LABEL_SAVE, LIVES_RESPONSE_ACCEPT,
                                          NULL);

  }

  gtk_file_chooser_set_local_only(LIVES_FILE_CHOOSER(chooser), TRUE);

  if (dir != NULL) {
    gtk_file_chooser_set_current_folder(LIVES_FILE_CHOOSER(chooser), dir);
    gtk_file_chooser_add_shortcut_folder(LIVES_FILE_CHOOSER(chooser), dir, NULL);
  }

  if (filt != NULL) {
    GtkFileFilter *filter = gtk_file_filter_new();
    for (i = 0; filt[i] != NULL; i++) gtk_file_filter_add_pattern(filter, filt[i]);
    gtk_file_chooser_set_filter(LIVES_FILE_CHOOSER(chooser), filter);
    if (fname == NULL && i == 1 && act == LIVES_FILE_CHOOSER_ACTION_SAVE)
      gtk_file_chooser_set_current_name(LIVES_FILE_CHOOSER(chooser), filt[0]); //utf-8
  }

  if (fname != NULL) {
    if (act == LIVES_FILE_CHOOSER_ACTION_SAVE || act == LIVES_FILE_CHOOSER_ACTION_CREATE_FOLDER) { // prevent assertion in gtk+
      gtk_file_chooser_set_current_name(LIVES_FILE_CHOOSER(chooser), fname); // utf-8
      if (fname != NULL && dir != NULL) {
        char *ffname = lives_build_filename(dir, fname, NULL);
        gtk_file_chooser_select_filename(LIVES_FILE_CHOOSER(chooser), ffname); // must be dir and file
        lives_free(ffname);
      }
    }
  }

  if (extra_widget != NULL && extra_widget != mainw->LiVES) {
    gtk_file_chooser_set_extra_widget(LIVES_FILE_CHOOSER(chooser), extra_widget);
    if (palette->style & STYLE_1) {
      GtkWidget *parent = lives_widget_get_parent(extra_widget);

      while (parent != NULL) {
        lives_widget_set_fg_color(parent, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
        lives_widget_set_bg_color(parent, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
        parent = lives_widget_get_parent(parent);
      }
    }
  }

  if (mainw->is_ready && palette->style & STYLE_1) {
    lives_widget_set_bg_color(chooser, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
    set_child_colour(chooser, FALSE);
  }

#endif

#ifdef GUI_QT
  LiVESFileChooser *fchooser = new LiVESFileChooser();
  QFileDialog *qchooser = static_cast<QFileDialog *>(fchooser);
  qchooser->setModal(true);
  qchooser->setWindowTitle(QString::fromUtf8(mytitle));

  //  LiVESWidget *aarea = chooser->get_action_area();

  if (act != LIVES_FILE_CHOOSER_ACTION_SAVE) {
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

  if (mainw->is_ready && palette->style & STYLE_1) {
    lives_widget_set_bg_color(fchooser, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
    //set_child_colour(chooser,FALSE);
  }

  if (dir != NULL) {
    qchooser->setDirectory(QString::fromUtf8(dir));
    QList<QUrl> urls = qchooser->sidebarUrls();
    urls.append(QString::fromUtf8(dir));
    qchooser->setSidebarUrls(urls);
  }

  if (filt != NULL) {
    QStringList filter;
    for (i = 0; filt[i] != NULL; i++) filter.append(QString::fromUtf8(filt[i]));

    qchooser->setNameFilters(filter);

    if (fname == NULL && i == 1 && act == LIVES_FILE_CHOOSER_ACTION_SAVE)
      qchooser->setDefaultSuffix(QString::fromUtf8(filt[0]));
  }

  // TODO
  //if (extra_widget!=NULL) gtk_file_chooser_set_extra_widget(fchooser,extra_widget);

  chooser = static_cast<LiVESWidget *>(fchooser);

#endif

  lives_container_set_border_width(LIVES_CONTAINER(chooser), widget_opts.border_width);

  if (prefs->show_gui) {
    lives_window_set_transient_for(LIVES_WINDOW(chooser), get_transient_full());
  }

  lives_signal_connect(chooser, LIVES_WIDGET_CURRENT_FOLDER_CHANGED_SIGNAL, LIVES_GUI_CALLBACK(chooser_check_dir), NULL);

  lives_widget_grab_focus(chooser);

  lives_widget_show_all(chooser);

  lives_window_center(LIVES_WINDOW(chooser));

  lives_window_set_modal(LIVES_WINDOW(chooser), TRUE);

  memset(last_good_folder, 0, 1);

  // set this so we know when button is pressed, even if waiting for preview to finish
  mainw->fc_buttonresponse = LIVES_RESPONSE_NONE;
  lives_signal_connect(chooser, LIVES_WIDGET_RESPONSE_SIGNAL, LIVES_GUI_CALLBACK(chooser_response), NULL);

  if (extra_widget == mainw->LiVES && mainw->LiVES != NULL) {
    return (char *)chooser; // kludge to allow custom adding of extra widgets
  }

rundlg:

  if ((response = lives_dialog_run(LIVES_DIALOG(chooser))) != LIVES_RESPONSE_CANCEL) {
    char *tmp;
    filename = lives_filename_to_utf8((tmp = lives_file_chooser_get_filename(LIVES_FILE_CHOOSER(chooser))), -1, NULL, NULL, NULL);
    lives_free(tmp);
  } else filename = NULL;

  if (filename != NULL && act == LIVES_FILE_CHOOSER_ACTION_SAVE) {
    if (!check_file(filename, TRUE)) {
      lives_free(filename);
      filename = NULL;
      goto rundlg;
    }
  }

  lives_free(mytitle);

  lives_widget_destroy(chooser);

  return filename;
}


LiVESWidget *choose_file_with_preview(const char *dir, const char *title, char **const filt, int filesel_type) {
  // filesel_type 1 - video and audio open (single - opensel)
  //LIVES_FILE_SELECTION_VIDEO_AUDIO

  // preview type 2 - import audio
  // LIVES_FILE_SELECTION_AUDIO_ONLY

  // filesel_type 3 - video and audio open (multiple)
  //LIVES_FILE_SELECTION_VIDEO_AUDIO_MULTI

  // type 4
  // LIVES_FILE_SELECTION_VIDEO_RANGE

  // type 5
  // LIVES_FILE_SELECTION_IMAGE_ONLY

  // unfortunately we cannot simply run this and return a filename, in case there is a selection

  LiVESWidget *chooser;

  int preview_type;

  chooser = (LiVESWidget *)choose_file(dir, NULL, filt, LIVES_FILE_CHOOSER_ACTION_OPEN, title, mainw->LiVES);

  if (filesel_type == LIVES_FILE_SELECTION_VIDEO_AUDIO_MULTI) {
#ifdef GUI_GTK
    gtk_file_chooser_set_select_multiple(LIVES_FILE_CHOOSER(chooser), TRUE);
#endif
#ifdef GUI_QT
    QFileDialog *qchooser = static_cast<QFileDialog *>(static_cast<LiVESFileChooser *>(chooser));
    qchooser->setFileMode(QFileDialog::ExistingFiles);
#endif
  }

  switch (filesel_type) {
  case LIVES_FILE_SELECTION_VIDEO_AUDIO:
  case LIVES_FILE_SELECTION_VIDEO_AUDIO_MULTI:
    preview_type = LIVES_PREVIEW_TYPE_VIDEO_AUDIO;
    break;
  case LIVES_FILE_SELECTION_IMAGE_ONLY:
    preview_type = LIVES_PREVIEW_TYPE_IMAGE_ONLY;
    break;
  default:
    preview_type = LIVES_PREVIEW_TYPE_AUDIO_ONLY;
  }

  widget_add_preview(chooser, LIVES_BOX(lives_dialog_get_content_area(LIVES_DIALOG(chooser))),
                     LIVES_BOX(lives_dialog_get_content_area(LIVES_DIALOG(chooser))),
                     LIVES_BOX(lives_dialog_get_content_area(LIVES_DIALOG(chooser))),
                     preview_type);

  if (prefs->fileselmax) {
    lives_window_set_resizable(LIVES_WINDOW(chooser), TRUE);
    lives_window_maximize(LIVES_WINDOW(chooser));
    lives_widget_queue_draw(chooser);
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
  LiVESWidget *cancelbutton;
  LiVESWidget *discardbutton;
  LiVESWidget *savebutton;
  LiVESWidget *hbox;

  LiVESAccelGroup *accel_group;

  char *labeltext = NULL;

  _entryw *cdsw = (_entryw *)(lives_malloc(sizeof(_entryw)));

  cdsw->warn_checkbutton = NULL;

  if (type == 0) {
    if (strlen(mainw->multitrack->layout_name) == 0) {
      labeltext = lives_strdup(
                    _("You are about to leave multitrack mode.\nThe current layout has not been saved.\nWhat would you like to do ?\n"));
    } else {
      labeltext = lives_strdup(
                    _("You are about to leave multitrack mode.\nThe current layout has been changed since the last save.\nWhat would you like to do ?\n"));
    }
  } else if (type == 1) {
    if (!mainw->only_close) labeltext = lives_strdup(
                                            _("You are about to exit LiVES.\nThe current clip set can be saved.\nWhat would you like to do ?\n"));
    else labeltext = lives_strdup(_("The current clip set has not been saved.\nWhat would you like to do ?\n"));
  } else if (type == 2 || type == 3) {
    if ((mainw->multitrack != NULL && mainw->multitrack->changed) || (mainw->stored_event_list != NULL && mainw->stored_event_list_changed)) {
      labeltext = lives_strdup(_("The current layout has not been saved.\nWhat would you like to do ?\n"));
    } else {
      labeltext = lives_strdup(_("The current layout has *NOT BEEN CHANGED* since it was last saved.\nWhat would you like to do ?\n"));
    }
  } else if (type == 4) {
    labeltext = lives_strdup(
                  _("You are about to leave multitrack mode.\nThe current layout contains generated frames and cannot be retained.\nWhat do you wish to do ?"));
  }

  cdsw->dialog = create_question_dialog(_("Cancel/Discard/Save"), labeltext,
                                        LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET));
  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(cdsw->dialog));

  if (labeltext != NULL) lives_free(labeltext);

  if (type == 1) {
    LiVESWidget *checkbutton;

    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

    cdsw->entry = lives_standard_entry_new(_("Clip set _name"), strlen(mainw->set_name) ? mainw->set_name : "",
                                           SHORT_ENTRY_WIDTH, 128, LIVES_BOX(hbox), NULL);

    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

    checkbutton = lives_standard_check_button_new(_("_Auto reload next time"), FALSE, LIVES_BOX(hbox), NULL);

    if ((type == 0 && prefs->ar_layout) || (type == 1 && !mainw->only_close)) {
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(checkbutton), TRUE);
      if (type == 1) prefs->ar_clipset = TRUE;
      else prefs->ar_layout = TRUE;
    } else {
      if (type == 1) prefs->ar_clipset = FALSE;
      else prefs->ar_layout = FALSE;
    }

    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(checkbutton), "cdsw", (livespointer)cdsw);

    lives_signal_connect(LIVES_GUI_OBJECT(checkbutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                         LIVES_GUI_CALLBACK(on_autoreload_toggled),
                         LIVES_INT_TO_POINTER(type));
  }

  if (type == 0 && !(prefs->warning_mask & WARN_MASK_EXIT_MT)) {
    add_warn_check(LIVES_BOX(dialog_vbox), WARN_MASK_EXIT_MT);
  }

  accel_group = LIVES_ACCEL_GROUP(lives_accel_group_new());
  lives_window_add_accel_group(LIVES_WINDOW(cdsw->dialog), accel_group);

  cancelbutton = lives_standard_button_new_from_stock(LIVES_STOCK_CANCEL, NULL);
  lives_dialog_add_action_widget(LIVES_DIALOG(cdsw->dialog), cancelbutton, LIVES_RESPONSE_CANCEL);
  lives_widget_add_accelerator(cancelbutton, LIVES_WIDGET_CLICKED_SIGNAL, accel_group,
                               LIVES_KEY_Escape, (LiVESXModifierType)0, (LiVESAccelFlags)0);

  discardbutton = lives_standard_button_new_from_stock(LIVES_STOCK_DELETE, NULL);

  lives_dialog_add_action_widget(LIVES_DIALOG(cdsw->dialog), discardbutton, 1 + (type == 2));

  if ((type == 0 && strlen(mainw->multitrack->layout_name) == 0) || type == 3 || type == 4)
    lives_button_set_label(LIVES_BUTTON(discardbutton), _("_Wipe layout"));
  else if (type == 0) lives_button_set_label(LIVES_BUTTON(discardbutton), _("_Ignore changes"));
  else if (type == 1) lives_button_set_label(LIVES_BUTTON(discardbutton), _("_Delete clip set"));
  else if (type == 2) lives_button_set_label(LIVES_BUTTON(discardbutton), _("_Delete layout"));

  savebutton = lives_standard_button_new_from_stock(LIVES_STOCK_SAVE, NULL);

  if (type == 0 || type == 3) lives_button_set_label(LIVES_BUTTON(savebutton), _("_Save layout"));
  else if (type == 1) lives_button_set_label(LIVES_BUTTON(savebutton), _("_Save clip set"));
  else if (type == 2) lives_button_set_label(LIVES_BUTTON(savebutton), _("_Wipe layout"));
  if (type != 4) lives_dialog_add_action_widget(LIVES_DIALOG(cdsw->dialog), savebutton, 2 - (type == 2));
  lives_widget_set_can_focus_and_default(savebutton);
  if (type == 1 || type == 2)lives_widget_grab_default_special(savebutton);

  lives_widget_show_all(cdsw->dialog);

  if (type == 1) {
    lives_widget_grab_focus(cdsw->entry);
  }

  if (!mainw->interactive) lives_widget_set_sensitive(cancelbutton, FALSE);

  return cdsw;
}


boolean do_layout_recover_dialog(void) {
  if (!do_yesno_dialog(_("\nLiVES has detected a multitrack layout from a previous session.\nWould you like to try and recover it ?\n"))) {
    recover_layout_cancelled(TRUE);
    return FALSE;
  } else return recover_layout();
}


static void flip_cdisk_bit(LiVESToggleButton *t, livespointer user_data) {
  uint32_t bitmask = LIVES_POINTER_TO_INT(user_data);
  prefs->clear_disk_opts ^= bitmask;
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

  boolean woat = widget_opts.apply_theme;

  char *tmp, *tmp2;

  dialog = lives_standard_dialog_new(_("Disk Recovery Options"), FALSE, DEF_DIALOG_WIDTH, DEF_DIALOG_HEIGHT);

  if (prefs->show_gui) {
    lives_window_set_transient_for(LIVES_WINDOW(dialog), LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET));
  }

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(dialog));

  vbox = lives_vbox_new(FALSE, 0);
  lives_container_set_border_width(LIVES_CONTAINER(vbox), widget_opts.border_width * 2);

  widget_opts.apply_theme = FALSE;
  scrollw = lives_standard_scrolled_window_new(DEF_DIALOG_WIDTH, DEF_DIALOG_HEIGHT, vbox);
  widget_opts.apply_theme = woat;

  lives_container_add(LIVES_CONTAINER(dialog_vbox), scrollw);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, TRUE, widget_opts.packing_height);

  checkbutton = lives_standard_check_button_new((tmp = lives_strdup(_("Delete _Orphaned Clips"))),
                !(prefs->clear_disk_opts & LIVES_CDISK_LEAVE_ORPHAN_SETS), LIVES_BOX(hbox),
                (tmp2 = lives_strdup(_("Delete any clips which are not currently loaded or part of a set"))));

  lives_free(tmp);
  lives_free(tmp2);

  lives_signal_connect_after(LIVES_GUI_OBJECT(checkbutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                             LIVES_GUI_CALLBACK(flip_cdisk_bit),
                             LIVES_INT_TO_POINTER(LIVES_CDISK_LEAVE_ORPHAN_SETS));

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, TRUE, widget_opts.packing_height);

  checkbutton = lives_standard_check_button_new(_("Clear _Backup Files from Closed Clips"),
                !(prefs->clear_disk_opts & LIVES_CDISK_LEAVE_BFILES), LIVES_BOX(hbox), NULL);

  lives_signal_connect_after(LIVES_GUI_OBJECT(checkbutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                             LIVES_GUI_CALLBACK(flip_cdisk_bit),
                             LIVES_INT_TO_POINTER(LIVES_CDISK_LEAVE_BFILES));

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, TRUE, widget_opts.packing_height);

  checkbutton = lives_standard_check_button_new(_("Remove Sets which have _Layouts but no Clips"),
                (prefs->clear_disk_opts & LIVES_CDISK_REMOVE_ORPHAN_LAYOUTS), LIVES_BOX(hbox), NULL);

  lives_signal_connect_after(LIVES_GUI_OBJECT(checkbutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                             LIVES_GUI_CALLBACK(flip_cdisk_bit),
                             LIVES_INT_TO_POINTER(LIVES_CDISK_REMOVE_ORPHAN_LAYOUTS));

  resetbutton = lives_standard_button_new_from_stock(LIVES_STOCK_REFRESH, _("_Reset to Defaults"));
  lives_dialog_add_action_widget(LIVES_DIALOG(dialog), resetbutton, LIVES_RESPONSE_RETRY);

  okbutton = lives_standard_button_new_from_stock(LIVES_STOCK_OK, NULL);
  lives_dialog_add_action_widget(LIVES_DIALOG(dialog), okbutton, LIVES_RESPONSE_OK);

  lives_widget_set_can_focus_and_default(okbutton);
  lives_widget_grab_default_special(okbutton);

  return dialog;
}


#ifdef GTK_TEXT_VIEW_DRAW_BUG

static ulong expt;

static boolean exposetview(LiVESWidget *widget, lives_painter_t *cr, livespointer user_data) {
  LiVESWidgetColor fgcol, bgcol;
  lives_colRGBA64_t fg, bg;
  LingoLayout *layout = NULL;
  lives_painter_surface_t *surface;

  char *text = lives_text_view_get_text(LIVES_TEXT_VIEW(widget));

  lives_signal_handler_block(widget, expt);

  surface = lives_painter_get_target(cr);
  lives_painter_surface_flush(surface);

  lives_widget_get_fg_state_color(widget, lives_widget_get_state(widget), &fgcol);
  lives_widget_get_bg_state_color(widget, lives_widget_get_state(widget), &bgcol);

  widget_color_to_lives_rgba(&fg, &fgcol);
  widget_color_to_lives_rgba(&bg, &bgcol);

  layout = render_text_to_cr(widget, cr, text, "", 0.0,
                             LIVES_TEXT_MODE_FOREGROUND_ONLY, &fg, &bg, FALSE, FALSE, 0., 0.,
                             lives_widget_get_allocation_width(widget), lives_widget_get_allocation_height(widget));

  lives_free(text);

  if (layout != NULL) {
    lingo_painter_show_layout(cr, layout);
    lives_object_unref(layout);
  }

  lives_painter_fill(cr);

  lives_signal_handler_unblock(widget, expt);

  return FALSE;
}

#endif


LiVESTextView *create_output_textview(void) {
  LiVESWidget *textview = lives_standard_text_view_new(NULL, NULL);

#ifdef GTK_TEXT_VIEW_DRAW_BUG
  expt = lives_signal_connect(LIVES_GUI_OBJECT(textview), LIVES_WIDGET_EXPOSE_EVENT,
                              LIVES_GUI_CALLBACK(exposetview),
                              NULL);
#endif

  lives_object_ref(textview);
  return LIVES_TEXT_VIEW(textview);
}


static int currow;

static void pair_add(LiVESWidget *table, const char *key, const char *meaning) {
  LiVESWidget *label;
  LiVESWidget *align;

  label = lives_standard_label_new(key);
  align = lives_alignment_new(0., 0., 0., 0.);
  lives_container_add(LIVES_CONTAINER(align), label);

  if (meaning != NULL) {
    lives_table_attach(LIVES_TABLE(table), align, 0, 1, currow, currow + 1,
                       (LiVESAttachOptions)(0),
                       (LiVESAttachOptions)(0), 0, 0);

    label = lives_standard_label_new(meaning);
    align = lives_alignment_new(0., 0., 0., 0.);
    lives_container_add(LIVES_CONTAINER(align), label);

    lives_table_attach(LIVES_TABLE(table), align, 1, 40, currow, currow + 1,
                       (LiVESAttachOptions)(LIVES_EXPAND),
                       (LiVESAttachOptions)(0), 0, 0);
  } else {
    lives_table_attach(LIVES_TABLE(table), align, 0, 39, currow, currow + 1,
                       (LiVESAttachOptions)(LIVES_EXPAND),
                       (LiVESAttachOptions)(0), 0, 0);
  }

  currow++;

  lives_widget_show_all(table);
}


void do_keys_window(void) {
  char *tmp = lives_strdup(_("Show Keys")), *tmp2;
  text_window *textwindow = create_text_window(tmp, NULL, NULL);
  lives_free(tmp);

  lives_table_resize(LIVES_TABLE(textwindow->table), 1, 40);
  currow = 0;

  pair_add(textwindow->table, _("You can use the following keys during playback to control LiVES:-\n\n"
                                "Recordable keys (press 'r' before playback to make a recording)\n"
                                "-----------------------\n"), NULL);

  pair_add(textwindow->table, (tmp = lives_strdup(_("ctrl-left"))), (tmp2 = lives_strdup(_("skip back\n"))));
  lives_free(tmp);
  lives_free(tmp2);
  pair_add(textwindow->table, (tmp = lives_strdup(_("ctrl-right"))), (tmp2 = lives_strdup(_("skip forwards\n"))));
  lives_free(tmp);
  lives_free(tmp2);
  pair_add(textwindow->table, (tmp = lives_strdup(_("ctrl-up"))), (tmp2 = lives_strdup(_("faster/increase effect\n"))));
  lives_free(tmp);
  lives_free(tmp2);
  pair_add(textwindow->table, (tmp = lives_strdup(_("ctrl-down"))), (tmp2 = lives_strdup(_("slower/decrease effect\n"))));
  lives_free(tmp);
  lives_free(tmp2);
  pair_add(textwindow->table, (tmp = lives_strdup(_("ctrl-enter"))), (tmp2 = lives_strdup(_("reset frame rate\n"))));
  lives_free(tmp);
  lives_free(tmp2);
  pair_add(textwindow->table, (tmp = lives_strdup(_("ctrl-space"))), (tmp2 = lives_strdup(_("reverse direction\n"))));
  lives_free(tmp);
  lives_free(tmp2);
  pair_add(textwindow->table, (tmp = lives_strdup(_("ctrl-alt-space"))), (tmp2 = lives_strdup(_("reverse direction (background clip)\n"))));
  lives_free(tmp);
  lives_free(tmp2);
  pair_add(textwindow->table, (tmp = lives_strdup(_("ctrl-backspace"))), (tmp2 = lives_strdup(_("freeze frame\n"))));
  lives_free(tmp);
  lives_free(tmp2);
  pair_add(textwindow->table, (tmp = lives_strdup(("n"))), (tmp2 = lives_strdup(_("nervous\n"))));
  lives_free(tmp);
  lives_free(tmp2);
  pair_add(textwindow->table, (tmp = lives_strdup(_("ctrl-page-up"))), (tmp2 = lives_strdup(_("previous clip\n"))));
  lives_free(tmp);
  lives_free(tmp2);
  pair_add(textwindow->table, (tmp = lives_strdup(_("ctrl-page-down"))), (tmp2 = lives_strdup(_("next clip\n"))));
  lives_free(tmp);
  lives_free(tmp2);
  pair_add(textwindow->table, (tmp = lives_strdup(("\n"))), NULL);
  lives_free(tmp);
  pair_add(textwindow->table, (tmp = lives_strdup(_("ctrl-1"))), (tmp2 = lives_strdup(_("toggle real-time effect 1\n"))));
  lives_free(tmp);
  lives_free(tmp2);
  pair_add(textwindow->table, (tmp = lives_strdup(_("ctrl-2"))), (tmp2 = lives_strdup(_("toggle real-time effect 2\n"))));
  lives_free(tmp);
  lives_free(tmp2);
  pair_add(textwindow->table, (tmp = lives_strdup(_("...etc...\n"))), NULL);
  lives_free(tmp);
  pair_add(textwindow->table, (tmp = lives_strdup(_("ctrl-0"))), (tmp2 = lives_strdup(_("real-time effects off\n"))));
  lives_free(tmp);
  lives_free(tmp2);
  pair_add(textwindow->table, (tmp = lives_strdup(("x"))), (tmp2 = lives_strdup(_("swap background/foreground\n"))));
  lives_free(tmp);
  lives_free(tmp2);
  pair_add(textwindow->table, (tmp = lives_strdup(("\n"))), NULL);
  lives_free(tmp);
  pair_add(textwindow->table, (tmp = lives_strdup(("k"))), (tmp2 = lives_strdup(_("grab keyboard for last activated effect\n"))));
  lives_free(tmp);
  lives_free(tmp2);
  pair_add(textwindow->table, (tmp = lives_strdup(("m"))), (tmp2 = lives_strdup(_("switch effect mode (when effect has keyboard grab)\n"))));
  lives_free(tmp);
  lives_free(tmp2);
  pair_add(textwindow->table, (tmp = lives_strdup(("t"))),
           (tmp2 = lives_strdup(_("enter text parameter (when effect has keyboard grab)\n"))));
  lives_free(tmp);
  lives_free(tmp2);
  pair_add(textwindow->table, (tmp = lives_strdup(_("tab"))),
           (tmp2 = lives_strdup(_("leave text parameter (when effect has keyboard grab)\n"))));
  lives_free(tmp);
  lives_free(tmp2);
  pair_add(textwindow->table, (tmp = lives_strdup(_("f1"))), (tmp2 = lives_strdup(_("store/switch to clip mnemonic 1\n"))));
  lives_free(tmp);
  lives_free(tmp2);
  pair_add(textwindow->table, (tmp = lives_strdup(_("f2"))), (tmp2 = lives_strdup(_("store/switch to clip mnemonic 2\n"))));
  lives_free(tmp);
  lives_free(tmp2);
  pair_add(textwindow->table, (tmp = lives_strdup(_("...etc...\n"))), NULL);
  lives_free(tmp);
  pair_add(textwindow->table, (tmp = lives_strdup(_("f12"))), (tmp2 = lives_strdup(_("clear function keys\n"))));
  lives_free(tmp);
  pair_add(textwindow->table, (tmp = lives_strdup(("\n"))), NULL);
  lives_free(tmp);
  pair_add(textwindow->table, (tmp = lives_strdup(("\n"))), NULL);
  lives_free(tmp);
  pair_add(textwindow->table, (tmp = lives_strdup(_("Other playback keys\n"))), NULL);
  lives_free(tmp);
  pair_add(textwindow->table, (tmp = lives_strdup("-----------------------------\n")), NULL);
  lives_free(tmp);
  pair_add(textwindow->table, (tmp = lives_strdup(("p"))), (tmp2 = lives_strdup(_("play all\n"))));
  lives_free(tmp);
  lives_free(tmp2);
  pair_add(textwindow->table, (tmp = lives_strdup(("y"))), (tmp2 = lives_strdup(_("play selection\n"))));
  lives_free(tmp);
  lives_free(tmp2);
  pair_add(textwindow->table, (tmp = lives_strdup(("q"))), (tmp2 = lives_strdup(_("stop\n"))));
  lives_free(tmp);
  lives_free(tmp2);
  pair_add(textwindow->table, (tmp = lives_strdup(("f"))), (tmp2 = lives_strdup(_("fullscreen\n"))));
  lives_free(tmp);
  lives_free(tmp2);
  pair_add(textwindow->table, (tmp = lives_strdup(("s"))), (tmp2 = lives_strdup(_("separate window\n"))));
  lives_free(tmp);
  lives_free(tmp2);
  pair_add(textwindow->table, (tmp = lives_strdup(("d"))), (tmp2 = lives_strdup(_("double size\n"))));
  lives_free(tmp);
  lives_free(tmp2);
  pair_add(textwindow->table, (tmp = lives_strdup(("g"))), (tmp2 = lives_strdup(_("ping pong loops\n"))));
  lives_free(tmp);
  lives_free(tmp2);
}


void do_mt_keys_window(void) {
  char *tmp = lives_strdup(_("Multitrack Keys")), *tmp2;
  text_window *textwindow = create_text_window(tmp, NULL, NULL);

  lives_free(tmp);

  lives_table_resize(LIVES_TABLE(textwindow->table), 1, 40);

  currow = 0;

  pair_add(textwindow->table, _("You can use the following keys to control the multitrack window:-\n"
                                "-----------------------\n"), NULL);

  pair_add(textwindow->table, (tmp = lives_strdup(_("ctrl-left-arrow"))), (tmp2 = lives_strdup(_("move timeline cursor left 1 second\n"))));
  lives_free(tmp);
  lives_free(tmp2);
  pair_add(textwindow->table, (tmp = lives_strdup(_("ctrl-right-arrow"))), (tmp2 = lives_strdup(_("move timeline cursor right 1 second\n"))));
  lives_free(tmp);
  lives_free(tmp2);
  pair_add(textwindow->table, (tmp = lives_strdup(_("shift-left-arrow"))), (tmp2 = lives_strdup(_("move timeline cursor left 1 frame\n"))));
  lives_free(tmp);
  lives_free(tmp2);
  pair_add(textwindow->table, (tmp = lives_strdup(_("shift-right-arrow"))), (tmp2 = lives_strdup(_("move timeline cursor right 1 frame\n"))));
  lives_free(tmp);
  lives_free(tmp2);
  pair_add(textwindow->table, (tmp = lives_strdup(_("ctrl-up-arrow"))), (tmp2 = lives_strdup(_("move current track up\n"))));
  lives_free(tmp);
  lives_free(tmp2);
  pair_add(textwindow->table, (tmp = lives_strdup(_("ctrl-down-arrow"))), (tmp2 = lives_strdup(_("move current track down\n"))));
  lives_free(tmp);
  lives_free(tmp2);
  pair_add(textwindow->table, (tmp = lives_strdup(_("ctrl-page-up"))), (tmp2 = lives_strdup(_("select previous clip\n"))));
  lives_free(tmp);
  lives_free(tmp2);
  pair_add(textwindow->table, (tmp = lives_strdup(_("ctrl-page-down"))), (tmp2 = lives_strdup(_("select next clip\n"))));
  lives_free(tmp);
  lives_free(tmp2);
  pair_add(textwindow->table, (tmp = lives_strdup(_("ctrl-space"))), (tmp2 = lives_strdup(_("select/deselect current track\n"))));
  lives_free(tmp);
  lives_free(tmp2);
  pair_add(textwindow->table, (tmp = lives_strdup(_("ctrl-plus"))), (tmp2 = lives_strdup(_("zoom in\n"))));
  lives_free(tmp);
  lives_free(tmp2);
  pair_add(textwindow->table, (tmp = lives_strdup(_("ctrl-minus"))), (tmp2 = lives_strdup(_("zoom out\n"))));
  lives_free(tmp);
  lives_free(tmp2);
  pair_add(textwindow->table, (tmp = lives_strdup(("m"))), (tmp2 = lives_strdup(_("make a mark on the timeline (during playback)\n"))));
  lives_free(tmp);
  lives_free(tmp2);
  pair_add(textwindow->table, (tmp = lives_strdup(("w"))), (tmp2 = lives_strdup(_("rewind to play start.\n"))));
  lives_free(tmp);
  lives_free(tmp2);
  pair_add(textwindow->table, (tmp = lives_strdup(("\n"))), NULL);
  lives_free(tmp);
  pair_add(textwindow->table, (tmp = lives_strdup(_("For other keys, see the menus.\n"))), NULL);
  lives_free(tmp);
}


autolives_window *autolives_pre_dialog(void) {
  // dialog for autolives activation
  // options: trigger: auto, time
  //                   omc - user1

  // TODO: add port numbers, add change types and probabilities.
  autolives_window *alwindow;

  LiVESWidget *trigframe;
  LiVESWidget *dialog_vbox;
  LiVESWidget *label;
  LiVESWidget *vbox;
  LiVESWidget *hbox;
  LiVESWidget *radiobutton;

  LiVESSList *radiobutton1_group = NULL;
  LiVESSList *radiobutton2_group = NULL;

  char *tmp, *tmp2;

  alwindow = (autolives_window *)lives_malloc(sizeof(autolives_window));

  alwindow->dialog = lives_standard_dialog_new(_("Autolives Options"), TRUE, DEF_DIALOG_WIDTH, DEF_DIALOG_HEIGHT);

  if (prefs->show_gui) {
    lives_window_set_transient_for(LIVES_WINDOW(alwindow->dialog), LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET));
  }

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(alwindow->dialog));

  trigframe = lives_standard_frame_new(_("Trigger"), 0., FALSE);

  lives_box_pack_start(LIVES_BOX(dialog_vbox), trigframe, FALSE, FALSE, widget_opts.packing_height);

  vbox = lives_vbox_new(FALSE, 0);
  lives_container_set_border_width(LIVES_CONTAINER(vbox), widget_opts.border_width);
  lives_container_add(LIVES_CONTAINER(trigframe), vbox);

  hbox = lives_hbox_new(FALSE, widget_opts.packing_width);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, TRUE, TRUE, widget_opts.packing_height);
  alwindow->atrigger_button = lives_standard_radio_button_new((tmp = lives_strdup(_("Timed:"))),
                              &radiobutton1_group, LIVES_BOX(hbox),
                              (tmp2 = lives_strdup(_("Trigger a change based on time"))));

  lives_free(tmp);
  lives_free(tmp2);

  alwindow->atrigger_spin = lives_standard_spin_button_new(_("change time (seconds)"), 1., 1., 1800., 1., 10., 0, LIVES_BOX(hbox), NULL);

  hbox = lives_hbox_new(FALSE, widget_opts.packing_width);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, TRUE, TRUE, widget_opts.packing_height);
  radiobutton = lives_standard_radio_button_new((tmp = lives_strdup(_("OMC"))),
                &radiobutton1_group, LIVES_BOX(hbox),
                (tmp2 = lives_strdup(_("Trigger a change based on receiving an OSC message"))));

  lives_free(tmp);
  lives_free(tmp2);

  if (has_devicemap(OSC_NOTIFY)) {
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(radiobutton), TRUE);
  }

  vbox = lives_vbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(dialog_vbox), vbox, TRUE, FALSE, widget_opts.packing_height * 2.);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

  label = lives_standard_label_new(_("Playback start:"));
  lives_box_pack_start(LIVES_BOX(hbox), label, FALSE, TRUE, 0);

  add_fill_to_box(LIVES_BOX(hbox));

  hbox = lives_hbox_new(FALSE, widget_opts.packing_width);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, TRUE, TRUE, widget_opts.packing_height);
  alwindow->apb_button = lives_standard_radio_button_new((tmp = lives_strdup(_("Automatic"))),
                         &radiobutton2_group, LIVES_BOX(hbox),
                         (tmp2 = lives_strdup(_("Start playback automatically"))));

  lives_free(tmp);
  lives_free(tmp2);

  radiobutton = lives_standard_radio_button_new((tmp = lives_strdup(_("Manual"))),
                &radiobutton2_group, LIVES_BOX(hbox),
                (tmp2 = lives_strdup(_("Wait for the user to start playback"))));

  lives_free(tmp);
  lives_free(tmp2);

  hbox = lives_hbox_new(FALSE, widget_opts.packing_width);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, TRUE, TRUE, widget_opts.packing_height * 2);

  alwindow->mute_button = lives_standard_check_button_new
                          ((tmp = lives_strdup(_("Mute audio during playback"))), FALSE, LIVES_BOX(hbox),
                           (tmp2 = lives_strdup(_("Mute the audio in LiVES during playback."))));

  lives_free(tmp);
  lives_free(tmp2);

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(alwindow->mute_button), TRUE);

  add_hsep_to_box(LIVES_BOX(dialog_vbox));

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

  alwindow->debug_button = lives_standard_check_button_new
                           ((tmp = lives_strdup(_("Debug mode"))), FALSE, LIVES_BOX(hbox),
                            (tmp2 = lives_strdup(_("Show debug output on stderr."))));

  lives_free(tmp);
  lives_free(tmp2);

  lives_widget_show_all(alwindow->dialog);
  return alwindow;
}


const lives_special_aspect_t *add_aspect_ratio_button(LiVESSpinButton *sp_width, LiVESSpinButton *sp_height, LiVESBox *box) {
  static lives_param_t aspect_width, aspect_height;

  init_special();

  aspect_width.widgets[0] = (LiVESWidget *)sp_width;
  aspect_height.widgets[0] = (LiVESWidget *)sp_height;
  set_aspect_ratio_widgets(&aspect_width, &aspect_height);
  check_for_special(NULL, &aspect_width, box);
  check_for_special(NULL, &aspect_height, box);

  return paramspecial_get_aspect();
}


static void on_freedom_toggled(LiVESToggleButton *togglebutton, livespointer user_data) {
  LiVESWidget *label = (LiVESWidget *)user_data;
  if (!lives_toggle_button_get_active(togglebutton)) lives_label_set_text(LIVES_LABEL(label), "." LIVES_FILE_EXT_WEBM);
  else lives_label_set_text(LIVES_LABEL(label), "." LIVES_FILE_EXT_MP4);
}


static LiVESWidget *spinbutton_width;
static LiVESWidget *spinbutton_height;
static const lives_special_aspect_t *aspect;

static void utsense(LiVESToggleButton *togglebutton, livespointer user_data) {
  boolean sensitive = (boolean)LIVES_POINTER_TO_INT(user_data);
  if (!lives_toggle_button_get_active(togglebutton)) return;
  lives_widget_set_sensitive(spinbutton_width, sensitive);
  lives_widget_set_sensitive(spinbutton_height, sensitive);
  lives_widget_set_sensitive(aspect->lockbutton, sensitive);
}


// prompt for the following:

// - URL

// save dir

// format selection (free / nonfree) [NEW]

// filename

// approx file size

// update youtube-dl : root passwd

// advanced :: save subs / sub language

boolean run_youtube_dialog(void) {
  LiVESWidget *dialog_vbox;
  LiVESWidget *cancelbutton;
  LiVESWidget *okbutton;
  LiVESWidget *label;
  LiVESWidget *ext_label;
  LiVESWidget *hbox;
  LiVESWidget *dialog;
  LiVESWidget *url_entry;
  LiVESWidget *name_entry;
  LiVESWidget *dir_entry;
  LiVESWidget *checkbutton;
#ifdef ALLOW_NONFREE_CODECS
  LiVESWidget *radiobutton_free;
  LiVESWidget *radiobutton_nonfree;
#endif
  LiVESWidget *radiobutton_approx;
  LiVESWidget *radiobutton_atleast;
  LiVESWidget *radiobutton_atmost;
  LiVESWidget *radiobutton_smallest;
  LiVESWidget *radiobutton_largest;
  LiVESWidget *radiobutton_choose;

  char *fname;

#ifdef ALLOW_NONFREE_CODECS
  LiVESSList *radiobutton_group = NULL;
#endif
  LiVESSList *radiobutton_group2 = NULL;

  LiVESAccelGroup *accel_group = LIVES_ACCEL_GROUP(lives_accel_group_new());

  char *title, *tmp, *tmp2;

  char *dfile = NULL, *url = NULL;

  char dirname[PATH_MAX];

  int response;

  boolean onlyfree = TRUE;

  title = lives_strdup(_("Open Youtube Clip"));

  dialog = lives_standard_dialog_new(title, FALSE, -1, -1);
  lives_signal_handlers_disconnect_by_func(dialog, return_true, NULL);

  lives_free(title);

  lives_window_add_accel_group(LIVES_WINDOW(dialog), accel_group);

  if (prefs->show_gui) {
    lives_window_set_transient_for(LIVES_WINDOW(dialog), LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET));
  }

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(dialog));

  widget_opts.justify = LIVES_JUSTIFY_CENTER;
  label = lives_standard_label_new(
            _("To open a clip from Youtube, LiVES will first download it with youtube-dl.\n"
              "PLEASE MAKE SURE YOU HAVE THE MOST RECENT VERSION OF THAT TOOL INSTALLED !"));

  lives_box_pack_start(LIVES_BOX(dialog_vbox), label, FALSE, FALSE, 0);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, TRUE, TRUE, widget_opts.packing_height * 2);

  add_spring_to_box(LIVES_BOX(hbox), 0);

  // TODO - check for zenity

  checkbutton = lives_standard_check_button_new(_("<--- Auto update (may require your password.)"), FALSE, LIVES_BOX(hbox), NULL);

  if (!capable->has_zenity) {
    lives_widget_set_sensitive(checkbutton, FALSE);
    label = lives_standard_label_new(_(" (REQUIRES ZENITY)."));
    lives_box_pack_start(LIVES_BOX(hbox), label, FALSE, FALSE, 0);
  }

  add_spring_to_box(LIVES_BOX(hbox), 0);

  label = lives_standard_label_new(_("Enter the URL of the clip below.\nE.g: http://www.youtube.com/watch?v=WCR6f6WzjP8"));
  widget_opts.justify = LIVES_JUSTIFY_DEFAULT;

  lives_box_pack_start(LIVES_BOX(dialog_vbox), label, FALSE, FALSE, 0);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, TRUE, TRUE, widget_opts.packing_height * 3);

  url_entry = lives_standard_entry_new(_("Youtube URL : "), "", STD_ENTRY_WIDTH, 32768, LIVES_BOX(hbox), NULL);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, TRUE, FALSE, widget_opts.packing_height);

  dir_entry = lives_standard_direntry_new(_("Save to _Directory : "), mainw->vid_dl_dir,
                                          STD_ENTRY_WIDTH, PATH_MAX, LIVES_BOX(hbox), NULL);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, TRUE, FALSE, widget_opts.packing_height * 3);

  ext_label = lives_standard_label_new("." LIVES_FILE_EXT_WEBM);

#ifdef ALLOW_NONFREE_CODECS
  label = lives_standard_label_new(_("Format selection:"));

  lives_box_pack_start(LIVES_BOX(hbox), label, FALSE, FALSE, widget_opts.packing_width);

  radiobutton_free = lives_standard_radio_button_new((tmp = lives_strdup(_("_Free (eg. vp9 / opus / webm)"))), &radiobutton_group,
                     LIVES_BOX(hbox),
                     (tmp2 = lives_strdup(_("Download clip using Free codecs"))));

  lives_free(tmp);
  lives_free(tmp2);

  add_fill_to_box(LIVES_BOX(hbox));

#endif
  name_entry = lives_standard_entry_new(_("_File Name : "), "", STD_ENTRY_WIDTH / 2, PATH_MAX, LIVES_BOX(hbox), NULL);

  lives_box_pack_start(LIVES_BOX(hbox), ext_label, FALSE, FALSE, 0);

#ifdef ALLOW_NONFREE_CODECS
  //
  hbox = lives_hbox_new(FALSE, 0);

  lives_widget_show_all(dialog);
  lives_widget_context_update();
  align_horizontal(hbox, LIVES_VBOX(dialog_vbox), radiobutton_free);

  radiobutton_nonfree = lives_standard_radio_button_new((tmp = lives_strdup(_("_Non-free (eg. h264 / aac / mp4)"))), &radiobutton_group,
                        LIVES_BOX(hbox),
                        (tmp2 = lives_strdup(_("Download clip using non-free codecs"))));
  lives_free(tmp);
  lives_free(tmp2);

  lives_signal_connect(LIVES_GUI_OBJECT(radiobutton_nonfree), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(on_freedom_toggled),
                       (livespointer)ext_label);
#endif

  add_hsep_to_box(LIVES_BOX(dialog_vbox));

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, TRUE, FALSE, widget_opts.packing_height);

  label = lives_standard_label_new(_("Desired frame size:"));
  lives_box_pack_start(LIVES_BOX(hbox), label, FALSE, FALSE, widget_opts.packing_width);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, TRUE, FALSE, widget_opts.packing_height);

  radiobutton_approx = lives_standard_radio_button_new((tmp = lives_strdup(_("- Approximately:"))), &radiobutton_group2,
                       LIVES_BOX(hbox),
                       (tmp2 = lives_strdup(_("Download the closest to this size"))));

  lives_free(tmp);
  lives_free(tmp2);

  lives_signal_connect(LIVES_GUI_OBJECT(radiobutton_approx), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(utsense),
                       LIVES_INT_TO_POINTER(TRUE));

  radiobutton_atleast = lives_standard_radio_button_new((tmp = lives_strdup(_("- At _least"))), &radiobutton_group2,
                        LIVES_BOX(hbox),
                        (tmp2 = lives_strdup(_("Frame size should be at least this size"))));
  lives_free(tmp);
  lives_free(tmp2);

  lives_signal_connect(LIVES_GUI_OBJECT(radiobutton_atleast), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(utsense),
                       LIVES_INT_TO_POINTER(TRUE));

  radiobutton_atmost = lives_standard_radio_button_new((tmp = lives_strdup(_("- At _most:"))), &radiobutton_group2,
                       LIVES_BOX(hbox),
                       (tmp2 = lives_strdup(_("Frame size should be at most this size"))));
  lives_free(tmp);
  lives_free(tmp2);

  lives_signal_connect(LIVES_GUI_OBJECT(radiobutton_atmost), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(utsense),
                       LIVES_INT_TO_POINTER(TRUE));

  add_fill_to_box(LIVES_BOX(hbox));

  spinbutton_width = lives_standard_spin_button_new(_("_Width"), CURRENT_CLIP_HAS_VIDEO ? cfile->hsize : DEF_GEN_WIDTH,
                     4., 100000., 4., 16., 0., LIVES_BOX(hbox), NULL);


  spinbutton_height = lives_standard_spin_button_new(_("X        _Height"), CURRENT_CLIP_HAS_VIDEO ? cfile->vsize : DEF_GEN_HEIGHT,
                      4., 100000., 4., 16., 0., LIVES_BOX(hbox), NULL);

  label = lives_standard_label_new(_("    pixels"));
  lives_box_pack_start(LIVES_BOX(hbox), label, FALSE, FALSE, 0);

  // add "aspectratio" widget
  aspect = add_aspect_ratio_button(LIVES_SPIN_BUTTON(spinbutton_width), LIVES_SPIN_BUTTON(spinbutton_height), LIVES_BOX(hbox));
  lives_widget_set_no_show_all(lives_widget_get_parent(aspect->label), TRUE);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, TRUE, FALSE, widget_opts.packing_height);

  label = lives_standard_label_new(_("or:"));
  lives_box_pack_start(LIVES_BOX(hbox), label, FALSE, FALSE, widget_opts.packing_width);

  // TODO - add aspect button

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, TRUE, FALSE, widget_opts.packing_height);

  radiobutton_smallest = lives_standard_radio_button_new((tmp = lives_strdup(_("- The _smallest"))), &radiobutton_group2,
                         LIVES_BOX(hbox),
                         (tmp2 = lives_strdup(_("Download the lowest resolution available"))));

  lives_free(tmp);
  lives_free(tmp2);


  lives_signal_connect(LIVES_GUI_OBJECT(radiobutton_smallest), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(utsense),
                       LIVES_INT_TO_POINTER(FALSE));

  radiobutton_largest = lives_standard_radio_button_new((tmp = lives_strdup(_("- The _largest"))), &radiobutton_group2,
                        LIVES_BOX(hbox),
                        (tmp2 = lives_strdup(_("Download the highest resolution available"))));

  lives_free(tmp);
  lives_free(tmp2);

  lives_signal_connect(LIVES_GUI_OBJECT(radiobutton_largest), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(utsense),
                       LIVES_INT_TO_POINTER(FALSE));

  radiobutton_choose = lives_standard_radio_button_new((tmp = lives_strdup(_("- Let me choose (opens in new window)..."))),
                       &radiobutton_group2,
                       LIVES_BOX(hbox),
                       (tmp2 = lives_strdup(_("Choose the resolution from a list (opens in new window)"))));

  lives_free(tmp);
  lives_free(tmp2);

  lives_signal_connect(LIVES_GUI_OBJECT(radiobutton_choose), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(utsense),
                       LIVES_INT_TO_POINTER(FALSE));

  ///////

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, TRUE, FALSE, widget_opts.packing_height * 2);

  add_spring_to_box(LIVES_BOX(hbox), 0);
  lives_standard_expander_new(_("_Other options (e.g audio, subtitles)..."), LIVES_BOX(hbox), NULL);
  add_spring_to_box(LIVES_BOX(hbox), 0);

  // TODO - add other options

  ///////

  cancelbutton = lives_standard_button_new_from_stock(LIVES_STOCK_CANCEL, NULL);
  lives_dialog_add_action_widget(LIVES_DIALOG(dialog), cancelbutton, LIVES_RESPONSE_CANCEL);
  lives_widget_set_can_focus_and_default(cancelbutton);

  okbutton = lives_standard_button_new_from_stock(LIVES_STOCK_OK, NULL);
  lives_dialog_add_action_widget(LIVES_DIALOG(dialog), okbutton, LIVES_RESPONSE_OK);
  lives_widget_set_can_focus_and_default(okbutton);
  lives_widget_grab_default_special(okbutton);

  lives_signal_connect(LIVES_GUI_OBJECT(cancelbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                       LIVES_GUI_CALLBACK(lives_general_button_clicked),
                       locw);

  lives_widget_add_accelerator(cancelbutton, LIVES_WIDGET_CLICKED_SIGNAL, accel_group,
                               LIVES_KEY_Escape, (LiVESXModifierType)0, (LiVESAccelFlags)0);

  lives_widget_show_all(dialog);

  while (1) {
    response = lives_dialog_run(LIVES_DIALOG(dialog));
    if (response == LIVES_RESPONSE_CANCEL) {
      mainw->cancelled = CANCEL_USER;
      return FALSE;
    }

    ///////

    if (!strlen(lives_entry_get_text(LIVES_ENTRY(name_entry)))) {
      do_error_dialog_with_check_transient(_("Please enter the name of the file to save the clip as.\n"), TRUE, 0, LIVES_WINDOW(dialog));
      continue;
    }

    url = lives_strdup(lives_entry_get_text(LIVES_ENTRY(locw->entry)));

    if (!strlen(url)) {
      lives_free(url);
      do_error_dialog_with_check_transient(_("Please enter a valid URL for the download.\n"), TRUE, 0, LIVES_WINDOW(dialog));
      continue;
    }

    fname  = ensure_extension(lives_entry_get_text(LIVES_ENTRY(name_entry)), onlyfree ? LIVES_FILE_EXT_WEBM : LIVES_FILE_EXT_MP4);
    lives_snprintf(dirname, PATH_MAX, "%s", lives_entry_get_text(LIVES_ENTRY(dir_entry)));
    ensure_isdir(dirname);
    dfile = lives_build_filename(dirname, fname, NULL);

    if (!check_file(dfile, TRUE)) {
      char *msg = lives_strdup_printf(_("Unable to write to the file\n%s\nPlease check the directory permissions and try again, or Cancel.\n"),
                                      dfile);
      lives_free(fname);
      lives_free(url);
      lives_free(dfile);
      do_error_dialog_with_check_transient(msg, TRUE, 0, LIVES_WINDOW(dialog));
      lives_free(msg);
      continue;
    }
    break;
  };

  lives_snprintf(mainw->vid_dl_dir, PATH_MAX, "%s", dirname);

  lives_widget_destroy(dialog);

  lives_widget_context_update();

  mainw->error = FALSE;
  d_print(_("Downloading %s to %s..."), url, dfile);

  /*  options.output = dfile;
      options.url = url;
      options.nonfree = nonfree; // TODO
      options.sizetype = sizetype; // TODO
      options.width = width; // TODO
      options.height = height; // TODO
      options.getsubs = getsubs; // TODO
      options.sublang = sublang; // TODO
      options.update = update; // TODO

      return options;*/

  return TRUE;
}
