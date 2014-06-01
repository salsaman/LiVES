// callbacks.c
// LiVES
// (c) G. Finch 2003 - 2014 <salsaman@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#ifdef HAVE_SYSTEM_WEED
#include <weed/weed-palettes.h>
#else
#include "../libweed/weed-palettes.h"
#endif

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <dlfcn.h>

#include "support.h"
#include "main.h"
#include "callbacks.h"
#include "interface.h"
#include "effects.h"
#include "resample.h"
#include "rte_window.h"
#include "events.h"
#include "audio.h"
#include "cvirtual.h"
#include "paramwindow.h"
#include "ce_thumbs.h"

#ifdef HAVE_YUV4MPEG
#include "lives-yuv4mpeg.h"
#endif

#ifdef HAVE_UNICAP
#include "videodev.h"
#endif

static gchar file_name[PATH_MAX];

boolean on_LiVES_delete_event (GtkWidget *widget, GdkEvent *event, gpointer user_data) {
  on_quit_activate(NULL,NULL);
  return TRUE;
}


void lives_exit (void) {
  gchar *cwd;

  register int i;

  if (!mainw->only_close) mainw->is_exiting=TRUE;

  if (mainw->is_ready) {
    gchar *com;

    lives_close_all_file_buffers();

    if (mainw->multitrack!=NULL&&mainw->multitrack->idlefunc>0) {
      //g_source_remove(mainw->multitrack->idlefunc);
      mainw->multitrack->idlefunc=0;
    }

    threaded_dialog_spin();

    if (mainw->toy_type!=LIVES_TOY_NONE) {
      on_toy_activate(NULL, GINT_TO_POINTER(LIVES_TOY_NONE));
    }

    if (mainw->stored_event_list!=NULL||mainw->sl_undo_mem!=NULL) {
      stored_event_list_free_all(FALSE);
    }

    if (mainw->multitrack!=NULL&&!mainw->only_close) {
       if (mainw->multitrack->undo_mem!=NULL) g_free(mainw->multitrack->undo_mem);
       mainw->multitrack->undo_mem=NULL;
    }

    if (mainw->playing_file>-1) {
      mainw->ext_keyboard=FALSE;
      if (mainw->ext_playback) {
	if (mainw->vpp->exit_screen!=NULL) (*mainw->vpp->exit_screen)(mainw->ptr_x,mainw->ptr_y);
	stop_audio_stream();
	mainw->stream_ticks=-1;
      }
      
      // tell non-realtime audio players (sox or mplayer) to stop
      if (prefs->audio_player!=AUD_PLAYER_JACK&&prefs->audio_player!=AUD_PLAYER_PULSE&&mainw->aud_file_to_kill>-1&&
	  mainw->files[mainw->aud_file_to_kill]!=NULL) {
	gchar *lsname=g_build_filename(prefs->tmpdir,mainw->files[mainw->aud_file_to_kill]->handle,NULL);
#ifndef IS_MINGW
	com=g_strdup_printf ("/bin/touch \"%s\" 2>/dev/null",lsname);
#else
	com=g_strdup_printf ("touch.exe \"%s\" 2>NUL",lsname);
#endif
	g_free(lsname);
	lives_system(com,TRUE);
	g_free (com);
	com=g_strdup_printf("%s stop_audio \"%s\"",prefs->backend,mainw->files[mainw->aud_file_to_kill]->handle);
	lives_system(com,TRUE);
	g_free(com);
      }
    }

    // stop any background processing for the current clip
    if (mainw->current_file>-1) {
      if (cfile->handle!=NULL&&(cfile->clip_type==CLIP_TYPE_DISK||cfile->clip_type==CLIP_TYPE_FILE)) {
#ifndef IS_MINGW
	com=g_strdup_printf("%s stopsubsub \"%s\" 2>/dev/null",prefs->backend_sync,cfile->handle);
	lives_system(com,TRUE);
#else
	// get pid from backend
	FILE *rfile;
	ssize_t rlen;
	char val[16];
	int pid;
	com=g_strdup_printf("%s get_pid_for_handle \"%s\"",prefs->backend_sync,cfile->handle);
	rfile=popen(com,"r");
	rlen=fread(val,1,16,rfile);
	pclose(rfile);
	memset(val+rlen,0,1);
	pid=atoi(val);
	
	lives_win32_kill_subprocesses(pid,TRUE);
#endif
	g_free(com);

      }
    }
    
    // prevent crash in "threaded" dialog
    mainw->current_file=-1;

    if (!mainw->only_close) {
#ifdef HAVE_PULSE_AUDIO
    pthread_mutex_lock(&mainw->abuf_mutex);
    if (mainw->pulsed!=NULL) pulse_close_client(mainw->pulsed);
    if (mainw->pulsed_read!=NULL) pulse_close_client(mainw->pulsed_read);
    pulse_shutdown();
    pthread_mutex_unlock(&mainw->abuf_mutex);
#endif
#ifdef ENABLE_JACK
      pthread_mutex_lock(&mainw->abuf_mutex);
      lives_jack_end();
      if (mainw->jackd!=NULL) {
	jack_close_device(mainw->jackd);
      }
      if (mainw->jackd_read!=NULL) {
	jack_close_device(mainw->jackd_read);
      }
      pthread_mutex_unlock(&mainw->abuf_mutex);
#endif
    }

    if (mainw->vpp!=NULL&&!mainw->only_close) {
      if (!mainw->leave_recovery) {
	if (mainw->write_vpp_file) {
	  gchar *vpp_file=g_build_filename(capable->home_dir,LIVES_CONFIG_DIR,"vpp_defaults",NULL);
	  save_vpp_defaults(mainw->vpp,vpp_file);
	}
      }
      close_vid_playback_plugin(mainw->vpp);
    }

    if (!mainw->leave_recovery) {
      unlink(mainw->recovery_file);
      // hide the main window
      threaded_dialog_spin();
      lives_widget_context_update();
      threaded_dialog_spin();
    }

    if (strcmp(future_prefs->tmpdir,prefs->tmpdir)) {
      // if we changed the tempdir, remove everything but sets from the old dir
      // create the new directory, and then move any sets over
      end_threaded_dialog();
      if (do_move_tmpdir_dialog()) {
	do_do_not_close_d();
	lives_widget_context_update();

	// TODO *** - check for namespace collisions between sets in old dir and sets in new dir


	// use backend to move the sets
	com=g_strdup_printf("%s weed \"%s\" &",prefs->backend_sync,future_prefs->tmpdir);
	lives_system (com,FALSE);
	g_free (com);
      }
      g_snprintf(prefs->tmpdir,PATH_MAX,"%s",future_prefs->tmpdir);
    }
    else if (!mainw->only_close) g_snprintf(future_prefs->tmpdir,256,"NULL");

    if (mainw->leave_files&&!mainw->fatal) {
      gchar *msg;
      msg=g_strdup_printf(_("Saving as set %s..."),mainw->set_name);
      d_print(msg);
      g_free(msg);
    }

    cwd=g_get_current_dir();

    for (i=1;i<=MAX_FILES;i++) {
      if (mainw->files[i]!=NULL) {
	mainw->current_file=i;
	threaded_dialog_spin();
	if (cfile->event_list_back!=NULL) event_list_free (cfile->event_list_back);
	if (cfile->event_list!=NULL) event_list_free (cfile->event_list);

	cfile->event_list=cfile->event_list_back=NULL;

	if (cfile->layout_map!=NULL) {
	  g_list_free_strings(cfile->layout_map);
	  g_list_free(cfile->layout_map);
	}

	if (cfile->laudio_drawable!=NULL) {
	  lives_painter_surface_destroy(cfile->laudio_drawable);
	}

	if (cfile->raudio_drawable!=NULL) {
	  lives_painter_surface_destroy(cfile->raudio_drawable);
	}


	if ((mainw->files[i]->clip_type==CLIP_TYPE_FILE||mainw->files[i]->clip_type==CLIP_TYPE_DISK)&&mainw->files[i]->ext_src!=NULL) {
	  // must do this before we move it
	  gchar *ppath=g_build_filename(prefs->tmpdir,cfile->handle,NULL);
	  lives_chdir(ppath,FALSE);
	  g_free(ppath);
	  threaded_dialog_spin();
	  close_decoder_plugin((lives_decoder_t *)mainw->files[i]->ext_src);
	  mainw->files[i]->ext_src=NULL;
	  threaded_dialog_spin();
	}
	
	if (mainw->files[i]->frame_index!=NULL) {
	  g_free(mainw->files[i]->frame_index);
	  mainw->files[i]->frame_index=NULL;
	}

	cfile->layout_map=NULL;

      }
    }

    lives_chdir(cwd,FALSE);
    g_free(cwd);

    for (i=0;i<=MAX_FILES;i++) {
      if (mainw->files[i]!=NULL) {
	if ((!mainw->leave_files&&!prefs->crash_recovery&&strlen(mainw->set_name)==0)||
	    (!mainw->only_close&&(i==0||(mainw->files[i]->clip_type!=CLIP_TYPE_DISK&&
					 mainw->files[i]->clip_type!=CLIP_TYPE_FILE)))||
	    (i==mainw->scrap_file&&!mainw->leave_recovery)||
	    (i==mainw->ascrap_file&&!mainw->leave_recovery)||
	    (mainw->multitrack!=NULL&&i==mainw->multitrack->render_file)) {
	  // close all open clips, except for ones we want to retain

#ifdef IS_MINGW
	  FILE *rfile;
	  ssize_t rlen;
	  char val[16];
	  int pid;
#endif

#ifdef HAVE_YUV4MPEG
	  if (mainw->files[i]->clip_type==CLIP_TYPE_YUV4MPEG) {
	    lives_yuv_stream_stop_read((lives_yuv4m_t *)mainw->files[i]->ext_src);
	    g_free (mainw->files[i]->ext_src);
	  }
#endif
#ifdef HAVE_UNICAP
	  if (mainw->files[i]->clip_type==CLIP_TYPE_VIDEODEV) {
	    lives_vdev_free((lives_vdev_t *)mainw->files[i]->ext_src);
	    g_free (mainw->files[i]->ext_src);
	  }
#endif
	  threaded_dialog_spin();
#ifdef IS_MINGW
	  // kill any active processes: for other OSes the backend does this
	  // get pid from backend
	  com=g_strdup_printf("%s get_pid_for_handle \"%s\"",prefs->backend_sync,cfile->handle);
	  rfile=popen(com,"r");
	  rlen=fread(val,1,16,rfile);
	  pclose(rfile);
	  memset(val+rlen,0,1);
	  pid=atoi(val);
	  
	  lives_win32_kill_subprocesses(pid,TRUE);
#endif
	  com=g_strdup_printf("%s close \"%s\"",prefs->backend,mainw->files[i]->handle);
	  lives_system(com,FALSE);
	  g_free(com);
	  threaded_dialog_spin();
	}
	else {
	  threaded_dialog_spin();
	  // or just clean them up
	  com=g_strdup_printf("%s clear_tmp_files \"%s\"",prefs->backend_sync,mainw->files[i]->handle);
	  lives_system(com,FALSE);
	  threaded_dialog_spin();
	  g_free(com);
	  if (mainw->files[i]->frame_index!=NULL) {
	    save_frame_index(i);
	  }
	  lives_freep((void**)&mainw->files[i]->op_dir);
	  if (!mainw->only_close) {
	    if ((mainw->files[i]->clip_type==CLIP_TYPE_FILE||mainw->files[i]->clip_type==CLIP_TYPE_DISK)&&mainw->files[i]->ext_src!=NULL) {
	      gchar *ppath=g_build_filename(prefs->tmpdir,mainw->files[i]->handle,NULL);
	      cwd=g_get_current_dir();
	      lives_chdir(ppath,FALSE);
	      g_free(ppath);
	      close_decoder_plugin((lives_decoder_t *)mainw->files[i]->ext_src);
	      mainw->files[i]->ext_src=NULL;
	      
	      lives_chdir(cwd,FALSE);
	      g_free(cwd);
	    }
	  }
	}
      }
    }

    if (!mainw->leave_files&&strlen(mainw->set_name)&&!mainw->leave_recovery) {
      gchar *set_layout_dir=g_build_filename(prefs->tmpdir,mainw->set_name,"layouts",NULL);
      if (!g_file_test(set_layout_dir,G_FILE_TEST_IS_DIR)) {
	gchar *sdname=g_build_filename(prefs->tmpdir,mainw->set_name,NULL);

	// note, we do not use the flag -f
#ifndef IS_MINGW
	com=g_strdup_printf("/bin/rm -r \"%s/\" 2>/dev/null",sdname);
#else
	com=g_strdup_printf("rm.exe -r \"%s/\" 2>NUL",sdname);
#endif
	g_free(sdname);
	lives_system(com,TRUE);
	threaded_dialog_spin();
	g_free(com);
      }
      else {
	gchar *dname=g_build_filename(prefs->tmpdir,mainw->set_name,"clips",NULL);
#ifndef IS_MINGW
	com=g_strdup_printf("/bin/rm -r \"%s\" 2>/dev/null",dname);
#else
	com=g_strdup_printf("rm.exe -r \"%s\" 2>NUL",dname);
#endif
	g_free(dname);
	lives_system(com,TRUE);
	threaded_dialog_spin();
	g_free(com);
	dname=g_build_filename(prefs->tmpdir,mainw->set_name,"order",NULL);
#ifndef IS_MINGW
	com=g_strdup_printf("/bin/rm \"%s\" 2>/dev/null",dname);
#else
	com=g_strdup_printf("rm.exe \"%s\" 2>NUL",dname);
#endif
	g_free(dname);
	lives_system(com,TRUE);
	threaded_dialog_spin();
	g_free(com);
      }
      g_free(set_layout_dir);
    }

    if (strlen(mainw->set_name)) {
      gchar *set_lock_file=g_strdup_printf("%s/%s/lock.%d",prefs->tmpdir,mainw->set_name,capable->mainpid);
      unlink(set_lock_file);
      g_free(set_lock_file);
      threaded_dialog_spin();
    }
    
    if (mainw->only_close) {
      mainw->suppress_dprint=TRUE;
      for (i=1;i<=MAX_FILES;i++) {
	if (mainw->files[i]!=NULL&&(mainw->files[i]->clip_type==CLIP_TYPE_DISK||
				    mainw->files[i]->clip_type==CLIP_TYPE_FILE)&&(mainw->multitrack==NULL||
										  i!=mainw->multitrack->render_file)) {
	  mainw->current_file=i;
	  close_current_file(0);
	  threaded_dialog_spin();
	}
      }
      mainw->suppress_dprint=FALSE;
      if (mainw->multitrack==NULL) resize(1);
      mainw->was_set=FALSE;
      mainw->leave_files=FALSE;
      memset(mainw->set_name,0,1);
      mainw->only_close=FALSE;
      prefs->crash_recovery=TRUE;

      threaded_dialog_spin();
      if (mainw->current_file>-1) sensitize();
      lives_widget_queue_draw(mainw->LiVES);
      d_print_done();
      end_threaded_dialog();

      if (mainw->multitrack!=NULL) {
	mainw->current_file=mainw->multitrack->render_file;
	mainw->multitrack->file_selected=-1;
	polymorph(mainw->multitrack,POLY_NONE);
	polymorph(mainw->multitrack,POLY_CLIPS);
	mt_sensitise(mainw->multitrack);
      }

      return;
    }

    save_future_prefs();

    // stop valgrind from complaining
#ifdef VALG_COMPLAIN
    if (mainw->preview_image!=NULL && LIVES_IS_IMAGE(mainw->preview_image)) 
      lives_image_set_from_pixbuf(LIVES_IMAGE(mainw->preview_image), NULL);

    if (mainw->start_image!=NULL) lives_image_set_from_pixbuf(LIVES_IMAGE(mainw->start_image), NULL);
    if (mainw->end_image!=NULL) lives_image_set_from_pixbuf(LIVES_IMAGE(mainw->end_image), NULL);
#endif

    if (mainw->frame_layer!=NULL) {
      check_layer_ready(mainw->frame_layer);
      weed_layer_free(mainw->frame_layer);
    }

    if (mainw->sep_win&&(mainw->playing_file>-1||prefs->sepwin_type==SEPWIN_TYPE_STICKY)) {
      threaded_dialog_spin();
      kill_play_window();
      threaded_dialog_spin();
    }
  }

  if (mainw->current_layouts_map!=NULL) {
    g_list_free_strings(mainw->current_layouts_map);
    g_list_free(mainw->current_layouts_map);
    mainw->current_layouts_map=NULL;
  }

  if (capable->smog_version_correct&&!mainw->startup_error) {
    if (capable->has_encoder_plugins) plugin_request("encoders",prefs->encoder.name,"finalise");

    weed_unload_all();

    threaded_dialog_spin();

    rfx_free_all();
    threaded_dialog_spin();

#ifdef ENABLE_OSC
    if (prefs->osc_udp_started) lives_osc_end();

#ifdef IS_MINGW
    WSACleanup();
#endif

#endif
  }

  pconx_delete_all();
  cconx_delete_all();

  if (mainw->multitrack!=NULL) {
    event_list_free_undos(mainw->multitrack);

    if (mainw->multitrack->event_list!=NULL) {
      event_list_free(mainw->multitrack->event_list);
      mainw->multitrack->event_list=NULL;
    }
  }

  if (prefs->fxdefsfile!=NULL) g_free(prefs->fxdefsfile);
  if (prefs->fxsizesfile!=NULL) g_free(prefs->fxsizesfile);

  if (prefs->wm!=NULL) g_free(prefs->wm);

  if (mainw->recovery_file!=NULL) g_free(mainw->recovery_file);

  for (i=0;i<NUM_LIVES_STRING_CONSTANTS;i++) if (mainw->string_constants[i]!=NULL) g_free(mainw->string_constants[i]);

  for (i=0;i<mainw->n_screen_areas;i++) g_free(mainw->screen_areas[i].name);

  if (mainw->video_drawable!=NULL) {
    lives_painter_surface_destroy(mainw->video_drawable);
  }

  if (mainw->laudio_drawable!=NULL) {
    lives_painter_surface_destroy(mainw->laudio_drawable);
  }

  if (mainw->raudio_drawable!=NULL) {
    lives_painter_surface_destroy(mainw->raudio_drawable);
  }

  if (mainw->foreign_visual!=NULL) g_free(mainw->foreign_visual);
  if (mainw->read_failed_file!=NULL) g_free(mainw->read_failed_file);
  if (mainw->write_failed_file!=NULL) g_free(mainw->write_failed_file);
  if (mainw->bad_aud_file!=NULL) g_free(mainw->bad_aud_file);

  unload_decoder_plugins();

  if (mainw->multitrack!=NULL) g_free(mainw->multitrack);
  mainw->multitrack=NULL;
  mainw->is_ready=FALSE;

  if (mainw->mgeom!=NULL) g_free(mainw->mgeom);

  if (prefs->disabled_decoders!=NULL) {
    g_list_free_strings(prefs->disabled_decoders);
    g_list_free(prefs->disabled_decoders);
  }

  if (mainw->fonts_array!=NULL) g_strfreev(mainw->fonts_array);
#ifdef USE_SWSCALE
  sws_free_context();
#endif

#ifdef ENABLE_NLS
  if (trString!=NULL) g_free(trString);
#endif

  exit(0);
}




void on_filesel_button_clicked (GtkButton *button, gpointer user_data) {
  GtkWidget *tentry=LIVES_WIDGET(user_data);
  gchar *dirname;
  gchar *fname;
  gchar *tmp;

  gchar *def_dir=(gchar *)g_object_get_data(G_OBJECT(button),"def_dir");

  boolean is_dir=LIVES_POINTER_TO_INT(g_object_get_data(G_OBJECT(button),"is_dir"));

  if (LIVES_IS_TEXT_VIEW(tentry)) fname=text_view_get_text(LIVES_TEXT_VIEW(tentry));
  else fname=g_strdup(lives_entry_get_text(LIVES_ENTRY(tentry)));

  if (!strlen(fname)) {
    g_free(fname);
    fname=def_dir;
  }

  lives_widget_context_update();

  dirname=choose_file(fname,NULL,NULL,
		      is_dir?LIVES_FILE_CHOOSER_ACTION_SELECT_FOLDER:
		      (fname==def_dir&&!strcmp(def_dir,LIVES_DEVICE_DIR))?LIVES_FILE_CHOOSER_ACTION_SELECT_DEVICE:
		      LIVES_FILE_CHOOSER_ACTION_OPEN,
		      NULL,NULL);

  if (fname!=NULL&&fname!=def_dir) g_free(fname);

  if (dirname==NULL) return;

  g_snprintf(file_name,PATH_MAX,"%s",dirname);
  g_free(dirname);

  if (button!=NULL) {
    if (LIVES_IS_ENTRY(tentry)) lives_entry_set_text(LIVES_ENTRY(tentry),(tmp=g_filename_to_utf8(file_name,-1,NULL,NULL,NULL)));
    else text_view_set_text (LIVES_TEXT_VIEW(tentry), (tmp=g_filename_to_utf8(file_name,-1,NULL,NULL,NULL)), -1);
    g_free(tmp);
  }

  // force update to be recognized
  if (g_object_get_data(G_OBJECT(tentry),"rfx")!=NULL) 
    after_param_text_changed(tentry,(lives_rfx_t *)g_object_get_data(G_OBJECT(tentry),"rfx"));
}



void on_filesel_complex_clicked (GtkButton *button, GtkEntry *entry) {
  // append /livestmp
  size_t chklen=strlen(LIVES_TMP_NAME);

  on_filesel_button_clicked (NULL,entry);

  // TODO - dirsep
#ifndef IS_MINGW
  if (strcmp(file_name+strlen(file_name)-1,"/")) {
    g_strappend(file_name,PATH_MAX,"/");
  }
  if (strlen (file_name)<chklen+2||strncmp (file_name+strlen (file_name)-chklen-2,"/"LIVES_TMP_NAME"/",chklen-2)) 
    g_strappend (file_name,PATH_MAX,LIVES_TMP_NAME"/");
#else
  if (strcmp(file_name+strlen(file_name)-1,"\\")) {
    g_strappend(file_name,PATH_MAX,"\\");
  }
  if (strlen (file_name)<chklen-2||strncmp (file_name+strlen (file_name)-chklen-2,"\\"LIVES_TMP_NAME"\\",chklen-2)) 
    g_strappend (file_name,PATH_MAX,LIVES_TMP_NAME"\\");
#endif

  lives_entry_set_text(entry,file_name);

}



void on_open_sel_activate (GtkMenuItem *menuitem, gpointer user_data) {
  // OPEN A FILE

  if (mainw->multitrack!=NULL) {
    if (mainw->multitrack->idlefunc>0) {
      g_source_remove(mainw->multitrack->idlefunc);
      mainw->multitrack->idlefunc=0;
    }
    mt_desensitise(mainw->multitrack);
    lives_widget_set_sensitive(mainw->multitrack->playall,TRUE);
    lives_widget_set_sensitive (mainw->m_playbutton, TRUE);
  }

  choose_file_with_preview(strlen(mainw->vid_load_dir)?mainw->vid_load_dir:NULL,NULL,1);
}



void on_ok_filesel_open_clicked (GtkFileChooser *chooser, gpointer user_data) {
  gchar *fname=lives_file_chooser_get_filename (chooser);
  gchar *tmp;

  if (fname==NULL) return;

  g_snprintf(file_name,PATH_MAX,"%s",(tmp=g_filename_to_utf8(fname,-1,NULL,NULL,NULL)));
  g_free(tmp); g_free(fname);

  end_fs_preview();
  lives_widget_destroy(LIVES_WIDGET(chooser));

  g_snprintf(mainw->vid_load_dir,PATH_MAX,"%s",file_name);
  get_dirname(mainw->vid_load_dir);

  if (mainw->multitrack==NULL) lives_widget_queue_draw(mainw->LiVES);
  else lives_widget_queue_draw(mainw->multitrack->window);
  lives_widget_context_update();
  
  mainw->fs_playarea=NULL;
  
  if (prefs->save_directories) {
    set_pref ("vid_load_dir",(tmp=g_filename_from_utf8(mainw->vid_load_dir,-1,NULL,NULL,NULL)));
    g_free(tmp);
  }
  
  mainw->cancelled=CANCEL_NONE;

  open_sel_range_activate();
}



void on_open_vcd_activate (GtkMenuItem *menuitem, gpointer user_data) {
  GtkWidget *vcdtrack_dialog;
  
  if (mainw->multitrack!=NULL) {
    if (mainw->multitrack->idlefunc>0) {
      g_source_remove(mainw->multitrack->idlefunc);
      mainw->multitrack->idlefunc=0;
    }
    mt_desensitise(mainw->multitrack);
    lives_widget_set_sensitive(mainw->multitrack->playall,TRUE);
    lives_widget_set_sensitive (mainw->m_playbutton, TRUE);
  }

  mainw->fx1_val=1;
  mainw->fx2_val=1;
  mainw->fx3_val=128;
  vcdtrack_dialog = create_cdtrack_dialog (GPOINTER_TO_INT (user_data),NULL);
  lives_widget_show (vcdtrack_dialog);
}


void on_open_loc_activate (GtkMenuItem *menuitem, gpointer user_data) {
  if (mainw->multitrack!=NULL) {
    if (mainw->multitrack->idlefunc>0) {
      g_source_remove(mainw->multitrack->idlefunc);
      mainw->multitrack->idlefunc=0;
    }
    mt_desensitise(mainw->multitrack);
    lives_widget_set_sensitive(mainw->multitrack->playall,TRUE);
    lives_widget_set_sensitive (mainw->m_playbutton, TRUE);
  }

  locw=create_location_dialog(1);
  lives_widget_show(locw->dialog);

}


void on_open_utube_activate (GtkMenuItem *menuitem, gpointer user_data) {
  if (mainw->multitrack!=NULL) {
    if (mainw->multitrack->idlefunc>0) {
      g_source_remove(mainw->multitrack->idlefunc);
      mainw->multitrack->idlefunc=0;
    }
    mt_desensitise(mainw->multitrack);
    lives_widget_set_sensitive(mainw->multitrack->playall,TRUE);
    lives_widget_set_sensitive (mainw->m_playbutton, TRUE);
  }

  locw=create_location_dialog(2);
  lives_widget_show(locw->dialog);
}




void on_autoreload_toggled (GtkToggleButton *togglebutton, gpointer user_data) {
  // type==1, autoreload clipset
  // type==2, autoreload layout

  int type=LIVES_POINTER_TO_INT(user_data);
  if (type==0) {
    _entryw *cdsw=(_entryw *)g_object_get_data(G_OBJECT(togglebutton),"cdsw");
    prefs->ar_layout=!prefs->ar_layout;
    if (cdsw->warn_checkbutton!=NULL) {
      if (prefs->ar_layout) {
	lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(cdsw->warn_checkbutton),FALSE);
	lives_widget_set_sensitive(cdsw->warn_checkbutton,FALSE);
      }
      else {
	lives_widget_set_sensitive(cdsw->warn_checkbutton,TRUE);
      }
    }
  }
  if (type==1) prefs->ar_clipset=!prefs->ar_clipset;
  if (type==2) prefs->ar_layout=!prefs->ar_layout;
}


void on_recent_activate (GtkMenuItem *menuitem, gpointer user_data) {
  char file[PATH_MAX];
  double start=0.;
  int end=0,pno;
  char *pref;

  pno=LIVES_POINTER_TO_INT(user_data);

  if (mainw->multitrack!=NULL) {
    if (mainw->multitrack->idlefunc>0) {
      g_source_remove(mainw->multitrack->idlefunc);
      mainw->multitrack->idlefunc=0;
    }
    mt_desensitise(mainw->multitrack);
  }


  lives_widget_context_update(); // hide menu popdown

  pref=g_strdup_printf("recent%d",pno);

  get_pref(pref,file,PATH_MAX);

  g_free(pref);

  if (get_token_count(file,'\n')>1) {
    gchar **array=g_strsplit(file,"\n",2);
    g_snprintf(file,PATH_MAX,"%s",array[0]);
    if (mainw->file_open_params!=NULL) g_free (mainw->file_open_params);
    mainw->file_open_params=g_strdup(array[1]);
    g_strfreev (array);
  }

  if (get_token_count (file,'|')>2) {
    gchar **array=g_strsplit(file,"|",3);
    g_snprintf (file,PATH_MAX,"%s",array[0]);
    start=g_strtod (array[1],NULL);
    end=atoi (array[2]);
    g_strfreev (array);
  }
  deduce_file(file,start,end);

  if (mainw->multitrack!=NULL) {
    polymorph(mainw->multitrack,POLY_NONE);
    polymorph(mainw->multitrack,POLY_CLIPS);
    mt_sensitise(mainw->multitrack);
    mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
  }

}



void on_location_select (GtkButton *button, gpointer user_data) {
  g_snprintf(file_name,PATH_MAX,"%s",lives_entry_get_text(LIVES_ENTRY(locw->entry)));
  lives_widget_destroy(locw->dialog);
  lives_widget_context_update();
  g_free(locw);

  mainw->opening_loc=TRUE;
  if (mainw->file_open_params!=NULL) g_free (mainw->file_open_params);
  if (prefs->no_bandwidth) {
    mainw->file_open_params=g_strdup ("nobandwidth");
  }
  else mainw->file_open_params=g_strdup ("sendbandwidth");
  mainw->img_concat_clip=-1;
  open_file(file_name);

  if (mainw->multitrack!=NULL) {
    polymorph(mainw->multitrack,POLY_NONE);
    polymorph(mainw->multitrack,POLY_CLIPS);
    mt_sensitise(mainw->multitrack);
    mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
  }

}




void on_utube_select (GtkButton *button, gpointer user_data) {
  gchar *fname=ensure_extension(lives_entry_get_text(LIVES_ENTRY(locw->name_entry)),".webm");
  gchar *url;
  gchar *dirname;
  gchar *dfile;
  gchar *com;
  gchar *msg;
  int current_file=mainw->current_file;

  if (!strlen(fname)) {
    do_blocking_error_dialog(_("Please enter the name of the file to save the clip as.\n"));
    g_free(fname);
    return;
  }

  url=g_strdup(lives_entry_get_text(LIVES_ENTRY(locw->entry)));

  if (!strlen(url)) {
    do_blocking_error_dialog(_("Please enter a valid URL for the download.\n"));
    g_free(fname);
    g_free(url);
    return;
  }

  dirname=g_strdup(lives_entry_get_text(LIVES_ENTRY(locw->dir_entry)));
  ensure_isdir(dirname);
  g_snprintf(mainw->vid_dl_dir,PATH_MAX,"%s",dirname);

  lives_widget_destroy(locw->dialog);
  lives_widget_context_update();
  g_free(locw);

  dfile=g_build_filename(dirname,fname,NULL);

  if (!check_file(dfile,TRUE)) {
    g_free(dirname);
    g_free(fname);
    g_free(url);
    g_free(dfile);
    return;
  }

  mainw->error=FALSE;

  msg=g_strdup_printf(_("Downloading %s to %s..."),url,dfile);
  d_print(msg);
  g_free(msg);

  if (current_file==-1) {
    if (!get_temp_handle(mainw->first_free_file,TRUE)) {
      d_print_failed();
      return;
    }
  }

  mainw->no_switch_dprint=TRUE;

  unlink(cfile->info_file);

  com=g_strdup_printf("%s download_clip \"%s\" \"%s\" \"%s\"",prefs->backend,cfile->handle,url,dfile);
  mainw->com_failed=FALSE;
  lives_system(com,FALSE);
  g_free(com);

  if (mainw->com_failed) {
    d_print_failed();
    return;
  }
  
  cfile->nopreview=TRUE;
  cfile->keep_without_preview=TRUE;
  cfile->no_proc_sys_errors=TRUE;  ///< do not show processing error dialogs, we will show our own msg
  if (!do_progress_dialog(TRUE,TRUE,_("Downloading clip"))||mainw->error) {
    // user cancelled or error
    cfile->nopreview=FALSE;
    cfile->no_proc_sys_errors=FALSE;
    cfile->keep_without_preview=FALSE;
    
    if (mainw->cancelled==CANCEL_KEEP&&!mainw->error) {
      mainw->cancelled=CANCEL_NONE;
    }
    else {
      if (current_file==-1) {
#ifdef IS_MINGW
	// kill any active processes: for other OSes the backend does this
      // get pid from backend
      FILE *rfile;
      ssize_t rlen;
      char val[16];
      int pid;
      com=g_strdup_printf("%s get_pid_for_handle \"%s\"",prefs->backend_sync,cfile->handle);
      rfile=popen(com,"r");
      rlen=fread(val,1,16,rfile);
      pclose(rfile);
      memset(val+rlen,0,1);
      pid=atoi(val);
      
      lives_win32_kill_subprocesses(pid,TRUE);
#endif
      // we made a temp file so close it
      com=g_strdup_printf("%s close \"%s\"",prefs->backend,cfile->handle);
      lives_system(com,TRUE);
      g_free(com);
      g_free(cfile);
      cfile=NULL;
      mainw->current_file=-1;
    }
      
    if (mainw->error) {
      d_print_failed();
      do_blocking_error_dialog(_("\nLiVES was unable to download the clip.\nPlease check the clip URL and make sure you have \nthe latest youtube-dl installed.\n"));
      mainw->error=FALSE;
    }
    
    unlink(dfile);

    g_free(dirname);
    g_free(fname);
    g_free(url);
    g_free(dfile);

    sensitize();
    mainw->no_switch_dprint=FALSE;
    return;
    }
  }
  
  cfile->nopreview=FALSE;
  cfile->no_proc_sys_errors=FALSE;
  cfile->keep_without_preview=FALSE;

  if (current_file==-1) {
    com=g_strdup_printf("%s close \"%s\"",prefs->backend,cfile->handle);
    lives_system(com,TRUE);
    g_free(com);
    g_free(cfile);
    cfile=NULL;
    mainw->current_file=-1;
  }

  g_free(dirname);
  g_free(fname);
  g_free(url);

  mainw->no_switch_dprint=FALSE;
  open_file(dfile);
  g_free(dfile);

  if (mainw->multitrack!=NULL) {
    polymorph(mainw->multitrack,POLY_NONE);
    polymorph(mainw->multitrack,POLY_CLIPS);
    mt_sensitise(mainw->multitrack);
    mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
  }

}



void count_opening_frames(void) {
  int cframes=cfile->frames;
  get_frame_count(mainw->current_file);
  mainw->opening_frames=cfile->frames;
  cfile->frames=cframes;
}



void on_stop_clicked (GtkMenuItem *menuitem, gpointer user_data) {
// 'enough' button for open, open location, and record audio
  gchar *com;

#ifdef IS_MINGW
  FILE *rfile;
  ssize_t rlen;
  char val[16];
  int pid;
#endif

#ifdef ENABLE_JACK
  if (mainw->jackd!=NULL&&mainw->jackd_read!=NULL) {
    mainw->cancelled=CANCEL_KEEP;
    return;
  }
#endif
#ifdef HAVE_PULSE_AUDIO
  if (mainw->pulsed!=NULL&&mainw->pulsed_read!=NULL) {
    mainw->cancelled=CANCEL_KEEP;
    return;
  }
#endif

#ifndef IS_MINGW
  com=g_strdup_printf("%s stopsubsubs \"%s\" 2>/dev/null",prefs->backend_sync,cfile->handle);
  lives_system(com,TRUE);
#else
  // get pid from backend
  com=g_strdup_printf("%s get_pid_for_handle \"%s\"",prefs->backend_sync,cfile->handle);
  rfile=popen(com,"r");
  rlen=fread(val,1,16,rfile);
  pclose(rfile);
  memset(val+rlen,0,1);
  pid=atoi(val);
  
  lives_win32_kill_subprocesses(pid,FALSE);
#endif
  g_free(com);

  if (mainw->current_file>-1&&cfile!=NULL&&cfile->proc_ptr!=NULL) {
    lives_widget_set_sensitive(cfile->proc_ptr->stop_button, FALSE);
    lives_widget_set_sensitive(cfile->proc_ptr->pause_button,FALSE);
    lives_widget_set_sensitive(cfile->proc_ptr->preview_button, FALSE);
    lives_widget_set_sensitive(cfile->proc_ptr->cancel_button, FALSE);
  }


  // resume to allow return
  if (mainw->effects_paused) {
#ifndef IS_MINGW
    com=g_strdup_printf("%s stopsubsub \"%s\" SIGCONT 2>/dev/null",prefs->backend_sync,cfile->handle);
    lives_system(com,TRUE);
#else
    FILE *rfile;
    ssize_t rlen;
    char val[16];
    
    // get pid from backend
    com=g_strdup_printf("%s get_pid_for_handle \"%s\"",prefs->backend_sync,cfile->handle);
    rfile=popen(com,"r");
    rlen=fread(val,1,16,rfile);
    pclose(rfile);
    memset(val+rlen,0,1);
    pid=atoi(val);
    
    lives_win32_suspend_resume_process(pid,FALSE);
#endif
    g_free(com);
    
    com=g_strdup_printf("%s resume \"%s\"",prefs->backend_sync,cfile->handle);
    lives_system(com,FALSE);
    g_free(com);
  }


}




void on_save_as_activate (GtkMenuItem *menuitem, gpointer user_data) {

  if (cfile->frames==0) {
    on_export_audio_activate (NULL,NULL);
    return;
  }

  save_file(mainw->current_file,1,cfile->frames,NULL);
}


void
on_save_selection_activate (GtkMenuItem *menuitem, gpointer user_data) {
  save_file(mainw->current_file,cfile->start,cfile->end,NULL);
}


void on_close_activate (GtkMenuItem *menuitem, gpointer user_data) {
  gchar *warn,*extra;
  gchar title[256];
  boolean lmap_errors=FALSE,acurrent=FALSE,only_current=FALSE;

  if (mainw->multitrack!=NULL) {
    if (mainw->multitrack->idlefunc>0) {
      g_source_remove(mainw->multitrack->idlefunc);
      mainw->multitrack->idlefunc=0;
    }
    mt_desensitise(mainw->multitrack);
    mainw->current_file=mainw->multitrack->file_selected;
  }

  if (!(prefs->warning_mask&WARN_MASK_LAYOUT_CLOSE_FILE)) {
    mainw->xlays=layout_frame_is_affected(mainw->current_file,1);
    mainw->xlays=layout_audio_is_affected(mainw->current_file,0.);

    acurrent=used_in_current_layout(mainw->multitrack,mainw->current_file);
    if (acurrent) {
      if (mainw->xlays==NULL) only_current=TRUE;
      mainw->xlays=g_list_append_unique(mainw->xlays,mainw->string_constants[LIVES_STRING_CONSTANT_CL]);
    }

    if (mainw->xlays!=NULL) {
      get_menu_text(cfile->menuentry,title);
      if (strlen(title)>128) g_snprintf(title,32,"%s",(_("This file")));
      if (acurrent) extra=g_strdup(_(",\n - including the current layout - "));
      else extra=g_strdup("");
      if (!only_current) warn=g_strdup_printf(_ ("\n%s\nis used in some multitrack layouts%s.\n\nReally close it ?"),
					      title,extra);
      else warn=g_strdup_printf(_ ("\n%s\nis used in the current layout.\n\nReally close it ?"),title);
      g_free(extra);
      if (!do_warning_dialog(warn)) {
	g_free(warn);
	if (mainw->xlays!=NULL) {
	  g_list_free_strings(mainw->xlays);
	  g_list_free(mainw->xlays);
	  mainw->xlays=NULL;
	}

	if (mainw->multitrack!=NULL) {
	  mainw->current_file=mainw->multitrack->render_file;
	  mt_sensitise(mainw->multitrack);
	  mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
	}
	return;
      }
      g_free(warn);
      add_lmap_error(LMAP_ERROR_CLOSE_FILE,cfile->name,cfile->layout_map,0,1,0.,acurrent);
      lmap_errors=TRUE;
      if (mainw->xlays!=NULL) {
	g_list_free_strings(mainw->xlays);
	g_list_free(mainw->xlays);
	mainw->xlays=NULL;
      }
    }
  }
  if (!lmap_errors) {
    if (cfile->changed) {
      get_menu_text(cfile->menuentry,title);
      if (strlen(title)>128) g_snprintf(title,32,"%s",(_("This file")));
      warn=g_strdup(_ ("Changes made to this clip have not been saved or backed up.\n\nReally close it ?"));
      if (!do_warning_dialog(warn)) {
	g_free(warn);

	if (mainw->multitrack!=NULL) {
	  mainw->current_file=mainw->multitrack->render_file;
	  mt_sensitise(mainw->multitrack);
	  mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
	}

	return;
      }
      g_free(warn);
    }
  }

  if (mainw->sl_undo_mem!=NULL&&(cfile->stored_layout_frame!=0||cfile->stored_layout_audio!=0.)) {
    // need to invalidate undo/redo stack, in case file was used in some layout undo
    stored_event_list_free_undos();
  }

  if (mainw->multitrack!=NULL) {
    event_list_free_undos(mainw->multitrack);
  }

  close_current_file(0);

  if (mainw->multitrack!=NULL) {
    mainw->current_file=mainw->multitrack->render_file;
    polymorph(mainw->multitrack,POLY_NONE);
    polymorph(mainw->multitrack,POLY_CLIPS);
    if (mainw->multitrack->event_list!=NULL) only_current=FALSE;
  }

  if (lmap_errors&&!only_current&&mainw->cliplist!=NULL) popup_lmap_errors(NULL,NULL);

  if (mainw->cliplist==NULL&&strlen(mainw->set_name)>0) {
    gchar *lfiles;
    gchar *ofile;
    gchar *sdir;
    gchar *cdir;
    gchar *laydir;
    gchar *com;

    boolean has_layout_map=FALSE;
    int i;


    // TODO - combine this with lives_exit and make into a function

    // check for layout maps
    if (mainw->current_layouts_map!=NULL) {
      has_layout_map=TRUE;
    }

    if (has_layout_map) {
      if (prompt_remove_layout_files()) {
	// delete layout files
	for (i=1;i<MAX_FILES;i++) {
	  if (!(mainw->files[i]==NULL)) {
	    if (mainw->was_set&&mainw->files[i]->layout_map!=NULL) {
	      remove_layout_files(mainw->files[i]->layout_map);
	    }
	  }
	}

	// delete layout directory
	laydir=g_build_filename(prefs->tmpdir,mainw->set_name,"layouts",NULL);
#ifndef IS_MINGW
	com=g_strdup_printf("/bin/rm -r \"%s/\" 2>/dev/null &",laydir);
#else
	com=g_strdup_printf("START /MIN /b rm.exe -r \"%s/\" 2>NUL",laydir);
#endif
	lives_system(com,TRUE);
	g_free(com);
	g_free(laydir);
      }
      recover_layout_cancelled(FALSE);
    }

    cdir=g_build_filename(prefs->tmpdir,mainw->set_name,"clips",NULL);


#ifndef IS_MINGW
    com=g_strdup_printf("/bin/rmdir \"%s\" 2>/dev/null",cdir);
#else
    com=g_strdup_printf("rmdir.exe \"%s\"",cdir);
#endif
    do {
      // keep trying until backend has deleted the clip
      mainw->com_failed=FALSE;
      lives_system(com,TRUE);
      if (g_file_test(cdir,G_FILE_TEST_IS_DIR)) {
	lives_widget_context_update();
	g_usleep(prefs->sleep_time);
      }
    } while (g_file_test(cdir,G_FILE_TEST_IS_DIR));

    mainw->com_failed=FALSE;

    g_free(com);
    g_free(cdir);

    lfiles=g_build_filename(prefs->tmpdir,mainw->set_name,"lock",NULL);
#ifndef IS_MINGW
    com=g_strdup_printf("/bin/rm \"%s\"* 2>/dev/null",lfiles);
#else
    com=g_strdup_printf("DEL \"%s*\" 2>NUL",lfiles);
#endif
    lives_system(com,TRUE);
    g_free(com);
    g_free(lfiles);

    ofile=g_build_filename(prefs->tmpdir,mainw->set_name,"order",NULL);
#ifndef IS_MINGW
    com=g_strdup_printf("/bin/rm \"%s\" 2>/dev/null",ofile);
#else
    com=g_strdup_printf("rm.exe \"%s\" 2>NUL",ofile);
#endif
    lives_system(com,TRUE);
    g_free(com);
    g_free(ofile);


    lives_sync();


    sdir=g_build_filename(prefs->tmpdir,mainw->set_name,NULL);
#ifndef IS_MINGW
    com=g_strdup_printf("/bin/rmdir \"%s/\" 2>/dev/null",sdir);
#else
    com=g_strdup_printf("rmdir.exe \"%s/\" 2>NUL",sdir);
#endif
    lives_system(com,TRUE);
    g_free(com);
    g_free(sdir);

    if (prefs->ar_clipset&&!strcmp(prefs->ar_clipset_name,mainw->set_name)) {
      prefs->ar_clipset=FALSE;
      memset(prefs->ar_clipset_name,0,1);
      set_pref("ar_clipset","");
    }
    memset(mainw->set_name,0,1);
    mainw->was_set=FALSE;
  }

  if (mainw->multitrack!=NULL) {
    mt_sensitise(mainw->multitrack);
    mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
  }

}


void
on_import_proj_activate                      (GtkMenuItem     *menuitem,
					      gpointer         user_data)
{
  gchar *com;
  gchar *filt[]={"*.lv2",NULL};
  gchar *proj_file=choose_file(NULL,NULL,filt,LIVES_FILE_CHOOSER_ACTION_OPEN,NULL,NULL);
  gchar *info_file;
  gchar *new_set;
  int info_fd;
  ssize_t bytes;
  gchar *set_dir;
  gchar *msg;

  if (proj_file==NULL) return;
  info_file=g_strdup_printf("%s/.impname.%d",prefs->tmpdir,capable->mainpid);
  unlink(info_file);
  mainw->com_failed=FALSE;
  com=g_strdup_printf("%s get_proj_set \"%s\">\"%s\"",prefs->backend_sync,proj_file,info_file);
  lives_system(com,FALSE);
  g_free(com);

  if (mainw->com_failed) {
    g_free(info_file);
    g_free(proj_file);
    return;
  }

  info_fd=open(info_file,O_RDONLY);

  if (info_fd>=0&&((bytes=read(info_fd,mainw->msg,256))>0)) {
    close(info_fd);
    memset(mainw->msg+bytes,0,1);
  }
  else {
    if (info_fd>=0) close(info_fd);
    unlink(info_file);
    g_free(info_file);
    g_free(proj_file);
    do_error_dialog(_("\nInvalid project file.\n"));
    return;
  }

  unlink(info_file);
  g_free(info_file);

  if (!is_legal_set_name(mainw->msg,TRUE)) return;

  new_set=g_strdup(mainw->msg);
  set_dir=g_build_filename(prefs->tmpdir,new_set,NULL);

  if (g_file_test(set_dir,G_FILE_TEST_IS_DIR)) {
    msg=g_strdup_printf(_("\nA set called %s already exists.\nIn order to import this project, you must rename or delete the existing set.\nYou can do this by File|Reload Set, and giving the set name\n%s\nthen File|Close/Save all Clips and provide a new set name or discard it.\nOnce you have done this, you will be able to import the new project.\n"),new_set,new_set);
    do_blocking_error_dialog(msg);
    g_free(msg);
    g_free(proj_file);
    g_free(set_dir);
    return;
  }

  g_free(set_dir);

  msg=g_strdup_printf(_("Importing the project %s as set %s..."),proj_file,new_set);
  d_print(msg);
  g_free(msg);

  if (!get_temp_handle(mainw->first_free_file,TRUE)) {
    d_print_failed();
    return;
  }

  com=g_strdup_printf("%s import_project \"%s\" \"%s\"",prefs->backend,cfile->handle,proj_file);
  mainw->com_failed=FALSE;
  lives_system(com,FALSE);
  g_free(com);
  g_free(proj_file);

  if (mainw->com_failed) {
    d_print_failed();
    return;
  }

  do_progress_dialog(TRUE,FALSE,_("Importing project"));

  com=g_strdup_printf("%s close \"%s\"",prefs->backend,cfile->handle);
  lives_system(com,TRUE);
  g_free(com);
  g_free(cfile);
  cfile=NULL;
  if (mainw->first_free_file==-1||mainw->first_free_file>mainw->current_file) mainw->first_free_file=mainw->current_file;
  mainw->current_file=-1;
  sensitize();

  if (mainw->error) {
    g_free(new_set);
    d_print_failed();
    return;
  }

  d_print_done();

  g_snprintf(mainw->set_name,128,"%s",new_set);
  on_load_set_ok(NULL,GINT_TO_POINTER(FALSE));
  g_free(new_set);
}



void
on_export_proj_activate                      (GtkMenuItem     *menuitem,
					      gpointer         user_data)
{
  gchar *filt[]={"*.lv2",NULL};
  gchar *def_file;
  gchar *proj_file;
  gchar *msg;
  gchar *com,*tmp;

  if (strlen(mainw->set_name)==0) {
    int response;
    gchar new_set_name[128];
    do {
      // prompt for a set name, advise user to save set
      renamew=create_rename_dialog(5);
      lives_widget_show(renamew->dialog);
      response=lives_dialog_run(LIVES_DIALOG(renamew->dialog));
      if (response==GTK_RESPONSE_CANCEL) {
	lives_widget_destroy(renamew->dialog);
	g_free(renamew);
	mainw->cancelled=CANCEL_USER;
	return;
      }
      g_snprintf(new_set_name,128,"%s",lives_entry_get_text (LIVES_ENTRY (renamew->entry)));
      lives_widget_destroy(renamew->dialog);
      g_free(renamew);
      lives_widget_context_update();
    } while (!is_legal_set_name(new_set_name,FALSE));
    g_snprintf(mainw->set_name,128,"%s",new_set_name);
  }

  if (mainw->stored_event_list!=NULL&&mainw->stored_event_list_changed) {
    if (!check_for_layout_del(NULL,FALSE)) return;
  }

  if (mainw->sl_undo_mem!=NULL) stored_event_list_free_undos();

  if (!mainw->was_set) {
    mainw->no_exit=TRUE;
    on_save_set_activate(NULL,mainw->set_name);
    mainw->no_exit=FALSE;
    mainw->was_set=TRUE;
    if (mainw->multitrack!=NULL&&!mainw->multitrack->changed) recover_layout_cancelled(FALSE);
  }

  def_file=g_strdup_printf("%s.lv2",mainw->set_name);
  proj_file=choose_file(NULL,def_file,filt,LIVES_FILE_CHOOSER_ACTION_SAVE,NULL,NULL);
  g_free(def_file);

  if (proj_file==NULL) return;

  unlink((tmp=g_filename_from_utf8(proj_file,-1,NULL,NULL,NULL)));
  g_free(tmp);

  if (!check_file(proj_file,FALSE)) {
    g_free(proj_file);
    return;
  }

  msg=g_strdup_printf(_("Exporting project %s..."),proj_file);
  d_print(msg);
  g_free(msg);

  com=g_strdup_printf("%s export_project \"%s\" \"%s\" \"%s\"",prefs->backend,cfile->handle,mainw->set_name,proj_file);
  mainw->com_failed=FALSE;
  lives_system(com,FALSE);
  g_free(com);

  if (mainw->com_failed) {
    g_free(proj_file);
    d_print_failed();
    return;
  }

  cfile->op_dir=g_filename_from_utf8((tmp=get_dir(proj_file)),-1,NULL,NULL,NULL);
  g_free(tmp);

  do_progress_dialog(TRUE,FALSE,_("Exporting project"));

  if (mainw->error) d_print_failed();
  else d_print_done();

  g_free(proj_file);
}






void on_backup_activate (GtkMenuItem *menuitem, gpointer user_data) {
  gchar *filt[]={"*.lv1",NULL};
  gchar *file_name = choose_file (strlen(mainw->proj_save_dir)?mainw->proj_save_dir:NULL,NULL,filt,
				  LIVES_FILE_CHOOSER_ACTION_SAVE,_ ("Backup as .lv1 file"),NULL);

  if (file_name==NULL) return;

  backup_file(mainw->current_file,1,cfile->frames,file_name);

  g_snprintf(mainw->proj_save_dir,PATH_MAX,"%s",file_name);
  get_dirname (mainw->proj_save_dir);
  g_free(file_name);
}



void on_restore_activate (GtkMenuItem *menuitem, gpointer user_data) {
  gchar *filt[]={"*.lv1",NULL};
  gchar *file_name = choose_file (strlen(mainw->proj_load_dir)?mainw->proj_load_dir:NULL,NULL,filt,
				  LIVES_FILE_CHOOSER_ACTION_OPEN,_ ("Restore .lv1 file"),NULL);

  if (file_name==NULL) return;

  restore_file(file_name);

  g_snprintf(mainw->proj_load_dir,PATH_MAX,"%s",file_name);
  get_dirname (mainw->proj_load_dir);
  g_free(file_name);
}



void mt_memory_free(void) {
  int i;

  threaded_dialog_spin();

  mainw->multitrack->no_expose=TRUE;

  if (mainw->current_file>-1&&cfile->achans>0) {
    delete_audio_tracks(mainw->multitrack,mainw->multitrack->audio_draws,FALSE);
    if (mainw->multitrack->audio_vols!=NULL) g_list_free(mainw->multitrack->audio_vols);
  }
  
  if (mainw->multitrack->video_draws!=NULL) {
    for (i=0;i<mainw->multitrack->num_video_tracks;i++) {
      delete_video_track(mainw->multitrack,i,FALSE);
    }
    g_list_free (mainw->multitrack->video_draws);
  }
  
  g_object_unref(mainw->multitrack->clip_scroll);
  g_object_unref(mainw->multitrack->in_out_box);
  
  g_list_free(mainw->multitrack->tl_marks);
  
  if (mainw->multitrack->event_list!=NULL) event_list_free(mainw->multitrack->event_list);
  mainw->multitrack->event_list=NULL;

  if (mainw->multitrack->undo_mem!=NULL) event_list_free_undos(mainw->multitrack);

  recover_layout_cancelled(FALSE);

  threaded_dialog_spin();
}




void
on_quit_activate                      (GtkMenuItem     *menuitem,
				       gpointer         user_data)
{
  int i;
  boolean has_layout_map=FALSE;
  gchar *com,*esave_dir,*msg;
  boolean had_clips=FALSE,legal_set_name;

  if (user_data!=NULL&&GPOINTER_TO_INT(user_data)==1) mainw->only_close=TRUE;
  else mainw->only_close=FALSE;

  // stop if playing
  if (mainw->playing_file>-1) {
    mainw->cancelled=CANCEL_APP_QUIT;
    return;
  }

  if (mainw->multitrack!=NULL) {
    if (mainw->multitrack->idlefunc>0) {
      g_source_remove(mainw->multitrack->idlefunc);
      mainw->multitrack->idlefunc=0;
    }
    mt_desensitise(mainw->multitrack);
  }

  if (mainw->multitrack!=NULL&&mainw->multitrack->event_list!=NULL) {
    if (mainw->only_close) {
      if (!check_for_layout_del(mainw->multitrack,FALSE)) {
	if (mainw->multitrack!=NULL) {
	  mt_sensitise(mainw->multitrack);
	  mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
	}
	return;
      }
    }
  }

  if (mainw->stored_event_list!=NULL&&mainw->stored_event_list_changed) {
    if (!check_for_layout_del(NULL,FALSE)) return;
  }
  else if (mainw->stored_layout_undos!=NULL) {
    stored_event_list_free_undos();
  }
  
  // do not popup layout errors if the set name changes
  if (!mainw->only_close) mainw->is_exiting=TRUE;

  if (mainw->scrap_file>-1) close_scrap_file();
  if (mainw->ascrap_file>-1) close_ascrap_file();

  if (mainw->clips_available>0) {
    gchar *set_name;
    _entryw *cdsw=create_cds_dialog(1);
    int resp;
    had_clips=TRUE;
    do {
      legal_set_name=TRUE;
      lives_widget_show(cdsw->dialog);
      resp=lives_dialog_run(LIVES_DIALOG(cdsw->dialog));
      if (resp==0) {
	lives_widget_destroy(cdsw->dialog);
	g_free(cdsw);
	if (mainw->multitrack!=NULL) {
	  mt_sensitise(mainw->multitrack);
	  mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
	}
	return;
      }
      if (resp==2) {
	// save set
	if ((legal_set_name=is_legal_set_name((set_name=g_strdup(lives_entry_get_text(LIVES_ENTRY(cdsw->entry)))),TRUE))) {
	  lives_widget_destroy(cdsw->dialog);
	  g_free(cdsw);

	  if (prefs->ar_clipset) set_pref("ar_clipset",set_name);
	  else set_pref("ar_clipset","");
	  mainw->no_exit=FALSE;
	  mainw->leave_recovery=FALSE;
	  on_save_set_activate(NULL,set_name);

	  if (mainw->multitrack!=NULL) {
	    mt_sensitise(mainw->multitrack);
	    mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
	  }
	  g_free(set_name);
	  return;
	}
	legal_set_name=FALSE;
	lives_widget_hide(cdsw->dialog);
	g_free(set_name);
      }
      if (mainw->was_set&&legal_set_name) {
	if (!do_warning_dialog(_("\n\nSet will be deleted from the disk.\nAre you sure ?\n"))) {
	  resp=2;
	}
      }
    } while (resp==2);

    // discard clipset

    lives_widget_destroy(cdsw->dialog);
    g_free(cdsw);

    set_pref("ar_clipset","");
    prefs->ar_clipset=FALSE;
  
    if (mainw->multitrack!=NULL) {
      event_list_free_undos(mainw->multitrack);
      
      if (mainw->multitrack->event_list!=NULL) {
	event_list_free(mainw->multitrack->event_list);
	mainw->multitrack->event_list=NULL;
      }
    }
  
    // check for layout maps
    if (mainw->current_layouts_map!=NULL) {
      has_layout_map=TRUE;
    }

    if (has_layout_map) {
      if (prompt_remove_layout_files()) {
	// delete layout files
	for (i=1;i<MAX_FILES;i++) {
	  if (!(mainw->files[i]==NULL)) {
	    if (mainw->was_set&&mainw->files[i]->layout_map!=NULL) {
	      remove_layout_files(mainw->files[i]->layout_map);
	    }
	  }
	}
	// delete layout directory
	esave_dir=g_build_filename(prefs->tmpdir,mainw->set_name,"layouts",NULL);
#ifndef IS_MINGW
	com=g_strdup_printf("/bin/rm -r \"%s/\" 2>/dev/null &",esave_dir);
#else
	com=g_strdup_printf("START /MIN /b rm.exe -r \"%s/\" 2>NUL",esave_dir);
#endif
	lives_system(com,TRUE);
	g_free(com);
	g_free(esave_dir);
      }
    }
  }

  if (mainw->multitrack!=NULL&&!mainw->only_close) mt_memory_free();
  else if (mainw->multitrack!=NULL) wipe_layout(mainw->multitrack);;

  mainw->was_set=mainw->leave_files=mainw->leave_recovery=FALSE;

  recover_layout_cancelled(FALSE);

  if (had_clips) {
    if (strlen(mainw->set_name))
      msg=g_strdup_printf(_("Deleting set %s..."),mainw->set_name);
    else
      msg=g_strdup_printf(_("Deleting set..."),mainw->set_name);
    d_print(msg);
    g_free(msg);

    do_threaded_dialog(_("Deleting set"),FALSE);
  }

  prefs->crash_recovery=FALSE;
  lives_exit();
  prefs->crash_recovery=TRUE;

  if (strlen(mainw->set_name)>0) {
    msg=g_strdup_printf(_("Set %s was permanently deleted from the disk.\n"),mainw->set_name);
    d_print(msg);
    g_free(msg);
    memset(mainw->set_name,0,1);
  }

  mainw->leave_files=mainw->leave_recovery=TRUE;
}



// TODO - split into undo.c
void
on_undo_activate                      (GtkMenuItem     *menuitem,
				       gpointer         user_data)
{
  gchar *com;
  int ostart=cfile->start;
  int oend=cfile->end;
  gchar msg[256];
  int current_file=mainw->current_file;
  int switch_file=current_file;
  int asigned,aendian;

  boolean bad_header=FALSE;
  boolean retvalb;

  lives_widget_set_sensitive (mainw->undo, FALSE);
  lives_widget_set_sensitive (mainw->redo, TRUE);
  cfile->undoable=FALSE;
  cfile->redoable=TRUE;
  lives_widget_hide(mainw->undo);
  lives_widget_show(mainw->redo);

  mainw->osc_block=TRUE;

  d_print("");

  if (menuitem!=NULL) {
    get_menu_text (mainw->undo,msg);
    mainw->no_switch_dprint=TRUE;
    d_print(msg);
    d_print ("...");
    mainw->no_switch_dprint=FALSE;
  }


  if (cfile->undo_action==UNDO_INSERT_SILENCE) {
    on_del_audio_activate(NULL,NULL);
    cfile->undo_action=UNDO_INSERT_SILENCE;
    set_redoable(_("Insert Silence"),TRUE);
  }

  if (cfile->undo_action==UNDO_CUT||cfile->undo_action==UNDO_DELETE||cfile->undo_action==UNDO_DELETE_AUDIO) {
    int reset_achans=0;
    unlink(cfile->info_file);

    cfile->arate=cfile->undo_arate;
    cfile->signed_endian=cfile->undo_signed_endian;
    cfile->achans=cfile->undo_achans;
    cfile->asampsize=cfile->undo_asampsize;
    cfile->arps=cfile->undo_arps;

    if (cfile->frames==0) {
      cfile->hsize=cfile->ohsize;
      cfile->vsize=cfile->ovsize;
    }

    if (cfile->undo_action==UNDO_DELETE_AUDIO) {
      if (cfile->undo1_dbl==cfile->undo2_dbl&&cfile->undo1_dbl==0.) {
	// undo delete_all_audio
	reset_achans=cfile->undo_achans;
	com=g_strdup_printf("%s undo_audio \"%s\"",prefs->backend_sync,cfile->handle);
      }
      // undo delete selected audio
      // (set with with_audio==2 [audio only],therfore start,end,where are in secs.; times==-1)
      else com=g_strdup_printf("%s insert \"%s\" \"%s\" %.8f 0. %.8f \"%s\" 2 0 0 0 0 %d %d %d %d %d -1",
			       prefs->backend,
			       cfile->handle,get_image_ext_for_type(cfile->img_type),cfile->undo1_dbl,
			       cfile->undo2_dbl-cfile->undo1_dbl, cfile->handle, cfile->arps, cfile->achans, 
			       cfile->asampsize,!(cfile->signed_endian&AFORM_UNSIGNED),
			       !(cfile->signed_endian&AFORM_BIG_ENDIAN));
    }
    else {
      // undo cut or delete (times to insert is -1)
      // start,end, where are in frames
      cfile->undo1_boolean&=mainw->ccpd_with_sound;
      com=g_strdup_printf("%s insert \"%s\" \"%s\" %d %d %d \"%s\" %d %d 0 0 %.3f %d %d %d %d %d -1",
			  prefs->backend,cfile->handle,
			  get_image_ext_for_type(cfile->img_type),cfile->undo_start-1,cfile->undo_start,
			  cfile->undo_end,cfile->handle, cfile->undo1_boolean, cfile->frames, cfile->fps, 
			  cfile->arps, cfile->achans, cfile->asampsize, !(cfile->signed_endian&AFORM_UNSIGNED),
			  !(cfile->signed_endian&AFORM_BIG_ENDIAN));

    }

    mainw->com_failed=FALSE;
    lives_system(com,FALSE);
    g_free(com);

    if (mainw->com_failed) return;

    // show a progress dialog, not cancellable
    do_progress_dialog(TRUE,FALSE,_("Undoing"));


    if (mainw->error) {
      d_print_failed();
      //cfile->may_be_damaged=TRUE;
      return;
    }

    if (cfile->undo_action!=UNDO_DELETE_AUDIO) {
      cfile->insert_start=cfile->undo_start;
      cfile->insert_end=cfile->undo_end;
      
      if (cfile->start>=cfile->undo_start) {
	cfile->start+=cfile->undo_end-cfile->undo_start+1;
      }
      if (cfile->end>=cfile->undo_start) {
	cfile->end+=cfile->undo_end-cfile->undo_start+1;
      }
      
      cfile->frames+=cfile->undo_end-cfile->undo_start+1;
      if (cfile->frames>0) {
	if (cfile->start==0) {
	  cfile->start=1;
	}
	if (cfile->end==0) {
	  cfile->end=cfile->frames;
	}
      }
      if (cfile->frame_index_back!=NULL) {
	restore_frame_index_back(mainw->current_file);
      }
      save_clip_value(mainw->current_file,CLIP_DETAILS_FRAMES,&cfile->frames);
      if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
    }
    if (reset_achans>0) {
      asigned=!(cfile->signed_endian&AFORM_UNSIGNED);
      aendian=cfile->signed_endian&AFORM_BIG_ENDIAN;
      cfile->achans=reset_achans;
      save_clip_value(mainw->current_file,CLIP_DETAILS_ACHANS,&cfile->achans);
      if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
      save_clip_value(mainw->current_file,CLIP_DETAILS_ARATE,&cfile->arps);
      if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
      save_clip_value(mainw->current_file,CLIP_DETAILS_PB_ARATE,&cfile->arate);
      if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
      save_clip_value(mainw->current_file,CLIP_DETAILS_ASAMPS,&cfile->asampsize);
      if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
      save_clip_value(mainw->current_file,CLIP_DETAILS_AENDIAN,&aendian);
      if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
      save_clip_value(mainw->current_file,CLIP_DETAILS_ASIGNED,&asigned);
      if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
    }

    reget_afilesize (mainw->current_file);
    get_play_times();

    if (bad_header) do_header_write_error(mainw->current_file);

  }


  if (cfile->undo_action==UNDO_RESIZABLE||cfile->undo_action==UNDO_RENDER||cfile->undo_action==UNDO_EFFECT||
      cfile->undo_action==UNDO_MERGE||(cfile->undo_action==UNDO_ATOMIC_RESAMPLE_RESIZE&&
				       (cfile->frames!=cfile->old_frames||cfile->hsize!=cfile->ohsize||
					cfile->vsize!=cfile->ovsize||cfile->fps!=cfile->undo1_dbl))) {
    gchar *audfile;

    com=g_strdup_printf("%s undo \"%s\" %d %d \"%s\"",prefs->backend,cfile->handle,cfile->undo_start,cfile->undo_end,
			get_image_ext_for_type(cfile->img_type));
    unlink(cfile->info_file);
    mainw->com_failed=FALSE;
    lives_system(com,FALSE);
    g_free(com);

    if (mainw->com_failed) return;
    
    mainw->com_failed=FALSE;
    mainw->cancelled=CANCEL_NONE;
    mainw->error=FALSE;
    
    // show a progress dialog, not cancellable
    cfile->progress_start=cfile->undo_start;
    cfile->progress_end=cfile->undo_end;
    do_progress_dialog(TRUE,FALSE,_ ("Undoing"));

    if (cfile->undo_action!=UNDO_ATOMIC_RESAMPLE_RESIZE) {
      audfile=g_strdup_printf("%s/%s/audio.bak",prefs->tmpdir,cfile->handle);
      if (g_file_test (audfile, G_FILE_TEST_EXISTS)) {
	// restore overwritten audio
	com=g_strdup_printf("%s undo_audio \"%s\"",prefs->backend_sync,cfile->handle);
	mainw->com_failed=FALSE;
	unlink (cfile->info_file);
	lives_system(com,FALSE);
	g_free(com);
	if (mainw->com_failed) return;

	retvalb=do_auto_dialog(_("Restoring audio..."),0);
	if (!retvalb) {
	  d_print_failed();
	  //cfile->may_be_damaged=TRUE;
	  return;
	}
	reget_afilesize(mainw->current_file);
      }
      g_free(audfile);
    }

    if (cfile->frame_index_back!=NULL) {
      int *tmpindex=cfile->frame_index;
      cfile->clip_type=CLIP_TYPE_FILE;
      cfile->frame_index=cfile->frame_index_back;
      if (cfile->undo_action==UNDO_RENDER) {
	do_threaded_dialog(_("Clearing frame images"),FALSE);
	clean_images_from_virtual(cfile,cfile->undo_end);
	save_frame_index(mainw->current_file);
	cfile->frame_index_back=NULL;
	end_threaded_dialog();
      }
      else {
	save_frame_index(mainw->current_file);
	cfile->frame_index_back=tmpindex;
      }
    }
  }

  if (cfile->undo_action==UNDO_ATOMIC_RESAMPLE_RESIZE&&(cfile->frames!=cfile->old_frames||
							cfile->hsize!=cfile->ohsize||cfile->vsize!=cfile->ovsize)) {

    if (cfile->frames>cfile->old_frames) {
      com=g_strdup_printf("%s cut \"%s\" %d %d %d %d \"%s\" %.3f %d %d %d",
			  prefs->backend,cfile->handle,cfile->old_frames+1, 
			  cfile->frames, FALSE, cfile->frames, get_image_ext_for_type(cfile->img_type), 
			  cfile->fps, cfile->arate, cfile->achans, cfile->asampsize);

      cfile->progress_start=cfile->old_frames+1;
      cfile->progress_end=cfile->frames;

      unlink(cfile->info_file);
      mainw->com_failed=FALSE;
      lives_system(com,FALSE);
      g_free(com);

      if (mainw->com_failed) return;

      // show a progress dialog, not cancellable
      do_progress_dialog(TRUE,FALSE,_ ("Deleting excess frames"));
      
      if (cfile->clip_type==CLIP_TYPE_FILE) {
	delete_frames_from_virtual (mainw->current_file, cfile->old_frames+1, cfile->frames);
      }
    }

    cfile->frames=cfile->old_frames;
    cfile->hsize=cfile->ohsize;
    cfile->vsize=cfile->ovsize;
    save_clip_value(mainw->current_file,CLIP_DETAILS_WIDTH,&cfile->hsize);
    if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
    save_clip_value(mainw->current_file,CLIP_DETAILS_HEIGHT,&cfile->vsize);
    if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
    cfile->fps=cfile->undo1_dbl;

    save_clip_value(mainw->current_file,CLIP_DETAILS_FPS,&cfile->fps);
    if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
    save_clip_value(mainw->current_file,CLIP_DETAILS_PB_FPS,&cfile->fps);
    if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
    cfile->redoable=FALSE;
    // force a resize in switch_to_file
    switch_file=0;

    if (bad_header) do_header_write_error(mainw->current_file);
  }

  if (cfile->undo_action==UNDO_RENDER) {
    cfile->frames=cfile->old_frames;
    save_clip_value(mainw->current_file,CLIP_DETAILS_FRAMES,&cfile->frames);
    if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
    if (bad_header) do_header_write_error(mainw->current_file);
  }

  if (cfile->undo_action==UNDO_INSERT||cfile->undo_action==UNDO_MERGE||cfile->undo_action==UNDO_INSERT_WITH_AUDIO) {
    boolean ccpd_with_sound=mainw->ccpd_with_sound;
    if (!(cfile->undo_action==UNDO_MERGE&&cfile->insert_start==cfile->undo_start&&cfile->insert_end==cfile->undo_end)) {
      if (cfile->undo_action==UNDO_MERGE) {
	if (cfile->insert_start==cfile->undo_start) {
	  cfile->insert_start=cfile->undo_end+1;
	}
	if (cfile->insert_end==cfile->undo_end) {
	  cfile->insert_end=cfile->undo_start-1;
	}
      }
      cfile->start=cfile->insert_start;
      cfile->end=cfile->insert_end;

      if (cfile->undo_action==UNDO_INSERT_WITH_AUDIO) mainw->ccpd_with_sound=TRUE;
      else mainw->ccpd_with_sound=FALSE;
      on_delete_activate (NULL,NULL);

      cfile->start=ostart;
      if (ostart>=cfile->insert_start) {
	cfile->start-=cfile->insert_end-cfile->insert_start+1;
	if (cfile->start<cfile->insert_start-1) {
	  cfile->start=cfile->insert_start-1;
	}
      }
      cfile->end=oend;
      if (oend>=cfile->insert_start) {
	cfile->end-=cfile->insert_end-cfile->insert_start+1;
	if (cfile->end<cfile->insert_start-1) {
	  cfile->end=cfile->insert_start-1;
	}
      }
      // TODO - use lives_clip_start macro
      if (cfile->start<1) cfile->start=cfile->frames>0?1:0;
      if (cfile->end<1) cfile->end=cfile->frames>0?1:0;
      
      cfile->insert_start=cfile->insert_end=0;
    }
    mainw->ccpd_with_sound=ccpd_with_sound;
    save_clip_value(mainw->current_file,CLIP_DETAILS_FRAMES,&cfile->frames);
    if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
    if (bad_header) do_header_write_error(mainw->current_file);
  }

  if (cfile->undo_action==UNDO_REC_AUDIO) {
    mainw->fx1_val=cfile->arate;
    mainw->fx2_val=cfile->achans;
    mainw->fx3_val=cfile->asampsize;
    mainw->fx4_val=cfile->signed_endian;
    mainw->fx5_val=cfile->arps;
  }
  
  if (cfile->undo_action==UNDO_AUDIO_RESAMPLE||cfile->undo_action==UNDO_REC_AUDIO||cfile->undo_action==UNDO_FADE_AUDIO||
      cfile->undo_action==UNDO_TRIM_AUDIO||cfile->undo_action==UNDO_APPEND_AUDIO||
      (cfile->undo_action==UNDO_ATOMIC_RESAMPLE_RESIZE&&cfile->arate!=cfile->undo1_int)) {
    unlink(cfile->info_file);
    com=g_strdup_printf("%s undo_audio \"%s\"",prefs->backend_sync,cfile->handle);
    mainw->com_failed=FALSE;
    mainw->cancelled=CANCEL_NONE;
    mainw->error=FALSE;
    lives_system (com,FALSE);
    g_free (com);

    if (mainw->com_failed) {
      d_print_failed();
      return;
    }
    if (!do_auto_dialog(_("Undoing"),0)) {
      d_print_failed();
      return;
    }

  }

  if ((cfile->undo_action==UNDO_AUDIO_RESAMPLE)||(cfile->undo_action==UNDO_ATOMIC_RESAMPLE_RESIZE&&
						  cfile->arate!=cfile->undo1_int)) {

    cfile->arate+=cfile->undo1_int;
    cfile->undo1_int=cfile->arate-cfile->undo1_int;
    cfile->arate-=cfile->undo1_int;

    cfile->achans+=cfile->undo2_int;
    cfile->undo2_int=cfile->achans-cfile->undo2_int;
    cfile->achans-=cfile->undo2_int;

    cfile->asampsize+=cfile->undo3_int;
    cfile->undo3_int=cfile->asampsize-cfile->undo3_int;
    cfile->asampsize-=cfile->undo3_int;

    cfile->arps+=cfile->undo4_int;
    cfile->undo4_int=cfile->arps-cfile->undo4_int;
    cfile->arps-=cfile->undo4_int;

    cfile->signed_endian+=cfile->undo1_uint;
    cfile->undo1_uint=cfile->signed_endian-cfile->undo1_uint;
    cfile->signed_endian-=cfile->undo1_uint;

    asigned=!(cfile->signed_endian&AFORM_UNSIGNED);
    aendian=cfile->signed_endian&AFORM_BIG_ENDIAN;

    save_clip_value(mainw->current_file,CLIP_DETAILS_ARATE,&cfile->arps);
    if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
    save_clip_value(mainw->current_file,CLIP_DETAILS_PB_ARATE,&cfile->arate);
    if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
    save_clip_value(mainw->current_file,CLIP_DETAILS_ACHANS,&cfile->achans);
    if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
    save_clip_value(mainw->current_file,CLIP_DETAILS_ASAMPS,&cfile->asampsize);
    if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
    save_clip_value(mainw->current_file,CLIP_DETAILS_AENDIAN,&aendian);
    if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
    save_clip_value(mainw->current_file,CLIP_DETAILS_ASIGNED,&asigned);
    if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;

    if (bad_header) do_header_write_error(mainw->current_file);
  }
    
  if (cfile->undo_action==UNDO_NEW_AUDIO) {
    unlink(cfile->info_file);
    com=g_strdup_printf("%s undo_audio \"%s\"",prefs->backend_sync,cfile->handle);
    mainw->com_failed=FALSE;
    lives_system(com,FALSE);
    g_free(com);

    if (mainw->com_failed) return;

    mainw->cancelled=CANCEL_NONE;
    mainw->error=FALSE;

    if (!do_auto_dialog(_("Restoring audio..."),0)) {
      d_print_failed();
      return;
    }

    cfile->achans=cfile->undo_achans;
    cfile->arate=cfile->undo_arate;
    cfile->arps=cfile->undo_arps;
    cfile->asampsize=cfile->undo_asampsize;
    cfile->signed_endian=cfile->undo_signed_endian;
    
    reget_afilesize(mainw->current_file);
    
    asigned=!(cfile->signed_endian&AFORM_UNSIGNED);
    aendian=cfile->signed_endian&AFORM_BIG_ENDIAN;
    
    save_clip_value(mainw->current_file,CLIP_DETAILS_ARATE,&cfile->arps);
    if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
    save_clip_value(mainw->current_file,CLIP_DETAILS_PB_ARATE,&cfile->arate);
    if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
    save_clip_value(mainw->current_file,CLIP_DETAILS_ACHANS,&cfile->achans);
    if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
    save_clip_value(mainw->current_file,CLIP_DETAILS_ASAMPS,&cfile->asampsize);
    if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
    save_clip_value(mainw->current_file,CLIP_DETAILS_AENDIAN,&aendian);
    if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
    save_clip_value(mainw->current_file,CLIP_DETAILS_ASIGNED,&asigned);
    if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
    
    if (bad_header) do_header_write_error(mainw->current_file);
  }


  if (cfile->undo_action==UNDO_CHANGE_SPEED) {
    cfile->fps+=cfile->undo1_dbl;
    cfile->undo1_dbl=cfile->fps-cfile->undo1_dbl;
    cfile->fps-=cfile->undo1_dbl;

    cfile->arate+=cfile->undo1_int;
    cfile->undo1_int=cfile->arate-cfile->undo1_int;
    cfile->arate-=cfile->undo1_int;
    save_clip_value(mainw->current_file,CLIP_DETAILS_FPS,&cfile->fps);
    if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
    save_clip_value(mainw->current_file,CLIP_DETAILS_PB_FPS,&cfile->fps);
    if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
    save_clip_value(mainw->current_file,CLIP_DETAILS_PB_ARATE,&cfile->arate);
    if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;

    if (bad_header) do_header_write_error(mainw->current_file);
  }

  if (cfile->undo_action==UNDO_INSERT||cfile->undo_action==UNDO_INSERT_WITH_AUDIO||cfile->undo_action==UNDO_MERGE||
      cfile->undo_action==UNDO_NEW_AUDIO) {
    cfile->redoable=FALSE;
  }

  if (menuitem!=NULL) {
    mainw->no_switch_dprint=TRUE;
    d_print_done();
    mainw->no_switch_dprint=FALSE;
  }

  if (cfile->undo_action==UNDO_RESAMPLE) {
    cfile->start=(int)((cfile->start-1)/cfile->fps*cfile->undo1_dbl+1.);
    if ((cfile->end=(int) (cfile->end/cfile->fps*cfile->undo1_dbl+.49999))<1) cfile->end=1;
    cfile->fps+=cfile->undo1_dbl; 
    cfile->undo1_dbl=cfile->fps-cfile->undo1_dbl;
    cfile->fps-=cfile->undo1_dbl;

    // deorder the frames
    cfile->frames=deorder_frames(cfile->old_frames,mainw->current_file==0&&!prefs->conserve_space);

    save_clip_value(mainw->current_file,CLIP_DETAILS_FRAMES,&cfile->frames);
    if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
    save_clip_value(mainw->current_file,CLIP_DETAILS_FPS,&cfile->fps);
    if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
    save_clip_value(mainw->current_file,CLIP_DETAILS_PB_FPS,&cfile->fps);
    if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;

    if (bad_header) do_header_write_error(mainw->current_file);

    if (mainw->current_file>0) {
      com=g_strdup_printf(_ ("Length of video is now %d frames at %.3f frames per second.\n"),cfile->frames,cfile->fps);
    }
    else {
      mainw->no_switch_dprint=TRUE;
      com=g_strdup_printf(_ ("Clipboard was resampled to %d frames.\n"),cfile->frames);
    }
    d_print(com);
    g_free(com);
    mainw->no_switch_dprint=FALSE;
  }

  if (cfile->end>cfile->frames) {
    cfile->end=cfile->frames;
  }

  if (cfile->undo_action==UNDO_RESIZABLE) {
    cfile->vsize+=cfile->ovsize;
    cfile->ovsize=cfile->vsize-cfile->ovsize;
    cfile->vsize-=cfile->ovsize;
    cfile->hsize+=cfile->ohsize;
    cfile->ohsize=cfile->hsize-cfile->ohsize;
    cfile->hsize-=cfile->ohsize;
    save_clip_value(mainw->current_file,CLIP_DETAILS_WIDTH,&cfile->hsize);
    if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
    save_clip_value(mainw->current_file,CLIP_DETAILS_HEIGHT,&cfile->vsize);
    if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;

    // force a resize in switch_to_file
    switch_file=0;

    if (bad_header) do_header_write_error(mainw->current_file);

  }
  
  if (current_file>0) {
    switch_to_file((mainw->current_file=switch_file),current_file);
  }

  if (cfile->undo_action==UNDO_RENDER) {
    if (mainw->event_list!=NULL) event_list_free (mainw->event_list);
    mainw->event_list=cfile->event_list_back;
    cfile->event_list_back=NULL;
    deal_with_render_choice(FALSE);
  }
  mainw->osc_block=FALSE;

}

void
on_redo_activate                      (GtkMenuItem     *menuitem,
				       gpointer         user_data)
{
  gchar *com;
  int ostart=cfile->start;
  int oend=cfile->end;
  int current_file=mainw->current_file;
  gchar msg[256];

  mainw->osc_block=TRUE;

  cfile->undoable=TRUE;
  cfile->redoable=FALSE;
  lives_widget_hide(mainw->redo);
  lives_widget_show(mainw->undo);
  lives_widget_set_sensitive(mainw->undo,TRUE);
  lives_widget_set_sensitive(mainw->redo,FALSE);

  d_print("");

  if (menuitem!=NULL) {
    get_menu_text (mainw->redo,msg);
    mainw->no_switch_dprint=TRUE;
    d_print(msg);
    d_print ("...");
    mainw->no_switch_dprint=FALSE;
  }

  if (cfile->undo_action==UNDO_INSERT_SILENCE) {
    on_ins_silence_activate (NULL,NULL);
    mainw->osc_block=FALSE;
    mainw->no_switch_dprint=TRUE;
    d_print_done();
    mainw->no_switch_dprint=FALSE;
    sensitize();
    return;
  }
  if (cfile->undo_action==UNDO_CHANGE_SPEED) {
    on_change_speed_ok_clicked (NULL,NULL);
    mainw->osc_block=FALSE;
    d_print_done();
    return;
  }
  if (cfile->undo_action==UNDO_RESAMPLE) {
    on_resample_vid_ok (NULL,NULL);
    mainw->osc_block=FALSE;
    return;
  }
  if (cfile->undo_action==UNDO_AUDIO_RESAMPLE) {
    on_resaudio_ok_clicked (NULL,NULL);
    mainw->osc_block=FALSE;
    d_print_done();
    return;
  }
  if (cfile->undo_action==UNDO_CUT||cfile->undo_action==UNDO_DELETE) {
    cfile->start=cfile->undo_start;
    cfile->end=cfile->undo_end;
    mainw->osc_block=FALSE;
  }
  if (cfile->undo_action==UNDO_CUT) {
    on_cut_activate(NULL,NULL);
    mainw->osc_block=FALSE;
  }
  if (cfile->undo_action==UNDO_DELETE) {
    on_delete_activate(NULL,NULL);
    mainw->osc_block=FALSE;
  }
  if (cfile->undo_action==UNDO_DELETE_AUDIO) {
    on_del_audio_activate(NULL,NULL);
    mainw->osc_block=FALSE;
    d_print_done();
    return;
  }
  if (cfile->undo_action==UNDO_CUT||cfile->undo_action==UNDO_DELETE) {
    cfile->start=ostart;
    cfile->end=oend;
    if (mainw->current_file==current_file) {
      if (cfile->start>=cfile->undo_start) {
	cfile->start-=cfile->undo_end-cfile->undo_start+1;
	if (cfile->start<cfile->undo_start-1) {
	  cfile->start=cfile->undo_start-1;
	}
      }
      if (cfile->end>=cfile->undo_start) {
	cfile->end-=cfile->undo_end-cfile->undo_start+1;
	if (cfile->end<cfile->undo_start-1) {
	  cfile->end=cfile->undo_start-1;
	}
      }
      switch_to_file (mainw->current_file,mainw->current_file);
    }
    mainw->osc_block=FALSE;
    return;
  }

  if (cfile->undo_action==UNDO_REC_AUDIO) {
    cfile->arate=mainw->fx1_val;
    cfile->achans=mainw->fx2_val;
    cfile->asampsize=mainw->fx3_val;
    cfile->signed_endian=mainw->fx4_val;
    cfile->arps=mainw->fx5_val;
    save_clip_values(mainw->current_file);
  }

  if (cfile->undo_action==UNDO_REC_AUDIO||cfile->undo_action==UNDO_FADE_AUDIO||cfile->undo_action==UNDO_TRIM_AUDIO||
      cfile->undo_action==UNDO_APPEND_AUDIO) {
    com=g_strdup_printf("%s undo_audio \"%s\"",prefs->backend_sync,cfile->handle);
    unlink(cfile->info_file);
    mainw->com_failed=FALSE;
    lives_system(com,FALSE);
    g_free(com);

    if (mainw->com_failed) {
      d_print_failed();
      return;
    }

    // show a progress dialog, not cancellable
    do_progress_dialog(TRUE,FALSE,_ ("Redoing"));

    if (mainw->error) {
      d_print_failed();
      return;
    }

    d_print_done();
    switch_to_file (mainw->current_file,mainw->current_file);
    mainw->osc_block=FALSE;
    return;
  }

  com=g_strdup_printf("%s redo \"%s\" %d %d \"%s\"",prefs->backend,cfile->handle,cfile->undo_start,cfile->undo_end,
		      get_image_ext_for_type(cfile->img_type));
  unlink(cfile->info_file);
  mainw->com_failed=FALSE;
  lives_system(com,FALSE);
  g_free(com);

  if (mainw->com_failed) {
    d_print_failed();
    return;
  }

  cfile->progress_start=cfile->undo_start;
  cfile->progress_end=cfile->undo_end;

  // show a progress dialog, not cancellable
  do_progress_dialog(TRUE,FALSE,_ ("Redoing"));

  if (mainw->error) {
    d_print_failed();
    return;
  }

  if (cfile->clip_type==CLIP_TYPE_FILE&&(cfile->undo_action==UNDO_EFFECT||cfile->undo_action==UNDO_RESIZABLE)) {
    int *tmpindex=cfile->frame_index;
    cfile->frame_index=cfile->frame_index_back;
    cfile->frame_index_back=tmpindex;
    cfile->clip_type=CLIP_TYPE_FILE;
    if (!check_if_non_virtual(mainw->current_file,1,cfile->frames)) save_frame_index(mainw->current_file);
  }

  if (cfile->undo_action==UNDO_RESIZABLE) {
    cfile->vsize+=cfile->ovsize;
    cfile->ovsize=cfile->vsize-cfile->ovsize;
    cfile->vsize-=cfile->ovsize;
    cfile->hsize+=cfile->ohsize;
    cfile->ohsize=cfile->hsize-cfile->ohsize;
    cfile->hsize-=cfile->ohsize;
    switch_to_file ((mainw->current_file=0),current_file);
  }
  else {
    if (cfile->start>=cfile->undo_start) load_start_image(cfile->start);
    if (cfile->end<=cfile->undo_end) load_end_image(cfile->end);
  }

  d_print_done();
  mainw->osc_block=FALSE;
}



//////////////////////////////////////////////////

void on_copy_activate (GtkMenuItem *menuitem, gpointer user_data) {
  gchar *com;
  gchar *text=g_strdup_printf(_ ("Copying frames %d to %d%s to the clipboard..."),cfile->start,cfile->end,
			      mainw->ccpd_with_sound&&cfile->achans>0?" (with sound)":"");


  int current_file=mainw->current_file;
  int start,end;

  desensitize();

  d_print(""); // force switchtext
  d_print(text);
  g_free(text);

  init_clipboard();

  unlink(cfile->info_file);
  mainw->last_transition_loops=1;

  start=cfile->start;
  end=cfile->end;

  if (cfile->clip_type==CLIP_TYPE_FILE) {
    clipboard->clip_type=CLIP_TYPE_FILE;
    clipboard->interlace=cfile->interlace;
    clipboard->deinterlace=cfile->deinterlace;
    clipboard->frame_index=frame_index_copy(cfile->frame_index,end-start+1,start-1);
    clipboard->frames=end-start+1;
    check_if_non_virtual(0,1,clipboard->frames);
    if (clipboard->clip_type==CLIP_TYPE_FILE) {
      clipboard->ext_src=clone_decoder(mainw->current_file);
      end=-end;
    }
  }

  mainw->fx1_val=1;
  mainw->fx1_bool=FALSE;

  clipboard->img_type=cfile->img_type;

  com=g_strdup_printf("%s insert \"%s\" \"%s\" 0 %d %d \"%s\" %d 0 0 0 %.3f %d %d %d %d %d",prefs->backend,
		      clipboard->handle, get_image_ext_for_type(clipboard->img_type),
		      start, end, cfile->handle, mainw->ccpd_with_sound, cfile->fps, cfile->arate, 
		      cfile->achans, cfile->asampsize, !(cfile->signed_endian&AFORM_UNSIGNED),
		      !(cfile->signed_endian&AFORM_BIG_ENDIAN));

  if (clipboard->clip_type==CLIP_TYPE_FILE) end=-end;

  mainw->com_failed=FALSE;
  lives_system(com,FALSE);
  g_free(com);

  if (mainw->com_failed) {
    d_print_failed();
    return;
  }

  // we need to set this to look at the right info_file
  mainw->current_file=0;
  cfile->progress_start=clipboard->start=1;
  cfile->progress_end=clipboard->end=end-start+1;

  // stop the 'preview' and 'pause' buttons from appearing
  cfile->nopreview=TRUE;
  if (!do_progress_dialog(TRUE,TRUE,_ ("Copying to the clipboard"))) {
#ifdef IS_MINGW
    // kill any active processes: for other OSes the backend does this
    // get pid from backend
    FILE *rfile;
    ssize_t rlen;
    char val[16];
    int pid;
    com=g_strdup_printf("%s get_pid_for_handle \"%s\"",prefs->backend_sync,cfile->handle);
    rfile=popen(com,"r");
    rlen=fread(val,1,16,rfile);
    pclose(rfile);
    memset(val+rlen,0,1);
    pid=atoi(val);
    
    lives_win32_kill_subprocesses(pid,TRUE);
#endif

    // close clipboard, it is invalid

    com=g_strdup_printf ("%s close \"%s\"",prefs->backend,clipboard->handle);
    lives_system (com,FALSE);
    g_free (com);
    clipboard=NULL;
    mainw->current_file=current_file;
    sensitize();
    mainw->cancelled=CANCEL_USER;
    return;
  }

  cfile->nopreview=FALSE;
  mainw->current_file=current_file;

  //set all clipboard details
  clipboard->frames=clipboard->old_frames=clipboard->end;
  clipboard->hsize=cfile->hsize;
  clipboard->vsize=cfile->vsize;
  clipboard->bpp=cfile->bpp;
  clipboard->undo1_dbl=clipboard->fps=cfile->fps;
  clipboard->ratio_fps=cfile->ratio_fps;
  clipboard->is_loaded=TRUE;
  g_snprintf(clipboard->type,40,"Frames");

  clipboard->asampsize=clipboard->arate=clipboard->achans=0;
  clipboard->afilesize=0l;

  if (mainw->ccpd_with_sound) {
    clipboard->achans=cfile->achans;
    clipboard->asampsize=cfile->asampsize;

    clipboard->arate=cfile->arate;
    clipboard->arps=cfile->arps;
    clipboard->signed_endian=cfile->signed_endian;
    reget_afilesize (0);
  }

  clipboard->start=1;
  clipboard->end=clipboard->frames;

  get_total_time (clipboard);

  sensitize();
  d_print_done();

}


void
on_cut_activate                       (GtkMenuItem     *menuitem,
				       gpointer         user_data)
{
  int current_file=mainw->current_file;
  on_copy_activate(menuitem, user_data);
  if (mainw->cancelled) {
    return;
  }
  on_delete_activate(menuitem, user_data);
  if (mainw->current_file==current_file) {
    set_undoable (_("Cut"),TRUE);
    cfile->undo_action=UNDO_CUT;
  }
}


void on_paste_as_new_activate                       (GtkMenuItem     *menuitem,
						     gpointer         user_data)
{
  gchar *com;
  gchar *msg;
  int old_file=mainw->current_file,current_file;

  if (clipboard==NULL) return;

  mainw->current_file=mainw->first_free_file;
  
  if (!get_new_handle(mainw->current_file,NULL)) {
    mainw->current_file=old_file;
    return;
  }

  //set file details
  cfile->frames=clipboard->frames;
  cfile->hsize=clipboard->hsize;
  cfile->vsize=clipboard->vsize;
  cfile->pb_fps=cfile->fps=clipboard->fps;
  cfile->ratio_fps=clipboard->ratio_fps;
  cfile->progress_start=cfile->start=cfile->frames>0?1:0;
  cfile->progress_end=cfile->end=cfile->frames;
  cfile->changed=TRUE;
  cfile->is_loaded=TRUE;

  mainw->fx1_val=1;
  mainw->fx1_bool=FALSE;

  if (!check_if_non_virtual(0,1,clipboard->frames)) {
    int current_file=mainw->current_file;
    boolean retb;
    mainw->cancelled=CANCEL_NONE;
    mainw->current_file=0;
    cfile->progress_start=1;
    cfile->progress_end=count_virtual_frames(cfile->frame_index,1,cfile->frames);
    do_threaded_dialog(_("Pulling frames from clipboard"),TRUE);
    retb=virtual_to_images(mainw->current_file,1,cfile->frames,TRUE,NULL);
    end_threaded_dialog();
    mainw->current_file=current_file;

    if (mainw->cancelled!=CANCEL_NONE||!retb) {
      sensitize();
      mainw->cancelled=CANCEL_USER;
      return;
    }
  }

  mainw->no_switch_dprint=TRUE;
  msg=g_strdup_printf (_ ("Pasting %d frames to new clip %s..."),cfile->frames,cfile->name);
  d_print (msg);
  g_free (msg);
  mainw->no_switch_dprint=FALSE;

  com=g_strdup_printf("%s insert \"%s\" \"%s\" 0 1 %d \"%s\" %d 0 0 0 %.3f %d %d %d %d %d",
		      prefs->backend,cfile->handle, 
		      get_image_ext_for_type(cfile->img_type),clipboard->frames, clipboard->handle, 
		      mainw->ccpd_with_sound, clipboard->fps, clipboard->arate, clipboard->achans, 
		      clipboard->asampsize, !(cfile->signed_endian&AFORM_UNSIGNED), 
		      !(cfile->signed_endian&AFORM_BIG_ENDIAN));
  
  mainw->com_failed=FALSE;
  lives_system(com,FALSE);
  g_free(com);

  if (mainw->com_failed) {
    d_print_failed();
    return;
  }

  cfile->nopreview=TRUE;

  // show a progress dialog, not cancellable
  if (!do_progress_dialog(TRUE,TRUE,_ ("Pasting"))) {

    if (mainw->error) d_print_failed();

    close_current_file(old_file);
    return;
  }
  cfile->nopreview=FALSE;

  if (mainw->ccpd_with_sound) {
    cfile->arate=clipboard->arate;
    cfile->arps=clipboard->arps;
    cfile->achans=clipboard->achans;
    cfile->asampsize=clipboard->asampsize;
    cfile->afilesize=clipboard->afilesize;
    cfile->signed_endian=clipboard->signed_endian;
    if (cfile->afilesize>0) d_print(_("...added audio..."));
  }

  // add entry to window menu
  add_to_clipmenu();
  current_file=mainw->current_file;
  if (!save_clip_values(current_file)) {
    close_current_file(old_file);
    return;
  }

  if (prefs->crash_recovery) add_to_recovery_file(cfile->handle);
  switch_to_file((mainw->current_file=old_file),current_file);
  d_print_done();

  mainw->last_dprint_file=old_file;
  d_print(""); // force switchtext

#ifdef ENABLE_OSC
    lives_osc_notify(LIVES_OSC_NOTIFY_CLIP_OPENED,"");
#endif
}


void on_insert_pre_activate (GtkMenuItem *menuitem, gpointer user_data) {
  insertw = create_insert_dialog ();

  lives_widget_show (insertw->insert_dialog);
  mainw->fx1_bool=FALSE;
  mainw->fx1_val=1;

  mainw->fx2_bool=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(insertw->with_sound));
}



void on_insert_activate (GtkButton *button, gpointer user_data) {
  double times_to_insert=mainw->fx1_val;
  double audio_stretch;

  gchar *msg,*com;

  boolean with_sound=mainw->fx2_bool;
  boolean has_lmap_error=FALSE;
  boolean bad_header=FALSE;
  boolean insert_silence=FALSE;

  // have we resampled ?
  boolean cb_audio_change=FALSE;
  boolean cb_video_change=FALSE;

  boolean virtual_ins=FALSE;
  boolean all_virtual=FALSE;

  int where=cfile->start-1;
  int start=cfile->start;
  int end=cfile->end;

  int hsize=cfile->hsize;
  int vsize=cfile->vsize;

  int cfile_signed=0,cfile_endian=0,clipboard_signed=0,clipboard_endian=0;
  int current_file=mainw->current_file;

  int orig_frames=cfile->frames;
  int ocarps=clipboard->arps;
  int leave_backup=1;
  int remainder_frames;
  int cb_start=1,cb_end=clipboard->frames;

  // if it is an insert into the original file, and we can do fast seek, we can insert virtual frames
  if (button!=NULL&&mainw->current_file==clipboard->cb_src&&!check_if_non_virtual(0,1,clipboard->frames)) {
    lives_clip_data_t *cdata=((lives_decoder_t *)cfile->ext_src)->cdata;
    if (cdata->seek_flag&LIVES_SEEK_FAST) {
      virtual_ins=TRUE;
      if (count_virtual_frames(clipboard->frame_index,1,clipboard->frames)==clipboard->frames) all_virtual=TRUE;
    }
  }

  // don't ask smogrify to resize if frames are the same size and type
  if (all_virtual||(((cfile->hsize==clipboard->hsize && cfile->vsize==clipboard->vsize)||orig_frames==0)&&
		    (cfile->img_type==clipboard->img_type))) hsize=vsize=0;
  else {
    if (!capable->has_convert) {
      do_error_dialog(_ ("This operation requires resizing or converting of frames.\nPlease install 'convert' from the Image-magick package, and then restart LiVES.\n"));
      mainw->error=TRUE;
      if (button!=NULL) {
	lives_widget_destroy(insertw->insert_dialog);
	g_free(insertw);
      }
      return;
    }
  }

  // fit video to audio if requested
  if (mainw->fx1_bool&&(cfile->asampsize*cfile->arate*cfile->achans)) {
    get_total_time(cfile);
    times_to_insert=(cfile->laudio_time*cfile->fps-cfile->frames)/clipboard->frames;
  }

  if (times_to_insert<0.&&(mainw->fx1_bool)) {
    do_error_dialog(_ ("\n\nVideo is longer than audio.\nTry selecting all frames, and then using \nthe 'Trim Audio' function from the Audio menu."));
    mainw->error=TRUE;
    if (button!=NULL) {
      lives_widget_destroy(insertw->insert_dialog);
      g_free(insertw);
    }
    return;
  }


  if (with_sound) {
    cfile_signed=!(cfile->signed_endian&AFORM_UNSIGNED);
    cfile_endian=!(cfile->signed_endian&AFORM_BIG_ENDIAN);

    clipboard_signed=!(clipboard->signed_endian&AFORM_UNSIGNED);
    clipboard_endian=!(clipboard->signed_endian&AFORM_BIG_ENDIAN);

    if ((cfile->achans*cfile->arps*cfile->asampsize>0)&&(cfile->achans!=clipboard->achans||
							 (cfile->arps!=clipboard->arps&&clipboard->achans>0)||
							 cfile->asampsize!=clipboard->asampsize||
							 cfile_signed!=clipboard_signed||cfile_endian!=clipboard_endian||
							 cfile->arate!=clipboard->arate)) {
      if (!(capable->has_sox_sox)) {
	if (cfile->arps!=clipboard->arps) {
	  do_error_dialog(_ ("LiVES cannot insert because the audio rates do not match.\nPlease install 'sox', and try again."));
	  mainw->error=TRUE;
	  return;
	}
      }
    }
  }


  if ((!(prefs->warning_mask&WARN_MASK_LAYOUT_SHIFT_FRAMES)||(!prefs->warning_mask&&WARN_MASK_LAYOUT_ALTER_FRAMES))) {
    int insert_start;
    if (mainw->insert_after) {
      insert_start=cfile->end+1;
    }
    else {
      insert_start=cfile->start;
    }
    if ((mainw->xlays=layout_frame_is_affected(mainw->current_file,insert_start))!=NULL) {
      if (!do_warning_dialog
	  (_("\nInsertion will cause frames to shift in some multitrack layouts.\nAre you sure you wish to continue ?\n"))) {
	if (button!=NULL) {
	  lives_widget_destroy(insertw->insert_dialog);
	  g_free(insertw);
	}
	mainw->error=TRUE;
	g_list_free_strings(mainw->xlays);
	g_list_free(mainw->xlays);
	mainw->xlays=NULL;
	return;
      }
      add_lmap_error(LMAP_ERROR_SHIFT_FRAMES,cfile->name,(gpointer)cfile->layout_map,mainw->current_file,
		     insert_start,0.,count_resampled_frames(cfile->stored_layout_frame,cfile->stored_layout_fps,
							    cfile->fps)>=insert_start);
      has_lmap_error=TRUE;
      mainw->error=TRUE;
      g_list_free_strings(mainw->xlays);
      g_list_free(mainw->xlays);
      mainw->xlays=NULL;
    }
    else {
      if (!(prefs->warning_mask&WARN_MASK_LAYOUT_ALTER_FRAMES)&&(mainw->xlays=
								 layout_frame_is_affected(mainw->current_file,1))!=NULL) {
	if (!do_layout_alter_frames_warning()) {
	  g_list_free_strings(mainw->xlays);
	  g_list_free(mainw->xlays);
	  mainw->xlays=NULL;
	  return;
	}
	add_lmap_error(LMAP_ERROR_ALTER_FRAMES,cfile->name,(gpointer)cfile->layout_map,mainw->current_file,0,0.,
		       cfile->stored_layout_frame>0);
	g_list_free_strings(mainw->xlays);
	g_list_free(mainw->xlays);
	mainw->xlays=NULL;
	has_lmap_error=TRUE;
      }
    }
  }

  if (with_sound&&(!(prefs->warning_mask&WARN_MASK_LAYOUT_SHIFT_AUDIO)||
		   (!prefs->warning_mask&&WARN_MASK_LAYOUT_ALTER_AUDIO))) {
    int insert_start;
    if (mainw->insert_after) {
      insert_start=cfile->end+1;
    }
    else {
      insert_start=cfile->start;
    }
    if ((mainw->xlays=layout_audio_is_affected(mainw->current_file,(insert_start-1.)/cfile->fps))!=NULL) {
      if (!do_warning_dialog
	  (_("\nInsertion will cause audio to shift in some multitrack layouts.\nAre you sure you wish to continue ?\n"))) {
	if (button!=NULL) {
	  lives_widget_destroy(insertw->insert_dialog);
	  g_free(insertw);
	}
	mainw->error=TRUE;
	g_list_free_strings(mainw->xlays);
	g_list_free(mainw->xlays);
	mainw->xlays=NULL;
	return;
      }
      add_lmap_error(LMAP_ERROR_SHIFT_AUDIO,cfile->name,(gpointer)cfile->layout_map,mainw->current_file,0,
		     (insert_start-1.)/cfile->fps,(insert_start-1.)/cfile->fps<cfile->stored_layout_audio);
      has_lmap_error=TRUE;
    }
    else {
      if (!(prefs->warning_mask&WARN_MASK_LAYOUT_ALTER_AUDIO)&&
	  (mainw->xlays=layout_audio_is_affected(mainw->current_file,0.))!=NULL) {
	if (!do_layout_alter_audio_warning()) {
	  g_list_free_strings(mainw->xlays);
	  g_list_free(mainw->xlays);
	  mainw->xlays=NULL;
	  return;
	}
	add_lmap_error(LMAP_ERROR_ALTER_AUDIO,cfile->name,(gpointer)cfile->layout_map,mainw->current_file,0,0.,
		       cfile->stored_layout_audio>0.);
	has_lmap_error=TRUE;
	g_list_free_strings(mainw->xlays);
	g_list_free(mainw->xlays);
	mainw->xlays=NULL;
      }
    }
  }

  if (button!=NULL) {
    lives_widget_destroy(insertw->insert_dialog);
    lives_widget_context_update();
    g_free(insertw);
    if ((cfile->fps!=clipboard->fps&&orig_frames>0)||(cfile->arps!=clipboard->arps&&clipboard->achans>0&&with_sound)) {
      if (!do_clipboard_fps_warning()) {
	mainw->error=TRUE;
	return;
      }
    }
    if (prefs->ins_resample&&clipboard->fps!=cfile->fps&&orig_frames!=0) {
      cb_end=count_resampled_frames(clipboard->frames,clipboard->fps,cfile->fps);
    }
  }
  else {
    // called from on_merge_activate()
    cb_start=mainw->fx1_start;
    cb_end=mainw->fx2_start;

    // we will use leave_backup as this will leave our
    // merge backup in place
    leave_backup=-1;
   }

  if (mainw->insert_after) {
    cfile->insert_start=cfile->end+1;
  }
  else {
    cfile->insert_start=cfile->start;
  }
  
  cfile->insert_end=cfile->insert_start-1;

  if (mainw->insert_after) where=cfile->end;

  // at least we should try to convert the audio to match...
  // if with_sound is TRUE, and clipboard has no audio, we will insert silence (unless target 
  // also has no audio
  if (with_sound) {

    if (clipboard->achans==0) {
      if (cfile->achans>0) insert_silence=TRUE;
      with_sound=FALSE;
    }
    else {
      if ((cfile->achans*cfile->arps*cfile->asampsize>0)
	  &&clipboard->achans>0&&(cfile->achans!=clipboard->achans||
				  cfile->arps!=clipboard->arps||
				  cfile->asampsize!=clipboard->asampsize||
				  cfile_signed!=clipboard_signed||
				  cfile_endian!=clipboard_endian||cfile->arate!=clipboard->arate)) {

	cb_audio_change=TRUE;

	if (clipboard->arps!=clipboard->arps||cfile->arate!=clipboard->arate) {
	  // pb rate != real rate - stretch to pb rate and resample
	  if ((audio_stretch=(double)clipboard->arps/(double)clipboard->arate*
	       (double)cfile->arate/(double)cfile->arps)!=1.) {
	    unlink (clipboard->info_file);
	    com=g_strdup_printf ("%s resample_audio \"%s\" %d %d %d %d %d %d %d %d %d %d %.4f",
				 prefs->backend,
				 clipboard->handle,clipboard->arps,clipboard->achans,clipboard->asampsize,
				 clipboard_signed,clipboard_endian,cfile->arps,clipboard->achans,
				 clipboard->asampsize,clipboard_signed,clipboard_endian,audio_stretch);
	    mainw->com_failed=FALSE;
	    lives_system (com,FALSE);
	    g_free (com);

	    if (mainw->com_failed) {
	      return;
	    }

	    mainw->current_file=0;
	    mainw->error=FALSE;
	    do_progress_dialog (TRUE,FALSE,_ ("Resampling clipboard audio"));
	    mainw->current_file=current_file;
	    if (mainw->error) {
	      d_print_failed();
	      return;
	    }

	    // not really, but we pretend...
	    clipboard->arps=cfile->arps;
	  }
	}

	if (clipboard->achans>0&&(cfile->achans!=clipboard->achans||cfile->arps!=clipboard->arps||
				  cfile->asampsize!=clipboard->asampsize||cfile_signed!=clipboard_signed||
				  cfile_endian!=clipboard_endian)) {
	  unlink (clipboard->info_file);
	  com=g_strdup_printf ("%s resample_audio \"%s\" %d %d %d %d %d %d %d %d %d %d",
			       prefs->backend,clipboard->handle,
			       clipboard->arps,clipboard->achans,clipboard->asampsize,clipboard_signed,
			       clipboard_endian,cfile->arps,cfile->achans,cfile->asampsize,cfile_signed,cfile_endian);
	  mainw->com_failed=FALSE;
	  lives_system (com,FALSE);
	  g_free (com);

	  if (mainw->com_failed) {
	    return;
	  }

	  mainw->current_file=0;
	  do_progress_dialog (TRUE,FALSE,_ ("Resampling clipboard audio"));
	  mainw->current_file=current_file;

	  if (mainw->error) {
	    d_print_failed();
	    return;
	  }

	}
      
	if (clipboard->achans>0&&clipboard->afilesize==0l) {
	  if (prefs->conserve_space) {
	    // oops...
	    clipboard->achans=clipboard->arate=clipboard->asampsize=0;
	    with_sound=FALSE;
	    do_error_dialog 
	      (_ ("\n\nLiVES was unable to resample the clipboard audio. \nClipboard audio has been erased.\n"));
	  }
	  else {
	    unlink (clipboard->info_file);
	    mainw->current_file=0;
	    com=g_strdup_printf ("%s undo_audio \"%s\"",prefs->backend_sync,clipboard->handle);
	    lives_system (com,FALSE);
	    g_free (com);
	    mainw->current_file=current_file;
	  
	    clipboard->arps=ocarps;
	    reget_afilesize(0);
	  
	    if (!do_yesno_dialog 
		(_("\n\nLiVES was unable to resample the clipboard audio.\nDo you wish to continue with the insert \nusing unchanged audio ?\n"))) {
	      mainw->error=TRUE;
	      return;
	    }}}}}}

  
  if (!virtual_ins) {
    int current_file=mainw->current_file;
    boolean retb;
    mainw->cancelled=CANCEL_NONE;
    mainw->current_file=0;
    cfile->progress_start=1;
    cfile->progress_end=count_virtual_frames(cfile->frame_index,start,end);
    do_threaded_dialog(_("Pulling frames from clipboard"),TRUE);
    retb=virtual_to_images(mainw->current_file,start,end,TRUE,NULL);
    end_threaded_dialog();
    mainw->current_file=current_file;

    if (mainw->cancelled!=CANCEL_NONE||!retb) {
      sensitize();
      mainw->cancelled=CANCEL_USER;
      return;
    }
  }

  // if pref is set, resample clipboard video
  if (prefs->ins_resample&&cfile->fps!=clipboard->fps&&orig_frames>0) {
    cb_video_change=TRUE;
  }

  d_print(""); // force switchtext
  if (!resample_clipboard(cfile->fps)) return;

  if (mainw->fx1_bool&&(cfile->asampsize*cfile->arate*cfile->achans)) {
    times_to_insert=(times_to_insert*cfile->fps-cfile->frames)/clipboard->frames;
  }
  switch_to_file(0,current_file);

  if (cb_end>clipboard->frames) {
    cb_end=clipboard->frames;
  }

  if (with_sound&&cfile->achans==0) {
    int asigned=!(clipboard->signed_endian&AFORM_UNSIGNED);
    int endian=clipboard->signed_endian&AFORM_BIG_ENDIAN;

    cfile->achans=clipboard->achans;
    cfile->asampsize=clipboard->asampsize;
    cfile->arps=cfile->arate=clipboard->arate;
    cfile->signed_endian=clipboard->signed_endian;

    save_clip_value(mainw->current_file,CLIP_DETAILS_ARATE,&cfile->arps);
    if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
    save_clip_value(mainw->current_file,CLIP_DETAILS_PB_ARATE,&cfile->arate);
    if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
    save_clip_value(mainw->current_file,CLIP_DETAILS_ACHANS,&cfile->achans);
    if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
    save_clip_value(mainw->current_file,CLIP_DETAILS_ASIGNED,&asigned);
    if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
    save_clip_value(mainw->current_file,CLIP_DETAILS_AENDIAN,&endian);
    if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
    save_clip_value(mainw->current_file,CLIP_DETAILS_ASAMPS,&cfile->asampsize);
    if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;

    if (bad_header) do_header_write_error(mainw->current_file);
  }

  // first remainder frames
  remainder_frames=(int)(times_to_insert-(double)(int)times_to_insert)*clipboard->frames;

  end=clipboard->frames;
  if (virtual_ins) end=-end;

  if (!mainw->insert_after&&remainder_frames>0) {
    msg=g_strdup_printf(_ ("Inserting %d%s frames from the clipboard..."),remainder_frames,
			times_to_insert>1.?" remainder":"");
    d_print(msg);
    g_free(msg);

    com=g_strdup_printf("%s insert \"%s\" \"%s\" %d %d %d \"%s\" %d %d %d %d %.3f %d %d %d %d %d",
			prefs->backend,cfile->handle, 
			get_image_ext_for_type(cfile->img_type), where, clipboard->frames-remainder_frames+1, 
			end, clipboard->handle, with_sound, cfile->frames, hsize, vsize, cfile->fps, 
			cfile->arate, cfile->achans, cfile->asampsize, !(cfile->signed_endian&AFORM_UNSIGNED), 
			!(cfile->signed_endian&AFORM_BIG_ENDIAN));

    unlink(cfile->info_file);
    mainw->com_failed=FALSE;
    lives_system(com,FALSE);
    g_free(com);

    if (mainw->com_failed) {
      d_print_failed();
      return;
    }
    
    cfile->progress_start=1;
    cfile->progress_end=remainder_frames;

    do_progress_dialog(TRUE,FALSE,_ ("Inserting"));

    if (mainw->error) {
      d_print_failed();
      return;
    }

    if (cfile->clip_type==CLIP_TYPE_FILE||virtual_ins) {
      insert_images_in_virtual(mainw->current_file,where,remainder_frames,clipboard->frame_index,clipboard->frames-remainder_frames+1);
    }

    cfile->frames+=remainder_frames;
    where+=remainder_frames;

    cfile->insert_end+=remainder_frames;

    if (!mainw->insert_after) {
      cfile->start+=remainder_frames;
      cfile->end+=remainder_frames;
    }

    if (with_sound) {
      reget_afilesize (mainw->current_file);
    }
    get_play_times();
    d_print_done();
  } 
  

  // inserts of whole clipboard
  if ((int)times_to_insert>1) {
    msg=g_strdup_printf(_ ("Inserting %d times from the clipboard%s..."),(int)times_to_insert,with_sound?
			" (with sound)":"");
    d_print("");
    d_print(msg);
    g_free(msg);
  }
  else if ((int)times_to_insert>0) {
    msg=g_strdup_printf(_ ("Inserting %d frames from the clipboard%s..."),cb_end-cb_start+1,with_sound?
			" (with sound)":"");
    d_print("");
    d_print(msg);
    g_free(msg);
  }
  
  if (virtual_ins) cb_end=-cb_end;

  // for an insert after a merge we set our start posn. -ve
  // this should indicate to the back end to leave our
  // backup frames alone

  com=g_strdup_printf("%s insert \"%s\" \"%s\" %d %d %d \"%s\" %d %d %d %d %.3f %d %d %d %d %d %d",
		      prefs->backend,cfile->handle, 
		      get_image_ext_for_type(cfile->img_type), where, cb_start*leave_backup, cb_end, 
		      clipboard->handle, with_sound, cfile->frames, hsize, vsize, cfile->fps, cfile->arate, 
		      cfile->achans, cfile->asampsize, !(cfile->signed_endian&AFORM_UNSIGNED), 
		      !(cfile->signed_endian&AFORM_BIG_ENDIAN), (int)times_to_insert);

  if (virtual_ins) cb_end=-cb_end;

  cfile->progress_start=1;
  cfile->progress_end=(cb_end-cb_start+1)*(int)times_to_insert+cfile->frames-where;
  mainw->com_failed=FALSE;
  unlink(cfile->info_file);
  lives_system(com,FALSE);
  g_free (com);

  if (mainw->com_failed) {
    d_print_failed();
    return;
  }

  // show a progress dialog
  cfile->nopreview=TRUE;
  if (!do_progress_dialog(TRUE,TRUE,_ ("Inserting"))) {
    // cancelled

    cfile->nopreview=FALSE;

    if (mainw->error) {
      d_print_failed();
      return;
    }

    // clean up moved/inserted frames
    com=g_strdup_printf ("%s undo_insert \"%s\" %d %d %d \"%s\"",
			 prefs->backend,cfile->handle,where+1,
			 where+(cb_end-cb_start+1)*(int)times_to_insert,cfile->frames,
			 get_image_ext_for_type(cfile->img_type));
    lives_system (com,FALSE);
    g_free(com);

    cfile->start=start;
    cfile->end=end;

    if (with_sound) {
      // desample clipboard audio
      if (cb_audio_change&&!prefs->conserve_space) {
	unlink (clipboard->info_file);
	com=g_strdup_printf ("%s undo_audio \"%s\"",prefs->backend_sync,clipboard->handle);
	mainw->current_file=0;
	lives_system (com,FALSE);
	g_free (com);
	mainw->current_file=current_file;
	clipboard->arps=ocarps;
	reget_afilesize(0);
      }
    }
  
    if (cb_video_change) {
      // desample clipboard video
      mainw->current_file=0;
      mainw->no_switch_dprint=TRUE;
      on_undo_activate(NULL,NULL);
      mainw->no_switch_dprint=FALSE;
      mainw->current_file=current_file;
    }
    
    switch_to_file (0,current_file);
    set_undoable (NULL,FALSE);
    mainw->cancelled=CANCEL_USER;
    return;
  }

  mainw->cancelled=CANCEL_NONE;
  cfile->nopreview=FALSE;
  
  if (cfile->clip_type==CLIP_TYPE_FILE||virtual_ins) {
    insert_images_in_virtual(mainw->current_file,where,(cb_end-cb_start+1)*(int)times_to_insert,clipboard->frame_index,cb_start*leave_backup);
  }

  cfile->frames+=(cb_end-cb_start+1)*(int)times_to_insert;
  where+=(cb_end-cb_start+1)*(int)times_to_insert;
  cfile->insert_end+=(cb_end-cb_start+1)*(int)times_to_insert;

  if (!mainw->insert_after) {
    cfile->start+=(cb_end-cb_start+1)*(int)times_to_insert;
    cfile->end+=(cb_end-cb_start+1)*(int)times_to_insert;
  }

  if (with_sound==1) {
    reget_afilesize(mainw->current_file);
  }
  get_play_times();
  d_print_done();

  // last remainder frames

  if (mainw->insert_after&&remainder_frames>0) {
    msg=g_strdup_printf(_ ("Inserting %d%s frames from the clipboard..."),remainder_frames,
			times_to_insert>1.?" remainder":"");
    d_print(msg);
    g_free(msg);

    if (virtual_ins) remainder_frames=-remainder_frames;

    com=g_strdup_printf("%s insert \"%s\" \"%s\" %d %d %d \"%s\" %d %d %d %d %3f %d %d %d %d %d",
			prefs->backend,cfile->handle, 
			get_image_ext_for_type(cfile->img_type), where, 1, remainder_frames, clipboard->handle, 
			with_sound, cfile->frames, hsize, vsize, cfile->fps, cfile->arate, cfile->achans, 
			cfile->asampsize, !(cfile->signed_endian&AFORM_UNSIGNED), 
			!(cfile->signed_endian&AFORM_BIG_ENDIAN ));
    
    unlink(cfile->info_file);
    mainw->com_failed=FALSE;
    lives_system(com,FALSE);

    if (mainw->com_failed) {
      d_print_failed();
      return;
    }

    if (virtual_ins) remainder_frames=-remainder_frames;

    cfile->progress_start=1;
    cfile->progress_end=remainder_frames;

    do_progress_dialog(TRUE,FALSE,_ ("Inserting"));

    if (mainw->error) {
      d_print_failed();
      return;
    }

    if (cfile->clip_type==CLIP_TYPE_FILE||virtual_ins) {
      insert_images_in_virtual(mainw->current_file,where,remainder_frames,clipboard->frame_index,1);
    }

    cfile->frames+=remainder_frames;
    cfile->insert_end+=remainder_frames;
    g_free(com);

    if (!mainw->insert_after) {
      cfile->start+=remainder_frames;
      cfile->end+=remainder_frames;
    }
    get_play_times();

    d_print_done();
  } 
  

  // if we had deferred audio, we insert silence in selection
  if (insert_silence) {
    cfile->undo1_dbl=calc_time_from_frame(mainw->current_file,cfile->insert_start);
    cfile->undo2_dbl=calc_time_from_frame(mainw->current_file,cfile->insert_end+1);
    cfile->undo_arate=cfile->arate;
    cfile->undo_signed_endian=cfile->signed_endian;
    cfile->undo_achans=cfile->achans;
    cfile->undo_asampsize=cfile->asampsize;
    cfile->undo_arps=cfile->arps;

    on_ins_silence_activate(NULL,NULL);

    with_sound=TRUE;
  }


  // insert done

  // start or end can be zero if we inserted into pure audio
  if (cfile->start==0&&cfile->frames>0) cfile->start=1;
  if (cfile->end==0) cfile->end=cfile->frames;

  if (cfile->frames>0&&orig_frames==0) {
    g_snprintf (cfile->type,40,"Frames");
    cfile->orig_file_name=FALSE;
    cfile->hsize=clipboard->hsize;
    cfile->vsize=clipboard->vsize;
    cfile->bpp=clipboard->bpp;
    cfile->fps=cfile->pb_fps=clipboard->fps;
    save_clip_value(mainw->current_file,CLIP_DETAILS_WIDTH,&cfile->hsize);
    if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
    save_clip_value(mainw->current_file,CLIP_DETAILS_HEIGHT,&cfile->vsize);
    if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
    save_clip_value(mainw->current_file,CLIP_DETAILS_BPP,&cfile->bpp);
    if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
    save_clip_value(mainw->current_file,CLIP_DETAILS_FPS,&cfile->fps);
    if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
    save_clip_value(mainw->current_file,CLIP_DETAILS_PB_FPS,&cfile->fps);
    if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;

    if (bad_header) do_header_write_error(mainw->current_file);
  }

  g_signal_handler_block(mainw->spinbutton_end,mainw->spin_end_func);
  lives_spin_button_set_range(LIVES_SPIN_BUTTON(mainw->spinbutton_end),cfile->frames==0?0:1,cfile->frames);
  lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_end),cfile->end);
  g_signal_handler_unblock(mainw->spinbutton_end,mainw->spin_end_func);


  g_signal_handler_block(mainw->spinbutton_start,mainw->spin_start_func);
  lives_spin_button_set_range(LIVES_SPIN_BUTTON(mainw->spinbutton_start),cfile->frames==0?0:1,cfile->frames);
  lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_start),cfile->start);
  g_signal_handler_unblock(mainw->spinbutton_start,mainw->spin_start_func);

  set_undoable (_("Insert"),TRUE);
  cfile->undo1_boolean=with_sound;
  lives_widget_set_sensitive (mainw->select_new, TRUE);

  // mark new file size as 'Unknown'
  cfile->f_size=0l;
  cfile->changed=TRUE;

  if (with_sound) {
    cfile->undo_action=UNDO_INSERT_WITH_AUDIO;
    if (cb_audio_change&&!prefs->conserve_space&&clipboard->achans>0) {
      unlink (clipboard->info_file);
      mainw->current_file=0;
      com=g_strdup_printf ("%s undo_audio \"%s\"",prefs->backend_sync,clipboard->handle);
      lives_system (com,FALSE);
      g_free (com);
      mainw->current_file=current_file;
      clipboard->arps=ocarps;
      reget_afilesize(0);
    }
  }
  else cfile->undo_action=UNDO_INSERT;

  if (cb_video_change) {
    mainw->current_file=0;
    mainw->no_switch_dprint=TRUE;
    on_undo_activate(NULL,NULL);
    mainw->no_switch_dprint=FALSE;
    mainw->current_file=current_file;
  }

  save_clip_value(mainw->current_file,CLIP_DETAILS_FRAMES,&cfile->frames);
  if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;

  if (bad_header) do_header_write_error(mainw->current_file);

  switch_to_file(0,current_file);
  mainw->error=FALSE;

  if (has_lmap_error) popup_lmap_errors(NULL,NULL);

  if (mainw->sl_undo_mem!=NULL&&(cfile->stored_layout_frame!=0||(with_sound&&cfile->stored_layout_audio!=0.))) {
    // need to invalidate undo/redo stack, in case file was used in some layout undo
    stored_event_list_free_undos();
  }

}



void
on_delete_activate                    (GtkMenuItem     *menuitem,
				       gpointer         user_data)
{
  int frames_cut=cfile->end-cfile->start+1;
  int start=cfile->start;
  int end=cfile->end;
  gchar *com;
  boolean has_lmap_error=FALSE;
  boolean bad_header=FALSE;

  // occasionally we get a keyboard misread, so this should prevent that
  if (mainw->playing_file>-1) return;

  if (cfile->start<=1 && cfile->end==cfile->frames) {
    if (!mainw->osc_auto&&menuitem!=LIVES_MENU_ITEM(mainw->cut) && (cfile->achans==0||
								    ((double)frames_cut/cfile->fps>=cfile->laudio_time && 
								     mainw->ccpd_with_sound))) {
      if (do_warning_dialog
	  (_("\nDeleting all frames will close this file.\nAre you sure ?"))) close_current_file(0);
      return;
    }
  }
  
  if (menuitem!=NULL) {
    if (!(prefs->warning_mask&WARN_MASK_LAYOUT_DELETE_FRAMES)) {
      if ((mainw->xlays=layout_frame_is_affected(mainw->current_file,cfile->end-frames_cut))!=NULL) {
	if (!do_warning_dialog
	    (_("\nDeletion will cause missing frames in some multitrack layouts.\nAre you sure you wish to continue ?\n"))) {
	  g_list_free_strings(mainw->xlays);
	  g_list_free(mainw->xlays);
	  mainw->xlays=NULL;
	  return;
	}
	add_lmap_error(LMAP_ERROR_DELETE_FRAMES,cfile->name,(gpointer)cfile->layout_map,mainw->current_file,
		       cfile->end-frames_cut,0.,count_resampled_frames(cfile->stored_layout_frame,
								       cfile->stored_layout_fps,cfile->fps)>=
		       (cfile->end-frames_cut));
	has_lmap_error=TRUE;
	g_list_free_strings(mainw->xlays);
	g_list_free(mainw->xlays);
	mainw->xlays=NULL;
      }
    }
    
    if (mainw->ccpd_with_sound&&!(prefs->warning_mask&WARN_MASK_LAYOUT_DELETE_AUDIO)) {
      if ((mainw->xlays=layout_audio_is_affected(mainw->current_file,(cfile->end-frames_cut)/cfile->fps))!=NULL) {
	if (!do_warning_dialog
	    (_("\nDeletion will cause missing audio in some multitrack layouts.\nAre you sure you wish to continue ?\n"))) {
	  g_list_free_strings(mainw->xlays);
	  g_list_free(mainw->xlays);
	  mainw->xlays=NULL;
	  return;
	}
	add_lmap_error(LMAP_ERROR_DELETE_AUDIO,cfile->name,(gpointer)cfile->layout_map,mainw->current_file,0,
		       (cfile->end-frames_cut-1.)/cfile->fps,(cfile->end-frames_cut-1.)/
		       cfile->fps<cfile->stored_layout_audio);
	has_lmap_error=TRUE;
	g_list_free_strings(mainw->xlays);
	g_list_free(mainw->xlays);
	mainw->xlays=NULL;
      }
    }
    
    if (!has_lmap_error&&!(prefs->warning_mask&WARN_MASK_LAYOUT_SHIFT_FRAMES)) {
      if ((mainw->xlays=layout_frame_is_affected(mainw->current_file,cfile->start))!=NULL) {
	if (!do_warning_dialog
	    (_("\nDeletion will cause frames to shift in some multitrack layouts.\nAre you sure you wish to continue ?\n"))) {
	  g_list_free_strings(mainw->xlays);
	  g_list_free(mainw->xlays);
	  mainw->xlays=NULL;
	  return;
	}
	add_lmap_error(LMAP_ERROR_SHIFT_FRAMES,cfile->name,(gpointer)cfile->layout_map,mainw->current_file,
		       cfile->start,0.,cfile->start<=count_resampled_frames(cfile->stored_layout_frame,
									    cfile->stored_layout_fps,cfile->fps));
	has_lmap_error=TRUE;
	g_list_free_strings(mainw->xlays);
	g_list_free(mainw->xlays);
	mainw->xlays=NULL;
      }
    }
    
    if (mainw->ccpd_with_sound&&!has_lmap_error&&!(prefs->warning_mask&WARN_MASK_LAYOUT_SHIFT_AUDIO)) {
      if ((mainw->xlays=layout_audio_is_affected(mainw->current_file,(cfile->start-1.)/cfile->fps))!=NULL) {
	if (!do_warning_dialog
	    (_("\nDeletion will cause audio to shift in some multitrack layouts.\nAre you sure you wish to continue ?\n"))) {
	  g_list_free_strings(mainw->xlays);
	  g_list_free(mainw->xlays);
	  mainw->xlays=NULL;
	  return;
	}
	add_lmap_error(LMAP_ERROR_SHIFT_AUDIO,cfile->name,(gpointer)cfile->layout_map,mainw->current_file,0,
		       (cfile->start-1.)/cfile->fps,(cfile->start-1.)/cfile->fps<=cfile->stored_layout_audio);
	has_lmap_error=TRUE;
	g_list_free_strings(mainw->xlays);
	g_list_free(mainw->xlays);
	mainw->xlays=NULL;
      }
    }
    
    if (!has_lmap_error&&!(prefs->warning_mask&WARN_MASK_LAYOUT_ALTER_FRAMES)&&
	(mainw->xlays=layout_frame_is_affected(mainw->current_file,1))!=NULL) {
      if (!do_layout_alter_frames_warning()) {
	g_list_free_strings(mainw->xlays);
	g_list_free(mainw->xlays);
	mainw->xlays=NULL;
	return;
      }
      add_lmap_error(LMAP_ERROR_ALTER_FRAMES,cfile->name,(gpointer)cfile->layout_map,mainw->current_file,0,0.,
		     cfile->stored_layout_frame>0);
      has_lmap_error=TRUE;
      g_list_free_strings(mainw->xlays);
      g_list_free(mainw->xlays);
      mainw->xlays=NULL;
    }

    if (mainw->ccpd_with_sound&&!has_lmap_error&&!(prefs->warning_mask&WARN_MASK_LAYOUT_ALTER_AUDIO)&&
	(mainw->xlays=layout_audio_is_affected(mainw->current_file,0.))!=NULL) {
      if (!do_layout_alter_audio_warning()) {
	g_list_free_strings(mainw->xlays);
	g_list_free(mainw->xlays);
	mainw->xlays=NULL;
	return;
      }
      add_lmap_error(LMAP_ERROR_ALTER_AUDIO,cfile->name,(gpointer)cfile->layout_map,mainw->current_file,0,0.,
		     cfile->stored_layout_audio>0.);
      has_lmap_error=TRUE;
      g_list_free_strings(mainw->xlays);
      g_list_free(mainw->xlays);
      mainw->xlays=NULL;
    }
  }

  if (cfile->start<=1&&cfile->end==cfile->frames) {
    cfile->ohsize=cfile->hsize;
    cfile->ovsize=cfile->vsize;
  }

  cfile->undo_start=cfile->start;
  cfile->undo_end=cfile->end;
  cfile->undo1_boolean=mainw->ccpd_with_sound;

  if (menuitem!=NULL||mainw->osc_auto) {
    com=g_strdup_printf(_ ("Deleting frames %d to %d%s..."),cfile->start,cfile->end,
			mainw->ccpd_with_sound&&cfile->achans>0?" (with sound)":"");
    d_print(""); // force switchtext
    d_print(com);
    g_free(com);
  }

  com=g_strdup_printf("%s cut \"%s\" %d %d %d %d \"%s\" %.3f %d %d %d",
		      prefs->backend,cfile->handle,cfile->start,cfile->end, 
		      mainw->ccpd_with_sound, cfile->frames, get_image_ext_for_type(cfile->img_type), 
		      cfile->fps, cfile->arate, cfile->achans, cfile->asampsize);
  unlink(cfile->info_file);
  mainw->com_failed=FALSE;
  lives_system(com,FALSE);
  g_free(com);

  if (mainw->com_failed) {
    d_print_failed();
    return;
  }

  cfile->progress_start=cfile->start;
  cfile->progress_end=cfile->frames;

  // show a progress dialog, not cancellable
  do_progress_dialog(TRUE,FALSE,_ ("Deleting"));

  if (cfile->clip_type==CLIP_TYPE_FILE) {
    delete_frames_from_virtual (mainw->current_file, cfile->start, cfile->end);
  }

  cfile->frames-=frames_cut;

  cfile->undo_arate=cfile->arate;
  cfile->undo_signed_endian=cfile->signed_endian;
  cfile->undo_achans=cfile->achans;
  cfile->undo_asampsize=cfile->asampsize;
  cfile->undo_arps=cfile->arps;

  if (mainw->ccpd_with_sound) {
    reget_afilesize(mainw->current_file);
  }

  if (cfile->frames==0) {
    if (cfile->afilesize==0l) {
      close_current_file(0);
      return;
    }
    g_snprintf (cfile->type,40,"Audio");
    cfile->hsize=cfile->vsize=0;
    save_clip_value(mainw->current_file,CLIP_DETAILS_WIDTH,&cfile->hsize);
    if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
    save_clip_value(mainw->current_file,CLIP_DETAILS_HEIGHT,&cfile->vsize);
    if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;

    if (bad_header) do_header_write_error(mainw->current_file);
    cfile->orig_file_name=FALSE;
    desensitize();
    sensitize();
  }

  if (!mainw->selwidth_locked||cfile->start>cfile->frames) {
    if (--start==0&&cfile->frames>0) {
      start=1;
    }
  }

  cfile->start=start;

  if (!mainw->selwidth_locked) {
    cfile->end=start;
  }
  else {
    cfile->end=end;
    if (cfile->end>cfile->frames) {
      cfile->end=cfile->frames;
    }
  }

  g_signal_handler_block(mainw->spinbutton_end,mainw->spin_end_func);
  lives_spin_button_set_range(LIVES_SPIN_BUTTON(mainw->spinbutton_end),cfile->frames==0?0:1,cfile->frames);
  lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_end),cfile->end);
  g_signal_handler_unblock(mainw->spinbutton_end,mainw->spin_end_func);

  g_signal_handler_block(mainw->spinbutton_start,mainw->spin_start_func);
  lives_spin_button_set_range(LIVES_SPIN_BUTTON(mainw->spinbutton_start),cfile->frames==0?0:1,cfile->frames);
  lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_start),cfile->start);
  g_signal_handler_unblock(mainw->spinbutton_start,mainw->spin_start_func);

  // menuitem is NULL if we came here from undo_insert
  if (menuitem==NULL&&!mainw->osc_auto) return;

  save_clip_value(mainw->current_file,CLIP_DETAILS_FRAMES,&cfile->frames);
  if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;

  load_start_image(cfile->start);
  load_end_image(cfile->end);
  
  get_play_times();

  if (bad_header) do_header_write_error(mainw->current_file);

  // mark new file size as 'Unknown'
  cfile->f_size=0l;
  cfile->changed=TRUE;

  set_undoable (_("Delete"),TRUE);
  cfile->undo_action=UNDO_DELETE;
  d_print_done();

  if (has_lmap_error) popup_lmap_errors(NULL,NULL);

  if (mainw->sl_undo_mem!=NULL&&(cfile->stored_layout_frame!=0||(mainw->ccpd_with_sound&&cfile->stored_layout_audio!=0.))) {
    // need to invalidate undo/redo stack, in case file was used in some layout undo
    stored_event_list_free_undos();
  }
}



void
on_select_all_activate                (GtkMenuItem     *menuitem,
				       gpointer         user_data)
{
  if (mainw->current_file==-1) return;

  if (mainw->selwidth_locked) {
    if (menuitem!=NULL) do_error_dialog(_ ("\n\nSelection is locked.\n"));
    return;
  }

  lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_start),1);
  lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_end),cfile->frames);

  cfile->start=cfile->frames>0?1:0;
  cfile->end=cfile->frames;

  get_play_times();

  load_start_image(cfile->start);
  load_end_image(cfile->end);
}


void
on_select_start_only_activate                (GtkMenuItem     *menuitem,
					      gpointer         user_data)
{
  if (mainw->current_file==-1) return;
  lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_end),cfile->start);
}

void
on_select_end_only_activate                (GtkMenuItem     *menuitem,
					    gpointer         user_data)
{
  if (mainw->current_file==-1) return;
  lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_start),cfile->end);
}



void
on_select_invert_activate                (GtkMenuItem     *menuitem,
					  gpointer         user_data)
{
  if (cfile->start==1) {
    lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_start),cfile->end+1);
    lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_end),cfile->frames);
  }
  else {
    lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_end),cfile->start-1);
    lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_start),1);
  }

  get_play_times();

  load_start_image(cfile->start);
  load_end_image(cfile->end);
}

void
on_select_last_activate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  if (cfile->undo_start > cfile->frames) cfile->undo_start=cfile->frames;
  if (cfile->undo_end > cfile->frames) cfile->undo_end=cfile->frames;

  lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_start),cfile->undo_start);
  lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_end),cfile->undo_end);

  cfile->start=cfile->undo_start;
  cfile->end=cfile->undo_end;

  get_play_times();

  load_start_image(cfile->start);
  load_end_image(cfile->end);
}


void
on_select_new_activate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  if (cfile->insert_start > cfile->frames) cfile->insert_start=cfile->frames;
  if (cfile->insert_end > cfile->frames) cfile->insert_end=cfile->frames;

  lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_start),cfile->insert_start);
  lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_end),cfile->insert_end);

  cfile->start=cfile->insert_start;
  cfile->end=cfile->insert_end;

  get_play_times();

  load_start_image(cfile->start);
  load_end_image(cfile->end);
}

void
on_select_to_end_activate                (GtkMenuItem     *menuitem,
					  gpointer         user_data)
{
  lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_end),cfile->frames);
  cfile->end=cfile->frames;
  get_play_times();
  load_end_image(cfile->end);
}

void
on_select_from_start_activate                (GtkMenuItem     *menuitem,
					      gpointer         user_data)
{
  lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_start),1);
  cfile->start=cfile->frames>0?1:0;
  get_play_times();
  load_start_image(cfile->start);
}

void
on_lock_selwidth_activate                (GtkMenuItem     *menuitem,
					  gpointer         user_data)
{
  mainw->selwidth_locked=!mainw->selwidth_locked;
  lives_widget_set_sensitive(mainw->select_submenu,!mainw->selwidth_locked);
}


void
on_menubar_activate_menuitem                    (GtkMenuItem     *menuitem,
						 gpointer         user_data) {
  gtk_menu_item_activate(menuitem);
  //gtk_menu_shell_set_take_focus(GTK_MENU_SHELL(mainw->menubar),TRUE);
}


void on_playall_activate (GtkMenuItem *menuitem, gpointer user_data) {
  if (mainw->current_file<=0) return;

  if (mainw->multitrack!=NULL) {
    if (mainw->playing_file==-1) {
      if (!mainw->multitrack->playing_sel) multitrack_playall(mainw->multitrack);
      else multitrack_play_sel(NULL,mainw->multitrack);
    }
    else on_pause_clicked();
    return;
  }

  if (mainw->playing_file==-1) {
    if (cfile->proc_ptr!=NULL&&menuitem!=NULL) {
      on_preview_clicked (LIVES_BUTTON (cfile->proc_ptr->preview_button),NULL);
      return;
    }

    if (!mainw->osc_auto) {
      mainw->play_start=calc_frame_from_time(mainw->current_file,
					     cfile->pointer_time);
      mainw->play_end=cfile->frames;
    }

    mainw->playing_sel=FALSE;
    unlink(cfile->info_file);

    play_file();
    lives_ruler_set_value(LIVES_RULER (mainw->hruler),cfile->pointer_time);
    get_play_times();
    mainw->noswitch=FALSE;
  }

}


void on_playsel_activate (GtkMenuItem *menuitem, gpointer user_data) {
  // play part of a clip (in clip editor)

  if (mainw->current_file<=0) return;

  if (cfile->proc_ptr!=NULL&&menuitem!=NULL) {
    on_preview_clicked (LIVES_BUTTON (cfile->proc_ptr->preview_button),NULL);
    return;
  }
  
  if (!mainw->is_rendering) {
    mainw->play_start=cfile->start;
    mainw->play_end=cfile->end;
  }

  if (!mainw->preview) {
    int orig_play_frame=calc_frame_from_time(mainw->current_file,cfile->pointer_time);
    if (orig_play_frame>mainw->play_start&&orig_play_frame<mainw->play_end) {
      mainw->play_start=orig_play_frame;
    }
  }

  mainw->playing_sel=TRUE;
  unlink(cfile->info_file);

  play_file();

  mainw->playing_sel=FALSE;
  lives_ruler_set_value(LIVES_RULER (mainw->hruler),cfile->pointer_time);
  lives_widget_queue_draw (mainw->hruler);

  // in case we are rendering and previewing, in case we now have audio
  if (mainw->preview&&mainw->is_rendering&&mainw->is_processing) reget_afilesize(mainw->current_file);

  get_play_times();
  mainw->noswitch=FALSE;
}


void on_playclip_activate (GtkMenuItem *menuitem, gpointer user_data) {
  // play the clipboard
  int current_file=mainw->current_file;
  boolean oloop=mainw->loop;
  boolean oloop_cont=mainw->loop_cont;

  // switch to the clipboard
  switch_to_file (current_file,0);
  lives_widget_set_sensitive (mainw->loop_video, FALSE);
  lives_widget_set_sensitive (mainw->loop_continue, FALSE);
  mainw->loop=mainw->loop_cont=FALSE;

  mainw->play_start=1;
  mainw->play_end=clipboard->frames;
  mainw->playing_sel=FALSE;
  mainw->loop=FALSE;

  unlink(cfile->info_file);
  play_file();
  mainw->loop=oloop;
  mainw->loop_cont=oloop_cont;

  if (current_file>-1) {
    switch_to_file(0,current_file);
  }
  else {
    mainw->current_file=current_file;
    close_current_file(0);
  }
  mainw->noswitch=FALSE;
}




void on_record_perf_activate (GtkMenuItem *menuitem, gpointer user_data) {
  // real time recording

  if (mainw->multitrack!=NULL) return;

  if (mainw->playing_file>-1) {
    // we are playing a clip
    weed_timecode_t tc;

    if (!mainw->record||mainw->record_paused) {
      // recording is starting
      mainw->record_starting=TRUE;

      toggle_record();

      if ((prefs->rec_opts&REC_AUDIO)&&(mainw->agen_key!=0||mainw->agen_needs_reinit||prefs->audio_src==AUDIO_SRC_EXT)&&
	  ((prefs->audio_player==AUD_PLAYER_JACK) ||
	   (prefs->audio_player==AUD_PLAYER_PULSE))) {

	if (mainw->ascrap_file==-1) open_ascrap_file();
	if (mainw->ascrap_file!=-1) {
	  mainw->rec_samples=-1; // record unlimited
	  
	  mainw->rec_aclip=mainw->ascrap_file;
	  mainw->rec_avel=1.;
	  
#ifdef ENABLE_JACK
	if (prefs->audio_player==AUD_PLAYER_JACK) {
	  if (mainw->agen_key==0&&!mainw->agen_needs_reinit) {

	    // TODO - check
	    jack_rec_audio_to_clip(mainw->ascrap_file, -1, RECA_EXTERNAL);


	  }
	  else {
	    if (mainw->jackd!=NULL) jack_rec_audio_to_clip(mainw->ascrap_file, -1, RECA_GENERATED);
	  }

	}

#endif
#ifdef HAVE_PULSE_AUDIO
	if (prefs->audio_player==AUD_PLAYER_PULSE) {
	  if (mainw->agen_key==0&&!mainw->agen_needs_reinit) {
	    if (mainw->pulsed_read!=NULL) pulse_rec_audio_to_clip(mainw->ascrap_file, -1, RECA_EXTERNAL);
	  }
	  else {
	    if (mainw->pulsed!=NULL) pulse_rec_audio_to_clip(mainw->ascrap_file, -1, RECA_GENERATED);
	  }
	}
#endif
	}
	return;
      }


      if (prefs->rec_opts&REC_AUDIO) {
	// recording INTERNAL audio
#ifdef ENABLE_JACK
	if (prefs->audio_player==AUD_PLAYER_JACK&&mainw->jackd!=NULL) {
	  jack_get_rec_avals(mainw->jackd);
	}
#endif
#ifdef HAVE_PULSE_AUDIO
	if (prefs->audio_player==AUD_PLAYER_PULSE&&mainw->pulsed!=NULL) {
	  pulse_get_rec_avals(mainw->pulsed);
	}
#endif	
      }
      return;
    }

    // end record during playback
      
    if (mainw->event_list!=NULL) {
      // switch audio off at previous frame event
      pthread_mutex_lock(&mainw->event_list_mutex);

#ifdef RT_AUDIO
      if ((prefs->audio_player==AUD_PLAYER_JACK||prefs->audio_player==AUD_PLAYER_PULSE)&&(prefs->rec_opts&REC_AUDIO)) {
	weed_plant_t *last_frame=get_last_frame_event(mainw->event_list);
	insert_audio_event_at(mainw->event_list, last_frame, -1, mainw->rec_aclip, 0., 0.);
      }
#endif

      if (prefs->rec_opts&REC_EFFECTS) {
	// add deinit events for all active effects
	add_filter_deinit_events(mainw->event_list);
      }

      // write a RECORD_END marker
      tc=get_event_timecode(get_last_event(mainw->event_list));
      mainw->event_list=append_marker_event(mainw->event_list, tc, EVENT_MARKER_RECORD_END); // mark record end
      pthread_mutex_unlock(&mainw->event_list_mutex);
    }

    mainw->record_paused=TRUE; // pause recording of further events

    enable_record();

    return;
  }


  // out of playback
 
  // record performance
  if (!mainw->record) {

    // TODO - change message depending on rec_opts
    d_print(_ ("Ready to record. Use 'control' and cursor keys during playback to record your performance.\n(To cancel, press 'r' or click on Play|Record Performance again before you play.)\n"));
    mainw->record=TRUE;
    toggle_record();
    get_play_times();
  }
  else {
    d_print (_("Record cancelled.\n"));
    enable_record();
    mainw->record=FALSE;
  }
}



boolean record_toggle_callback (GtkAccelGroup *group, GObject *obj, guint keyval, GdkModifierType mod, 
				 gpointer user_data) {
  // from osc
  boolean start=(boolean)GPOINTER_TO_INT(user_data);

  if ((start&&(!mainw->record||mainw->record_paused))||(!start&&(mainw->record&&!mainw->record_paused))) 
    on_record_perf_activate(NULL,NULL);

  return TRUE;
}






void
on_rewind_activate                    (GtkMenuItem     *menuitem,
				       gpointer         user_data)
{
  if (mainw->multitrack!=NULL) {
    mt_tl_move(mainw->multitrack,-lives_ruler_get_value(LIVES_RULER (mainw->multitrack->timeline)));
    return;
  }

  cfile->pointer_time=lives_ruler_set_value(LIVES_RULER (mainw->hruler),0.);
  lives_widget_queue_draw (mainw->hruler);
  lives_widget_set_sensitive (mainw->rewind, FALSE);
  lives_widget_set_sensitive (mainw->m_rewindbutton, FALSE);
  lives_widget_set_sensitive (mainw->trim_to_pstart, FALSE);
  get_play_times();
}


void on_stop_activate (GtkMenuItem *menuitem, gpointer user_data) {
  if (mainw->multitrack!=NULL&&mainw->multitrack->is_paused&&mainw->playing_file==-1) {
    mainw->multitrack->is_paused=FALSE;
    mainw->multitrack->playing_sel=FALSE;
    mt_tl_move(mainw->multitrack,mainw->multitrack->ptr_time-lives_ruler_get_value(LIVES_RULER (mainw->multitrack->timeline)));
    lives_widget_set_sensitive (mainw->stop, FALSE);
    lives_widget_set_sensitive (mainw->m_stopbutton, FALSE);
    return;
  }
  mainw->cancelled=CANCEL_USER;
  if (mainw->jack_can_stop) mainw->jack_can_start=FALSE;
  mainw->jack_can_stop=FALSE;

}



boolean on_stop_activate_by_del (GtkWidget *widget, GdkEvent *event, gpointer user_data) {
  // called if the user closes the separate play window
  if (mainw->playing_file>-1) {
    mainw->cancelled=CANCEL_USER;
    if (mainw->jack_can_stop) mainw->jack_can_start=FALSE;
    mainw->jack_can_stop=FALSE;
  }
  if (prefs->sepwin_type==SEPWIN_TYPE_STICKY) {
    on_sepwin_pressed(NULL,NULL);
  }
  return TRUE;
}



void on_pause_clicked(void) {

  mainw->jack_can_stop=FALSE;
  mainw->cancelled=CANCEL_USER_PAUSED;
}


void 
on_encoder_entry_changed (GtkComboBox *combo, gpointer ptr) {
  GList *encoder_capabilities=NULL;
  GList *ofmt_all=NULL;
  GList *ofmt=NULL;

  gchar *new_encoder_name = lives_combo_get_active_text(combo);
  gchar *msg;
  gchar **array;
  int i;
  render_details *rdet=(render_details *)ptr;
  GList *dummy_list;

  if (!strlen (new_encoder_name)) {
    g_free(new_encoder_name);
    return;
  }

  if (!strcmp(new_encoder_name,mainw->string_constants[LIVES_STRING_CONSTANT_ANY])) { 
    GList *ofmt = NULL;
    ofmt = g_list_append(ofmt,g_strdup(mainw->string_constants[LIVES_STRING_CONSTANT_ANY]));

    g_signal_handler_block(rdet->encoder_combo, rdet->encoder_name_fn);
    // ---
    lives_combo_set_active_string(LIVES_COMBO(rdet->encoder_combo), mainw->string_constants[LIVES_STRING_CONSTANT_ANY]);
    // ---
    g_signal_handler_unblock(rdet->encoder_combo, rdet->encoder_name_fn);

    lives_combo_populate(LIVES_COMBO(rdet->ofmt_combo), ofmt);
    g_signal_handler_block (rdet->ofmt_combo, rdet->encoder_ofmt_fn);
    lives_combo_set_active_string(LIVES_COMBO(rdet->ofmt_combo),mainw->string_constants[LIVES_STRING_CONSTANT_ANY]);
    g_signal_handler_unblock (rdet->ofmt_combo, rdet->encoder_ofmt_fn);

    g_list_free(ofmt);
    if (prefs->acodec_list!=NULL) {
      g_list_free_strings (prefs->acodec_list);
      g_list_free (prefs->acodec_list);
      prefs->acodec_list=NULL;
    }
    prefs->acodec_list = g_list_append(prefs->acodec_list, g_strdup(mainw->string_constants[LIVES_STRING_CONSTANT_ANY]));

    lives_combo_populate(LIVES_COMBO(rdet->acodec_combo), prefs->acodec_list);

    lives_combo_set_active_string(LIVES_COMBO(rdet->acodec_combo),mainw->string_constants[LIVES_STRING_CONSTANT_ANY]);

    g_free(new_encoder_name);
  
    rdet->enc_changed=FALSE;

    return;
  }

  // finalise old plugin
  plugin_request(PLUGIN_ENCODERS,prefs->encoder.name,"finalise");
  
  clear_mainw_msg();
  // initialise new plugin
  if ((dummy_list = plugin_request(PLUGIN_ENCODERS, new_encoder_name, "init")) == NULL) {
    if (strlen (mainw->msg)) {
      msg = g_strdup_printf (_ ("\n\nThe '%s' plugin reports:\n%s\n"), new_encoder_name, mainw->msg);
    }
    else {
      msg = g_strdup_printf 
	(_("\n\nUnable to find the 'init' method in the %s plugin.\nThe plugin may be broken or not installed correctly."),
	 new_encoder_name);
    }
    g_free(new_encoder_name);

    if (mainw->is_ready){
      LiVESWindow *twindow=LIVES_WINDOW(mainw->LiVES);
      if (prefsw!=NULL) twindow=LIVES_WINDOW(prefsw->prefs_dialog);
      else if (mainw->multitrack!=NULL) twindow=LIVES_WINDOW(mainw->multitrack->window);
      if (!prefs->show_gui) twindow=(LiVESWindow *)NULL;
      do_error_dialog_with_check_transient(msg,TRUE,0,twindow);
    }

    g_free (msg);

    if (prefsw != NULL) {
      g_signal_handler_block(prefsw->encoder_combo, prefsw->encoder_name_fn);
      // ---
      lives_combo_set_active_string(LIVES_COMBO(prefsw->encoder_combo), prefs->encoder.name);
      // ---
      g_signal_handler_unblock(prefsw->encoder_combo, prefsw->encoder_name_fn);
    }

    if (rdet != NULL) {
      g_signal_handler_block(rdet->encoder_combo, rdet->encoder_name_fn);
      // ---
      lives_combo_set_active_string(LIVES_COMBO(rdet->encoder_combo), rdet->encoder_name);
      // ---
      g_signal_handler_unblock(rdet->encoder_combo, rdet->encoder_name_fn);
    }

    dummy_list = plugin_request(PLUGIN_ENCODERS, prefs->encoder.name, "init");
    if (dummy_list != NULL) {
      g_list_free_strings(dummy_list);
      g_list_free(dummy_list);
    }
    return;
  }
  g_list_free_strings(dummy_list);
  g_list_free(dummy_list);

  g_snprintf(future_prefs->encoder.name,51,"%s",new_encoder_name);
  g_free(new_encoder_name);

  if ((encoder_capabilities=plugin_request(PLUGIN_ENCODERS,future_prefs->encoder.name,"get_capabilities"))==NULL) {
 
    do_plugin_encoder_error(future_prefs->encoder.name);

    if (prefsw!=NULL) {
      g_signal_handler_block(prefsw->encoder_combo, prefsw->encoder_name_fn);
      // ---
      lives_combo_set_active_string(LIVES_COMBO(prefsw->encoder_combo), prefs->encoder.name);
      // ---
      g_signal_handler_unblock(prefsw->encoder_combo, prefsw->encoder_name_fn);
    }

    if (rdet!=NULL) {
      g_signal_handler_block (rdet->encoder_combo, rdet->encoder_name_fn);
      // ---
      lives_combo_set_active_string(LIVES_COMBO(rdet->encoder_combo), rdet->encoder_name);
      // ---
      g_signal_handler_unblock (rdet->encoder_combo, rdet->encoder_name_fn);
    }

    plugin_request(PLUGIN_ENCODERS, prefs->encoder.name, "init");
    g_snprintf(future_prefs->encoder.name,51,"%s",prefs->encoder.name);
    return;
  }
  prefs->encoder.capabilities = atoi((gchar *)g_list_nth_data(encoder_capabilities,0));
  g_list_free_strings (encoder_capabilities);
  g_list_free (encoder_capabilities);

  // fill list with new formats
  if ((ofmt_all = plugin_request_by_line(PLUGIN_ENCODERS,future_prefs->encoder.name,"get_formats"))!=NULL) {
    for (i=0;i<g_list_length(ofmt_all);i++) {
      if (get_token_count ((gchar *)g_list_nth_data (ofmt_all,i),'|')>2) {
	array=g_strsplit ((gchar *)g_list_nth_data (ofmt_all,i),"|",-1);
	ofmt=g_list_append(ofmt,g_strdup (array[1]));
	g_strfreev (array);
      }
    }

    if (prefsw!=NULL) {
      // we have to block here, otherwise on_ofmt_changed gets called for every added entry !
      g_signal_handler_block(prefsw->ofmt_combo, prefsw->encoder_ofmt_fn);

      lives_combo_populate(LIVES_COMBO(prefsw->ofmt_combo), ofmt);

      g_signal_handler_unblock(prefsw->ofmt_combo, prefsw->encoder_ofmt_fn);
    }

    if (rdet!=NULL) {
      // we have to block here, otherwise on_ofmt_changed gets called for every added entry !
      g_signal_handler_block (rdet->ofmt_combo, rdet->encoder_ofmt_fn);

      lives_combo_populate(LIVES_COMBO(rdet->ofmt_combo), ofmt);

      g_signal_handler_unblock(rdet->ofmt_combo, rdet->encoder_ofmt_fn);
    }
    
    g_list_free(ofmt);

    // set default (first) output type
    array=g_strsplit ((gchar *)g_list_nth_data (ofmt_all,0),"|",-1);

    if (rdet!=NULL) {
      lives_combo_set_active_string(LIVES_COMBO(rdet->ofmt_combo), array[1]);

      if (prefsw==NULL&&strcmp(prefs->encoder.name,future_prefs->encoder.name)) {
	g_snprintf(prefs->encoder.name,51,"%s",future_prefs->encoder.name);
	set_pref("encoder",prefs->encoder.name);
	g_snprintf(prefs->encoder.of_restrict,1024,"%s",future_prefs->encoder.of_restrict);
	prefs->encoder.of_allowed_acodecs=future_prefs->encoder.of_allowed_acodecs;
      }
      rdet->enc_changed=TRUE;
      rdet->encoder_name=g_strdup(prefs->encoder.name);
      lives_widget_set_sensitive(rdet->okbutton,TRUE);
    }

    if (prefsw!=NULL) {
      lives_combo_set_active_string(LIVES_COMBO(prefsw->ofmt_combo), array[1]);
    }
    on_encoder_ofmt_changed (NULL, rdet);
    g_strfreev (array);
    if (ofmt_all!=NULL) {
      g_list_free_strings(ofmt_all);
      g_list_free(ofmt_all);
    }
  }
}


void
on_insertwsound_toggled                (GtkToggleButton *togglebutton,
                                        gpointer         user_data)
{
  if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(togglebutton))) {
    lives_widget_set_sensitive (insertw->fit_checkbutton,FALSE);
  }
  else {
    lives_widget_set_sensitive (insertw->fit_checkbutton,cfile->achans>0);
  }
  mainw->fx2_bool=!mainw->fx2_bool;
}


void 
on_insfitaudio_toggled                (GtkToggleButton *togglebutton,
                                        gpointer         user_data)
{
  mainw->fx1_bool=!mainw->fx1_bool;

  if (lives_toggle_button_get_active(togglebutton)) {
    lives_widget_set_sensitive(insertw->with_sound,FALSE);
    lives_widget_set_sensitive(insertw->without_sound,FALSE);
    lives_widget_set_sensitive(insertw->spinbutton_times,FALSE);
  }
  else {
    lives_widget_set_sensitive(insertw->with_sound,clipboard->achans>0);
    lives_widget_set_sensitive(insertw->without_sound,clipboard->achans>0);
    lives_widget_set_sensitive(insertw->spinbutton_times,TRUE);
  }
}




boolean dirchange_callback (GtkAccelGroup *group, GObject *obj, guint keyval, GdkModifierType mod, gpointer user_data) {
  if (mainw->playing_file==-1) return TRUE;

  // change play direction
  if (cfile->play_paused) {
    cfile->freeze_fps=-cfile->freeze_fps;
    return TRUE;
  }

  g_signal_handler_block(mainw->spinbutton_pb_fps,mainw->pb_fps_func);
  lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_pb_fps),-cfile->pb_fps);
  g_signal_handler_unblock(mainw->spinbutton_pb_fps,mainw->pb_fps_func);

  // make sure this is called, sometimes we switch clips too soon...
  changed_fps_during_pb (LIVES_SPIN_BUTTON(mainw->spinbutton_pb_fps), NULL);

  return TRUE;
}


boolean fps_reset_callback (GtkAccelGroup *group, GObject *obj, guint keyval, GdkModifierType mod, gpointer user_data) {
  // reset playback fps (cfile->pb_fps) to normal fps (cfile->fps)
  // also resync the audio


  if (mainw->playing_file==-1) return TRUE;

  // change play direction
  if (cfile->play_paused) {
    if (cfile->freeze_fps<0.) cfile->freeze_fps=-cfile->fps; 
    else cfile->freeze_fps=cfile->fps;
    return TRUE;
  }

  g_signal_handler_block(mainw->spinbutton_pb_fps,mainw->pb_fps_func);
  if (cfile->pb_fps>0.) lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_pb_fps),cfile->fps);
  else lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_pb_fps),-cfile->fps);
  g_signal_handler_unblock(mainw->spinbutton_pb_fps,mainw->pb_fps_func);

  // make sure this is called, sometimes we switch clips too soon...
  changed_fps_during_pb (LIVES_SPIN_BUTTON(mainw->spinbutton_pb_fps), NULL);

  if (prefs->audio_opts&AUDIO_OPTS_FOLLOW_FPS) {
    resync_audio(cfile->frameno);
  }

  return TRUE;
}

boolean prevclip_callback (GtkAccelGroup *group, GObject *obj, guint keyval, GdkModifierType mod, gpointer user_data) {
  GList *list_index;
  int i=0;
  int num_tried=0,num_clips;
  int type=0;

  // prev clip
  // type = 0 : if the effect is a transition, this will change the background clip
  // type = 1 fg only
  // type = 2 bg only 

  if (mainw->current_file<1||mainw->preview||(mainw->is_processing&&cfile->is_loaded)||mainw->cliplist==NULL) return TRUE;

  if (user_data!=NULL) type=GPOINTER_TO_INT(user_data);

  num_clips=g_list_length(mainw->cliplist);

  if (type==2||(mainw->active_sa_clips==SCREEN_AREA_BACKGROUND&&mainw->playing_file>0&&type!=1)) {
    list_index=g_list_find (mainw->cliplist, GINT_TO_POINTER (mainw->blend_file));
  }
  else {
    list_index=g_list_find (mainw->cliplist, GINT_TO_POINTER (mainw->current_file));
  }
  do {
    if (num_tried++==num_clips) return TRUE; // we might have only audio clips, and then we will block here
    if ((list_index=g_list_previous(list_index))==NULL) list_index=g_list_last (mainw->cliplist);
    i=LIVES_POINTER_TO_INT (list_index->data);
  } while ((mainw->files[i]==NULL||mainw->files[i]->opening||mainw->files[i]->restoring||i==mainw->scrap_file||
	    i==mainw->ascrap_file||(!mainw->files[i]->frames&&mainw->playing_file>-1))&&
	   i!=((type==2||(mainw->playing_file>0&&mainw->active_sa_clips==SCREEN_AREA_BACKGROUND&&type!=1))?
	       mainw->blend_file:mainw->current_file));

  switch_clip(type,i);

  return TRUE;
}


boolean nextclip_callback (GtkAccelGroup *group, GObject *obj, guint keyval, GdkModifierType mod, gpointer user_data) {
  GList *list_index;
  int i;
  int num_tried=0,num_clips;

  int type=0;

  // next clip
  // if the effect is a transition, this will change the background clip
  if (mainw->current_file<1||mainw->preview||(mainw->is_processing&&cfile->is_loaded)||mainw->cliplist==NULL) return TRUE;

  if (user_data!=NULL) type=GPOINTER_TO_INT(user_data);

  if (type==2||(mainw->active_sa_clips==SCREEN_AREA_BACKGROUND&&mainw->playing_file>0&&type!=1)) {
    list_index=g_list_find (mainw->cliplist, GINT_TO_POINTER (mainw->blend_file));
  }
  else {
    list_index=g_list_find (mainw->cliplist, GINT_TO_POINTER (mainw->current_file));
  }

  num_clips=g_list_length(mainw->cliplist);

  do {
    if (num_tried++==num_clips) return TRUE; // we might have only audio clips, and then we will block here
    if ((list_index=g_list_next(list_index))==NULL) list_index=g_list_first (mainw->cliplist);
    i=LIVES_POINTER_TO_INT (list_index->data);
  } while ((mainw->files[i]==NULL||mainw->files[i]->opening||mainw->files[i]->restoring||i==mainw->scrap_file||
	    i==mainw->ascrap_file||(!mainw->files[i]->frames&&mainw->playing_file>-1))&&
	   i!=((type==2||(mainw->playing_file>0&&mainw->active_sa_clips==SCREEN_AREA_BACKGROUND&&type!=1))?
	       mainw->blend_file:mainw->current_file));
  
  switch_clip(type,i);

  return TRUE;
}


void on_save_set_activate (GtkMenuItem *menuitem, gpointer user_data) {

  // here is where we save clipsets

  // SAVE CLIPSET FUNCTION

  // also handles migration and merging of sets


  // TODO - caller to do end_threaded_dialog()

  GList *cliplist;

  gchar *old_set=g_strdup(mainw->set_name);
  gchar *layout_map_file,*layout_map_dir,*new_clips_dir,*current_clips_dir;
  gchar *com,*tmp;
  gchar new_handle[256];
  gchar *text;
  gchar *new_dir;
  gchar *cwd;
  gchar *ordfile;
  gchar *ord_entry;
  gchar new_set_name[128];
  gchar *msg,*extra;
  gchar *dfile,*osetn,*nsetn;

  boolean is_append=FALSE; // we will overwrite the target layout.map file
  boolean response;
  boolean got_new_handle=FALSE;

  int ord_fd;
  int retval;

  register int i;

  // warn the user what will happen
  if (!mainw->no_exit&&!mainw->only_close) extra=g_strdup(", and LiVES will exit");
  else extra=g_strdup("");

  msg=g_strdup_printf(_("Saving the set will cause copies of all loaded clips to remain on the disk%s.\n\nPlease press 'Cancel' if that is not what you want.\n"),extra);
  g_free(extra);

  if (menuitem!=NULL&&!do_warning_dialog_with_check (msg,WARN_MASK_SAVE_SET)) {
    g_free(msg);
    return;
  }
  g_free(msg);


  if (mainw->stored_event_list!=NULL&&mainw->stored_event_list_changed) {
    // if we have a current layout, give the user the chance to change their mind
    if (!check_for_layout_del(NULL,FALSE)) return;
  }


  if (menuitem!=NULL) {
    // this was called from the GUI

    do {
      // prompt for a set name, advise user to save set
      renamew=create_rename_dialog(2);
      lives_widget_show(renamew->dialog);
      response=lives_dialog_run(LIVES_DIALOG(renamew->dialog));
      if (response==GTK_RESPONSE_CANCEL) {
	lives_widget_destroy(renamew->dialog);
	g_free(renamew);
	return;
      }
      g_snprintf(new_set_name,128,"%s",lives_entry_get_text (LIVES_ENTRY (renamew->entry)));
      lives_widget_destroy(renamew->dialog);
      g_free(renamew);
      lives_widget_context_update();
    } while (!is_legal_set_name(new_set_name,TRUE));
  }
  else g_snprintf(new_set_name,128,"%s",(gchar *)user_data);

  lives_widget_queue_draw (mainw->LiVES);
  lives_widget_context_update();

  g_snprintf(mainw->set_name,128,"%s",new_set_name);

  if (strcmp(mainw->set_name,old_set)) {
    // THE USER CHANGED the set name

    // we must migrate all physical files for the set


    // and possibly merge with another set

    new_clips_dir=g_build_filename(prefs->tmpdir,mainw->set_name,"clips",NULL);
    // check if target clips dir exists, ask if user wants to append files
    if (g_file_test(new_clips_dir,G_FILE_TEST_IS_DIR)) {
      g_free(new_clips_dir);
      if (!do_set_duplicate_warning(mainw->set_name)) {
	g_snprintf(mainw->set_name,128,"%s",old_set);
	return;
      }
      is_append=TRUE;
    }
    else {
      g_free(new_clips_dir);
      layout_map_file=g_build_filename(prefs->tmpdir,mainw->set_name,"layouts","layout.map",NULL);
      // if target has layouts dir but no clips, it means we have old layouts !
      if (g_file_test(layout_map_file,G_FILE_TEST_EXISTS)) {
	if (do_set_rename_old_layouts_warning(mainw->set_name)) {
	  // user answered "yes" - delete
	  // clear _old_ layout maps
	  gchar *dfile=g_build_filename(prefs->tmpdir,mainw->set_name,"layouts",NULL);
#ifndef IS_MINGW
	  com=g_strdup_printf("/bin/rm -r \"%s/\" 2>/dev/null",dfile);
#else
	  com=g_strdup_printf("rm.exe -r \"%s/\" 2>NUL",dfile);
#endif
	  g_free(dfile);
	  lives_system(com,TRUE);
	  g_free(com);
	}
      }
      g_free(layout_map_file);
    }
  }
  
  text=g_strdup_printf(_("Saving set %s"),mainw->set_name);
  do_threaded_dialog(text,FALSE);
  g_free(text);

  /////////////////////////////////////////////////////////////

  mainw->com_failed=FALSE;

  current_clips_dir=g_build_filename(prefs->tmpdir,old_set,"clips/",NULL);
  if (strlen(old_set)&&strcmp(mainw->set_name,old_set)&&g_file_test(current_clips_dir,G_FILE_TEST_IS_DIR)) {
    // set name was changed for an existing set

    if (!is_append) {
      // create new dir, in case it doesn't already exist
      dfile=g_build_filename(prefs->tmpdir,mainw->set_name,"clips",NULL);
      if (g_mkdir_with_parents(dfile,S_IRWXU)==-1) {
	if (!check_dir_access(dfile)) {
	  // abort if we cannot create the new subdir
	  LIVES_ERROR("Could not create directory");
	  LIVES_ERROR(dfile);
	  d_print_file_error_failed();
	  g_snprintf(mainw->set_name,128,"%s",old_set);
	  g_free(dfile);
	  end_threaded_dialog();
	  return;
	}
      }
      g_free(dfile);
    }
  }
  else {
    // saving as same name (or as new set)

    dfile=g_build_filename(prefs->tmpdir,mainw->set_name,"clips",NULL);
    if (g_mkdir_with_parents(dfile,S_IRWXU)==-1) {
      if (!check_dir_access(dfile)) {
	// abort if we cannot create the new subdir
	LIVES_ERROR("Could not create directory");
	LIVES_ERROR(dfile);
	d_print_file_error_failed();
	g_snprintf(mainw->set_name,128,"%s",old_set);
	g_free(dfile);
	end_threaded_dialog();
	return;
      }
    }
    g_free(dfile);
  }
  g_free(current_clips_dir);

  ordfile=g_build_filename(prefs->tmpdir,mainw->set_name,"order",NULL);

  cwd=g_get_current_dir();

  do {
    // create the orderfile which lists all the clips in order
    retval=0;
    if (!is_append) ord_fd=creat(ordfile,DEF_FILE_PERMS);
    else ord_fd=open(ordfile,O_CREAT|O_WRONLY|O_APPEND,DEF_FILE_PERMS);

    if (ord_fd<0) {
      retval=do_write_failed_error_s_with_retry(ordfile,g_strerror(errno),NULL);
      if (retval==LIVES_CANCEL) {
	end_threaded_dialog();
	g_free(ordfile);
	return;
      }
    }

    else {
      mainw->write_failed=FALSE;
      cliplist=mainw->cliplist;

      while (cliplist!=NULL) {
	if (mainw->write_failed) break;
	threaded_dialog_spin();
	lives_widget_context_update();

	// TODO - dirsep

	i=GPOINTER_TO_INT(cliplist->data);
	if (mainw->files[i]!=NULL&&(mainw->files[i]->clip_type==CLIP_TYPE_FILE||
				    mainw->files[i]->clip_type==CLIP_TYPE_DISK)) {
	  if ((tmp=strrchr(mainw->files[i]->handle,'/'))!=NULL) {
	    g_snprintf(new_handle,256,"%s/clips%s",mainw->set_name,tmp);
	  }
	  else {
	    g_snprintf(new_handle,256,"%s/clips/%s",mainw->set_name,mainw->files[i]->handle);
	  }
	  if (strcmp(new_handle,mainw->files[i]->handle)) {
	    new_dir=g_build_filename(prefs->tmpdir,new_handle,NULL);
	    if (g_file_test(new_dir,G_FILE_TEST_IS_DIR)) {
	      // get a new unique handle
	      get_temp_handle(i,FALSE);
	      g_snprintf(new_handle,256,"%s/clips/%s",mainw->set_name,mainw->files[i]->handle);
	    }
	    g_free(new_dir);
	
	    // move the files


	    mainw->com_failed=FALSE;
#ifndef IS_MINGW
	    com=g_strdup_printf("/bin/mv \"%s/%s\" \"%s/%s\"",
				prefs->tmpdir,mainw->files[i]->handle,prefs->tmpdir,new_handle);
#else
	    com=g_strdup_printf("mv.exe \"%s\\%s\" \"%s\\%s\"",
				prefs->tmpdir,mainw->files[i]->handle,prefs->tmpdir,new_handle);
#endif
	    lives_system(com,FALSE);
	    g_free(com);
	
	    if (mainw->com_failed) {
	      end_threaded_dialog();
	      g_free(ordfile);
	      return;
	    }

	    got_new_handle=TRUE;
	
	    g_snprintf(mainw->files[i]->handle,256,"%s",new_handle);
#ifndef IS_MINGW
	    dfile=g_build_filename(prefs->tmpdir,mainw->files[i]->handle,".status",NULL);
#else
	    dfile=g_build_filename(prefs->tmpdir,mainw->files[i]->handle,"status",NULL);
#endif
	    g_snprintf(mainw->files[i]->info_file,PATH_MAX,"%s",dfile);
	    g_free(dfile);
	  }
      
	  ord_entry=g_strdup_printf("%s\n",mainw->files[i]->handle);
	  lives_write(ord_fd,ord_entry,strlen(ord_entry),FALSE);
	  g_free(ord_entry);

	}

	cliplist=cliplist->next;
      }

      if (mainw->write_failed) {
	retval=do_write_failed_error_s_with_retry(ordfile,NULL,NULL);
      }

    }

  } while (retval==LIVES_RETRY);
  
  close (ord_fd);
  g_free(ordfile);

  lives_chdir(cwd,FALSE);
  g_free(cwd);

  if (retval==LIVES_CANCEL) {
    end_threaded_dialog();
    return;
  }

  if (got_new_handle&&!strlen(old_set)) migrate_layouts(NULL,mainw->set_name);

  if (strlen(old_set)&&strcmp(old_set,mainw->set_name)) {
    layout_map_dir=g_build_filename(prefs->tmpdir,old_set,"layouts",G_DIR_SEPARATOR_S,NULL);
    layout_map_file=g_build_filename(layout_map_dir,"layout.map",NULL);
    // update details for layouts - needs_set, current_layout_map and affected_layout_map
    if (g_file_test(layout_map_file,G_FILE_TEST_EXISTS)) {
      migrate_layouts(old_set,mainw->set_name);
      // save updated layout.map (with new handles), we will move it below

      save_layout_map(NULL,NULL,NULL,layout_map_dir);

      got_new_handle=FALSE;
    }
    g_free(layout_map_file);
    g_free(layout_map_dir);

    if (is_append) {
      osetn=g_build_filename(prefs->tmpdir,old_set,"layouts","layout.map",NULL);
      nsetn=g_build_filename(prefs->tmpdir,mainw->set_name,"layouts","layout.map",NULL);

      //append current layout.map to target one
#ifndef IS_MINGW
      com=g_strdup_printf("/bin/cat \"%s\" >> \"%s\" 2>/dev/null",osetn,nsetn);
#else
      com=g_strdup_printf("cat.exe \"%s\" >> \"%s\" 2>NUL",osetn,nsetn);
#endif
      lives_system(com,TRUE);
      g_free(com);
#ifndef IS_MINGW
      com=g_strdup_printf("/bin/rm \"%s\" 2>/dev/null",osetn);
#else
      com=g_strdup_printf("rm.exe \"%s\" 2>NUL",osetn);
#endif
      lives_system(com,TRUE);
      g_free(com);
      g_free(osetn);
      g_free(nsetn);
    }
    
    osetn=g_build_filename(prefs->tmpdir,old_set,"layouts",NULL);
    nsetn=g_build_filename(prefs->tmpdir,mainw->set_name,NULL);

    // move any layouts from old set to new (including layout.map)
#ifndef IS_MINGW
    com=g_strdup_printf("/bin/cp -a \"%s\" \"%s/\" 2>/dev/null",osetn,nsetn);
#else
    com=g_strdup_printf("cp.exe -a \"%s\" \"%s/\" 2>NUL",osetn,nsetn);
#endif
    lives_system(com,TRUE);
    g_free(com);

    g_free(osetn);
    g_free(nsetn);

    osetn=g_build_filename(prefs->tmpdir,old_set,NULL);

    // remove old set dir
#ifndef IS_MINGW
    com=g_strdup_printf("/bin/rm -r \"%s\" 2>/dev/null",osetn);
#else
    com=g_strdup_printf("rm.exe -r \"%s\" 2>NUL",osetn);
#endif
    lives_system(com,TRUE);
    g_free(com);
    g_free(osetn);
  }


  if (!mainw->was_set&&!strcmp(old_set,mainw->set_name)) {
    // set name was set by export or save layout, now we need to update our layout map
    layout_map_dir=g_build_filename(prefs->tmpdir,old_set,"layouts",G_DIR_SEPARATOR_S,NULL);
    layout_map_file=g_build_filename(layout_map_dir,"layout.map",NULL);
    if (g_file_test(layout_map_file,G_FILE_TEST_EXISTS)) save_layout_map(NULL,NULL,NULL,layout_map_dir);
    mainw->was_set=TRUE;
    got_new_handle=FALSE;
    g_free(layout_map_dir);
    g_free(layout_map_file);
    if (mainw->multitrack!=NULL&&!mainw->multitrack->changed) recover_layout_cancelled(FALSE);
  }

  if (mainw->current_layouts_map!=NULL&&strcmp(old_set,mainw->set_name)&&!mainw->is_exiting) {
    // warn the user about layouts if the set name changed
    // but, don't bother the user with errors if we are exiting
    add_lmap_error(LMAP_INFO_SETNAME_CHANGED,old_set,mainw->set_name,0,0,0.,FALSE);
    popup_lmap_errors(NULL,NULL);
  }

#ifdef ENABLE_OSC
  lives_osc_notify(LIVES_OSC_NOTIFY_CLIPSET_SAVED,old_set);
#endif

  g_free (old_set);
  if (!mainw->no_exit) {
    mainw->leave_files=TRUE;
    if (mainw->multitrack!=NULL&&!mainw->only_close) mt_memory_free();
    else if (mainw->multitrack!=NULL) wipe_layout(mainw->multitrack);
    lives_exit();
  }
  else end_threaded_dialog();

  lives_widget_set_sensitive (mainw->vj_load_set, TRUE);
}


void on_load_set_activate (GtkMenuItem *menuitem, gpointer user_data) {
  // get set name (use a modified rename window)

  if (mainw->multitrack!=NULL) {
    if (mainw->multitrack->idlefunc>0) {
      g_source_remove(mainw->multitrack->idlefunc);
      mainw->multitrack->idlefunc=0;
    }
    mt_desensitise(mainw->multitrack);
  }

  renamew=create_rename_dialog(3);
  lives_widget_show(renamew->dialog);
}



boolean on_load_set_ok (GtkButton *button, gpointer user_data) {
  // this is the main clip set loader


  // CLIP SET LOADER


  // (user_data TRUE skips threaded dialog and allows use of
  // return value FALSE to indicate error)

  FILE *orderfile;

  gchar *msg;
  gchar *com;
  gchar *ordfile;
  gchar *subfname;
  gchar vid_open_dir[PATH_MAX];
  gchar set_name[128];
  gchar *cwd;

  boolean added_recovery=FALSE;
  boolean skip_threaded_dialog=(boolean)GPOINTER_TO_INT(user_data);
  boolean needs_update=FALSE;

  int last_file=-1,new_file=-1;
  int current_file=mainw->current_file;
  int clipnum=0;


  threaded_dialog_spin();


  // renamew is create_rename_dialog(3);


  if (!strlen(mainw->set_name)) {
    // get new set name from user

    g_snprintf(set_name,128,"%s",lives_entry_get_text(LIVES_ENTRY(renamew->entry)));

    // ensure name is valid
    if (!is_legal_set_name(set_name,TRUE)) return !skip_threaded_dialog;

    // set_name length is max 128
    g_snprintf(mainw->set_name,128,"%s",set_name);


    // need to clean up renamew
    lives_widget_destroy(renamew->dialog);
    g_free(renamew);
    renamew=NULL;
  }
  else {
    // here if we already have a set_name

    // check if set is locked
    if (!check_for_lock_file(mainw->set_name,0)) {
      memset(mainw->set_name,0,1);
      d_print_failed();
      threaded_dialog_spin();
      if (mainw->multitrack!=NULL) {
	mainw->current_file=mainw->multitrack->render_file;
	mt_sensitise(mainw->multitrack);
	mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
      }
      return !skip_threaded_dialog;
    }
  }

  g_snprintf (mainw->msg,256,"none");

  msg=g_strdup_printf(_("Loading clips from set %s"),mainw->set_name);
  if (!skip_threaded_dialog&&prefs->show_gui) {
    do_threaded_dialog(msg,FALSE);
  }
  g_free(msg);

  ordfile=g_build_filename(prefs->tmpdir,mainw->set_name,"order",NULL);
  orderfile=fopen(ordfile,"r"); // no we can't assert this, because older sets did not have this file
  g_free(ordfile);

  mainw->suppress_dprint=TRUE;
  mainw->read_failed=FALSE;

  g_snprintf(vid_open_dir,PATH_MAX,"%s",mainw->vid_load_dir);

  cwd=g_get_current_dir();


  while (1) {
    if (!skip_threaded_dialog&&prefs->show_gui) {
      threaded_dialog_spin();
    }
    if (mainw->cached_list!=NULL) {
      g_list_free_strings(mainw->cached_list);
      g_list_free(mainw->cached_list);
      mainw->cached_list=NULL;
    }

    if (orderfile==NULL) {
      // old style (pre 0.9.6)
      com=g_strdup_printf ("%s get_next_in_set \"%s\" \"%s\" %d",prefs->backend_sync,mainw->msg,
			   mainw->set_name,capable->mainpid);
      lives_system (com,FALSE);
      g_free (com);

      if (strlen (mainw->msg)>0&&(strncmp (mainw->msg,"none",4))) {

	if ((new_file=mainw->first_free_file)==-1) {
	  recover_layout_map(MAX_FILES);

	  if (!skip_threaded_dialog) end_threaded_dialog();
	  mainw->suppress_dprint=FALSE;
	  too_many_files();
	  if (mainw->multitrack!=NULL) {
	    mainw->current_file=mainw->multitrack->render_file;
	    polymorph(mainw->multitrack,POLY_NONE);
	    polymorph(mainw->multitrack,POLY_CLIPS);
	    mt_sensitise(mainw->multitrack);
	    mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
	  }
	  g_free(cwd);
	  return !skip_threaded_dialog;
	}
	mainw->current_file=new_file;
	get_handle_from_info_file(new_file);
      }
    }
    else {
      if (lives_fgets(mainw->msg,512,orderfile)==NULL) clear_mainw_msg();
      else memset(mainw->msg+strlen(mainw->msg)-strlen("\n"),0,1);
    }


    if (strlen (mainw->msg)==0||(!strncmp (mainw->msg,"none",4))) {
      mainw->suppress_dprint=FALSE;
      if (!skip_threaded_dialog) end_threaded_dialog();
      if (orderfile!=NULL) fclose (orderfile);

      mainw->current_file=current_file;

      if (last_file>0) {
	threaded_dialog_spin();
	switch_to_file (current_file,last_file);
	threaded_dialog_spin();
      }


      if (clipnum==0) {
	do_set_noclips_error(mainw->set_name);
	memset (mainw->set_name,0,1);
      }
      else {
	reset_clipmenu();
	lives_widget_set_sensitive (mainw->vj_load_set, FALSE);

	
	recover_layout_map(MAX_FILES);

	msg=g_strdup_printf (_ ("%d clips and %d layouts were recovered from set (%s).\n"),
			     clipnum,g_list_length(mainw->current_layouts_map),mainw->set_name);
	d_print (msg);
	g_free (msg);

#ifdef ENABLE_OSC
	lives_osc_notify(LIVES_OSC_NOTIFY_CLIPSET_OPENED,mainw->set_name);
#endif

      }
    
      threaded_dialog_spin();
      if (mainw->multitrack==NULL) {
	if (mainw->is_ready) {
	  if (clipnum>0&&mainw->current_file>0) {
	    load_start_image (cfile->start);
	    load_end_image (cfile->end);
	  }
	  lives_widget_context_update();
	}
      }
      else {
	mainw->current_file=mainw->multitrack->render_file;
	polymorph(mainw->multitrack,POLY_NONE);
	polymorph(mainw->multitrack,POLY_CLIPS);
	mt_sensitise(mainw->multitrack);
	mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
      }
      threaded_dialog_spin();

      if (!skip_threaded_dialog) end_threaded_dialog();
      g_free(cwd);
      return TRUE;
    }

    mainw->was_set=TRUE;

    if (prefs->crash_recovery&&!added_recovery) {
      gchar *recovery_entry=g_build_filename(mainw->set_name,"*",NULL);
      threaded_dialog_spin();
      add_to_recovery_file(recovery_entry);
      g_free(recovery_entry);
      added_recovery=TRUE;
    }

    if (orderfile!=NULL) {
      // newer style (0.9.6+)
      gchar *clipdir=g_build_filename(prefs->tmpdir,mainw->msg,NULL);
      if (!g_file_test(clipdir,G_FILE_TEST_IS_DIR)) {
	g_free(clipdir);
	continue;
      }
      g_free(clipdir);
      threaded_dialog_spin();
      if ((new_file=mainw->first_free_file)==-1) {
	mainw->suppress_dprint=FALSE;

	if (!skip_threaded_dialog) end_threaded_dialog();

	too_many_files();

	if (mainw->multitrack!=NULL) {
	  mainw->current_file=mainw->multitrack->render_file;
	  polymorph(mainw->multitrack,POLY_NONE);
	  polymorph(mainw->multitrack,POLY_CLIPS);
	  mt_sensitise(mainw->multitrack);
	  mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
	}

	recover_layout_map(MAX_FILES);
	g_free(cwd);
	return !skip_threaded_dialog;
      }
      mainw->current_file=new_file;
      cfile=(lives_clip_t *)(g_malloc(sizeof(lives_clip_t)));
      g_snprintf(cfile->handle,256,"%s",mainw->msg);
      cfile->clip_type=CLIP_TYPE_DISK; // the default

      // lock the set
#ifndef IS_MINGW
      com=g_strdup_printf("/bin/touch \"%s/%s/lock.%d\"",prefs->tmpdir,mainw->set_name,capable->mainpid);
#else
      com=g_strdup_printf("touch.exe \"%s\\%s\\lock.%d\"",prefs->tmpdir,mainw->set_name,capable->mainpid);
#endif
      lives_system(com,FALSE);
      g_free(com);
    }
  
    //create a new cfile and fill in the details
    create_cfile();
    threaded_dialog_spin();

    // get file details
    read_headers(".");
    threaded_dialog_spin();
    
    // if the clip has a frame_index file, then it is CLIP_TYPE_FILE
    // and we must load the frame_index and locate a suitable decoder plugin

    if (load_frame_index(mainw->current_file)) {
      // CLIP_TYPE_FILE
      if (!reload_clip(mainw->current_file)) continue;
    }
    else {
      // CLIP_TYPE_DISK
      if (!check_frame_count(mainw->current_file)) {
	get_frame_count(mainw->current_file);
	needs_update=TRUE;
      }
      if (cfile->frames>0&&(cfile->hsize*cfile->vsize==0)) {
	get_frames_sizes(mainw->current_file,1);
	if (cfile->hsize*cfile->vsize>0) needs_update=TRUE;
      }
    }

    last_file=new_file;

    // read the playback fps, play frame, and name
    open_set_file (mainw->set_name,++clipnum);
    threaded_dialog_spin();

    if (needs_update) {
      save_clip_values(mainw->current_file);
      needs_update=FALSE;
    }

    if (mainw->cached_list!=NULL) {
      g_list_free_strings(mainw->cached_list);
      g_list_free(mainw->cached_list);
      mainw->cached_list=NULL;
    }

    if (prefs->autoload_subs) {
      subfname=g_build_filename(prefs->tmpdir,cfile->handle,"subs.srt",NULL);
      if (g_file_test(subfname,G_FILE_TEST_EXISTS)) {
	subtitles_init(cfile,subfname,SUBTITLE_TYPE_SRT);
      }
      else {
	g_free(subfname);
	subfname=g_build_filename(prefs->tmpdir,cfile->handle,"subs.sub",NULL);
	if (g_file_test(subfname,G_FILE_TEST_EXISTS)) {
	  subtitles_init(cfile,subfname,SUBTITLE_TYPE_SUB);
	}
      }
      g_free(subfname);
      threaded_dialog_spin();
    }

    get_total_time (cfile);
    if (cfile->achans) cfile->aseek_pos=(int64_t)((double)(cfile->frameno-1.)/
					       cfile->fps*cfile->arate*cfile->achans*cfile->asampsize/8);

    // add to clip menu
    threaded_dialog_spin();
    add_to_clipmenu();
    get_next_free_file();
    cfile->start=cfile->frames>0?1:0;
    cfile->end=cfile->frames;
    cfile->is_loaded=TRUE;
    cfile->changed=TRUE;
    unlink (cfile->info_file);
    set_main_title(cfile->name,0);

    if (mainw->multitrack!=NULL&&mainw->multitrack->is_ready) {
      mainw->current_file=mainw->multitrack->render_file;
      mt_init_clips(mainw->multitrack,new_file,TRUE);
      lives_widget_context_update();
      mt_clip_select(mainw->multitrack,TRUE);
    }

#ifdef ENABLE_OSC
    lives_osc_notify(LIVES_OSC_NOTIFY_CLIP_OPENED,"");
#endif
  }
  
  threaded_dialog_spin();
  reset_clipmenu();
  threaded_dialog_spin();

  if (mainw->multitrack!=NULL&&mainw->multitrack->is_ready) {
    mainw->current_file=mainw->multitrack->render_file;
    polymorph(mainw->multitrack,POLY_NONE);
    polymorph(mainw->multitrack,POLY_CLIPS);
    mt_sensitise(mainw->multitrack);
    mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
  }

  recover_layout_map(MAX_FILES);

  mainw->suppress_dprint=FALSE;
  g_free(cwd);
  return TRUE;
}



void on_cleardisk_activate (GtkWidget *widget, gpointer user_data) {
  // recover disk space

  int64_t bytes=0,fspace;
  int64_t ds_warn_level=mainw->next_ds_warn_level;

  gchar *markerfile;
  gchar **array;
  gchar *com;

  int current_file=mainw->current_file;
  int marker_fd;
  int retval=0;

  register int i;

  mainw->next_ds_warn_level=0; /// < avoid nested warnings

  if (user_data!=NULL) lives_widget_hide(lives_widget_get_toplevel(LIVES_WIDGET(user_data)));

  mainw->tried_ds_recover=TRUE; ///< indicates we tried ds recovery already

  mainw->add_clear_ds_adv=TRUE; ///< auto reset by do_warning_dialog()
  if (!do_warning_dialog (_ ("LiVES will attempt to recover some disk space.\nYou should ONLY run this if you have no other copies of LiVES running on this machine.\nClick OK to proceed.\n"))) {
    mainw->next_ds_warn_level=ds_warn_level;;
    return;
  }

  d_print(_ ("Cleaning up disk space..."));

  // get a temporary clip for receiving data from backend
  if (!get_temp_handle(mainw->first_free_file,TRUE)) {
    d_print_failed();
    mainw->next_ds_warn_level=ds_warn_level;
    return;
  }

  cfile->cb_src=current_file;

  if (mainw->multitrack!=NULL) {
    if (mainw->multitrack->idlefunc>0) {
      g_source_remove(mainw->multitrack->idlefunc);
      mainw->multitrack->idlefunc=0;
    }
    mt_desensitise(mainw->multitrack);
  }


  for (i=0;i<MAX_FILES;i++) {
    // mark all free-floating files (directories) which we do not want to remove
    // we do error checking here

    if ((i==mainw->current_file)||(mainw->files[i]!=NULL&&(mainw->files[i]->clip_type==CLIP_TYPE_DISK||
							   mainw->files[i]->clip_type==CLIP_TYPE_FILE))) {
      markerfile=g_build_filename(prefs->tmpdir,mainw->files[i]->handle,"set.",NULL);

      do {
	retval=0;
	marker_fd=creat(markerfile,S_IRUSR|S_IWUSR);
	if (marker_fd<0) {
	  retval=do_write_failed_error_s_with_retry(markerfile,g_strerror(errno),NULL);
	}
      } while (retval==LIVES_RETRY);

      close(marker_fd);
      g_free(markerfile);
      if (mainw->files[i]->undo_action!=UNDO_NONE) {
	markerfile=g_build_filename(prefs->tmpdir,mainw->files[i]->handle,"noprune",NULL);
	do {
	  retval=0;
	  marker_fd=creat(markerfile,S_IRUSR|S_IWUSR);
	  if (marker_fd<0) {
	    retval=do_write_failed_error_s_with_retry(markerfile,g_strerror(errno),NULL);
	  }
	} while (retval==LIVES_RETRY);
	close(marker_fd);
	g_free(markerfile);
      }
    }
  }


  // get space before
  fspace=get_fs_free(prefs->tmpdir);

  // call "smogrify bg_weed" to do the actual cleanup
  // its parameters are the handle of a temp file, and opts mask

  if (retval!=LIVES_CANCEL) {
    mainw->com_failed=FALSE;
    unlink(cfile->info_file);
    com=g_strdup_printf("%s bg_weed \"%s\" %d",prefs->backend,cfile->handle,prefs->clear_disk_opts);
    lives_system(com,FALSE);
    g_free(com);
    
    if (!mainw->com_failed) {

      // show a progress dialog, not cancellable
      do_progress_dialog(TRUE,FALSE,_("Recovering disk space"));
      
      array=g_strsplit(mainw->msg,"|",2);
      bytes=strtol(array[1],NULL,10);
      if (bytes<0) bytes=0;
      g_strfreev(array);
      
    }
  }


  // close the temporary clip
  com=g_strdup_printf("%s close \"%s\"",prefs->backend,cfile->handle);
  lives_system(com,FALSE);
  g_free(com);
  g_free(cfile);
  cfile=NULL;
  if (mainw->first_free_file==-1||mainw->first_free_file>mainw->current_file) 
    mainw->first_free_file=mainw->current_file;

  // remove the protective markers
  for (i=0;i<MAX_FILES;i++) {
    if (mainw->files[i]!=NULL&&mainw->files[i]->clip_type==CLIP_TYPE_DISK) {
      markerfile=g_build_filename(prefs->tmpdir,mainw->files[i]->handle,"set.",NULL);
      unlink (markerfile);
      g_free(markerfile);
      if (mainw->files[i]->undo_action!=UNDO_NONE) {
	markerfile=g_build_filename(prefs->tmpdir,mainw->files[i]->handle,"noprune",NULL);
	unlink (markerfile);
	g_free(markerfile);
      }
    }
  }

  mainw->current_file=current_file;
  sensitize();

  if (mainw->multitrack!=NULL) {
    mt_sensitise(mainw->multitrack);
    mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
  }


  if (bytes==0) {
    // get after
    bytes=get_fs_free(prefs->tmpdir)-fspace;
  }

  if (bytes<0) bytes=0;

  if (retval!=LIVES_CANCEL&&!mainw->com_failed) {
    d_print_done();
    do_info_dialog(g_strdup_printf(_("%s of disk space was recovered.\n"), 
				   lives_format_storage_space_string((uint64_t)bytes)));
    if (user_data!=NULL) lives_widget_set_sensitive(lives_widget_get_toplevel(LIVES_WIDGET(user_data)),FALSE);
  }
  else d_print_failed();

  mainw->next_ds_warn_level=ds_warn_level;;

}


void on_cleardisk_advanced_clicked (GtkWidget *widget, gpointer user_data) {
  // make cleardisk adv window

  // show various options and OK/Cancel button

  // on OK set clear_disk opts
  int response;
  GtkWidget *dialog;
  do {
    dialog=create_cleardisk_advanced_dialog();
    lives_widget_show_all(dialog);
    response=lives_dialog_run(LIVES_DIALOG(dialog));
    lives_widget_destroy(dialog);
    if (response==LIVES_RETRY) prefs->clear_disk_opts=0;
  } while (response==LIVES_RETRY);

  set_int_pref("clear_disk_opts",prefs->clear_disk_opts);
}


void on_show_keys_activate (GtkMenuItem *menuitem, gpointer user_data) {
  do_keys_window();
}


void on_vj_reset_activate (GtkMenuItem *menuitem, gpointer user_data) {
  boolean bad_header=FALSE;

  register int i;

  //mainw->soft_debug=TRUE;

  do_threaded_dialog(_("Resetting frame rates and frame values..."),FALSE);

  for (i=1;i<MAX_FILES;i++) {
    if (mainw->files[i]!=NULL&&i!=mainw->playing_file) {
      mainw->files[i]->pb_fps=mainw->files[i]->fps;
      mainw->files[i]->frameno=1;
      mainw->files[i]->aseek_pos=0;

      save_clip_value(i,CLIP_DETAILS_PB_FPS,&mainw->files[i]->pb_fps);
      if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
      save_clip_value(i,CLIP_DETAILS_PB_FRAMENO,&mainw->files[i]->frameno);
      if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
    }
    if (bad_header) do_header_write_error(i);
  }


  mainw->noswitch=FALSE; // just in case...

  end_threaded_dialog();

}


void on_show_messages_activate (GtkMenuItem *menuitem, gpointer user_data) {
  do_messages_window();
}


void on_show_file_info_activate (GtkMenuItem *menuitem, gpointer user_data) {
  char buff[512];
  lives_clipinfo_t *filew;

  gchar *sigs,*ends,*tmp;
  
  if (mainw->current_file==-1) return;

  filew = create_clip_info_window (cfile->achans,FALSE);
  
  if (cfile->frames>0) {
    // type
    g_snprintf(buff,512,_("\nExternal: %s\nInternal: %s (%d bpp) / %s\n"),cfile->type,
	       (tmp=g_strdup((cfile->clip_type==CLIP_TYPE_YUV4MPEG||
			      cfile->clip_type==CLIP_TYPE_VIDEODEV)?(_("buffered")):
			     (cfile->img_type==IMG_TYPE_JPEG?"jpeg":"png"))),cfile->bpp,"pcm");
    g_free(tmp);
    text_view_set_text (LIVES_TEXT_VIEW (filew->textview24),buff, -1);
    // fps
    g_snprintf(buff,512,"\n  %.3f%s",cfile->fps,cfile->ratio_fps?"...":"");

    text_view_set_text (LIVES_TEXT_VIEW (filew->textview25),buff, -1);
    // image size
    g_snprintf(buff,512,"\n  %dx%d",cfile->hsize,cfile->vsize);
    text_view_set_text (LIVES_TEXT_VIEW (filew->textview26),buff, -1);
    // frames
    if ((cfile->opening&&!cfile->opening_audio&&cfile->frames==0)||cfile->frames==123456789) {
      g_snprintf(buff,512,"%s",_ ("\n  Opening..."));
    }
    else {
      g_snprintf(buff,512,"\n  %d",cfile->frames);

      if (cfile->frame_index!=NULL) {
	int fvirt=count_virtual_frames(cfile->frame_index,1,cfile->frames);
	gchar *tmp=g_strdup_printf(_("\n(%d virtual)"),fvirt);
	g_strappend(buff,512,tmp);
	g_free(tmp);
	tmp=g_strdup_printf(_("\n(%d decoded)"),cfile->frames-fvirt);
	g_strappend(buff,512,tmp);
	g_free(tmp);
      }

    }
    text_view_set_text (LIVES_TEXT_VIEW (filew->textview27),buff, -1);
    // video time
    if ((cfile->opening&&!cfile->opening_audio&&cfile->frames==0)||cfile->frames==123456789) {
      g_snprintf(buff,512,"%s",_ ("\n  Opening..."));
    }
    else {
      g_snprintf(buff,512,_("\n  %.2f sec."),cfile->video_time);
    }
    text_view_set_text (LIVES_TEXT_VIEW (filew->textview28),buff, -1);
    // file size
    if (cfile->f_size>=0l) {
      gchar *file_ds=lives_format_storage_space_string((uint64_t)cfile->f_size);
      g_snprintf(buff,512,"\n  %s",file_ds);
      g_free(file_ds);
    }
    else g_snprintf(buff,512,"%s",_ ("\n  Unknown"));
    text_view_set_text (LIVES_TEXT_VIEW (filew->textview29),buff, -1);
  }

  if (cfile->achans>0) {
    if (cfile->opening) {
      g_snprintf(buff,512,"%s",_ ("\n  Opening..."));
    }
    else {
      g_snprintf(buff,512,_("\n  %.2f sec."),cfile->laudio_time);
    }
    text_view_set_text (LIVES_TEXT_VIEW (filew->textview_ltime),buff, -1);

    if (cfile->signed_endian&AFORM_UNSIGNED) sigs=g_strdup(_("unsigned"));
    else sigs=g_strdup(_("signed"));

    if (cfile->signed_endian&AFORM_BIG_ENDIAN) ends=g_strdup(_("big-endian"));
    else ends=g_strdup(_("little-endian"));

    g_snprintf(buff,512,_("  %d Hz %d bit\n%s %s"),cfile->arate,cfile->asampsize,sigs,ends);
    text_view_set_text (LIVES_TEXT_VIEW (filew->textview_lrate),buff, -1);

    g_free(sigs);
    g_free(ends);
  }
  
  if (cfile->achans>1) {
    if (cfile->signed_endian&AFORM_UNSIGNED) sigs=g_strdup(_("unsigned"));
    else sigs=g_strdup(_("signed"));

    if (cfile->signed_endian&AFORM_BIG_ENDIAN) ends=g_strdup(_("big-endian"));
    else ends=g_strdup(_("little-endian"));

    g_snprintf(buff,512,_("  %d Hz %d bit\n%s %s"),cfile->arate,cfile->asampsize,sigs,ends);
    text_view_set_text (LIVES_TEXT_VIEW (filew->textview_rrate),buff, -1);

    g_free(sigs);
    g_free(ends);
    
    if (cfile->opening) {
      g_snprintf(buff,512,"%s",_ ("\n  Opening..."));
    }
    else {
      g_snprintf(buff,512,_("\n  %.2f sec."),cfile->raudio_time);
    }
    text_view_set_text (LIVES_TEXT_VIEW (filew->textview_rtime),buff, -1);
  }

}


void on_show_file_comments_activate (GtkMenuItem *menuitem, gpointer user_data) {
  do_comments_dialog(NULL,NULL);
}



void on_show_clipboard_info_activate (GtkMenuItem *menuitem, gpointer user_data) {
  int current_file=mainw->current_file;
  mainw->current_file=0;
  on_show_file_info_activate(menuitem,user_data);
  mainw->current_file=current_file;
}


void switch_clip(int type, int newclip) {
  // generic switch clip callback

  // prev clip
  // type = 0 : if the effect is a transition, this will change the background clip
  // type = 1 fg only
  // type = 2 bg only 

  if (mainw->current_file<1||mainw->multitrack!=NULL||mainw->preview||mainw->internal_messaging||
      (mainw->is_processing&&cfile->is_loaded)||mainw->cliplist==NULL) return;

  if (type==2||(mainw->active_sa_clips==SCREEN_AREA_BACKGROUND&&mainw->playing_file>0&&type!=1)) {
    // switch bg clip
    if (newclip!=mainw->blend_file) {
      if (mainw->blend_file!=-1&&mainw->files[mainw->blend_file]->clip_type==CLIP_TYPE_GENERATOR&&
	  mainw->blend_file!=mainw->current_file) {
	mainw->osc_block=TRUE;
	if (rte_window!=NULL) rtew_set_keych(rte_bg_gen_key(),FALSE);
	if (mainw->ce_thumbs) ce_thumbs_set_keych(rte_bg_gen_key(),FALSE);
	mainw->new_blend_file=newclip;
	weed_generator_end ((weed_plant_t *)mainw->files[mainw->blend_file]->ext_src);
	mainw->osc_block=FALSE;
      }
      mainw->blend_file=newclip;
      mainw->whentostop=NEVER_STOP;
      if (mainw->ce_thumbs&&mainw->active_sa_clips==SCREEN_AREA_BACKGROUND) ce_thumbs_highlight_current_clip();
    }
    return;
  }
  
  // switch fg clip

  if (newclip==mainw->current_file) return;
  if (!cfile->is_loaded) mainw->cancelled=CANCEL_NO_PROPOGATE;

  if (mainw->playing_file>-1) {
    mainw->pre_src_file=newclip;
    mainw->new_clip=newclip;
  }
  else {
    switch_to_file (mainw->current_file,newclip);
  }
}


void switch_clip_activate (GtkMenuItem *menuitem, gpointer user_data) {
  // switch clips from the clips menu

  register int i;
  if (mainw->current_file<1||mainw->preview||(mainw->is_processing&&cfile->is_loaded)||mainw->cliplist==NULL) return;

  for (i=1;i<MAX_FILES;i++) {
    if (!(mainw->files[i]==NULL)) {
      if (LIVES_MENU_ITEM(menuitem)==LIVES_MENU_ITEM(mainw->files[i]->menuentry)&&lives_check_menu_item_get_active(LIVES_CHECK_MENU_ITEM(mainw->files[i]->menuentry))) {
	if (!(i==mainw->current_file)) {
	  switch_clip(0,i);
	}
	return;
      }
    }
  }
}



void
on_about_activate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2,14,0)
  gchar *license = g_strdup(_(
        "This program is free software; you can redistribute it and/or modify\n"
        "it under the terms of the GNU General Public License as published by\n"
        "the Free Software Foundation; either version 3 of the License, or\n"
        "(at your option) any later version.\n"
        "\n"
        "This program is distributed in the hope that it will be useful,\n"
        "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
        "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
        "GNU General Public License for more details.\n"
        "\n"
        "You should have received a copy of the GNU General Public License\n"
        "along with this program; if not, write to the Free Software\n"
        "Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA.\n"));

  gchar *comments= g_strdup(_("A video editor and VJ program."));
  gchar *title= g_strdup(_("About LiVES"));

  gchar *translator_credits = g_strdup(_("translator_credits"));

#if GTK_CHECK_VERSION(3,0,0)
  gchar *authors[2]={"salsaman@gmail.com",NULL};
#else
  gtk_about_dialog_set_url_hook (activate_url, NULL, NULL);
  gtk_about_dialog_set_email_hook (activate_url, NULL, NULL);
#endif

  gtk_show_about_dialog (LIVES_WINDOW(mainw->LiVES),
			 "logo", NULL,
			 "name", "LiVES",
			 "version", LiVES_VERSION,
			 "comments",comments,
			 "copyright", "(C) 2002-2014 salsaman <salsaman@gmail.com> and others",
			 "website", "http://lives.sourceforge.net",
			 "license", license,
			 "title", title,
			 "translator_credits", translator_credits,
#if GTK_CHECK_VERSION(3,0,0)
			 "authors", authors,
			 "license-type", GTK_LICENSE_GPL_3_0,
#endif
			 NULL);

  g_free(translator_credits);
  g_free(comments);
  g_free(title);
  g_free(license);
  return;
#endif
#endif

  gchar *mesg;
  mesg=g_strdup_printf(_ ("LiVES Version %s\n(c) G. Finch (salsaman) %s\n\nReleased under the GPL 3 or later (http://www.gnu.org/licenses/gpl.txt)\nLiVES is distributed WITHOUT WARRANTY\n\nContact the author at:\nsalsaman@gmail.com\nHomepage: http://lives.sourceforge.net"),LiVES_VERSION,"2002-2014");
  do_error_dialog(mesg);
  g_free(mesg);
  
}



void
show_manual_activate                     (GtkMenuItem     *menuitem,
					  gpointer         user_data)
{
  show_manual_section(NULL,NULL);
}



void
email_author_activate                     (GtkMenuItem     *menuitem,
					  gpointer         user_data)
{
  activate_url_inner(LIVES_AUTHOR_EMAIL);
}


void
report_bug_activate                     (GtkMenuItem     *menuitem,
					  gpointer         user_data)
{
  activate_url_inner(LIVES_BUG_URL);
}


void
suggest_feature_activate                     (GtkMenuItem     *menuitem,
					      gpointer         user_data)
{
  activate_url_inner(LIVES_FEATURE_URL);
}


void
help_translate_activate                     (GtkMenuItem     *menuitem,
					      gpointer         user_data)
{
  activate_url_inner(LIVES_TRANSLATE_URL);
}



void
donate_activate                     (GtkMenuItem     *menuitem,
				     gpointer         user_data)
{
  const gchar *link=g_strdup_printf("%s%s",LIVES_DONATE_URL,user_data==NULL?"":(gchar *)user_data);
  activate_url_inner(link);
}



void on_fs_preview_clicked (GtkWidget *widget, gpointer user_data) {
  // file selector preview
  double start_time=0.;

  uint64_t xwin=0;

  FILE *ifile=NULL;

  lives_painter_t *cr;

  LiVESPixbuf *blank;

  gchar **array;

  int preview_frames=1000000000;
  int preview_type=GPOINTER_TO_INT (user_data);

  int height=0,width=0;
  int fwidth,fheight,owidth,oheight;

  int offs_x,offs_y;
  pid_t pid=capable->mainpid;
  int alarm_handle;
  int retval;
  boolean timeout;

  gchar *info_file,*thm_dir;
  gchar *file_open_params=NULL;
  gchar *tmp,*tmp2;
  gchar *com;
  gchar *dfile;

  if (mainw->in_fs_preview) {
    end_fs_preview();

#ifndef IS_MINGW
    com=g_strdup_printf ("%s stopsubsub thm%d 2>/dev/null",prefs->backend_sync,pid);
    lives_system (com,TRUE);
#else
    // get pid from backend
    FILE *rfile;
    ssize_t rlen;
    char val[16];
    int xpid;
    com=g_strdup_printf("%s get_pid_for_handle thm%d",prefs->backend_sync,pid);
    rfile=popen(com,"r");
    rlen=fread(val,1,16,rfile);
    pclose(rfile);
    memset(val+rlen,0,1);
    xpid=atoi(val);
    if (xpid!=0) 
      lives_win32_kill_subprocesses(xpid,TRUE);
#endif
    g_free (com);

    lives_widget_context_update();
  }

  if (preview_type==3) {
    // open selection
    start_time=mainw->fx1_val;
    preview_frames=(int)mainw->fx2_val;
  }
  else {
    // open file
    g_snprintf(file_name,PATH_MAX,"%s",
	       (tmp=g_filename_to_utf8((tmp2=lives_file_chooser_get_filename (LIVES_FILE_CHOOSER(lives_widget_get_toplevel(widget)))),
				       -1,NULL,NULL,NULL)));
    g_free(tmp); g_free(tmp2);
    
  }


#ifndef IS_MINGW
  info_file=g_strdup_printf ("%s/thm%d/.status",prefs->tmpdir,capable->mainpid);
#else
  info_file=g_strdup_printf ("%s/thm%d/status",prefs->tmpdir,capable->mainpid);
#endif
  unlink (info_file);

  if (preview_type==1) {

    preview_frames=1000000000;

    clear_mainw_msg();
    
    if (capable->has_identify) {
      mainw->error=FALSE;

      // make thumb from any image file
      com=g_strdup_printf("%s make_thumb thm%d %d %d \"%s\" \"%s\"",prefs->backend_sync,pid,DEFAULT_FRAME_HSIZE,
			  DEFAULT_FRAME_VSIZE,prefs->image_ext,(tmp=g_filename_from_utf8(file_name,-1,NULL,NULL,NULL)));
      g_free(tmp);
      lives_system(com,FALSE);
      g_free(com);
      
#define LIVES_FILE_READ_TIMEOUT  (10 * U_SEC) // 5 sec timeout
      
      do {
	retval=0;
	timeout=FALSE;
	alarm_handle=lives_alarm_set(LIVES_FILE_READ_TIMEOUT);
	
	while (!((ifile=fopen (info_file,"r")) || (timeout=lives_alarm_get(alarm_handle)))) {
	  lives_widget_context_update();
	  g_usleep (prefs->sleep_time);
	}
	
	lives_alarm_clear(alarm_handle);
	
	if (!timeout) {
	  mainw->read_failed=FALSE;
	  lives_fgets(mainw->msg,512,ifile);
	  fclose (ifile);
	}
	
	if (timeout || mainw->read_failed) {
	  retval=do_read_failed_error_s_with_retry(info_file,NULL,NULL);
	}
	
      } while (retval==LIVES_RETRY);
      
      if (retval==LIVES_CANCEL) {
	mainw->read_failed=FALSE;
	return;
      }
      
      if (!mainw->error) {
	array=g_strsplit(mainw->msg,"|",3);
	width=atoi(array[1]);
	height=atoi(array[2]);
	g_strfreev(array);
      }
      else height=width=0;
      mainw->error=FALSE;
    }
    else {
      height=width=0;
    }

    if (height*width) {
      // draw image
      GError *error=NULL;
      gchar *thumb=g_strdup_printf("%s/thm%d/%08d.%s",prefs->tmpdir,pid,1,prefs->image_ext);
      LiVESPixbuf *pixbuf=lives_pixbuf_new_from_file((tmp=g_filename_from_utf8(thumb,-1,NULL,NULL,NULL)),&error);
      g_free(tmp);

      if (error==NULL) {
	fwidth=lives_widget_get_allocation_width(mainw->fs_playalign)-20;
	fheight=lives_widget_get_allocation_height(mainw->fs_playalign)-20;

	owidth=width=lives_pixbuf_get_width(pixbuf);
	oheight=height=lives_pixbuf_get_height(pixbuf);

	calc_maxspect(fwidth,fheight,
		      &width,&height);
	
	width=(width>>1)<<1;
	height=(height>>1)<<1;

	if (width>owidth && height>oheight) {
	  width=owidth;
	  height=oheight;
	}

	lives_alignment_set(LIVES_ALIGNMENT(mainw->fs_playalign),0.5,
			  0.5,1.,1.);

	lives_widget_context_update();

	blank=lives_pixbuf_new_blank(fwidth+20,fheight+20,WEED_PALETTE_RGB24);

	cr = lives_painter_create_from_widget (mainw->fs_playarea);
	lives_painter_set_source_pixbuf (cr, blank, 0, 0);
	lives_painter_paint (cr);
	lives_painter_destroy (cr);
	lives_object_unref(blank);

	lives_widget_context_update();
	

	offs_x=(fwidth-width)/2;
	offs_y=(fheight-height)/2;

	cr = lives_painter_create_from_widget (mainw->fs_playarea);
	lives_painter_set_source_pixbuf (cr, pixbuf, offs_x, offs_y);
	lives_painter_paint (cr);
	lives_painter_destroy (cr);
      }	
      else {
	g_error_free(error);
      }
      if (pixbuf!=NULL) {
	lives_object_unref(pixbuf);
      }
      g_free(thumb);
    }
    g_free(info_file);

    thm_dir=g_strdup_printf ("%s/thm%d",prefs->tmpdir,capable->mainpid);
#ifndef IS_MINGW
    com=g_strdup_printf("/bin/rm -rf \"%s\"",thm_dir);
#else
    com=g_strdup_printf("DEL /q \"%s\"",thm_dir);
    lives_system(com,TRUE);
    g_free(com);
    com=g_strdup_printf("RMDIR \"%s\"",thm_dir);
#endif
    lives_system(com,TRUE);
    g_free(com);
    g_free(thm_dir);
  } 


  if (!(height*width)) {
    // media preview
    if (!capable->has_mplayer&&!capable->has_mplayer2) {
      gchar *msg;
      if (capable->has_identify) {
	msg=g_strdup(_ ("\n\nYou need to install mplayer or mplayer2 to be able to preview this file.\n"));
      }
      else {
	msg=g_strdup(_ ("\n\nYou need to install mplayer, mplayer2 or imageMagick to be able to preview this file.\n"));
      }
      do_blocking_error_dialog(msg);
      g_free(msg);
      return;
    }

#ifndef IS_MINGW
    dfile=g_strdup_printf("%s/fsp%d/",prefs->tmpdir,capable->mainpid);
#else
    dfile=g_strdup_printf("%s\\fsp%d\\",prefs->tmpdir,capable->mainpid);
#endif

    g_mkdir_with_parents(dfile,S_IRWXU);

#ifndef IS_MINGW
    info_file=g_strdup_printf ("%s.status",dfile);
#else
    info_file=g_strdup_printf ("%sstatus",dfile);
#endif

    g_free(dfile);


    lives_widget_set_bg_color (mainw->fs_playarea, LIVES_WIDGET_STATE_NORMAL, &palette->black);
    lives_widget_set_bg_color (mainw->fs_playframe, LIVES_WIDGET_STATE_NORMAL, &palette->black);
    lives_widget_set_bg_color (mainw->fs_playalign, LIVES_WIDGET_STATE_NORMAL, &palette->black);

    mainw->in_fs_preview=TRUE;

    // get width and height of clip
    com=g_strdup_printf("%s get_details fsp%d \"%s\" \"%s\" %d %d",prefs->backend,capable->mainpid,
			(tmp=g_filename_from_utf8(file_name,-1,NULL,NULL,NULL)),
			prefs->image_ext,FALSE,FALSE);


    mainw->com_failed=FALSE;
    lives_system(com,FALSE);
    g_free(com);
    
    if (mainw->com_failed) {
      end_fs_preview();
      g_free(info_file);
      return;
    }


    do {
      retval=0;
      timeout=FALSE;
      clear_mainw_msg();
      
#define LIVES_LONGER_TIMEOUT  (10 * U_SEC) // 10 second timeout

      alarm_handle=lives_alarm_set(LIVES_LONGER_TIMEOUT);
    
      while (!((ifile=fopen(info_file,"r")) || (timeout=lives_alarm_get(alarm_handle)))
	     &&mainw->in_fs_preview) {
	lives_widget_context_update();
	threaded_dialog_spin();
	g_usleep(prefs->sleep_time);
      }
      
      lives_alarm_clear(alarm_handle);
    
      if (!mainw->in_fs_preview) {
	// user cancelled
	end_fs_preview();
	g_free(info_file);
	return;
      }

      if (!timeout) {
	mainw->read_failed=FALSE;
	lives_fgets(mainw->msg,512,ifile);
	fclose(ifile);
      }

      if (timeout||mainw->read_failed) {
	retval=do_read_failed_error_s_with_retry(info_file,NULL,NULL);
      }
    } while (retval==LIVES_RETRY);
    
    if (mainw->msg!=NULL&&get_token_count(mainw->msg,'|')>6) {
      array=g_strsplit(mainw->msg,"|",-1);
      width=atoi(array[4]);
      height=atoi(array[5]);
      g_strfreev(array);
    }
    else {
      width=DEFAULT_FRAME_HSIZE;
      height=DEFAULT_FRAME_VSIZE;
    }

    unlink(info_file);

    if (preview_type!=2) {

      owidth=width;
      oheight=height;
      
      // -20 since border width was set to 10 pixels
      fwidth=lives_widget_get_allocation_width(mainw->fs_playalign)-20;
      fheight=lives_widget_get_allocation_height(mainw->fs_playalign)-20;
      
      calc_maxspect(fwidth,fheight,
		    &width,&height);
      
      width=(width>>1)<<1;
      height=(height>>1)<<1;
      
      if (width>owidth && height>oheight) {
	width=owidth;
	height=oheight;
      }
      
      lives_alignment_set(LIVES_ALIGNMENT(mainw->fs_playalign),0.5,
			0.5,0.,
			(float)height/(float)fheight);
      
      
      lives_widget_context_update();
      
      blank=lives_pixbuf_new_blank(fwidth+20,fheight+20,WEED_PALETTE_RGB24);
      
      cr = lives_painter_create_from_widget (mainw->fs_playarea);
      lives_painter_set_source_pixbuf (cr, blank, 0, 0);
      lives_painter_paint (cr);
      lives_painter_destroy (cr);
      lives_object_unref(blank);
      
      lives_widget_context_update();
    }

    if (prefs->audio_player==AUD_PLAYER_JACK) {
      file_open_params=g_strdup_printf("%s %s -ao jack",mainw->file_open_params!=NULL?
				       mainw->file_open_params:"",get_deinterlace_string());
    }
    else if (prefs->audio_player==AUD_PLAYER_PULSE) {
      file_open_params=g_strdup_printf("%s %s -ao pulse",mainw->file_open_params!=NULL?
				       mainw->file_open_params:"",get_deinterlace_string());
    }
    else {
      file_open_params=g_strdup_printf("%s %s -ao null",mainw->file_open_params!=NULL?
				       mainw->file_open_params:"",get_deinterlace_string());
    }


    if (preview_type==1||preview_type==3) {
      xwin=lives_widget_get_xwinid(mainw->fs_playarea,"Unsupported display type for preview.");
      if (xwin==-1) {
	end_fs_preview();
	g_free(info_file);
	return;
      }
    }



    if (file_open_params!=NULL) {
      com=g_strdup_printf("%s fs_preview fsp%d %"PRIu64" %d %d %.2f %d \"%s\" \"%s\"",prefs->backend,capable->mainpid,
			  xwin, width, height, start_time, preview_frames,
			  (tmp=g_filename_from_utf8(file_name,-1,NULL,NULL,NULL)),file_open_params);

    }
    else {
      com=g_strdup_printf("%s fs_preview fsp%d %"PRIu64" %d %d %.2f %d \"%s\"",prefs->backend,capable->mainpid,
			  xwin, width, height, start_time, preview_frames,
			  (tmp=g_filename_from_utf8(file_name,-1,NULL,NULL,NULL)));
    }
    
    g_free(tmp);

    if (preview_type!=2) lives_widget_set_app_paintable(mainw->fs_playarea,TRUE);
    
    mainw->com_failed=FALSE;

    lives_system(com,FALSE);
    g_free(com);
    
    if (mainw->com_failed) {
      end_fs_preview();
      g_free(info_file);
      return;
    }

    // loop here until preview has finished, or the user cancels

    while ((!(ifile=fopen (info_file,"r")))&&mainw->in_fs_preview) {
      lives_widget_context_update();
      g_usleep (prefs->sleep_time);
    }

    if (ifile!=NULL) {
      fclose(ifile);
    }

    end_fs_preview();
    g_free(info_file);
  }
  if (file_open_params!=NULL) g_free(file_open_params);
}




void on_open_activate (GtkMenuItem *menuitem, gpointer user_data) {
  // OPEN A FILE

  if (mainw->multitrack!=NULL) {
    if (mainw->multitrack->idlefunc>0) {
      g_source_remove(mainw->multitrack->idlefunc);
      mainw->multitrack->idlefunc=0;
    }
    mt_desensitise(mainw->multitrack);
    lives_widget_set_sensitive(mainw->multitrack->playall,TRUE);
    lives_widget_set_sensitive (mainw->m_playbutton, TRUE);
  }

  choose_file_with_preview(strlen(mainw->vid_load_dir)?mainw->vid_load_dir:NULL,NULL,3);
}




void on_ok_file_open_clicked(GtkFileChooser *chooser, GSList *fnames) {
  GSList *ofnames;
  gchar *tmp;

  if (chooser!=NULL) {
    fnames=lives_file_chooser_get_filenames (chooser);

    end_fs_preview();
    lives_widget_destroy(LIVES_WIDGET(chooser));

    if (fnames==NULL) return;

    if (fnames->data==NULL) {
      g_list_free_strings((GList *)fnames);
      g_slist_free(fnames);
      return;
    }

    g_snprintf(mainw->vid_load_dir,PATH_MAX,"%s",(gchar *)fnames->data);
    get_dirname(mainw->vid_load_dir);

    if (mainw->multitrack==NULL) lives_widget_queue_draw(mainw->LiVES);
    else lives_widget_queue_draw(mainw->multitrack->window);
    lives_widget_context_update();
    
    mainw->fs_playarea=NULL;
    
    if (prefs->save_directories) {
      set_pref ("vid_load_dir",(tmp=g_filename_from_utf8(mainw->vid_load_dir,-1,NULL,NULL,NULL)));
      g_free(tmp);
    }
    
    mainw->cancelled=CANCEL_NONE;
  }

  ofnames=fnames;

  while (fnames!=NULL&&mainw->cancelled==CANCEL_NONE) {
    g_snprintf(file_name,PATH_MAX,"%s",(gchar *)fnames->data);
    g_free(fnames->data);
    open_file(file_name);
    fnames=fnames->next;
  }

  g_slist_free(ofnames);

  mainw->opening_multi=FALSE;
  mainw->img_concat_clip=-1;

  if (mainw->multitrack!=NULL) {
    polymorph(mainw->multitrack,POLY_NONE);
    polymorph(mainw->multitrack,POLY_CLIPS);
    mt_sensitise(mainw->multitrack);
    mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
  }

}



// files dragged onto target from outside - try to open them
void drag_from_outside(GtkWidget *widget, GdkDragContext *dcon, int x, int y, 
		       GtkSelectionData *data, guint info, guint time, gpointer user_data) {
  GSList *fnames=NULL;
#if GTK_CHECK_VERSION(3,0,0)
  gchar *filelist=(gchar *)gtk_selection_data_get_data(data);
#else
  gchar *filelist=(gchar *)data->data;
#endif
  gchar *nfilelist,**array;
  int nfiles,i;

  if (filelist==NULL) {
    gtk_drag_finish(dcon,FALSE,FALSE,time);
    return;
  }

  if (mainw->multitrack!=NULL&&widget==mainw->multitrack->window) {
    if (!lives_widget_is_sensitive(mainw->multitrack->open_menu)) {
      gtk_drag_finish(dcon,FALSE,FALSE,time);
      return;
    }
  }
  else {
    if (!lives_widget_is_sensitive(mainw->open)) {
      gtk_drag_finish(dcon,FALSE,FALSE,time);
      return;
    }
  }

  nfilelist=subst(filelist,"file://","");

  nfiles=get_token_count(nfilelist,'\n');
  array=g_strsplit(nfilelist,"\n",nfiles);
  g_free(nfilelist);

  for (i=0;i<nfiles;i++) {
    fnames=g_slist_append(fnames,array[i]);
  }

  on_ok_file_open_clicked(NULL,fnames);

  // fn will free array elements and fnames

  g_free(array);

  gtk_drag_finish(dcon,TRUE,FALSE,time);
}




void on_opensel_range_ok_clicked (GtkButton *button, gpointer user_data) {
  // open file selection
  end_fs_preview();
  lives_general_button_clicked(button,NULL);

  mainw->fs_playarea=NULL;
  mainw->img_concat_clip=-1;
  open_file_sel(file_name,mainw->fx1_val,(int)mainw->fx2_val);

  if (mainw->multitrack!=NULL) {
    polymorph(mainw->multitrack,POLY_NONE);
    polymorph(mainw->multitrack,POLY_CLIPS);
    mt_sensitise(mainw->multitrack);
    mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
  }
}


void open_sel_range_activate(void) {
  // open selection range dialog

  GtkWidget *opensel_dialog = create_opensel_dialog ();
  lives_widget_show(opensel_dialog);
  mainw->fx1_val=0.;
  mainw->fx2_val=1000;

}


void on_open_new_audio_clicked (GtkFileChooser *chooser, gpointer user_data) {
  // open audio file
  gchar *a_type;
  gchar *com,*mesg,*tmp;
  gchar **array;

  int oundo_start;
  int oundo_end;
  int israw=1;
  int asigned,aendian;

  boolean has_lmap_error=FALSE;
  boolean bad_header=FALSE;
  boolean preparse=FALSE;

  if (!(prefs->warning_mask&WARN_MASK_LAYOUT_DELETE_AUDIO)) {
    if ((mainw->xlays=layout_audio_is_affected(mainw->current_file,0.))!=NULL) {
      if (!do_warning_dialog(_("\nLoading new audio may cause missing audio in some multitrack layouts.\nAre you sure you wish to continue ?\n."))) {
	g_list_free_strings(mainw->xlays);
	g_list_free(mainw->xlays);
	mainw->xlays=NULL;
	return;
      }
      add_lmap_error(LMAP_ERROR_DELETE_AUDIO,cfile->name,(gpointer)cfile->layout_map,mainw->current_file,0,0.,
		     cfile->stored_layout_frame>0.);
      has_lmap_error=TRUE;
      g_list_free_strings(mainw->xlays);
      g_list_free(mainw->xlays);
      mainw->xlays=NULL;
    }
  }

  if (!has_lmap_error&&!(prefs->warning_mask&WARN_MASK_LAYOUT_ALTER_AUDIO)&&
      (mainw->xlays=layout_audio_is_affected(mainw->current_file,0.))) {
    if (!do_layout_alter_audio_warning()) {
      g_list_free_strings(mainw->xlays);
      g_list_free(mainw->xlays);
      mainw->xlays=NULL;
      return;
    }
    add_lmap_error(LMAP_ERROR_ALTER_AUDIO,cfile->name,(gpointer)cfile->layout_map,mainw->current_file,0,0.,
		   cfile->stored_layout_audio>0.);
    has_lmap_error=TRUE;
    g_list_free_strings(mainw->xlays);
    g_list_free(mainw->xlays);
    mainw->xlays=NULL;
  }

  mainw->noswitch=TRUE;

  cfile->undo_arate=cfile->arate;
  cfile->undo_achans=cfile->achans;
  cfile->undo_asampsize=cfile->asampsize;
  cfile->undo_signed_endian=cfile->signed_endian;
  cfile->undo_arps=cfile->arps;

  oundo_start=cfile->undo_start;
  oundo_end=cfile->undo_end;

  if (user_data==NULL) {
    gchar *filename=lives_file_chooser_get_filename(chooser);
    g_snprintf(file_name,PATH_MAX,"%s",(tmp=g_filename_to_utf8(filename,-1,NULL,NULL,NULL)));
    g_free(filename);
    g_free(tmp);
  }
  else g_snprintf(file_name,PATH_MAX,"%s",(char *)user_data);

  g_snprintf(mainw->audio_dir,PATH_MAX,"%s",file_name);
  get_dirname(mainw->audio_dir);
  end_fs_preview();
  lives_widget_destroy(LIVES_WIDGET(chooser));
  lives_widget_context_update();
  mainw->fs_playarea=NULL;

  a_type=file_name+strlen(file_name)-3;

  if (!g_ascii_strncasecmp(a_type,".it",3)||
      !g_ascii_strncasecmp(a_type,"mp3",3)||
      !g_ascii_strncasecmp(a_type,"ogg",3)||
      !g_ascii_strncasecmp(a_type,"wav",3)||
      !g_ascii_strncasecmp(a_type,"mod",3)||
      !g_ascii_strncasecmp(a_type,".xm",3)) {
    com=g_strdup_printf("%s audioopen \"%s\" \"%s\"",prefs->backend,cfile->handle,
			(tmp=g_filename_from_utf8(file_name,-1,NULL,NULL,NULL)));
    g_free(tmp);
  }
  else {
    do_audio_import_error();
    mainw->noswitch=FALSE;
    return;
  }

  if (!g_ascii_strncasecmp(a_type,"wav",3)) israw=0;

  if (capable->has_mplayer||capable->has_mplayer2) {
    if (read_file_details (file_name,TRUE)) {
      array=g_strsplit(mainw->msg,"|",15);
      cfile->arate=atoi(array[9]);
      cfile->achans=atoi(array[10]);
      cfile->asampsize=atoi(array[11]);
      cfile->signed_endian=get_signed_endian(atoi (array[12]), atoi (array[13]));
      g_strfreev(array);
      preparse=TRUE;
    }
  }

  if (!preparse) {
    // TODO !!! - need some way to identify audio without invoking mplayer
    cfile->arate=cfile->arps=DEFAULT_AUDIO_RATE;
    cfile->achans=DEFAULT_AUDIO_CHANS;
    cfile->asampsize=DEFAULT_AUDIO_SAMPS;
    cfile->signed_endian=mainw->endian;
  }

  if (cfile->undo_arate>0) cfile->arps=cfile->undo_arps/cfile->undo_arate*cfile->arate;
  else cfile->arps=cfile->arate;

  mesg=g_strdup_printf(_ ("Opening audio %s, type %s..."),file_name,a_type);
  d_print(""); // force switchtext
  d_print(mesg);
  g_free(mesg);

  unlink(cfile->info_file);
  mainw->com_failed=FALSE;
  lives_system(com,FALSE);
  g_free(com);

  if (mainw->com_failed) {
    cfile->arate=cfile->undo_arate;
    cfile->achans=cfile->undo_achans;
    cfile->asampsize=cfile->undo_asampsize;
    cfile->signed_endian=cfile->undo_signed_endian;
    cfile->arps=cfile->undo_arps;
    cfile->undo_start=oundo_start;
    cfile->undo_end=oundo_end;
    sensitize();
    reget_afilesize(mainw->current_file);
    get_play_times();
    mainw->noswitch=FALSE;
    return;
  }

  cfile->opening=cfile->opening_audio=cfile->opening_only_audio=TRUE;

  cfile->undo_start=1;
  cfile->undo_end=cfile->frames;
  
  // show audio [opening...] in main window
  get_play_times();

  if (!(do_progress_dialog(TRUE,TRUE,_ ("Opening audio")))) {
    lives_widget_queue_draw(mainw->LiVES);
    lives_widget_context_update();
    mainw->cancelled=CANCEL_NONE;
    mainw->error=FALSE;
    mainw->com_failed=FALSE;
    unlink (cfile->info_file);
    com=g_strdup_printf("%s cancel_audio \"%s\"",prefs->backend,cfile->handle);
    lives_system(com,FALSE);
    do_auto_dialog(_("Cancelling"),0);
    g_free(com);
    cfile->opening_audio=cfile->opening=cfile->opening_only_audio=FALSE;
    cfile->arate=cfile->undo_arate;
    cfile->achans=cfile->undo_achans;
    cfile->asampsize=cfile->undo_asampsize;
    cfile->signed_endian=cfile->undo_signed_endian;
    cfile->arps=cfile->undo_arps;
    cfile->undo_start=oundo_start;
    cfile->undo_end=oundo_end;
    sensitize();
    reget_afilesize(mainw->current_file);
    get_play_times();
    mainw->noswitch=FALSE;
    if (mainw->error) d_print_failed();
    return;
  }

  cfile->opening_audio=cfile->opening=cfile->opening_only_audio=FALSE;

  lives_widget_queue_draw(mainw->LiVES);
  lives_widget_context_update();

  cfile->afilesize=0;
  if (get_token_count(mainw->msg,'|')>6) {
    array=g_strsplit(mainw->msg,"|",7);
    cfile->arate=atoi(array[1]);
    cfile->achans=atoi(array[2]);
    cfile->asampsize=atoi(array[3]);
    cfile->signed_endian=get_signed_endian(atoi (array[4]), atoi (array[5]));
    cfile->afilesize=strtol(array[6],NULL,10);
    g_strfreev(array);
  }

  if (cfile->afilesize==0) {
    d_print_failed();
      
    mainw->cancelled=CANCEL_NONE;
    mainw->error=FALSE;
    unlink (cfile->info_file);

    com=g_strdup_printf("%s cancel_audio \"%s\"",prefs->backend,cfile->handle);

    mainw->com_failed=FALSE;
    lives_system(com,FALSE);
    g_free(com);

    if (!mainw->com_failed) do_auto_dialog(_("Cancelling"),0);

    cfile->arate=cfile->undo_arate;
    cfile->achans=cfile->undo_achans;
    cfile->asampsize=cfile->undo_asampsize;
    cfile->signed_endian=cfile->undo_signed_endian;
    cfile->arps=cfile->undo_arps;
    cfile->undo_start=oundo_start;
    cfile->undo_end=oundo_end;
    sensitize();
    reget_afilesize(mainw->current_file);
    get_play_times();
    mainw->noswitch=FALSE;
    return;
  }

  cfile->changed=TRUE;
  d_print_done();

  mesg=g_strdup_printf(P_("New audio: %d Hz %d channel %d bps\n","New audio: %d Hz %d channels %d bps\n",cfile->achans),
		       cfile->arate,cfile->achans,cfile->asampsize);
  d_print(mesg);
  g_free(mesg);

  mainw->com_failed=FALSE;
  mainw->cancelled=CANCEL_NONE;
  mainw->error=FALSE;
  unlink (cfile->info_file);

  com=g_strdup_printf("%s commit_audio \"%s\" %d",prefs->backend,cfile->handle,israw);
  lives_system(com,FALSE);
  g_free(com);

  if (mainw->com_failed) {
    cfile->arate=cfile->undo_arate;
    cfile->achans=cfile->undo_achans;
    cfile->asampsize=cfile->undo_asampsize;
    cfile->signed_endian=cfile->undo_signed_endian;
    cfile->arps=cfile->undo_arps;
    cfile->undo_start=oundo_start;
    cfile->undo_end=oundo_end;
    sensitize();
    reget_afilesize(mainw->current_file);
    get_play_times();
    mainw->noswitch=FALSE;
    return;
  }

  if (!do_auto_dialog(_("Committing audio"),0)) {
    //cfile->may_be_damaged=TRUE;
    d_print_failed();
    return;
  }


  if (prefs->save_directories) {
    set_pref ("audio_dir",mainw->audio_dir);
  }
  if (!prefs->conserve_space) {
    cfile->undo_action=UNDO_NEW_AUDIO;
    set_undoable (_("New Audio"),TRUE);
  }

  asigned=!(cfile->signed_endian&AFORM_UNSIGNED);
  aendian=cfile->signed_endian&AFORM_BIG_ENDIAN;

  save_clip_value(mainw->current_file,CLIP_DETAILS_ARATE,&cfile->arps);
  if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
  save_clip_value(mainw->current_file,CLIP_DETAILS_PB_ARATE,&cfile->arate);
  if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
  save_clip_value(mainw->current_file,CLIP_DETAILS_ACHANS,&cfile->achans);
  if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
  save_clip_value(mainw->current_file,CLIP_DETAILS_ASAMPS,&cfile->asampsize);
  if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
  save_clip_value(mainw->current_file,CLIP_DETAILS_AENDIAN,&aendian);
  if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
  save_clip_value(mainw->current_file,CLIP_DETAILS_ASIGNED,&asigned);
  if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;

  if (bad_header) do_header_write_error(mainw->current_file);

  switch_to_file (mainw->current_file,mainw->current_file);

  mainw->noswitch=FALSE;
}


void end_fs_preview(void) {
  // clean up if we were playing a preview - should be called from all callbacks 
  // where there is a possibility of fs preview still playing
  gchar *com;

  if (mainw->in_fs_preview) {
#ifndef IS_MINGW
    com=g_strdup_printf ("%s stopsubsub fsp%d 2>/dev/null",prefs->backend_sync,capable->mainpid);
    lives_system (com,TRUE);
#else
    // get pid from backend
    FILE *rfile;
    ssize_t rlen;
    char val[16];
    int pid;
    com=g_strdup_printf("%s get_pid_for_handle fsp%d",prefs->backend_sync,capable->mainpid);
    rfile=popen(com,"r");
    rlen=fread(val,1,16,rfile);
    pclose(rfile);
    memset(val+rlen,0,1);
    pid=atoi(val);
    
    lives_win32_kill_subprocesses(pid,TRUE);
#endif
    g_free (com);
    com=g_strdup_printf ("%s close fsp%d",prefs->backend,capable->mainpid);
    lives_system (com,TRUE);
    g_free (com);

    mainw->in_fs_preview=FALSE;

    if (mainw->fs_playarea!=NULL&&(LIVES_IS_WIDGET(mainw->fs_playarea))) {
      lives_widget_set_app_paintable(mainw->fs_playarea,FALSE);
    }
  }
}


void on_save_textview_clicked (GtkButton *button, gpointer user_data) {
  GtkTextView *textview=(GtkTextView *)user_data;
  gchar *filt[]={"*.txt",NULL};
  int fd;
  gchar *btext;
  gchar *save_file;

  lives_widget_hide(lives_widget_get_toplevel(LIVES_WIDGET(button)));
  lives_widget_context_update();

  save_file=choose_file(NULL,NULL,filt,LIVES_FILE_CHOOSER_ACTION_SAVE,NULL,NULL);

  if (save_file==NULL) {
    lives_widget_show(lives_widget_get_toplevel(LIVES_WIDGET(button)));
    return;
  }

#ifndef IS_MINGW
  if ((fd=creat(save_file,S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH))==-1) {
#else
  if ((fd=creat(save_file,S_IRUSR|S_IWUSR))==-1) {
#endif
    lives_widget_show(lives_widget_get_toplevel(LIVES_WIDGET(button)));
    do_write_failed_error_s(save_file,g_strerror(errno));
    g_free(save_file);
    return;
  }

  btext=text_view_get_text(textview);
  
  lives_general_button_clicked(button,NULL);

  mainw->write_failed=FALSE;
  lives_write(fd,btext,strlen(btext),FALSE);
  g_free(btext);
  
  close (fd);

  if (mainw->write_failed) {
    do_write_failed_error_s(save_file,g_strerror(errno));
  }
  else {
    gchar *msg=g_strdup_printf(_("Text was saved as\n%s\n"),save_file);
    do_blocking_error_dialog(msg);
    g_free(msg);
  }

  g_free(save_file);

}




void on_cancel_button1_clicked (GtkWidget *widget, gpointer user_data) {
  // generic cancel callback

  end_fs_preview();

  if (LIVES_IS_BUTTON(widget)) 
    lives_general_button_clicked(LIVES_BUTTON(widget),user_data);
  else {
    lives_widget_destroy(widget);
    if (user_data!=NULL) g_free(user_data);
  }

  mainw->fs_playarea=NULL;
 
  if (mainw->multitrack!=NULL) {
    mt_sensitise(mainw->multitrack);
    mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
  }
  else {
    if (mainw->current_file>0) {
      if (!cfile->opening) {
	get_play_times();
      }
    }
  }
}




void on_cancel_opensel_clicked (GtkButton  *button, gpointer user_data) {
  end_fs_preview();
  lives_general_button_clicked(button,NULL);
  mainw->fs_playarea=NULL;

  if (mainw->multitrack!=NULL) {
    mt_sensitise(mainw->multitrack);
    mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
  }

}



void on_cancel_keep_button_clicked (GtkButton *button, gpointer user_data) {
  // Cancel/Keep from progress dialog
  gchar *com=NULL;
  guint keep_frames=0;
  FILE *infofile;

  if (cfile->opening&&mainw->effects_paused) {
    on_stop_clicked(NULL,NULL);
    return;
  }

  clear_mainw_msg();

  if (mainw->current_file>-1&&cfile!=NULL&&cfile->proc_ptr!=NULL) {
    lives_widget_set_sensitive(cfile->proc_ptr->cancel_button,FALSE);
    lives_widget_set_sensitive(cfile->proc_ptr->pause_button,FALSE);
    lives_widget_set_sensitive(cfile->proc_ptr->stop_button,FALSE);
    lives_widget_set_sensitive(cfile->proc_ptr->preview_button,FALSE);
  }
  lives_widget_context_update();

  if ((!mainw->effects_paused||cfile->nokeep)&&(!mainw->is_rendering||
						(mainw->multitrack!=NULL&&(!mainw->multitrack->is_rendering||
									   !mainw->preview)))) {
    // Cancel
    if (mainw->cancel_type==CANCEL_SOFT) {
      // cancel in record audio
      mainw->cancelled=CANCEL_USER;
      d_print_cancelled();
      return;
    }
    else if (mainw->cancel_type==CANCEL_KILL) {
      // kill processes and subprocesses working on cfile
#ifndef IS_MINGW
      com=g_strdup_printf("%s stopsubsub \"%s\" 2>/dev/null",prefs->backend_sync,cfile->handle);
#else
      com=g_strdup_printf("%s get_pid_for_handle \"%s\"",prefs->backend_sync,cfile->handle);
#endif
    }

    if (!cfile->opening&&!mainw->internal_messaging) {
      // if we are opening, this is 'stop' in the preview, so don't cancel
      // otherwise, come here

      // kill off the background process
      if (com!=NULL) {
#ifndef IS_MINGW
	lives_system(com,TRUE);
#else
	// get pid from backend
	FILE *rfile;
	ssize_t rlen;
	char val[16];
	int pid;
	
	rfile=popen(com,"r");
	rlen=fread(val,1,16,rfile);
	pclose(rfile);
	memset(val+rlen,0,1);
	g_free(com);
	com=NULL;
	pid=atoi(val);
	
	lives_win32_kill_subprocesses(pid,TRUE);
#endif
      }

      // resume for next time
      if (mainw->effects_paused) {
	if (com!=NULL) g_free(com);
	com=g_strdup_printf("%s resume \"%s\"",prefs->backend_sync,cfile->handle);
	lives_system(com,FALSE);
      }

    }

    mainw->cancelled=CANCEL_USER;

    if (mainw->is_rendering) {
      cfile->frames=0;
      d_print_cancelled();
    }
    else {
      // see if there was a message from backend

      if (mainw->cancel_type!=CANCEL_SOFT) {
	if ((infofile=fopen(cfile->info_file,"r"))>0) {
	  mainw->read_failed=FALSE;
	  lives_fgets(mainw->msg,512,infofile);
	  fclose(infofile);
	}
	
	if (strncmp (mainw->msg,"completed",9)) {
	  d_print_cancelled();
	}
	else {
	  // processing finished before we could cancel
	  mainw->cancelled=CANCEL_NONE;
	}
      }
      else d_print_cancelled();
    }
  }
  else {
    // Keep
    if (mainw->cancel_type==CANCEL_SOFT) {
      mainw->cancelled=CANCEL_KEEP;
      return;
    }
    if (!mainw->is_rendering) {
      keep_frames=cfile->proc_ptr->frames_done-cfile->progress_start+cfile->start-1+mainw->internal_messaging*2;
      if (mainw->internal_messaging && atoi(mainw->msg)>cfile->proc_ptr->frames_done) 
	keep_frames=atoi(mainw->msg)-cfile->progress_start+cfile->start-1+2;
    }
    else keep_frames=cfile->frames+1;
    if (keep_frames>mainw->internal_messaging) {
      gchar *msg=g_strdup_printf(_ ("%d frames are enough !\n"),keep_frames-cfile->start);
      d_print(msg);
      g_free(msg);

      lives_set_cursor_style(LIVES_CURSOR_BUSY,NULL);
      if (!mainw->internal_messaging) {
#ifndef IS_MINGW
	com=g_strdup_printf("%s stopsubsub \"%s\" 2>/dev/null",prefs->backend_sync,cfile->handle);
	lives_system(com,TRUE);
#else
	// get pid from backend
	FILE *rfile;
	ssize_t rlen;
	char val[16];
	int pid;
	com=g_strdup_printf("%s get_pid_for_handle \"%s\"",prefs->backend_sync,cfile->handle);
	rfile=popen(com,"r");
	rlen=fread(val,1,16,rfile);
	pclose(rfile);
	memset(val+rlen,0,1);
	pid=atoi(val);
	
	lives_win32_kill_subprocesses(pid,TRUE);
#endif
	g_free(com);

	com=g_strdup_printf("%s resume \"%s\"",prefs->backend_sync,cfile->handle);
	lives_system(com,FALSE);
	g_free(com);
	if (!mainw->keep_pre) com=g_strdup_printf("%s mv_mgk \"%s\" %d %d \"%s\"",prefs->backend,cfile->handle,
						  cfile->start,keep_frames-1,get_image_ext_for_type(cfile->img_type));
	else {
	  com=g_strdup_printf("%s mv_pre \"%s\" %d %d \"%s\" &",prefs->backend_sync,cfile->handle,
			      cfile->start,keep_frames-1,get_image_ext_for_type(cfile->img_type));
	  mainw->keep_pre=FALSE;
	}
      }
      else {
	mainw->internal_messaging=FALSE;
	if (!mainw->keep_pre) com=g_strdup_printf ("%s mv_mgk \"%s\" %d %d \"%s\"",prefs->backend,cfile->handle,
						   cfile->start,keep_frames-1,get_image_ext_for_type(cfile->img_type));
	else {
	  com=g_strdup_printf("%s mv_pre \"%s\" %d %d \"%s\" &",prefs->backend_sync,cfile->handle,
			      cfile->start,keep_frames-1,get_image_ext_for_type(cfile->img_type));
	  mainw->keep_pre=FALSE;
	}
      }
      if (!mainw->is_rendering||mainw->multitrack!=NULL) {
	unlink(cfile->info_file);
	lives_system(com,FALSE);
	cfile->undo_end=keep_frames-1;
      }
      else mainw->cancelled=CANCEL_KEEP;
      lives_set_cursor_style(LIVES_CURSOR_NORMAL,NULL);
    }
    else {
      // no frames there, nothing to keep
      d_print_cancelled();

#ifndef IS_MINGW
      com=g_strdup_printf("%s stopsubsub \"%s\" 2>/dev/null",prefs->backend_sync,cfile->handle);
#else
      com=g_strdup_printf("%s get_pid_for_handle \"%s\"",prefs->backend_sync,cfile->handle);
#endif
      if (!mainw->internal_messaging&&!mainw->is_rendering) {

#ifndef IS_MINGW
	lives_system(com,TRUE);
#else
	// get pid from backend
	FILE *rfile;
	ssize_t rlen;
	char val[16];
	int pid;
	rfile=popen(com,"r");
	rlen=fread(val,1,16,rfile);
	pclose(rfile);
	memset(val+rlen,0,1);
	pid=atoi(val);
	
	lives_win32_kill_subprocesses(pid,TRUE);
#endif
	g_free(com);
	
	com=g_strdup_printf("%s resume \"%s\"",prefs->backend_sync,cfile->handle);
	lives_system(com,FALSE);
      }
      mainw->cancelled=CANCEL_USER;
    }
  }

  if (com!=NULL) g_free(com);
}




void
on_details_button_clicked            (GtkButton       *button,
				      gpointer         user_data)
{
  text_window *textwindow;

  lives_general_button_clicked(button,NULL);

  g_object_ref(mainw->optextview);
  textwindow=create_text_window(_("LiVES: - Encoder debug output"),NULL,NULL);
  lives_widget_show_all(textwindow->dialog);
}


void on_full_screen_pressed (GtkButton *button,
			     gpointer user_data) {
  // toolbar button (full screen)
  lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->full_screen),!mainw->fs);
}


void on_full_screen_activate (GtkMenuItem *menuitem, gpointer user_data) {
  gchar buff[PATH_MAX];
  GtkWidget *fs_img;
  gchar *fnamex;
  gchar *title,*xtrabit;

  if (mainw->current_file>-1&&!cfile->frames&&mainw->multitrack==NULL) return;

  if (user_data==NULL) {
    mainw->fs=!mainw->fs;
  }

  if (mainw->current_file==-1) return;

  fnamex=g_build_filename(prefs->prefix_dir,ICON_DIR,"fullscreen.png",NULL);
  g_snprintf (buff,PATH_MAX,"%s",fnamex);
  g_free(fnamex);

  fs_img=lives_image_new_from_file (buff);
  lives_widget_show(fs_img);
  if (!mainw->fs) {
    if (g_file_test(buff,G_FILE_TEST_EXISTS)) {
      GdkPixbuf *pixbuf=lives_image_get_pixbuf(LIVES_IMAGE(fs_img));
      lives_pixbuf_saturate_and_pixelate(pixbuf,pixbuf,0.2,FALSE);
    }
    lives_widget_set_tooltip_text(mainw->t_fullscreen,_("Fullscreen playback (f)"));
  }
  else lives_widget_set_tooltip_text(mainw->t_fullscreen,_("Fullscreen playback off (f)"));

  lives_tool_button_set_icon_widget(LIVES_TOOL_BUTTON(mainw->t_fullscreen),fs_img);

  if (mainw->playing_file>-1){
    if (mainw->fs) {
      // switch TO full screen during pb
      if (mainw->multitrack==NULL&&(!mainw->sep_win||prefs->play_monitor==prefs->gui_monitor)&&
	  !(mainw->vpp!=NULL&&!(mainw->vpp->capabilities&VPP_LOCAL_DISPLAY)&&mainw->sep_win)) {
	if (!mainw->faded) {
	  fade_background();
	}
      
	fullscreen_internal();
	lives_widget_hide(mainw->framebar);
      }
    
      if (mainw->sep_win) {
	if (prefs->sepwin_type==SEPWIN_TYPE_STICKY) {
	  resize_play_window();
	}
	else {
	  kill_play_window();
	  make_play_window();
	  if (mainw->play_window!=NULL) {
	    hide_cursor(lives_widget_get_xwindow(mainw->play_window));
	    lives_widget_set_app_paintable(mainw->play_window,TRUE);
	  }
	}
	if (cfile->frames==1||cfile->play_paused) {
	  lives_widget_context_update();
	}
      }

      if (mainw->ext_playback&&mainw->vpp->fheight>-1&&mainw->vpp->fwidth>-1) {	  
	// fixed o/p size for stream
	if (!(mainw->vpp->fwidth*mainw->vpp->fheight)) {
	  mainw->vpp->fwidth=cfile->hsize;
	  mainw->vpp->fheight=cfile->vsize;
	}
	mainw->pwidth=mainw->vpp->fwidth;
	mainw->pheight=mainw->vpp->fheight;
      }
      if ((cfile->frames==1||cfile->play_paused)&&!mainw->noswitch&&(cfile->clip_type==CLIP_TYPE_DISK||
								     cfile->clip_type==CLIP_TYPE_FILE)) {
	weed_plant_t *frame_layer=mainw->frame_layer;
	mainw->frame_layer=NULL;
	load_frame_image (cfile->frameno);
	mainw->frame_layer=frame_layer;
      }
    } else {
      if (mainw->multitrack==NULL) {
	// switch FROM fullscreen during pb
	lives_widget_show(mainw->frame1);
	lives_widget_show(mainw->frame2);
	lives_widget_show(mainw->eventbox3);
	lives_widget_show(mainw->eventbox4);
	
	if (prefs->show_framecount) {
	  lives_widget_show(mainw->framebar);
	}
	
	lives_container_set_border_width (LIVES_CONTAINER (mainw->playframe), widget_opts.border_width);
	
	lives_widget_set_sensitive(mainw->fade,TRUE);
	lives_widget_set_sensitive(mainw->dsize,TRUE);
	
	lives_widget_show(mainw->t_bckground);
	lives_widget_show(mainw->t_double);

	resize(1);
	if (mainw->multitrack==NULL) {
	  if (cfile->is_loaded) {
	    load_start_image (cfile->start);
	    load_end_image (cfile->end);
	  }
	  else {
	    load_start_image (0);
	    load_end_image (0);
	  }
	}
      }

      if (mainw->sep_win) {
	// separate window

	// multi monitors don't like this it seems, breaks the window
	lives_window_unfullscreen(LIVES_WINDOW(mainw->play_window));

	if (!mainw->faded) {
	  unfade_background();
	}

	if (mainw->ext_playback) {
	  vid_playback_plugin_exit();
	}

	resize_play_window();

	if (mainw->sepwin_scale!=100.) xtrabit=g_strdup_printf(_(" (%d %% scale)"),(int)mainw->sepwin_scale);
	else xtrabit=g_strdup("");
	title=g_strdup_printf(_("LiVES: - Play Window%s"),xtrabit);
	if (mainw->play_window!=NULL)
	  lives_window_set_title (LIVES_WINDOW (mainw->play_window), title);
	g_free(title);
	g_free(xtrabit);

	if (mainw->opwx>-1) {
	  //opwx and opwy were stored when we first switched to full screen
	  lives_window_move (LIVES_WINDOW (mainw->play_window), mainw->opwx, mainw->opwy);
	  mainw->opwx=-1;
	  mainw->opwy=-1;
	}
	else {
	  // non-sticky
	  kill_play_window();
	  make_play_window();
	  if (mainw->play_window!=NULL) {
	    hide_cursor(lives_widget_get_xwindow(mainw->play_window));
	    lives_widget_set_app_paintable(mainw->play_window,TRUE);
	  }
	}
	if (mainw->multitrack==NULL&&(cfile->frames==1||cfile->play_paused)) {
	  lives_widget_context_update();
	  if (mainw->play_window!=NULL&&!mainw->noswitch&&(cfile->clip_type==CLIP_TYPE_DISK||
							   cfile->clip_type==CLIP_TYPE_FILE)) {
	    weed_plant_t *frame_layer=mainw->frame_layer;
	    mainw->frame_layer=NULL;
	    load_frame_image (cfile->frameno);
	    mainw->frame_layer=frame_layer;
	  }
	}
      }
      else {
	if (mainw->multitrack==NULL) {
	  // in-frame window
	  lives_widget_context_update();
	  
	  mainw->pwidth=lives_widget_get_allocation_width(mainw->playframe)-H_RESIZE_ADJUST;
	  mainw->pheight=lives_widget_get_allocation_height(mainw->playframe)-V_RESIZE_ADJUST;
	  
	  // double size
	  if (mainw->double_size) {
	    resize(2);
	    mainw->pheight*=2;
	    mainw->pheight++;
	    mainw->pwidth*=2;
	    mainw->pwidth+=2;
	  }
	}
	if (!mainw->faded) {
	  unfade_background();
	}
	else {
	  gtk_frame_set_label(GTK_FRAME(mainw->playframe), "");
	}
      }
      if ((cfile->frames==1||cfile->play_paused)&&!mainw->noswitch&&mainw->multitrack==NULL&&
	  (cfile->clip_type==CLIP_TYPE_DISK||cfile->clip_type==CLIP_TYPE_FILE)) {
	weed_plant_t *frame_layer=mainw->frame_layer;
	mainw->frame_layer=NULL;
	load_frame_image (cfile->frameno);
	mainw->frame_layer=frame_layer;
      }
    }
  }
  else {
    if (mainw->multitrack==NULL) {
      if (mainw->playing_file==-1) {
	if (mainw->fs) {
	  lives_widget_set_sensitive(mainw->fade,FALSE);
	  lives_widget_set_sensitive(mainw->dsize,FALSE);
	} else {
	  lives_widget_set_sensitive(mainw->fade,TRUE);
	  lives_widget_set_sensitive(mainw->dsize,TRUE);
	}
      }
    }
  }
}


void on_double_size_pressed (GtkButton *button,
			     gpointer user_data) {
  // toolbar button (separate window)
  lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->dsize),!mainw->double_size);
}



void
on_double_size_activate               (GtkMenuItem     *menuitem,
				       gpointer         user_data)
{
  gchar buff[PATH_MAX];
  GtkWidget *sngl_img;
  gchar *fnamex;

  if (mainw->multitrack!=NULL||(mainw->current_file>-1&&cfile->frames==0&&user_data==NULL)) return;

  if (user_data==NULL) {
    mainw->double_size=!mainw->double_size;
  }

  if (mainw->current_file==-1) return;

  if (user_data!=NULL) {
    // change the blank window icons
    if (!mainw->double_size) {
      fnamex=g_build_filename(prefs->prefix_dir,ICON_DIR,"zoom-in.png",NULL);
      g_snprintf (buff,PATH_MAX,"%s",fnamex);
      g_free(fnamex);
      sngl_img=lives_image_new_from_file (buff);
      lives_widget_set_tooltip_text(mainw->t_double,_("Double size (d)"));
    }
    else {
      fnamex=g_build_filename(prefs->prefix_dir,ICON_DIR,"zoom-out.png",NULL);
      g_snprintf (buff,PATH_MAX,"%s",fnamex);
      g_free(fnamex);
      sngl_img=lives_image_new_from_file (buff);
      lives_widget_set_tooltip_text(mainw->t_double,_("Single size (d)"));
    }
    
    if (g_file_test(buff,G_FILE_TEST_EXISTS)) {
      GdkPixbuf *pixbuf=lives_image_get_pixbuf(LIVES_IMAGE(sngl_img));
      if (pixbuf!=NULL) lives_pixbuf_saturate_and_pixelate(pixbuf,pixbuf,0.2,FALSE);
    }
    
    lives_widget_show(sngl_img);
    lives_tool_button_set_icon_widget(LIVES_TOOL_BUTTON(mainw->t_double),sngl_img);
  }

  if (mainw->playing_file>-1&&!mainw->fs) {
    // needed
    block_expose();
    do {
      lives_widget_context_update();
      mainw->pwidth=lives_widget_get_allocation_width(mainw->playframe)-H_RESIZE_ADJUST;
      mainw->pheight=lives_widget_get_allocation_height(mainw->playframe)-V_RESIZE_ADJUST;
    } while (!(mainw->pwidth*mainw->pheight));
    unblock_expose();

    if (mainw->sep_win) {
      if (prefs->sepwin_type==SEPWIN_TYPE_STICKY) {
	gchar *title,*xtrabit;
	resize_play_window();

	if (mainw->sepwin_scale!=100.) xtrabit=g_strdup_printf(_(" (%d %% scale)"),(int)mainw->sepwin_scale);
	else xtrabit=g_strdup("");
	title=g_strdup_printf(_("LiVES: - Play Window%s"),xtrabit);
	if (mainw->play_window!=NULL)
	  lives_window_set_title (LIVES_WINDOW (mainw->play_window), title);
	g_free(title);
	g_free(xtrabit);
      }
      else {
	if (mainw->play_window!=NULL) {
	  kill_play_window();
	}
	make_play_window();
	if (mainw->play_window!=NULL) {
	  hide_cursor(lives_widget_get_xwindow(mainw->play_window));
	  lives_widget_set_app_paintable(mainw->play_window,TRUE);
	}
      }
      if (cfile->frames==1||cfile->play_paused) {
	lives_widget_context_update();
	if (!(mainw->play_window==NULL)&&!mainw->noswitch&&(cfile->clip_type==CLIP_TYPE_DISK||
							    cfile->clip_type==CLIP_TYPE_FILE)) {
	  weed_plant_t *frame_layer=mainw->frame_layer;
	  mainw->frame_layer=NULL;
	  load_frame_image (cfile->frameno);
	  mainw->frame_layer=frame_layer;
	}
      }
    }
    else {
      // in-frame
      if (mainw->double_size) {
	if (!mainw->sep_win) {
	  if (!mainw->faded) {
	    if (palette->style&STYLE_1) {
	      lives_widget_hide(mainw->sep_image);
	    }
	    lives_widget_hide(mainw->scrolledwindow);
	  }
	  resize(2);
	}
	if (!prefs->ce_maxspect) {
	  mainw->pheight*=2;
	  mainw->pheight++;
	  mainw->pwidth*=2;
	  mainw->pwidth+=2;
	}
      }
      else {
	resize(1);
	if (!prefs->ce_maxspect) {
	  mainw->pheight--;
	  mainw->pheight/=2;
	  mainw->pwidth-=2;
	  mainw->pwidth/=2;
	}
	if (!mainw->faded) {
	  if (palette->style&STYLE_1) {
	    lives_widget_show(mainw->sep_image);
	  }
	  lives_widget_show(mainw->scrolledwindow);
	}}}
  }
}



void on_sepwin_pressed (GtkButton *button, gpointer user_data) {
  if (mainw->go_away) return;

  // toolbar button (separate window)
  if (mainw->multitrack!=NULL) {
    lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->multitrack->sepwin),!mainw->sep_win);
    lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->sepwin),mainw->sep_win);
  }
  else lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->sepwin),!mainw->sep_win);
}



void on_sepwin_activate (GtkMenuItem *menuitem, gpointer user_data) {
  GtkWidget *sep_img;
  GtkWidget *sep_img2;

  gchar buff[PATH_MAX];
  gchar *fnamex;

  if (mainw->go_away) return;

  mainw->sep_win=!mainw->sep_win;

  if (mainw->multitrack!=NULL) {
    if (mainw->playing_file==-1) return;
    unpaint_lines(mainw->multitrack);
    mainw->multitrack->redraw_block=TRUE; // stop pb cursor from updating
    mt_show_current_frame(mainw->multitrack, FALSE);
  }

  fnamex=g_build_filename(prefs->prefix_dir,ICON_DIR,"sepwin.png",NULL);
  g_snprintf (buff,PATH_MAX,"%s",fnamex);
  g_free(fnamex);

  sep_img=lives_image_new_from_file (buff);
  sep_img2=lives_image_new_from_file (buff);

  if (mainw->sep_win) {
    lives_widget_set_tooltip_text(mainw->m_sepwinbutton,_("Hide the play window (s)"));
    lives_widget_set_tooltip_text(mainw->t_sepwin,_("Hide the play window (s)"));
  }
  else {
    if (g_file_test(buff,G_FILE_TEST_EXISTS)) {
      GdkPixbuf *pixbuf=lives_image_get_pixbuf(LIVES_IMAGE(sep_img));
      if (pixbuf!=NULL) lives_pixbuf_saturate_and_pixelate(pixbuf,pixbuf,0.2,FALSE);
      pixbuf=lives_image_get_pixbuf(LIVES_IMAGE(sep_img2));
      if (pixbuf!=NULL) lives_pixbuf_saturate_and_pixelate(pixbuf,pixbuf,0.2,FALSE);
    }
    lives_widget_set_tooltip_text(mainw->m_sepwinbutton,_("Show the play window (s)"));
    lives_widget_set_tooltip_text(mainw->t_sepwin,_("Play in separate window (s)"));
  }

  lives_widget_show(sep_img);
  lives_widget_show(sep_img2);
  lives_tool_button_set_icon_widget(LIVES_TOOL_BUTTON(mainw->m_sepwinbutton),sep_img);
  lives_tool_button_set_icon_widget(LIVES_TOOL_BUTTON(mainw->t_sepwin),sep_img2);

  if (prefs->sepwin_type==SEPWIN_TYPE_STICKY&&mainw->playing_file==-1) {
    if (mainw->sep_win) {
      make_play_window();
    }
    else {
      kill_play_window();
    }
    if (mainw->multitrack!=NULL&&mainw->playing_file==-1) {
      activate_mt_preview(mainw->multitrack); // show frame preview
    }
  }
  else {
    if (mainw->playing_file>-1) {
      if (mainw->sep_win) {
	// switch to separate window during pb
	if (mainw->multitrack==NULL) {

	  if (prefs->show_framecount&&((!mainw->preview&&(cfile->frames>0||mainw->foreign))||cfile->opening)) {
	    lives_widget_show(mainw->framebar);
	  }
	  if ((!mainw->faded&&mainw->fs&&((prefs->play_monitor!=prefs->gui_monitor&&prefs->play_monitor>0)))||
					 (mainw->fs&&mainw->vpp!=NULL&&
					  !(mainw->vpp->capabilities&VPP_LOCAL_DISPLAY))) {
	    lives_container_set_border_width (LIVES_CONTAINER (mainw->playframe), widget_opts.border_width);
	    unfade_background();
	    lives_widget_show(mainw->frame1);
	    lives_widget_show(mainw->frame2);
	    lives_widget_show(mainw->eventbox3);
	    lives_widget_show(mainw->eventbox4);
	    if (cfile->is_loaded) {
	      load_start_image (cfile->start);
	      load_end_image (cfile->end);
	    }
	    else {
	      load_start_image (0);
	      load_end_image (0);
	    }
	  }
	  if (mainw->fs&&!mainw->faded) {
	    resize(1);
	  }
	  else {
	    if (mainw->faded) {
	      lives_widget_hide(mainw->playframe);
	    }
	    if (mainw->double_size&&mainw->multitrack==NULL) {
	      resize(1);
	      if (!mainw->faded) {
		if (palette->style&STYLE_1) {
		  lives_widget_show(mainw->sep_image);
		}
		lives_widget_show(mainw->scrolledwindow);
	      }
	    }
	  }
	}

	make_play_window();

	mainw->pw_scroll_func=g_signal_connect (GTK_OBJECT (mainw->play_window), "scroll_event",
						G_CALLBACK (on_mouse_scroll),
						NULL);


	if (mainw->ext_playback&&mainw->vpp->fheight>-1&&mainw->vpp->fwidth>-1) {	  
	  // fixed o/p size for stream
	  if (!(mainw->vpp->fwidth*mainw->vpp->fheight)) {
	    mainw->vpp->fwidth=cfile->hsize;
	    mainw->vpp->fheight=cfile->vsize;
	  }
	  mainw->pwidth=mainw->vpp->fwidth;
	  mainw->pheight=mainw->vpp->fheight;

	  if (!(mainw->vpp->capabilities&VPP_LOCAL_DISPLAY)) {
	    lives_window_set_title (LIVES_WINDOW (mainw->play_window),_("LiVES: - Streaming"));
	    unfade_background();
	  }

	  resize(1);
	  resize_play_window();
	}
	
	if (mainw->play_window!=NULL&&GDK_IS_WINDOW(lives_widget_get_xwindow(mainw->play_window))) {
	  hide_cursor(lives_widget_get_xwindow(mainw->play_window));
	  lives_widget_set_app_paintable(mainw->play_window,TRUE);
	}
	if (cfile->frames==1||cfile->play_paused) {
	  lives_widget_context_update();
	  if (mainw->play_window!=NULL&&GDK_IS_WINDOW(lives_widget_get_xwindow(mainw->play_window))&&
	      !mainw->noswitch&&mainw->multitrack==NULL&&(cfile->clip_type==CLIP_TYPE_DISK||
							  cfile->clip_type==CLIP_TYPE_FILE)) {
	    weed_plant_t *frame_layer=mainw->frame_layer;
	    mainw->frame_layer=NULL;
	    load_frame_image (cfile->frameno);
	    mainw->frame_layer=frame_layer;
	  }
	}

      }
      else {
	// switch from separate window during playback
	if (cfile->frames>0&&mainw->multitrack==NULL) {
	  lives_widget_show(mainw->playframe);
	}
	if (!mainw->fs&&mainw->multitrack==NULL) {
	  if (!mainw->double_size) {
	    resize(1);
	  }
	  else {
	    if (palette->style&STYLE_1) {
	      lives_widget_hide(mainw->sep_image);
	    }
	    lives_widget_hide(mainw->scrolledwindow);
	    resize(2);
	  }
	  
	  lives_widget_queue_draw (mainw->playframe);
	  lives_widget_context_update();
	  mainw->pwidth=lives_widget_get_allocation_width(mainw->playframe)-H_RESIZE_ADJUST;
	  mainw->pheight=lives_widget_get_allocation_height(mainw->playframe)-V_RESIZE_ADJUST;
	}
	else {
	  if (mainw->ext_playback) {
	    vid_playback_plugin_exit();
	  }
	  if (mainw->multitrack==NULL) {
	    lives_widget_show(mainw->playframe);
	    lives_widget_hide(mainw->framebar);
	    if (!mainw->faded) fade_background();
	    fullscreen_internal();
	    lives_widget_context_update();
	  }
	}
	if (mainw->multitrack!=NULL) {
	  mainw->must_resize=TRUE;
	  mainw->pwidth=mainw->multitrack->play_width;
	  mainw->pheight=mainw->multitrack->play_height;
	}
	kill_play_window();
	if (mainw->multitrack==NULL) add_to_playframe();

	hide_cursor(lives_widget_get_xwindow(mainw->playarea));
	if (mainw->multitrack==NULL&&(cfile->frames==1||cfile->play_paused)&&
	    !mainw->noswitch&&(cfile->clip_type==CLIP_TYPE_DISK||
			       cfile->clip_type==CLIP_TYPE_FILE)) {
	  weed_plant_t *frame_layer=mainw->frame_layer;
	  mainw->frame_layer=NULL;
	  load_frame_image (cfile->frameno);
	  mainw->frame_layer=frame_layer;
	}
      }
    }
  }
  /*  if (mainw->multitrack!=NULL) {
    mainw->multitrack->redraw_block=FALSE;
    if (mainw->sep_win&&prefs->play_monitor!=prefs->gui_monitor&&prefs->play_monitor!=0) {
      lives_widget_hide(mainw->multitrack->play_box);
      lives_widget_hide(lives_widget_get_parent(mainw->multitrack->play_box));
      lives_widget_hide(lives_widget_get_parent(lives_widget_get_parent(mainw->multitrack->play_box)));
    }
    else {
      lives_widget_show(mainw->multitrack->play_box);
      lives_widget_show(lives_widget_get_parent(mainw->multitrack->play_box));
      lives_widget_show(lives_widget_get_parent(lives_widget_get_parent(mainw->multitrack->play_box)));
      if (mainw->playing_file>-1) mt_show_current_frame(mainw->multitrack, FALSE);
    }


    }*/
}


void on_showfct_activate (GtkMenuItem *menuitem, gpointer user_data) {
  prefs->show_framecount=!prefs->show_framecount;
  if (mainw->playing_file>-1) {
    if (!mainw->fs||(prefs->play_monitor!=prefs->gui_monitor&&mainw->play_window!=NULL)) {
      if (prefs->show_framecount) {
	lives_widget_show (mainw->framebar);
      }
      else {
	lives_widget_hide (mainw->framebar);
      }
    }
  }
}



void on_sticky_activate (GtkMenuItem *menuitem, gpointer user_data) {
  // type is SEPWIN_TYPE_STICKY (shown even when not playing)
  // or SEPWIN_TYPE_NON_STICKY (shown only when playing)
  
  gchar *title,*xtrabit;

  if (prefs->sepwin_type==SEPWIN_TYPE_NON_STICKY) {
    prefs->sepwin_type=SEPWIN_TYPE_STICKY;
    if (mainw->sep_win) {
      if (mainw->playing_file==-1) {
	make_play_window();
      }
    }
    else {
      if (!(mainw->play_window==NULL)) {
	if (mainw->sepwin_scale!=100.) xtrabit=g_strdup_printf(_(" (%d %% scale)"),(int)mainw->sepwin_scale);
	else xtrabit=g_strdup("");
	title=g_strdup_printf(_("LiVES: - Play Window%s"),xtrabit);
	lives_window_set_title (LIVES_WINDOW (mainw->play_window), title);
	g_free(title);
	g_free(xtrabit);
      }
    }
  }
  else {
    if (prefs->sepwin_type==SEPWIN_TYPE_STICKY) {
      prefs->sepwin_type=SEPWIN_TYPE_NON_STICKY;
      if (mainw->sep_win) {
	if (mainw->playing_file==-1) {
	  kill_play_window();
	}
	else {
	  if (mainw->sepwin_scale!=100.) xtrabit=g_strdup_printf(_(" (%d %% scale)"),(int)mainw->sepwin_scale);
	  else xtrabit=g_strdup("");
	  title=g_strdup_printf("%s%s",lives_window_get_title(LIVES_WINDOW
							    (mainw->multitrack==NULL?mainw->LiVES:
							     mainw->multitrack->window)),xtrabit);
	  if (mainw->play_window!=NULL)
	    lives_window_set_title(LIVES_WINDOW(mainw->play_window),title);
	  g_free(title);
	  g_free(xtrabit);
	}
      }
    }
  }
}



void on_fade_pressed (GtkButton *button,
		      gpointer user_data) 
{
  // toolbar button (unblank background)
  if (mainw->fs&&(mainw->play_window==NULL||prefs->play_monitor==prefs->gui_monitor)) return;
  lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->fade),!mainw->faded);
}



void
on_fade_activate               (GtkMenuItem     *menuitem,
				gpointer         user_data)
{
  mainw->faded=!mainw->faded;
  if (mainw->playing_file>-1&&(!mainw->fs||(prefs->play_monitor!=prefs->gui_monitor&&mainw->play_window!=NULL))) {
    if (mainw->faded) fade_background();
    else unfade_background();
  }
}




void on_showsubs_toggled(GObject *obj, gpointer user_data) {
  prefs->show_subtitles=!prefs->show_subtitles;
  if (mainw->current_file>0&&mainw->multitrack==NULL) {
    if (mainw->play_window!=NULL) {
      load_preview_image(FALSE);
    }
    load_start_image(cfile->start);
    load_end_image(cfile->end);
  }
}



void on_boolean_toggled(GObject *obj, gpointer user_data) {
  boolean *ppref=(boolean *)user_data;
  *ppref=!*ppref;
}


void
on_loop_video_activate                (GtkMenuItem     *menuitem,
				       gpointer         user_data)
{
  if (mainw->current_file==0) return;
  mainw->loop=!mainw->loop;
  lives_widget_set_sensitive (mainw->playclip, mainw->playing_file==-1&&clipboard!=NULL);
  if (mainw->current_file>-1) find_when_to_stop();
}

void
on_loop_button_activate                (GtkMenuItem     *menuitem,
					gpointer         user_data)
{
  if (mainw->multitrack!=NULL) {
    g_signal_handler_block (mainw->multitrack->loop_continue, mainw->multitrack->loop_cont_func);
    lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->multitrack->loop_continue),!mainw->loop_cont);
    g_signal_handler_unblock (mainw->multitrack->loop_continue, mainw->multitrack->loop_cont_func);
  }
  lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->loop_continue), !mainw->loop_cont);
}


void on_loop_cont_activate (GtkMenuItem *menuitem, gpointer user_data) {
  gchar buff[PATH_MAX];
  GtkWidget *loop_img;
  gchar *fnamex;

  mainw->loop_cont=!mainw->loop_cont;

  fnamex=g_build_filename(prefs->prefix_dir,ICON_DIR,"loop.png",NULL);
  g_snprintf (buff,PATH_MAX,"%s",fnamex);
  g_free(fnamex);

  loop_img=lives_image_new_from_file (buff);

  if (mainw->loop_cont) {
    lives_widget_set_tooltip_text(mainw->m_loopbutton,_("Switch continuous looping off (o)"));
  }
  else {
    if (g_file_test(buff,G_FILE_TEST_EXISTS)) {
      GdkPixbuf *pixbuf=lives_image_get_pixbuf(LIVES_IMAGE(loop_img));
      if (pixbuf!=NULL) lives_pixbuf_saturate_and_pixelate(pixbuf,pixbuf,0.2,FALSE);
    }
    lives_widget_set_tooltip_text(mainw->m_loopbutton,_("Switch continuous looping on (o)"));
  }

  lives_widget_show(loop_img);
  lives_tool_button_set_icon_widget(LIVES_TOOL_BUTTON(mainw->m_loopbutton),loop_img);

  lives_widget_set_sensitive (mainw->playclip, !(clipboard==NULL));
  if (mainw->current_file>-1) find_when_to_stop();
  else mainw->whentostop=NEVER_STOP;

#ifdef ENABLE_JACK
  if (prefs->audio_player==AUD_PLAYER_JACK) {
    if (mainw->jackd!=NULL&&(mainw->loop_cont||mainw->whentostop==NEVER_STOP)) {
      if (mainw->ping_pong&&prefs->audio_opts&AUDIO_OPTS_FOLLOW_FPS) mainw->jackd->loop=AUDIO_LOOP_PINGPONG;
      else mainw->jackd->loop=AUDIO_LOOP_FORWARD;
    }
    else if (mainw->jackd!=NULL) mainw->jackd->loop=AUDIO_LOOP_NONE;
  }
#endif
#ifdef HAVE_PULSE_AUDIO
  if (prefs->audio_player==AUD_PLAYER_PULSE) {
    if (mainw->pulsed!=NULL&&(mainw->loop_cont||mainw->whentostop==NEVER_STOP)) {
      if (mainw->ping_pong&&prefs->audio_opts&AUDIO_OPTS_FOLLOW_FPS) mainw->pulsed->loop=AUDIO_LOOP_PINGPONG;
      else mainw->pulsed->loop=AUDIO_LOOP_FORWARD;
    }
    else if (mainw->pulsed!=NULL) mainw->pulsed->loop=AUDIO_LOOP_NONE;
  }
#endif

}


void on_ping_pong_activate (GtkMenuItem *menuitem, gpointer user_data) {
  mainw->ping_pong=!mainw->ping_pong;
#ifdef ENABLE_JACK
  if (prefs->audio_player==AUD_PLAYER_JACK&&mainw->jackd!=NULL&&mainw->jackd->loop!=AUDIO_LOOP_NONE) {
    if (mainw->ping_pong&&prefs->audio_opts&AUDIO_OPTS_FOLLOW_FPS) mainw->jackd->loop=AUDIO_LOOP_PINGPONG;
    else mainw->jackd->loop=AUDIO_LOOP_FORWARD;
  }
#endif
#ifdef HAVE_PULSE_AUDIO
  if (prefs->audio_player==AUD_PLAYER_PULSE&&mainw->pulsed!=NULL&&mainw->pulsed->loop!=AUDIO_LOOP_NONE) {
    if (mainw->ping_pong&&prefs->audio_opts&AUDIO_OPTS_FOLLOW_FPS) mainw->pulsed->loop=AUDIO_LOOP_PINGPONG;
    else mainw->pulsed->loop=AUDIO_LOOP_FORWARD;
  }
#endif
}


void on_volume_slider_value_changed (LiVESScaleButton *sbutton, gpointer user_data) {
  gchar *ttip;
  mainw->volume=lives_scale_button_get_value(sbutton);
  ttip=g_strdup_printf(_("Audio volume (%.2f)"),mainw->volume);
  lives_widget_set_tooltip_text(mainw->vol_toolitem,_(ttip));
  g_free(ttip);
}



void on_mute_button_activate (GtkMenuItem *menuitem, gpointer user_data) {
  if (mainw->multitrack!=NULL) {
    g_signal_handler_block (mainw->multitrack->mute_audio, mainw->multitrack->mute_audio_func);
    lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->multitrack->mute_audio),!mainw->mute);
    g_signal_handler_unblock (mainw->multitrack->mute_audio, mainw->multitrack->mute_audio_func);
  }
  lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->mute_audio), !mainw->mute);
}


boolean mute_audio_callback (GtkAccelGroup *group, GObject *obj, guint keyval, GdkModifierType mod, gpointer user_data) {
  lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->mute_audio), !mainw->mute);
  return TRUE;
}


void on_mute_activate (GtkMenuItem *menuitem, gpointer user_data) {
  gchar buff[PATH_MAX];
  GtkWidget *mute_img;
  GtkWidget *mute_img2=NULL;
  gchar *fnamex;

  mainw->mute=!mainw->mute;

  // change the mute icon
  fnamex=g_build_filename(prefs->prefix_dir,ICON_DIR,"volume_mute.png",NULL);
  g_snprintf (buff,PATH_MAX,"%s",fnamex);
  g_free(fnamex);

  mute_img=lives_image_new_from_file (buff);
  if (mainw->preview_box!=NULL) mute_img2=lives_image_new_from_file (buff);
  if (mainw->mute) {
    lives_widget_set_tooltip_text(mainw->m_mutebutton,_("Unmute the audio (z)"));
    if (mainw->preview_box!=NULL) lives_widget_set_tooltip_text( mainw->p_mutebutton,_("Unmute the audio (z)"));
  }
  else {
    if (g_file_test(buff,G_FILE_TEST_EXISTS)) {
      GdkPixbuf *pixbuf=lives_image_get_pixbuf(LIVES_IMAGE(mute_img));
      if (pixbuf!=NULL) lives_pixbuf_saturate_and_pixelate(pixbuf,pixbuf,0.2,FALSE);
      if (mainw->preview_box!=NULL) {
	pixbuf=lives_image_get_pixbuf(LIVES_IMAGE(mute_img2));
	if (pixbuf!=NULL) lives_pixbuf_saturate_and_pixelate(pixbuf,pixbuf,0.2,FALSE);
      }
    }
    lives_widget_set_tooltip_text(mainw->m_mutebutton,_("Mute the audio (z)"));
    if (mainw->preview_box!=NULL) lives_widget_set_tooltip_text( mainw->p_mutebutton,_("Mute the audio (z)"));
  }

  lives_widget_show(mute_img);

  lives_tool_button_set_icon_widget(LIVES_TOOL_BUTTON(mainw->m_mutebutton),mute_img);

  if (mainw->preview_box!=NULL) {
    lives_widget_show(mute_img2);
    //gtk_button_set_image(GTK_BUTTON(mainw->p_mutebutton),mute_img2); // doesn't work (gtk+ bug ?)
    lives_widget_queue_draw(mainw->p_mutebutton);
    lives_widget_queue_draw(mute_img2);
  }
#ifdef ENABLE_JACK
  if (prefs->audio_player==AUD_PLAYER_JACK&&mainw->playing_file>-1&&mainw->jackd!=NULL) {
    mainw->jackd->mute=mainw->mute;
    if (mainw->jackd->playing_file==mainw->current_file&&cfile->achans>0&&!mainw->is_rendering) {
      if (!jack_audio_seek_bytes(mainw->jackd,mainw->jackd->seek_pos)) {
	if (jack_try_reconnect()) jack_audio_seek_bytes(mainw->jackd,mainw->jackd->seek_pos);
      }
      mainw->jackd->in_use=TRUE;
    }
  }
#endif
#ifdef HAVE_PULSE_AUDIO
  if (prefs->audio_player==AUD_PLAYER_PULSE&&mainw->playing_file>-1&&mainw->pulsed!=NULL) {
    mainw->pulsed->mute=mainw->mute;
    if (mainw->pulsed->playing_file==mainw->current_file&&cfile->achans>0&&!mainw->is_rendering) {
      if (!pulse_audio_seek_bytes(mainw->pulsed, mainw->pulsed->seek_pos)) {
	if (pulse_try_reconnect()) pulse_audio_seek_bytes(mainw->pulsed,mainw->pulsed->seek_pos);
      }
      mainw->pulsed->in_use=TRUE;
    }
  }
#endif
}


void on_spin_value_changed (GtkSpinButton *spinbutton, gpointer user_data) {
  // TODO - use array
  switch (GPOINTER_TO_INT (user_data)) {
  case 1 :
    mainw->fx1_val=lives_spin_button_get_value(LIVES_SPIN_BUTTON(spinbutton));
    break;
  case 2 :
    mainw->fx2_val=lives_spin_button_get_value(LIVES_SPIN_BUTTON(spinbutton));
    break;
  case 3 :
    mainw->fx3_val=lives_spin_button_get_value(LIVES_SPIN_BUTTON(spinbutton));
    break;
  case 4 :
    mainw->fx4_val=lives_spin_button_get_value(LIVES_SPIN_BUTTON(spinbutton));
    break;
  }
}


void
on_spin_start_value_changed           (GtkSpinButton   *spinbutton,
				       gpointer         user_data)
{
  // generic
  // TODO - use array
  switch (GPOINTER_TO_INT (user_data)) {
  case 1 :
    mainw->fx1_start=lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(spinbutton));
    break;
  case 2 :
    mainw->fx2_start=lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(spinbutton));
    break;
  case 3 :
    mainw->fx3_start=lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(spinbutton));
    break;
  case 4 :
    mainw->fx4_start=lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(spinbutton));
    break;
  }
}
void
on_spin_step_value_changed           (GtkSpinButton   *spinbutton,
				     gpointer         user_data)
{
  // generic
  // TODO - use array
  switch (GPOINTER_TO_INT (user_data)) {
  case 1 :
    mainw->fx1_step=lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(spinbutton));
    break;
  case 2 :
    mainw->fx2_step=lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(spinbutton));
    break;
  case 3 :
    mainw->fx3_step=lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(spinbutton));
    break;
  case 4 :
    mainw->fx4_step=lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(spinbutton));
    break;
  }
}

void
on_spin_end_value_changed           (GtkSpinButton   *spinbutton,
				     gpointer         user_data)
{
  // generic
  // TODO - use array
  switch (GPOINTER_TO_INT (user_data)) {
  case 1 :
    mainw->fx1_end=lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(spinbutton));
    break;
  case 2 :
    mainw->fx2_end=lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(spinbutton));
    break;
  case 3 :
    mainw->fx3_end=lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(spinbutton));
    break;
  case 4 :
    mainw->fx4_end=lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(spinbutton));
    break;
  }
}



void on_rev_clipboard_activate (GtkMenuItem *menuitem, gpointer user_data) {
  // reverse the clipboard
  gchar *com;
  int current_file=mainw->current_file;
  mainw->current_file=0;

  if (!check_if_non_virtual(0,1,cfile->frames)) {
    lives_clip_data_t *cdata=((lives_decoder_t *)cfile->ext_src)->cdata;
    if (!(cdata->seek_flag&LIVES_SEEK_FAST)) {
      boolean retb;
      mainw->cancelled=CANCEL_NONE;
      cfile->progress_start=1;
      cfile->progress_end=cfile->frames;
      do_threaded_dialog(_("Pulling frames from clipboard"),TRUE);
      retb=virtual_to_images(mainw->current_file,1,cfile->frames,TRUE,NULL);
      end_threaded_dialog();
      
      if (mainw->cancelled!=CANCEL_NONE||!retb) {
	sensitize();
	mainw->cancelled=CANCEL_USER;
	return;
      }
    }
  }

  d_print(_ ("Reversing clipboard..."));
  com=g_strdup_printf("%s reverse \"%s\" %d %d \"%s\"",prefs->backend,clipboard->handle,1,clipboard->frames,
		      get_image_ext_for_type(cfile->img_type));

  unlink(cfile->info_file);
  mainw->com_failed=FALSE;
  lives_system(com,FALSE);
  g_free(com);

  if (!mainw->com_failed) {
    cfile->progress_start=1;
    cfile->progress_end=cfile->frames;

    // show a progress dialog, not cancellable
    do_progress_dialog(TRUE,FALSE,_ ("Reversing clipboard"));
  }

  if (mainw->com_failed||mainw->error) d_print_failed();
  else {
    if (clipboard->frame_index!=NULL) reverse_frame_index(0);
    d_print_done();
  }

  mainw->current_file=current_file;
  sensitize();


}




void on_load_subs_activate (GtkMenuItem *menuitem, gpointer user_data) {
  gchar *subfile;
  gchar *filt[]={"*.srt","*.sub",NULL};
  gchar filename[512];
  gchar *subfname,*isubfname,*com,*tmp;
  lives_subtitle_type_t subtype=SUBTITLE_TYPE_NONE;
  gchar *lfile_name;
  gchar *ttl;

  if (cfile->subt!=NULL) if (!do_existing_subs_warning()) return;

  // try to repaint the screen, as it may take a few seconds to get a directory listing
  lives_widget_context_update();

  ttl=g_strdup (_("LiVES: Load subtitles from..."));

  if (strlen(mainw->vid_load_dir)) {
    subfile=choose_file(mainw->vid_load_dir,NULL,filt,LIVES_FILE_CHOOSER_ACTION_OPEN,ttl,NULL);
  }
  else subfile=choose_file(NULL,NULL,filt,LIVES_FILE_CHOOSER_ACTION_OPEN,ttl,NULL);
  g_free(ttl);

  if (subfile==NULL) return; // cancelled

  g_snprintf(filename,512,"%s",subfile);
  g_free(subfile);

  get_filename(filename,FALSE); // strip extension
  isubfname=g_strdup_printf("%s.srt",filename);
  lfile_name=g_filename_from_utf8(isubfname,-1,NULL,NULL,NULL);

  if (g_file_test(lfile_name,G_FILE_TEST_EXISTS)) {
    subfname=g_build_filename(prefs->tmpdir,cfile->handle,"subs.srt",NULL);
    subtype=SUBTITLE_TYPE_SRT;
  }
  else {
    g_free(isubfname);
    g_free(lfile_name);
    isubfname=g_strdup_printf("%s.sub",filename);
    lfile_name=g_filename_from_utf8(isubfname,-1,NULL,NULL,NULL);

    if (g_file_test(isubfname,G_FILE_TEST_EXISTS)) {
      subfname=g_build_filename(prefs->tmpdir,cfile->handle,"subs.sub",NULL);
      subtype=SUBTITLE_TYPE_SUB;
    }
    else {
      g_free(isubfname);
      do_invalid_subs_error();
      g_free(lfile_name);
      return;
    }
  }

  if (cfile->subt!=NULL) {
    // erase any existing subs
    on_erase_subs_activate(NULL,NULL);
    subtitles_free(cfile);
  }

  mainw->com_failed=FALSE;
#ifndef IS_MINGW
  com=g_strdup_printf("/bin/cp \"%s\" \"%s\"",lfile_name,subfname);
#else
  com=g_strdup_printf("cp.exe \"%s\" \"%s\"",lfile_name,subfname);
#endif
  lives_system(com,FALSE);
  g_free(com);

  if (mainw->com_failed) {
    g_free(subfname);
    g_free(isubfname);
    g_free(lfile_name);
    return;
  }

  subtitles_init(cfile,subfname,subtype);
  g_free(subfname);

  // force update
  switch_to_file(0,mainw->current_file);
  
  tmp=g_strdup_printf(_("Loaded subtitle file: %s\n"),isubfname);
  d_print(tmp);
  g_free(tmp);
  g_free(isubfname);
  g_free(lfile_name);
}





void on_save_subs_activate (GtkMenuItem *menuitem, gpointer user_data) {
  gchar *subfile;
  gchar xfname[512];
  gchar xfname2[512];

  GtkEntry *entry=(GtkEntry *)user_data;

  // try to repaint the screen, as it may take a few seconds to get a directory listing
  lives_widget_context_update();

  g_snprintf(xfname,512,"%s",mainw->subt_save_file);
  get_dirname(xfname);

  g_snprintf(xfname2,512,"%s",mainw->subt_save_file);
  get_basename(xfname2);

  subfile=choose_file(xfname,xfname2,NULL,LIVES_FILE_CHOOSER_ACTION_SAVE,NULL,NULL);

  if (subfile==NULL) return; // cancelled

  if (check_file(subfile,FALSE))
    lives_entry_set_text(entry,subfile);

  g_free(subfile);
}



void on_erase_subs_activate (GtkMenuItem *menuitem, gpointer user_data) {
  gchar *sfname;

  if (cfile->subt==NULL) return;

  if (menuitem!=NULL)
    if (!do_erase_subs_warning()) return;

  switch (cfile->subt->type) {
  case SUBTITLE_TYPE_SRT:
    sfname=g_build_filename(prefs->tmpdir,cfile->handle,"subs.srt",NULL);
    break;

  case SUBTITLE_TYPE_SUB:
    sfname=g_build_filename(prefs->tmpdir,cfile->handle,"subs.sub",NULL);
    break;

  default:
    return;
  }

  subtitles_free(cfile);

  unlink(sfname);
  g_free(sfname);

  if (menuitem!=NULL) {
    // force update
    switch_to_file(0,mainw->current_file);

    d_print(_("Subtitles were erased.\n"));
  }
}





void on_load_audio_activate (GtkMenuItem *menuitem, gpointer user_data) {
  if (mainw->multitrack!=NULL) {
    if (mainw->multitrack->idlefunc>0) {
      g_source_remove(mainw->multitrack->idlefunc);
      mainw->multitrack->idlefunc=0;
    }
    mt_desensitise(mainw->multitrack);
    lives_widget_set_sensitive(mainw->multitrack->playall,TRUE);
    lives_widget_set_sensitive (mainw->m_playbutton, TRUE);
  }

  choose_file_with_preview(strlen(mainw->audio_dir)?mainw->audio_dir:NULL,_ ("LiVES: - Select Audio File"),2);
}


void
on_load_cdtrack_activate                (GtkMenuItem     *menuitem,
					 gpointer         user_data)
{
  GtkWidget *cdtrack_dialog;
  
  if (!strlen(prefs->cdplay_device)) {
    do_error_dialog(_ ("Please set your CD play device in Tools | Preferences | Misc\n"));
    return;
  }

  cdtrack_dialog = create_cdtrack_dialog (0,NULL);
  lives_widget_show (cdtrack_dialog);
  mainw->fx1_val=1;

}


void
on_eject_cd_activate                (GtkMenuItem     *menuitem,
				     gpointer         user_data)
{
  gchar *com=g_strdup_printf("eject \"%s\"",prefs->cdplay_device);
  lives_widget_context_update();
  lives_system(com,TRUE);
  g_free(com);
}


void 
on_load_cdtrack_ok_clicked                (GtkButton     *button,
					   gpointer         user_data)
{
  gchar *com,*mesg;
  gchar **array;
  boolean was_new=FALSE;
  int new_file=mainw->first_free_file;
  int asigned,endian;
  boolean has_lmap_error=FALSE;
  boolean bad_header=FALSE;

  lives_general_button_clicked(button,NULL);

  if (mainw->current_file>-1) {
    if (!(prefs->warning_mask&WARN_MASK_LAYOUT_DELETE_AUDIO)) {
      if ((mainw->xlays=layout_audio_is_affected(mainw->current_file,0.))!=NULL) {
	if (!do_warning_dialog(_("\nLoading new audio may cause missing audio in some multitrack layouts.\nAre you sure you wish to continue ?\n."))) {
	  g_list_free_strings(mainw->xlays);
	  g_list_free(mainw->xlays);
	  mainw->xlays=NULL;
	  return;
	}
	add_lmap_error(LMAP_ERROR_DELETE_AUDIO,cfile->name,(gpointer)cfile->layout_map,mainw->current_file,0,0.,
		       cfile->stored_layout_audio>0.);
	has_lmap_error=TRUE;
	g_list_free_strings(mainw->xlays);
	g_list_free(mainw->xlays);
	mainw->xlays=NULL;
      }
    }
    
    if (!has_lmap_error&&!(prefs->warning_mask&WARN_MASK_LAYOUT_ALTER_AUDIO)&&
	(mainw->xlays=layout_audio_is_affected(mainw->current_file,0.))!=NULL) {
      if (!do_layout_alter_audio_warning()) {
	g_list_free_strings(mainw->xlays);
	g_list_free(mainw->xlays);
	mainw->xlays=NULL;
	return;
      }
      add_lmap_error(LMAP_ERROR_ALTER_AUDIO,cfile->name,(gpointer)cfile->layout_map,mainw->current_file,0,0.,
		     cfile->stored_layout_audio>0.);
      has_lmap_error=TRUE;
      g_list_free_strings(mainw->xlays);
      g_list_free(mainw->xlays);
      mainw->xlays=NULL;
    }
  }

  mesg=g_strdup_printf(_ ("Opening CD track %d from %s..."),(int)mainw->fx1_val,prefs->cdplay_device);
  d_print(mesg);
  g_free(mesg);

  if (mainw->current_file==-1) {
    if (!get_new_handle(new_file,g_strdup_printf (_ ("CD track %d"),(int)mainw->fx1_val))) {
      return;
    }

    mainw->current_file=new_file;
    g_snprintf(cfile->type,40,"CD track %d on %s",(int)mainw->fx1_val,prefs->cdplay_device);
    get_play_times();
    add_to_clipmenu();
    was_new=TRUE;
    cfile->opening=cfile->opening_audio=cfile->opening_only_audio=TRUE;
    cfile->hsize=DEFAULT_FRAME_HSIZE;
    cfile->vsize=DEFAULT_FRAME_VSIZE;
  }
  else {
    mainw->noswitch=TRUE;

    cfile->undo_arate=cfile->arate;
    cfile->undo_achans=cfile->achans;
    cfile->undo_asampsize=cfile->asampsize;
    cfile->undo_signed_endian=cfile->signed_endian;
    cfile->undo_arps=cfile->arps;
  }

  com=g_strdup_printf("%s cdopen \"%s\" %d",prefs->backend,cfile->handle,(int)mainw->fx1_val);

  unlink(cfile->info_file);
  mainw->com_failed=FALSE;
  lives_system(com,FALSE);
  g_free(com);

  if (mainw->com_failed) {
    cfile->arate=cfile->undo_arate;
    cfile->achans=cfile->undo_achans;
    cfile->asampsize=cfile->undo_asampsize;
    cfile->signed_endian=cfile->undo_signed_endian;
    cfile->arps=cfile->undo_arps;
    
    sensitize();
    reget_afilesize(mainw->current_file);
    get_play_times();

    mainw->noswitch=FALSE;
    if (was_new) close_current_file(0);
    return;
  }


  if (!(do_progress_dialog(TRUE,TRUE,_ ("Opening CD track...")))) {
    lives_widget_queue_draw(mainw->LiVES);
    lives_widget_context_update();

    if (!was_new) {
      mainw->com_failed=FALSE;
      mainw->cancelled=CANCEL_NONE;
      mainw->error=FALSE;
      unlink (cfile->info_file);

      com=g_strdup_printf("%s cancel_audio \"%s\"",prefs->backend,cfile->handle);
      lives_system(com,FALSE);
      g_free(com);

      if (!mainw->com_failed) do_auto_dialog(_("Cancelling"),0);

      cfile->arate=cfile->undo_arate;
      cfile->achans=cfile->undo_achans;
      cfile->asampsize=cfile->undo_asampsize;
      cfile->signed_endian=cfile->undo_signed_endian;
      cfile->arps=cfile->undo_arps;
      
      sensitize();
      reget_afilesize(mainw->current_file);
      get_play_times();

    }

    mainw->noswitch=FALSE;
    if (was_new) close_current_file(0);

    if (mainw->error) {
      d_print_failed();
    }

    return;
  }

  lives_widget_queue_draw(mainw->LiVES);
  lives_widget_context_update();

  if (mainw->error) {
    d_print(_ ("Error loading CD track\n"));

    lives_widget_queue_draw(mainw->LiVES);
    lives_widget_context_update();

    if (!was_new) {
      com=g_strdup_printf("%s cancel_audio \"%s\"",prefs->backend,cfile->handle);
      mainw->cancelled=CANCEL_NONE;
      mainw->error=FALSE;
      unlink (cfile->info_file);
      mainw->com_failed=FALSE;
      lives_system(com,FALSE);
      g_free(com);

      if (!mainw->com_failed) do_auto_dialog(_("Cancelling"),0);

      cfile->arate=cfile->undo_arate;
      cfile->achans=cfile->undo_achans;
      cfile->asampsize=cfile->undo_asampsize;
      cfile->signed_endian=cfile->undo_signed_endian;
      cfile->arps=cfile->undo_arps;
      
      sensitize();
      reget_afilesize(mainw->current_file);
      get_play_times();
    }

    mainw->noswitch=FALSE;
    if (was_new) close_current_file(0);
    return;
  }

  array=g_strsplit(mainw->msg,"|",5);
  cfile->arate=atoi(array[1]);
  cfile->achans=atoi(array[2]);
  cfile->asampsize=atoi(array[3]);
  cfile->afilesize=strtol(array[4],NULL,10);
  g_strfreev(array);

  if (!was_new&&cfile->undo_arate>0) cfile->arps=cfile->undo_arps/cfile->undo_arate*cfile->arate;
  else cfile->arps=cfile->arate;

  asigned=!(cfile->signed_endian&AFORM_UNSIGNED);
  endian=cfile->signed_endian&AFORM_BIG_ENDIAN;

  if (cfile->afilesize==0l) {
    d_print(_ ("Error loading CD track\n"));
  
    if (!was_new) {
      com=g_strdup_printf("%s cancel_audio \"%s\"",prefs->backend,cfile->handle);
      mainw->com_failed=FALSE;
      mainw->cancelled=CANCEL_NONE;
      mainw->error=FALSE;
      unlink (cfile->info_file);
      lives_system(com,FALSE);
      g_free(com);

      if (!mainw->com_failed) do_auto_dialog(_("Cancelling"),0);

      cfile->achans=cfile->undo_achans;
      cfile->arate=cfile->undo_arate;
      cfile->arps=cfile->undo_arps;
      cfile->asampsize=cfile->undo_asampsize;
      cfile->signed_endian=cfile->undo_signed_endian;

      reget_afilesize(mainw->current_file);
    }

    mainw->noswitch=FALSE;
    if (was_new) close_current_file(0);
    return;
  }

  cfile->opening=cfile->opening_audio=cfile->opening_only_audio=FALSE;

  mainw->cancelled=CANCEL_NONE;
  mainw->error=FALSE;
  mainw->com_failed=FALSE;
  unlink (cfile->info_file);

  com=g_strdup_printf("%s commit_audio \"%s\"",prefs->backend,cfile->handle);
  lives_system(com, FALSE);
  g_free(com);

  if (mainw->com_failed) {
    d_print_failed();
    cfile->achans=cfile->undo_achans;
    cfile->arate=cfile->undo_arate;
    cfile->arps=cfile->undo_arps;
    cfile->asampsize=cfile->undo_asampsize;
    cfile->signed_endian=cfile->undo_signed_endian;
    
    reget_afilesize(mainw->current_file);
    
    mainw->noswitch=FALSE;
    if (was_new) close_current_file(0);
    return;
  }

  if (!do_auto_dialog(_("Committing audio"),0)) {
    d_print_failed();
    return;
  }

  save_clip_value(mainw->current_file,CLIP_DETAILS_ARATE,&cfile->arps);
  if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
  save_clip_value(mainw->current_file,CLIP_DETAILS_PB_ARATE,&cfile->arate);
  if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
  save_clip_value(mainw->current_file,CLIP_DETAILS_ACHANS,&cfile->achans);
  if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
  save_clip_value(mainw->current_file,CLIP_DETAILS_ASIGNED,&asigned);
  if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
  save_clip_value(mainw->current_file,CLIP_DETAILS_AENDIAN,&endian);
  if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
  save_clip_value(mainw->current_file,CLIP_DETAILS_ASAMPS,&cfile->asampsize);
  if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;

  if (bad_header) do_header_write_error(mainw->current_file);

  reget_afilesize(mainw->current_file);
  get_play_times();
  cfile->changed=TRUE;
  d_print_done();
  mesg=g_strdup_printf(P_("New audio: %d Hz %d channel %d bps\n","New audio: %d Hz %d channels %d bps\n",cfile->achans),
		       cfile->arate,cfile->achans,cfile->asampsize);
  d_print(mesg);
  g_free(mesg);
  
  if (!was_new) {
    if (!prefs->conserve_space) {
      cfile->undo_action=UNDO_NEW_AUDIO;
      set_undoable (_("New Audio"),TRUE);
    }
  }

  lives_widget_set_sensitive(mainw->loop_video,TRUE);
  mainw->noswitch=FALSE;

#ifdef ENABLE_OSC
    lives_osc_notify(LIVES_OSC_NOTIFY_CLIP_OPENED,"");
#endif

}

void on_load_vcd_ok_clicked (GtkButton *button, gpointer         user_data)
{
  lives_general_button_clicked(button,NULL);
  if (GPOINTER_TO_INT (user_data)==1) {
    g_snprintf (file_name,PATH_MAX,"dvd://%d",(int)mainw->fx1_val);
    if (mainw->file_open_params!=NULL) g_free(mainw->file_open_params);
    mainw->file_open_params=g_strdup_printf ("-chapter %d -aid %d",(int)mainw->fx2_val,(int)mainw->fx3_val);
  }
  else {
    g_snprintf (file_name,PATH_MAX,"vcd://%d",(int)mainw->fx1_val);
  }
  open_sel_range_activate();
}



void popup_lmap_errors(GtkMenuItem *menuitem, gpointer user_data) {
  // popup layout map errors dialog
  GtkWidget *dialog_action_area,*vbox;
  GtkWidget *button;
  text_window *textwindow;

  if (prefs->warning_mask&WARN_MASK_LAYOUT_POPUP) return;

  widget_opts.expand=LIVES_EXPAND_NONE;
  textwindow=create_text_window(_("layout errors"),NULL,mainw->layout_textbuffer);
  widget_opts.expand=LIVES_EXPAND_DEFAULT;

  dialog_action_area = lives_dialog_get_action_area(LIVES_DIALOG (textwindow->dialog));

  gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog_action_area), GTK_BUTTONBOX_SPREAD);

  vbox = lives_dialog_get_content_area(LIVES_DIALOG(textwindow->dialog));

  add_warn_check(LIVES_BOX(vbox),WARN_MASK_LAYOUT_POPUP);

  button = lives_button_new_with_mnemonic (_("Close _Window"));

  lives_dialog_add_action_widget (LIVES_DIALOG (textwindow->dialog), button, GTK_RESPONSE_OK);

  g_signal_connect (GTK_OBJECT (button), "clicked",
		    G_CALLBACK (lives_general_button_clicked),
		    textwindow);

  lives_container_set_border_width (LIVES_CONTAINER (button), widget_opts.border_width);
  lives_widget_set_can_focus_and_default (button);

  textwindow->clear_button = lives_button_new_with_mnemonic (_("Clear _Errors"));

  lives_dialog_add_action_widget (LIVES_DIALOG (textwindow->dialog), textwindow->clear_button, GTK_RESPONSE_CANCEL);

  g_signal_connect (GTK_OBJECT (textwindow->clear_button), "clicked",
		    G_CALLBACK (on_lerrors_clear_clicked),
		    GINT_TO_POINTER(FALSE));

  lives_container_set_border_width (LIVES_CONTAINER (textwindow->clear_button), widget_opts.border_width);
  lives_widget_set_can_focus_and_default (textwindow->clear_button);

  textwindow->delete_button = lives_button_new_with_mnemonic (_("_Delete affected layouts"));

  lives_dialog_add_action_widget (LIVES_DIALOG (textwindow->dialog), textwindow->delete_button, GTK_RESPONSE_CANCEL);

  lives_container_set_border_width (LIVES_CONTAINER (textwindow->delete_button), widget_opts.border_width);
  lives_widget_set_can_focus_and_default (textwindow->delete_button);

  g_signal_connect (GTK_OBJECT (textwindow->delete_button), "clicked",
		    G_CALLBACK (on_lerrors_delete_clicked),
		    NULL);

  lives_widget_show_all(textwindow->dialog);
  
}



void
on_rename_activate                    (GtkMenuItem     *menuitem,
				       gpointer         user_data)
{
  renamew=create_rename_dialog(1);
  lives_widget_show(renamew->dialog);

}


void
on_rename_set_name                   (GtkButton       *button,
				      gpointer         user_data)
{
  gchar title[256];
  boolean bad_header=FALSE;

  if (user_data==NULL) {
    g_snprintf(title,256,"%s",lives_entry_get_text(LIVES_ENTRY(renamew->entry)));
    lives_widget_destroy(renamew->dialog);
    g_free(renamew);
  }
  else g_snprintf(title,256,"%s",(gchar *)user_data);

  if (!(strlen(title))) return;

  set_menu_text(cfile->menuentry,title,FALSE);
  g_snprintf (cfile->name,256,"%s",title);

  if (user_data==NULL) {
    set_main_title(title,0);
  }

  save_clip_value(mainw->current_file,CLIP_DETAILS_CLIPNAME,cfile->name);
  if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;

  if (bad_header) do_header_write_error(mainw->current_file);
  cfile->was_renamed=TRUE;
}


void on_toy_activate  (GtkMenuItem *menuitem, gpointer user_data) {
#ifdef ENABLE_OSC
#ifndef IS_MINGW
  gchar string[PATH_MAX];
#endif
  gchar *com;
#endif

  if (menuitem!=NULL&&mainw->toy_type==GPOINTER_TO_INT(user_data)) {
    // switch is off
    user_data=GINT_TO_POINTER(LIVES_TOY_NONE);
  }

  switch (mainw->toy_type) {
    // old status
  case LIVES_TOY_AUTOLIVES:
    g_signal_handler_block (mainw->toy_autolives, mainw->toy_func_autolives);
    lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->toy_autolives),FALSE);
    g_signal_handler_unblock (mainw->toy_autolives, mainw->toy_func_autolives);

    if (mainw->toy_alives_pgid>1) {
      lives_killpg(mainw->toy_alives_pgid,LIVES_SIGHUP);
    }

    // switch off rte so as not to cause alarm
    if (mainw->autolives_reset_fx) 
      rte_on_off_callback(NULL,NULL,0,(GdkModifierType)0,GINT_TO_POINTER(0));
    mainw->autolives_reset_fx=FALSE;
    break;

  case LIVES_TOY_MAD_FRAMES:
    g_signal_handler_block (mainw->toy_random_frames, mainw->toy_func_random_frames);
    lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->toy_random_frames),FALSE);
    g_signal_handler_unblock (mainw->toy_random_frames, mainw->toy_func_random_frames);
    if (mainw->playing_file>-1) {
      if (mainw->faded) {
	lives_widget_hide(mainw->start_image);
	lives_widget_hide(mainw->end_image);
      }
      load_start_image (cfile->start);
      load_end_image (cfile->end);
    }
    break;
  case LIVES_TOY_TV:
    g_signal_handler_block (mainw->toy_tv, mainw->toy_func_lives_tv);
    lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->toy_tv),FALSE);
    g_signal_handler_unblock (mainw->toy_tv, mainw->toy_func_lives_tv);
    break;
  default:
    g_signal_handler_block (mainw->toy_none, mainw->toy_func_none);
    lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->toy_none),FALSE);
    g_signal_handler_unblock (mainw->toy_none, mainw->toy_func_none);
    break;
  }

  mainw->toy_type=(lives_toy_t)GPOINTER_TO_INT(user_data);

  switch (mainw->toy_type) {
  case LIVES_TOY_NONE:
    g_signal_handler_block (mainw->toy_none, mainw->toy_func_none);
    lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->toy_none),TRUE);
    g_signal_handler_unblock (mainw->toy_none, mainw->toy_func_none);
    return;
#ifdef ENABLE_OSC
  case LIVES_TOY_AUTOLIVES:
    if (mainw->current_file<1) {
      do_autolives_needs_clips_error();
      lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->toy_none),TRUE);
      return;
    }

    if ((mainw->current_file>-1&&cfile!=NULL&&(
					       cfile->event_list!=NULL || 
					       cfile->opening
					       )) ||
	mainw->multitrack!=NULL || 
	mainw->is_processing ||
	mainw->preview
	) {
      // ignore if doing something more important
    
      on_toy_activate(NULL, GINT_TO_POINTER(LIVES_TOY_NONE));
      return;
    }

#ifndef IS_MINGW
    // search for autolives.pl
    if (!capable->has_autolives) {
      get_location("autolives.pl",string,PATH_MAX);
      if (strlen(string)) capable->has_autolives=TRUE;
      else {
	do_no_autolives_error();
	on_toy_activate(NULL, GINT_TO_POINTER(LIVES_TOY_NONE));
	return;
      }
    }
#endif

    // chek if osc is started; if not ask permission
    if (!prefs->osc_udp_started) {
      if (!lives_ask_permission(LIVES_PERM_OSC_PORTS)) {
	// permission not given
	on_toy_activate(NULL, GINT_TO_POINTER(LIVES_TOY_NONE));
	return;
      }

      // try: start up osc
      prefs->osc_udp_started=lives_osc_init(prefs->osc_udp_port);
      if (!prefs->osc_udp_started) {
	on_toy_activate(NULL, GINT_TO_POINTER(LIVES_TOY_NONE));
	return;
      }
    }

    // TODO *** store full fx state and restore it
    if (mainw->rte==EFFECT_NONE) {
      mainw->autolives_reset_fx=TRUE;
    }

#ifndef IS_MINGW
    com=g_strdup_printf("autolives.pl localhost %d %d >/dev/null 2>&1",prefs->osc_udp_port,prefs->osc_udp_port-1);
#else
    com=g_strdup_printf("START /MIN /B perl \"%s\\bin\\autolives.pl\" localhost %d %d >NUL 2>&1",
			prefs->prefix_dir,prefs->osc_udp_port,prefs->osc_udp_port-1);
#endif

    mainw->toy_alives_pgid=lives_fork(com);

    g_free(com);

    break;
#endif
  case LIVES_TOY_MAD_FRAMES:
    break;
  case LIVES_TOY_TV:
    // load in the lives TV clip
    deduce_file (LIVES_TV_CHANNEL1,0.,0);

    // if we choose to discard it, discard it....otherwise keep it
    if (prefs->discard_tv) {
      close_current_file(0);
    }
    else {
      // keep it
      int current_file=mainw->current_file;
      gchar *com=g_strdup_printf("%s commit_audio \"%s\"",prefs->backend,cfile->handle);
      cfile->start=1;
      get_frame_count(mainw->current_file);
      cfile->end=cfile->frames;
      cfile->opening=cfile->opening_loc=cfile->opening_audio=cfile->opening_only_audio=FALSE;
      cfile->is_loaded=TRUE;
      mainw->com_failed=FALSE;
      lives_system(com,FALSE);
      save_clip_values(current_file);
      if (prefs->crash_recovery) add_to_recovery_file(cfile->handle);
      switch_to_file((mainw->current_file=0),current_file);
      sensitize();
    }
    break;
  default:
    if (mainw->faded&&!mainw->foreign) {
      lives_widget_show(mainw->start_image);
      lives_widget_show(mainw->end_image);
    }
  }
}



void
on_preview_spinbutton_changed          (GtkSpinButton   *spinbutton,
					gpointer         user_data)
{
  // update the play window preview
  int preview_frame=lives_spin_button_get_value_as_int(spinbutton);
  if ((preview_frame)==mainw->preview_frame) return;
  mainw->preview_frame=preview_frame;
  load_preview_image(TRUE);
}

void
on_prv_link_toggled                (GtkToggleButton *togglebutton,
				    gpointer         user_data)
{
  if (!lives_toggle_button_get_active(togglebutton)) return;
  mainw->prv_link=GPOINTER_TO_INT(user_data);
  if (mainw->is_processing&&(mainw->prv_link==PRV_START||mainw->prv_link==PRV_END)) {
    // block spinbutton in play window
    lives_widget_set_sensitive(mainw->preview_spinbutton,FALSE);
  }
  else {
    lives_widget_set_sensitive(mainw->preview_spinbutton,TRUE);
  }
  load_preview_image(FALSE);
  lives_widget_grab_focus (mainw->preview_spinbutton);
}

void
on_spinbutton_start_value_changed          (GtkSpinButton   *spinbutton,
					    gpointer         user_data)
{
  int start,ostart=cfile->start;

  if (mainw->playing_file==-1&&mainw->current_file==0) return;

  if ((start=lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(spinbutton)))==cfile->start) return;
  cfile->start=start;

  if (mainw->selwidth_locked) {
    cfile->end+=cfile->start-ostart;
    if (cfile->end>cfile->frames) {
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_start),cfile->start-cfile->end+cfile->frames);
    }
    else {
      mainw->selwidth_locked=FALSE;
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_end),cfile->end);
      load_end_image (cfile->end);
      mainw->selwidth_locked=TRUE;
    }
  }

  else {
    if ((cfile->start==1||cfile->end==cfile->frames)&&!(cfile->start==1&&cfile->end==cfile->frames)) {
      lives_widget_set_sensitive(mainw->select_invert,TRUE);
    }
    else {
      lives_widget_set_sensitive(mainw->select_invert,FALSE);
    }
  }

  load_start_image(cfile->start);

  if (cfile->start>cfile->end) {
    lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_end),cfile->start);
  }
  set_sel_label(mainw->sel_label);
  get_play_times();

  if (mainw->playing_file==-1&&mainw->play_window!=NULL&&cfile->is_loaded) {
    if (mainw->prv_link==PRV_START&&mainw->preview_frame!=cfile->start)
      load_preview_image(FALSE);
  }
}



void
on_spinbutton_end_value_changed          (GtkSpinButton   *spinbutton,
					  gpointer         user_data)
{
  int end,oend=cfile->end;

  if (mainw->playing_file==-1&&mainw->current_file==0) return;


  if ((end=lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(spinbutton)))==cfile->end) return;
  cfile->end=end;

  if (mainw->selwidth_locked) {
    cfile->start+=cfile->end-oend;
    if (cfile->start<1) {
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_end),cfile->end-cfile->start+1);
    }
    else {
      mainw->selwidth_locked=FALSE;
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_start),cfile->start);
      load_start_image (cfile->start);
      mainw->selwidth_locked=TRUE;
    }
  }

  else {
    if ((cfile->start==1||cfile->end==cfile->frames)&&!(cfile->start==1&&cfile->end==cfile->frames)) {
      lives_widget_set_sensitive(mainw->select_invert,TRUE);
    }
    else {
      lives_widget_set_sensitive(mainw->select_invert,FALSE);
    }
  }


  load_end_image(cfile->end);

  if (cfile->end<cfile->start) {
    lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_start),cfile->end);
  }

  set_sel_label(mainw->sel_label);
  get_play_times();

  if (mainw->playing_file==-1&&mainw->play_window!=NULL&&cfile->is_loaded) {
    if (mainw->prv_link==PRV_END&&mainw->preview_frame!=cfile->end)
      load_preview_image(FALSE);
  }

}


// for the timer bars

#if GTK_CHECK_VERSION(3,0,0)
boolean expose_vid_event (GtkWidget *widget, lives_painter_t *cr, gpointer user_data) {
  GdkEventExpose *event=NULL;
  boolean dest_cr=FALSE;
#else
boolean expose_vid_event (GtkWidget *widget, GdkEventExpose *event) {
  lives_painter_t *cr=lives_painter_create_from_widget(mainw->video_draw);
  boolean dest_cr=TRUE;
#endif

  int ex,ey,ew,eh;
  int width;

  if (mainw->recoverable_layout) return FALSE;

  if (mainw->draw_blocked) return TRUE;

  if (!prefs->show_gui||mainw->multitrack!=NULL||
      (mainw->fs&&(prefs->play_monitor==prefs->gui_monitor||
		   prefs->play_monitor==0)&&mainw->playing_file>-1&&
       !(mainw->ext_playback&&!(mainw->vpp->capabilities&VPP_LOCAL_DISPLAY)))||
      mainw->foreign) {
    return FALSE;
  }

  if ((event!=NULL&&event->count>0)) return TRUE;

  if (event!=NULL) {
    ex=event->area.x;
    ey=event->area.y;
    ew=event->area.width;
    eh=event->area.height;
  }
  else {
    ex=ey=0;
    ew=lives_widget_get_allocation_width(mainw->video_draw);
    eh=lives_widget_get_allocation_height(mainw->video_draw);
  }



  if (mainw->video_drawable!=NULL) {
    // check if a resize happened

    width = lives_painter_image_surface_get_width (mainw->video_drawable);

    if (width!=lives_widget_get_allocation_width(mainw->LiVES)) {
      lives_painter_surface_destroy(mainw->video_drawable);
      mainw->video_drawable=NULL;
    }
  }

  if (mainw->video_drawable==NULL) {
    mainw->video_drawable = lives_painter_surface_create_from_widget(mainw->video_draw,
								       LIVES_PAINTER_CONTENT_COLOR,
								       lives_widget_get_allocation_width(mainw->video_draw),
								       lives_widget_get_allocation_height(mainw->video_draw));
    block_expose();
    get_play_times();
    unblock_expose();


  }

  if (mainw->current_file==-1) {
    lives_painter_t *cr2=lives_painter_create(mainw->video_drawable);

    lives_painter_set_source_to_bg(cr2,mainw->video_draw);
    lives_painter_rectangle(cr2,0,0,
		    lives_widget_get_allocation_width(mainw->video_draw),
		    lives_widget_get_allocation_height(mainw->video_draw));
    lives_painter_fill(cr2);
    lives_painter_destroy(cr2);
  }

  lives_painter_set_source_surface (cr, mainw->video_drawable,0.,0.);
  lives_painter_rectangle (cr,ex,ey,ew,eh);
  lives_painter_paint(cr);

  unblock_expose();

  if (dest_cr) lives_painter_destroy(cr);

  return TRUE;
  
}


static void redraw_laudio(lives_painter_t *cr, int ex, int ey, int ew, int eh) {
  int width;

  block_expose();

  if (mainw->laudio_drawable!=NULL) {
    // check if a resize happened

    width = lives_painter_image_surface_get_width (mainw->laudio_drawable);

    if (width!=lives_widget_get_allocation_width(mainw->LiVES)) {
      lives_painter_surface_destroy(mainw->laudio_drawable);
      mainw->laudio_drawable=NULL;
    }
  }

  if (mainw->laudio_drawable==NULL) {
    mainw->laudio_drawable = lives_painter_surface_create_from_widget(mainw->laudio_draw,
							       LIVES_PAINTER_CONTENT_COLOR,
							       lives_widget_get_allocation_width(mainw->laudio_draw),
							       lives_widget_get_allocation_height(mainw->laudio_draw));

    block_expose();
    get_play_times();
    unblock_expose();

    if (1||mainw->current_file==-1) mainw->blank_laudio_drawable=mainw->laudio_drawable;
    else cfile->laudio_drawable=mainw->laudio_drawable;

  }


  if (mainw->current_file==-1) {
    lives_painter_t *cr2=lives_painter_create(mainw->laudio_drawable);
    lives_painter_set_source_to_bg(cr2,mainw->laudio_draw);
    lives_painter_rectangle(cr2,0,0,
		    lives_widget_get_allocation_width(mainw->laudio_draw),
		    lives_widget_get_allocation_height(mainw->laudio_draw));
    lives_painter_fill(cr2);
    lives_painter_destroy(cr2);
  }

  lives_painter_set_source_surface (cr, mainw->laudio_drawable,0.,0.);
  lives_painter_rectangle (cr,ex,ey,ew,eh);
  lives_painter_paint(cr);

  unblock_expose();
}




static void redraw_raudio(lives_painter_t *cr, int ex, int ey, int ew, int eh) {

  int width;

  block_expose();

  if (mainw->raudio_drawable!=NULL) {
    // check if a resize happened

    width = lives_painter_image_surface_get_width (mainw->raudio_drawable);

    if (width!=lives_widget_get_allocation_width(mainw->LiVES)) {
      lives_painter_surface_destroy(mainw->raudio_drawable);
      mainw->raudio_drawable=NULL;
    }
  }

  if (mainw->raudio_drawable==NULL) {
    mainw->raudio_drawable = lives_painter_surface_create_from_widget(mainw->raudio_draw,
							       LIVES_PAINTER_CONTENT_COLOR,
							       lives_widget_get_allocation_width(mainw->raudio_draw),
							       lives_widget_get_allocation_height(mainw->raudio_draw));


    block_expose();
    get_play_times();
    unblock_expose();
  }

  // TODO - *** 
  if (mainw->current_file==-1) {
    lives_painter_t *cr2=lives_painter_create(mainw->raudio_drawable);
    lives_painter_set_source_to_bg(cr2,mainw->raudio_draw);

    lives_painter_rectangle(cr2,0,0,
		    lives_widget_get_allocation_width(mainw->raudio_draw),
		    lives_widget_get_allocation_height(mainw->raudio_draw));
    lives_painter_fill(cr2);
    lives_painter_destroy(cr2);

  }


  lives_painter_set_source_surface (cr, mainw->raudio_drawable, 0., 0.);
  lives_painter_rectangle (cr,ex,ey,ew,eh);
  lives_painter_paint(cr);

  if (1||mainw->current_file==-1) mainw->blank_raudio_drawable=mainw->raudio_drawable;
  else cfile->raudio_drawable=mainw->raudio_drawable;

  unblock_expose();
}





#if GTK_CHECK_VERSION(3,0,0)
boolean expose_laud_event (GtkWidget *widget, lives_painter_t *cr, gpointer user_data) {
  GdkEventExpose *event=NULL;
  boolean need_cr=TRUE;
#else
boolean expose_laud_event (GtkWidget *widget, GdkEventExpose *event) {
  lives_painter_t *cr;
  boolean need_cr=TRUE;
#endif
  int ex,ey,ew,eh;

  if (mainw->recoverable_layout) return FALSE;

  if (mainw->draw_blocked) return TRUE;

  if (!prefs->show_gui||mainw->multitrack!=NULL||
      (mainw->fs&&(prefs->play_monitor==prefs->gui_monitor||
		   prefs->play_monitor==0)&&mainw->playing_file>-1&&
       !(mainw->ext_playback&&!(mainw->vpp->capabilities&VPP_LOCAL_DISPLAY)))||
      mainw->foreign) {
    return FALSE;
  }

  if ((event!=NULL&&event->count>0)) return TRUE;

  if (event!=NULL) {
    ex=event->area.x;
    ey=event->area.y;
    ew=event->area.width;
    eh=event->area.height;
  }
  else {
    ex=ey=0;
    ew=lives_widget_get_allocation_width(mainw->laudio_draw);
    eh=lives_widget_get_allocation_height(mainw->laudio_draw);
  }

  if (need_cr) cr=lives_painter_create_from_widget(mainw->laudio_draw);

  redraw_laudio(cr,ex,ey,ew,eh);

  if (need_cr) lives_painter_destroy(cr);

  return TRUE;
  
}



#if GTK_CHECK_VERSION(3,0,0)
boolean expose_raud_event (GtkWidget *widget, lives_painter_t *cr, gpointer user_data) {
  GdkEventExpose *event=NULL;
  boolean need_cr=FALSE;
#else
boolean expose_raud_event (GtkWidget *widget, GdkEventExpose *event) {
  lives_painter_t *cr;
  boolean need_cr=TRUE;
#endif
  int ex,ey,ew,eh;

  if (mainw->recoverable_layout) return FALSE;

  if (mainw->draw_blocked) return TRUE;

  if (!prefs->show_gui||mainw->multitrack!=NULL||
      (mainw->fs&&(prefs->play_monitor==prefs->gui_monitor||
		   prefs->play_monitor==0)&&mainw->playing_file>-1&&
       !(mainw->ext_playback&&!(mainw->vpp->capabilities&VPP_LOCAL_DISPLAY)))||
      mainw->foreign) {
    return FALSE;
  }

  if ((event!=NULL&&event->count>0)) return TRUE;

  if (event!=NULL) {
    ex=event->area.x;
    ey=event->area.y;
    ew=event->area.width;
    eh=event->area.height;
  }
  else {
    ex=ey=0;
    ew=lives_widget_get_allocation_width(mainw->raudio_draw);
    eh=lives_widget_get_allocation_height(mainw->raudio_draw);
  }

  if (need_cr) cr=lives_painter_create_from_widget(mainw->raudio_draw);

  redraw_raudio(cr,ex,ey,ew,eh);

  if (need_cr) lives_painter_destroy(cr);

  return TRUE;
  
}



boolean config_event (GtkWidget *widget, GdkEventConfigure *event, gpointer user_data) {
  int scr_width=mainw->mgeom[prefs->gui_monitor>0?prefs->gui_monitor-1:0].width;
  int scr_height=mainw->mgeom[prefs->gui_monitor>0?prefs->gui_monitor-1:0].height;

  if (mainw->is_ready) {
    if (scr_width!=mainw->scr_width||scr_height!=mainw->scr_height) {
      mainw->scr_width=scr_width;
      mainw->scr_height=scr_height;
      resize_widgets_for_monitor(FALSE);
    }

    if (mainw->current_file>-1&&!mainw->recoverable_layout) {
      get_play_times();
    }
  }

  if (!mainw->is_ready) {
    mainw->scr_width=scr_width;
    mainw->scr_height=scr_height;

    if (prefs->startup_interface==STARTUP_CE) {

#ifdef ENABLE_JACK
      if (mainw->jackd!=NULL) {
	jack_driver_activate(mainw->jackd);
      }
#endif
#ifdef HAVE_PULSE_AUDIO
      if (mainw->pulsed!=NULL) {
	pulse_driver_activate(mainw->pulsed);
      }
#endif
    }
    mainw->is_ready=TRUE;
    if (palette->style&STYLE_1) widget_opts.apply_theme=TRUE;
    resize(1);
  }
  return FALSE;
}



// these two really belong with the processing widget



void on_effects_paused (GtkButton *button, gpointer user_data) {
  gchar *com=NULL;
  int64_t xticks;
#ifdef IS_MINGW
  int pid;
#endif

  if (mainw->iochan!=NULL||cfile->opening) {
    // pause during encoding (if we start using mainw->iochan for other things, this will
    // need changing...)

    if (!mainw->effects_paused) {
#ifndef IS_MINGW
      com=g_strdup_printf("%s stopsubsub \"%s\" SIGTSTP 2>/dev/null",prefs->backend_sync,cfile->handle);
      lives_system(com,TRUE);
#else
      FILE *rfile;
      ssize_t rlen;
      char val[16];

      // get pid from backend
      com=g_strdup_printf("%s get_pid_for_handle \"%s\"",prefs->backend_sync,cfile->handle);
      rfile=popen(com,"r");
      rlen=fread(val,1,16,rfile);
      pclose(rfile);
      memset(val+rlen,0,1);
      pid=atoi(val);

      lives_win32_suspend_resume_process(pid,TRUE);
#endif
      g_free(com);
      com=NULL;

      if (!cfile->opening) {
	lives_button_set_label(LIVES_BUTTON(button),_ ("Resume"));
	lives_label_set_text(LIVES_LABEL(cfile->proc_ptr->label2),_ ("\nPaused\n(click Resume to continue processing)"));
	d_print(_ ("paused..."));
      }
    }

    else {
#ifndef IS_MINGW
      com=g_strdup_printf("%s stopsubsub \"%s\" SIGCONT 2>/dev/null",prefs->backend_sync,cfile->handle);
      lives_system(com,TRUE);
#else
      FILE *rfile;
      ssize_t rlen;
      char val[16];

      // get pid from backend
      com=g_strdup_printf("%s get_pid_for_handle \"%s\"",prefs->backend_sync,cfile->handle);
      rfile=popen(com,"r");
      rlen=fread(val,1,16,rfile);
      pclose(rfile);
      memset(val+rlen,0,1);
      pid=atoi(val);

      lives_win32_suspend_resume_process(pid,FALSE);
#endif
      g_free(com);
      com=NULL;

      if (!cfile->opening) {
	lives_button_set_label(LIVES_BUTTON(button),_ ("Pause"));
	lives_label_set_text(LIVES_LABEL(cfile->proc_ptr->label2),_ ("\nPlease Wait"));
	d_print(_ ("resumed..."));
      }
    }
  }

  if (mainw->iochan==NULL) {
    // pause during effects processing or opening
    gettimeofday(&tv, NULL);
    xticks=U_SECL*(tv.tv_sec-mainw->origsecs)+tv.tv_usec*U_SEC_RATIO-mainw->origusecs*U_SEC_RATIO;
    
    if (!mainw->effects_paused) {
      mainw->timeout_ticks-=xticks;
      com=g_strdup_printf("%s pause \"%s\"",prefs->backend_sync,cfile->handle);
      if (!mainw->preview) {
	lives_button_set_label(LIVES_BUTTON(button),_ ("Resume"));
	if (!cfile->nokeep) {
	  gchar *tmp,*ltext;

	  if (!cfile->opening) {
	    ltext=g_strdup(_("Keep"));
	  }
	  else {
	    ltext=g_strdup(_("Enough"));
	  }
	  lives_button_set_label(LIVES_BUTTON(cfile->proc_ptr->cancel_button),ltext);
	  lives_label_set_text(LIVES_LABEL(cfile->proc_ptr->label2),
			       (tmp=g_strdup_printf(
						    _ ("\nPaused\n(click %s to keep what you have and stop)\n(click Resume to continue processing)"),
						    ltext)));
	  g_free(tmp);
	  g_free(ltext);
	}
	d_print(_ ("paused..."));
      }
#ifdef RT_AUDIO
      if ((mainw->jackd!=NULL&&mainw->jackd_read!=NULL)||(mainw->pulsed!=NULL&&mainw->pulsed_read!=NULL)) 
	lives_widget_hide(cfile->proc_ptr->stop_button);
#endif
    } else {
      mainw->timeout_ticks+=xticks;
      com=g_strdup_printf("%s resume \"%s\"",prefs->backend_sync,cfile->handle);
      if (!mainw->preview) {
	if (cfile->opening) lives_button_set_label(LIVES_BUTTON(button),_ ("Pause/_Enough"));
	else lives_button_set_label(LIVES_BUTTON(button),_ ("Pause"));
	lives_button_set_label(LIVES_BUTTON(cfile->proc_ptr->cancel_button), _("Cancel"));
	lives_label_set_text(LIVES_LABEL(cfile->proc_ptr->label2),_ ("\nPlease Wait"));
	d_print(_ ("resumed..."));
      }
#ifdef RT_AUDIO
      if ((mainw->jackd!=NULL&&mainw->jackd_read!=NULL)||(mainw->pulsed!=NULL&&mainw->pulsed_read!=NULL)) 
	lives_widget_show(cfile->proc_ptr->stop_button);
#endif
    }

    if (!cfile->opening&&!mainw->internal_messaging
#ifdef RT_AUDIO
	&&!((mainw->jackd!=NULL&&mainw->jackd_read!=NULL)||(mainw->pulsed!=NULL&&mainw->pulsed_read!=NULL))
#endif
	) {
      lives_system(com,FALSE);
    }
  }
  if (com!=NULL) g_free(com);
  mainw->effects_paused=!mainw->effects_paused;

}



void
on_preview_clicked                     (GtkButton       *button,
					gpointer         user_data)
{
  // play an effect/tool preview
  // IMPORTANT: cfile->undo_start and cfile->undo_end determine which frames
  // should be played

  static boolean in_preview_func=FALSE;

  boolean resume_after;
  int ostart=cfile->start;
  int oend=cfile->end;
  gshort oaudp=prefs->audio_player;
  int toy_type=mainw->toy_type;
  boolean ointernal_messaging=mainw->internal_messaging;
  //int i;
  uint64_t old_rte; //TODO - block better
  int64_t xticks;
  int current_file=mainw->current_file;
  weed_plant_t *filter_map=mainw->filter_map; // back this up in case we are rendering
  weed_plant_t *afilter_map=mainw->afilter_map; // back this up in case we are rendering
  weed_plant_t *audio_event=mainw->audio_event;

  if (in_preview_func) {
    // this is a special value of cancel - don't propogate it to "open"
    mainw->cancelled=CANCEL_NO_PROPOGATE;
    return;
  }

  in_preview_func=TRUE;

  mainw->preview=TRUE;
  old_rte=mainw->rte;
  gettimeofday(&tv, NULL);
  xticks=U_SECL*(tv.tv_sec-mainw->origsecs)+tv.tv_usec*U_SEC_RATIO-mainw->origusecs*U_SEC_RATIO;
  mainw->timeout_ticks-=xticks;

  if (mainw->internal_messaging) {
    mainw->internal_messaging=FALSE;
    // for realtime fx previews, we will switch all effects off and restore old
    // value after
    mainw->rte=EFFECT_NONE;
  }

  if (mainw->playing_file==-1) {
    if (cfile->opening) {
      // set vid player to int, and audio player to sox
      
      /*      if (prefs->audio_player==AUD_PLAYER_MPLAYER) {
	switch_aud_to_sox(FALSE);
	}*/
      if (!cfile->opening_only_audio) {
	mainw->toy_type=LIVES_TOY_NONE;
	lives_widget_set_sensitive(mainw->toys,FALSE);
      }
      if (mainw->multitrack==NULL&&prefs->show_gui) lives_widget_show (mainw->LiVES);

      if (mainw->multitrack==NULL&&!cfile->is_loaded) {
	if (mainw->play_window!=NULL) {
	  cfile->is_loaded=TRUE;
	  resize_play_window();
	  cfile->is_loaded=FALSE;
	}
      }
    }

    resume_after=FALSE;

    if (mainw->multitrack!=NULL) {
      mt_prepare_for_playback(mainw->multitrack);
      if (cfile->opening) {
	lives_widget_set_sensitive(mainw->multitrack->playall,FALSE);
	lives_widget_set_sensitive (mainw->m_playbutton, FALSE);
      }
    }

    if (user_data!=NULL) {
      // called from multitrack
      if (mainw->play_window!=NULL) {
	resize_play_window();
      }
      if (mainw->multitrack!=NULL&&mainw->multitrack->is_rendering) {
	mainw->play_start=1;
	mainw->play_end=cfile->frames;
      }
      else {
	mainw->play_start=1;
	mainw->play_end=INT_MAX;
      }
    }
    else {
      if (!mainw->is_processing&&!mainw->is_rendering) {
	mainw->play_start=cfile->start=cfile->undo_start;
	mainw->play_end=cfile->end=cfile->undo_end;
      }
      else {
	mainw->play_start=calc_frame_from_time (mainw->current_file,event_list_get_start_secs (cfile->event_list));
	mainw->play_end=INT_MAX;
      }
    }

    // stop effects processing (if preferred)
    if (prefs->pause_effect_during_preview) {
      if (!(mainw->effects_paused)) {
	on_effects_paused(LIVES_BUTTON(cfile->proc_ptr->pause_button),NULL);
	resume_after=TRUE;
      }
    }

    if (button!=NULL) lives_button_set_label(LIVES_BUTTON(button),_("Stop"));
    if (cfile->proc_ptr!=NULL) {
      lives_widget_set_sensitive(cfile->proc_ptr->pause_button,FALSE);
      lives_widget_set_sensitive(cfile->proc_ptr->cancel_button,FALSE);
    }
    if (!cfile->opening) {
      lives_widget_set_sensitive (mainw->showfct,FALSE);
    }

    desensitize();
   
    if (cfile->opening||cfile->opening_only_audio) {
      lives_widget_hide (cfile->proc_ptr->processing);
      if (mainw->multitrack==NULL&&!cfile->opening_audio) {
	load_start_image (0);
	load_end_image (0);
      }
      resize (1);
    }

    if (ointernal_messaging) {
      lives_system ("sync;sync;sync",TRUE);
    }
    current_file=mainw->current_file;
    resize(1);

    // play the clip
    on_playsel_activate(NULL,NULL);

    if (current_file!=mainw->current_file) {
      if (cfile->proc_ptr!=NULL) {
	mainw->files[current_file]->proc_ptr=cfile->proc_ptr;
	cfile->proc_ptr=NULL;
      }
      if (mainw->is_rendering) {
	mainw->files[current_file]->next_event=cfile->next_event;
	cfile->next_event=NULL;
	mainw->current_file=current_file;
      }
      else switch_to_file ((mainw->current_file=0),current_file);
    }

    // restart effects processing (if necessary)
    if (resume_after) on_effects_paused(LIVES_BUTTON(cfile->proc_ptr->pause_button),NULL);

    // user_data is non-NULL if called from multitrack. We want to preserve the value of cancelled.
    if (user_data==NULL) mainw->cancelled=CANCEL_NONE;

    if (oaudp==AUD_PLAYER_MPLAYER&&prefs->audio_player!=oaudp) {
      switch_aud_to_mplayer(FALSE);
    }

    if (oaudp==AUD_PLAYER_MPLAYER2&&prefs->audio_player!=oaudp) {
      switch_aud_to_mplayer2(FALSE);
    }

    cfile->start=ostart;
    cfile->end=oend;

    mainw->toy_type=(lives_toy_t)toy_type;
    lives_widget_set_sensitive(mainw->toys,TRUE);
    
    if (cfile->proc_ptr!=NULL) {
      // proc_ptr can be NULL if we finished loading with a bg generator running
      lives_widget_show (cfile->proc_ptr->processing);
      lives_button_set_label(LIVES_BUTTON(button),_ ("Preview"));
      lives_widget_set_sensitive(cfile->proc_ptr->pause_button,TRUE);
      lives_widget_set_sensitive(cfile->proc_ptr->cancel_button,TRUE);
    }
    mainw->preview=FALSE;
    desensitize();
    procw_desensitize();

    if (!cfile->opening) {
      lives_widget_set_sensitive (mainw->showfct,TRUE);
    }
    else {
      /*      for (i=1;i<MAX_FILES;i++) {
	if (mainw->files[i]!=NULL) {
	  if (mainw->files[i]->menuentry!=NULL) {
	    lives_widget_set_sensitive (mainw->files[i]->menuentry, TRUE);
	    }}}*/
      if (mainw->play_window!=NULL) {
	resize_play_window();
      }
    }
  }
    
  if (mainw->preview_box!=NULL) lives_widget_set_tooltip_text( mainw->p_playbutton, _("Preview"));
  lives_widget_set_tooltip_text( mainw->m_playbutton,_("Preview"));

  // redraw our bars for the clip 
  if (!mainw->merge) {
    get_play_times();
  }
  if (ointernal_messaging) {
    mainw->internal_messaging=TRUE;

    // switch realtime fx back on
    mainw->rte=old_rte;
  }
  
  if (mainw->play_window!=NULL&&mainw->fs) {
    // this prevents a hang when the separate window is visible
    // it may be the first time we have shown it
    block_expose();
    lives_widget_context_update();
    unblock_expose();
    lives_widget_queue_draw (mainw->LiVES);
  }
  gettimeofday(&tv, NULL);
  xticks=U_SECL*(tv.tv_sec-mainw->origsecs)+tv.tv_usec*U_SEC_RATIO-mainw->origusecs*U_SEC_RATIO;
  mainw->timeout_ticks+=xticks;
  mainw->filter_map=filter_map;
  mainw->afilter_map=afilter_map;
  mainw->audio_event=audio_event;

  if (mainw->multitrack!=NULL) {
    current_file=mainw->current_file;
    mainw->current_file=mainw->multitrack->render_file;
    mt_post_playback(mainw->multitrack);
    mainw->current_file=current_file;
  }

  in_preview_func=FALSE;

}


void changed_fps_during_pb (GtkSpinButton   *spinbutton, gpointer user_data) {
  double new_fps=(double)((int)(lives_spin_button_get_value(LIVES_SPIN_BUTTON(spinbutton))*1000)/1000.);

  if ((!cfile->play_paused&&cfile->pb_fps==new_fps)||(cfile->play_paused&&new_fps==0.)) {
    mainw->period=U_SEC/cfile->pb_fps;
    return;
  }

  cfile->pb_fps=new_fps;

  mainw->period=U_SEC/cfile->pb_fps;

  if (prefs->audio_opts&AUDIO_OPTS_FOLLOW_FPS) {
#ifdef ENABLE_JACK
    if (prefs->audio_player==AUD_PLAYER_JACK&&mainw->jackd!=NULL&&mainw->jackd->playing_file==mainw->current_file&&
	!(mainw->record&&!mainw->record_paused&&prefs->audio_src==AUDIO_SRC_EXT)) {
      
      mainw->jackd->sample_in_rate=cfile->arate*cfile->pb_fps/cfile->fps;
      if (mainw->agen_key==0&&!mainw->agen_needs_reinit&&!has_audio_filters(AF_TYPE_NONA)) {
	mainw->rec_aclip=mainw->current_file;
	mainw->rec_avel=cfile->pb_fps/cfile->fps;
	mainw->rec_aseek=(double)mainw->jackd->seek_pos/(double)(cfile->arate*cfile->achans*cfile->asampsize/8);
      }
    }
#endif

#ifdef HAVE_PULSE_AUDIO
    if (prefs->audio_player==AUD_PLAYER_PULSE&&mainw->pulsed!=NULL&&mainw->pulsed->playing_file==mainw->current_file&&
      !(mainw->record&&!mainw->record_paused&&prefs->audio_src==AUDIO_SRC_EXT)) {

      mainw->pulsed->in_arate=cfile->arate*cfile->pb_fps/cfile->fps;
      if (mainw->agen_key==0&&!mainw->agen_needs_reinit&&!has_audio_filters(AF_TYPE_NONA)) {
	mainw->rec_aclip=mainw->current_file;
	mainw->rec_avel=cfile->pb_fps/cfile->fps;
	mainw->rec_aseek=(double)mainw->pulsed->seek_pos/(double)(cfile->arate*cfile->achans*cfile->asampsize/8);
      }
    }
#endif
  }

  if (cfile->play_paused) {
    cfile->freeze_fps=new_fps;
    freeze_callback(NULL,NULL,0,(GdkModifierType)0,NULL);
    return;
  }

  if (cfile->pb_fps==0.) {
    freeze_callback(NULL,NULL,0,(GdkModifierType)0,NULL);
    return;
  }

}


boolean on_mouse_scroll (GtkWidget *widget, GdkEventScroll  *event, gpointer user_data) {
  uint32_t kstate;
  uint32_t type=1;

  if (!prefs->mouse_scroll_clips||mainw->noswitch) return FALSE;

  if (mainw->multitrack!=NULL) {
    if (event->direction==GDK_SCROLL_UP) mt_prevclip(NULL,NULL,0,(GdkModifierType)0,user_data);
    else if (event->direction==GDK_SCROLL_DOWN) mt_nextclip(NULL,NULL,0,(GdkModifierType)0,user_data);
    return FALSE;
  }

  kstate=event->state;

  if (kstate==LIVES_SHIFT_MASK) type=2; // bg
  else if (kstate==LIVES_CONTROL_MASK) type=0; // fg or bg

  if (event->direction==GDK_SCROLL_UP) prevclip_callback(NULL,NULL,0,(GdkModifierType)0,GINT_TO_POINTER(type));
  else if (event->direction==GDK_SCROLL_DOWN) nextclip_callback(NULL,NULL,0,(GdkModifierType)0,GINT_TO_POINTER(type));
  return FALSE;
}



// next few functions are for the timer bars
boolean
on_mouse_sel_update           (GtkWidget       *widget,
			       GdkEventMotion  *event,
			       gpointer         user_data)
{
  if (mainw->current_file>-1&&mainw->sel_start>0) {
    int x,sel_current;

    lives_widget_get_pointer((LiVESXDevice *)mainw->mgeom[prefs->gui_monitor>0?prefs->gui_monitor-1:0].mouse_device, 
			     mainw->LiVES, &x, NULL);

    if (mainw->sel_move==SEL_MOVE_AUTO) 
      sel_current=calc_frame_from_time3(mainw->current_file,
					(double)x/(double)lives_widget_get_allocation_width(mainw->vidbar)*cfile->total_time);
    else
      sel_current=calc_frame_from_time(mainw->current_file,
				       (double)x/(double)lives_widget_get_allocation_width(mainw->vidbar)*cfile->total_time);


    if (mainw->sel_move==SEL_MOVE_SINGLE) {
      sel_current=calc_frame_from_time3(mainw->current_file,
					(double)x/(double)lives_widget_get_allocation_width(mainw->vidbar)*cfile->total_time);
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_start),sel_current);
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_end),sel_current);
    }

    if (mainw->sel_move==SEL_MOVE_START||(mainw->sel_move==SEL_MOVE_AUTO&&sel_current<mainw->sel_start)) {
      sel_current=calc_frame_from_time(mainw->current_file,
				       (double)x/(double)lives_widget_get_allocation_width(mainw->vidbar)*cfile->total_time);
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_start),sel_current);
    }
    else if (mainw->sel_move==SEL_MOVE_END||(mainw->sel_move==SEL_MOVE_AUTO&&sel_current>mainw->sel_start)) {
      sel_current=calc_frame_from_time2(mainw->current_file,
					(double)x/(double)lives_widget_get_allocation_width(mainw->vidbar)*cfile->total_time);
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_end),sel_current-1);
    }
  }
  return FALSE;
}


boolean
on_mouse_sel_reset           (GtkWidget       *widget,
			      GdkEventButton  *event,
			      gpointer         user_data)
{
  if (mainw->current_file<=0) return FALSE;
  mainw->sel_start=0;
  if (!mainw->mouse_blocked) {
    g_signal_handler_block (mainw->eventbox2,mainw->mouse_fn1);
    mainw->mouse_blocked=TRUE;
  }
  return FALSE;
}


boolean
on_mouse_sel_start           (GtkWidget       *widget,
			      GdkEventButton  *event,
			      gpointer         user_data)
{
  int x;
  if (mainw->current_file<=0) return FALSE;

  lives_widget_get_pointer((LiVESXDevice *)mainw->mgeom[prefs->gui_monitor>0?prefs->gui_monitor-1:0].mouse_device, 
			   mainw->LiVES, &x, NULL);

  mainw->sel_start=calc_frame_from_time(mainw->current_file,
					(double)x/(double)lives_widget_get_allocation_width(mainw->vidbar)*cfile->total_time);

  
  if (event->button==3&&!mainw->selwidth_locked) {
    mainw->sel_start=calc_frame_from_time3(mainw->current_file,
					   (double)x/(double)lives_widget_get_allocation_width(mainw->vidbar)*cfile->total_time);
    lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_start),mainw->sel_start);
    lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_end),mainw->sel_start);
    mainw->sel_move=SEL_MOVE_AUTO;
  }
  
  else {
    if (event->button==2&&!mainw->selwidth_locked) {
      mainw->sel_start=calc_frame_from_time3(mainw->current_file,
					     (double)x/(double)lives_widget_get_allocation_width(mainw->vidbar)*cfile->total_time);
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_start),mainw->sel_start);
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_end),(int)mainw->sel_start);
      mainw->sel_move=SEL_MOVE_SINGLE;
    }
    
    else {
      if (!mainw->selwidth_locked) {
	if ((mainw->sel_start<cfile->end&&((mainw->sel_start-cfile->start)<=(cfile->end-mainw->sel_start)))||
	    mainw->sel_start<cfile->start) {
	  lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_start),mainw->sel_start);
	  mainw->sel_move=SEL_MOVE_START;
	}
	else {
	  mainw->sel_start=calc_frame_from_time2(mainw->current_file,
						 (double)x/(double)lives_widget_get_allocation_width(mainw->vidbar)*cfile->total_time);
	  lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_end),mainw->sel_start-1);
	  mainw->sel_move=SEL_MOVE_END;
	}
      }
      else {
	// locked selection
	if (mainw->sel_start>cfile->end) {
	  // past end
	  if (cfile->end+cfile->end-cfile->start+1<=cfile->frames) {
	    lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_end),cfile->end+cfile->end-cfile->start+1);
	    mainw->sel_move=SEL_MOVE_START;
	  }
	}
	else {
	  if (mainw->sel_start>=cfile->start) {
	    if (mainw->sel_start>cfile->start+(cfile->end-cfile->start+1)/2) {
	      // nearer to end
	      lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_end),mainw->sel_start);
	      mainw->sel_move=SEL_MOVE_END;
	    }
	    else {
	      // nearer to start
	      lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_start),mainw->sel_start);
	      mainw->sel_move=SEL_MOVE_START;
	    }
	  }
	  else {
	    // before start
	    if (cfile->start-cfile->end+cfile->start-1>=1) {
	      lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_end),cfile->start-1);
	      mainw->sel_move=SEL_MOVE_END;
	    }
	  }}}}}
  if (mainw->mouse_blocked) {// stops a warning if the user clicks around a lot...
    g_signal_handler_unblock (mainw->eventbox2,mainw->mouse_fn1);
    mainw->mouse_blocked=FALSE;
  }
  return FALSE;
}


boolean on_hrule_enter (GtkWidget *widget, GdkEventCrossing *event, gpointer user_data) {
  if (mainw->cursor_style!=LIVES_CURSOR_NORMAL) return FALSE;
  lives_set_cursor_style(LIVES_CURSOR_CENTER_PTR,widget);
  return FALSE;
}


boolean
on_hrule_update           (GtkWidget       *widget,
			   GdkEventMotion  *event,
			   gpointer         user_data) {
  int x;
  if (mainw->current_file<=0) return FALSE;

  lives_widget_get_pointer((LiVESXDevice *)mainw->mgeom[prefs->gui_monitor>0?prefs->gui_monitor-1:0].mouse_device, 
			   mainw->LiVES, &x, NULL);
  if (x<0) x=0;

  // figure out where ptr should be even when > cfile->frames
  
  if ((lives_ruler_set_value(LIVES_RULER (mainw->hruler),(cfile->pointer_time=
       calc_time_from_frame(mainw->current_file,calc_frame_from_time
			    (mainw->current_file,(double)x/lives_widget_get_allocation_width(mainw->vidbar)*cfile->total_time)))))<=0.) 
    lives_ruler_set_value(LIVES_RULER (mainw->hruler),(cfile->pointer_time=(double)x/
						       lives_widget_get_allocation_width(mainw->vidbar)*cfile->total_time));
  lives_widget_queue_draw (mainw->hruler);
  get_play_times();
  return FALSE;
}


boolean
on_hrule_reset           (GtkWidget       *widget,
			  GdkEventButton  *event,
			  gpointer         user_data)
{
  //button release
  int x;
  if (mainw->current_file<=0) return FALSE;

  lives_widget_get_pointer((LiVESXDevice *)mainw->mgeom[prefs->gui_monitor>0?prefs->gui_monitor-1:0].mouse_device, 
			   mainw->LiVES, &x, NULL);
  if (x<0) x=0;
  if ((lives_ruler_set_value(LIVES_RULER (mainw->hruler),(cfile->pointer_time=
       calc_time_from_frame(mainw->current_file,
			    calc_frame_from_time(mainw->current_file,(double)x/
						 lives_widget_get_allocation_width(mainw->vidbar)*cfile->total_time)))))<=0.) 
    lives_ruler_set_value(LIVES_RULER (mainw->hruler),(cfile->pointer_time=(double)x/
						       lives_widget_get_allocation_width(mainw->vidbar)*cfile->total_time));

  if (!mainw->hrule_blocked) {
    g_signal_handler_block (mainw->eventbox5,mainw->hrule_func);
    mainw->hrule_blocked=TRUE;
  }
  if (cfile->pointer_time>0.) {
    lives_widget_set_sensitive (mainw->rewind, TRUE);
    lives_widget_set_sensitive (mainw->trim_to_pstart, (cfile->achans*cfile->frames>0));
    lives_widget_set_sensitive (mainw->m_rewindbutton, TRUE);
    if (mainw->preview_box!=NULL) {
      lives_widget_set_sensitive (mainw->p_rewindbutton, TRUE);
    }
  }
  else {
    lives_widget_set_sensitive (mainw->rewind, FALSE);
    lives_widget_set_sensitive (mainw->trim_to_pstart, FALSE);
    lives_widget_set_sensitive (mainw->m_rewindbutton, FALSE);
    if (mainw->preview_box!=NULL) {
      lives_widget_set_sensitive (mainw->p_rewindbutton, FALSE);
    }
  }
  lives_widget_queue_draw (mainw->hruler);
  get_play_times();
  return FALSE;
}


boolean
on_hrule_set           (GtkWidget       *widget,
			GdkEventButton  *event,
			gpointer         user_data)
{
  // button press
  int x;
  int frame;

  if (mainw->current_file<=0) return FALSE;

  lives_widget_get_pointer((LiVESXDevice *)mainw->mgeom[prefs->gui_monitor>0?prefs->gui_monitor-1:0].mouse_device, 
			   mainw->LiVES, &x, NULL);
  if (x<0) x=0;

  frame=calc_frame_from_time(mainw->current_file,(double)x/lives_widget_get_allocation_width(mainw->vidbar)*cfile->total_time);

  if ((lives_ruler_set_value(LIVES_RULER (mainw->hruler),(cfile->pointer_time=
							  calc_time_from_frame(mainw->current_file,frame))))<=0.)
    lives_ruler_set_value(LIVES_RULER (mainw->hruler),(cfile->pointer_time=(double)x/
						       lives_widget_get_allocation_width(mainw->vidbar)*cfile->total_time));
  lives_widget_queue_draw (mainw->hruler);
  get_play_times();
  if (mainw->hrule_blocked) {
    g_signal_handler_unblock (mainw->eventbox5,mainw->hrule_func);
    mainw->hrule_blocked=FALSE;
  }

  if (mainw->playing_file==-1&&mainw->play_window!=NULL&&cfile->is_loaded) {
    if (mainw->prv_link==PRV_PTR&&mainw->preview_frame!=frame)
      load_preview_image(FALSE);
  }

  return FALSE;
}



boolean frame_context (GtkWidget *widget, GdkEventButton *event, gpointer which) {
  //popup a context menu when we right click on a frame
  int frame=0;

  GtkWidget *save_frame_as;
  GtkWidget *menu;

  // check if a file is loaded
  if (mainw->current_file<=0) return FALSE;

  if (mainw->multitrack!=NULL && mainw->multitrack->event_list==NULL) return FALSE;

  // only accept right mouse clicks

  if (event->button!=3) return FALSE;

  if (mainw->multitrack==NULL) {
    switch (GPOINTER_TO_INT (which)) {
    case 1:
      // start frame
      frame=cfile->start;
      break;
    case 2:
      // end frame
      frame=cfile->end;
      break;
    default:
      // preview frame
      frame=mainw->preview_frame;
      break;
    }
  }
  
  menu=lives_menu_new();
  lives_menu_set_title (LIVES_MENU(menu),_("LiVES: Selected frame"));

  if (palette->style&STYLE_1) {
    lives_widget_set_bg_color(menu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
    lives_widget_set_fg_color(menu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars_fore);
  }

  if (cfile->frames>0||mainw->multitrack!=NULL) {
    save_frame_as = lives_menu_item_new_with_mnemonic (_("_Save frame as..."));
    g_signal_connect (GTK_OBJECT (save_frame_as), "activate",
		      G_CALLBACK (save_frame),
		      GINT_TO_POINTER(frame));
    

    if (capable->has_convert&&capable->has_composite)
      lives_container_add (LIVES_CONTAINER (menu), save_frame_as);

  }

  lives_widget_show_all (menu);
  lives_menu_popup (LIVES_MENU(menu), event);

  return FALSE;
}



void on_slower_pressed (GtkButton *button, gpointer user_data) {
  double change=1.,new_fps;

  int type=0;

  lives_clip_t *sfile=cfile;

  if (user_data!=NULL) {
    type=GPOINTER_TO_INT(user_data);
    if (type==2) sfile=mainw->files[mainw->blend_file];
    change=0.1;
  }

  if (mainw->playing_file==-1||mainw->internal_messaging||(mainw->is_processing&&cfile->is_loaded)) return;

  if (mainw->rte_keys!=-1&&user_data==NULL) {
    mainw->blend_factor--;
    weed_set_blend_factor(mainw->rte_keys);
    return;
  }

  if (mainw->record&&!mainw->record_paused&&!(prefs->rec_opts&REC_FPS)) return;
  if (sfile->next_event!=NULL) return;

#define PB_CHANGE_RATE .005

  change*=PB_CHANGE_RATE*sfile->pb_fps;

  if (sfile->pb_fps==0.) return;
  if (sfile->pb_fps>0.) {
    if (sfile->pb_fps<0.1||sfile->pb_fps<change) sfile->pb_fps=change;
    new_fps=sfile->pb_fps-change;
    if (sfile==cfile) lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_pb_fps),new_fps);
    else sfile->pb_fps=new_fps;
  }
  else {
    if (sfile->pb_fps>change) sfile->pb_fps=change;
    if (sfile==cfile) lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_pb_fps),(sfile->pb_fps-change));
    else sfile->pb_fps-=change;
  }

  

}


void on_faster_pressed (GtkButton *button, gpointer user_data) {
  double change=1.;
  int type=0;

  lives_clip_t *sfile=cfile;

  if (user_data!=NULL) {
    type=GPOINTER_TO_INT(user_data);
    if (type==2) sfile=mainw->files[mainw->blend_file];
    change=0.1;
  }

  if (mainw->playing_file==-1||mainw->internal_messaging||(mainw->is_processing&&cfile->is_loaded)) return;

  if (mainw->rte_keys!=-1&&user_data==NULL) {
    mainw->blend_factor++;
    weed_set_blend_factor(mainw->rte_keys);
    return;
  }

  if (sfile->play_paused&&sfile->freeze_fps<0.) {
    sfile->pb_fps=-.00000001;
  }

  if (mainw->record&&!mainw->record_paused&&!(prefs->rec_opts&REC_FPS)) return;
  if (sfile->next_event!=NULL) return;

  change=PB_CHANGE_RATE*(sfile->pb_fps==0.?1.:sfile->pb_fps);

  if (sfile->pb_fps>=0.) {
    if (sfile->pb_fps==FPS_MAX) return;
    if (sfile->pb_fps<0.5) sfile->pb_fps=.5;
    if (sfile->pb_fps>FPS_MAX-change) sfile->pb_fps=FPS_MAX-change;
    if (sfile==cfile) lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_pb_fps),(sfile->pb_fps+change));
    else sfile->pb_fps=sfile->pb_fps+change;
  }
  else {
    if (sfile->pb_fps==-FPS_MAX) return;
    if (sfile->pb_fps<-FPS_MAX-change) sfile->pb_fps=-FPS_MAX-change;
    if (sfile==cfile) lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_pb_fps),(sfile->pb_fps+change));
    else sfile->pb_fps=sfile->pb_fps+change;
  }

}



//TODO - make pref
#define CHANGE_SPEED (cfile->pb_fps*(double)KEY_RPT_INTERVAL/100.)
void on_back_pressed (GtkButton *button, gpointer user_data) {
  if (mainw->playing_file==-1||mainw->internal_messaging||(mainw->is_processing&&cfile->is_loaded)) return;
  if (mainw->record&&!(prefs->rec_opts&REC_FRAMES)) return;
  if (cfile->next_event!=NULL) return;

  mainw->deltaticks-=(int64_t)(CHANGE_SPEED*3*mainw->period);
  mainw->scratch=SCRATCH_BACK;

}

void on_forward_pressed (GtkButton *button, gpointer user_data) {
  if (mainw->playing_file==-1||mainw->internal_messaging||(mainw->is_processing&&cfile->is_loaded)) return;
  if (mainw->record&&!(prefs->rec_opts&REC_FRAMES)) return;
  if (cfile->next_event!=NULL) return;

  mainw->deltaticks+=(int64_t)(CHANGE_SPEED*mainw->period);
  mainw->scratch=SCRATCH_FWD;

}


boolean freeze_callback (GtkAccelGroup *group, GObject *obj, guint keyval, GdkModifierType mod, gpointer user_data) {
  if (mainw->playing_file==-1||(mainw->is_processing&&cfile->is_loaded)) return TRUE;
  if (mainw->record&&!(prefs->rec_opts&REC_FRAMES)) return TRUE;

  if (group!=NULL) mainw->rte_keys=-1;

  if (cfile->play_paused) {
    cfile->pb_fps=cfile->freeze_fps;
    if (cfile->pb_fps!=0.) mainw->period=U_SEC/cfile->pb_fps;
    else mainw->period=INT_MAX;
    cfile->play_paused=FALSE;
  }
  else {
    cfile->freeze_fps=cfile->pb_fps;
    cfile->play_paused=TRUE;
    cfile->pb_fps=0.;
    mainw->deltaticks=0;
    if (!mainw->noswitch&&(cfile->clip_type==CLIP_TYPE_DISK||cfile->clip_type==CLIP_TYPE_FILE)) {
      weed_plant_t *frame_layer=mainw->frame_layer;
      mainw->frame_layer=NULL;
      load_frame_image (cfile->frameno);
      mainw->frame_layer=frame_layer;
    }
  }

  if (group!=NULL) {
    lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_pb_fps),cfile->pb_fps);
  }
  
#ifdef ENABLE_JACK
  if (mainw->jackd!=NULL&&prefs->audio_player==AUD_PLAYER_JACK&&(prefs->jack_opts&JACK_OPTS_NOPLAY_WHEN_PAUSED||
								 prefs->audio_opts&AUDIO_OPTS_FOLLOW_FPS)) {
    if (!cfile->play_paused&&prefs->audio_player==AUD_PLAYER_JACK&&(prefs->audio_opts&AUDIO_OPTS_FOLLOW_FPS)&&
	mainw->jackd!=NULL&&mainw->jackd->playing_file==mainw->current_file) {
      mainw->jackd->sample_in_rate=cfile->arate*cfile->pb_fps/cfile->fps;
    }
    mainw->jackd->is_paused=cfile->play_paused;
  }
  if (cfile->play_paused) jack_pb_stop();
  else jack_pb_start();
#endif
#ifdef HAVE_PULSE_AUDIO
  if (mainw->pulsed!=NULL&&prefs->audio_player==AUD_PLAYER_PULSE&&prefs->audio_opts&AUDIO_OPTS_FOLLOW_FPS) {
    if (!cfile->play_paused&&mainw->pulsed!=NULL&&mainw->pulsed->playing_file==mainw->current_file) {
      mainw->pulsed->in_arate=cfile->arate*cfile->pb_fps/cfile->fps;
    }
    mainw->pulsed->is_paused=cfile->play_paused;
  }
#endif
  
  return TRUE;
}


boolean nervous_callback (GtkAccelGroup *group, GObject *obj, guint keyval, GdkModifierType mod, gpointer clip_number) {
  if (mainw->multitrack!=NULL) return FALSE;
  mainw->nervous=!mainw->nervous;
  return TRUE;
}


boolean show_sync_callback (GtkAccelGroup *group, GObject *obj, guint keyval, GdkModifierType mod, gpointer clip_number) {
  double avsync;
  gchar *msg;

  int last_dprint_file;

  if (mainw->playing_file<0) return FALSE;

  if (cfile->frames==0||cfile->achans==0) return FALSE;

  if (prefs->audio_player==AUD_PLAYER_JACK) {
#ifdef ENABLE_JACK
    if (mainw->jackd!=NULL&&mainw->jackd->in_use) avsync=(double)mainw->jackd->seek_pos/
						    cfile->arate/cfile->achans/cfile->asampsize*8;
    else return FALSE;
#else
    return FALSE;
#endif
  }

  if (prefs->audio_player==AUD_PLAYER_PULSE) {
#ifdef HAVE_PULSE_AUDIO
    if (mainw->pulsed!=NULL&&mainw->pulsed->in_use) avsync=(double)mainw->pulsed->seek_pos/
						      cfile->arate/cfile->achans/cfile->asampsize*8;
    else return FALSE;
#else
    return FALSE;
#endif
  }
  else return FALSE;

  avsync-=(mainw->actual_frame-1.)/cfile->fps;

  msg=g_strdup_printf(_("Audio is ahead of video by %.4f secs. at frame %d, with fps %.4f\n"),
		      avsync,mainw->actual_frame,cfile->pb_fps);
  last_dprint_file=mainw->last_dprint_file;
  mainw->no_switch_dprint=TRUE;
  d_print(msg);
  mainw->no_switch_dprint=FALSE;
  mainw->last_dprint_file=last_dprint_file;
  g_free(msg);
  return TRUE;
}



boolean storeclip_callback (GtkAccelGroup *group, GObject *obj, guint keyval, GdkModifierType mod, gpointer clip_number) {
  // ctrl-fn key will store a clip for higher switching
  int clip=GPOINTER_TO_INT (clip_number)-1;
  register int i;

  if (clip>=FN_KEYS-1) {
    // last fn key will clear all
    for (i=0;i<FN_KEYS-1;i++) {
      mainw->clipstore[i]=0;
    }
    return TRUE;
  }

  if (mainw->clipstore[clip]<1||mainw->files[mainw->clipstore[clip]]==NULL) {
    mainw->clipstore[clip]=mainw->current_file;
  }
  else {
    switch_clip(0,mainw->clipstore[clip]);
  }
  return TRUE;
}



void on_toolbar_hide (GtkButton *button, gpointer user_data) {
  lives_widget_hide (mainw->tb_hbox);
  fullscreen_internal();
  future_prefs->show_tool=FALSE;
}





void on_capture_activate (GtkMenuItem *menuitem, gpointer user_data) {
  int curr_file=mainw->current_file;
  gchar *msg;
  gchar *com;
  gchar **array;
  int response;
  double rec_end_time=-1.;

#if !GTK_CHECK_VERSION(3,0,0)
#ifndef GDK_WINDOWING_X11
  do_blocking_error_dialog(_("\n\nThis function will only work with X11.\nPlease send a patch to get it working on other platforms.\n\n"));
  return;
#endif
#endif

  if (!capable->has_xwininfo) {
    do_blocking_error_dialog(_("\n\nYou must install \"xwininfo\" before you can use this feature\n\n"));
    return;
  }

  if (mainw->first_free_file==-1) {
    too_many_files();
    return;
  }

  if (mainw->multitrack!=NULL) {
    if (mainw->multitrack->idlefunc>0) {
      g_source_remove(mainw->multitrack->idlefunc);
      mainw->multitrack->idlefunc=0;
    }
    mt_desensitise(mainw->multitrack);
  }

  if (prefs->rec_desktop_audio&&((prefs->audio_player==AUD_PLAYER_JACK&&capable->has_jackd)||
				 (prefs->audio_player==AUD_PLAYER_PULSE&&capable->has_pulse_audio))) {
    resaudw=create_resaudw(8,NULL,NULL);
  }
  else {
    resaudw=create_resaudw(9,NULL,NULL);
  }
  response=lives_dialog_run (LIVES_DIALOG (resaudw->dialog));
  
  if (response!=GTK_RESPONSE_OK) {
    lives_widget_destroy (resaudw->dialog);

    if (mainw->multitrack!=NULL) {
      mt_sensitise(mainw->multitrack);
      mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
    }

    return;
  }
  
  if (prefs->rec_desktop_audio&&((prefs->audio_player==AUD_PLAYER_JACK&&capable->has_jackd)||
				 (prefs->audio_player==AUD_PLAYER_PULSE&&capable->has_pulse_audio))) {
    mainw->rec_arate=(int)atoi (lives_entry_get_text(LIVES_ENTRY(resaudw->entry_arate)));
    mainw->rec_achans=(int)atoi (lives_entry_get_text(LIVES_ENTRY(resaudw->entry_achans)));
    mainw->rec_asamps=(int)atoi (lives_entry_get_text(LIVES_ENTRY(resaudw->entry_asamps)));
  
    if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(resaudw->rb_unsigned))) {
      mainw->rec_signed_endian=AFORM_UNSIGNED;
    }
    if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(resaudw->rb_bigend))) {
      mainw->rec_signed_endian|=AFORM_BIG_ENDIAN;
    }
  }
  else {
    mainw->rec_arate=mainw->rec_achans=mainw->rec_asamps=mainw->rec_signed_endian=0;
  }

  mainw->rec_fps=lives_spin_button_get_value(LIVES_SPIN_BUTTON(resaudw->fps_spinbutton));

  if (!lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(resaudw->unlim_radiobutton))) {
    rec_end_time=(lives_spin_button_get_value(LIVES_SPIN_BUTTON(resaudw->hour_spinbutton))*60.
		  +lives_spin_button_get_value(LIVES_SPIN_BUTTON(resaudw->minute_spinbutton)))*60.
      +lives_spin_button_get_value(LIVES_SPIN_BUTTON(resaudw->second_spinbutton));
    mainw->rec_vid_frames=(rec_end_time*mainw->rec_fps+.5);
  }
  else mainw->rec_vid_frames=-1;

  lives_widget_destroy (resaudw->dialog);
  lives_widget_context_update();
  if (resaudw!=NULL) g_free(resaudw);
  resaudw=NULL;
  
  if (prefs->rec_desktop_audio&&mainw->rec_arate<=0&&((prefs->audio_player==AUD_PLAYER_JACK&&capable->has_jackd)||
						      (prefs->audio_player==AUD_PLAYER_PULSE&&capable->has_pulse_audio))) {
    do_audrate_error_dialog();
    return;
  }
  
  if (rec_end_time==0.) {
    do_error_dialog(_("\nRecord time must be greater than 0.\n"));
    if (mainw->multitrack!=NULL) {
      mt_sensitise(mainw->multitrack);
      mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
    }
    return;
  }

  if (mainw->multitrack==NULL) lives_widget_hide(mainw->LiVES);
  else lives_widget_hide(mainw->multitrack->window);

  if (!(do_warning_dialog(_ ("Capture an External Window:\n\nClick on 'OK', then click on any window to capture it\nClick 'Cancel' to cancel\n\n")))) {
    if (prefs->show_gui) {
      if (mainw->multitrack==NULL) lives_widget_show(mainw->LiVES);
      else lives_widget_show(mainw->multitrack->window);
    }
    d_print (_ ("External window was released.\n"));
    if (mainw->multitrack!=NULL) {
      mt_sensitise(mainw->multitrack);
      mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
    }
    return;
  }
  
  // an example of using 'get_temp_handle()' ////////
  if (!get_temp_handle(mainw->first_free_file,TRUE)) {
    if (prefs->show_gui) lives_widget_show(mainw->LiVES);
    lives_widget_context_update();
    if (mainw->multitrack!=NULL) {
      mt_sensitise(mainw->multitrack);
      mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
    }
    return;
  }

  com=g_strdup_printf("%s get_window_id \"%s\"",prefs->backend,cfile->handle);
  mainw->com_failed=FALSE;
  lives_system(com,FALSE);
  g_free(com);

  if (mainw->com_failed) {
    if (prefs->show_gui) lives_widget_show(mainw->LiVES);
    lives_widget_context_update();
    if (mainw->multitrack!=NULL) {
      mt_sensitise(mainw->multitrack);
      mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
    }
    return;
  }

  do_progress_dialog(TRUE,FALSE,_ ("Click on a Window to Capture it\nPress 'q' to stop recording"));

  if (get_token_count(mainw->msg,'|')<6) {
    if (prefs->show_gui) lives_widget_show(mainw->LiVES);
    lives_widget_context_update();
    if (mainw->multitrack!=NULL) {
      mt_sensitise(mainw->multitrack);
      mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
    }
    return;
  }

  array=g_strsplit(mainw->msg,"|",-1);
#if GTK_CHECK_VERSION(3,0,0)
  mainw->foreign_id=(Window)atoi(array[1]);
#else
  mainw->foreign_id=(GdkNativeWindow)atoi(array[1]);
#endif
  mainw->foreign_width=atoi(array[2]);
  mainw->foreign_height=atoi(array[3]);
  mainw->foreign_bpp=atoi(array[4]);
  mainw->foreign_visual=g_strdup(array[5]);
  g_strfreev(array);

  com=g_strdup_printf("%s close \"%s\"",prefs->backend,cfile->handle);
  lives_system(com,TRUE);
  g_free(com);
  g_free(cfile);
  cfile=NULL;
  if (mainw->first_free_file==-1||mainw->first_free_file>mainw->current_file) mainw->first_free_file=mainw->current_file;

  mainw->current_file=curr_file;
  ////////////////////////////////////////

  msg=g_strdup_printf(_ ("\nExternal window captured. Width=%d, height=%d, bpp=%d. *Do not resize*\n\nStop or 'q' to finish.\n(Default of %.3f frames per second will be used.)\n"),
		      mainw->foreign_width,mainw->foreign_height,mainw->foreign_bpp,mainw->rec_fps);
  d_print(msg);
  g_free (msg);

  // start another copy of LiVES and wait for it to return values
  com=g_strdup_printf("%s -capture %d %u %d %d %s %d %d %.4f %d %d %d %d \"%s\"",capable->myname_full,capable->mainpid,
		      (unsigned int)mainw->foreign_id,mainw->foreign_width,mainw->foreign_height,prefs->image_ext,
		      mainw->foreign_bpp,mainw->rec_vid_frames,mainw->rec_fps,mainw->rec_arate,
		      mainw->rec_asamps,mainw->rec_achans,mainw->rec_signed_endian,mainw->foreign_visual);

  // force the dialog to disappear
  lives_widget_context_update();

  lives_system(com,FALSE);

  if (prefs->show_gui) {
    if (mainw->multitrack==NULL) lives_widget_show(mainw->LiVES);
    else lives_widget_show(mainw->multitrack->window);
  }

  mainw->noswitch=TRUE;
  lives_widget_context_update();
  mainw->noswitch=FALSE;

  if (!after_foreign_play()&&mainw->cancelled==CANCEL_NONE) {
    do_error_dialog(_ ("LiVES was unable to capture this window. Sorry.\n"));
    sensitize();
  }

  if (mainw->multitrack!=NULL) {
    polymorph(mainw->multitrack,POLY_NONE);
    polymorph(mainw->multitrack,POLY_CLIPS);
    mt_sensitise(mainw->multitrack);
    mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
  }

}


void on_capture2_activate(void) {
  // this is in the second copy of lives, we are now going to grab frames from the X window
  gchar *capfilename=g_strdup_printf(".capture.%d",mainw->foreign_key);
  gchar *capfile=g_build_filename(prefs->tmpdir,capfilename,NULL);

  gchar buf[32];

  boolean retval;
  int capture_fd;
  register int i;


  retval=prepare_to_play_foreign();

  if (mainw->foreign_visual!=NULL) g_free(mainw->foreign_visual);

  if (!retval) exit (2);

  mainw->record_foreign=TRUE;  // for now...

  play_file();

  // pass the handle and frames back to the caller
  capture_fd=creat(capfile,S_IRUSR|S_IWUSR);
  if (capture_fd<0) {
    g_free(capfile);
    exit(1);
  }

  for (i=1;i<MAX_FILES;i++) {
    if (mainw->files[i]==NULL) break;
    lives_write(capture_fd,mainw->files[i]->handle,strlen(mainw->files[i]->handle),TRUE);
    lives_write(capture_fd,"|",1,TRUE);
    g_snprintf (buf,32,"%d",cfile->frames);
    lives_write(capture_fd,buf,strlen (buf),TRUE);
    lives_write(capture_fd,"|",1,TRUE);
  }

  close(capture_fd);
  g_free(capfilename);
  g_free(capfile);
  exit(0);

}




// TODO - move all encoder related stuff from here and plugins.c into encoders.c
void on_encoder_ofmt_changed (GtkComboBox *combo, gpointer user_data) {
  // change encoder format in the encoder plugin
  gchar **array;
  GList *ofmt_all=NULL;
  int i, counter;
  gchar *new_fmt;

  render_details *rdet = (render_details *)user_data;

  if (rdet == NULL){
      new_fmt = lives_combo_get_active_text(LIVES_COMBO(prefsw->ofmt_combo));
  }
  else{
      new_fmt = lives_combo_get_active_text(LIVES_COMBO(rdet->ofmt_combo));
  }

  if ( !strlen( new_fmt ) || !strcmp( new_fmt, mainw->string_constants[LIVES_STRING_CONSTANT_ANY] ) ){
    g_free(new_fmt);
    return;
  }

  if ((ofmt_all=plugin_request_by_line(PLUGIN_ENCODERS,future_prefs->encoder.name,"get_formats"))!=NULL) {
    // get details for the current format
    counter = 0;
    for (i=0;i<g_list_length(ofmt_all);i++) {
      if (get_token_count ((gchar *)g_list_nth_data (ofmt_all,i),'|')>2) {
	array=g_strsplit ((gchar *)g_list_nth_data (ofmt_all,i),"|",-1);

	if (!strcmp(array[1],new_fmt)) {
	  if (prefsw!=NULL) {
	    g_signal_handler_block(prefsw->ofmt_combo, prefsw->encoder_ofmt_fn);
            lives_combo_set_active_index(LIVES_COMBO(prefsw->ofmt_combo), counter);
	    g_signal_handler_unblock(prefsw->ofmt_combo, prefsw->encoder_ofmt_fn);
	  }
	  if (rdet!=NULL) {
	    g_signal_handler_block(rdet->ofmt_combo, rdet->encoder_ofmt_fn);
            lives_combo_set_active_index(LIVES_COMBO(rdet->ofmt_combo), counter);
	    g_signal_handler_unblock(rdet->ofmt_combo, rdet->encoder_ofmt_fn);
	  }
	  g_snprintf(future_prefs->encoder.of_name,51,"%s",array[0]);
	  g_snprintf(future_prefs->encoder.of_desc,128,"%s",array[1]);
	  
	  future_prefs->encoder.of_allowed_acodecs=atoi(array[2]);
	  g_strfreev (array);
	  break;
	}
	g_strfreev (array);
	counter++;
      }
    }
    if (ofmt_all!=NULL) {
      g_list_free_strings (ofmt_all);
      g_list_free (ofmt_all);
    }
    g_free(new_fmt);
    
    if (rdet!=NULL&&prefsw==NULL) {
      if (strcmp(prefs->encoder.of_name,future_prefs->encoder.of_name)) {
	rdet->enc_changed=TRUE;
	g_snprintf(prefs->encoder.of_name,51,"%s",future_prefs->encoder.of_name);
	g_snprintf(prefs->encoder.of_desc,128,"%s",future_prefs->encoder.of_desc);
	g_snprintf(prefs->encoder.of_restrict,1024,"%s",future_prefs->encoder.of_restrict);
	prefs->encoder.of_allowed_acodecs=future_prefs->encoder.of_allowed_acodecs;
	set_pref("output_type",prefs->encoder.of_name);
      }
    }
    set_acodec_list_from_allowed(prefsw,rdet);
  }
  else {
    do_plugin_encoder_error(future_prefs->encoder.name);
  }
}







// TODO - move all this to audio.c

void on_export_audio_activate (GtkMenuItem *menuitem, gpointer user_data) {

  gchar *com,*tmp;
  int nrate=cfile->arps;
  double start,end;
  int asigned=!(cfile->signed_endian&AFORM_UNSIGNED);

  gchar *filt[]={"*.wav",NULL};
  gchar *filename,*file_name;

  if (cfile->end>0&&!GPOINTER_TO_INT (user_data)) {
    filename = choose_file (strlen(mainw->audio_dir)?mainw->audio_dir:NULL,NULL,
			    filt,LIVES_FILE_CHOOSER_ACTION_SAVE,_ ("Export Selected Audio as..."),NULL);
  }
  else {
    filename = choose_file (strlen(mainw->audio_dir)?mainw->audio_dir:NULL,NULL,
			    filt,LIVES_FILE_CHOOSER_ACTION_SAVE,_ ("Export Audio as..."),NULL);
  }

  if (filename==NULL) return;
  file_name=ensure_extension(filename,".wav");
  g_free (filename);
  
  if (!check_file(file_name,FALSE)) {
    g_free(file_name);
    return;
  }

  // warn if arps!=arate
  if ((prefs->audio_player==AUD_PLAYER_SOX||prefs->audio_player==AUD_PLAYER_JACK||
       prefs->audio_player==AUD_PLAYER_PULSE)&&cfile->arate!=cfile->arps) {
    if (do_warning_dialog(_ ("\n\nThe audio playback speed has been altered for this clip.\nClick 'OK' to export at the new speed, or 'Cancel' to export at the original rate.\n"))) {
      nrate=cfile->arate;
    }
  }
  
  if (cfile->start*cfile->end>0&&!GPOINTER_TO_INT (user_data)) {
    g_snprintf (mainw->msg,256,_ ("Exporting audio frames %d to %d as %s..."),cfile->start,cfile->end,file_name);
    start=calc_time_from_frame (mainw->current_file,cfile->start);
    end=calc_time_from_frame (mainw->current_file,cfile->end);
  }
  else {
    g_snprintf (mainw->msg,256,_ ("Exporting audio as %s..."),file_name);
    start=0.;
    end=0.;
  }

  d_print (mainw->msg);
  
  com=g_strdup_printf ("%s export_audio \"%s\" %.8f %.8f %d %d %d %d %d \"%s\"",prefs->backend,cfile->handle,
		       start,end,cfile->arps,cfile->achans,cfile->asampsize,asigned,nrate,
		       (tmp=g_filename_from_utf8 (file_name,-1,NULL,NULL,NULL)));
  g_free(tmp);
 
  unlink (cfile->info_file);
  mainw->com_failed=FALSE;
  lives_system (com,FALSE);
  g_free (com);

  if (mainw->com_failed) {
    g_free(file_name);
    d_print_failed();
    return;
  }

  cfile->op_dir=g_filename_from_utf8((tmp=get_dir(file_name)),-1,NULL,NULL,NULL);
  g_free(tmp);

  do_progress_dialog (TRUE, FALSE, _("Exporting audio"));

  if (mainw->error) {
    d_print_failed();
    do_error_dialog (mainw->msg);
  }
  else {
    d_print_done();
    g_snprintf (mainw->audio_dir,PATH_MAX,"%s",file_name);
    get_dirname (mainw->audio_dir);
  }
  g_free(file_name);
}


void on_append_audio_activate (GtkMenuItem *menuitem, gpointer user_data) {
  if (!(prefs->warning_mask&WARN_MASK_LAYOUT_ALTER_AUDIO)&&
      (mainw->xlays=layout_audio_is_affected(mainw->current_file,0.))!=NULL) {
    if (!do_layout_alter_audio_warning()) {
      g_list_free_strings(mainw->xlays);
      g_list_free(mainw->xlays);
      mainw->xlays=NULL;
      return;
    }
    add_lmap_error(LMAP_ERROR_ALTER_AUDIO,cfile->name,(gpointer)cfile->layout_map,mainw->current_file,0,0.,
		   cfile->stored_layout_audio>0.);
    g_list_free_strings(mainw->xlays);
    g_list_free(mainw->xlays);
    mainw->xlays=NULL;
  }

  choose_file_with_preview(strlen(mainw->audio_dir)?mainw->audio_dir:NULL,_ ("LiVES: - Append Audio File"),5);

}




void on_ok_append_audio_clicked (GtkFileChooser *chooser, gpointer user_data) {

  gchar *com,*tmp,*tmp2;
  gchar *a_type;

  int asigned=!(cfile->signed_endian&AFORM_UNSIGNED);
  int aendian=!(cfile->signed_endian&AFORM_BIG_ENDIAN);

  g_snprintf(file_name,PATH_MAX,"%s",(tmp=g_filename_to_utf8((tmp2=lives_file_chooser_get_filename(chooser)),
							     -1,NULL,NULL,NULL)));
  g_free(tmp); g_free(tmp2);

  end_fs_preview();
  lives_widget_destroy(LIVES_WIDGET(chooser));

  lives_widget_queue_draw(mainw->LiVES);
  lives_widget_context_update();
  
  a_type=file_name+strlen(file_name)-3;

  g_snprintf(mainw->audio_dir,PATH_MAX,"%s",file_name);
  get_dirname(mainw->audio_dir);

  if (!g_ascii_strncasecmp(a_type,".it",2)||!g_ascii_strncasecmp(a_type,"mp3",3)||!g_ascii_strncasecmp(a_type,"ogg",3)||
      !g_ascii_strncasecmp(a_type,"wav",3)||!g_ascii_strncasecmp(a_type,"mod",3)||!g_ascii_strncasecmp(a_type,"xm",2)) {
    com=g_strdup_printf ("%s append_audio \"%s\" \"%s\" %d %d %d %d %d \"%s\"",prefs->backend,cfile->handle,
			 a_type,cfile->arate,
			 cfile->achans,cfile->asampsize,asigned,aendian,
			 (tmp=g_filename_from_utf8 (file_name,-1,NULL,NULL,NULL)));
    g_free(tmp);
  }
  else {
    do_audio_import_error();
    return;
  }

  g_snprintf (mainw->msg,256,_ ("Appending audio file %s..."),file_name);
  d_print(""); // force switchtext
  d_print (mainw->msg);
 
  unlink (cfile->info_file);
  mainw->com_failed=FALSE;
  lives_system (com,FALSE);
  g_free (com);

  if (mainw->com_failed) return;

  if (!do_progress_dialog (TRUE, TRUE,_ ("Appending audio"))) {
    lives_widget_queue_draw(mainw->LiVES);
    lives_widget_context_update();
    mainw->cancelled=CANCEL_NONE;
    mainw->error=FALSE;
    mainw->com_failed=FALSE;
    unlink (cfile->info_file);
    com=g_strdup_printf ("%s cancel_audio \"%s\"",prefs->backend,cfile->handle);
    lives_system (com,FALSE);
    if (!mainw->com_failed) {
      do_auto_dialog(_("Cancelling"),0);
      check_backend_return(cfile);
    }
    g_free (com);
    reget_afilesize(mainw->current_file);
    get_play_times();
    if (mainw->error) d_print_failed();
    return;
  }

  if (mainw->error) {
    d_print_failed();
    do_error_dialog (mainw->msg);
  }
  else {
    lives_widget_queue_draw(mainw->LiVES);
    lives_widget_context_update();
    com=g_strdup_printf ("%s commit_audio \"%s\"",prefs->backend,cfile->handle);
    mainw->com_failed=FALSE;
    mainw->cancelled=CANCEL_NONE;
    mainw->error=FALSE;
    unlink (cfile->info_file);
    lives_system (com,FALSE);
    g_free (com);

    if (mainw->com_failed) {
      d_print_failed();
      return;
    }

    do_auto_dialog(_("Committing audio"),0);
    check_backend_return(cfile);
    if (mainw->error) {
      d_print_failed();
      if (mainw->cancelled!=CANCEL_ERROR) do_error_dialog (mainw->msg);
    }
    else {
      get_dirname (file_name);
      g_snprintf (mainw->audio_dir,PATH_MAX,"%s",file_name);
      reget_afilesize(mainw->current_file);
      cfile->changed=TRUE;
      get_play_times();
      d_print_done();
    }
  }
  cfile->undo_action=UNDO_APPEND_AUDIO;
  set_undoable (_("Append Audio"),!prefs->conserve_space);
}



void on_trim_audio_activate (GtkMenuItem *menuitem, gpointer user_data) {
  // type 0 == trim selected
  // type 1 == trim to play pointer

  gchar *com,*msg;

  int type=GPOINTER_TO_INT (user_data);

  double start,end;
  boolean has_lmap_error=FALSE;

  if (type==0) {
    start=calc_time_from_frame (mainw->current_file,cfile->start);
    end=calc_time_from_frame (mainw->current_file,cfile->end+1);
  }
  else {
    start=0.;
    end=cfile->pointer_time;
  }

  if (!(prefs->warning_mask&WARN_MASK_LAYOUT_DELETE_AUDIO)) {
    if (end<cfile->laudio_time&&(mainw->xlays=layout_audio_is_affected(mainw->current_file,end))!=NULL) {
      if (!do_warning_dialog
	  (_("\nDeletion will cause missing audio in some multitrack layouts.\nAre you sure you wish to continue ?\n"))) {
	g_list_free_strings(mainw->xlays);
	g_list_free(mainw->xlays);
	mainw->xlays=NULL;
	return;
      }
      add_lmap_error(LMAP_ERROR_DELETE_AUDIO,cfile->name,(gpointer)cfile->layout_map,mainw->current_file,0,end,
		     cfile->stored_layout_audio>end);
      has_lmap_error=TRUE;
      g_list_free_strings(mainw->xlays);
      g_list_free(mainw->xlays);
      mainw->xlays=NULL;
    }
  }

  if (!has_lmap_error&&!(prefs->warning_mask&WARN_MASK_LAYOUT_ALTER_AUDIO)&&
      (mainw->xlays=layout_audio_is_affected(mainw->current_file,0.))!=NULL) {
    if (!do_layout_alter_audio_warning()) {
      g_list_free_strings(mainw->xlays);
      g_list_free(mainw->xlays);
      mainw->xlays=NULL;
      return;
    }
    add_lmap_error(LMAP_ERROR_ALTER_AUDIO,cfile->name,(gpointer)cfile->layout_map,mainw->current_file,0,0.,
		   cfile->stored_layout_audio>0.);
    has_lmap_error=TRUE;
    g_list_free_strings(mainw->xlays);
    g_list_free(mainw->xlays);
    mainw->xlays=NULL;
  }

  if (end>cfile->laudio_time&&end>cfile->raudio_time)
    msg=g_strdup_printf(_ ("Padding audio to %.2f seconds..."),end);
  else
    msg=g_strdup_printf(_ ("Trimming audio from %.2f to %.2f seconds..."),start,end);

  d_print(msg);
  g_free(msg);

  com=g_strdup_printf("%s trim_audio \"%s\" %.8f %.8f %d %d %d %d %d",prefs->backend, cfile->handle, 
		      start, end, cfile->arate, 
		      cfile->achans, cfile->asampsize, !(cfile->signed_endian&AFORM_UNSIGNED),
		      !(cfile->signed_endian&AFORM_BIG_ENDIAN));
  unlink (cfile->info_file);
  mainw->com_failed=FALSE;
  lives_system (com,FALSE);
  g_free(com);

  if (mainw->com_failed) {
    d_print_failed();
    return;
  }

  do_progress_dialog(TRUE, FALSE, _("Trimming/Padding audio"));

  if (mainw->error) {
    d_print_failed();
    return;
  }

  if (!prefs->conserve_space) {
    set_undoable (_("Trim/Pad Audio"),!prefs->conserve_space);
    cfile->undo_action=UNDO_TRIM_AUDIO;
  }

  reget_afilesize(mainw->current_file);
  get_play_times();
  cfile->changed=TRUE;
  d_print_done();
  if (has_lmap_error) popup_lmap_errors(NULL,NULL);

  if (mainw->sl_undo_mem!=NULL&&cfile->stored_layout_audio!=0.) {
    // need to invalidate undo/redo stack, in case file was used in some layout undo
    stored_event_list_free_undos();
  }

}



void on_fade_audio_activate (GtkMenuItem *menuitem, gpointer user_data) {
  // type == 0 fade in
  // type == 1 fade out

  double startt,endt,startv,endv,time=0.;
  gchar *msg,*msg2,*utxt,*com;

  boolean has_lmap_error=FALSE;
  int alarm_handle;
  int type;

  aud_dialog_t *aud_d=NULL;

  if (menuitem!=NULL) {
    cfile->undo1_int=type=GPOINTER_TO_INT(user_data);
    aud_d=create_audfade_dialog(type);
    if (lives_dialog_run(LIVES_DIALOG(aud_d->dialog))==GTK_RESPONSE_CANCEL) {
      lives_widget_destroy(aud_d->dialog);
      g_free(aud_d);
      return;
    }
    
    time=lives_spin_button_get_value(LIVES_SPIN_BUTTON(aud_d->time_spin));
    
    lives_widget_destroy(aud_d->dialog);
  }
  else {
    type=cfile->undo1_int;
  }

  if (menuitem==NULL||!aud_d->is_sel) {
    if (menuitem==NULL) {
      endt=cfile->undo1_dbl;
      startt=cfile->undo2_dbl;
    }
    else {
      if (type==0) {
	cfile->undo2_dbl=startt=0.;
	cfile->undo1_dbl=endt=time;
      }
      else {
	cfile->undo1_dbl=endt=cfile->laudio_time;
	cfile->undo2_dbl=startt=cfile->laudio_time-time;
      }
    }
  }
  else {
    cfile->undo2_dbl=startt=((double)cfile->start-1.)/cfile->fps;
    cfile->undo1_dbl=endt=(double)cfile->end/cfile->fps;
  }
  
 
  if (type==0) {
    startv=0.;
    endv=1.;
    msg2=g_strdup(_("Fading audio in"));
    utxt=g_strdup(_("Fade audio in"));
  }
  else {
    startv=1.;
    endv=0.;
    msg2=g_strdup(_("Fading audio out"));
    utxt=g_strdup(_("Fade audio out"));
  }

  if (menuitem!=NULL) {
    if (!(prefs->warning_mask&WARN_MASK_LAYOUT_ALTER_AUDIO)&&
	(mainw->xlays=layout_audio_is_affected(mainw->current_file,0.))!=NULL) {
      if (!do_layout_alter_audio_warning()) {
	g_free(msg2);
	g_free(utxt);
	g_list_free_strings(mainw->xlays);
	g_list_free(mainw->xlays);
	mainw->xlays=NULL;
	return;
      }
      add_lmap_error(LMAP_ERROR_ALTER_AUDIO,cfile->name,(gpointer)cfile->layout_map,mainw->current_file,0,0.,
		     cfile->stored_layout_audio>0.);
      has_lmap_error=TRUE;
      g_list_free_strings(mainw->xlays);
      g_list_free(mainw->xlays);
      mainw->xlays=NULL;
    }

    if (!aud_d->is_sel) 
      msg=g_strdup_printf(_("%s over %.1f seconds..."),msg2,time); 
    else 
      msg=g_strdup_printf(_("%s from time %.2f seconds to %.2f seconds..."),msg2,startt,endt); 
    d_print(msg);
    g_free(msg);
    g_free(msg2);
  }

  desensitize();
  do_threaded_dialog(_("Fading audio..."),FALSE);
  alarm_handle=lives_alarm_set(1);

  threaded_dialog_spin();
  lives_widget_context_update();
  threaded_dialog_spin();

  if (!prefs->conserve_space) {
    mainw->error=FALSE;
    com=g_strdup_printf("%s backup_audio \"%s\"",prefs->backend_sync,cfile->handle);
    lives_system(com,FALSE);
    g_free(com);

    if (mainw->error) {
      lives_alarm_clear(alarm_handle);
      end_threaded_dialog();
      d_print_failed();
      return;
    }
  }

  aud_fade(mainw->current_file,startt,endt,startv,endv);
  audio_free_fnames();

  while (!lives_alarm_get(alarm_handle)) {
    g_usleep(prefs->sleep_time);
  }

  lives_alarm_clear(alarm_handle);

  end_threaded_dialog();
  d_print_done();

  cfile->changed=TRUE;

  if (!prefs->conserve_space) {
    set_undoable (utxt,TRUE);
    cfile->undo_action=UNDO_FADE_AUDIO;
  }
  g_free(utxt);
  sensitize();

  if (has_lmap_error) popup_lmap_errors(NULL,NULL);
  if (aud_d!=NULL) g_free(aud_d);

}





boolean on_del_audio_activate (GtkMenuItem *menuitem, gpointer user_data) {
  double start,end;
  gchar *com,*msg=NULL;
  boolean has_lmap_error=FALSE;
  boolean bad_header=FALSE;

  if (menuitem==NULL) {
    // undo/redo
    start=cfile->undo1_dbl;
    end=cfile->undo2_dbl;
  }
  else {
    if (GPOINTER_TO_INT(user_data)) {

      if (!(prefs->warning_mask&WARN_MASK_LAYOUT_DELETE_AUDIO)) {
	if ((mainw->xlays=layout_audio_is_affected(mainw->current_file,0.))!=NULL) {
	  if (!do_warning_dialog
	      (_("\nDeletion will cause missing audio in some multitrack layouts.\nAre you sure you wish to continue ?\n"))) {
	    g_list_free_strings(mainw->xlays);
	    g_list_free(mainw->xlays);
	    mainw->xlays=NULL;
	    return FALSE;
	  }
	  add_lmap_error(LMAP_ERROR_DELETE_AUDIO,cfile->name,(gpointer)cfile->layout_map,mainw->current_file,0,0.,
			 cfile->stored_layout_audio>0.);
	  has_lmap_error=TRUE;
	  g_list_free_strings(mainw->xlays);
	  g_list_free(mainw->xlays);
	  mainw->xlays=NULL;
	}
      }
      
      if (!has_lmap_error&&!(prefs->warning_mask&WARN_MASK_LAYOUT_ALTER_AUDIO)&&
	  (mainw->xlays=layout_audio_is_affected(mainw->current_file,0.))!=NULL) {
	if (!do_layout_alter_audio_warning()) {
	  g_list_free_strings(mainw->xlays);
	  g_list_free(mainw->xlays);
	  mainw->xlays=NULL;
	  return FALSE;
	}
	add_lmap_error(LMAP_ERROR_ALTER_AUDIO,cfile->name,(gpointer)cfile->layout_map,mainw->current_file,0,0.,
		       cfile->stored_layout_audio>0.);
	has_lmap_error=TRUE;
	g_list_free_strings(mainw->xlays);
	g_list_free(mainw->xlays);
	mainw->xlays=NULL;
      }

      if (!cfile->frames) {
	if (do_warning_dialog(_ ("\nDeleting all audio will close this file.\nAre you sure ?"))) close_current_file(0);
	return FALSE;
      }
      msg=g_strdup(_ ("Deleting all audio..."));
      start=end=0.;
    }
    else {
      start=calc_time_from_frame (mainw->current_file,cfile->start);
      end=calc_time_from_frame (mainw->current_file,cfile->end+1);
      msg=g_strdup_printf(_ ("Deleting audio from %.2f to %.2f seconds..."),start,end);
      start*=(double)cfile->arate/(double)cfile->arps;
      end*=(double)cfile->arate/(double)cfile->arps;

      if (!(prefs->warning_mask&WARN_MASK_LAYOUT_SHIFT_AUDIO)) {
	if ((mainw->xlays=layout_audio_is_affected(mainw->current_file,end))!=NULL) {
	  if (!do_warning_dialog
	      (_("\nDeletion will cause audio to shift in some multitrack layouts.\nAre you sure you wish to continue ?\n"))) {
	    g_list_free_strings(mainw->xlays);
	    g_list_free(mainw->xlays);
	    mainw->xlays=NULL;
	    return FALSE;
	  }
	  add_lmap_error(LMAP_ERROR_SHIFT_AUDIO,cfile->name,(gpointer)cfile->layout_map,mainw->current_file,0,start,
			 cfile->stored_layout_audio>end);
	  has_lmap_error=TRUE;
	  g_list_free_strings(mainw->xlays);
	  g_list_free(mainw->xlays);
	  mainw->xlays=NULL;
	}
      }

      if (!has_lmap_error&&!(prefs->warning_mask&WARN_MASK_LAYOUT_DELETE_AUDIO)) {
	if ((mainw->xlays=layout_audio_is_affected(mainw->current_file,start))!=NULL) {
	  if (!do_warning_dialog
	      (_("\nDeletion will cause missing audio in some multitrack layouts.\nAre you sure you wish to continue ?\n"))) {
	    g_list_free_strings(mainw->xlays);
	    g_list_free(mainw->xlays);
	    mainw->xlays=NULL;
	    return FALSE;
	  }
	  add_lmap_error(LMAP_ERROR_DELETE_AUDIO,cfile->name,(gpointer)cfile->layout_map,mainw->current_file,0,start,
			 cfile->stored_layout_audio>start);
	  has_lmap_error=TRUE;
	  g_list_free_strings(mainw->xlays);
	  g_list_free(mainw->xlays);
	  mainw->xlays=NULL;
	}
      }
      
      if (!has_lmap_error&&!(prefs->warning_mask&WARN_MASK_LAYOUT_ALTER_AUDIO)&&
	  (mainw->xlays=layout_audio_is_affected(mainw->current_file,0.))!=NULL) {
	if (!do_layout_alter_audio_warning()) {
	  g_list_free_strings(mainw->xlays);
	  g_list_free(mainw->xlays);
	  mainw->xlays=NULL;
	  return FALSE;
	}
	add_lmap_error(LMAP_ERROR_ALTER_AUDIO,cfile->name,(gpointer)cfile->layout_map,mainw->current_file,0,0.,
		       cfile->stored_layout_audio>0.);
	has_lmap_error=TRUE;
	g_list_free_strings(mainw->xlays);
	g_list_free(mainw->xlays);
	mainw->xlays=NULL;
      }

    }

    cfile->undo1_dbl=start;
    cfile->undo2_dbl=end;
  }

  cfile->undo_arate=cfile->arate;
  cfile->undo_signed_endian=cfile->signed_endian;
  cfile->undo_achans=cfile->achans;
  cfile->undo_asampsize=cfile->asampsize;
  cfile->undo_arps=cfile->arps;

  if (msg!=NULL) {
    d_print("");
    d_print(msg);
    g_free(msg);
  }

  com=g_strdup_printf("%s delete_audio \"%s\" %.8f %.8f %d %d %d", prefs->backend, 
		      cfile->handle, start, end, cfile->arps, 
		      cfile->achans, cfile->asampsize);
  unlink (cfile->info_file);
  mainw->com_failed=FALSE;
  lives_system (com,FALSE);
  g_free(com);

  if (mainw->com_failed) {
    if (menuitem!=NULL) d_print_failed();
    return FALSE;
  }

  do_progress_dialog(TRUE, FALSE, _ ("Deleting Audio"));

  if (mainw->error) {
    if (menuitem!=NULL) d_print_failed();
    return FALSE;
  }

  set_undoable (_("Delete Audio"),TRUE);
  cfile->undo_action=UNDO_DELETE_AUDIO;

  reget_afilesize(mainw->current_file);
  get_play_times();
  cfile->changed=TRUE;
  sensitize();

  if (cfile->laudio_time==0.||cfile->raudio_time==0.) {
    if (cfile->laudio_time==cfile->raudio_time) cfile->achans=0;
    else cfile->achans=1;
    save_clip_value(mainw->current_file,CLIP_DETAILS_ACHANS,&cfile->achans);
    if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
    
    if (bad_header) do_header_write_error(mainw->current_file);
  }

  if (menuitem!=NULL) {
    d_print_done();
  }
  if (has_lmap_error) popup_lmap_errors(NULL,NULL);

  if (mainw->sl_undo_mem!=NULL&&cfile->stored_layout_audio!=0.) {
    // need to invalidate undo/redo stack, in case file was used in some layout undo
    stored_event_list_free_undos();
  }

  return TRUE;
}


void
on_rb_audrec_time_toggled                (GtkToggleButton *togglebutton,
					  gpointer         user_data)
{
  _resaudw *resaudw=(_resaudw *)user_data;  
  if (lives_toggle_button_get_active(togglebutton)) {
    lives_widget_set_sensitive(resaudw->hour_spinbutton,TRUE);
    lives_widget_set_sensitive(resaudw->minute_spinbutton,TRUE);
    lives_widget_set_sensitive(resaudw->second_spinbutton,TRUE);
  }
  else {
    lives_widget_set_sensitive(resaudw->hour_spinbutton,FALSE);
    lives_widget_set_sensitive(resaudw->minute_spinbutton,FALSE);
    lives_widget_set_sensitive(resaudw->second_spinbutton,FALSE);
  }
}


void
on_recaudclip_activate (GtkMenuItem     *menuitem,
			gpointer         user_data) {

  if (prefs->audio_player!=AUD_PLAYER_JACK&&prefs->audio_player!=AUD_PLAYER_PULSE) {
    do_nojack_rec_error();
    return;
  }

  mainw->fx1_val=DEFAULT_AUDIO_RATE;
  mainw->fx2_val=DEFAULT_AUDIO_CHANS;
  mainw->fx3_val=DEFAULT_AUDIO_SAMPS;
  mainw->fx4_val=mainw->endian;
  mainw->rec_end_time=-1.;
  resaudw=create_resaudw(5,NULL,NULL);
  lives_widget_show (resaudw->dialog);
}

static boolean has_lmap_error_recsel;

void
on_recaudsel_activate (GtkMenuItem     *menuitem,
			gpointer         user_data) {

  if (prefs->audio_player!=AUD_PLAYER_JACK&&prefs->audio_player!=AUD_PLAYER_PULSE) {
    do_nojack_rec_error();
    return;
  }

  has_lmap_error_recsel=FALSE;
  if ((prefs->warning_mask&WARN_MASK_LAYOUT_ALTER_AUDIO)&&
      (mainw->xlays=layout_audio_is_affected(mainw->current_file,0.))!=NULL) {
    if (!do_layout_alter_audio_warning()) {
      has_lmap_error_recsel=FALSE;
      g_list_free_strings(mainw->xlays);
      g_list_free(mainw->xlays);
      mainw->xlays=NULL;
      return;
    }
    add_lmap_error(LMAP_ERROR_ALTER_AUDIO,cfile->name,(gpointer)cfile->layout_map,mainw->current_file,0,0.,
		   cfile->stored_layout_audio>0.);
    has_lmap_error_recsel=TRUE;
    g_list_free_strings(mainw->xlays);
    g_list_free(mainw->xlays);
    mainw->xlays=NULL;
  }

  mainw->rec_end_time=(cfile->end-cfile->start+1.)/cfile->fps;

  if (cfile->achans>0) {
    mainw->fx1_val=cfile->arate;
    mainw->fx2_val=cfile->achans;
    mainw->fx3_val=cfile->asampsize;
    mainw->fx4_val=cfile->signed_endian;
    resaudw=create_resaudw(7,NULL,NULL);
  }
  else {
    mainw->fx1_val=DEFAULT_AUDIO_RATE;
    mainw->fx2_val=DEFAULT_AUDIO_CHANS;
    mainw->fx3_val=DEFAULT_AUDIO_SAMPS;
    mainw->fx4_val=mainw->endian;
    resaudw=create_resaudw(6,NULL,NULL);
  }
  lives_widget_show (resaudw->dialog);
}



void
on_recaudclip_ok_clicked                      (GtkButton *button,
					       gpointer user_data)
{
#ifdef RT_AUDIO
  weed_timecode_t ins_pt;
  double aud_start,aud_end,vel=1.,vol=1.;

  int asigned=1,aendian=1;
  int old_file=mainw->current_file,new_file;
  int type=LIVES_POINTER_TO_INT(user_data);
  int oachans=0,oarate=0,oarps=0,ose=0,oasamps=0;
  boolean backr=FALSE;

  gchar *com;


  // type == 0 - new clip
  // type == 1 - existing clip

  if (type==1) d_print(""); // show switch message, if appropriate

  mainw->current_file=mainw->first_free_file;
  if (!get_new_handle(mainw->current_file,NULL)) {
    mainw->current_file=old_file;
    return;
  }

  cfile->is_loaded=TRUE;

  cfile->arps=cfile->arate=(int)atoi (lives_entry_get_text(LIVES_ENTRY(resaudw->entry_arate)));
  cfile->achans=(int)atoi (lives_entry_get_text(LIVES_ENTRY(resaudw->entry_achans)));
  cfile->asampsize=(int)atoi (lives_entry_get_text(LIVES_ENTRY(resaudw->entry_asamps)));

  if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(resaudw->unlim_radiobutton))) {
    mainw->rec_end_time=-1.;
    mainw->rec_samples=-1;
  }
  else {
    mainw->rec_end_time=(lives_spin_button_get_value(LIVES_SPIN_BUTTON(resaudw->hour_spinbutton))*60.
			 +lives_spin_button_get_value(LIVES_SPIN_BUTTON(resaudw->minute_spinbutton)))*60.
      +lives_spin_button_get_value(LIVES_SPIN_BUTTON(resaudw->second_spinbutton));
    mainw->rec_samples=mainw->rec_end_time*cfile->arate;
  }

  if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(resaudw->rb_unsigned))) {
    asigned=0;
  }
  if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(resaudw->rb_bigend))) {
    aendian=0;
  }

  mainw->is_processing=TRUE;

  cfile->signed_endian=get_signed_endian(asigned,aendian);
  lives_widget_destroy (resaudw->dialog);
  lives_widget_context_update();
  if (resaudw!=NULL) g_free(resaudw);
  resaudw=NULL;

  if (cfile->arate<=0) {
    do_audrate_error_dialog();
    mainw->is_processing=FALSE;
    close_current_file(old_file);
    return;
  }

  if (mainw->rec_end_time==0.) {
    do_error_dialog(_("\nRecord time must be greater than 0.\n"));
    mainw->is_processing=FALSE;
    close_current_file(old_file);
    return;
  }

  asigned=!asigned;

  if (type==0) {
    g_snprintf(cfile->type,40,"Audio");
    add_to_clipmenu();

#ifdef ENABLE_OSC
    lives_osc_notify(LIVES_OSC_NOTIFY_CLIP_OPENED,"");
#endif

  }

  mainw->effects_paused=FALSE;

  if (type==1) {
    oachans=mainw->files[old_file]->achans;
    oarate=mainw->files[old_file]->arate;
    oarps=mainw->files[old_file]->arps;
    oasamps=mainw->files[old_file]->asampsize;
    ose=mainw->files[old_file]->signed_endian;

    mainw->files[old_file]->arate=mainw->files[old_file]->arps=cfile->arate;
    mainw->files[old_file]->asampsize=cfile->asampsize;
    mainw->files[old_file]->achans=cfile->achans;
    mainw->files[old_file]->signed_endian=cfile->signed_endian;
  }

  mainw->suppress_dprint=TRUE;
  mainw->no_switch_dprint=TRUE;
  
#ifdef ENABLE_JACK
  if (prefs->audio_player==AUD_PLAYER_JACK) {
    jack_rec_audio_to_clip(mainw->current_file,old_file,type==0?RECA_NEW_CLIP:RECA_EXISTING);
  }
#endif
#ifdef HAVE_PULSE_AUDIO
  if (prefs->audio_player==AUD_PLAYER_PULSE) {
    pulse_rec_audio_to_clip(mainw->current_file,old_file,type==0?RECA_NEW_CLIP:RECA_EXISTING);
  }
#endif

  if (type==1) {
    // set these again, as playsel may have reset them
    mainw->files[old_file]->arate=mainw->files[old_file]->arps=cfile->arate;
    mainw->files[old_file]->asampsize=cfile->asampsize;
    mainw->files[old_file]->achans=cfile->achans;
    mainw->files[old_file]->signed_endian=cfile->signed_endian;
  }

  if (type!=1&&mainw->cancelled==CANCEL_USER) {
    mainw->cancelled=CANCEL_NONE;
    if (type==1) {
      mainw->files[old_file]->arps=oarps;
      mainw->files[old_file]->arate=oarate;
      mainw->files[old_file]->achans=oachans;
      mainw->files[old_file]->asampsize=oasamps;
      mainw->files[old_file]->signed_endian=ose;
    }
    mainw->is_processing=FALSE;
    close_current_file(old_file);
    mainw->suppress_dprint=FALSE;
    d_print_cancelled();
    mainw->no_switch_dprint=FALSE;
    return;
  }

  mainw->cancelled=CANCEL_NONE;
  lives_widget_context_update();
  
  if (type==1) {
    // set these again in case reget_afilesize() reset them
    cfile->arate=cfile->arps=mainw->files[old_file]->arate;
    cfile->asampsize=mainw->files[old_file]->asampsize;
    cfile->achans=mainw->files[old_file]->achans;
    cfile->signed_endian=mainw->files[old_file]->signed_endian;

    do_threaded_dialog(_("Committing audio"),FALSE);
    aud_start=0.;
    reget_afilesize(mainw->current_file);
    get_total_time(cfile);
    aud_end=cfile->laudio_time;
    ins_pt=(mainw->files[old_file]->start-1.)/mainw->files[old_file]->fps*U_SEC;

    if (!prefs->conserve_space) {
      mainw->error=FALSE;
      com=g_strdup_printf("%s backup_audio \"%s\"",prefs->backend_sync,mainw->files[old_file]->handle);
      lives_system(com,FALSE);
      g_free(com);

      if (mainw->error) {
	end_threaded_dialog();
	d_print_failed();
	return;
      }

    }


    mainw->read_failed=mainw->write_failed=FALSE;
    if (mainw->read_failed_file!=NULL) g_free(mainw->read_failed_file);
    mainw->read_failed_file=NULL;


    // copy audio from old clip to current
    render_audio_segment(1,&(mainw->current_file),old_file,&vel,&aud_start,ins_pt,
			 ins_pt+(weed_timecode_t)((aud_end-aud_start)*U_SEC),&vol,vol,vol,NULL);

    end_threaded_dialog();
    close_current_file(old_file);

    if (mainw->write_failed) {
      // on failure
      int outfile=(mainw->multitrack!=NULL?mainw->multitrack->render_file:mainw->current_file);
      gchar *outfilename=g_build_filename(prefs->tmpdir,mainw->files[outfile]->handle,"audio",NULL);
      do_write_failed_error_s(outfilename,NULL);
      
      if (!prefs->conserve_space&&type==1) {
	// try to recover backup
	com=g_strdup_printf("%s undo_audio \"%s\"",prefs->backend_sync,mainw->files[old_file]->handle);
	lives_system(com,FALSE);
	g_free(com);
	backr=TRUE;
      }
    }

    if (mainw->read_failed) {
      do_read_failed_error_s(mainw->read_failed_file,NULL);
      if (mainw->read_failed_file!=NULL) g_free(mainw->read_failed_file);
      mainw->read_failed_file=NULL;
      if (!prefs->conserve_space&&type==1&&!backr) {
	// try to recover backup
	com=g_strdup_printf("%s undo_audio \"%s\"",prefs->backend_sync,mainw->files[old_file]->handle);
	lives_system(com,FALSE);
	g_free(com);
      }
    }

  }

  mainw->suppress_dprint=FALSE;
  cfile->changed=TRUE;
  save_clip_values(mainw->current_file);

  mainw->cancelled=CANCEL_NONE;

  new_file=mainw->current_file;
  if (type==0) switch_to_file((mainw->current_file=0),new_file);
  else {
    if (!prefs->conserve_space) {
      set_undoable (_("Record new audio"),TRUE);
      cfile->undo_action=UNDO_REC_AUDIO;
    }
  }

  d_print_done();
  mainw->no_switch_dprint=FALSE;

  if (has_lmap_error_recsel) {
    has_lmap_error_recsel=FALSE;
    popup_lmap_errors(NULL,NULL);
  }
  mainw->is_processing=FALSE;

#endif
}


boolean on_ins_silence_activate (GtkMenuItem *menuitem, gpointer user_data) {
  double start=0,end=0;
  gchar *com,*msg;
  boolean has_lmap_error=FALSE;
  boolean has_new_audio=FALSE;

  if (menuitem==NULL) {
    // redo
    start=cfile->undo1_dbl;
    end=cfile->undo2_dbl;
    cfile->arate=cfile->undo_arate;
    cfile->signed_endian=cfile->undo_signed_endian;
    cfile->achans=cfile->undo_achans;
    cfile->asampsize=cfile->undo_asampsize;
    cfile->arps=cfile->undo_arps;
  }

  if (!cfile->achans) {
    mainw->fx1_val=DEFAULT_AUDIO_RATE;
    mainw->fx2_val=DEFAULT_AUDIO_CHANS;
    mainw->fx3_val=DEFAULT_AUDIO_SAMPS;
    mainw->fx4_val=mainw->endian;
    resaudw=create_resaudw(2,NULL,NULL);
    if (lives_dialog_run(LIVES_DIALOG(resaudw->dialog))!=GTK_RESPONSE_OK) return FALSE;
    if (mainw->error) {
      mainw->error=FALSE;
      return FALSE;
    }
    has_new_audio=TRUE;
  }


  if (menuitem!=NULL) {
    start=calc_time_from_frame (mainw->current_file,cfile->start);
    end=calc_time_from_frame (mainw->current_file,cfile->end+1);

    if (!(prefs->warning_mask&WARN_MASK_LAYOUT_SHIFT_AUDIO)) {
      if ((mainw->xlays=layout_audio_is_affected(mainw->current_file,start))!=NULL) {
	if (!do_warning_dialog(_("\nInsertion will cause audio to shift in some multitrack layouts.\nAre you sure you wish to continue ?\n"))) {
	  g_list_free_strings(mainw->xlays);
	  g_list_free(mainw->xlays);
	  mainw->xlays=NULL;
	  if (has_new_audio) cfile->achans=cfile->arate=cfile->asampsize=cfile->arps=0;
	  return FALSE;
	}
	add_lmap_error(LMAP_ERROR_SHIFT_AUDIO,cfile->name,(gpointer)cfile->layout_map,mainw->current_file,0,start,
		       cfile->stored_layout_audio>start);
	has_lmap_error=TRUE;
	g_list_free_strings(mainw->xlays);
	g_list_free(mainw->xlays);
	mainw->xlays=NULL;
      }
    }
    
    if (!has_lmap_error&&!(prefs->warning_mask&WARN_MASK_LAYOUT_ALTER_AUDIO)
	&&(mainw->xlays=layout_audio_is_affected(mainw->current_file,0.))!=NULL) {
      if (!do_layout_alter_audio_warning()) {
	g_list_free_strings(mainw->xlays);
	g_list_free(mainw->xlays);
	mainw->xlays=NULL;
	if (has_new_audio) cfile->achans=cfile->arate=cfile->asampsize=cfile->arps=0;
	return FALSE;
      }
      add_lmap_error(LMAP_ERROR_ALTER_AUDIO,cfile->name,(gpointer)cfile->layout_map,mainw->current_file,0,start,
		     cfile->stored_layout_audio>0.);
      has_lmap_error=TRUE;
      g_list_free_strings(mainw->xlays);
      g_list_free(mainw->xlays);
      mainw->xlays=NULL;
    }

    msg=g_strdup_printf(_ ("Inserting silence from %.2f to %.2f seconds..."),start,end);
    d_print(""); // force switchtext
    d_print(msg);
    g_free(msg);
  }

  cfile->undo1_dbl=start;
  start*=(double)cfile->arate/(double)cfile->arps;
  cfile->undo2_dbl=end;
  end*=(double)cfile->arate/(double)cfile->arps;

  // with_sound is 2 (audio only), therfore start, end, where, are in seconds. rate is -ve to indicate silence
  com=g_strdup_printf("%s insert \"%s\" \"%s\" %.8f 0. %.8f \"%s\" 2 0 0 0 0 %d %d %d %d %d", 
		      prefs->backend, cfile->handle, 
		      get_image_ext_for_type(cfile->img_type), start, end-start, cfile->handle, -cfile->arps, 
		      cfile->achans, cfile->asampsize, !(cfile->signed_endian&AFORM_UNSIGNED), 
		      !(cfile->signed_endian&AFORM_BIG_ENDIAN));

  unlink (cfile->info_file);
  mainw->com_failed=FALSE;
  lives_system (com,FALSE);
  g_free(com);

  if (mainw->com_failed) {
    d_print_failed();
    if (has_new_audio) cfile->achans=cfile->arate=cfile->asampsize=cfile->arps=0;
    return FALSE;
  }

  do_progress_dialog(TRUE, FALSE, _("Inserting Silence"));

  if (mainw->error) {
    d_print_failed();
    if (has_new_audio) cfile->achans=cfile->arate=cfile->asampsize=cfile->arps=0;
    return FALSE;
  }



  set_undoable (_("Insert Silence"),TRUE);
  cfile->undo_action=UNDO_INSERT_SILENCE;

  reget_afilesize(mainw->current_file);
  get_play_times();
  cfile->changed=TRUE;

  save_clip_values(mainw->current_file);

  if (menuitem!=NULL) {
    sensitize();
    d_print_done();
  }
  if (has_lmap_error) popup_lmap_errors(NULL,NULL);

  if (mainw->sl_undo_mem!=NULL&&cfile->stored_layout_audio!=0.) {
    // need to invalidate undo/redo stack, in case file was used in some layout undo
    stored_event_list_free_undos();
  }

  return TRUE;
}



void on_ins_silence_details_clicked (GtkButton *button, gpointer user_data) {
  int asigned=1,aendian=1;
  boolean bad_header=FALSE;
  
  cfile->arps=cfile->arate=(int)atoi (lives_entry_get_text(LIVES_ENTRY(resaudw->entry_arate)));
  cfile->achans=(int)atoi (lives_entry_get_text(LIVES_ENTRY(resaudw->entry_achans)));
  cfile->asampsize=(int)atoi (lives_entry_get_text(LIVES_ENTRY(resaudw->entry_asamps)));
  if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(resaudw->rb_unsigned))) {
    asigned=0;
  }
  if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(resaudw->rb_bigend))) {
    aendian=0;
  }
  cfile->signed_endian=get_signed_endian(asigned,aendian);
  lives_widget_destroy (resaudw->dialog);
  lives_widget_context_update();
  if (resaudw!=NULL) g_free(resaudw);
  resaudw=NULL;
  if (cfile->arate<=0) {
    do_audrate_error_dialog();
    cfile->achans=cfile->arate=cfile->arps=cfile->asampsize=0;
    save_clip_value(mainw->current_file,CLIP_DETAILS_ARATE,&cfile->arps);
    if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
    save_clip_value(mainw->current_file,CLIP_DETAILS_PB_ARATE,&cfile->arate);
    if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
    save_clip_value(mainw->current_file,CLIP_DETAILS_ACHANS,&cfile->achans);
    if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
    save_clip_value(mainw->current_file,CLIP_DETAILS_ASAMPS,&cfile->asampsize);
    if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;

    if (bad_header) do_header_write_error(mainw->current_file);
    mainw->error=TRUE;
    return;
  }
  mainw->error=FALSE;
}


void on_lerrors_clear_clicked (GtkButton *button, gpointer user_data) {
  boolean close=GPOINTER_TO_INT(user_data);

  if (mainw->multitrack!=NULL) {
    if (mainw->multitrack->idlefunc>0) {
      g_source_remove(mainw->multitrack->idlefunc);
      mainw->multitrack->idlefunc=0;
    }
    mt_desensitise(mainw->multitrack);
  }

  clear_lmap_errors();
  save_layout_map(NULL,NULL,NULL,NULL);
  if (close) lives_general_button_clicked(button,textwindow);
  else {
    lives_widget_queue_draw(lives_widget_get_toplevel(LIVES_WIDGET(button)));
    lives_widget_set_sensitive(textwindow->clear_button,FALSE);
    lives_widget_set_sensitive(textwindow->delete_button,FALSE);

    if (mainw->multitrack!=NULL) {
      mt_sensitise(mainw->multitrack);
      mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
    }
  }
}


void on_lerrors_delete_clicked (GtkButton *button, gpointer user_data) {
  int num_maps=g_list_length(mainw->affected_layouts_map);
  gchar *msg=g_strdup_printf(P_("\nDelete %d layout...are you sure ?\n","\nDelete %d layouts...are you sure ?\n",num_maps),num_maps);

  if (mainw->multitrack!=NULL) {
    if (mainw->multitrack->idlefunc>0) {
      g_source_remove(mainw->multitrack->idlefunc);
      mainw->multitrack->idlefunc=0;
    }
    mt_desensitise(mainw->multitrack);
  }
  
  if (!do_warning_dialog(msg)) {
    g_free(msg);
    if (mainw->multitrack!=NULL) {
      mt_sensitise(mainw->multitrack);
      mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
    }
    return;
  }

  g_free(msg);
  remove_layout_files(mainw->affected_layouts_map);
  on_lerrors_clear_clicked(button,GINT_TO_POINTER(TRUE));
}


