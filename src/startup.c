// startup.c
// LiVES
// (c) G. Finch 2010 - 2012 <salsaman@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details


// functions for first time startup

#include "support.h"
#include "main.h"
#include "interface.h"





static gboolean prompt_existing_dir(gchar *dirname, guint64 freespace, gboolean wrtable) {
  gchar *msg;
  gboolean res=FALSE;

  if (wrtable) {
    gchar *fspstr=lives_format_storage_space_string(freespace);
    msg=g_strdup_printf(_("A directory named\n%s\nalready exists. Do you wish to use this directory ?\n\n(Free space = %s)\n"),dirname,fspstr);
    g_free(fspstr);
    res=do_yesno_dialog(msg);
  }
  else
    {
      msg=g_strdup_printf(_("A directory named\n%s\nalready exists.\nLiVES could not write to this directory or read its free space.\nPlease select another location.\n"),dirname);
      do_error_dialog(msg);
    }
  g_free(msg);
  return res;
}





static gboolean prompt_new_dir(gchar *dirname, guint64 freespace, gboolean wrtable) {
  gboolean res=FALSE;
  gchar *msg;
  if (wrtable) {
    gchar *fspstr=lives_format_storage_space_string(freespace);
    msg=g_strdup_printf(_("\nCreate the directory\n%s\n?\n\n(Free space = %s)"),dirname,fspstr);
    g_free(fspstr);
    res=do_warning_dialog(msg);
  }
  else {
    msg=g_strdup_printf(_("\nLiVES could not write to the directory\n%s\nPlease try again and choose a different location.\n"),dirname);
    do_error_dialog(msg);
  }
  g_free(msg);
  return res;
}




gboolean do_tempdir_query(void) {
  gint response;
  gboolean ok=FALSE;
  _entryw *tdentry;
  gchar *dirname;
  guint64 freesp;


 top:

  tdentry=create_rename_dialog(6);

  while (!ok) {
    response=gtk_dialog_run(GTK_DIALOG(tdentry->dialog));

    if (response==GTK_RESPONSE_CANCEL) {
      return FALSE;
    }
    dirname=g_strdup(gtk_entry_get_text(GTK_ENTRY(tdentry->entry)));

    if (strcmp(dirname+strlen(dirname)-1,G_DIR_SEPARATOR_S)) {
      gchar *tmp=g_strdup_printf("%s%s",dirname,G_DIR_SEPARATOR_S);
      g_free(dirname);
      dirname=tmp;
    }

    if (strlen(dirname)>(PATH_MAX-1)) {
      do_blocking_error_dialog(_("Directory name is too long !"));
      g_free(dirname);
      continue;
    }

    if (!check_dir_access(dirname)) {
      do_dir_perm_error(dirname);
      g_free(dirname);
      continue;
    }


    if (g_file_test(dirname,G_FILE_TEST_IS_DIR)) {
      if (is_writeable_dir(dirname)) {
	freesp=get_fs_free(dirname);
	if (!prompt_existing_dir(dirname,freesp,TRUE)) {
	  g_free(dirname);
	  continue;
	}
      }
      else {
	if (!prompt_existing_dir(dirname,0,FALSE)) {
	  g_free(dirname);
	  continue;
	}
      }
    }
    else {
      if (is_writeable_dir(dirname)) {
	freesp=get_fs_free(dirname);
	if (!prompt_new_dir(dirname,freesp,TRUE)) {
	  rmdir(dirname);
	  g_free(dirname);
	  continue;
	}
      }
      else {
	if (!prompt_new_dir(dirname,0,FALSE)) {
	  rmdir(dirname);
	  g_free(dirname);
	  continue;
	}
      }
    }

    ok=TRUE;
  }

  gtk_widget_destroy(tdentry->dialog);
  g_free(tdentry);

  mainw->com_failed=FALSE;

  if (!g_file_test(dirname,G_FILE_TEST_IS_DIR)) {
    if (g_mkdir_with_parents(dirname,S_IRWXU)==-1) goto top;
  }

#ifndef IS_MINGW
  com=g_strdup_printf ("/bin/chmod 777 \"%s\" 2>/dev/null",dirname);
  lives_system (com,FALSE);
  g_free (com);
#endif

  g_snprintf(prefs->tmpdir,PATH_MAX,"%s",dirname);
  g_snprintf(future_prefs->tmpdir,PATH_MAX,"%s",prefs->tmpdir);

  set_pref("tempdir",prefs->tmpdir);
  set_pref("session_tempdir",prefs->tmpdir);

#ifndef IS_MINGW
  g_snprintf(mainw->first_info_file,PATH_MAX,"%s"G_DIR_SEPARATOR_S".info.%d",prefs->tmpdir,getpid());
#else
  g_snprintf(mainw->first_info_file,PATH_MAX,"%s"G_DIR_SEPARATOR_S"info.%d",prefs->tmpdir,getpid());
#endif

  g_free(dirname);
  return TRUE;
}





static void on_init_aplayer_toggled (GtkToggleButton *tbutton, gpointer user_data) {
  gint audp=GPOINTER_TO_INT(user_data);

  if (!gtk_toggle_button_get_active(tbutton)) return;

  prefs->audio_player=audp;

  switch (audp) {
  case AUD_PLAYER_PULSE:
    set_pref("audio_player","pulse");
    break;
  case AUD_PLAYER_JACK:
    set_pref("audio_player","jack");
    break;
  case AUD_PLAYER_SOX:
    set_pref("audio_player","sox");
    break;
  case AUD_PLAYER_MPLAYER:
    set_pref("audio_player","mplayer");
    break;
  }

}




gboolean do_audio_choice_dialog(short startup_phase) {
  GtkWidget *dialog,*dialog_vbox,*radiobutton0,*radiobutton1,*radiobutton2,*radiobutton3,*label;
  GtkWidget *okbutton,*cancelbutton;
  GtkWidget *eventbox,*hbox;
  GSList *radiobutton_group = NULL;
  gchar *txt0,*txt1,*txt2,*txt3,*txt4,*txt5,*txt6,*txt7,*msg;

  gint response;

  if (startup_phase==2) {
    txt0=g_strdup(_("LiVES FAILED TO START YOUR SELECTED AUDIO PLAYER !\n\n"));
  }
  else {
    prefs->audio_player=-1;
    txt0=g_strdup("");
  }

  txt1=g_strdup(_("Before starting LiVES, you need to choose an audio player.\n\nPULSE AUDIO is recommended for most users"));

#ifndef HAVE_PULSE_AUDIO
  txt2=g_strdup(_(", but this version of LiVES was not compiled with pulse audio support.\n\n"));
#else
  if (!capable->has_pulse_audio) {
    txt2=g_strdup(_(", but you do not have pulse audio installed on your system.\n You are advised to install pulse audio first before running LiVES.\n\n"));
  }
  else txt2=g_strdup(".\n\n");
#endif

  txt3=g_strdup(_("JACK audio is recommended for pro users"));

#ifndef ENABLE_JACK
  txt4=g_strdup(_(", but this version of LiVES was not compiled with jack audio support.\n\n"));
#else
  if (!capable->has_jackd) {
    txt4=g_strdup(_(", but you do not have jackd installed. You may wish to install jackd first before running LiVES.\n\n"));
  }
  else {
    txt4=g_strdup(_(", but may prevent LiVES from starting on some systems.\nIf LiVES will not start with jack, you can restart and try with another audio player instead.\n\n"));
  }
#endif

  txt5=g_strdup(_("SOX may be used if neither of the preceding players work, "));

  if (capable->has_sox_play) {
    txt6=g_strdup(_("but some audio features will be disabled.\n\n"));
  }
  else {
    txt6=g_strdup(_("but you do not have sox installed.\nYou are advised to install it before running LiVES.\n\n"));
  }

  if (capable->has_mplayer) {
    txt7=g_strdup(_("The MPLAYER audio player is only recommended for testing purposes.\n\n"));
  }
  else {
    txt7=g_strdup("");
  }

  msg=g_strdup_printf("%s%s%s%s%s%s%s%s",txt0,txt1,txt2,txt3,txt4,txt5,txt6,txt7);

  g_free(txt0);
  g_free(txt1);
  g_free(txt2);
  g_free(txt3);
  g_free(txt4);
  g_free(txt5);
  g_free(txt6);
  g_free(txt7);

  dialog = gtk_dialog_new ();
  gtk_window_set_title (GTK_WINDOW (dialog), _("LiVES: - Choose an audio player"));

  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER_ALWAYS);
  gtk_window_set_default_size (GTK_WINDOW (dialog), 350, 200);

  if (palette->style&STYLE_1) {
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog),FALSE);
    gtk_widget_modify_bg(dialog, GTK_STATE_NORMAL, &palette->normal_back);
  }

  dialog_vbox = lives_dialog_get_content_area(GTK_DIALOG(dialog));
  
  label=gtk_label_new(msg);
  gtk_container_add (GTK_CONTAINER (dialog_vbox), label);
  g_free(msg);


  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }




  radiobutton0 = gtk_radio_button_new (NULL);

#ifdef HAVE_PULSE_AUDIO
  eventbox=gtk_event_box_new();
  label=gtk_label_new_with_mnemonic ( _("Use _pulse audio player"));
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), radiobutton0);

  gtk_container_add(GTK_CONTAINER(eventbox),label);
  g_signal_connect (GTK_OBJECT (eventbox), "button_press_event",
		    G_CALLBACK (label_act_toggle),
		    radiobutton0);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg (eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (dialog_vbox), hbox, FALSE, FALSE, 10);

  gtk_box_pack_start (GTK_BOX (hbox), radiobutton0, FALSE, FALSE, 10);
  gtk_box_pack_start (GTK_BOX (hbox), eventbox, FALSE, FALSE, 10);

  if (prefs->audio_player==-1) prefs->audio_player=AUD_PLAYER_PULSE;

  gtk_radio_button_set_group (GTK_RADIO_BUTTON (radiobutton0), radiobutton_group);
  radiobutton_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radiobutton0));
  if (prefs->audio_player==AUD_PLAYER_PULSE) {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radiobutton0),TRUE);
    set_pref("audio_player","pulse");
  }

#endif

  radiobutton1 = gtk_radio_button_new(NULL);
#ifdef ENABLE_JACK

  eventbox=gtk_event_box_new();
  label=gtk_label_new_with_mnemonic ( _("Use _jack audio player"));
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), radiobutton1);

  gtk_container_add(GTK_CONTAINER(eventbox),label);
  g_signal_connect (GTK_OBJECT (eventbox), "button_press_event",
		    G_CALLBACK (label_act_toggle),
		    radiobutton1);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg (eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (dialog_vbox), hbox, FALSE, FALSE, 10);

  gtk_box_pack_start (GTK_BOX (hbox), radiobutton1, FALSE, FALSE, 10);
  gtk_box_pack_start (GTK_BOX (hbox), eventbox, FALSE, FALSE, 10);


  if (prefs->audio_player==-1) prefs->audio_player=AUD_PLAYER_JACK;

  gtk_radio_button_set_group (GTK_RADIO_BUTTON (radiobutton1), radiobutton_group);
  radiobutton_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radiobutton1));

  if (prefs->audio_player==AUD_PLAYER_JACK||!capable->has_pulse_audio) {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radiobutton1),TRUE);
    set_pref("audio_player","jack");
  }


#endif

  radiobutton2 = gtk_radio_button_new (NULL);
  if (capable->has_sox_play) {

    eventbox=gtk_event_box_new();
    label=gtk_label_new_with_mnemonic ( _("Use _sox audio player"));
    gtk_label_set_mnemonic_widget (GTK_LABEL (label), radiobutton2);
    
    gtk_container_add(GTK_CONTAINER(eventbox),label);
    g_signal_connect (GTK_OBJECT (eventbox), "button_press_event",
		      G_CALLBACK (label_act_toggle),
		      radiobutton2);
    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
      gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
      gtk_widget_modify_bg (eventbox, GTK_STATE_NORMAL, &palette->normal_back);
    }
    
    hbox = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (dialog_vbox), hbox, FALSE, FALSE, 10);
    
    gtk_box_pack_start (GTK_BOX (hbox), radiobutton2, FALSE, FALSE, 10);
    gtk_box_pack_start (GTK_BOX (hbox), eventbox, FALSE, FALSE, 10);

    if (prefs->audio_player==-1) prefs->audio_player=AUD_PLAYER_SOX;

    gtk_radio_button_set_group (GTK_RADIO_BUTTON (radiobutton2), radiobutton_group);
    radiobutton_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radiobutton2));

    if (prefs->audio_player==AUD_PLAYER_SOX) {
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radiobutton2),TRUE);
      set_pref("audio_player","sox");
    }


  }

  radiobutton3 = gtk_radio_button_new (NULL);
  if (capable->has_mplayer) {
    eventbox=gtk_event_box_new();
    label=gtk_label_new_with_mnemonic ( _("Use _mplayer audio player"));
    gtk_label_set_mnemonic_widget (GTK_LABEL (label), radiobutton3);
    
    gtk_container_add(GTK_CONTAINER(eventbox),label);
    g_signal_connect (GTK_OBJECT (eventbox), "button_press_event",
		      G_CALLBACK (label_act_toggle),
		      radiobutton3);
    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
      gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
      gtk_widget_modify_bg (eventbox, GTK_STATE_NORMAL, &palette->normal_back);
    }
    
    hbox = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (dialog_vbox), hbox, FALSE, FALSE, 10);
    
    gtk_box_pack_start (GTK_BOX (hbox), radiobutton3, FALSE, FALSE, 10);
    gtk_box_pack_start (GTK_BOX (hbox), eventbox, FALSE, FALSE, 10);

    if (prefs->audio_player==-1) prefs->audio_player=AUD_PLAYER_MPLAYER;

    gtk_radio_button_set_group (GTK_RADIO_BUTTON (radiobutton3), radiobutton_group);
    radiobutton_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radiobutton3));

    if (prefs->audio_player==AUD_PLAYER_MPLAYER) {
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radiobutton3),TRUE);
      set_pref("audio_player","mplayer");
    }

  }


  g_signal_connect (GTK_OBJECT (radiobutton0), "toggled",
                      G_CALLBACK (on_init_aplayer_toggled),
                      GINT_TO_POINTER(AUD_PLAYER_PULSE));

  g_signal_connect (GTK_OBJECT (radiobutton1), "toggled",
                      G_CALLBACK (on_init_aplayer_toggled),
                      GINT_TO_POINTER(AUD_PLAYER_JACK));

  g_signal_connect (GTK_OBJECT (radiobutton2), "toggled",
                      G_CALLBACK (on_init_aplayer_toggled),
                      GINT_TO_POINTER(AUD_PLAYER_SOX));

  g_signal_connect (GTK_OBJECT (radiobutton3), "toggled",
                      G_CALLBACK (on_init_aplayer_toggled),
                      GINT_TO_POINTER(AUD_PLAYER_MPLAYER));

  cancelbutton = gtk_button_new_from_stock ("gtk-cancel");
  gtk_widget_show (cancelbutton);
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), cancelbutton, GTK_RESPONSE_CANCEL);

  okbutton = gtk_button_new_from_stock ("gtk-go-forward");
  gtk_button_set_label(GTK_BUTTON(okbutton),_("_Next"));
  gtk_widget_show (okbutton);
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), okbutton, GTK_RESPONSE_OK);
  GTK_WIDGET_SET_FLAGS (okbutton, GTK_CAN_DEFAULT|GTK_CAN_FOCUS);
  gtk_widget_grab_default(okbutton);
  gtk_widget_grab_focus(okbutton);


  if (prefs->audio_player==-1) {
    do_no_mplayer_sox_error();
    return GTK_RESPONSE_CANCEL;
  }

  gtk_widget_show_all(dialog);

  response=gtk_dialog_run(GTK_DIALOG(dialog));

  gtk_widget_destroy(dialog);

  if (prefs->audio_player==AUD_PLAYER_SOX||prefs->audio_player==AUD_PLAYER_MPLAYER) {
    gtk_widget_hide(mainw->vol_toolitem);
    gtk_widget_hide(mainw->vol_label);
    gtk_widget_hide (mainw->recaudio_submenu);
  }

  while (g_main_context_iteration(NULL,FALSE));

  return (response==GTK_RESPONSE_OK);
}


static void add_test(GtkWidget *table, gint row, gchar *ttext, gboolean noskip) {
  GtkWidget *label=gtk_label_new(ttext);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);

  gtk_table_attach (GTK_TABLE (table), label, 0, 1, row, row+1, (GtkAttachOptions)0, (GtkAttachOptions)0, 10, 10);
  gtk_widget_show(label);

  if (!noskip) {
    GtkWidget *image=gtk_image_new_from_stock(GTK_STOCK_REMOVE,GTK_ICON_SIZE_LARGE_TOOLBAR);
    // TRANSLATORS - as in "skipped test"
    label=gtk_label_new(_("Skipped"));
    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    }
    gtk_table_attach (GTK_TABLE (table), label, 1, 2, row, row+1, (GtkAttachOptions)0, (GtkAttachOptions)0, 10, 10);
    gtk_widget_show(label);

    gtk_table_attach (GTK_TABLE (table), image, 2, 3, row, row+1, (GtkAttachOptions)0, (GtkAttachOptions)0, 0, 10);
    gtk_widget_show(image);
  }

  while (g_main_context_iteration(NULL,FALSE));
}


static gboolean pass_test(GtkWidget *table, gint row) {
  // TRANSLATORS - as in "passed test"
  GtkWidget *label=gtk_label_new(_("Passed"));
  GtkWidget *image=gtk_image_new_from_stock(GTK_STOCK_APPLY,GTK_ICON_SIZE_LARGE_TOOLBAR);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  gtk_table_attach (GTK_TABLE (table), label, 1, 2, row, row+1, (GtkAttachOptions)0, (GtkAttachOptions)0, 10, 10);
  gtk_widget_show(label);

  gtk_table_attach (GTK_TABLE (table), image, 2, 3, row, row+1, (GtkAttachOptions)0, (GtkAttachOptions)0, 0, 10);
  gtk_widget_show(image);

  while (g_main_context_iteration(NULL,FALSE));
  return TRUE;
}


static gboolean fail_test(GtkWidget *table, gint row, gchar *ftext) {
  GtkWidget *label;
  GtkWidget *image=gtk_image_new_from_stock(GTK_STOCK_CANCEL,GTK_ICON_SIZE_LARGE_TOOLBAR);

  label=gtk_label_new(ftext);

  gtk_table_attach (GTK_TABLE (table), label, 3, 4, row, row+1, (GtkAttachOptions)0, (GtkAttachOptions)0, 10, 10);
  gtk_widget_show(label);
  
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  
  // TRANSLATORS - as in "failed test"
  label=gtk_label_new(_("Failed"));

  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  gtk_table_attach (GTK_TABLE (table), label, 1, 2, row, row+1, (GtkAttachOptions)0, (GtkAttachOptions)0, 10, 10);
  gtk_widget_show(label);

  gtk_table_attach (GTK_TABLE (table), image, 2, 3, row, row+1, (GtkAttachOptions)0, (GtkAttachOptions)0, 0, 10);
  gtk_widget_show(image);

  while (g_main_context_iteration(NULL,FALSE));
  
  return FALSE;
}


LIVES_INLINE gchar *get_resource(gchar *fname) {
  return g_strdup_printf("%s%sresources/%s",prefs->prefix_dir,DATA_DIR,fname);
}



gboolean do_startup_tests(gboolean tshoot) {
  GtkWidget *dialog;
  GtkWidget *dialog_vbox;

  GtkWidget *label;
  GtkWidget *table;
  GtkWidget *okbutton;
  GtkWidget *cancelbutton;

  gchar *com,*rname,*afile,*tmp;

  guchar *abuff;

  size_t fsize;

  gboolean success,success2,success3,success4;

  gint response,res;
  gint current_file=mainw->current_file;

  int out_fd,info_fd;

  gchar *image_ext=g_strdup(prefs->image_ext);

  mainw->suppress_dprint=TRUE;

  if (mainw->multitrack!=NULL) {
    if (mainw->multitrack->idlefunc>0) {
      g_source_remove(mainw->multitrack->idlefunc);
      mainw->multitrack->idlefunc=0;
    }
    mt_desensitise(mainw->multitrack);
  }


  dialog = gtk_dialog_new ();

  if (!tshoot) {
    gtk_window_set_title (GTK_WINDOW (dialog), _("LiVES: - Testing Configuration"));
  }
  else {
    gtk_window_set_title (GTK_WINDOW (dialog), _("LiVES: - Troubleshoot"));
  }

  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER_ALWAYS);
  gtk_window_set_default_size (GTK_WINDOW (dialog), 350, 200);

  if (palette->style&STYLE_1) {
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog),FALSE);
    gtk_widget_modify_bg(dialog, GTK_STATE_NORMAL, &palette->normal_back);
  }

  dialog_vbox = lives_dialog_get_content_area(GTK_DIALOG(dialog));

  label=gtk_label_new(_("LiVES will now run some basic configuration tests\n"));
  gtk_container_add (GTK_CONTAINER (dialog_vbox), label);


  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }



  cancelbutton = gtk_button_new_from_stock ("gtk-cancel");
  gtk_widget_show (cancelbutton);
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), cancelbutton, GTK_RESPONSE_CANCEL);

  if (!tshoot) {
    okbutton = gtk_button_new_from_stock ("gtk-go-forward");
    gtk_button_set_label(GTK_BUTTON(okbutton),_("_Next"));
  }
  else okbutton = gtk_button_new_from_stock ("gtk-ok");
  gtk_widget_show (okbutton);
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), okbutton, GTK_RESPONSE_OK);
  GTK_WIDGET_SET_FLAGS (okbutton, GTK_CAN_DEFAULT|GTK_CAN_FOCUS);
  gtk_widget_grab_default(okbutton);
  gtk_widget_grab_focus(okbutton);


  gtk_widget_set_sensitive(okbutton,FALSE);


  table = gtk_table_new (10, 4, FALSE);
  gtk_container_add (GTK_CONTAINER (dialog_vbox), table);

  gtk_widget_show_all(dialog);

  while (g_main_context_iteration(NULL,FALSE));


  // check for sox presence

  add_test(table,0,_("Checking for \"sox\" presence"),TRUE);


  if (!capable->has_sox_sox) {
    success=fail_test(table,0,_("You should install sox to be able to use all the audio features in LiVES"));

  }
  else {
    success=pass_test(table,0);
  }

  // test if sox can convert raw 44100 -> wav 22050
  add_test(table,1,_("Checking if sox can convert audio"),success);
  
  if (!tshoot) set_pref("default_image_format","png");
  g_snprintf (prefs->image_ext,16,"%s","png");

  get_temp_handle(mainw->first_free_file,TRUE);

  if (success) {
    
    info_fd=-1;

    unlink(cfile->info_file);

    // write 1 second of silence
    afile=g_build_filename(prefs->tmpdir,cfile->handle,"audio",NULL);
    out_fd=open(afile,O_WRONLY|O_CREAT,S_IRUSR|S_IWUSR);

    if (out_fd<0) mainw->write_failed=TRUE;
    else mainw->write_failed=FALSE;

    if (!mainw->write_failed) {
      abuff=(guchar *)calloc(44100,4);
      if (!abuff) {
	tmp=g_strdup(_("Unable to allocate 176400 bytes memory."));
	fail_test(table,1,tmp);
	g_free(tmp);
      }
      else {
#ifdef IS_MINGW
	setmode(out_fd, O_BINARY);
#endif
	lives_write (out_fd,abuff,176400,TRUE);
	close(out_fd);
	g_free(abuff);
      }
    }

    if (mainw->write_failed) {
      tmp=g_strdup_printf(_("Unable to write to: %s"),afile);
      fail_test(table,1,tmp);
      g_free(tmp);
    }

    g_free(afile);

    if (!mainw->write_failed) {
      afile=g_build_filename(prefs->tmpdir,cfile->handle,"testout.wav",NULL);
    
      mainw->com_failed=FALSE;
      com=g_strdup_printf("%s export_audio \"%s\" 0. 0. 44100 2 16 0 22050 \"%s\"",prefs->backend_sync,cfile->handle,afile);
      lives_system(com,TRUE);
      if (mainw->com_failed) {
	tmp=g_strdup_printf(_("Command failed: %s"),com);
	fail_test(table,1,tmp);
	g_free(tmp);
      }
      
      g_free(com);

      while (mainw->cancelled==CANCEL_NONE&&(info_fd=open(cfile->info_file,O_RDONLY))==-1) {
	g_usleep(prefs->sleep_time);
	while (g_main_context_iteration(NULL,FALSE));
      }
      
      if (info_fd!=-1) {
	close(info_fd);
	
	lives_sync();
	
	fsize=sget_file_size(afile);
	unlink(afile);
	g_free(afile);
	
	if (fsize==0) {
	  fail_test(table,1,_("You should install sox_fmt_all or similar"));
	}
	
	else pass_test(table,1);

      }
    }
  }

  if (tshoot) g_snprintf (prefs->image_ext,16,"%s",image_ext);

  if (mainw->cancelled!=CANCEL_NONE) {
    mainw->cancelled=CANCEL_NONE;
    close_current_file(current_file);
    gtk_widget_destroy(dialog);
    mainw->suppress_dprint=FALSE;

    if (mainw->multitrack!=NULL) {
      mt_sensitise(mainw->multitrack);
      mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
    }

    return FALSE;
  }


  // check for mplayer presence

  add_test(table,2,_("Checking for \"mplayer\" presence"),TRUE);


  if (!capable->has_mplayer) {
    success2=fail_test(table,2,_("You should install mplayer to be able to use all the decoding features in LiVES"));

    if (!success) {
      gtk_widget_destroy(dialog);
      while (g_main_context_iteration(NULL,FALSE));
      do_no_mplayer_sox_error();
      close_current_file(current_file);
      mainw->suppress_dprint=FALSE;

      if (mainw->multitrack!=NULL) {
	mt_sensitise(mainw->multitrack);
	mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
      }

      return FALSE;
    }
  }
  else {
    success2=pass_test(table,2);
  }


  // if present

  // check if mplayer can decode audio

  add_test(table,3,_("Checking if mplayer can convert audio"),success2);

  if (success2) {

#ifndef IS_MINGW
    res=system("LANG=en LANGUAGE=en mplayer -ao help | grep pcm >/dev/null 2>&1");
#else    
    res=system("mplayer -ao help | grep pcm >NUL 2>&1");
#endif
    if (res==0) {
      pass_test(table,3);
    }
    else {
      fail_test(table,3,_("You should install mplayer with pcm/wav support"));
    }
  }

  // check if mplayer can decode to png/alpha

  rname=get_resource("");

  if (!g_file_test(rname,G_FILE_TEST_IS_DIR)) {
    /// oops, no resources dir
    success4=FALSE;
  }
  else success4=TRUE;


  g_free(rname);

  add_test(table,4,_("Checking if mplayer can decode to png/alpha"),success2&&success4);

  success3=FALSE;

  // try to open resource vidtest.avi

  if (success2&&success4) {

    info_fd=-1;

    unlink(cfile->info_file);

    rname=get_resource("vidtest.avi");

    com=g_strdup_printf("%s open_test \"%s\" \"%s\" 0 png",prefs->backend_sync,cfile->handle,
			(tmp=g_filename_from_utf8 (rname,-1,NULL,NULL,NULL)));
    g_free(tmp);
    g_free(rname);

    mainw->com_failed=FALSE;
    lives_system(com,TRUE);
    if (mainw->com_failed) {
      tmp=g_strdup_printf(_("Command failed: %s"),com);
      fail_test(table,4,tmp);
      g_free(tmp);
    }

    g_free(com);

    while (mainw->cancelled==CANCEL_NONE&&(info_fd=open(cfile->info_file,O_RDONLY))==-1) {
      g_usleep(prefs->sleep_time);
    }
    
    if (info_fd!=-1) {

      close(info_fd);
      
      lives_sync();
      
      cfile->img_type=IMG_TYPE_PNG;
      get_frame_count(mainw->current_file);
      
      if (cfile->frames==0) {
	fail_test(table,4,_("You may wish to upgrade mplayer to a newer version"));
      }
      
      else {
	pass_test(table,4);
	success3=TRUE;
      }
    }
  }

  if (mainw->cancelled!=CANCEL_NONE) {
    mainw->cancelled=CANCEL_NONE;
    close_current_file(current_file);
    gtk_widget_destroy(dialog);
    mainw->suppress_dprint=FALSE;

    if (mainw->multitrack!=NULL) {
      mt_sensitise(mainw->multitrack);
      mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
    }

    return FALSE;
  }
  
  // check if mplayer can decode to jpeg

  add_test(table,5,_("Checking if mplayer can decode to jpeg"),success2);


  if (success2) {
#ifndef IS_MINGW
    res=system("LANG=en LANGUAGE=en mplayer -vo help | grep -i \"jpeg file\" >/dev/null 2>&1");
#else    
    res=system("mplayer -vo help | grep -i \"jpeg file\" >NUL 2>&1");
#endif
    
    if (res==0) {
      pass_test(table,5);
      if (!success3) {
	set_pref("default_image_format","jpeg");
	g_snprintf (prefs->image_ext,16,"%s","jpg");
      }
    }
    else {
      if (!success3) fail_test(table,5,_("You should install mplayer with either png/alpha or jpeg support"));
      else fail_test(table,5,_("You may wish to add jpeg output support to mplayer"));
    }
  }
  
  // TODO - check each enabled decoder plugin in turn


  // check for convert

  add_test(table,8,_("Checking for \"convert\" presence"),TRUE);


  if (!capable->has_convert) {
    success=fail_test(table,8,_("Install imageMagick to be able to use all of the rendered effects"));
  }
  else {
    success=pass_test(table,8);
  }


  close_current_file(current_file);

  gtk_widget_set_sensitive(okbutton,TRUE);
  if (tshoot) gtk_widget_hide(cancelbutton); 
  

  if (!tshoot) {
    label=gtk_label_new(_("\n\n    Click Cancel to exit and install any missing components, or Next to continue    \n"));
    gtk_container_add (GTK_CONTAINER (dialog_vbox), label);

    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    }
    
    gtk_widget_show(label);
  }

  response=gtk_dialog_run(GTK_DIALOG(dialog));

  gtk_widget_destroy(dialog);
  mainw->suppress_dprint=FALSE;

  if (mainw->multitrack!=NULL) {
    mt_sensitise(mainw->multitrack);
    mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
  }

  return (response==GTK_RESPONSE_OK);
}






void do_startup_interface_query(void) {

  // prompt for startup ce or startup mt


  GtkWidget *dialog,*dialog_vbox,*radiobutton0,*radiobutton1,*label;
  GtkWidget *okbutton;
  GtkWidget *eventbox,*hbox;
  GSList *radiobutton_group = NULL;
  gchar *txt1,*txt2,*txt3,*msg;

  txt1=g_strdup(_("\n\nFinally, you can choose the default startup interface for LiVES.\n"));
  txt2=g_strdup(_("\n\nLiVES has two main interfaces and you can start up with either of them.\n"));
  txt3=g_strdup(_("\n\nThe default can always be changed later from Preferences.\n"));


  msg=g_strdup_printf("%s%s%s",txt1,txt2,txt3);


  g_free(txt1);
  g_free(txt2);
  g_free(txt3);

  dialog = gtk_dialog_new ();
  gtk_window_set_title (GTK_WINDOW (dialog), _("LiVES: - Choose the startup interface"));

  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER_ALWAYS);
  gtk_window_set_default_size (GTK_WINDOW (dialog), 600, 200);

  if (palette->style&STYLE_1) {
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog),FALSE);
    gtk_widget_modify_bg(dialog, GTK_STATE_NORMAL, &palette->normal_back);
  }

  dialog_vbox = lives_dialog_get_content_area(GTK_DIALOG(dialog));
  
  label=gtk_label_new(msg);
  gtk_container_add (GTK_CONTAINER (dialog_vbox), label);
  g_free(msg);


  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }




  radiobutton0 = gtk_radio_button_new (NULL);


  eventbox=gtk_event_box_new();
  label=gtk_label_new_with_mnemonic ( _("Start in _Clip Edit mode"));
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), radiobutton0);

  gtk_container_add(GTK_CONTAINER(eventbox),label);
  g_signal_connect (GTK_OBJECT (eventbox), "button_press_event",
		    G_CALLBACK (label_act_toggle),
		    radiobutton0);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg (eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (dialog_vbox), hbox, FALSE, FALSE, 10);

  gtk_box_pack_start (GTK_BOX (hbox), radiobutton0, FALSE, FALSE, 10);
  gtk_box_pack_start (GTK_BOX (hbox), eventbox, FALSE, FALSE, 10);



  gtk_radio_button_set_group (GTK_RADIO_BUTTON (radiobutton0), radiobutton_group);
  radiobutton_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radiobutton0));

  label=gtk_label_new(_("This is the best choice for simple editing tasks and for VJs\n"));

  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  gtk_box_pack_start (GTK_BOX (dialog_vbox), label, FALSE, FALSE, 10);

  radiobutton1 = gtk_radio_button_new(NULL);


  eventbox=gtk_event_box_new();
  label=gtk_label_new_with_mnemonic ( _("Start in _Multitrack mode"));
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), radiobutton1);

  gtk_container_add(GTK_CONTAINER(eventbox),label);
  g_signal_connect (GTK_OBJECT (eventbox), "button_press_event",
		    G_CALLBACK (label_act_toggle),
		    radiobutton1);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg (eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (dialog_vbox), hbox, FALSE, FALSE, 10);

  gtk_box_pack_start (GTK_BOX (hbox), radiobutton1, FALSE, FALSE, 10);
  gtk_box_pack_start (GTK_BOX (hbox), eventbox, FALSE, FALSE, 10);


  label=gtk_label_new(_("This is a better choice for complex editing tasks involving multiple clips.\n"));

  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  gtk_box_pack_start (GTK_BOX (dialog_vbox), label, FALSE, FALSE, 10);


  gtk_radio_button_set_group (GTK_RADIO_BUTTON (radiobutton1), radiobutton_group);
  radiobutton_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radiobutton1));

  if (prefs->startup_interface==STARTUP_MT) {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radiobutton1),TRUE);
  }

  okbutton = gtk_button_new_from_stock ("gtk-go-forward");
  gtk_button_set_label(GTK_BUTTON(okbutton),_("_Finish"));
  gtk_widget_show (okbutton);
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), okbutton, GTK_RESPONSE_OK);
  GTK_WIDGET_SET_FLAGS (okbutton, GTK_CAN_DEFAULT|GTK_CAN_FOCUS);
  gtk_widget_grab_default(okbutton);
  gtk_widget_grab_focus(okbutton);


  gtk_widget_show_all(dialog);

  gtk_dialog_run(GTK_DIALOG(dialog));


  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radiobutton1))) 
    future_prefs->startup_interface=prefs->startup_interface=STARTUP_MT;
  
  set_int_pref("startup_interface",prefs->startup_interface);

  gtk_widget_destroy(dialog);

  while (g_main_context_iteration(NULL,FALSE));

}



void on_troubleshoot_activate                     (GtkMenuItem     *menuitem,
						   gpointer         user_data) {
  do_startup_tests(TRUE);
}
