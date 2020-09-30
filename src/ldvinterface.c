// ldvinterface.c
// LiVES
// (c) G. Finch 2006-2018 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#include "main.h"
#include "ldvcallbacks.h"
#include "ldvinterface.h"
#include "callbacks.h"

struct _dvgrabw *create_camwindow(s_cam *cam, int type) {
  LiVESWidget *hbuttonbox1;
  LiVESWidget *button3;
  LiVESWidget *button4;
  LiVESWidget *vbox;
  LiVESWidget *hbox;
  LiVESWidget *label;

  char *tmp;

  struct _dvgrabw *dvgrabw = (struct _dvgrabw *)lives_malloc(sizeof(struct _dvgrabw));

  dvgrabw->filename = NULL;

  dvgrabw->dialog = lives_standard_dialog_new(_("DVGrab"), FALSE, -1, -1);
  dvgrabw->playing = FALSE;

  vbox = lives_dialog_get_content_area(LIVES_DIALOG(dvgrabw->dialog));

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

  dvgrabw->dirname = lives_filename_to_utf8((tmp = lives_get_current_dir()), -1, NULL, NULL, NULL);
  dvgrabw->dirent = lives_standard_direntry_new(_("Save directory:"), dvgrabw->dirname, LONG_ENTRY_WIDTH, PATH_MAX,
                    LIVES_BOX(hbox), NULL);
  lives_free(tmp);

  //////////////////

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

  dvgrabw->filent = lives_standard_entry_new(_("File_name:"), type == CAM_FORMAT_DV ? "dvgrab-" : "hdvgrab-", -1, -1,
                    LIVES_BOX(hbox), NULL);

  if (type == CAM_FORMAT_DV) label = lives_standard_label_new("%d.dv");
  else label = lives_standard_label_new("%d.mpg");
  lives_box_pack_start(LIVES_BOX(hbox), label, FALSE, FALSE, 0);

  label = lives_standard_label_new(_("(files will not be overwritten)"));
  lives_box_pack_end(LIVES_BOX(hbox), label, FALSE, FALSE, widget_opts.packing_width);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

  dvgrabw->split = lives_standard_check_button_new(_("_Split into scenes"), FALSE, LIVES_BOX(hbox), NULL);

  // TODO - widget_opts.editable
  dvgrabw->status_entry = lives_entry_new();

  lives_box_pack_start(LIVES_BOX(vbox), dvgrabw->status_entry, FALSE, FALSE, widget_opts.packing_height);
  lives_entry_set_text(LIVES_ENTRY(dvgrabw->status_entry), _("Status: Ready"));
  lives_editable_set_editable(LIVES_EDITABLE(dvgrabw->status_entry), FALSE);

  add_hsep_to_box(LIVES_BOX(vbox));

  hbuttonbox1 = lives_hbutton_box_new();

  lives_box_pack_start(LIVES_BOX(vbox), hbuttonbox1, FALSE, FALSE, widget_opts.packing_height);

  // TODO: use lives_dialog_add_button_from_stock()

  button3 = lives_standard_button_new_from_stock(LIVES_STOCK_MEDIA_REWIND,  LIVES_STOCK_LABEL_MEDIA_REWIND,
            DEF_BUTTON_WIDTH, DEF_BUTTON_HEIGHT);

  lives_container_add(LIVES_CONTAINER(hbuttonbox1), button3);
  lives_widget_set_can_focus(button3, TRUE);

  button4 = lives_standard_button_new_from_stock(LIVES_STOCK_MEDIA_FORWARD, LIVES_STOCK_LABEL_MEDIA_FORWARD,
            DEF_BUTTON_WIDTH, DEF_BUTTON_HEIGHT);

  lives_container_add(LIVES_CONTAINER(hbuttonbox1), button4);
  lives_widget_set_can_focus(button4, TRUE);

  dvgrabw->stop = lives_standard_button_new_from_stock(LIVES_STOCK_MEDIA_STOP, LIVES_STOCK_LABEL_MEDIA_STOP,
                  DEF_BUTTON_WIDTH, DEF_BUTTON_HEIGHT);

  lives_container_add(LIVES_CONTAINER(hbuttonbox1), dvgrabw->stop);
  lives_widget_set_can_focus_and_default(dvgrabw->stop);
  lives_widget_set_sensitive(dvgrabw->stop, FALSE);

  dvgrabw->play = lives_standard_button_new_from_stock(LIVES_STOCK_MEDIA_PLAY, LIVES_STOCK_LABEL_MEDIA_PLAY,
                  DEF_BUTTON_WIDTH, DEF_BUTTON_HEIGHT);

  lives_container_add(LIVES_CONTAINER(hbuttonbox1), dvgrabw->play);
  lives_widget_set_can_focus_and_default(dvgrabw->play);

  dvgrabw->grab = lives_standard_button_new_from_stock(LIVES_STOCK_MEDIA_RECORD,  _("_Grab"),
                  DEF_BUTTON_WIDTH, DEF_BUTTON_HEIGHT);

  lives_container_add(LIVES_CONTAINER(hbuttonbox1), dvgrabw->grab);
  lives_widget_set_can_focus_and_default(dvgrabw->grab);

  label = lives_standard_label_new(
            _("\nUse this tool to control your camera and grab clips.\n"
              "After grabbing your clips, you can close this window \nand then load them into LiVES.\n"));
  lives_box_pack_start(LIVES_BOX(vbox), label, FALSE, FALSE, widget_opts.packing_height * 4);

  dvgrabw->quit =
    lives_dialog_add_button_from_stock(LIVES_DIALOG(dvgrabw->dialog),
                                       LIVES_STOCK_CLOSE, LIVES_STOCK_LABEL_CLOSE_WINDOW,
                                       LIVES_RESPONSE_ACCEPT);

  lives_widget_set_can_focus_and_default(dvgrabw->quit);

  //////////////////////////////////////////////////////////////////////////////////////////

  lives_signal_sync_connect(button3, LIVES_WIDGET_CLICKED_SIGNAL, LIVES_GUI_CALLBACK(on_camrew_clicked), (livespointer)cam);
  lives_signal_sync_connect(button4, LIVES_WIDGET_CLICKED_SIGNAL, LIVES_GUI_CALLBACK(on_camff_clicked), (livespointer)cam);
  lives_signal_sync_connect(dvgrabw->stop, LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_camstop_clicked), (livespointer)cam);
  lives_signal_sync_connect(dvgrabw->play, LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_camplay_clicked), (livespointer)cam);
  lives_signal_sync_connect(dvgrabw->grab, LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_camgrab_clicked), (livespointer)cam);
  lives_signal_sync_connect(dvgrabw->quit, LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_camquit_clicked), (livespointer)cam);
  return dvgrabw;
}

