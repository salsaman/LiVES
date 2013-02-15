// ldvinterface.c
// LiVES
// (c) G. Finch 2006-2013 <salsaman@xs4all.nl,salsaman@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#include "main.h"
#include "support.h"
#include "ldvcallbacks.h"
#include "ldvinterface.h"


struct _dvgrabw *create_camwindow (s_cam *cam, gint type)
{
  GtkWidget *hbuttonbox1;
  GtkWidget *hbuttonbox2;
  GtkWidget *button3;
  GtkWidget *button4;
  GtkWidget *buttond;
  GtkWidget *image;
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *label;
  GtkWidget *hseparator;
  GtkWidget *direntry;

  gchar *tmp;

  struct _dvgrabw * dvgrabw=(struct _dvgrabw *)g_malloc(sizeof(struct _dvgrabw));

  dvgrabw->filename=NULL;

  dvgrabw->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  dvgrabw->playing=FALSE;
  gtk_window_set_title (GTK_WINDOW (dvgrabw->window), (_("LiVES: DVGrab")));
  gtk_container_set_border_width (GTK_CONTAINER (dvgrabw->window), 20);

  gtk_window_set_modal (GTK_WINDOW (dvgrabw->window), TRUE);
  gtk_window_set_position (GTK_WINDOW (dvgrabw->window), GTK_WIN_POS_CENTER_ALWAYS);
  if (palette->style&STYLE_1) {
    lives_widget_set_bg_color(dvgrabw->window, GTK_STATE_NORMAL, &palette->normal_back);
  }

  if (prefs->show_gui) {
    if (mainw->multitrack==NULL) gtk_window_set_transient_for(GTK_WINDOW(dvgrabw->window),GTK_WINDOW(mainw->LiVES));
    else gtk_window_set_transient_for(GTK_WINDOW(dvgrabw->window),GTK_WINDOW(mainw->multitrack->window));
  }

  vbox=lives_vbox_new(FALSE,10);
  gtk_container_add (GTK_CONTAINER (dvgrabw->window), vbox);

  hbox = lives_hbox_new (FALSE,10);
  gtk_box_pack_start(GTK_BOX(vbox),hbox,FALSE,FALSE,10);


  // TODO - lives_standard_dir_entry_new
  label=gtk_label_new_with_mnemonic(_("Save _directory :"));
  gtk_box_pack_start(GTK_BOX(hbox),label,FALSE,FALSE,10);
  if (palette->style&STYLE_1) {
    lives_widget_set_fg_color(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  direntry=gtk_entry_new();
  gtk_box_pack_start(GTK_BOX(hbox),direntry,TRUE,TRUE,0);
  gtk_entry_set_text(GTK_ENTRY(direntry),(tmp=g_filename_to_utf8(g_get_current_dir(),-1,NULL,NULL,NULL)));
  g_free(tmp);
  lives_entry_set_editable(LIVES_ENTRY(direntry),FALSE);

  buttond = gtk_file_chooser_button_new(_("Save directory"),GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
  gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(buttond),g_get_current_dir());
  gtk_label_set_mnemonic_widget(GTK_LABEL(label),buttond);

  gtk_box_pack_start(GTK_BOX(hbox),buttond,FALSE,FALSE,10);
  lives_widget_set_can_focus_and_default (buttond);


  //////////////////

  hbox = lives_hbox_new (FALSE,10);
  gtk_box_pack_start(GTK_BOX(vbox),hbox,FALSE,FALSE,10);

  dvgrabw->filent=lives_standard_entry_new(_("File_name:"),TRUE,type==CAM_FORMAT_DV?"dvgrab-":"hdvgrab-",-1,-1,LIVES_BOX(hbox),NULL);

  if (type==CAM_FORMAT_DV) label=lives_standard_label_new("%d.dv");
  else label=lives_standard_label_new("%d.mpg");
  gtk_box_pack_start(GTK_BOX(hbox),label,FALSE,FALSE,0);

  label=lives_standard_label_new(_("(files will not be overwritten)"));
  gtk_box_pack_end(GTK_BOX(hbox),label,FALSE,FALSE,0);

  hbox = lives_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 10);

  dvgrabw->split=lives_standard_check_button_new(_("_Split into scenes"),FALSE,LIVES_BOX(hbox),NULL);


  // TODO - widget_opts.editable
  dvgrabw->status_entry=gtk_entry_new();

  gtk_box_pack_start(GTK_BOX(vbox),dvgrabw->status_entry,FALSE,FALSE,0);
  gtk_entry_set_text(GTK_ENTRY(dvgrabw->status_entry),_("Status: Ready"));
  gtk_editable_set_editable (GTK_EDITABLE(dvgrabw->status_entry),FALSE);


  hseparator = lives_hseparator_new ();
  gtk_box_pack_start (GTK_BOX (vbox), hseparator, FALSE, TRUE, 10);

  hbuttonbox1 = gtk_hbutton_box_new ();

  gtk_box_pack_start(GTK_BOX(vbox),hbuttonbox1,FALSE,FALSE,0);

  button3 = gtk_button_new_from_stock(GTK_STOCK_MEDIA_REWIND);
  gtk_container_add (GTK_CONTAINER (hbuttonbox1), button3);
  lives_widget_set_can_focus (button3,TRUE);

  button4 = gtk_button_new_from_stock(GTK_STOCK_MEDIA_FORWARD);
  gtk_container_add (GTK_CONTAINER (hbuttonbox1), button4);
  lives_widget_set_can_focus (button4,TRUE);

  dvgrabw->stop = gtk_button_new_from_stock (GTK_STOCK_MEDIA_STOP);
  gtk_container_add (GTK_CONTAINER (hbuttonbox1), dvgrabw->stop);
  lives_widget_set_can_focus_and_default (dvgrabw->stop);
  gtk_widget_set_sensitive(dvgrabw->stop,FALSE);

  dvgrabw->play = gtk_button_new_from_stock (GTK_STOCK_MEDIA_PLAY);
  gtk_container_add (GTK_CONTAINER (hbuttonbox1), dvgrabw->play);
  lives_widget_set_can_focus_and_default (dvgrabw->play);
  gtk_button_set_use_stock(GTK_BUTTON(dvgrabw->play),TRUE);

  dvgrabw->grab = gtk_button_new_from_stock (GTK_STOCK_MEDIA_RECORD);
  gtk_container_add (GTK_CONTAINER (hbuttonbox1), dvgrabw->grab);
  lives_widget_set_can_focus_and_default (dvgrabw->grab);

  image=gtk_image_new_from_stock(GTK_STOCK_MEDIA_RECORD,GTK_ICON_SIZE_BUTTON);
  gtk_button_set_label(GTK_BUTTON(dvgrabw->grab),_("_Grab"));
  gtk_button_set_image(GTK_BUTTON(dvgrabw->grab),image);

  label=lives_standard_label_new(_("\nUse this tool to control your camera and grab clips.\nAfter grabbing your clips, you can close this window \nand then load them into LiVES.\n"));
  gtk_box_pack_start(GTK_BOX(vbox),label,FALSE,FALSE,40);


  hbuttonbox2 = gtk_hbutton_box_new ();
  gtk_box_pack_start(GTK_BOX(vbox),hbuttonbox2,FALSE,FALSE,0);

  dvgrabw->quit = gtk_button_new_from_stock (GTK_STOCK_CLOSE);
  gtk_container_add (GTK_CONTAINER (hbuttonbox2), dvgrabw->quit);
  lives_widget_set_can_focus_and_default (dvgrabw->quit);

  image=gtk_image_new_from_stock(GTK_STOCK_CLOSE,GTK_ICON_SIZE_BUTTON);
  gtk_button_set_label(GTK_BUTTON(dvgrabw->quit),_("_Close Window"));
  gtk_button_set_image(GTK_BUTTON(dvgrabw->quit),image);

  //////////////////////////////////////////////////////////////////////////////////////////

  g_signal_connect (button3, "clicked",G_CALLBACK (on_camrew_clicked),(gpointer)cam);
  g_signal_connect (button4, "clicked",G_CALLBACK (on_camff_clicked),(gpointer)cam);
  g_signal_connect (dvgrabw->stop, "clicked",G_CALLBACK (on_camstop_clicked),(gpointer)cam);
  g_signal_connect (dvgrabw->play, "clicked",G_CALLBACK (on_camplay_clicked),(gpointer)cam);
  g_signal_connect (dvgrabw->grab, "clicked",G_CALLBACK (on_camgrab_clicked),(gpointer)cam);
  g_signal_connect (dvgrabw->quit, "clicked",G_CALLBACK (on_camquit_clicked),(gpointer)cam);
  g_signal_connect (buttond, "current_folder_changed",G_CALLBACK (on_camfile_clicked),(gpointer)direntry);

  g_signal_connect (GTK_OBJECT (dvgrabw->window), "delete_event",
		    G_CALLBACK (on_camdelete_event),
		    (gpointer)cam);

  gtk_widget_show_all(dvgrabw->window);
  
  dvgrabw->dirname=NULL;

  return dvgrabw;
}

