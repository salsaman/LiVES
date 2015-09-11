// ldvinterface.c
// LiVES
// (c) G. Finch 2006-2013 <salsaman@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#include "main.h"
#include "support.h"
#include "ldvcallbacks.h"
#include "ldvinterface.h"


struct _dvgrabw *create_camwindow(s_cam *cam, int type) {
  LiVESWidget *hbuttonbox1;
  LiVESWidget *hbuttonbox2;
  LiVESWidget *button3;
  LiVESWidget *button4;
  LiVESWidget *buttond;
  LiVESWidget *image;
  LiVESWidget *vbox;
  LiVESWidget *hbox;
  LiVESWidget *label;

  struct _dvgrabw *dvgrabw=(struct _dvgrabw *)lives_malloc(sizeof(struct _dvgrabw));

  dvgrabw->filename=NULL;

  dvgrabw->dialog = lives_standard_dialog_new(_("LiVES: DVGrab"),FALSE,-1,-1);
  dvgrabw->playing=FALSE;

  lives_container_set_border_width(LIVES_CONTAINER(dvgrabw->dialog), widget_opts.border_width*2);

  if (prefs->show_gui) {
    if (mainw->multitrack==NULL) lives_window_set_transient_for(LIVES_WINDOW(dvgrabw->dialog),LIVES_WINDOW(mainw->LiVES));
    else lives_window_set_transient_for(LIVES_WINDOW(dvgrabw->dialog),LIVES_WINDOW(mainw->multitrack->window));
  }

  vbox=lives_dialog_get_content_area(LIVES_DIALOG(dvgrabw->dialog));

  hbox = lives_hbox_new(FALSE,0);
  lives_box_pack_start(LIVES_BOX(vbox),hbox,FALSE,FALSE,widget_opts.packing_height);


  buttond = lives_standard_file_button_new(TRUE,NULL);

  label=lives_standard_label_new_with_mnemonic(_("Save _directory :"),buttond);
  lives_box_pack_start(LIVES_BOX(hbox),label,FALSE,FALSE,widget_opts.packing_width);

  lives_box_pack_start(LIVES_BOX(hbox),buttond,FALSE,FALSE,widget_opts.packing_width);


  dvgrabw->dirname=lives_filename_to_utf8(lives_get_current_dir(),-1,NULL,NULL,NULL);
  dvgrabw->dirent=lives_standard_entry_new(NULL,FALSE,dvgrabw->dirname,-1,PATH_MAX,
					   LIVES_BOX(hbox),NULL);


  lives_signal_connect(buttond, LIVES_WIDGET_CLICKED_SIGNAL, LIVES_GUI_CALLBACK(on_filesel_button_clicked), (livespointer)dvgrabw->dirent);


  //////////////////

  hbox = lives_hbox_new(FALSE,0);
  lives_box_pack_start(LIVES_BOX(vbox),hbox,FALSE,FALSE,widget_opts.packing_height);

  dvgrabw->filent=lives_standard_entry_new(_("File_name:"),TRUE,type==CAM_FORMAT_DV?"dvgrab-":"hdvgrab-",-1,-1,LIVES_BOX(hbox),NULL);

  if (type==CAM_FORMAT_DV) label=lives_standard_label_new("%d.dv");
  else label=lives_standard_label_new("%d.mpg");
  lives_box_pack_start(LIVES_BOX(hbox),label,FALSE,FALSE,0);

  label=lives_standard_label_new(_("(files will not be overwritten)"));
  lives_box_pack_end(LIVES_BOX(hbox),label,FALSE,FALSE,widget_opts.packing_width);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

  dvgrabw->split=lives_standard_check_button_new(_("_Split into scenes"),TRUE,LIVES_BOX(hbox),NULL);


  // TODO - widget_opts.editable
  dvgrabw->status_entry=lives_entry_new();

  lives_box_pack_start(LIVES_BOX(vbox),dvgrabw->status_entry,FALSE,FALSE,widget_opts.packing_height);
  lives_entry_set_text(LIVES_ENTRY(dvgrabw->status_entry),_("Status: Ready"));
  lives_editable_set_editable(LIVES_EDITABLE(dvgrabw->status_entry),FALSE);

  add_hsep_to_box(LIVES_BOX(vbox));

  hbuttonbox1 = lives_hbutton_box_new();

  lives_box_pack_start(LIVES_BOX(vbox),hbuttonbox1,FALSE,FALSE,widget_opts.packing_height);

  button3 = lives_button_new_from_stock(LIVES_STOCK_MEDIA_REWIND);

  lives_container_add(LIVES_CONTAINER(hbuttonbox1), button3);
  lives_widget_set_can_focus(button3,TRUE);

  button4 = lives_button_new_from_stock(LIVES_STOCK_MEDIA_FORWARD);

  lives_container_add(LIVES_CONTAINER(hbuttonbox1), button4);
  lives_widget_set_can_focus(button4,TRUE);

  dvgrabw->stop = lives_button_new_from_stock(LIVES_STOCK_MEDIA_STOP);

  lives_container_add(LIVES_CONTAINER(hbuttonbox1), dvgrabw->stop);
  lives_widget_set_can_focus_and_default(dvgrabw->stop);
  lives_widget_set_sensitive(dvgrabw->stop,FALSE);

  dvgrabw->play = lives_button_new_from_stock(LIVES_STOCK_MEDIA_PLAY);

  lives_container_add(LIVES_CONTAINER(hbuttonbox1), dvgrabw->play);
  lives_widget_set_can_focus_and_default(dvgrabw->play);

  dvgrabw->grab = lives_button_new_from_stock(LIVES_STOCK_MEDIA_RECORD);

  lives_container_add(LIVES_CONTAINER(hbuttonbox1), dvgrabw->grab);
  lives_widget_set_can_focus_and_default(dvgrabw->grab);

  image = lives_image_new_from_stock(LIVES_STOCK_MEDIA_RECORD,LIVES_ICON_SIZE_BUTTON);

  lives_button_set_label(LIVES_BUTTON(dvgrabw->grab),_("_Grab"));
  lives_button_set_image(LIVES_BUTTON(dvgrabw->grab),image);

  label=lives_standard_label_new(
          _("\nUse this tool to control your camera and grab clips.\nAfter grabbing your clips, you can close this window \nand then load them into LiVES.\n"));
  lives_box_pack_start(LIVES_BOX(vbox),label,FALSE,FALSE,widget_opts.packing_height*4);


  hbuttonbox2 = lives_hbutton_box_new();
  lives_box_pack_start(LIVES_BOX(vbox),hbuttonbox2,FALSE,FALSE,widget_opts.packing_height);

  dvgrabw->quit = lives_button_new_from_stock(LIVES_STOCK_CLOSE);

  lives_container_add(LIVES_CONTAINER(hbuttonbox2), dvgrabw->quit);
  lives_widget_set_can_focus_and_default(dvgrabw->quit);

  image=lives_image_new_from_stock(LIVES_STOCK_CLOSE,LIVES_ICON_SIZE_BUTTON);
  lives_button_set_label(LIVES_BUTTON(dvgrabw->quit),_("_Close Window"));
  lives_button_set_image(LIVES_BUTTON(dvgrabw->quit),image);

  //////////////////////////////////////////////////////////////////////////////////////////

  lives_signal_connect(button3, LIVES_WIDGET_CLICKED_SIGNAL,LIVES_GUI_CALLBACK(on_camrew_clicked),(livespointer)cam);
  lives_signal_connect(button4, LIVES_WIDGET_CLICKED_SIGNAL,LIVES_GUI_CALLBACK(on_camff_clicked),(livespointer)cam);
  lives_signal_connect(dvgrabw->stop, LIVES_WIDGET_CLICKED_SIGNAL,LIVES_GUI_CALLBACK(on_camstop_clicked),(livespointer)cam);
  lives_signal_connect(dvgrabw->play, LIVES_WIDGET_CLICKED_SIGNAL,LIVES_GUI_CALLBACK(on_camplay_clicked),(livespointer)cam);
  lives_signal_connect(dvgrabw->grab, LIVES_WIDGET_CLICKED_SIGNAL,LIVES_GUI_CALLBACK(on_camgrab_clicked),(livespointer)cam);
  lives_signal_connect(dvgrabw->quit, LIVES_WIDGET_CLICKED_SIGNAL,LIVES_GUI_CALLBACK(on_camquit_clicked),(livespointer)cam);


  lives_widget_show_all(dvgrabw->dialog);

  return dvgrabw;
}

