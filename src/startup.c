// startup.c
// LiVES
// (c) G. Finch 2010 <salsaman@xs4all.nl>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details


// functions for first time startup

#include "support.h"
#include "main.h"
#include "interface.h"





static gboolean prompt_existing_dir(gchar *dirname) {
  gchar *msg=g_strdup_printf(_("A directory named\n%s\nalready exists. Do you wish to use this directory ?\n"),dirname);
  gboolean res=do_warning_dialog(msg);
  g_free(msg);
  return res;
}





static gboolean prompt_new_dir(gchar *dirname) {
  gchar *msg=g_strdup_printf(_("Create the directory\n%s\n?\n"),dirname);
  gboolean res=do_warning_dialog(msg);
  g_free(msg);
  return res;
}




gboolean do_tempdir_query(void) {
  gint response;
  gboolean ok=FALSE;
  _entryw *tdentry=create_rename_dialog(6);
  gchar *tmp,*com,*dirname;

  while (!ok) {
    response=gtk_dialog_run(GTK_DIALOG(tdentry->dialog));

    if (response==GTK_RESPONSE_CANCEL) {
      g_free(tdentry);
      return FALSE;
    }
    dirname=g_strdup(gtk_entry_get_text(GTK_ENTRY(tdentry->entry)));

    if (strlen(dirname)==0||strncmp(dirname+strlen(dirname)-1,"/",1)) {
      tmp=g_strdup_printf("%s/",dirname);
      g_free(dirname);
      dirname=tmp;
    }

    if (strlen(dirname)<10||strncmp(dirname+strlen(dirname)-10,"/livestmp/",10)) {
      tmp=g_strdup_printf("%slivestmp/",dirname);
      g_free(dirname);
      dirname=tmp;
    }

    if (strlen(dirname)>255) {
      do_blocking_error_dialog(_("Directory name is too long !"));
      g_free(dirname);
      continue;
    }

    if (g_file_test(dirname,G_FILE_TEST_IS_DIR)) {
      if (!prompt_existing_dir(dirname)) {
	g_free(dirname);
	continue;
      }
    }
    else {
      if (!prompt_new_dir(dirname)) {
	g_free(dirname);
	continue;
      }
    }

    if (!check_dir_access(dirname)) {
      do_dir_perm_error(dirname);
      g_free(dirname);
      continue;
    }
    ok=TRUE;
  }

  gtk_widget_destroy(tdentry->dialog);
  g_free(tdentry);

  com=g_strdup_printf("/bin/rmdir %s 2>/dev/null",prefs->tmpdir);
  dummyvar=system(com);
  g_free(com);

  g_snprintf(prefs->tmpdir,256,"%s",dirname);
  g_snprintf(future_prefs->tmpdir,256,"%s",prefs->tmpdir);

  com=g_strdup_printf ("/bin/mkdir -p %s 2>/dev/null",dirname);
  dummyvar=system (com);
  g_free (com);

  set_pref("tempdir",prefs->tmpdir);

  g_snprintf(mainw->first_info_file,255,"%s/.info.%d",prefs->tmpdir,getpid());

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

  if (capable->has_sox) {
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
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
  gtk_window_set_default_size (GTK_WINDOW (dialog), 350, 200);

  if (palette->style&STYLE_1) {
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog),FALSE);
    gtk_widget_modify_bg(dialog, GTK_STATE_NORMAL, &palette->normal_back);
  }

  dialog_vbox = GTK_DIALOG (dialog)->vbox;
  
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

  if (prefs->audio_player==AUD_PLAYER_PULSE) {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radiobutton0),TRUE);
    set_pref("audio_player","pulse");
  }

  gtk_radio_button_set_group (GTK_RADIO_BUTTON (radiobutton0), radiobutton_group);
  radiobutton_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radiobutton0));
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

  if (prefs->audio_player==AUD_PLAYER_JACK) {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radiobutton1),TRUE);
    set_pref("audio_player","jack");
  }

  gtk_radio_button_set_group (GTK_RADIO_BUTTON (radiobutton1), radiobutton_group);
  radiobutton_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radiobutton1));
#endif

  radiobutton2 = gtk_radio_button_new (NULL);
  if (capable->has_sox) {

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

    if (prefs->audio_player==AUD_PLAYER_SOX) {
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radiobutton2),TRUE);
      set_pref("audio_player","sox");
    }

    gtk_radio_button_set_group (GTK_RADIO_BUTTON (radiobutton2), radiobutton_group);
    radiobutton_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radiobutton2));

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

    if (prefs->audio_player==AUD_PLAYER_MPLAYER) {
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radiobutton3),TRUE);
      set_pref("audio_player","mplayer");
    }

    gtk_radio_button_set_group (GTK_RADIO_BUTTON (radiobutton3), radiobutton_group);
    radiobutton_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radiobutton3));

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

  okbutton = gtk_button_new_from_stock ("gtk-ok");
  gtk_widget_show (okbutton);
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), okbutton, GTK_RESPONSE_OK);
  GTK_WIDGET_SET_FLAGS (okbutton, GTK_CAN_DEFAULT|GTK_CAN_FOCUS);
  gtk_widget_grab_default(okbutton);
  gtk_widget_grab_focus(okbutton);


  if (prefs->audio_player==-1) {
    do_blocking_error_dialog(_ ("\nLiVES currently requires either 'mplayer' or 'sox' to function. Please install one or other of these, and try again.\n"));
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







gboolean do_startup_tests(void) {
  // check for sox presence

  // test if sox can convert wav -> raw
  



  // check for mplayer presence

  // if present

  // check if mplayer can decode audio
  // -ao list
  // -vo list

  // if not, warn


  // check if mplayer can decode to png/alpha


  // check if mplayer can decode to jpeg

  
  //    if neither: warn and continue


  //    if no png set def img fmt to jpeg




  // check decode of ogg/theora
  // if not, warn




  // check decode of dv
  // if not, warn


  // check for convert


  // check for composite


  return TRUE;

}






void do_startup_interface_query(void) {

  // prompt for startup ce or startup mt


  GtkWidget *dialog,*dialog_vbox,*radiobutton0,*radiobutton1,*label;
  GtkWidget *okbutton;
  GtkWidget *eventbox,*hbox;
  GSList *radiobutton_group = NULL;
  gchar *txt1,*txt2,*txt3,*msg;

  gint response;

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
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
  gtk_window_set_default_size (GTK_WINDOW (dialog), 350, 200);

  if (palette->style&STYLE_1) {
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog),FALSE);
    gtk_widget_modify_bg(dialog, GTK_STATE_NORMAL, &palette->normal_back);
  }

  dialog_vbox = GTK_DIALOG (dialog)->vbox;
  
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

  label=gtk_label_new(_("This is the best choice for simple editing tasks or for VJs\n"));

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

  okbutton = gtk_button_new_from_stock ("gtk-ok");
  gtk_widget_show (okbutton);
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), okbutton, GTK_RESPONSE_OK);
  GTK_WIDGET_SET_FLAGS (okbutton, GTK_CAN_DEFAULT|GTK_CAN_FOCUS);
  gtk_widget_grab_default(okbutton);
  gtk_widget_grab_focus(okbutton);


  gtk_widget_show_all(dialog);

  response=gtk_dialog_run(GTK_DIALOG(dialog));

  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radiobutton1))) future_prefs->startup_interface=prefs->startup_interface=STARTUP_MT;
  
  set_int_pref("startup_interface",prefs->startup_interface);

  gtk_widget_destroy(dialog);

  while (g_main_context_iteration(NULL,FALSE));

}
