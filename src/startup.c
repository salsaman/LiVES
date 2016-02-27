// startup.c
// LiVES
// (c) G. Finch 2010 - 2016 <salsaman@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details


// functions for first time startup

#include "main.h"
#include "interface.h"





static boolean prompt_existing_dir(char *dirname, uint64_t freespace, boolean wrtable) {
  char *msg;
  boolean res=FALSE;

  if (wrtable) {
    char *fspstr=lives_format_storage_space_string(freespace);
    msg=lives_strdup_printf
        (_("A directory named\n%s\nalready exists. Do you wish to use this directory ?\n\n(Free space = %s)\n"),dirname,fspstr);
    lives_free(fspstr);
    res=do_yesno_dialog(msg);
  } else {
    msg=lives_strdup_printf
        (_("A directory named\n%s\nalready exists.\nLiVES could not write to this directory or read its free space.\nPlease select another location.\n"),
         dirname);
    do_error_dialog(msg);
  }
  lives_free(msg);
  return res;
}





static boolean prompt_new_dir(char *dirname, uint64_t freespace, boolean wrtable) {
  boolean res=FALSE;
  char *msg;
  if (wrtable) {
    char *fspstr=lives_format_storage_space_string(freespace);
    msg=lives_strdup_printf(_("\nCreate the directory\n%s\n?\n\n(Free space = %s)"),dirname,fspstr);
    lives_free(fspstr);
    res=do_warning_dialog(msg);
  } else {
    msg=lives_strdup_printf(_("\nLiVES could not write to the directory\n%s\nPlease try again and choose a different location.\n"),dirname);
    do_error_dialog(msg);
  }

  lives_free(msg);
  return res;
}




boolean do_tempdir_query(void) {
  _entryw *tdentry;
  uint64_t freesp;

  int response;
  boolean ok=FALSE;

  char *dirname;

top:

  tdentry=create_rename_dialog(6);

  while (!ok) {
    response=lives_dialog_run(LIVES_DIALOG(tdentry->dialog));

    if (response==LIVES_RESPONSE_CANCEL) {
      return FALSE;
    }
    dirname=lives_strdup(lives_entry_get_text(LIVES_ENTRY(tdentry->entry)));

    if (strcmp(dirname+strlen(dirname)-1,LIVES_DIR_SEPARATOR_S)) {
      char *tmp=lives_strdup_printf("%s%s",dirname,LIVES_DIR_SEPARATOR_S);
      lives_free(dirname);
      dirname=tmp;
    }

    if (strlen(dirname)>(PATH_MAX-1)) {
      do_blocking_error_dialog(_("Directory name is too long !"));
      lives_free(dirname);
      continue;
    }

    if (!check_dir_access(dirname)) {
      do_dir_perm_error(dirname);
      lives_free(dirname);
      continue;
    }


    if (lives_file_test(dirname,LIVES_FILE_TEST_IS_DIR)) {
      if (is_writeable_dir(dirname)) {
        freesp=get_fs_free(dirname);
        if (!prompt_existing_dir(dirname,freesp,TRUE)) {
          lives_free(dirname);
          continue;
        }
      } else {
        if (!prompt_existing_dir(dirname,0,FALSE)) {
          lives_free(dirname);
          continue;
        }
      }
    } else {
      if (is_writeable_dir(dirname)) {
        freesp=get_fs_free(dirname);
        if (!prompt_new_dir(dirname,freesp,TRUE)) {
          lives_rmdir(dirname,FALSE);
          lives_free(dirname);
          continue;
        }
      } else {
        if (!prompt_new_dir(dirname,0,FALSE)) {
          lives_rmdir(dirname,FALSE);
          lives_free(dirname);
          continue;
        }
      }
    }

    ok=TRUE;
  }

  lives_widget_destroy(tdentry->dialog);
  lives_free(tdentry);

  mainw->com_failed=FALSE;

  if (!lives_file_test(dirname,LIVES_FILE_TEST_IS_DIR)) {
    if (lives_mkdir_with_parents(dirname,S_IRWXU)==-1) goto top;
  }

#ifndef IS_MINGW
  lives_chmod(dirname,"777");
#endif

  lives_snprintf(prefs->tmpdir,PATH_MAX,"%s",dirname);
  lives_snprintf(future_prefs->tmpdir,PATH_MAX,"%s",prefs->tmpdir);

  set_pref("tempdir",prefs->tmpdir);
  set_pref("session_tempdir",prefs->tmpdir);

#ifndef IS_MINGW
  lives_snprintf(mainw->first_info_file,PATH_MAX,"%s"LIVES_DIR_SEPARATOR_S".info.%d",prefs->tmpdir,capable->mainpid);
#else
  lives_snprintf(mainw->first_info_file,PATH_MAX,"%s"LIVES_DIR_SEPARATOR_S"info.%d",prefs->tmpdir,capable->mainpid);
#endif

  lives_free(dirname);
  return TRUE;
}





static void on_init_aplayer_toggled(LiVESToggleButton *tbutton, livespointer user_data) {
  int audp=LIVES_POINTER_TO_INT(user_data);

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
  case AUD_PLAYER_MPLAYER2:
    set_pref("audio_player","mplayer2");
    break;
  }

}




boolean do_audio_choice_dialog(short startup_phase) {
  LiVESWidget *dialog,*dialog_vbox,*radiobutton2,*radiobutton3,*radiobutton4,*label;
  LiVESWidget *okbutton,*cancelbutton;
  LiVESWidget *hbox;

#ifdef HAVE_PULSE_AUDIO
  LiVESWidget *radiobutton0;
#endif

#ifdef ENABLE_JACK
  LiVESWidget *radiobutton1;
#endif

  LiVESAccelGroup *accel_group;

  LiVESSList *radiobutton_group = NULL;

  char *txt0,*txt1,*txt2,*txt3,*txt4,*txt5,*txt6,*txt7,*msg;

  int response;

  if (startup_phase==2) {
    txt0=lives_strdup(_("LiVES FAILED TO START YOUR SELECTED AUDIO PLAYER !\n\n"));
  } else {
    prefs->audio_player=-1;
    txt0=lives_strdup("");
  }

  txt1=lives_strdup(_("Before starting LiVES, you need to choose an audio player.\n\nPULSE AUDIO is recommended for most users"));

#ifndef HAVE_PULSE_AUDIO
  txt2=lives_strdup(_(", but this version of LiVES was not compiled with pulse audio support.\n\n"));
#else
  if (!capable->has_pulse_audio) {
    txt2=lives_strdup(
           _(", but you do not have pulse audio installed on your system.\n You are advised to install pulse audio first before running LiVES.\n\n"));
  } else txt2=lives_strdup(".\n\n");
#endif

  txt3=lives_strdup(_("JACK audio is recommended for pro users"));

#ifndef ENABLE_JACK
  txt4=lives_strdup(_(", but this version of LiVES was not compiled with jack audio support.\n\n"));
#else
  if (!capable->has_jackd) {
    txt4=lives_strdup(_(", but you do not have jackd installed. You may wish to install jackd first before running LiVES.\n\n"));
  } else {
    txt4=lives_strdup(
           _(", but may prevent LiVES from starting on some systems.\nIf LiVES will not start with jack, you can restart and try with another audio player instead.\n\n"));
  }
#endif

  txt5=lives_strdup(_("SOX may be used if neither of the preceding players work, "));

  if (capable->has_sox_play) {
    txt6=lives_strdup(_("but some audio features will be disabled.\n\n"));
  } else {
    txt6=lives_strdup(_("but you do not have sox installed.\nYou are advised to install it before running LiVES.\n\n"));
  }

  if (capable->has_mplayer||capable->has_mplayer2) {
    txt7=lives_strdup(_("The MPLAYER/MPLAYER2 audio player is only recommended for testing purposes.\n\n"));
  } else {
    txt7=lives_strdup("");
  }

  msg=lives_strdup_printf("%s%s%s%s%s%s%s%s",txt0,txt1,txt2,txt3,txt4,txt5,txt6,txt7);

  lives_free(txt0);
  lives_free(txt1);
  lives_free(txt2);
  lives_free(txt3);
  lives_free(txt4);
  lives_free(txt5);
  lives_free(txt6);
  lives_free(txt7);

  dialog = lives_standard_dialog_new(_("LiVES: - Choose an audio player"),FALSE,-1,-1);

  accel_group = LIVES_ACCEL_GROUP(lives_accel_group_new());
  lives_window_add_accel_group(LIVES_WINDOW(dialog), accel_group);

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(dialog));

  label=lives_standard_label_new(msg);
  lives_container_add(LIVES_CONTAINER(dialog_vbox), label);
  lives_free(msg);



#ifdef HAVE_PULSE_AUDIO
  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

  radiobutton0 = lives_standard_radio_button_new(_("Use _pulse audio player"),TRUE,radiobutton_group,LIVES_BOX(hbox),NULL);
  radiobutton_group = lives_radio_button_get_group(LIVES_RADIO_BUTTON(radiobutton0));

  if (prefs->audio_player==-1) prefs->audio_player=AUD_PLAYER_PULSE;

  if (prefs->audio_player==AUD_PLAYER_PULSE) {
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(radiobutton0),TRUE);
    set_pref("audio_player","pulse");
  }

  lives_signal_connect(LIVES_GUI_OBJECT(radiobutton0), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(on_init_aplayer_toggled),
                       LIVES_INT_TO_POINTER(AUD_PLAYER_PULSE));


#endif


#ifdef ENABLE_JACK
  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

  radiobutton1 = lives_standard_radio_button_new(_("Use _jack audio player"),TRUE,radiobutton_group,LIVES_BOX(hbox),NULL);
  radiobutton_group = lives_radio_button_get_group(LIVES_RADIO_BUTTON(radiobutton1));

  if (prefs->audio_player==AUD_PLAYER_JACK||!capable->has_pulse_audio||prefs->audio_player==-1) {
    prefs->audio_player=AUD_PLAYER_JACK;
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(radiobutton1),TRUE);
    set_pref("audio_player","jack");
  }

  lives_signal_connect(LIVES_GUI_OBJECT(radiobutton1), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(on_init_aplayer_toggled),
                       LIVES_INT_TO_POINTER(AUD_PLAYER_JACK));


#endif

  if (capable->has_sox_play) {
    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

    radiobutton2 = lives_standard_radio_button_new(_("Use _sox audio player"),TRUE,radiobutton_group,LIVES_BOX(hbox),NULL);
    radiobutton_group = lives_radio_button_get_group(LIVES_RADIO_BUTTON(radiobutton2));

    if (prefs->audio_player==-1) prefs->audio_player=AUD_PLAYER_SOX;

    if (prefs->audio_player==AUD_PLAYER_SOX) {
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(radiobutton2),TRUE);
      set_pref("audio_player","sox");
    }

    lives_signal_connect(LIVES_GUI_OBJECT(radiobutton2), LIVES_WIDGET_TOGGLED_SIGNAL,
                         LIVES_GUI_CALLBACK(on_init_aplayer_toggled),
                         LIVES_INT_TO_POINTER(AUD_PLAYER_SOX));


  }

  if (capable->has_mplayer) {
    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

    radiobutton3 = lives_standard_radio_button_new(_("Use _mplayer audio player"),TRUE,radiobutton_group,LIVES_BOX(hbox),NULL);

    if (prefs->audio_player==-1) prefs->audio_player=AUD_PLAYER_MPLAYER;

    if (prefs->audio_player==AUD_PLAYER_MPLAYER) {
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(radiobutton3),TRUE);
      set_pref("audio_player","mplayer");
    }

    lives_signal_connect(LIVES_GUI_OBJECT(radiobutton3), LIVES_WIDGET_TOGGLED_SIGNAL,
                         LIVES_GUI_CALLBACK(on_init_aplayer_toggled),
                         LIVES_INT_TO_POINTER(AUD_PLAYER_MPLAYER));


  }

  if (capable->has_mplayer2) {
    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

    radiobutton4 = lives_standard_radio_button_new(_("Use _mplayer2 audio player"),TRUE,radiobutton_group,LIVES_BOX(hbox),NULL);

    if (prefs->audio_player==-1) prefs->audio_player=AUD_PLAYER_MPLAYER2;

    if (prefs->audio_player==AUD_PLAYER_MPLAYER2) {
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(radiobutton4),TRUE);
      set_pref("audio_player","mplayer2");
    }

    lives_signal_connect(LIVES_GUI_OBJECT(radiobutton4), LIVES_WIDGET_TOGGLED_SIGNAL,
                         LIVES_GUI_CALLBACK(on_init_aplayer_toggled),
                         LIVES_INT_TO_POINTER(AUD_PLAYER_MPLAYER2));


  }

  cancelbutton = lives_button_new_from_stock(LIVES_STOCK_CANCEL,NULL);

  lives_dialog_add_action_widget(LIVES_DIALOG(dialog), cancelbutton, LIVES_RESPONSE_CANCEL);

  lives_widget_add_accelerator(cancelbutton, LIVES_WIDGET_CLICKED_SIGNAL, accel_group,
                               LIVES_KEY_Escape, (LiVESXModifierType)0, (LiVESAccelFlags)0);

  okbutton = lives_button_new_from_stock(LIVES_STOCK_GO_FORWARD,_("_Next"));

  lives_dialog_add_action_widget(LIVES_DIALOG(dialog), okbutton, LIVES_RESPONSE_OK);
  lives_widget_set_can_focus_and_default(okbutton);
  lives_widget_grab_default(okbutton);
  lives_widget_grab_focus(okbutton);


  if (prefs->audio_player==-1) {
    do_no_mplayer_sox_error();
    return LIVES_RESPONSE_CANCEL;
  }

  lives_widget_show_all(dialog);

  response=lives_dialog_run(LIVES_DIALOG(dialog));

  lives_widget_destroy(dialog);

  if (!is_realtime_aplayer(prefs->audio_player)) {
    lives_widget_hide(mainw->vol_toolitem);
    if (mainw->vol_label!=NULL) lives_widget_hide(mainw->vol_label);
    lives_widget_hide(mainw->recaudio_submenu);
  }

  lives_widget_context_update();

  return (response==LIVES_RESPONSE_OK);
}


static void add_test(LiVESWidget *table, int row, char *ttext, boolean noskip) {
  LiVESWidget *label=lives_standard_label_new(ttext);

  lives_table_attach(LIVES_TABLE(table), label, 0, 1, row, row+1, (LiVESAttachOptions)0, (LiVESAttachOptions)0, 10, 10);
  lives_widget_show(label);

  if (!noskip) {
    LiVESWidget *image=lives_image_new_from_stock(LIVES_STOCK_REMOVE,LIVES_ICON_SIZE_LARGE_TOOLBAR);
    // TRANSLATORS - as in "skipped test"
    label=lives_standard_label_new(_("Skipped"));

    lives_table_attach(LIVES_TABLE(table), label, 1, 2, row, row+1, (LiVESAttachOptions)0, (LiVESAttachOptions)0, 10, 10);
    lives_widget_show(label);

    lives_table_attach(LIVES_TABLE(table), image, 2, 3, row, row+1, (LiVESAttachOptions)0, (LiVESAttachOptions)0, 0, 10);
    lives_widget_show(image);
  }

  lives_widget_context_update();
}


static boolean pass_test(LiVESWidget *table, int row) {
  // TRANSLATORS - as in "passed test"
  LiVESWidget *label=lives_standard_label_new(_("Passed"));

#if GTK_CHECK_VERSION(3,10,0)
  LiVESWidget *image=lives_image_new_from_stock(LIVES_STOCK_ADD,LIVES_ICON_SIZE_LARGE_TOOLBAR);
#else
  LiVESWidget *image=lives_image_new_from_stock(LIVES_STOCK_APPLY,LIVES_ICON_SIZE_LARGE_TOOLBAR);
#endif

  lives_table_attach(LIVES_TABLE(table), label, 1, 2, row, row+1, (LiVESAttachOptions)0, (LiVESAttachOptions)0, 10, 10);
  lives_widget_show(label);

  lives_table_attach(LIVES_TABLE(table), image, 2, 3, row, row+1, (LiVESAttachOptions)0, (LiVESAttachOptions)0, 0, 10);
  lives_widget_show(image);

  lives_widget_context_update();
  return TRUE;
}


static boolean fail_test(LiVESWidget *table, int row, char *ftext) {
  LiVESWidget *label;
#if GTK_CHECK_VERSION(3,10,0)
  LiVESWidget *image=lives_image_new_from_stock(LIVES_STOCK_REMOVE,LIVES_ICON_SIZE_LARGE_TOOLBAR);
#else
  LiVESWidget *image=lives_image_new_from_stock(LIVES_STOCK_CANCEL,LIVES_ICON_SIZE_LARGE_TOOLBAR);
#endif

  label=lives_standard_label_new(ftext);

  lives_table_attach(LIVES_TABLE(table), label, 3, 4, row, row+1, (LiVESAttachOptions)0, (LiVESAttachOptions)0, 10, 10);
  lives_widget_show(label);

  // TRANSLATORS - as in "failed test"
  label=lives_standard_label_new(_("Failed"));

  lives_table_attach(LIVES_TABLE(table), label, 1, 2, row, row+1, (LiVESAttachOptions)0, (LiVESAttachOptions)0, 10, 10);
  lives_widget_show(label);

  lives_table_attach(LIVES_TABLE(table), image, 2, 3, row, row+1, (LiVESAttachOptions)0, (LiVESAttachOptions)0, 0, 10);
  lives_widget_show(image);

  lives_widget_context_update();

  return FALSE;
}


LIVES_INLINE char *get_resource(char *fname) {
  return lives_strdup_printf("%s%sresources/%s",prefs->prefix_dir,DATA_DIR,fname);
}



boolean do_startup_tests(boolean tshoot) {
  LiVESWidget *dialog;
  LiVESWidget *dialog_vbox;

  LiVESWidget *label;
  LiVESWidget *table;
  LiVESWidget *okbutton;
  LiVESWidget *cancelbutton;

  LiVESAccelGroup *accel_group;

  char *com,*rname,*afile,*tmp;
  char *image_ext=lives_strdup(prefs->image_ext);
  char *title;

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
      lives_source_remove(mainw->multitrack->idlefunc);
      mainw->multitrack->idlefunc=0;
    }
    mt_desensitise(mainw->multitrack);
  }

  if (!tshoot) {
    title=lives_strdup(_("LiVES: - Testing Configuration"));
  } else {
    title=lives_strdup(_("LiVES: - Troubleshoot"));
  }


  dialog = lives_standard_dialog_new(title,FALSE,-1,-1);

  accel_group = LIVES_ACCEL_GROUP(lives_accel_group_new());
  lives_window_add_accel_group(LIVES_WINDOW(dialog), accel_group);

  lives_free(title);

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(dialog));

  label=lives_standard_label_new(_("LiVES will now run some basic configuration tests\n"));
  lives_container_add(LIVES_CONTAINER(dialog_vbox), label);

  cancelbutton = lives_button_new_from_stock(LIVES_STOCK_CANCEL,NULL);

  lives_dialog_add_action_widget(LIVES_DIALOG(dialog), cancelbutton, LIVES_RESPONSE_CANCEL);

  lives_widget_add_accelerator(cancelbutton, LIVES_WIDGET_CLICKED_SIGNAL, accel_group,
                               LIVES_KEY_Escape, (LiVESXModifierType)0, (LiVESAccelFlags)0);

  if (!tshoot) {
    okbutton = lives_button_new_from_stock(LIVES_STOCK_GO_FORWARD,_("_Next"));
  } else okbutton = lives_button_new_from_stock(LIVES_STOCK_OK,NULL);

  lives_dialog_add_action_widget(LIVES_DIALOG(dialog), okbutton, LIVES_RESPONSE_OK);
  lives_widget_set_can_focus_and_default(okbutton);
  lives_widget_grab_default(okbutton);
  lives_widget_grab_focus(okbutton);


  lives_widget_set_sensitive(okbutton,FALSE);


  table = lives_table_new(10, 4, FALSE);
  lives_container_add(LIVES_CONTAINER(dialog_vbox), table);

  lives_widget_show_all(dialog);

  lives_widget_context_update();


  // check for sox presence

  add_test(table,0,_("Checking for \"sox\" presence"),TRUE);


  if (!capable->has_sox_sox) {
    success=fail_test(table,0,_("You should install sox to be able to use all the audio features in LiVES"));

  } else {
    success=pass_test(table,0);
  }

  // test if sox can convert raw 44100 -> wav 22050
  add_test(table,1,_("Checking if sox can convert audio"),success);

  if (!tshoot) set_pref("default_image_format","png");
  lives_snprintf(prefs->image_ext,16,"%s",LIVES_FILE_EXT_PNG);

  get_temp_handle(mainw->first_free_file,TRUE);

  if (success) {

    info_fd=-1;

    lives_rm(cfile->info_file);

    // write 1 second of silence
    afile=lives_build_filename(prefs->tmpdir,cfile->handle,"audio",NULL);
    out_fd=open(afile,O_WRONLY|O_CREAT,S_IRUSR|S_IWUSR);

    if (out_fd<0) mainw->write_failed=TRUE;
    else mainw->write_failed=FALSE;

    if (!mainw->write_failed) {
      abuff=(uint8_t *)lives_calloc(44100,4);
      if (!abuff) {
        tmp=lives_strdup(_("Unable to allocate 176400 bytes memory."));
        fail_test(table,1,tmp);
        lives_free(tmp);
      } else {
#ifdef IS_MINGW
        setmode(out_fd, O_BINARY);
#endif
        lives_write(out_fd,abuff,176400,TRUE);
        close(out_fd);
        lives_free(abuff);
      }
    }

    if (mainw->write_failed) {
      tmp=lives_strdup_printf(_("Unable to write to: %s"),afile);
      fail_test(table,1,tmp);
      lives_free(tmp);
    }

    lives_free(afile);

    if (!mainw->write_failed) {
      afile=lives_build_filename(prefs->tmpdir,cfile->handle,"testout.wav",NULL);

      mainw->com_failed=FALSE;
      com=lives_strdup_printf("%s export_audio \"%s\" 0. 0. 44100 2 16 0 22050 \"%s\"",prefs->backend_sync,cfile->handle,afile);
      lives_system(com,TRUE);
      if (mainw->com_failed) {
        tmp=lives_strdup_printf(_("Command failed: %s"),com);
        fail_test(table,1,tmp);
        lives_free(tmp);
      }

      lives_free(com);

      while (mainw->cancelled==CANCEL_NONE&&(info_fd=open(cfile->info_file,O_RDONLY))==-1) {
        lives_usleep(prefs->sleep_time);
        lives_widget_context_update();
      }

      if (info_fd!=-1) {
        close(info_fd);

        lives_sync();

        fsize=sget_file_size(afile);
        lives_rm(afile);
        lives_free(afile);

        if (fsize==0) {
          fail_test(table,1,_("You should install sox_fmt_all or similar"));
        }

        else pass_test(table,1);

      }
    }
  }

  if (tshoot) lives_snprintf(prefs->image_ext,16,"%s",image_ext);

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

  // TODO: mpv

  add_test(table,2,_("Checking for \"mplayer\" presence"),TRUE);


  if (!capable->has_mplayer&&!capable->has_mplayer2) {
    success2=fail_test(table,2,_("You should install mplayer or mplayer2 to be able to use all the decoding features in LiVES"));

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
  } else {
    success2=pass_test(table,2);
  }


  // if present

  // check if mplayer can decode audio

  add_test(table,3,_("Checking if mplayer can convert audio"),success2);

  res=1;

  if (success2) {
    if (capable->has_mplayer) {
#ifndef IS_MINGW
      res=system("LANG=en LANGUAGE=en mplayer -ao help | grep pcm >/dev/null 2>&1");
#else
      res=system("mplayer -ao help | grep pcm >NUL 2>&1");
#endif
    } else if (capable->has_mplayer2) {
#ifndef IS_MINGW
      res=system("LANG=en LANGUAGE=en mplayer2 -ao help | grep pcm >/dev/null 2>&1");
#else
      res=system("mplayer2 -ao help | grep pcm >NUL 2>&1");
#endif
    } else {
#ifdef ALLOW_MPV
#ifndef IS_MINGW
      res=system("LANG=en LANGUAGE=en mpv --ao help | grep pcm >/dev/null 2>&1");
#else
      res=system("mpv --ao help | grep pcm >NUL 2>&1");
#endif
#endif
    }

    if (res==0) {
      pass_test(table,3);
    } else {
      fail_test(table,3,_("You should install mplayer or mplayer2 with pcm/wav support"));
    }
  }


  // check if mplayer can decode to png/alpha

  rname=get_resource("");

  if (!lives_file_test(rname,LIVES_FILE_TEST_IS_DIR)) {
    /// oops, no resources dir
    success4=FALSE;
  } else success4=TRUE;


  lives_free(rname);

  // TODO: mpv

  add_test(table,4,_("Checking if mplayer can decode to png/alpha"),success2&&success4);

  success3=FALSE;

  // try to open resource vidtest.avi

  if (success2&&success4) {

    info_fd=-1;

    lives_rm(cfile->info_file);

    rname=get_resource("vidtest.avi");

    com=lives_strdup_printf("%s open_test \"%s\" \"%s\" 0 png",prefs->backend_sync,cfile->handle,
                            (tmp=lives_filename_from_utf8(rname,-1,NULL,NULL,NULL)));
    lives_free(tmp);
    lives_free(rname);

    mainw->com_failed=FALSE;
    lives_system(com,TRUE);
    if (mainw->com_failed) {
      tmp=lives_strdup_printf(_("Command failed: %s"),com);
      fail_test(table,4,tmp);
      lives_free(tmp);
    }

    lives_free(com);

    while (mainw->cancelled==CANCEL_NONE&&(info_fd=open(cfile->info_file,O_RDONLY))==-1) {
      lives_usleep(prefs->sleep_time);
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

  res=1;

  if (success2) {
    if (capable->has_mplayer) {
#ifndef IS_MINGW
      res=system("LANG=en LANGUAGE=en mplayer -vo help | grep -i \"jpeg file\" >/dev/null 2>&1");
#else
      res=system("mplayer -vo help | grep -i \"jpeg file\" >NUL 2>&1");
#endif
    } else if (capable->has_mplayer2) {
#ifndef IS_MINGW
      res=system("LANG=en LANGUAGE=en mplayer2 -vo help | grep -i \"jpeg file\" >/dev/null 2>&1");
#else
      res=system("mplayer2 -vo help | grep -i \"jpeg file\" >NUL 2>&1");
#endif
    } else {
#ifdef ALLOW_MPV
#ifndef IS_MINGW
      res=system("LANG=en LANGUAGE=en mpv --vo help | grep -i \"image\" >/dev/null 2>&1");
#else
      res=system("mpv --vo help | grep -i \"image\" >NUL 2>&1");
#endif
#endif
    }

    if (res==0) {
      pass_test(table,5);
      if (!success3) {
        if (!strcmp(prefs->image_ext,LIVES_FILE_EXT_PNG)) imgext_switched=TRUE;
        set_pref("default_image_format","jpeg");
        lives_snprintf(prefs->image_ext,16,"%s",LIVES_FILE_EXT_JPG);
      }
    } else {
      if (!success3) fail_test(table,5,_("You should install mplayer with either png/alpha or jpeg support"));
      else fail_test(table,5,_("You may wish to add jpeg output support to mplayer"));
    }
  }

  // TODO - check each enabled decoder plugin in turn


  // check for convert

  add_test(table,8,_("Checking for \"convert\" presence"),TRUE);


  if (!capable->has_convert) {
    success=fail_test(table,8,_("Install imageMagick to be able to use all of the rendered effects"));
  } else {
    success=pass_test(table,8);
  }


  close_current_file(current_file);

  lives_widget_set_sensitive(okbutton,TRUE);
  if (tshoot) {
    lives_widget_hide(cancelbutton);
    if (imgext_switched) {
      label=lives_standard_label_new(
              _("\n\n    Image decoding type has been switched to jpeg. You can revert this in Preferences/Decoding.    \n"));
      lives_container_add(LIVES_CONTAINER(dialog_vbox), label);
    }
    lives_widget_show(label);
  } else {
    label=lives_standard_label_new(_("\n\n    Click Cancel to exit and install any missing components, or Next to continue    \n"));
    lives_container_add(LIVES_CONTAINER(dialog_vbox), label);
    lives_widget_show(label);
  }

  response=lives_dialog_run(LIVES_DIALOG(dialog));

  lives_widget_destroy(dialog);
  mainw->suppress_dprint=FALSE;

  if (mainw->multitrack!=NULL) {
    mt_sensitise(mainw->multitrack);
    mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
  }

  return (response==LIVES_RESPONSE_OK);
}






void do_startup_interface_query(void) {

  // prompt for startup ce or startup mt


  LiVESWidget *dialog,*dialog_vbox,*radiobutton0,*radiobutton1,*label;
  LiVESWidget *okbutton;
  LiVESWidget *hbox;
  LiVESSList *radiobutton_group = NULL;
  char *txt1,*txt2,*txt3,*msg;

  txt1=lives_strdup(_("\n\nFinally, you can choose the default startup interface for LiVES.\n"));
  txt2=lives_strdup(_("\n\nLiVES has two main interfaces and you can start up with either of them.\n"));
  txt3=lives_strdup(_("\n\nThe default can always be changed later from Preferences.\n"));


  msg=lives_strdup_printf("%s%s%s",txt1,txt2,txt3);


  lives_free(txt1);
  lives_free(txt2);
  lives_free(txt3);

  dialog = lives_standard_dialog_new(_("LiVES: - Choose the startup interface"),FALSE,-1,-1);

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(dialog));

  label=lives_standard_label_new(msg);
  lives_container_add(LIVES_CONTAINER(dialog_vbox), label);
  lives_free(msg);


  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, FALSE, FALSE, widget_opts.packing_height);
  radiobutton0 = lives_standard_radio_button_new(_("Start in _Clip Edit mode"),TRUE,radiobutton_group,LIVES_BOX(hbox),NULL);
  radiobutton_group = lives_radio_button_get_group(LIVES_RADIO_BUTTON(radiobutton0));

  label=lives_standard_label_new(_("This is the best choice for simple editing tasks and for VJs\n"));

  lives_box_pack_start(LIVES_BOX(dialog_vbox), label, FALSE, FALSE, widget_opts.packing_height);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, FALSE, FALSE, widget_opts.packing_height);
  radiobutton1 = lives_standard_radio_button_new(_("Start in _Multitrack mode"),TRUE,radiobutton_group,LIVES_BOX(hbox),NULL);

  label=lives_standard_label_new(_("This is a better choice for complex editing tasks involving multiple clips.\n"));

  lives_box_pack_start(LIVES_BOX(dialog_vbox), label, FALSE, FALSE, widget_opts.packing_height);

  if (prefs->startup_interface==STARTUP_MT) {
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(radiobutton1),TRUE);
  }

  okbutton = lives_button_new_from_stock(LIVES_STOCK_GO_FORWARD,_("_Finish"));

  lives_dialog_add_action_widget(LIVES_DIALOG(dialog), okbutton, LIVES_RESPONSE_OK);
  lives_widget_set_can_focus_and_default(okbutton);
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



void on_troubleshoot_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  do_startup_tests(TRUE);
}
