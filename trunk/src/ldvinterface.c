// ldvinterface.c
// LiVES
// (c) G. Finch 2006-2013 <salsaman@gmail.com>
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
  GtkWidget *direntry;

  gchar *tmp;

  struct _dvgrabw * dvgrabw=(struct _dvgrabw *)g_malloc(sizeof(struct _dvgrabw));

  dvgrabw->filename=NULL;

  dvgrabw->dialog = lives_standard_dialog_new (_("LiVES: DVGrab"),FALSE);
  dvgrabw->playing=FALSE;

  lives_container_set_border_width (GTK_CONTAINER (dvgrabw->dialog), widget_opts.border_width*2);

  if (prefs->show_gui) {
    if (mainw->multitrack==NULL) gtk_window_set_transient_for(GTK_WINDOW(dvgrabw->dialog),GTK_WINDOW(mainw->LiVES));
    else gtk_window_set_transient_for(GTK_WINDOW(dvgrabw->dialog),GTK_WINDOW(mainw->multitrack->window));
  }

  vbox=lives_dialog_get_content_area(LIVES_DIALOG(dvgrabw->dialog));

  hbox = lives_hbox_new (FALSE,0);
  lives_box_pack_start(GTK_BOX(vbox),hbox,FALSE,FALSE,widget_opts.packing_height);


  buttond = lives_standard_file_button_new(TRUE,NULL);

  label=lives_standard_label_new_with_mnemonic(_("Save _directory :"),buttond);
  lives_box_pack_start(GTK_BOX(hbox),label,FALSE,FALSE,widget_opts.packing_width);

  lives_box_pack_start(GTK_BOX(hbox),buttond,FALSE,FALSE,widget_opts.packing_width);

  direntry=lives_standard_entry_new(NULL,FALSE,(tmp=g_filename_to_utf8(g_get_current_dir(),-1,NULL,NULL,NULL)),-1,PATH_MAX,
				    LIVES_BOX(hbox),NULL);

  g_free(tmp);


  g_signal_connect(buttond, "clicked", G_CALLBACK (on_filesel_button_clicked), (gpointer)direntry);


  //////////////////

  hbox = lives_hbox_new (FALSE,0);
  lives_box_pack_start(GTK_BOX(vbox),hbox,FALSE,FALSE,widget_opts.packing_height);

  dvgrabw->filent=lives_standard_entry_new(_("File_name:"),TRUE,type==CAM_FORMAT_DV?"dvgrab-":"hdvgrab-",-1,-1,LIVES_BOX(hbox),NULL);

  if (type==CAM_FORMAT_DV) label=lives_standard_label_new("%d.dv");
  else label=lives_standard_label_new("%d.mpg");
  lives_box_pack_start(GTK_BOX(hbox),label,FALSE,FALSE,0);

  label=lives_standard_label_new(_("(files will not be overwritten)"));
  lives_box_pack_end(GTK_BOX(hbox),label,FALSE,FALSE,widget_opts.packing_width);

  hbox = lives_hbox_new (FALSE, 0);
  lives_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

  dvgrabw->split=lives_standard_check_button_new(_("_Split into scenes"),TRUE,LIVES_BOX(hbox),NULL);


  // TODO - widget_opts.editable
  dvgrabw->status_entry=gtk_entry_new();

  lives_box_pack_start(GTK_BOX(vbox),dvgrabw->status_entry,FALSE,FALSE,widget_opts.packing_height);
  lives_entry_set_text(GTK_ENTRY(dvgrabw->status_entry),_("Status: Ready"));
  gtk_editable_set_editable (GTK_EDITABLE(dvgrabw->status_entry),FALSE);

  add_hsep_to_box(LIVES_BOX(vbox));

  hbuttonbox1 = lives_hbutton_box_new ();

  lives_box_pack_start(GTK_BOX(vbox),hbuttonbox1,FALSE,FALSE,widget_opts.packing_height);

#if GTK_CHECK_VERSION(2,6,0)
  button3 = gtk_button_new_from_stock (GTK_STOCK_MEDIA_REWIND);
#else
  button3 = gtk_button_new_from_stock (GTK_STOCK_GO_BACK);
#endif

  lives_container_add (GTK_CONTAINER (hbuttonbox1), button3);
  lives_widget_set_can_focus (button3,TRUE);

#if GTK_CHECK_VERSION(2,6,0)
  button4 = gtk_button_new_from_stock (GTK_STOCK_MEDIA_FORWARD);
#else
  button4 = gtk_button_new_from_stock (GTK_STOCK_GO_FORWARD);
#endif
  lives_container_add (GTK_CONTAINER (hbuttonbox1), button4);
  lives_widget_set_can_focus (button4,TRUE);

#if GTK_CHECK_VERSION(2,6,0)
  dvgrabw->stop = gtk_button_new_from_stock (GTK_STOCK_MEDIA_STOP);
#else
  dvgrabw->stop = gtk_button_new_from_stock (GTK_STOCK_STOP);
#endif
  lives_container_add (GTK_CONTAINER (hbuttonbox1), dvgrabw->stop);
  lives_widget_set_can_focus_and_default (dvgrabw->stop);
  lives_widget_set_sensitive(dvgrabw->stop,FALSE);

#if GTK_CHECK_VERSION(2,6,0)
  dvgrabw->play = gtk_button_new_from_stock (GTK_STOCK_MEDIA_PLAY);
#else
  dvgrabw->play = gtk_button_new_from_stock (GTK_STOCK_GO_FORWARD);
#endif
  lives_container_add (GTK_CONTAINER (hbuttonbox1), dvgrabw->play);
  lives_widget_set_can_focus_and_default (dvgrabw->play);
  gtk_button_set_use_stock(GTK_BUTTON(dvgrabw->play),TRUE);

#if GTK_CHECK_VERSION(2,6,0)
  dvgrabw->grab = gtk_button_new_from_stock (GTK_STOCK_MEDIA_RECORD);
#else
  dvgrabw->grab = gtk_button_new_from_stock (GTK_STOCK_NO);
#endif
  lives_container_add (GTK_CONTAINER (hbuttonbox1), dvgrabw->grab);
  lives_widget_set_can_focus_and_default (dvgrabw->grab);

#if GTK_CHECK_VERSION(2,6,0)
  image = lives_image_new_from_stock (GTK_STOCK_MEDIA_RECORD,LIVES_ICON_SIZE_BUTTON);
#else
  image = lives_image_new_from_stock (GTK_STOCK_NO,LIVES_ICON_SIZE_BUTTON);
#endif
  lives_button_set_label(GTK_BUTTON(dvgrabw->grab),_("_Grab"));
  gtk_button_set_image(GTK_BUTTON(dvgrabw->grab),image);

  label=lives_standard_label_new(_("\nUse this tool to control your camera and grab clips.\nAfter grabbing your clips, you can close this window \nand then load them into LiVES.\n"));
  lives_box_pack_start(GTK_BOX(vbox),label,FALSE,FALSE,widget_opts.packing_height*4);


  hbuttonbox2 = lives_hbutton_box_new ();
  lives_box_pack_start(GTK_BOX(vbox),hbuttonbox2,FALSE,FALSE,widget_opts.packing_height);

  dvgrabw->quit = gtk_button_new_from_stock (GTK_STOCK_CLOSE);
  lives_container_add (GTK_CONTAINER (hbuttonbox2), dvgrabw->quit);
  lives_widget_set_can_focus_and_default (dvgrabw->quit);

  image=lives_image_new_from_stock(GTK_STOCK_CLOSE,LIVES_ICON_SIZE_BUTTON);
  lives_button_set_label(GTK_BUTTON(dvgrabw->quit),_("_Close Window"));
  gtk_button_set_image(GTK_BUTTON(dvgrabw->quit),image);

  //////////////////////////////////////////////////////////////////////////////////////////

  g_signal_connect (button3, "clicked",G_CALLBACK (on_camrew_clicked),(gpointer)cam);
  g_signal_connect (button4, "clicked",G_CALLBACK (on_camff_clicked),(gpointer)cam);
  g_signal_connect (dvgrabw->stop, "clicked",G_CALLBACK (on_camstop_clicked),(gpointer)cam);
  g_signal_connect (dvgrabw->play, "clicked",G_CALLBACK (on_camplay_clicked),(gpointer)cam);
  g_signal_connect (dvgrabw->grab, "clicked",G_CALLBACK (on_camgrab_clicked),(gpointer)cam);
  g_signal_connect (dvgrabw->quit, "clicked",G_CALLBACK (on_camquit_clicked),(gpointer)cam);


  lives_widget_show_all(dvgrabw->dialog);
  
  dvgrabw->dirname=NULL;

  return dvgrabw;
}

