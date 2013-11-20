// startup.c
// LiVES
// (c) G. Finch 2010 - 2013 <salsaman@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details


// functions for first time startup

#include "support.h"
#include "main.h"
#include "interface.h"





static boolean prompt_existing_dir(gchar *dirname, uint64_t freespace, boolean wrtable) {
  gchar *msg;
  boolean res=FALSE;

  if (wrtable) {
    gchar *fspstr=lives_format_storage_space_string(freespace);
    msg=g_strdup_printf
      (_("A directory named\n%s\nalready exists. Do you wish to use this directory ?\n\n(Free space = %s)\n"),dirname,fspstr);
    g_free(fspstr);
    res=do_yesno_dialog(msg);
  }
  else
    {
      msg=g_strdup_printf
	(_("A directory named\n%s\nalready exists.\nLiVES could not write to this directory or read its free space.\nPlease select another location.\n"),
	 dirname);
      do_error_dialog(msg);
    }
  g_free(msg);
  return res;
}





static boolean prompt_new_dir(gchar *dirname, uint64_t freespace, boolean wrtable) {
  boolean res=FALSE;
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




boolean do_tempdir_query(void) {
  _entryw *tdentry;
  uint64_t freesp;

  int response;
  boolean ok=FALSE;

  gchar *dirname;
#ifndef IS_MINGW
  gchar *com;
#endif

 top:

  tdentry=create_rename_dialog(6);

  while (!ok) {
    response=lives_dialog_run(LIVES_DIALOG(tdentry->dialog));

    if (response==GTK_RESPONSE_CANCEL) {
      return FALSE;
    }
    dirname=g_strdup(lives_entry_get_text(LIVES_ENTRY(tdentry->entry)));

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

  lives_widget_destroy(tdentry->dialog);
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
  g_snprintf(mainw->first_info_file,PATH_MAX,"%s"G_DIR_SEPARATOR_S".info.%d",prefs->tmpdir,capable->mainpid);
#else
  g_snprintf(mainw->first_info_file,PATH_MAX,"%s"G_DIR_SEPARATOR_S"info.%d",prefs->tmpdir,capable->mainpid);
#endif

  g_free(dirname);
  return TRUE;
}





static void on_init_aplayer_toggled (GtkToggleButton *tbutton, gpointer user_data) {
  int audp=GPOINTER_TO_INT(user_data);

  if (!lives_toggle_button_get_active(tbutton)) return;

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




boolean do_audio_choice_dialog(short startup_phase) {
  GtkWidget *dialog,*dialog_vbox,*radiobutton0,*radiobutton1,*radiobutton2,*radiobutton3,*label;
  GtkWidget *okbutton,*cancelbutton;
  GtkWidget *hbox;

  GtkAccelGroup *accel_group;

  GSList *radiobutton_group = NULL;

  gchar *txt0,*txt1,*txt2,*txt3,*txt4,*txt5,*txt6,*txt7,*msg;

  int response;

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

  dialog = lives_standard_dialog_new (_("LiVES: - Choose an audio player"),FALSE);

  accel_group = GTK_ACCEL_GROUP(lives_accel_group_new ());
  lives_window_add_accel_group (LIVES_WINDOW (dialog), accel_group);

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(dialog));
  
  label=lives_standard_label_new(msg);
  lives_container_add (LIVES_CONTAINER (dialog_vbox), label);
  g_free(msg);



#ifdef HAVE_PULSE_AUDIO
  hbox = lives_hbox_new (FALSE, 0);
  lives_box_pack_start (LIVES_BOX (dialog_vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

  radiobutton0 = lives_standard_radio_button_new ( _("Use _pulse audio player"),TRUE,radiobutton_group,LIVES_BOX(hbox),NULL);
  radiobutton_group = lives_radio_button_get_group (LIVES_RADIO_BUTTON (radiobutton0));

  if (prefs->audio_player==-1) prefs->audio_player=AUD_PLAYER_PULSE;

  if (prefs->audio_player==AUD_PLAYER_PULSE) {
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(radiobutton0),TRUE);
    set_pref("audio_player","pulse");
  }

  g_signal_connect (GTK_OBJECT (radiobutton0), "toggled",
                      G_CALLBACK (on_init_aplayer_toggled),
                      GINT_TO_POINTER(AUD_PLAYER_PULSE));


#endif


#ifdef ENABLE_JACK
  hbox = lives_hbox_new (FALSE, 0);
  lives_box_pack_start (LIVES_BOX (dialog_vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

  radiobutton1 = lives_standard_radio_button_new(_("Use _jack audio player"),TRUE,radiobutton_group,LIVES_BOX(hbox),NULL);
  radiobutton_group = lives_radio_button_get_group (LIVES_RADIO_BUTTON (radiobutton1));

  if (prefs->audio_player==AUD_PLAYER_JACK||!capable->has_pulse_audio||prefs->audio_player==-1) {
    prefs->audio_player=AUD_PLAYER_JACK;
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(radiobutton1),TRUE);
    set_pref("audio_player","jack");
  }

  g_signal_connect (GTK_OBJECT (radiobutton1), "toggled",
                      G_CALLBACK (on_init_aplayer_toggled),
                      GINT_TO_POINTER(AUD_PLAYER_JACK));


#endif

  if (capable->has_sox_play) {
    hbox = lives_hbox_new (FALSE, 0);
    lives_box_pack_start (LIVES_BOX (dialog_vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

    radiobutton2 = lives_standard_radio_button_new (_("Use _sox audio player"),TRUE,radiobutton_group,LIVES_BOX(hbox),NULL);
    radiobutton_group = lives_radio_button_get_group (LIVES_RADIO_BUTTON (radiobutton2));

    if (prefs->audio_player==-1) prefs->audio_player=AUD_PLAYER_SOX;

    if (prefs->audio_player==AUD_PLAYER_SOX) {
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(radiobutton2),TRUE);
      set_pref("audio_player","sox");
    }

    g_signal_connect (GTK_OBJECT (radiobutton2), "toggled",
                      G_CALLBACK (on_init_aplayer_toggled),
                      GINT_TO_POINTER(AUD_PLAYER_SOX));


  }

  if (capable->has_mplayer) {
    hbox = lives_hbox_new (FALSE, 0);
    lives_box_pack_start (LIVES_BOX (dialog_vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

    radiobutton3 = lives_standard_radio_button_new (_("Use _mplayer audio player"),TRUE,radiobutton_group,LIVES_BOX(hbox),NULL);

    if (prefs->audio_player==-1) prefs->audio_player=AUD_PLAYER_MPLAYER;

    if (prefs->audio_player==AUD_PLAYER_MPLAYER) {
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(radiobutton3),TRUE);
      set_pref("audio_player","mplayer");
    }
						    
    g_signal_connect (GTK_OBJECT (radiobutton3), "toggled",
		      G_CALLBACK (on_init_aplayer_toggled),
                      GINT_TO_POINTER(AUD_PLAYER_MPLAYER));


  }


  cancelbutton = lives_button_new_from_stock ("gtk-cancel");
  lives_widget_show (cancelbutton);
  lives_dialog_add_action_widget (LIVES_DIALOG (dialog), cancelbutton, GTK_RESPONSE_CANCEL);

  lives_widget_add_accelerator (cancelbutton, "activate", accel_group,
                              LIVES_KEY_Escape, (GdkModifierType)0, (GtkAccelFlags)0);

  okbutton = lives_button_new_from_stock ("gtk-go-forward");
  lives_button_set_label(GTK_BUTTON(okbutton),_("_Next"));
  lives_widget_show (okbutton);
  lives_dialog_add_action_widget (LIVES_DIALOG (dialog), okbutton, GTK_RESPONSE_OK);
  lives_widget_set_can_focus_and_default (okbutton);
  lives_widget_grab_default(okbutton);
  lives_widget_grab_focus(okbutton);


  if (prefs->audio_player==-1) {
    do_no_mplayer_sox_error();
    return GTK_RESPONSE_CANCEL;
  }

  lives_widget_show_all(dialog);

  response=lives_dialog_run(LIVES_DIALOG(dialog));

  lives_widget_destroy(dialog);

  if (prefs->audio_player==AUD_PLAYER_SOX||prefs->audio_player==AUD_PLAYER_MPLAYER) {
    lives_widget_hide(mainw->vol_toolitem);
    if (mainw->vol_label!=NULL) lives_widget_hide(mainw->vol_label);
    lives_widget_hide (mainw->recaudio_submenu);
  }

  lives_widget_context_update();

  return (response==GTK_RESPONSE_OK);
}


static void add_test(GtkWidget *table, int row, gchar *ttext, boolean noskip) {
  GtkWidget *label=lives_standard_label_new(ttext);

  lives_table_attach (LIVES_TABLE (table), label, 0, 1, row, row+1, (GtkAttachOptions)0, (GtkAttachOptions)0, 10, 10);
  lives_widget_show(label);

  if (!noskip) {
    GtkWidget *image=lives_image_new_from_stock(GTK_STOCK_REMOVE,LIVES_ICON_SIZE_LARGE_TOOLBAR);
    // TRANSLATORS - as in "skipped test"
    label=lives_standard_label_new(_("Skipped"));

    lives_table_attach (LIVES_TABLE (table), label, 1, 2, row, row+1, (GtkAttachOptions)0, (GtkAttachOptions)0, 10, 10);
    lives_widget_show(label);

    lives_table_attach (LIVES_TABLE (table), image, 2, 3, row, row+1, (GtkAttachOptions)0, (GtkAttachOptions)0, 0, 10);
    lives_widget_show(image);
  }

  lives_widget_context_update();
}


static boolean pass_test(GtkWidget *table, int row) {
  // TRANSLATORS - as in "passed test"
  GtkWidget *label=lives_standard_label_new(_("Passed"));
  GtkWidget *image=lives_image_new_from_stock(GTK_STOCK_APPLY,LIVES_ICON_SIZE_LARGE_TOOLBAR);

  lives_table_attach (LIVES_TABLE (table), label, 1, 2, row, row+1, (GtkAttachOptions)0, (GtkAttachOptions)0, 10, 10);
  lives_widget_show(label);

  lives_table_attach (LIVES_TABLE (table), image, 2, 3, row, row+1, (GtkAttachOptions)0, (GtkAttachOptions)0, 0, 10);
  lives_widget_show(image);

  lives_widget_context_update();
  return TRUE;
}


static boolean fail_test(GtkWidget *table, int row, gchar *ftext) {
  GtkWidget *label;
  GtkWidget *image=lives_image_new_from_stock(GTK_STOCK_CANCEL,LIVES_ICON_SIZE_LARGE_TOOLBAR);

  label=lives_standard_label_new(ftext);

  lives_table_attach (LIVES_TABLE (table), label, 3, 4, row, row+1, (GtkAttachOptions)0, (GtkAttachOptions)0, 10, 10);
  lives_widget_show(label);
  
  // TRANSLATORS - as in "failed test"
  label=lives_standard_label_new(_("Failed"));

  lives_table_attach (LIVES_TABLE (table), label, 1, 2, row, row+1, (GtkAttachOptions)0, (GtkAttachOptions)0, 10, 10);
  lives_widget_show(label);

  lives_table_attach (LIVES_TABLE (table), image, 2, 3, row, row+1, (GtkAttachOptions)0, (GtkAttachOptions)0, 0, 10);
  lives_widget_show(image);

  lives_widget_context_update();
  
  return FALSE;
}


LIVES_INLINE gchar *get_resource(gchar *fname) {
  return g_strdup_printf("%s%sresources/%s",prefs->prefix_dir,DATA_DIR,fname);
}



boolean do_startup_tests(boolean tshoot) {
  GtkWidget *dialog;
  GtkWidget *dialog_vbox;

  GtkWidget *label;
  GtkWidget *table;
  GtkWidget *okbutton;
  GtkWidget *cancelbutton;

  GtkAccelGroup *accel_group;

  gchar *com,*rname,*afile,*tmp;
  gchar *image_ext=g_strdup(prefs->image_ext);
  gchar *title;

  uint8_t *abuff;

  size_t fsize;

  boolean success,success2,success3,success4;
  boolean imgext_switched=FALSE;

  int response,res;
  int current_file=mainw->current_file;

  int out_fd,info_fd;

  mainw->suppress_dprint=TRUE;
  mainw->cancelled=CANCEL_NONE;

  if (mainw->multitrack!=NULL) {
    if (mainw->multitrack->idlefunc>0) {
      g_source_remove(mainw->multitrack->idlefunc);
      mainw->multitrack->idlefunc=0;
    }
    mt_desensitise(mainw->multitrack);
  }

  if (!tshoot) {
    title=g_strdup(_("LiVES: - Testing Configuration"));
  }
  else {
    title=g_strdup(_("LiVES: - Troubleshoot"));
  }


  dialog = lives_standard_dialog_new (title,FALSE);

  accel_group = GTK_ACCEL_GROUP(lives_accel_group_new ());
  lives_window_add_accel_group (LIVES_WINDOW (dialog), accel_group);

  g_free(title);

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(dialog));

  label=lives_standard_label_new(_("LiVES will now run some basic configuration tests\n"));
  lives_container_add (LIVES_CONTAINER (dialog_vbox), label);

  cancelbutton = lives_button_new_from_stock ("gtk-cancel");
  lives_widget_show (cancelbutton);
  lives_dialog_add_action_widget (LIVES_DIALOG (dialog), cancelbutton, GTK_RESPONSE_CANCEL);

  lives_widget_add_accelerator (cancelbutton, "activate", accel_group,
                              LIVES_KEY_Escape, (GdkModifierType)0, (GtkAccelFlags)0);

  if (!tshoot) {
    okbutton = lives_button_new_from_stock ("gtk-go-forward");
    lives_button_set_label(GTK_BUTTON(okbutton),_("_Next"));
  }
  else okbutton = lives_button_new_from_stock ("gtk-ok");
  lives_widget_show (okbutton);
  lives_dialog_add_action_widget (LIVES_DIALOG (dialog), okbutton, GTK_RESPONSE_OK);
  lives_widget_set_can_focus_and_default (okbutton);
  lives_widget_grab_default(okbutton);
  lives_widget_grab_focus(okbutton);


  lives_widget_set_sensitive(okbutton,FALSE);


  table = lives_table_new (10, 4, FALSE);
  lives_container_add (LIVES_CONTAINER (dialog_vbox), table);

  lives_widget_show_all(dialog);

  lives_widget_context_update();


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
      abuff=(uint8_t *)lives_calloc(44100,4);
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
	lives_widget_context_update();
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
    lives_widget_destroy(dialog);
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
      lives_widget_destroy(dialog);
      lives_widget_context_update();
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
    lives_widget_destroy(dialog);
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
	if (!strcmp(prefs->image_ext,"png")) imgext_switched=TRUE;
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

  lives_widget_set_sensitive(okbutton,TRUE);
  if (tshoot) {
    lives_widget_hide(cancelbutton); 
    if (imgext_switched) {
      label=lives_standard_label_new(_("\n\n    Image decoding type has been switched to jpeg. You can revert this in Preferences/Decoding.    \n"));
      lives_container_add (LIVES_CONTAINER (dialog_vbox), label);
    }
    lives_widget_show(label);
  }
  else {
    label=lives_standard_label_new(_("\n\n    Click Cancel to exit and install any missing components, or Next to continue    \n"));
    lives_container_add (LIVES_CONTAINER (dialog_vbox), label);
    lives_widget_show(label);
  }

  response=lives_dialog_run(LIVES_DIALOG(dialog));

  lives_widget_destroy(dialog);
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
  GtkWidget *hbox;
  GSList *radiobutton_group = NULL;
  gchar *txt1,*txt2,*txt3,*msg;

  txt1=g_strdup(_("\n\nFinally, you can choose the default startup interface for LiVES.\n"));
  txt2=g_strdup(_("\n\nLiVES has two main interfaces and you can start up with either of them.\n"));
  txt3=g_strdup(_("\n\nThe default can always be changed later from Preferences.\n"));


  msg=g_strdup_printf("%s%s%s",txt1,txt2,txt3);


  g_free(txt1);
  g_free(txt2);
  g_free(txt3);

  dialog = lives_standard_dialog_new (_("LiVES: - Choose the startup interface"),FALSE);

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(dialog));
  
  label=lives_standard_label_new(msg);
  lives_container_add (LIVES_CONTAINER (dialog_vbox), label);
  g_free(msg);


  hbox = lives_hbox_new (FALSE, 0);
  lives_box_pack_start (LIVES_BOX (dialog_vbox), hbox, FALSE, FALSE, widget_opts.packing_height);
  radiobutton0 = lives_standard_radio_button_new (_("Start in _Clip Edit mode"),TRUE,radiobutton_group,LIVES_BOX(hbox),NULL);
  radiobutton_group = lives_radio_button_get_group (LIVES_RADIO_BUTTON (radiobutton0));

  label=lives_standard_label_new(_("This is the best choice for simple editing tasks and for VJs\n"));

  lives_box_pack_start (LIVES_BOX (dialog_vbox), label, FALSE, FALSE, widget_opts.packing_height);

  hbox = lives_hbox_new (FALSE, 0);
  lives_box_pack_start (LIVES_BOX (dialog_vbox), hbox, FALSE, FALSE, widget_opts.packing_height);
  radiobutton1 = lives_standard_radio_button_new (_("Start in _Multitrack mode"),TRUE,radiobutton_group,LIVES_BOX(hbox),NULL);

  label=lives_standard_label_new(_("This is a better choice for complex editing tasks involving multiple clips.\n"));

  lives_box_pack_start (LIVES_BOX (dialog_vbox), label, FALSE, FALSE, widget_opts.packing_height);

  if (prefs->startup_interface==STARTUP_MT) {
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(radiobutton1),TRUE);
  }

  okbutton = lives_button_new_from_stock ("gtk-go-forward");
  lives_button_set_label(GTK_BUTTON(okbutton),_("_Finish"));

  lives_widget_show (okbutton);
  lives_dialog_add_action_widget (LIVES_DIALOG (dialog), okbutton, GTK_RESPONSE_OK);
  lives_widget_set_can_focus_and_default (okbutton);
  lives_widget_grab_default(okbutton);
  lives_widget_grab_focus(okbutton);


  lives_widget_show_all(dialog);

  lives_dialog_run(LIVES_DIALOG(dialog));


  if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(radiobutton1))) 
    future_prefs->startup_interface=prefs->startup_interface=STARTUP_MT;
  
  set_int_pref("startup_interface",prefs->startup_interface);

  lives_widget_destroy(dialog);

  lives_widget_context_update();

}



void on_troubleshoot_activate (GtkMenuItem *menuitem, gpointer user_data) {
  do_startup_tests(TRUE);
}
