// callbacks.c
// LiVES
// (c) G. Finch 2003 - 2016 <salsaman@gmail.com>
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

#ifdef ENABLE_OSC
#include "osc.h"
#endif

static char file_name[PATH_MAX];

void lives_notify(int msgnumber,const char *msgstring) {
#ifdef IS_LIBLIVES
  binding_cb(msgnumber, msgstring, mainw->id);
#endif
#ifdef ENABLE_OSC
  lives_osc_notify(msgnumber,msgstring);
#endif
}


boolean on_LiVES_delete_event(LiVESWidget *widget, LiVESXEventDelete *event, livespointer user_data) {
  if (!mainw->interactive) return TRUE;
  on_quit_activate(NULL,NULL);
  return TRUE;
}


void lives_exit(int signum) {
  char *cwd,*tmp;

  register int i;

  if (!mainw->only_close) mainw->is_exiting=TRUE;

  if (mainw->is_ready) {
    char *com;

    lives_close_all_file_buffers();

    if (mainw->multitrack!=NULL&&mainw->multitrack->idlefunc>0) {
      //lives_source_remove(mainw->multitrack->idlefunc);
      mainw->multitrack->idlefunc=0;
    }

    threaded_dialog_spin(0.);

    if (mainw->toy_type!=LIVES_TOY_NONE) {
      on_toy_activate(NULL, LIVES_INT_TO_POINTER(LIVES_TOY_NONE));
    }

    if (mainw->stored_event_list!=NULL||mainw->sl_undo_mem!=NULL) {
      stored_event_list_free_all(FALSE);
    }

    if (mainw->multitrack!=NULL&&!mainw->only_close) {
      if (mainw->multitrack->undo_mem!=NULL) lives_free(mainw->multitrack->undo_mem);
      mainw->multitrack->undo_mem=NULL;
    }

    if (mainw->multi_opts.set&&!mainw->only_close&&mainw->multi_opts.aparam_view_list!=NULL) {
      lives_list_free(mainw->multi_opts.aparam_view_list);
    }

    if (mainw->playing_file>-1) {
      mainw->ext_keyboard=FALSE;
      if (mainw->ext_playback) {
        if (mainw->vpp->exit_screen!=NULL)(*mainw->vpp->exit_screen)(mainw->ptr_x,mainw->ptr_y);
        stop_audio_stream();
        mainw->stream_ticks=-1;
      }

      // tell non-realtime audio players (sox or mplayer) to stop
      if (!is_realtime_aplayer(prefs->audio_player)&&mainw->aud_file_to_kill>-1&&mainw->files[mainw->aud_file_to_kill]!=NULL) {
        char *lsname=lives_build_filename(prefs->tmpdir,mainw->files[mainw->aud_file_to_kill]->handle,NULL);
        lives_touch(lsname);
        lives_free(lsname);
        com=lives_strdup_printf("%s stop_audio \"%s\"",prefs->backend,mainw->files[mainw->aud_file_to_kill]->handle);
        lives_system(com,TRUE);
        lives_free(com);
      }
    }

    // stop any background processing for the current clip
    if (mainw->current_file>-1) {
      if (cfile->handle!=NULL&&(cfile->clip_type==CLIP_TYPE_DISK||cfile->clip_type==CLIP_TYPE_FILE)) {
#ifndef IS_MINGW
        com=lives_strdup_printf("%s stopsubsub \"%s\" 2>/dev/null",prefs->backend_sync,cfile->handle);
        lives_system(com,TRUE);
#else
        // get pid from backend
        FILE *rfile;
        ssize_t rlen;
        char val[16];
        int pid;
        com=lives_strdup_printf("%s get_pid_for_handle \"%s\"",prefs->backend_sync,cfile->handle);
        rfile=popen(com,"r");
        rlen=fread(val,1,16,rfile);
        pclose(rfile);
        memset(val+rlen,0,1);
        pid=atoi(val);

        lives_win32_kill_subprocesses(pid,TRUE);
#endif
        lives_free(com);

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
          char *vpp_file=lives_build_filename(capable->home_dir,LIVES_CONFIG_DIR,"vpp_defaults",NULL);
          save_vpp_defaults(mainw->vpp,vpp_file);
        }
      }
      close_vid_playback_plugin(mainw->vpp);
    }

    if (!mainw->leave_recovery) {
      lives_rm(mainw->recovery_file);
      // hide the main window
      threaded_dialog_spin(0.);
      lives_widget_context_update();
      threaded_dialog_spin(0.);
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
        com=lives_strdup_printf("%s weed \"%s\" &",prefs->backend_sync,future_prefs->tmpdir);
        lives_system(com,FALSE);
        lives_free(com);
      }
      lives_snprintf(prefs->tmpdir,PATH_MAX,"%s",future_prefs->tmpdir);
    } else if (!mainw->only_close) lives_snprintf(future_prefs->tmpdir,256,"NULL");

    if (mainw->leave_files&&!mainw->fatal) {
      d_print(_("Saving as set %s..."),mainw->set_name);
    }

    cwd=lives_get_current_dir();

    for (i=1; i<=MAX_FILES; i++) {
      if (mainw->files[i]!=NULL) {
        mainw->current_file=i;
        threaded_dialog_spin(0.);
        if (cfile->event_list_back!=NULL) event_list_free(cfile->event_list_back);
        if (cfile->event_list!=NULL) event_list_free(cfile->event_list);

        cfile->event_list=cfile->event_list_back=NULL;

        if (cfile->layout_map!=NULL) {
          lives_list_free_strings(cfile->layout_map);
          lives_list_free(cfile->layout_map);
        }

        if (cfile->laudio_drawable!=NULL) {
          lives_painter_surface_destroy(cfile->laudio_drawable);
        }

        if (cfile->raudio_drawable!=NULL) {
          lives_painter_surface_destroy(cfile->raudio_drawable);
        }


        if ((mainw->files[i]->clip_type==CLIP_TYPE_FILE||mainw->files[i]->clip_type==CLIP_TYPE_DISK)&&mainw->files[i]->ext_src!=NULL) {
          // must do this before we move it
          char *ppath=lives_build_filename(prefs->tmpdir,cfile->handle,NULL);
          lives_chdir(ppath,FALSE);
          lives_free(ppath);
          threaded_dialog_spin(0.);
          close_decoder_plugin((lives_decoder_t *)mainw->files[i]->ext_src);
          mainw->files[i]->ext_src=NULL;
          threaded_dialog_spin(0.);
        }

        if (mainw->files[i]->frame_index!=NULL) {
          lives_free(mainw->files[i]->frame_index);
          mainw->files[i]->frame_index=NULL;
        }

        cfile->layout_map=NULL;

      }
    }

    lives_chdir(cwd,FALSE);
    lives_free(cwd);

    for (i=0; i<=MAX_FILES; i++) {
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
            lives_free(mainw->files[i]->ext_src);
          }
#endif
#ifdef HAVE_UNICAP
          if (mainw->files[i]->clip_type==CLIP_TYPE_VIDEODEV) {
            lives_vdev_free((lives_vdev_t *)mainw->files[i]->ext_src);
            lives_free(mainw->files[i]->ext_src);
          }
#endif
          threaded_dialog_spin(0.);
#ifdef IS_MINGW
          // kill any active processes: for other OSes the backend does this
          // get pid from backend
          com=lives_strdup_printf("%s get_pid_for_handle \"%s\"",prefs->backend_sync,cfile->handle);
          rfile=popen(com,"r");
          rlen=fread(val,1,16,rfile);
          pclose(rfile);
          memset(val+rlen,0,1);
          pid=atoi(val);

          lives_win32_kill_subprocesses(pid,TRUE);
#endif
          com=lives_strdup_printf("%s close \"%s\"",prefs->backend,mainw->files[i]->handle);
          lives_system(com,FALSE);
          lives_free(com);
          threaded_dialog_spin(0.);
        } else {
          threaded_dialog_spin(0.);
          // or just clean them up
          com=lives_strdup_printf("%s clear_tmp_files \"%s\"",prefs->backend_sync,mainw->files[i]->handle);
          lives_system(com,FALSE);
          threaded_dialog_spin(0.);
          lives_free(com);
          if (mainw->files[i]->frame_index!=NULL) {
            save_frame_index(i);
          }
          lives_freep((void **)&mainw->files[i]->op_dir);
          if (!mainw->only_close) {
            if ((mainw->files[i]->clip_type==CLIP_TYPE_FILE||mainw->files[i]->clip_type==CLIP_TYPE_DISK)&&mainw->files[i]->ext_src!=NULL) {
              char *ppath=lives_build_filename(prefs->tmpdir,mainw->files[i]->handle,NULL);
              cwd=lives_get_current_dir();
              lives_chdir(ppath,FALSE);
              lives_free(ppath);
              close_decoder_plugin((lives_decoder_t *)mainw->files[i]->ext_src);
              mainw->files[i]->ext_src=NULL;

              lives_chdir(cwd,FALSE);
              lives_free(cwd);
            }
          }
        }
      }
    }

    if (!mainw->leave_files&&strlen(mainw->set_name)&&!mainw->leave_recovery) {
      char *set_layout_dir=lives_build_filename(prefs->tmpdir,mainw->set_name,"layouts",NULL);
      if (!lives_file_test(set_layout_dir,LIVES_FILE_TEST_IS_DIR)) {
        char *sdname=lives_build_filename(prefs->tmpdir,mainw->set_name,NULL);

        // note, FORCE is FALSE
        lives_rmdir(sdname,FALSE);
        lives_free(sdname);
        threaded_dialog_spin(0.);
      } else {
        char *dname=lives_build_filename(prefs->tmpdir,mainw->set_name,"clips",NULL);

        // note, FORCE is FALSE
        lives_rmdir(dname,FALSE);
        lives_free(dname);
        threaded_dialog_spin(0.);

        dname=lives_build_filename(prefs->tmpdir,mainw->set_name,"order",NULL);
        lives_rm(dname);
        lives_free(dname);
        threaded_dialog_spin(0.);
      }
      lives_free(set_layout_dir);
    }

    if (strlen(mainw->set_name)) {
      char *set_lock_file=lives_strdup_printf("%s/%s/lock.%d",prefs->tmpdir,mainw->set_name,capable->mainpid);
      lives_rm(set_lock_file);
      lives_free(set_lock_file);
      threaded_dialog_spin(0.);
    }

    if (mainw->only_close) {
      mainw->suppress_dprint=TRUE;
      for (i=1; i<=MAX_FILES; i++) {
        if (mainw->files[i]!=NULL&&(mainw->files[i]->clip_type==CLIP_TYPE_DISK||
                                    mainw->files[i]->clip_type==CLIP_TYPE_FILE)&&(mainw->multitrack==NULL||
                                        i!=mainw->multitrack->render_file)) {
          mainw->current_file=i;
          close_current_file(0);
          threaded_dialog_spin(0.);
        }
      }
      mainw->suppress_dprint=FALSE;
      if (mainw->multitrack==NULL) resize(1);
      mainw->was_set=FALSE;
      mainw->leave_files=FALSE;
      memset(mainw->set_name,0,1);
      mainw->only_close=FALSE;
      prefs->crash_recovery=TRUE;

      threaded_dialog_spin(0.);
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
      threaded_dialog_spin(0.);
      kill_play_window();
      threaded_dialog_spin(0.);
    }
  }

  if (mainw->current_layouts_map!=NULL) {
    lives_list_free_strings(mainw->current_layouts_map);
    lives_list_free(mainw->current_layouts_map);
    mainw->current_layouts_map=NULL;
  }

  if (capable->smog_version_correct&&!mainw->startup_error) {
    if (capable->has_encoder_plugins) {
      LiVESList *dummy_list=plugin_request("encoders",prefs->encoder.name,"finalise");
      if (dummy_list!=NULL) {
        lives_list_free_strings(dummy_list);
        lives_list_free(dummy_list);
      }
    }

    weed_unload_all();

    threaded_dialog_spin(0.);

    rfx_free_all();
    threaded_dialog_spin(0.);

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

  if (prefs->fxdefsfile!=NULL) lives_free(prefs->fxdefsfile);
  if (prefs->fxsizesfile!=NULL) lives_free(prefs->fxsizesfile);

  if (prefs->wm!=NULL) lives_free(prefs->wm);

  if (mainw->recovery_file!=NULL) lives_free(mainw->recovery_file);

  for (i=0; i<NUM_LIVES_STRING_CONSTANTS; i++) if (mainw->string_constants[i]!=NULL) lives_free(mainw->string_constants[i]);

  for (i=0; i<mainw->n_screen_areas; i++) lives_free(mainw->screen_areas[i].name);

  if (mainw->video_drawable!=NULL) {
    lives_painter_surface_destroy(mainw->video_drawable);
  }

  if (mainw->laudio_drawable!=NULL) {
    lives_painter_surface_destroy(mainw->laudio_drawable);
  }

  if (mainw->raudio_drawable!=NULL) {
    lives_painter_surface_destroy(mainw->raudio_drawable);
  }

  if (mainw->foreign_visual!=NULL) lives_free(mainw->foreign_visual);
  if (mainw->read_failed_file!=NULL) lives_free(mainw->read_failed_file);
  if (mainw->write_failed_file!=NULL) lives_free(mainw->write_failed_file);
  if (mainw->bad_aud_file!=NULL) lives_free(mainw->bad_aud_file);

  unload_decoder_plugins();

  if (mainw->multitrack!=NULL) lives_free(mainw->multitrack);
  mainw->multitrack=NULL;
  mainw->is_ready=FALSE;

  if (mainw->mgeom!=NULL) lives_free(mainw->mgeom);

  if (prefs->disabled_decoders!=NULL) {
    lives_list_free_strings(prefs->disabled_decoders);
    lives_list_free(prefs->disabled_decoders);
  }

  if (mainw->fonts_array!=NULL) lives_strfreev(mainw->fonts_array);
#ifdef USE_SWSCALE
  sws_free_context();
#endif

#ifdef ENABLE_NLS
  if (trString!=NULL) lives_free(trString);
#endif

  tmp=lives_strdup_printf("%d",signum);
  lives_notify(LIVES_OSC_NOTIFY_QUIT,tmp);
  lives_free(tmp);

  exit(0);
}




void on_filesel_button_clicked(LiVESButton *button, livespointer user_data) {
  LiVESWidget *tentry=LIVES_WIDGET(user_data);

  char **filt=NULL;

  char *dirname=NULL;
  char *fname;
  char *tmp;

  char *def_dir=NULL;

  boolean is_dir=TRUE;

  int filesel_type=LIVES_FILE_SELECTION_UNDEFINED;

  if (button!=NULL) {
    def_dir=(char *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(button),"def_dir");
    is_dir=LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(button),"is_dir"));
    filt=(char **)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(button), "filter");
    if (lives_widget_object_get_data(LIVES_WIDGET_OBJECT(button), "filesel_type")!=NULL) {
      filesel_type=LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(button),"filesel_type"));
    }
  }

  if (LIVES_IS_TEXT_VIEW(tentry)) fname=lives_text_view_get_text(LIVES_TEXT_VIEW(tentry));
  else fname=lives_strdup(lives_entry_get_text(LIVES_ENTRY(tentry)));

  if (!strlen(fname)) {
    lives_free(fname);
    fname=def_dir;
  }

  lives_widget_context_update();

  if (filesel_type==LIVES_FILE_SELECTION_UNDEFINED) {
    dirname=choose_file(is_dir?fname:def_dir,is_dir?NULL:fname,filt,
                        is_dir?LIVES_FILE_CHOOSER_ACTION_SELECT_FOLDER:
                        (fname==def_dir&&def_dir!=NULL&&!strcmp(def_dir,LIVES_DEVICE_DIR))?LIVES_FILE_CHOOSER_ACTION_SELECT_DEVICE:
                        LIVES_FILE_CHOOSER_ACTION_OPEN,
                        NULL,NULL);
  } else {
    LiVESWidget *chooser=choose_file_with_preview(def_dir,fname,filt,filesel_type);
    int resp=lives_dialog_run(LIVES_DIALOG(chooser));

    end_fs_preview();

    if (resp==LIVES_RESPONSE_ACCEPT) {
      dirname=lives_file_chooser_get_filename(LIVES_FILE_CHOOSER(chooser));
    }

    lives_widget_destroy(LIVES_WIDGET(chooser));
  }

  if (fname!=NULL&&fname!=def_dir) lives_free(fname);

  if (dirname==NULL) return;

  lives_snprintf(file_name,PATH_MAX,"%s",dirname);
  lives_free(dirname);

  if (button!=NULL) {
    if (LIVES_IS_ENTRY(tentry)) lives_entry_set_text(LIVES_ENTRY(tentry),(tmp=lives_filename_to_utf8(file_name,-1,NULL,NULL,NULL)));
    else lives_text_view_set_text(LIVES_TEXT_VIEW(tentry), (tmp=lives_filename_to_utf8(file_name,-1,NULL,NULL,NULL)), -1);
    lives_free(tmp);
  }

  // force update to be recognized
  if (lives_widget_object_get_data(LIVES_WIDGET_OBJECT(tentry),"rfx")!=NULL)
    after_param_text_changed(tentry,(lives_rfx_t *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(tentry),"rfx"));
}



void on_filesel_complex_clicked(LiVESButton *button, LiVESEntry *entry) {
  // append /livestmp
  size_t chklen=strlen(LIVES_TMP_NAME);

  on_filesel_button_clicked(NULL,entry);

  // TODO - dirsep
#ifndef IS_MINGW
  if (strcmp(file_name+strlen(file_name)-1,"/")) {
    lives_strappend(file_name,PATH_MAX,"/");
  }
  if (strlen(file_name)<chklen+2||strncmp(file_name+strlen(file_name)-chklen-2,"/"LIVES_TMP_NAME"/",chklen-2))
    lives_strappend(file_name,PATH_MAX,LIVES_TMP_NAME"/");
#else
  if (strcmp(file_name+strlen(file_name)-1,"\\")) {
    lives_strappend(file_name,PATH_MAX,"\\");
  }
  if (strlen(file_name)<chklen-2||strncmp(file_name+strlen(file_name)-chklen-2,"\\"LIVES_TMP_NAME"\\",chklen-2))
    lives_strappend(file_name,PATH_MAX,LIVES_TMP_NAME"\\");
#endif

  lives_entry_set_text(entry,file_name);

}



void on_open_sel_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  // OPEN A FILE
  LiVESWidget *chooser;
  char *fname, *tmp;
  int resp;

  if (mainw->multitrack!=NULL) {
    if (mainw->multitrack->idlefunc>0) {
      lives_source_remove(mainw->multitrack->idlefunc);
      mainw->multitrack->idlefunc=0;
    }
    mt_desensitise(mainw->multitrack);
    lives_widget_set_sensitive(mainw->multitrack->playall,TRUE);
    lives_widget_set_sensitive(mainw->m_playbutton, TRUE);
  }

  chooser=choose_file_with_preview(strlen(mainw->vid_load_dir)?mainw->vid_load_dir:NULL,NULL,NULL,LIVES_FILE_SELECTION_VIDEO_AUDIO);
  resp=lives_dialog_run(LIVES_DIALOG(chooser));

  end_fs_preview();
  mainw->fs_playarea=NULL;

  if (resp!=LIVES_RESPONSE_ACCEPT) {
    on_filechooser_cancel_clicked(chooser);
    return;
  }

  fname=lives_file_chooser_get_filename(LIVES_FILE_CHOOSER(chooser));

  if (fname==NULL) {
    if (mainw->multitrack!=NULL) {
      mt_sensitise(mainw->multitrack);
      mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
    }
    return;
  }

  lives_snprintf(file_name,PATH_MAX,"%s",(tmp=lives_filename_to_utf8(fname,-1,NULL,NULL,NULL)));
  lives_free(tmp);
  lives_free(fname);

  lives_widget_destroy(LIVES_WIDGET(chooser));

  lives_snprintf(mainw->vid_load_dir,PATH_MAX,"%s",file_name);
  get_dirname(mainw->vid_load_dir);

  if (mainw->multitrack==NULL) lives_widget_queue_draw(mainw->LiVES);
  else lives_widget_queue_draw(mainw->multitrack->window);
  lives_widget_context_update();

  if (prefs->save_directories) {
    set_pref("vid_load_dir",(tmp=lives_filename_from_utf8(mainw->vid_load_dir,-1,NULL,NULL,NULL)));
    lives_free(tmp);
  }

  mainw->cancelled=CANCEL_NONE;

  open_sel_range_activate();
}



void on_open_vcd_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  LiVESWidget *vcdtrack_dialog;

  if (mainw->multitrack!=NULL) {
    if (mainw->multitrack->idlefunc>0) {
      lives_source_remove(mainw->multitrack->idlefunc);
      mainw->multitrack->idlefunc=0;
    }
    mt_desensitise(mainw->multitrack);
    lives_widget_set_sensitive(mainw->multitrack->playall,TRUE);
    lives_widget_set_sensitive(mainw->m_playbutton, TRUE);
  }

  mainw->fx1_val=1;
  mainw->fx2_val=1;
  mainw->fx3_val=128;
  vcdtrack_dialog = create_cdtrack_dialog(LIVES_POINTER_TO_INT(user_data),NULL);
  lives_widget_show_all(vcdtrack_dialog);
}


void on_open_loc_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  if (mainw->multitrack!=NULL) {
    if (mainw->multitrack->idlefunc>0) {
      lives_source_remove(mainw->multitrack->idlefunc);
      mainw->multitrack->idlefunc=0;
    }
    mt_desensitise(mainw->multitrack);
    lives_widget_set_sensitive(mainw->multitrack->playall,TRUE);
    lives_widget_set_sensitive(mainw->m_playbutton, TRUE);
  }

  locw=create_location_dialog(1);
  lives_widget_show_all(locw->dialog);

}


void on_open_utube_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  if (mainw->multitrack!=NULL) {
    if (mainw->multitrack->idlefunc>0) {
      lives_source_remove(mainw->multitrack->idlefunc);
      mainw->multitrack->idlefunc=0;
    }
    mt_desensitise(mainw->multitrack);
    lives_widget_set_sensitive(mainw->multitrack->playall,TRUE);
    lives_widget_set_sensitive(mainw->m_playbutton, TRUE);
  }

  locw=create_location_dialog(2);
  lives_widget_show_all(locw->dialog);
}




void on_autoreload_toggled(LiVESToggleButton *togglebutton, livespointer user_data) {
  // type==0, autoreload layout
  // type==1, autoreload clipset
  // type==2, autoreload layout (from choose layout name and ...)

  int type=LIVES_POINTER_TO_INT(user_data);
  if (type==0) {
    _entryw *cdsw=(_entryw *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(togglebutton),"cdsw");
    prefs->ar_layout=!prefs->ar_layout;
    if (cdsw->warn_checkbutton!=NULL) {
      if (prefs->ar_layout) {
        lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(cdsw->warn_checkbutton),FALSE);
        lives_widget_set_sensitive(cdsw->warn_checkbutton,FALSE);
      } else {
        lives_widget_set_sensitive(cdsw->warn_checkbutton,TRUE);
      }
    }
  }
  if (type==1) prefs->ar_clipset=!prefs->ar_clipset;
  if (type==2) prefs->ar_layout=!prefs->ar_layout;
}


void on_recent_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  char file[PATH_MAX];
  double start=0.;
  int end=0,pno;
  char *pref;

  pno=LIVES_POINTER_TO_INT(user_data);

  if (mainw->multitrack!=NULL) {
    if (mainw->multitrack->idlefunc>0) {
      lives_source_remove(mainw->multitrack->idlefunc);
      mainw->multitrack->idlefunc=0;
    }
    mt_desensitise(mainw->multitrack);
  }


  lives_widget_context_update(); // hide menu popdown

  pref=lives_strdup_printf("recent%d",pno);

  get_pref(pref,file,PATH_MAX);

  lives_free(pref);

  if (get_token_count(file,'\n')>1) {
    char **array=lives_strsplit(file,"\n",2);
    lives_snprintf(file,PATH_MAX,"%s",array[0]);
    if (mainw->file_open_params!=NULL) lives_free(mainw->file_open_params);
    mainw->file_open_params=lives_strdup(array[1]);
    lives_strfreev(array);
  }

  if (get_token_count(file,'|')>2) {
    char **array=lives_strsplit(file,"|",3);
    lives_snprintf(file,PATH_MAX,"%s",array[0]);
    start=lives_strtod(array[1],NULL);
    end=atoi(array[2]);
    lives_strfreev(array);
  }
  deduce_file(file,start,end);

  if (mainw->multitrack!=NULL) {
    polymorph(mainw->multitrack,POLY_NONE);
    polymorph(mainw->multitrack,POLY_CLIPS);
    mt_sensitise(mainw->multitrack);
    mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
  }

}



void on_location_select(LiVESButton *button, livespointer user_data) {
  lives_snprintf(file_name,PATH_MAX,"%s",lives_entry_get_text(LIVES_ENTRY(locw->entry)));
  lives_widget_destroy(locw->dialog);
  lives_widget_context_update();
  lives_free(locw);

  mainw->opening_loc=TRUE;
  if (mainw->file_open_params!=NULL) lives_free(mainw->file_open_params);
  if (prefs->no_bandwidth) {
    mainw->file_open_params=lives_strdup("nobandwidth");
  } else mainw->file_open_params=lives_strdup("sendbandwidth");
  mainw->img_concat_clip=-1;
  open_file(file_name);

  if (mainw->multitrack!=NULL) {
    polymorph(mainw->multitrack,POLY_NONE);
    polymorph(mainw->multitrack,POLY_CLIPS);
    mt_sensitise(mainw->multitrack);
    mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
  }

}




void on_utube_select(LiVESButton *button, livespointer user_data) {
  char *fname=ensure_extension(lives_entry_get_text(LIVES_ENTRY(locw->name_entry)),".webm");
  char *url;
  char *dirname;
  char *dfile;
  char *com;
  int current_file=mainw->current_file;

  if (!strlen(fname)) {
    do_blocking_error_dialog(_("Please enter the name of the file to save the clip as.\n"));
    lives_free(fname);
    return;
  }

  url=lives_strdup(lives_entry_get_text(LIVES_ENTRY(locw->entry)));

  if (!strlen(url)) {
    do_blocking_error_dialog(_("Please enter a valid URL for the download.\n"));
    lives_free(fname);
    lives_free(url);
    return;
  }

  dirname=lives_strdup(lives_entry_get_text(LIVES_ENTRY(locw->dir_entry)));
  ensure_isdir(dirname);
  lives_snprintf(mainw->vid_dl_dir,PATH_MAX,"%s",dirname);

  lives_widget_destroy(locw->dialog);
  lives_widget_context_update();
  lives_free(locw);

  dfile=lives_build_filename(dirname,fname,NULL);

  if (!check_file(dfile,TRUE)) {
    lives_free(dirname);
    lives_free(fname);
    lives_free(url);
    lives_free(dfile);
    return;
  }

  mainw->error=FALSE;

  d_print(_("Downloading %s to %s..."),url,dfile);

  if (current_file==-1) {
    if (!get_temp_handle(mainw->first_free_file,TRUE)) {
      d_print_failed();
      return;
    }
  }

  mainw->no_switch_dprint=TRUE;

  lives_rm(cfile->info_file);

  com=lives_strdup_printf("%s download_clip \"%s\" \"%s\" \"%s\"",prefs->backend,cfile->handle,url,dfile);
  mainw->com_failed=FALSE;
  lives_system(com,FALSE);
  lives_free(com);

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
    } else {
      if (current_file==-1) {
#ifdef IS_MINGW
        // kill any active processes: for other OSes the backend does this
        // get pid from backend
        FILE *rfile;
        ssize_t rlen;
        char val[16];
        int pid;
        com=lives_strdup_printf("%s get_pid_for_handle \"%s\"",prefs->backend_sync,cfile->handle);
        rfile=popen(com,"r");
        rlen=fread(val,1,16,rfile);
        pclose(rfile);
        memset(val+rlen,0,1);
        pid=atoi(val);

        lives_win32_kill_subprocesses(pid,TRUE);
#endif
        // we made a temp file so close it
        com=lives_strdup_printf("%s close \"%s\"",prefs->backend,cfile->handle);
        lives_system(com,TRUE);
        lives_free(com);
        lives_free(cfile);
        cfile=NULL;
        mainw->current_file=-1;
      }

      if (mainw->error) {
        d_print_failed();
        do_blocking_error_dialog(
          _("\nLiVES was unable to download the clip.\nPlease check the clip URL and make sure you have \nthe latest youtube-dl installed.\n"));
        mainw->error=FALSE;
      }

      lives_rm(dfile);

      lives_free(dirname);
      lives_free(fname);
      lives_free(url);
      lives_free(dfile);

      sensitize();
      mainw->no_switch_dprint=FALSE;
      return;
    }
  }

  cfile->nopreview=FALSE;
  cfile->no_proc_sys_errors=FALSE;
  cfile->keep_without_preview=FALSE;

  if (current_file==-1) {
    com=lives_strdup_printf("%s close \"%s\"",prefs->backend,cfile->handle);
    lives_system(com,TRUE);
    lives_free(com);
    lives_free(cfile);
    cfile=NULL;
    mainw->current_file=-1;
  }

  lives_free(dirname);
  lives_free(fname);
  lives_free(url);

  mainw->no_switch_dprint=FALSE;
  open_file(dfile);
  lives_free(dfile);

  if (mainw->multitrack!=NULL) {
    polymorph(mainw->multitrack,POLY_NONE);
    polymorph(mainw->multitrack,POLY_CLIPS);
    mt_sensitise(mainw->multitrack);
    mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
  }

}




void on_stop_clicked(LiVESMenuItem *menuitem, livespointer user_data) {
  // 'enough' button for open, open location, and record audio
  char *com;

#ifdef IS_MINGW
  FILE *rfile;
  ssize_t rlen;
  char val[16];
  int pid;
#endif

#ifdef ENABLE_JACK
  if (mainw->jackd!=NULL&&mainw->jackd_read!=NULL&&mainw->jackd_read->in_use) {
    mainw->cancelled=CANCEL_KEEP;
    return;
  }
#endif
#ifdef HAVE_PULSE_AUDIO
  if (mainw->pulsed!=NULL&&mainw->pulsed_read!=NULL&&mainw->pulsed->in_use) {
    mainw->cancelled=CANCEL_KEEP;
    return;
  }
#endif

#ifndef IS_MINGW
  com=lives_strdup_printf("%s stopsubsubs \"%s\" 2>/dev/null",prefs->backend_sync,cfile->handle);
  lives_system(com,TRUE);
#else
  // get pid from backend
  com=lives_strdup_printf("%s get_pid_for_handle \"%s\"",prefs->backend_sync,cfile->handle);
  rfile=popen(com,"r");
  rlen=fread(val,1,16,rfile);
  pclose(rfile);
  memset(val+rlen,0,1);
  pid=atoi(val);

  lives_win32_kill_subprocesses(pid,FALSE);
#endif
  lives_free(com);

  if (mainw->current_file>-1&&cfile!=NULL&&cfile->proc_ptr!=NULL) {
    lives_widget_set_sensitive(cfile->proc_ptr->stop_button, FALSE);
    lives_widget_set_sensitive(cfile->proc_ptr->pause_button,FALSE);
    lives_widget_set_sensitive(cfile->proc_ptr->preview_button, FALSE);
    lives_widget_set_sensitive(cfile->proc_ptr->cancel_button, FALSE);
  }


  // resume to allow return
  if (mainw->effects_paused) {
#ifndef IS_MINGW
    com=lives_strdup_printf("%s stopsubsub \"%s\" SIGCONT 2>/dev/null",prefs->backend_sync,cfile->handle);
    lives_system(com,TRUE);
#else
    FILE *rfile;
    ssize_t rlen;
    char val[16];

    // get pid from backend
    com=lives_strdup_printf("%s get_pid_for_handle \"%s\"",prefs->backend_sync,cfile->handle);
    rfile=popen(com,"r");
    rlen=fread(val,1,16,rfile);
    pclose(rfile);
    memset(val+rlen,0,1);
    pid=atoi(val);

    lives_win32_suspend_resume_process(pid,FALSE);
#endif
    lives_free(com);

    com=lives_strdup_printf("%s resume \"%s\"",prefs->backend_sync,cfile->handle);
    lives_system(com,FALSE);
    lives_free(com);
  }


}




void on_save_as_activate(LiVESMenuItem *menuitem, livespointer user_data) {

  if (cfile->frames==0) {
    on_export_audio_activate(NULL,NULL);
    return;
  }

  save_file(mainw->current_file,1,cfile->frames,NULL);
}


void on_save_selection_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  save_file(mainw->current_file,cfile->start,cfile->end,NULL);
}


void on_close_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  char *warn,*extra;
  char title[256];
  boolean lmap_errors=FALSE,acurrent=FALSE,only_current=FALSE;

  if (mainw->multitrack!=NULL) {
    if (mainw->multitrack->idlefunc>0) {
      lives_source_remove(mainw->multitrack->idlefunc);
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
      mainw->xlays=lives_list_append_unique(mainw->xlays,mainw->string_constants[LIVES_STRING_CONSTANT_CL]);
    }

    if (mainw->xlays!=NULL) {
      get_menu_text(cfile->menuentry,title);
      if (strlen(title)>128) lives_snprintf(title,32,"%s",(_("This file")));
      if (acurrent) extra=lives_strdup(_(",\n - including the current layout - "));
      else extra=lives_strdup("");
      if (!only_current) warn=lives_strdup_printf(_("\n%s\nis used in some multitrack layouts%s.\n\nReally close it ?"),
                                title,extra);
      else warn=lives_strdup_printf(_("\n%s\nis used in the current layout.\n\nReally close it ?"),title);
      lives_free(extra);
      if (!do_warning_dialog(warn)) {
        lives_free(warn);
        if (mainw->xlays!=NULL) {
          lives_list_free_strings(mainw->xlays);
          lives_list_free(mainw->xlays);
          mainw->xlays=NULL;
        }

        if (mainw->multitrack!=NULL) {
          mainw->current_file=mainw->multitrack->render_file;
          mt_sensitise(mainw->multitrack);
          mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
        }
        return;
      }
      lives_free(warn);
      add_lmap_error(LMAP_ERROR_CLOSE_FILE,cfile->name,cfile->layout_map,0,1,0.,acurrent);
      lmap_errors=TRUE;
      if (mainw->xlays!=NULL) {
        lives_list_free_strings(mainw->xlays);
        lives_list_free(mainw->xlays);
        mainw->xlays=NULL;
      }
    }
  }
  if (!lmap_errors) {
    if (cfile->changed) {
      get_menu_text(cfile->menuentry,title);
      if (strlen(title)>128) lives_snprintf(title,32,"%s",(_("This file")));
      warn=lives_strdup(_("Changes made to this clip have not been saved or backed up.\n\nReally close it ?"));
      if (!do_warning_dialog(warn)) {
        lives_free(warn);

        if (mainw->multitrack!=NULL) {
          mainw->current_file=mainw->multitrack->render_file;
          mt_sensitise(mainw->multitrack);
          mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
        }

        return;
      }
      lives_free(warn);
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
    char *lfiles;
    char *ofile;
    char *sdir;
    char *cdir;
    char *laydir;

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
        for (i=1; i<MAX_FILES; i++) {
          if (!(mainw->files[i]==NULL)) {
            if (mainw->was_set&&mainw->files[i]->layout_map!=NULL) {
              remove_layout_files(mainw->files[i]->layout_map);
            }
          }
        }

        // delete layout directory
        laydir=lives_build_filename(prefs->tmpdir,mainw->set_name,"layouts",NULL);
        lives_rmdir(laydir,FALSE);
        lives_free(laydir);
      }
      recover_layout_cancelled(FALSE);
    }

    cdir=lives_build_filename(prefs->tmpdir,mainw->set_name,"clips",NULL);

    do {
      // keep trying until backend has deleted the clip
      mainw->com_failed=FALSE;
      lives_rmdir(cdir,FALSE);
      if (lives_file_test(cdir,LIVES_FILE_TEST_IS_DIR)) {
        lives_widget_context_update();
        lives_usleep(prefs->sleep_time);
      }
    } while (lives_file_test(cdir,LIVES_FILE_TEST_IS_DIR));

    mainw->com_failed=FALSE;

    lives_free(cdir);

    lfiles=lives_build_filename(prefs->tmpdir,mainw->set_name,"lock",NULL);

    lives_rmglob(lfiles);
    lives_free(lfiles);

    ofile=lives_build_filename(prefs->tmpdir,mainw->set_name,"order",NULL);
    lives_rm(ofile);
    lives_free(ofile);


    lives_sync();


    sdir=lives_build_filename(prefs->tmpdir,mainw->set_name,NULL);
    lives_rmdir(sdir,FALSE);
    lives_free(sdir);

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


void on_import_proj_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  char *com;
  char *filt[]= {"*.lv2",NULL};
  char *proj_file=choose_file(NULL,NULL,filt,LIVES_FILE_CHOOSER_ACTION_OPEN,NULL,NULL);
  char *info_file;
  char *new_set;
  int info_fd;
  ssize_t bytes;
  char *set_dir;
  char *msg;

  if (proj_file==NULL) return;
  info_file=lives_strdup_printf("%s/.impname.%d",prefs->tmpdir,capable->mainpid);
  lives_rm(info_file);
  mainw->com_failed=FALSE;
  com=lives_strdup_printf("%s get_proj_set \"%s\">\"%s\"",prefs->backend_sync,proj_file,info_file);
  lives_system(com,FALSE);
  lives_free(com);

  if (mainw->com_failed) {
    lives_free(info_file);
    lives_free(proj_file);
    return;
  }

  info_fd=open(info_file,O_RDONLY);

  if (info_fd>=0&&((bytes=read(info_fd,mainw->msg,256))>0)) {
    close(info_fd);
    memset(mainw->msg+bytes,0,1);
  } else {
    if (info_fd>=0) close(info_fd);
    lives_rm(info_file);
    lives_free(info_file);
    lives_free(proj_file);
    do_error_dialog(_("\nInvalid project file.\n"));
    return;
  }

  lives_rm(info_file);
  lives_free(info_file);

  if (!is_legal_set_name(mainw->msg,TRUE)) return;

  new_set=lives_strdup(mainw->msg);
  set_dir=lives_build_filename(prefs->tmpdir,new_set,NULL);

  if (lives_file_test(set_dir,LIVES_FILE_TEST_IS_DIR)) {
    msg=lives_strdup_printf(
          _("\nA set called %s already exists.\nIn order to import this project, you must rename or delete the existing set.\nYou can do this by File|Reload Set, and giving the set name\n%s\nthen File|Close/Save all Clips and provide a new set name or discard it.\nOnce you have done this, you will be able to import the new project.\n"),
          new_set,new_set);
    do_blocking_error_dialog(msg);
    lives_free(msg);
    lives_free(proj_file);
    lives_free(set_dir);
    return;
  }

  lives_free(set_dir);

  d_print(_("Importing the project %s as set %s..."),proj_file,new_set);

  if (!get_temp_handle(mainw->first_free_file,TRUE)) {
    d_print_failed();
    return;
  }

  com=lives_strdup_printf("%s import_project \"%s\" \"%s\"",prefs->backend,cfile->handle,proj_file);
  mainw->com_failed=FALSE;
  lives_system(com,FALSE);
  lives_free(com);
  lives_free(proj_file);

  if (mainw->com_failed) {
    d_print_failed();
    return;
  }

  do_progress_dialog(TRUE,FALSE,_("Importing project"));

  com=lives_strdup_printf("%s close \"%s\"",prefs->backend,cfile->handle);
  lives_system(com,TRUE);
  lives_free(com);
  lives_free(cfile);
  cfile=NULL;
  if (mainw->first_free_file==-1||mainw->first_free_file>mainw->current_file) mainw->first_free_file=mainw->current_file;
  mainw->current_file=-1;
  sensitize();

  if (mainw->error) {
    lives_free(new_set);
    d_print_failed();
    return;
  }

  d_print_done();

  reload_set(new_set);
  lives_free(new_set);
}



void on_export_proj_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  char *filt[]= {"*.lv2",NULL};
  char *def_file;
  char *proj_file;
  char *com,*tmp;

  if (strlen(mainw->set_name)==0) {
    int response;
    char new_set_name[128];
    do {
      // prompt for a set name, advise user to save set
      renamew=create_rename_dialog(5);
      lives_widget_show_all(renamew->dialog);
      response=lives_dialog_run(LIVES_DIALOG(renamew->dialog));
      if (response==LIVES_RESPONSE_CANCEL) {
        lives_widget_destroy(renamew->dialog);
        lives_free(renamew);
        mainw->cancelled=CANCEL_USER;
        return;
      }
      lives_snprintf(new_set_name,128,"%s",(tmp=U82F(lives_entry_get_text(LIVES_ENTRY(renamew->entry)))));
      lives_widget_destroy(renamew->dialog);
      lives_free(renamew);
      renamew=NULL;
      lives_free(tmp);
      lives_widget_context_update();
    } while (!is_legal_set_name(new_set_name,FALSE));
    lives_snprintf(mainw->set_name,128,"%s",new_set_name);
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

  def_file=lives_strdup_printf("%s.lv2",mainw->set_name);
  proj_file=choose_file(NULL,def_file,filt,LIVES_FILE_CHOOSER_ACTION_SAVE,NULL,NULL);
  lives_free(def_file);

  if (proj_file==NULL) return;

  lives_rm((tmp=lives_filename_from_utf8(proj_file,-1,NULL,NULL,NULL)));
  lives_free(tmp);

  if (!check_file(proj_file,FALSE)) {
    lives_free(proj_file);
    return;
  }

  d_print(_("Exporting project %s..."),proj_file);

  com=lives_strdup_printf("%s export_project \"%s\" \"%s\" \"%s\"",prefs->backend,cfile->handle,mainw->set_name,proj_file);
  mainw->com_failed=FALSE;
  lives_system(com,FALSE);
  lives_free(com);

  if (mainw->com_failed) {
    lives_free(proj_file);
    d_print_failed();
    return;
  }

  cfile->op_dir=lives_filename_from_utf8((tmp=get_dir(proj_file)),-1,NULL,NULL,NULL);
  lives_free(tmp);

  do_progress_dialog(TRUE,FALSE,_("Exporting project"));

  if (mainw->error) d_print_failed();
  else d_print_done();

  lives_free(proj_file);
}



void on_export_theme_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  lives_colRGBA64_t lcol;

  char *filt[]= {"*.tar.gz",NULL};

  char theme_name[128];

  char *file_name,*tmp,*tmp2,*com,*fname;
  char *sepimg_ext,*frameimg_ext,*sepimg,*frameimg;
  char *dfile,*themefile;

  boolean set_opt=FALSE;

  int response;

  do {
    // prompt for a set name, advise user to save set
    renamew=create_rename_dialog(8);
    lives_widget_show_all(renamew->dialog);
    response=lives_dialog_run(LIVES_DIALOG(renamew->dialog));
    if (response==LIVES_RESPONSE_CANCEL) {
      lives_free(renamew);
      renamew=NULL;
      return;
    }
    lives_snprintf(theme_name,128,"%s",(tmp=U82F(lives_entry_get_text(LIVES_ENTRY(renamew->entry)))));
    lives_widget_destroy(renamew->dialog);
    lives_free(renamew);
    renamew=NULL;
    lives_free(tmp);
    lives_widget_context_update();
  } while (!is_legal_set_name(theme_name,TRUE));

  fname=lives_strdup_printf("%s.tar.gz",theme_name);

  file_name=choose_file(capable->home_dir,fname,filt,
                        LIVES_FILE_CHOOSER_ACTION_SAVE,_("Choose a directory to export to"),NULL);

  lives_free(fname);

  if (file_name==NULL) {
    return;
  }


  // create a header.theme file in tmp, then zip it up with the images

  sepimg_ext=get_extension(mainw->sepimg_path);
  frameimg_ext=get_extension(mainw->frameblank_path);

#ifndef IS_MINGW
  dfile=lives_strdup_printf("%s/theme%d/",prefs->tmpdir,capable->mainpid);
  themefile=lives_strdup_printf("%s/header.theme",dfile);
#ifdef GUI_GTK
#if !GTK_CHECK_VERSION(3,0,0)
  lives_free(themefile);
  themefile=lives_strdup_printf("%s/header.theme_gtk2",dfile);
#endif
#endif
  sepimg=lives_strdup_printf("%s/main.%s",dfile,sepimg_ext);
  frameimg=lives_strdup_printf("%s/frame.%s",dfile,frameimg_ext);
#else
  dfile=lives_strdup_printf("%s\\theme%d\\",prefs->tmpdir,capable->mainpid);
  themefile=lives_strdup_printf("%s\\header.theme",dfile);
#ifdef GUI_GTK
#if !GTK_CHECK_VERSION(3,0,0)
  lives_free(themefile);
  themefile=lives_strdup_printf("%s\\header.theme_gtk2",dfile);
#endif
#endif
  sepimg=lives_strdup_printf("%s\\main.%s",dfile,sepimg_ext);
  frameimg=lives_strdup_printf("%s\\frame.%s",dfile,frameimg_ext);
#endif

  lives_free(sepimg_ext);
  lives_free(frameimg_ext);

  lives_mkdir_with_parents(dfile,S_IRWXU);

  lcol.red=palette->style;
  lcol.green=lcol.blue=lcol.alpha=0;

  set_theme_colour_pref(themefile,THEME_DETAIL_STYLE,&lcol);

  widget_color_to_lives_rgba(&lcol,&palette->normal_fore);
  set_theme_colour_pref(themefile,THEME_DETAIL_NORMAL_FORE,&lcol);

  widget_color_to_lives_rgba(&lcol,&palette->normal_back);
  set_theme_colour_pref(themefile,THEME_DETAIL_NORMAL_BACK,&lcol);

  widget_color_to_lives_rgba(&lcol,&palette->menu_and_bars_fore);
  set_theme_colour_pref(themefile,THEME_DETAIL_ALT_FORE,&lcol);

  widget_color_to_lives_rgba(&lcol,&palette->menu_and_bars);
  set_theme_colour_pref(themefile,THEME_DETAIL_ALT_BACK,&lcol);

  widget_color_to_lives_rgba(&lcol,&palette->info_text);
  set_theme_colour_pref(themefile,THEME_DETAIL_INFO_TEXT,&lcol);

  widget_color_to_lives_rgba(&lcol,&palette->info_base);
  set_theme_colour_pref(themefile,THEME_DETAIL_INFO_BASE,&lcol);


  if (set_opt) {
    widget_color_to_lives_rgba(&lcol,&palette->mt_timecode_fg);
    set_theme_colour_pref(themefile,THEME_DETAIL_MT_TCFG,&lcol);

    widget_color_to_lives_rgba(&lcol,&palette->mt_timecode_bg);
    set_theme_colour_pref(themefile,THEME_DETAIL_MT_TCBG,&lcol);

    set_theme_colour_pref(themefile,THEME_DETAIL_AUDCOL,&palette->audcol);
    set_theme_colour_pref(themefile,THEME_DETAIL_VIDCOL,&palette->vidcol);
    set_theme_colour_pref(themefile,THEME_DETAIL_FXCOL,&palette->fxcol);

    set_theme_colour_pref(themefile,THEME_DETAIL_MT_TLREG,&palette->mt_timeline_reg);
    set_theme_colour_pref(themefile,THEME_DETAIL_MT_MARK,&palette->mt_mark);
    set_theme_colour_pref(themefile,THEME_DETAIL_MT_EVBOX,&palette->mt_evbox);

    set_theme_colour_pref(themefile,THEME_DETAIL_FRAME_SURROUND,&palette->frame_surround);

    set_theme_colour_pref(themefile,THEME_DETAIL_CE_SEL,&palette->ce_sel);
    set_theme_colour_pref(themefile,THEME_DETAIL_CE_UNSEL,&palette->ce_unsel);
  }

  lives_free(themefile);

  tmp=lives_strdup_printf(_("Exporting theme as %s..."),file_name);
  d_print(tmp);
  lives_free(tmp);

  // copy images for packaging
  mainw->com_failed=FALSE;
  lives_cp(mainw->sepimg_path,sepimg);
  lives_free(sepimg);

  if (mainw->com_failed) {
    lives_rmdir(dfile,TRUE);
    lives_free(frameimg);
    lives_free(file_name);
    lives_free(dfile);
    d_print_failed();
    return;
  }

  lives_cp(mainw->frameblank_path,frameimg);
  lives_free(frameimg);

  if (mainw->com_failed) {
    lives_rmdir(dfile,TRUE);
    lives_free(file_name);
    lives_free(dfile);
    d_print_failed();
    return;
  }


  com=lives_strdup_printf("%s create_package \"%s\" \"%s\"",prefs->backend_sync,
                          (tmp=lives_filename_from_utf8(file_name,-1,NULL,NULL,NULL)),
                          (tmp2=lives_filename_from_utf8(dfile,-1,NULL,NULL,NULL)));

  lives_free(tmp);
  lives_free(tmp2);
  lives_free(file_name);

  mainw->com_failed=FALSE;

  lives_system(com,TRUE);
  lives_free(com);

  lives_rmdir(dfile,TRUE);
  lives_free(dfile);

  if (mainw->com_failed) {
    d_print_failed();
    return;
  }

  d_print_done();

}



void on_backup_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  char *filt[]= {"*.lv1",NULL};
  char *file_name = choose_file(strlen(mainw->proj_save_dir)?mainw->proj_save_dir:NULL,NULL,filt,
                                LIVES_FILE_CHOOSER_ACTION_SAVE,_("Backup as .lv1 file"),NULL);

  if (file_name==NULL) return;

  backup_file(mainw->current_file,1,cfile->frames,file_name);

  lives_snprintf(mainw->proj_save_dir,PATH_MAX,"%s",file_name);
  get_dirname(mainw->proj_save_dir);
  lives_free(file_name);
}



void on_restore_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  char *filt[]= {"*.lv1",NULL};
  char *file_name = choose_file(strlen(mainw->proj_load_dir)?mainw->proj_load_dir:NULL,NULL,filt,
                                LIVES_FILE_CHOOSER_ACTION_OPEN,_("Restore .lv1 file"),NULL);

  if (file_name==NULL) return;

  restore_file(file_name);

  lives_snprintf(mainw->proj_load_dir,PATH_MAX,"%s",file_name);
  get_dirname(mainw->proj_load_dir);
  lives_free(file_name);
}



void mt_memory_free(void) {
  int i;

  threaded_dialog_spin(0.);

  mainw->multitrack->no_expose=TRUE;

  if (mainw->current_file>-1&&cfile->achans>0) {
    delete_audio_tracks(mainw->multitrack,mainw->multitrack->audio_draws,FALSE);
    if (mainw->multitrack->audio_vols!=NULL) lives_list_free(mainw->multitrack->audio_vols);
  }

  if (mainw->multitrack->video_draws!=NULL) {
    for (i=0; i<mainw->multitrack->num_video_tracks; i++) {
      delete_video_track(mainw->multitrack,i,FALSE);
    }
    lives_list_free(mainw->multitrack->video_draws);
  }

  lives_object_unref(mainw->multitrack->clip_scroll);
  lives_object_unref(mainw->multitrack->in_out_box);

  lives_list_free(mainw->multitrack->tl_marks);

  if (mainw->multitrack->event_list!=NULL) event_list_free(mainw->multitrack->event_list);
  mainw->multitrack->event_list=NULL;

  if (mainw->multitrack->undo_mem!=NULL) event_list_free_undos(mainw->multitrack);

  recover_layout_cancelled(FALSE);

  threaded_dialog_spin(0.);
}




void on_quit_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  char *esave_dir,*msg,*tmp;

  boolean has_layout_map=FALSE;
  boolean had_clips=FALSE,legal_set_name;

  register int i;

  if (user_data!=NULL&&LIVES_POINTER_TO_INT(user_data)==1) mainw->only_close=TRUE;
  else mainw->only_close=FALSE;

  // stop if playing
  if (mainw->playing_file>-1) {
    mainw->cancelled=CANCEL_APP_QUIT;
    return;
  }

  if (mainw->multitrack!=NULL) {
    if (mainw->multitrack->idlefunc>0) {
      lives_source_remove(mainw->multitrack->idlefunc);
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
  } else if (mainw->stored_layout_undos!=NULL) {
    stored_event_list_free_undos();
  }

  // do not popup layout errors if the set name changes
  if (!mainw->only_close) mainw->is_exiting=TRUE;

  if (mainw->scrap_file>-1) close_scrap_file();
  if (mainw->ascrap_file>-1) close_ascrap_file();

  if (mainw->clips_available>0) {
    char *set_name;
    _entryw *cdsw=create_cds_dialog(1);
    int resp;
    had_clips=TRUE;
    do {
      legal_set_name=TRUE;
      lives_widget_show_all(cdsw->dialog);
      resp=lives_dialog_run(LIVES_DIALOG(cdsw->dialog));
      if (resp==LIVES_RESPONSE_CANCEL) {
        lives_widget_destroy(cdsw->dialog);
        lives_free(cdsw);
        if (mainw->multitrack!=NULL) {
          mt_sensitise(mainw->multitrack);
          mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
        }
        return;
      }
      if (resp==2) {
        // save set
        if ((legal_set_name=is_legal_set_name((set_name=U82F(lives_entry_get_text(LIVES_ENTRY(cdsw->entry)))),TRUE))) {
          lives_widget_destroy(cdsw->dialog);
          lives_free(cdsw);

          if (prefs->ar_clipset) set_pref("ar_clipset",set_name);
          else set_pref("ar_clipset","");
          mainw->no_exit=FALSE;
          mainw->leave_recovery=FALSE;
          on_save_set_activate(NULL,(tmp=U82F(set_name)));
          lives_free(tmp);

          if (mainw->multitrack!=NULL) {
            mt_sensitise(mainw->multitrack);
            mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
          }
          lives_free(set_name);
          return;
        }
        legal_set_name=FALSE;
        lives_widget_hide(cdsw->dialog);
        lives_free(set_name);
      }
      if (mainw->was_set&&legal_set_name) {
        if (!do_warning_dialog(_("\n\nSet will be deleted from the disk.\nAre you sure ?\n"))) {
          resp=2;
        }
      }
    } while (resp==2);

    // discard clipset

    lives_widget_destroy(cdsw->dialog);
    lives_free(cdsw);

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
        for (i=1; i<MAX_FILES; i++) {
          if (!(mainw->files[i]==NULL)) {
            if (mainw->was_set&&mainw->files[i]->layout_map!=NULL) {
              remove_layout_files(mainw->files[i]->layout_map);
            }
          }
        }
        // delete layout directory
        esave_dir=lives_build_filename(prefs->tmpdir,mainw->set_name,"layouts",NULL);
        lives_rmdir(esave_dir,FALSE);
        lives_free(esave_dir);
      }
    }
  }

  if (mainw->multitrack!=NULL&&!mainw->only_close) mt_memory_free();
  else if (mainw->multitrack!=NULL) wipe_layout(mainw->multitrack);;

  mainw->was_set=mainw->leave_files=mainw->leave_recovery=FALSE;

  recover_layout_cancelled(FALSE);

  if (had_clips) {
    if (strlen(mainw->set_name))
      msg=lives_strdup_printf(_("Deleting set %s..."),mainw->set_name);
    else
      msg=lives_strdup_printf(_("Deleting set..."),mainw->set_name);
    d_print(msg);
    lives_free(msg);

    do_threaded_dialog(_("Deleting set"),FALSE);
  }

  prefs->crash_recovery=FALSE;
  lives_exit(0);
  prefs->crash_recovery=TRUE;

  if (strlen(mainw->set_name)>0) {
    d_print(_("Set %s was permanently deleted from the disk.\n"),mainw->set_name);
    memset(mainw->set_name,0,1);
  }

  mainw->leave_files=mainw->leave_recovery=TRUE;
}



// TODO - split into undo.c
void on_undo_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  char *com;
  char msg[256];

  boolean bad_header=FALSE;
  boolean retvalb;

  int ostart=cfile->start;
  int oend=cfile->end;
  int current_file=mainw->current_file;
  int switch_file=current_file;
  int asigned,aendian;

  lives_widget_set_sensitive(mainw->undo, FALSE);
  lives_widget_set_sensitive(mainw->redo, TRUE);
  cfile->undoable=FALSE;
  cfile->redoable=TRUE;
  lives_widget_hide(mainw->undo);
  lives_widget_show(mainw->redo);

  mainw->osc_block=TRUE;

  d_print("");

  if (menuitem!=NULL) {
    get_menu_text(mainw->undo,msg);
    mainw->no_switch_dprint=TRUE;
    d_print(msg);
    d_print("...");
    mainw->no_switch_dprint=FALSE;
  }


  if (cfile->undo_action==UNDO_INSERT_SILENCE) {
    on_del_audio_activate(NULL,NULL);
    cfile->undo_action=UNDO_INSERT_SILENCE;
    set_redoable(_("Insert Silence"),TRUE);
  }

  if (cfile->undo_action==UNDO_CUT||cfile->undo_action==UNDO_DELETE||cfile->undo_action==UNDO_DELETE_AUDIO) {
    int reset_achans=0;
    lives_rm(cfile->info_file);

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
        com=lives_strdup_printf("%s undo_audio \"%s\"",prefs->backend_sync,cfile->handle);
      }
      // undo delete selected audio
      // (set with with_audio==2 [audio only],therfore start,end,where are in secs.; times==-1)
      else com=lives_strdup_printf("%s insert \"%s\" \"%s\" %.8f 0. %.8f \"%s\" 2 0 0 0 0 %d %d %d %d %d -1",
                                     prefs->backend,
                                     cfile->handle,get_image_ext_for_type(cfile->img_type),cfile->undo1_dbl,
                                     cfile->undo2_dbl-cfile->undo1_dbl, cfile->handle, cfile->arps, cfile->achans,
                                     cfile->asampsize,!(cfile->signed_endian&AFORM_UNSIGNED),
                                     !(cfile->signed_endian&AFORM_BIG_ENDIAN));
    } else {
      // undo cut or delete (times to insert is -1)
      // start,end, where are in frames
      cfile->undo1_boolean&=mainw->ccpd_with_sound;
      com=lives_strdup_printf("%s insert \"%s\" \"%s\" %d %d %d \"%s\" %d %d 0 0 %.3f %d %d %d %d %d -1",
                              prefs->backend,cfile->handle,
                              get_image_ext_for_type(cfile->img_type),cfile->undo_start-1,cfile->undo_start,
                              cfile->undo_end,cfile->handle, cfile->undo1_boolean, cfile->frames, cfile->fps,
                              cfile->arps, cfile->achans, cfile->asampsize, !(cfile->signed_endian&AFORM_UNSIGNED),
                              !(cfile->signed_endian&AFORM_BIG_ENDIAN));

    }

    mainw->com_failed=FALSE;
    lives_system(com,FALSE);
    lives_free(com);

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

    reget_afilesize(mainw->current_file);
    get_play_times();

    if (bad_header) do_header_write_error(mainw->current_file);

  }


  if (cfile->undo_action==UNDO_RESIZABLE||cfile->undo_action==UNDO_RENDER||cfile->undo_action==UNDO_EFFECT||
      cfile->undo_action==UNDO_MERGE||(cfile->undo_action==UNDO_ATOMIC_RESAMPLE_RESIZE&&
                                       (cfile->frames!=cfile->old_frames||cfile->hsize!=cfile->ohsize||
                                        cfile->vsize!=cfile->ovsize||cfile->fps!=cfile->undo1_dbl))) {
    char *audfile;

    com=lives_strdup_printf("%s undo \"%s\" %d %d \"%s\"",prefs->backend,cfile->handle,cfile->undo_start,cfile->undo_end,
                            get_image_ext_for_type(cfile->img_type));
    lives_rm(cfile->info_file);
    mainw->com_failed=FALSE;
    lives_system(com,FALSE);
    lives_free(com);

    if (mainw->com_failed) return;

    mainw->com_failed=FALSE;
    mainw->cancelled=CANCEL_NONE;
    mainw->error=FALSE;

    // show a progress dialog, not cancellable
    cfile->progress_start=cfile->undo_start;
    cfile->progress_end=cfile->undo_end;
    do_progress_dialog(TRUE,FALSE,_("Undoing"));

    if (cfile->undo_action!=UNDO_ATOMIC_RESAMPLE_RESIZE) {
      audfile=lives_strdup_printf("%s/%s/audio.bak",prefs->tmpdir,cfile->handle);
      if (lives_file_test(audfile, LIVES_FILE_TEST_EXISTS)) {
        // restore overwritten audio
        com=lives_strdup_printf("%s undo_audio \"%s\"",prefs->backend_sync,cfile->handle);
        mainw->com_failed=FALSE;
        lives_rm(cfile->info_file);
        lives_system(com,FALSE);
        lives_free(com);
        if (mainw->com_failed) return;

        retvalb=do_auto_dialog(_("Restoring audio..."),0);
        if (!retvalb) {
          d_print_failed();
          //cfile->may_be_damaged=TRUE;
          return;
        }
        reget_afilesize(mainw->current_file);
      }
      lives_free(audfile);
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
      } else {
        save_frame_index(mainw->current_file);
        cfile->frame_index_back=tmpindex;
      }
    }
  }

  if (cfile->undo_action==UNDO_ATOMIC_RESAMPLE_RESIZE&&(cfile->frames!=cfile->old_frames||
      cfile->hsize!=cfile->ohsize||cfile->vsize!=cfile->ovsize)) {

    if (cfile->frames>cfile->old_frames) {
      com=lives_strdup_printf("%s cut \"%s\" %d %d %d %d \"%s\" %.3f %d %d %d",
                              prefs->backend,cfile->handle,cfile->old_frames+1,
                              cfile->frames, FALSE, cfile->frames, get_image_ext_for_type(cfile->img_type),
                              cfile->fps, cfile->arate, cfile->achans, cfile->asampsize);

      cfile->progress_start=cfile->old_frames+1;
      cfile->progress_end=cfile->frames;

      lives_rm(cfile->info_file);
      mainw->com_failed=FALSE;
      lives_system(com,FALSE);
      lives_free(com);

      if (mainw->com_failed) return;

      // show a progress dialog, not cancellable
      do_progress_dialog(TRUE,FALSE,_("Deleting excess frames"));

      if (cfile->clip_type==CLIP_TYPE_FILE) {
        delete_frames_from_virtual(mainw->current_file, cfile->old_frames+1, cfile->frames);
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
      on_delete_activate(NULL,NULL);

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
    lives_rm(cfile->info_file);
    com=lives_strdup_printf("%s undo_audio \"%s\"",prefs->backend_sync,cfile->handle);
    mainw->com_failed=FALSE;
    mainw->cancelled=CANCEL_NONE;
    mainw->error=FALSE;
    lives_system(com,FALSE);
    lives_free(com);

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
    lives_rm(cfile->info_file);
    com=lives_strdup_printf("%s undo_audio \"%s\"",prefs->backend_sync,cfile->handle);
    mainw->com_failed=FALSE;
    lives_system(com,FALSE);
    lives_free(com);

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
    if ((cfile->end=(int)(cfile->end/cfile->fps*cfile->undo1_dbl+.49999))<1) cfile->end=1;
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
      com=lives_strdup_printf(_("Length of video is now %d frames at %.3f frames per second.\n"),cfile->frames,cfile->fps);
    } else {
      mainw->no_switch_dprint=TRUE;
      com=lives_strdup_printf(_("Clipboard was resampled to %d frames.\n"),cfile->frames);
    }
    d_print(com);
    lives_free(com);
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
    if (mainw->event_list!=NULL) event_list_free(mainw->event_list);
    mainw->event_list=cfile->event_list_back;
    cfile->event_list_back=NULL;
    deal_with_render_choice(FALSE);
  }
  mainw->osc_block=FALSE;

}

void on_redo_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  char msg[256];
  char *com;

  int ostart=cfile->start;
  int oend=cfile->end;
  int current_file=mainw->current_file;

  mainw->osc_block=TRUE;

  cfile->undoable=TRUE;
  cfile->redoable=FALSE;
  lives_widget_hide(mainw->redo);
  lives_widget_show(mainw->undo);
  lives_widget_set_sensitive(mainw->undo,TRUE);
  lives_widget_set_sensitive(mainw->redo,FALSE);

  d_print("");

  if (menuitem!=NULL) {
    get_menu_text(mainw->redo,msg);
    mainw->no_switch_dprint=TRUE;
    d_print(msg);
    d_print("...");
    mainw->no_switch_dprint=FALSE;
  }

  if (cfile->undo_action==UNDO_INSERT_SILENCE) {
    on_ins_silence_activate(NULL,NULL);
    mainw->osc_block=FALSE;
    mainw->no_switch_dprint=TRUE;
    d_print_done();
    mainw->no_switch_dprint=FALSE;
    sensitize();
    return;
  }
  if (cfile->undo_action==UNDO_CHANGE_SPEED) {
    on_change_speed_ok_clicked(NULL,NULL);
    mainw->osc_block=FALSE;
    d_print_done();
    return;
  }
  if (cfile->undo_action==UNDO_RESAMPLE) {
    on_resample_vid_ok(NULL,NULL);
    mainw->osc_block=FALSE;
    return;
  }
  if (cfile->undo_action==UNDO_AUDIO_RESAMPLE) {
    on_resaudio_ok_clicked(NULL,NULL);
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
      switch_to_file(mainw->current_file,mainw->current_file);
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
    com=lives_strdup_printf("%s undo_audio \"%s\"",prefs->backend_sync,cfile->handle);
    lives_rm(cfile->info_file);
    mainw->com_failed=FALSE;
    lives_system(com,FALSE);
    lives_free(com);

    if (mainw->com_failed) {
      d_print_failed();
      return;
    }

    // show a progress dialog, not cancellable
    do_progress_dialog(TRUE,FALSE,_("Redoing"));

    if (mainw->error) {
      d_print_failed();
      return;
    }

    d_print_done();
    switch_to_file(mainw->current_file,mainw->current_file);
    mainw->osc_block=FALSE;
    return;
  }

  com=lives_strdup_printf("%s redo \"%s\" %d %d \"%s\"",prefs->backend,cfile->handle,cfile->undo_start,cfile->undo_end,
                          get_image_ext_for_type(cfile->img_type));
  lives_rm(cfile->info_file);
  mainw->com_failed=FALSE;
  lives_system(com,FALSE);
  lives_free(com);

  if (mainw->com_failed) {
    d_print_failed();
    return;
  }

  cfile->progress_start=cfile->undo_start;
  cfile->progress_end=cfile->undo_end;

  // show a progress dialog, not cancellable
  do_progress_dialog(TRUE,FALSE,_("Redoing"));

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
    switch_to_file((mainw->current_file=0),current_file);
  } else {
    if (cfile->start>=cfile->undo_start) load_start_image(cfile->start);
    if (cfile->end<=cfile->undo_end) load_end_image(cfile->end);
  }

  d_print_done();
  mainw->osc_block=FALSE;
}



//////////////////////////////////////////////////

void on_copy_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  gchar *com;

  int current_file=mainw->current_file;
  int start,end;

  desensitize();

  d_print(""); // force switchtext

  if (mainw->ccpd_with_sound&&cfile->achans>0)
    d_print(_("Copying frames %d to %d (with sound) to the clipboard..."),cfile->start,cfile->end);
  else
    d_print(lives_strdup_printf(_("Copying frames %d to %d to the clipboard..."),cfile->start,cfile->end));

  init_clipboard();

  lives_rm(cfile->info_file);
  mainw->last_transition_loops=1;

  start=cfile->start;
  end=cfile->end;

  if (cfile->clip_type==CLIP_TYPE_FILE) {
    // for virtual frames, we copy only the frame_index
    clipboard->clip_type=CLIP_TYPE_FILE;
    clipboard->interlace=cfile->interlace;
    clipboard->deinterlace=cfile->deinterlace;
    clipboard->frame_index=frame_index_copy(cfile->frame_index,end-start+1,start-1);
    clipboard->frames=end-start+1;
    check_if_non_virtual(0,1,clipboard->frames);
    if (clipboard->clip_type==CLIP_TYPE_FILE) {
      clipboard->ext_src=clone_decoder(mainw->current_file);
      end=-end; // allow missing frames
    }
  }

  mainw->fx1_val=1;
  mainw->fx1_bool=FALSE;

  clipboard->img_type=cfile->img_type;

  // copy audio and frames
  com=lives_strdup_printf("%s insert \"%s\" \"%s\" 0 %d %d \"%s\" %d 0 0 0 %.3f %d %d %d %d %d",prefs->backend,
                          clipboard->handle, get_image_ext_for_type(clipboard->img_type),
                          start, end, cfile->handle, mainw->ccpd_with_sound, cfile->fps, cfile->arate,
                          cfile->achans, cfile->asampsize, !(cfile->signed_endian&AFORM_UNSIGNED),
                          !(cfile->signed_endian&AFORM_BIG_ENDIAN));

  if (clipboard->clip_type==CLIP_TYPE_FILE) end=-end;

  mainw->com_failed=FALSE;
  lives_system(com,FALSE);
  lives_free(com);

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
  if (!do_progress_dialog(TRUE,TRUE,_("Copying to the clipboard"))) {
#ifdef IS_MINGW
    // kill any active processes: for other OSes the backend does this
    // get pid from backend
    FILE *rfile;
    ssize_t rlen;
    char val[16];
    int pid;
    com=lives_strdup_printf("%s get_pid_for_handle \"%s\"",prefs->backend_sync,cfile->handle);
    rfile=popen(com,"r");
    rlen=fread(val,1,16,rfile);
    pclose(rfile);
    memset(val+rlen,0,1);
    pid=atoi(val);

    lives_win32_kill_subprocesses(pid,TRUE);
#endif

    // close clipboard, it is invalid

    com=lives_strdup_printf("%s close \"%s\"",prefs->backend,clipboard->handle);
    lives_system(com,FALSE);
    lives_free(com);
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
  lives_snprintf(clipboard->type,40,"Frames");

  clipboard->asampsize=clipboard->arate=clipboard->achans=0;
  clipboard->afilesize=0l;

  if (mainw->ccpd_with_sound) {
    clipboard->achans=cfile->achans;
    clipboard->asampsize=cfile->asampsize;

    clipboard->arate=cfile->arate;
    clipboard->arps=cfile->arps;
    clipboard->signed_endian=cfile->signed_endian;
    reget_afilesize(0);
  }

  clipboard->start=1;
  clipboard->end=clipboard->frames;

  get_total_time(clipboard);

  sensitize();
  d_print_done();

}


void on_cut_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  int current_file=mainw->current_file;
  on_copy_activate(menuitem, user_data);
  if (mainw->cancelled) {
    return;
  }
  on_delete_activate(menuitem, user_data);
  if (mainw->current_file==current_file) {
    set_undoable(_("Cut"),TRUE);
    cfile->undo_action=UNDO_CUT;
  }
}


void on_paste_as_new_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  char *com;
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
  d_print(_("Pasting %d frames to new clip %s..."),cfile->frames,cfile->name);
  mainw->no_switch_dprint=FALSE;

  com=lives_strdup_printf("%s insert \"%s\" \"%s\" 0 1 %d \"%s\" %d 0 0 0 %.3f %d %d %d %d %d",
                          prefs->backend,cfile->handle,
                          get_image_ext_for_type(cfile->img_type),clipboard->frames, clipboard->handle,
                          mainw->ccpd_with_sound, clipboard->fps, clipboard->arate, clipboard->achans,
                          clipboard->asampsize, !(cfile->signed_endian&AFORM_UNSIGNED),
                          !(cfile->signed_endian&AFORM_BIG_ENDIAN));

  mainw->com_failed=FALSE;
  lives_system(com,FALSE);
  lives_free(com);

  if (mainw->com_failed) {
    d_print_failed();
    return;
  }

  cfile->nopreview=TRUE;

  // show a progress dialog, not cancellable
  if (!do_progress_dialog(TRUE,TRUE,_("Pasting"))) {

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

  lives_notify(LIVES_OSC_NOTIFY_CLIP_OPENED,"");
}


void on_insert_pre_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  insertw = create_insert_dialog();

  lives_widget_show_all(insertw->insert_dialog);
  mainw->fx1_bool=FALSE;
  mainw->fx1_val=1;

  mainw->fx2_bool=lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(insertw->with_sound));
}



void on_insert_activate(LiVESButton *button, livespointer user_data) {
  double times_to_insert=mainw->fx1_val;
  double audio_stretch;

  char *com;

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
      do_error_dialog(
        _("This operation requires resizing or converting of frames.\nPlease install 'convert' from the Image-magick package, and then restart LiVES.\n"));
      mainw->error=TRUE;
      if (button!=NULL) {
        lives_widget_destroy(insertw->insert_dialog);
        lives_free(insertw);
      }
      return;
    }
  }

  // fit video to audio if requested
  if (mainw->fx1_bool&&(cfile->asampsize*cfile->arate*cfile->achans)) {
    // "insert to fit audio" : number of inserts is (audio_time - video_time) / clipboard_time
    times_to_insert=(cfile->laudio_time-cfile->frames>0?(double)cfile->frames/cfile->fps:0.)/((double)clipboard->frames/clipboard->fps);
  }

  if (times_to_insert<0.&&(mainw->fx1_bool)) {
    do_error_dialog(
      _("\n\nVideo is longer than audio.\nTry selecting all frames, and then using \nthe 'Trim Audio' function from the Audio menu."));
    mainw->error=TRUE;
    if (button!=NULL) {
      lives_widget_destroy(insertw->insert_dialog);
      lives_free(insertw);
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
          do_error_dialog(_("LiVES cannot insert because the audio rates do not match.\nPlease install 'sox', and try again."));
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
    } else {
      insert_start=cfile->start;
    }
    if ((mainw->xlays=layout_frame_is_affected(mainw->current_file,insert_start))!=NULL) {
      if (!do_warning_dialog
          (_("\nInsertion will cause frames to shift in some multitrack layouts.\nAre you sure you wish to continue ?\n"))) {
        if (button!=NULL) {
          lives_widget_destroy(insertw->insert_dialog);
          lives_free(insertw);
        }
        mainw->error=TRUE;
        lives_list_free_strings(mainw->xlays);
        lives_list_free(mainw->xlays);
        mainw->xlays=NULL;
        return;
      }
      add_lmap_error(LMAP_ERROR_SHIFT_FRAMES,cfile->name,(livespointer)cfile->layout_map,mainw->current_file,
                     insert_start,0.,count_resampled_frames(cfile->stored_layout_frame,cfile->stored_layout_fps,
                         cfile->fps)>=insert_start);
      has_lmap_error=TRUE;
      mainw->error=TRUE;
      lives_list_free_strings(mainw->xlays);
      lives_list_free(mainw->xlays);
      mainw->xlays=NULL;
    } else {
      if (!(prefs->warning_mask&WARN_MASK_LAYOUT_ALTER_FRAMES)&&(mainw->xlays=
            layout_frame_is_affected(mainw->current_file,1))!=NULL) {
        if (!do_layout_alter_frames_warning()) {
          lives_list_free_strings(mainw->xlays);
          lives_list_free(mainw->xlays);
          mainw->xlays=NULL;
          return;
        }
        add_lmap_error(LMAP_ERROR_ALTER_FRAMES,cfile->name,(livespointer)cfile->layout_map,mainw->current_file,0,0.,
                       cfile->stored_layout_frame>0);
        lives_list_free_strings(mainw->xlays);
        lives_list_free(mainw->xlays);
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
    } else {
      insert_start=cfile->start;
    }
    if ((mainw->xlays=layout_audio_is_affected(mainw->current_file,(insert_start-1.)/cfile->fps))!=NULL) {
      if (!do_warning_dialog
          (_("\nInsertion will cause audio to shift in some multitrack layouts.\nAre you sure you wish to continue ?\n"))) {
        if (button!=NULL) {
          lives_widget_destroy(insertw->insert_dialog);
          lives_free(insertw);
        }
        mainw->error=TRUE;
        lives_list_free_strings(mainw->xlays);
        lives_list_free(mainw->xlays);
        mainw->xlays=NULL;
        return;
      }
      add_lmap_error(LMAP_ERROR_SHIFT_AUDIO,cfile->name,(livespointer)cfile->layout_map,mainw->current_file,0,
                     (insert_start-1.)/cfile->fps,(insert_start-1.)/cfile->fps<cfile->stored_layout_audio);
      has_lmap_error=TRUE;
    } else {
      if (!(prefs->warning_mask&WARN_MASK_LAYOUT_ALTER_AUDIO)&&
          (mainw->xlays=layout_audio_is_affected(mainw->current_file,0.))!=NULL) {
        if (!do_layout_alter_audio_warning()) {
          lives_list_free_strings(mainw->xlays);
          lives_list_free(mainw->xlays);
          mainw->xlays=NULL;
          return;
        }
        add_lmap_error(LMAP_ERROR_ALTER_AUDIO,cfile->name,(livespointer)cfile->layout_map,mainw->current_file,0,0.,
                       cfile->stored_layout_audio>0.);
        has_lmap_error=TRUE;
        lives_list_free_strings(mainw->xlays);
        lives_list_free(mainw->xlays);
        mainw->xlays=NULL;
      }
    }
  }

  if (button!=NULL) {
    lives_widget_destroy(insertw->insert_dialog);
    lives_widget_context_update();
    lives_free(insertw);
    if ((cfile->fps!=clipboard->fps&&orig_frames>0)||(cfile->arps!=clipboard->arps&&clipboard->achans>0&&with_sound)) {
      if (!do_clipboard_fps_warning()) {
        mainw->error=TRUE;
        return;
      }
    }
    if (prefs->ins_resample&&clipboard->fps!=cfile->fps&&orig_frames!=0) {
      cb_end=count_resampled_frames(clipboard->frames,clipboard->fps,cfile->fps);
    }
  } else {
    // called from on_merge_activate()
    cb_start=mainw->fx1_start;
    cb_end=mainw->fx2_start;

    // we will use leave_backup as this will leave our
    // merge backup in place
    leave_backup=-1;
  }

  if (mainw->insert_after) {
    cfile->insert_start=cfile->end+1;
  } else {
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
    } else {
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
            lives_rm(clipboard->info_file);
            com=lives_strdup_printf("%s resample_audio \"%s\" %d %d %d %d %d %d %d %d %d %d %.4f",
                                    prefs->backend,
                                    clipboard->handle,clipboard->arps,clipboard->achans,clipboard->asampsize,
                                    clipboard_signed,clipboard_endian,cfile->arps,clipboard->achans,
                                    clipboard->asampsize,clipboard_signed,clipboard_endian,audio_stretch);
            mainw->com_failed=FALSE;
            lives_system(com,FALSE);
            lives_free(com);

            if (mainw->com_failed) {
              return;
            }

            mainw->current_file=0;
            mainw->error=FALSE;
            do_progress_dialog(TRUE,FALSE,_("Resampling clipboard audio"));
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
          lives_rm(clipboard->info_file);
          com=lives_strdup_printf("%s resample_audio \"%s\" %d %d %d %d %d %d %d %d %d %d",
                                  prefs->backend,clipboard->handle,
                                  clipboard->arps,clipboard->achans,clipboard->asampsize,clipboard_signed,
                                  clipboard_endian,cfile->arps,cfile->achans,cfile->asampsize,cfile_signed,cfile_endian);
          mainw->com_failed=FALSE;
          lives_system(com,FALSE);
          lives_free(com);

          if (mainw->com_failed) {
            return;
          }

          mainw->current_file=0;
          do_progress_dialog(TRUE,FALSE,_("Resampling clipboard audio"));
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
            (_("\n\nLiVES was unable to resample the clipboard audio. \nClipboard audio has been erased.\n"));
          } else {
            lives_rm(clipboard->info_file);
            mainw->current_file=0;
            com=lives_strdup_printf("%s undo_audio \"%s\"",prefs->backend_sync,clipboard->handle);
            lives_system(com,FALSE);
            lives_free(com);
            mainw->current_file=current_file;

            clipboard->arps=ocarps;
            reget_afilesize(0);

            if (!do_yesno_dialog
                (_("\n\nLiVES was unable to resample the clipboard audio.\nDo you wish to continue with the insert \nusing unchanged audio ?\n"))) {
              mainw->error=TRUE;
              return;
            }
          }
        }
      }
    }
  }


  if (!virtual_ins && clipboard->frame_index!=NULL) {
    int current_file=mainw->current_file;
    boolean retb;
    mainw->cancelled=CANCEL_NONE;
    mainw->current_file=0;
    cfile->progress_start=1;
    cfile->progress_end=count_virtual_frames(cfile->frame_index,1,cb_end);
    do_threaded_dialog(_("Pulling frames from clipboard"),TRUE);
    retb=virtual_to_images(mainw->current_file,1,cb_end,TRUE,NULL);
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

  // if pref is set, resample clipboard video
  if (prefs->ins_resample&&cfile->fps!=clipboard->fps&&orig_frames>0) {
    if (!resample_clipboard(cfile->fps)) return;
  }

  if (mainw->fx1_bool&&(cfile->asampsize*cfile->arate*cfile->achans)) {
    // in theory this should not change after resampling, but we will recalculate anyway

    // "insert to fit audio" : number of inserts is (audio_time - video_time) / clipboard_time
    times_to_insert=(cfile->laudio_time-cfile->frames>0?(double)cfile->frames/cfile->fps:0.)/((double)clipboard->frames/clipboard->fps);
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
    d_print(_("Inserting %d%s frames from the clipboard..."),remainder_frames,
            times_to_insert>1.?" remainder":"");

    com=lives_strdup_printf("%s insert \"%s\" \"%s\" %d %d %d \"%s\" %d %d %d %d %.3f %d %d %d %d %d",
                            prefs->backend,cfile->handle,
                            get_image_ext_for_type(cfile->img_type), where, clipboard->frames-remainder_frames+1,
                            end, clipboard->handle, with_sound, cfile->frames, hsize, vsize, cfile->fps,
                            cfile->arate, cfile->achans, cfile->asampsize, !(cfile->signed_endian&AFORM_UNSIGNED),
                            !(cfile->signed_endian&AFORM_BIG_ENDIAN));

    lives_rm(cfile->info_file);
    mainw->com_failed=FALSE;
    lives_system(com,FALSE);
    lives_free(com);

    if (mainw->com_failed) {
      d_print_failed();
      return;
    }

    cfile->progress_start=1;
    cfile->progress_end=remainder_frames;

    do_progress_dialog(TRUE,FALSE,_("Inserting"));

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
      reget_afilesize(mainw->current_file);
    }
    get_play_times();
    d_print_done();
  }


  // inserts of whole clipboard
  if ((int)times_to_insert>1) {
    d_print("");
    d_print(_("Inserting %d times from the clipboard%s..."),(int)times_to_insert,with_sound?
            " (with sound)":"");
  } else if ((int)times_to_insert>0) {
    d_print("");
    d_print(_("Inserting %d frames from the clipboard%s..."),cb_end-cb_start+1,with_sound?
            " (with sound)":"");
  }

  if (virtual_ins) cb_end=-cb_end;

  // for an insert after a merge we set our start posn. -ve
  // this should indicate to the back end to leave our
  // backup frames alone

  com=lives_strdup_printf("%s insert \"%s\" \"%s\" %d %d %d \"%s\" %d %d %d %d %.3f %d %d %d %d %d %d",
                          prefs->backend,cfile->handle,
                          get_image_ext_for_type(cfile->img_type), where, cb_start*leave_backup, cb_end,
                          clipboard->handle, with_sound, cfile->frames, hsize, vsize, cfile->fps, cfile->arate,
                          cfile->achans, cfile->asampsize, !(cfile->signed_endian&AFORM_UNSIGNED),
                          !(cfile->signed_endian&AFORM_BIG_ENDIAN), (int)times_to_insert);

  if (virtual_ins) cb_end=-cb_end;

  cfile->progress_start=1;
  cfile->progress_end=(cb_end-cb_start+1)*(int)times_to_insert+cfile->frames-where;
  mainw->com_failed=FALSE;
  lives_rm(cfile->info_file);
  lives_system(com,FALSE);
  lives_free(com);

  if (mainw->com_failed) {
    d_print_failed();
    return;
  }

  // show a progress dialog
  cfile->nopreview=TRUE;
  if (!do_progress_dialog(TRUE,TRUE,_("Inserting"))) {
    // cancelled

    cfile->nopreview=FALSE;

    if (mainw->error) {
      d_print_failed();
      return;
    }

    // clean up moved/inserted frames
    com=lives_strdup_printf("%s undo_insert \"%s\" %d %d %d \"%s\"",
                            prefs->backend,cfile->handle,where+1,
                            where+(cb_end-cb_start+1)*(int)times_to_insert,cfile->frames,
                            get_image_ext_for_type(cfile->img_type));
    lives_system(com,FALSE);
    lives_free(com);

    cfile->start=start;
    cfile->end=end;

    if (with_sound) {
      // desample clipboard audio
      if (cb_audio_change&&!prefs->conserve_space) {
        lives_rm(clipboard->info_file);
        com=lives_strdup_printf("%s undo_audio \"%s\"",prefs->backend_sync,clipboard->handle);
        mainw->current_file=0;
        lives_system(com,FALSE);
        lives_free(com);
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

    switch_to_file(0,current_file);
    set_undoable(NULL,FALSE);
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
    d_print(_("Inserting %d%s frames from the clipboard..."),remainder_frames,
            times_to_insert>1.?" remainder":"");

    if (virtual_ins) remainder_frames=-remainder_frames;

    com=lives_strdup_printf("%s insert \"%s\" \"%s\" %d %d %d \"%s\" %d %d %d %d %3f %d %d %d %d %d",
                            prefs->backend,cfile->handle,
                            get_image_ext_for_type(cfile->img_type), where, 1, remainder_frames, clipboard->handle,
                            with_sound, cfile->frames, hsize, vsize, cfile->fps, cfile->arate, cfile->achans,
                            cfile->asampsize, !(cfile->signed_endian&AFORM_UNSIGNED),
                            !(cfile->signed_endian&AFORM_BIG_ENDIAN));

    lives_rm(cfile->info_file);
    mainw->com_failed=FALSE;
    lives_system(com,FALSE);

    if (mainw->com_failed) {
      d_print_failed();
      return;
    }

    if (virtual_ins) remainder_frames=-remainder_frames;

    cfile->progress_start=1;
    cfile->progress_end=remainder_frames;

    do_progress_dialog(TRUE,FALSE,_("Inserting"));

    if (mainw->error) {
      d_print_failed();
      return;
    }

    if (cfile->clip_type==CLIP_TYPE_FILE||virtual_ins) {
      insert_images_in_virtual(mainw->current_file,where,remainder_frames,clipboard->frame_index,1);
    }

    cfile->frames+=remainder_frames;
    cfile->insert_end+=remainder_frames;
    lives_free(com);

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
    lives_snprintf(cfile->type,40,"Frames");
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

  lives_signal_handler_block(mainw->spinbutton_end,mainw->spin_end_func);
  lives_spin_button_set_range(LIVES_SPIN_BUTTON(mainw->spinbutton_end),cfile->frames==0?0:1,cfile->frames);
  lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_end),cfile->end);
  lives_signal_handler_unblock(mainw->spinbutton_end,mainw->spin_end_func);


  lives_signal_handler_block(mainw->spinbutton_start,mainw->spin_start_func);
  lives_spin_button_set_range(LIVES_SPIN_BUTTON(mainw->spinbutton_start),cfile->frames==0?0:1,cfile->frames);
  lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_start),cfile->start);
  lives_signal_handler_unblock(mainw->spinbutton_start,mainw->spin_start_func);

  set_undoable(_("Insert"),TRUE);
  cfile->undo1_boolean=with_sound;
  lives_widget_set_sensitive(mainw->select_new, TRUE);

  // mark new file size as 'Unknown'
  cfile->f_size=0l;
  cfile->changed=TRUE;

  if (with_sound) {
    cfile->undo_action=UNDO_INSERT_WITH_AUDIO;
    if (cb_audio_change&&!prefs->conserve_space&&clipboard->achans>0) {
      lives_rm(clipboard->info_file);
      mainw->current_file=0;
      com=lives_strdup_printf("%s undo_audio \"%s\"",prefs->backend_sync,clipboard->handle);
      lives_system(com,FALSE);
      lives_free(com);
      mainw->current_file=current_file;
      clipboard->arps=ocarps;
      reget_afilesize(0);
    }
  } else cfile->undo_action=UNDO_INSERT;

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



void on_delete_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  char *com;

  boolean has_lmap_error=FALSE;
  boolean bad_header=FALSE;

  int frames_cut=cfile->end-cfile->start+1;
  int start=cfile->start;
  int end=cfile->end;

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
          lives_list_free_strings(mainw->xlays);
          lives_list_free(mainw->xlays);
          mainw->xlays=NULL;
          return;
        }
        add_lmap_error(LMAP_ERROR_DELETE_FRAMES,cfile->name,(livespointer)cfile->layout_map,mainw->current_file,
                       cfile->end-frames_cut,0.,count_resampled_frames(cfile->stored_layout_frame,
                           cfile->stored_layout_fps,cfile->fps)>=
                       (cfile->end-frames_cut));
        has_lmap_error=TRUE;
        lives_list_free_strings(mainw->xlays);
        lives_list_free(mainw->xlays);
        mainw->xlays=NULL;
      }
    }

    if (mainw->ccpd_with_sound&&!(prefs->warning_mask&WARN_MASK_LAYOUT_DELETE_AUDIO)) {
      if ((mainw->xlays=layout_audio_is_affected(mainw->current_file,(cfile->end-frames_cut)/cfile->fps))!=NULL) {
        if (!do_warning_dialog
            (_("\nDeletion will cause missing audio in some multitrack layouts.\nAre you sure you wish to continue ?\n"))) {
          lives_list_free_strings(mainw->xlays);
          lives_list_free(mainw->xlays);
          mainw->xlays=NULL;
          return;
        }
        add_lmap_error(LMAP_ERROR_DELETE_AUDIO,cfile->name,(livespointer)cfile->layout_map,mainw->current_file,0,
                       (cfile->end-frames_cut-1.)/cfile->fps,(cfile->end-frames_cut-1.)/
                       cfile->fps<cfile->stored_layout_audio);
        has_lmap_error=TRUE;
        lives_list_free_strings(mainw->xlays);
        lives_list_free(mainw->xlays);
        mainw->xlays=NULL;
      }
    }

    if (!has_lmap_error&&!(prefs->warning_mask&WARN_MASK_LAYOUT_SHIFT_FRAMES)) {
      if ((mainw->xlays=layout_frame_is_affected(mainw->current_file,cfile->start))!=NULL) {
        if (!do_warning_dialog
            (_("\nDeletion will cause frames to shift in some multitrack layouts.\nAre you sure you wish to continue ?\n"))) {
          lives_list_free_strings(mainw->xlays);
          lives_list_free(mainw->xlays);
          mainw->xlays=NULL;
          return;
        }
        add_lmap_error(LMAP_ERROR_SHIFT_FRAMES,cfile->name,(livespointer)cfile->layout_map,mainw->current_file,
                       cfile->start,0.,cfile->start<=count_resampled_frames(cfile->stored_layout_frame,
                           cfile->stored_layout_fps,cfile->fps));
        has_lmap_error=TRUE;
        lives_list_free_strings(mainw->xlays);
        lives_list_free(mainw->xlays);
        mainw->xlays=NULL;
      }
    }

    if (mainw->ccpd_with_sound&&!has_lmap_error&&!(prefs->warning_mask&WARN_MASK_LAYOUT_SHIFT_AUDIO)) {
      if ((mainw->xlays=layout_audio_is_affected(mainw->current_file,(cfile->start-1.)/cfile->fps))!=NULL) {
        if (!do_warning_dialog
            (_("\nDeletion will cause audio to shift in some multitrack layouts.\nAre you sure you wish to continue ?\n"))) {
          lives_list_free_strings(mainw->xlays);
          lives_list_free(mainw->xlays);
          mainw->xlays=NULL;
          return;
        }
        add_lmap_error(LMAP_ERROR_SHIFT_AUDIO,cfile->name,(livespointer)cfile->layout_map,mainw->current_file,0,
                       (cfile->start-1.)/cfile->fps,(cfile->start-1.)/cfile->fps<=cfile->stored_layout_audio);
        has_lmap_error=TRUE;
        lives_list_free_strings(mainw->xlays);
        lives_list_free(mainw->xlays);
        mainw->xlays=NULL;
      }
    }

    if (!has_lmap_error&&!(prefs->warning_mask&WARN_MASK_LAYOUT_ALTER_FRAMES)&&
        (mainw->xlays=layout_frame_is_affected(mainw->current_file,1))!=NULL) {
      if (!do_layout_alter_frames_warning()) {
        lives_list_free_strings(mainw->xlays);
        lives_list_free(mainw->xlays);
        mainw->xlays=NULL;
        return;
      }
      add_lmap_error(LMAP_ERROR_ALTER_FRAMES,cfile->name,(livespointer)cfile->layout_map,mainw->current_file,0,0.,
                     cfile->stored_layout_frame>0);
      has_lmap_error=TRUE;
      lives_list_free_strings(mainw->xlays);
      lives_list_free(mainw->xlays);
      mainw->xlays=NULL;
    }

    if (mainw->ccpd_with_sound&&!has_lmap_error&&!(prefs->warning_mask&WARN_MASK_LAYOUT_ALTER_AUDIO)&&
        (mainw->xlays=layout_audio_is_affected(mainw->current_file,0.))!=NULL) {
      if (!do_layout_alter_audio_warning()) {
        lives_list_free_strings(mainw->xlays);
        lives_list_free(mainw->xlays);
        mainw->xlays=NULL;
        return;
      }
      add_lmap_error(LMAP_ERROR_ALTER_AUDIO,cfile->name,(livespointer)cfile->layout_map,mainw->current_file,0,0.,
                     cfile->stored_layout_audio>0.);
      has_lmap_error=TRUE;
      lives_list_free_strings(mainw->xlays);
      lives_list_free(mainw->xlays);
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
    d_print(""); // force switchtext
    d_print(_("Deleting frames %d to %d%s..."),cfile->start,cfile->end,
            mainw->ccpd_with_sound&&cfile->achans>0?" (with sound)":"");
  }

  com=lives_strdup_printf("%s cut \"%s\" %d %d %d %d \"%s\" %.3f %d %d %d",
                          prefs->backend,cfile->handle,cfile->start,cfile->end,
                          mainw->ccpd_with_sound, cfile->frames, get_image_ext_for_type(cfile->img_type),
                          cfile->fps, cfile->arate, cfile->achans, cfile->asampsize);
  lives_rm(cfile->info_file);
  mainw->com_failed=FALSE;
  lives_system(com,FALSE);
  lives_free(com);

  if (mainw->com_failed) {
    d_print_failed();
    return;
  }

  cfile->progress_start=cfile->start;
  cfile->progress_end=cfile->frames;

  // show a progress dialog, not cancellable
  do_progress_dialog(TRUE,FALSE,_("Deleting"));

  if (cfile->clip_type==CLIP_TYPE_FILE) {
    delete_frames_from_virtual(mainw->current_file, cfile->start, cfile->end);
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
    lives_snprintf(cfile->type,40,"Audio");
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
  } else {
    cfile->end=end;
    if (cfile->end>cfile->frames) {
      cfile->end=cfile->frames;
    }
  }

  lives_signal_handler_block(mainw->spinbutton_end,mainw->spin_end_func);
  lives_spin_button_set_range(LIVES_SPIN_BUTTON(mainw->spinbutton_end),cfile->frames==0?0:1,cfile->frames);
  lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_end),cfile->end);
  lives_signal_handler_unblock(mainw->spinbutton_end,mainw->spin_end_func);

  lives_signal_handler_block(mainw->spinbutton_start,mainw->spin_start_func);
  lives_spin_button_set_range(LIVES_SPIN_BUTTON(mainw->spinbutton_start),cfile->frames==0?0:1,cfile->frames);
  lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_start),cfile->start);
  lives_signal_handler_unblock(mainw->spinbutton_start,mainw->spin_start_func);

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

  set_undoable(_("Delete"),TRUE);
  cfile->undo_action=UNDO_DELETE;
  d_print_done();

  if (has_lmap_error) popup_lmap_errors(NULL,NULL);

  if (mainw->sl_undo_mem!=NULL&&(cfile->stored_layout_frame!=0||(mainw->ccpd_with_sound&&cfile->stored_layout_audio!=0.))) {
    // need to invalidate undo/redo stack, in case file was used in some layout undo
    stored_event_list_free_undos();
  }
}



void on_select_all_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  if (mainw->current_file==-1) return;

  if (mainw->selwidth_locked) {
    if (menuitem!=NULL) do_error_dialog(_("\n\nSelection is locked.\n"));
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


void on_select_start_only_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  if (mainw->current_file==-1) return;
  lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_end),cfile->start);
}

void on_select_end_only_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  if (mainw->current_file==-1) return;
  lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_start),cfile->end);
}



void on_select_invert_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  if (cfile->start==1) {
    lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_start),cfile->end+1);
    lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_end),cfile->frames);
  } else {
    lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_end),cfile->start-1);
    lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_start),1);
  }

  get_play_times();

  load_start_image(cfile->start);
  load_end_image(cfile->end);
}


void on_select_last_activate(LiVESMenuItem *menuitem, livespointer user_data) {
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


void on_select_new_activate(LiVESMenuItem *menuitem, livespointer user_data) {
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


void on_select_to_end_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_end),cfile->frames);
  cfile->end=cfile->frames;
  get_play_times();
  load_end_image(cfile->end);
}


void on_select_from_start_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_start),1);
  cfile->start=cfile->frames>0?1:0;
  get_play_times();
  load_start_image(cfile->start);
}


void on_lock_selwidth_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  mainw->selwidth_locked=!mainw->selwidth_locked;
  lives_widget_set_sensitive(mainw->select_submenu,!mainw->selwidth_locked);
}


void on_menubar_activate_menuitem(LiVESMenuItem *menuitem, livespointer user_data) {
  lives_menu_item_activate(menuitem);
  //gtk_menu_shell_set_take_focus(GTK_MENU_SHELL(mainw->menubar),TRUE);
}


void on_playall_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  if (mainw->current_file<=0) return;

  if (mainw->multitrack!=NULL) {
    if (mainw->playing_file==-1) {
      if (!mainw->multitrack->playing_sel) multitrack_playall(mainw->multitrack);
      else multitrack_play_sel(NULL,mainw->multitrack);
    } else on_pause_clicked();
    return;
  }

  if (mainw->playing_file==-1) {
    if (cfile->proc_ptr!=NULL&&menuitem!=NULL) {
      on_preview_clicked(LIVES_BUTTON(cfile->proc_ptr->preview_button),NULL);
      return;
    }

    if (!mainw->osc_auto) {
      mainw->play_start=calc_frame_from_time(mainw->current_file,
                                             cfile->pointer_time);
      mainw->play_end=cfile->frames;
    }

    mainw->playing_sel=FALSE;
    lives_rm(cfile->info_file);

    play_file();
    lives_ruler_set_value(LIVES_RULER(mainw->hruler),cfile->pointer_time);
    get_play_times();
    mainw->noswitch=FALSE;
  }

}


void on_playsel_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  // play part of a clip (in clip editor)

  if (mainw->current_file<=0) return;

  if (cfile->proc_ptr!=NULL&&menuitem!=NULL) {
    on_preview_clicked(LIVES_BUTTON(cfile->proc_ptr->preview_button),NULL);
    return;
  }

  if (!mainw->is_rendering) {
    mainw->play_start=cfile->start;
    mainw->play_end=cfile->end;
    mainw->clip_switched=FALSE;
  }

  if (!mainw->preview) {
    int orig_play_frame=calc_frame_from_time(mainw->current_file,cfile->pointer_time);
    if (orig_play_frame>mainw->play_start&&orig_play_frame<mainw->play_end) {
      mainw->play_start=orig_play_frame;
    }
  }

  mainw->playing_sel=TRUE;
  lives_rm(cfile->info_file);

  play_file();

  mainw->playing_sel=FALSE;
  lives_ruler_set_value(LIVES_RULER(mainw->hruler),cfile->pointer_time);
  lives_widget_queue_draw(mainw->hruler);

  // in case we are rendering and previewing, in case we now have audio
  if (mainw->preview&&mainw->is_rendering&&mainw->is_processing) reget_afilesize(mainw->current_file);

  get_play_times();
  mainw->noswitch=FALSE;
}


void on_playclip_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  // play the clipboard
  int current_file=mainw->current_file;
  boolean oloop=mainw->loop;
  boolean oloop_cont=mainw->loop_cont;

  // switch to the clipboard
  switch_to_file(current_file,0);
  lives_widget_set_sensitive(mainw->loop_video, FALSE);
  lives_widget_set_sensitive(mainw->loop_continue, FALSE);
  mainw->loop=mainw->loop_cont=FALSE;

  mainw->play_start=1;
  mainw->play_end=clipboard->frames;
  mainw->playing_sel=FALSE;
  mainw->loop=FALSE;

  lives_rm(cfile->info_file);
  play_file();
  mainw->loop=oloop;
  mainw->loop_cont=oloop_cont;

  if (current_file>-1) {
    switch_to_file(0,current_file);
  } else {
    mainw->current_file=current_file;
    close_current_file(0);
  }
  mainw->noswitch=FALSE;
}




void on_record_perf_activate(LiVESMenuItem *menuitem, livespointer user_data) {
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
              jack_rec_audio_to_clip(mainw->ascrap_file, -1, RECA_EXTERNAL);
              mainw->jackd_read->in_use=TRUE;
            } else {
              if (mainw->jackd!=NULL) {
                jack_rec_audio_to_clip(mainw->ascrap_file, -1, RECA_GENERATED);
                mainw->jackd_read->in_use=TRUE;
              }
            }
          }

#endif
#ifdef HAVE_PULSE_AUDIO
          if (prefs->audio_player==AUD_PLAYER_PULSE) {
            if (mainw->agen_key==0&&!mainw->agen_needs_reinit) {
              pulse_rec_audio_to_clip(mainw->ascrap_file, -1, RECA_EXTERNAL);
              mainw->pulsed_read->in_use=TRUE;
            } else {
              if (mainw->pulsed!=NULL) {
                pulse_rec_audio_to_clip(mainw->ascrap_file, -1, RECA_GENERATED);
                mainw->pulsed_read->in_use=TRUE;
              }
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
      if (is_realtime_aplayer(prefs->audio_player)&&(prefs->rec_opts&REC_AUDIO)) {
        weed_plant_t *last_frame=get_last_frame_event(mainw->event_list);
        insert_audio_event_at(mainw->event_list, last_frame, -1, mainw->rec_aclip, 0., 0.);
      }
#endif

      if (prefs->rec_opts&REC_EFFECTS) {
        // add deinit events for all active effects
        pthread_mutex_unlock(&mainw->event_list_mutex);
        add_filter_deinit_events(mainw->event_list);
        pthread_mutex_lock(&mainw->event_list_mutex);
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
    d_print(_("Ready to record. Use 'control' and cursor keys during playback to record your performance.\n(To cancel, press 'r' or click on Play|Record Performance again before you play.)\n"));
    mainw->record=TRUE;
    toggle_record();
    get_play_times();
  } else {
    d_print(_("Record cancelled.\n"));
    enable_record();
    mainw->record=FALSE;
  }
}



boolean record_toggle_callback(LiVESAccelGroup *group, LiVESObject *obj, uint32_t keyval, LiVESXModifierType mod,
                               livespointer user_data) {
  // from osc
  boolean start=(boolean)LIVES_POINTER_TO_INT(user_data);

  if ((start&&(!mainw->record||mainw->record_paused))||(!start&&(mainw->record&&!mainw->record_paused)))
    on_record_perf_activate(NULL,NULL);

  return TRUE;
}






void on_rewind_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  if (mainw->multitrack!=NULL) {
    mt_tl_move(mainw->multitrack,0.);
    return;
  }

  cfile->pointer_time=lives_ruler_set_value(LIVES_RULER(mainw->hruler),0.);
  lives_widget_queue_draw(mainw->hruler);
  lives_widget_set_sensitive(mainw->rewind, FALSE);
  lives_widget_set_sensitive(mainw->m_rewindbutton, FALSE);
  lives_widget_set_sensitive(mainw->trim_to_pstart, FALSE);
  get_play_times();
}


void on_stop_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  if (mainw->multitrack!=NULL&&mainw->multitrack->is_paused&&mainw->playing_file==-1) {
    mainw->multitrack->is_paused=FALSE;
    mainw->multitrack->playing_sel=FALSE;
    mt_tl_move(mainw->multitrack,mainw->multitrack->ptr_time);
    lives_widget_set_sensitive(mainw->stop, FALSE);
    lives_widget_set_sensitive(mainw->m_stopbutton, FALSE);
    return;
  }
  mainw->cancelled=CANCEL_USER;
  if (mainw->jack_can_stop) mainw->jack_can_start=FALSE;
  mainw->jack_can_stop=FALSE;

}



boolean on_stop_activate_by_del(LiVESWidget *widget, LiVESXEventDelete *event, livespointer user_data) {
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


void on_encoder_entry_changed(LiVESCombo *combo, livespointer ptr) {
  LiVESList *encoder_capabilities=NULL;
  LiVESList *ofmt_all=NULL;
  LiVESList *ofmt=NULL;

  char *new_encoder_name = lives_combo_get_active_text(combo);
  char *msg;
  char **array;
  int i;
  render_details *rdet=(render_details *)ptr;
  LiVESList *dummy_list;

  if (!strlen(new_encoder_name)) {
    lives_free(new_encoder_name);
    return;
  }

  if (!strcmp(new_encoder_name,mainw->string_constants[LIVES_STRING_CONSTANT_ANY])) {
    LiVESList *ofmt = NULL;
    ofmt = lives_list_append(ofmt,lives_strdup(mainw->string_constants[LIVES_STRING_CONSTANT_ANY]));

    lives_signal_handler_block(rdet->encoder_combo, rdet->encoder_name_fn);
    // ---
    lives_combo_set_active_string(LIVES_COMBO(rdet->encoder_combo), mainw->string_constants[LIVES_STRING_CONSTANT_ANY]);
    // ---
    lives_signal_handler_unblock(rdet->encoder_combo, rdet->encoder_name_fn);

    lives_combo_populate(LIVES_COMBO(rdet->ofmt_combo), ofmt);
    lives_signal_handler_block(rdet->ofmt_combo, rdet->encoder_ofmt_fn);
    lives_combo_set_active_string(LIVES_COMBO(rdet->ofmt_combo),mainw->string_constants[LIVES_STRING_CONSTANT_ANY]);
    lives_signal_handler_unblock(rdet->ofmt_combo, rdet->encoder_ofmt_fn);

    lives_list_free(ofmt);
    if (prefs->acodec_list!=NULL) {
      lives_list_free_strings(prefs->acodec_list);
      lives_list_free(prefs->acodec_list);
      prefs->acodec_list=NULL;
    }
    prefs->acodec_list = lives_list_append(prefs->acodec_list, lives_strdup(mainw->string_constants[LIVES_STRING_CONSTANT_ANY]));

    lives_combo_populate(LIVES_COMBO(rdet->acodec_combo), prefs->acodec_list);

    lives_combo_set_active_string(LIVES_COMBO(rdet->acodec_combo),mainw->string_constants[LIVES_STRING_CONSTANT_ANY]);

    lives_free(new_encoder_name);

    rdet->enc_changed=FALSE;

    return;
  }

  // finalise old plugin
  plugin_request(PLUGIN_ENCODERS,prefs->encoder.name,"finalise");

  clear_mainw_msg();
  // initialise new plugin
  if ((dummy_list = plugin_request(PLUGIN_ENCODERS, new_encoder_name, "init")) == NULL) {
    if (strlen(mainw->msg)) {
      msg = lives_strdup_printf(_("\n\nThe '%s' plugin reports:\n%s\n"), new_encoder_name, mainw->msg);
    } else {
      msg = lives_strdup_printf
            (_("\n\nUnable to find the 'init' method in the %s plugin.\nThe plugin may be broken or not installed correctly."),
             new_encoder_name);
    }
    lives_free(new_encoder_name);

    if (mainw->is_ready) {
      LiVESWindow *twindow=LIVES_WINDOW(mainw->LiVES);
      if (prefsw!=NULL) twindow=LIVES_WINDOW(prefsw->prefs_dialog);
      else if (mainw->multitrack!=NULL) twindow=LIVES_WINDOW(mainw->multitrack->window);
      if (!prefs->show_gui) twindow=(LiVESWindow *)NULL;
      do_error_dialog_with_check_transient(msg,TRUE,0,twindow);
    }

    lives_free(msg);

    if (prefsw != NULL) {
      lives_signal_handler_block(prefsw->encoder_combo, prefsw->encoder_name_fn);
      // ---
      lives_combo_set_active_string(LIVES_COMBO(prefsw->encoder_combo), prefs->encoder.name);
      // ---
      lives_signal_handler_unblock(prefsw->encoder_combo, prefsw->encoder_name_fn);
    }

    if (rdet != NULL) {
      lives_signal_handler_block(rdet->encoder_combo, rdet->encoder_name_fn);
      // ---
      lives_combo_set_active_string(LIVES_COMBO(rdet->encoder_combo), rdet->encoder_name);
      // ---
      lives_signal_handler_unblock(rdet->encoder_combo, rdet->encoder_name_fn);
    }

    dummy_list = plugin_request(PLUGIN_ENCODERS, prefs->encoder.name, "init");
    if (dummy_list != NULL) {
      lives_list_free_strings(dummy_list);
      lives_list_free(dummy_list);
    }
    return;
  }
  lives_list_free_strings(dummy_list);
  lives_list_free(dummy_list);

  lives_snprintf(future_prefs->encoder.name,51,"%s",new_encoder_name);
  lives_free(new_encoder_name);

  if ((encoder_capabilities=plugin_request(PLUGIN_ENCODERS,future_prefs->encoder.name,"get_capabilities"))==NULL) {

    do_plugin_encoder_error(future_prefs->encoder.name);

    if (prefsw!=NULL) {
      lives_signal_handler_block(prefsw->encoder_combo, prefsw->encoder_name_fn);
      // ---
      lives_combo_set_active_string(LIVES_COMBO(prefsw->encoder_combo), prefs->encoder.name);
      // ---
      lives_signal_handler_unblock(prefsw->encoder_combo, prefsw->encoder_name_fn);
    }

    if (rdet!=NULL) {
      lives_signal_handler_block(rdet->encoder_combo, rdet->encoder_name_fn);
      // ---
      lives_combo_set_active_string(LIVES_COMBO(rdet->encoder_combo), rdet->encoder_name);
      // ---
      lives_signal_handler_unblock(rdet->encoder_combo, rdet->encoder_name_fn);
    }

    plugin_request(PLUGIN_ENCODERS, prefs->encoder.name, "init");
    lives_snprintf(future_prefs->encoder.name,51,"%s",prefs->encoder.name);
    return;
  }
  prefs->encoder.capabilities = atoi((char *)lives_list_nth_data(encoder_capabilities,0));
  lives_list_free_strings(encoder_capabilities);
  lives_list_free(encoder_capabilities);

  // fill list with new formats
  if ((ofmt_all = plugin_request_by_line(PLUGIN_ENCODERS,future_prefs->encoder.name,"get_formats"))!=NULL) {
    for (i=0; i<lives_list_length(ofmt_all); i++) {
      if (get_token_count((char *)lives_list_nth_data(ofmt_all,i),'|')>2) {
        array=lives_strsplit((char *)lives_list_nth_data(ofmt_all,i),"|",-1);
        ofmt=lives_list_append(ofmt,lives_strdup(array[1]));
        lives_strfreev(array);
      }
    }

    if (prefsw!=NULL) {
      // we have to block here, otherwise on_ofmt_changed gets called for every added entry !
      lives_signal_handler_block(prefsw->ofmt_combo, prefsw->encoder_ofmt_fn);

      lives_combo_populate(LIVES_COMBO(prefsw->ofmt_combo), ofmt);

      lives_signal_handler_unblock(prefsw->ofmt_combo, prefsw->encoder_ofmt_fn);
    }

    if (rdet!=NULL) {
      // we have to block here, otherwise on_ofmt_changed gets called for every added entry !
      lives_signal_handler_block(rdet->ofmt_combo, rdet->encoder_ofmt_fn);

      lives_combo_populate(LIVES_COMBO(rdet->ofmt_combo), ofmt);

      lives_signal_handler_unblock(rdet->ofmt_combo, rdet->encoder_ofmt_fn);
    }

    lives_list_free(ofmt);

    // set default (first) output type
    array=lives_strsplit((char *)lives_list_nth_data(ofmt_all,0),"|",-1);

    if (rdet!=NULL) {
      lives_combo_set_active_string(LIVES_COMBO(rdet->ofmt_combo), array[1]);

      if (prefsw==NULL&&strcmp(prefs->encoder.name,future_prefs->encoder.name)) {
        lives_snprintf(prefs->encoder.name,51,"%s",future_prefs->encoder.name);
        set_pref("encoder",prefs->encoder.name);
        lives_snprintf(prefs->encoder.of_restrict,1024,"%s",future_prefs->encoder.of_restrict);
        prefs->encoder.of_allowed_acodecs=future_prefs->encoder.of_allowed_acodecs;
      }
      rdet->enc_changed=TRUE;
      rdet->encoder_name=lives_strdup(prefs->encoder.name);
      lives_widget_set_sensitive(rdet->okbutton,TRUE);
    }

    if (prefsw!=NULL) {
      lives_combo_set_active_string(LIVES_COMBO(prefsw->ofmt_combo), array[1]);
    }
    on_encoder_ofmt_changed(NULL, rdet);
    lives_strfreev(array);
    if (ofmt_all!=NULL) {
      lives_list_free_strings(ofmt_all);
      lives_list_free(ofmt_all);
    }
  }
}


void on_insertwsound_toggled(LiVESToggleButton *togglebutton, livespointer user_data) {
  if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(togglebutton))) {
    lives_widget_set_sensitive(insertw->fit_checkbutton,FALSE);
  } else {
    lives_widget_set_sensitive(insertw->fit_checkbutton,cfile->achans>0);
  }
  mainw->fx2_bool=!mainw->fx2_bool;
}


void on_insfitaudio_toggled(LiVESToggleButton *togglebutton, livespointer user_data) {
  mainw->fx1_bool=!mainw->fx1_bool;

  if (lives_toggle_button_get_active(togglebutton)) {
    lives_widget_set_sensitive(insertw->with_sound,FALSE);
    lives_widget_set_sensitive(insertw->without_sound,FALSE);
    lives_widget_set_sensitive(insertw->spinbutton_times,FALSE);
  } else {
    lives_widget_set_sensitive(insertw->with_sound,clipboard->achans>0);
    lives_widget_set_sensitive(insertw->without_sound,clipboard->achans>0);
    lives_widget_set_sensitive(insertw->spinbutton_times,TRUE);
  }
}




boolean dirchange_callback(LiVESAccelGroup *group, LiVESObject *obj, uint32_t keyval, LiVESXModifierType mod, livespointer user_data) {
  if (mainw->playing_file==-1) return TRUE;

  // change play direction
  if (cfile->play_paused) {
    cfile->freeze_fps=-cfile->freeze_fps;
    return TRUE;
  }

  lives_signal_handler_block(mainw->spinbutton_pb_fps,mainw->pb_fps_func);
  lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_pb_fps),-cfile->pb_fps);
  lives_signal_handler_unblock(mainw->spinbutton_pb_fps,mainw->pb_fps_func);

  // make sure this is called, sometimes we switch clips too soon...
  changed_fps_during_pb(LIVES_SPIN_BUTTON(mainw->spinbutton_pb_fps), NULL);

  return TRUE;
}


boolean fps_reset_callback(LiVESAccelGroup *group, LiVESObject *obj, uint32_t keyval, LiVESXModifierType mod, livespointer user_data) {
  // reset playback fps (cfile->pb_fps) to normal fps (cfile->fps)
  // also resync the audio

  if (mainw->playing_file==-1) return TRUE;

  if (prefs->audio_opts&AUDIO_OPTS_FOLLOW_FPS) {
    resync_audio(cfile->frameno);
  }

  // change play direction
  if (cfile->play_paused) {
    if (cfile->freeze_fps<0.) cfile->freeze_fps=-cfile->fps;
    else cfile->freeze_fps=cfile->fps;
    return TRUE;
  }

  lives_signal_handler_block(mainw->spinbutton_pb_fps,mainw->pb_fps_func);
  if (cfile->pb_fps>0.) lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_pb_fps),cfile->fps);
  else lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_pb_fps),-cfile->fps);
  lives_signal_handler_unblock(mainw->spinbutton_pb_fps,mainw->pb_fps_func);

  // make sure this is called, sometimes we switch clips too soon...
  changed_fps_during_pb(LIVES_SPIN_BUTTON(mainw->spinbutton_pb_fps), NULL);

  return TRUE;
}

boolean prevclip_callback(LiVESAccelGroup *group, LiVESObject *obj, uint32_t keyval, LiVESXModifierType mod, livespointer user_data) {
  LiVESList *list_index;
  int i=0;
  int num_tried=0,num_clips;
  int type=0;

  // prev clip
  // type = 0 : if the effect is a transition, this will change the background clip
  // type = 1 fg only
  // type = 2 bg only

  if (!mainw->interactive) return TRUE;

  if (mainw->current_file<1||mainw->preview||(mainw->is_processing&&cfile->is_loaded)||mainw->cliplist==NULL) return TRUE;

  if (user_data!=NULL) type=LIVES_POINTER_TO_INT(user_data);

  num_clips=lives_list_length(mainw->cliplist);

  if (type==2||(mainw->active_sa_clips==SCREEN_AREA_BACKGROUND&&mainw->playing_file>0&&type!=1)) {
    list_index=lives_list_find(mainw->cliplist, LIVES_INT_TO_POINTER(mainw->blend_file));
  } else {
    list_index=lives_list_find(mainw->cliplist, LIVES_INT_TO_POINTER(mainw->current_file));
  }
  do {
    if (num_tried++==num_clips) return TRUE; // we might have only audio clips, and then we will block here
    if (list_index==NULL||((list_index=list_index->prev)==NULL)) list_index=lives_list_last(mainw->cliplist);
    i=LIVES_POINTER_TO_INT(list_index->data);
  } while ((mainw->files[i]==NULL||mainw->files[i]->opening||mainw->files[i]->restoring||i==mainw->scrap_file||
            i==mainw->ascrap_file||(!mainw->files[i]->frames&&mainw->playing_file>-1))&&
           i!=((type==2||(mainw->playing_file>0&&mainw->active_sa_clips==SCREEN_AREA_BACKGROUND&&type!=1))?
               mainw->blend_file:mainw->current_file));

  switch_clip(type,i,FALSE);

  return TRUE;
}


boolean nextclip_callback(LiVESAccelGroup *group, LiVESObject *obj, uint32_t keyval, LiVESXModifierType mod, livespointer user_data) {
  LiVESList *list_index;
  int i;
  int num_tried=0,num_clips;

  int type=0;

  if (!mainw->interactive) return TRUE;

  // next clip
  // if the effect is a transition, this will change the background clip
  if (mainw->current_file<1||mainw->preview||(mainw->is_processing&&cfile->is_loaded)||mainw->cliplist==NULL) return TRUE;

  if (user_data!=NULL) type=LIVES_POINTER_TO_INT(user_data);

  if (type==2||(mainw->active_sa_clips==SCREEN_AREA_BACKGROUND&&mainw->playing_file>0&&type!=1)) {
    list_index=lives_list_find(mainw->cliplist, LIVES_INT_TO_POINTER(mainw->blend_file));
  } else {
    list_index=lives_list_find(mainw->cliplist, LIVES_INT_TO_POINTER(mainw->current_file));
  }

  num_clips=lives_list_length(mainw->cliplist);

  do {
    if (num_tried++==num_clips) return TRUE; // we might have only audio clips, and then we will block here
    if (list_index==NULL||((list_index=list_index->next)==NULL)) list_index=mainw->cliplist;
    i=LIVES_POINTER_TO_INT(list_index->data);
  } while ((mainw->files[i]==NULL||mainw->files[i]->opening||mainw->files[i]->restoring||i==mainw->scrap_file||
            i==mainw->ascrap_file||(!mainw->files[i]->frames&&mainw->playing_file>-1))&&
           i!=((type==2||(mainw->playing_file>0&&mainw->active_sa_clips==SCREEN_AREA_BACKGROUND&&type!=1))?
               mainw->blend_file:mainw->current_file));

  switch_clip(type,i,FALSE);

  return TRUE;
}


boolean on_save_set_activate(LiVESMenuItem *menuitem, livespointer user_data) {

  // here is where we save clipsets

  // SAVE CLIPSET FUNCTION

  // also handles migration and merging of sets

  // new_set_name can be passed in userdata, it should be in filename encoding

  // TODO - caller to do end_threaded_dialog()

  LiVESList *cliplist;

  char new_handle[256];
  char new_set_name[128];

  char *old_set=lives_strdup(mainw->set_name);
  char *layout_map_file,*layout_map_dir,*new_clips_dir,*current_clips_dir;
  char *tmp;
  char *text;
  char *new_dir;
  char *cwd;
  char *ordfile;
  char *ord_entry;
  char *msg,*extra;
  char *dfile,*osetn,*nsetn;

  boolean is_append=FALSE; // we will overwrite the target layout.map file
  boolean response;
  boolean got_new_handle=FALSE;

  int ord_fd;
  int retval;

  register int i;

  if (mainw->cliplist == NULL) return FALSE;

  // warn the user what will happen
  if (!mainw->no_exit&&!mainw->only_close) extra=lives_strdup(", and LiVES will exit");
  else extra=lives_strdup("");

  msg=lives_strdup_printf(
        _("Saving the set will cause copies of all loaded clips to remain on the disk%s.\n\nPlease press 'Cancel' if that is not what you want.\n"),
        extra);
  lives_free(extra);

  if (menuitem!=NULL&&!do_warning_dialog_with_check(msg,WARN_MASK_SAVE_SET)) {
    lives_free(msg);
    return FALSE;
  }
  lives_free(msg);


  if (mainw->stored_event_list!=NULL&&mainw->stored_event_list_changed) {
    // if we have a current layout, give the user the chance to change their mind
    if (!check_for_layout_del(NULL,FALSE)) return FALSE;
  }


  if (menuitem!=NULL) {
    // this was called from the GUI

    do {
      // prompt for a set name, advise user to save set
      renamew=create_rename_dialog(2);
      lives_widget_show_all(renamew->dialog);
      response=lives_dialog_run(LIVES_DIALOG(renamew->dialog));
      if (response==LIVES_RESPONSE_CANCEL) {
        lives_widget_destroy(renamew->dialog);
        lives_free(renamew);
        return FALSE;
      }
      lives_snprintf(new_set_name,128,"%s",(tmp=U82F(lives_entry_get_text(LIVES_ENTRY(renamew->entry)))));
      lives_widget_destroy(renamew->dialog);
      lives_free(renamew);
      lives_widget_context_update();
    } while (!is_legal_set_name(new_set_name,TRUE));
  } else lives_snprintf(new_set_name,128,"%s",(char *)user_data);

  lives_widget_queue_draw(mainw->LiVES);
  lives_widget_context_update();

  lives_snprintf(mainw->set_name,128,"%s",new_set_name);

  if (strcmp(mainw->set_name,old_set)) {
    // THE USER CHANGED the set name

    // we must migrate all physical files for the set


    // and possibly merge with another set

    new_clips_dir=lives_build_filename(prefs->tmpdir,mainw->set_name,"clips",NULL);
    // check if target clips dir exists, ask if user wants to append files
    if (lives_file_test(new_clips_dir,LIVES_FILE_TEST_IS_DIR)) {
      lives_free(new_clips_dir);
      if (mainw->osc_auto==0) {
        if (!do_set_duplicate_warning(mainw->set_name)) {
          lives_snprintf(mainw->set_name,128,"%s",old_set);
          return FALSE;
        }
      } else if (mainw->osc_auto==1) return FALSE;

      is_append=TRUE;
    } else {
      lives_free(new_clips_dir);
      layout_map_file=lives_build_filename(prefs->tmpdir,mainw->set_name,"layouts","layout.map",NULL);
      // if target has layouts dir but no clips, it means we have old layouts !
      if (lives_file_test(layout_map_file,LIVES_FILE_TEST_EXISTS)) {
        if (do_set_rename_old_layouts_warning(mainw->set_name)) {
          // user answered "yes" - delete
          // clear _old_layout maps
          char *dfile=lives_build_filename(prefs->tmpdir,mainw->set_name,"layouts",NULL);
          lives_rm(dfile);
          lives_free(dfile);
        }
      }
      lives_free(layout_map_file);
    }
  }

  text=lives_strdup_printf(_("Saving set %s"),mainw->set_name);
  do_threaded_dialog(text,FALSE);
  lives_free(text);

  /////////////////////////////////////////////////////////////

  mainw->com_failed=FALSE;

  current_clips_dir=lives_build_filename(prefs->tmpdir,old_set,"clips/",NULL);
  if (strlen(old_set)&&strcmp(mainw->set_name,old_set)&&lives_file_test(current_clips_dir,LIVES_FILE_TEST_IS_DIR)) {
    // set name was changed for an existing set

    if (!is_append) {
      // create new dir, in case it doesn't already exist
      dfile=lives_build_filename(prefs->tmpdir,mainw->set_name,"clips",NULL);
      if (lives_mkdir_with_parents(dfile,S_IRWXU)==-1) {
        if (!check_dir_access(dfile)) {
          // abort if we cannot create the new subdir
          LIVES_ERROR("Could not create directory");
          LIVES_ERROR(dfile);
          d_print_file_error_failed();
          lives_snprintf(mainw->set_name,128,"%s",old_set);
          lives_free(dfile);
          end_threaded_dialog();
          return FALSE;
        }
      }
      lives_free(dfile);
    }
  } else {
    // saving as same name (or as new set)

    dfile=lives_build_filename(prefs->tmpdir,mainw->set_name,"clips",NULL);
    if (lives_mkdir_with_parents(dfile,S_IRWXU)==-1) {
      if (!check_dir_access(dfile)) {
        // abort if we cannot create the new subdir
        LIVES_ERROR("Could not create directory");
        LIVES_ERROR(dfile);
        d_print_file_error_failed();
        lives_snprintf(mainw->set_name,128,"%s",old_set);
        lives_free(dfile);
        end_threaded_dialog();
        return FALSE;
      }
    }
    lives_free(dfile);
  }
  lives_free(current_clips_dir);

  ordfile=lives_build_filename(prefs->tmpdir,mainw->set_name,"order",NULL);

  cwd=lives_get_current_dir();

  do {
    // create the orderfile which lists all the clips in order
    retval=0;
    if (!is_append) ord_fd=creat(ordfile,DEF_FILE_PERMS);
    else ord_fd=open(ordfile,O_CREAT|O_WRONLY|O_APPEND,DEF_FILE_PERMS);

    if (ord_fd<0) {
      retval=do_write_failed_error_s_with_retry(ordfile,lives_strerror(errno),NULL);
      if (retval==LIVES_RESPONSE_CANCEL) {
        end_threaded_dialog();
        lives_free(ordfile);
        return FALSE;
      }
    }

    else {
      char *oldval,*newval;

      mainw->write_failed=FALSE;
      cliplist=mainw->cliplist;

      while (cliplist!=NULL) {
        if (mainw->write_failed) break;
        threaded_dialog_spin(0.);
        lives_widget_context_update();

        // TODO - dirsep

        i=LIVES_POINTER_TO_INT(cliplist->data);
        if (mainw->files[i]!=NULL&&(mainw->files[i]->clip_type==CLIP_TYPE_FILE||
                                    mainw->files[i]->clip_type==CLIP_TYPE_DISK)) {
          if ((tmp=strrchr(mainw->files[i]->handle,'/'))!=NULL) {
            lives_snprintf(new_handle,256,"%s/clips%s",mainw->set_name,tmp);
          } else {
            lives_snprintf(new_handle,256,"%s/clips/%s",mainw->set_name,mainw->files[i]->handle);
          }
          if (strcmp(new_handle,mainw->files[i]->handle)) {
            new_dir=lives_build_filename(prefs->tmpdir,new_handle,NULL);
            if (lives_file_test(new_dir,LIVES_FILE_TEST_IS_DIR)) {
              // get a new unique handle
              get_temp_handle(i,FALSE);
              lives_snprintf(new_handle,256,"%s/clips/%s",mainw->set_name,mainw->files[i]->handle);
            }
            lives_free(new_dir);

            // move the files
            mainw->com_failed=FALSE;

#ifndef IS_MINGW
            oldval=lives_strdup_printf("%s/%s",prefs->tmpdir,mainw->files[i]->handle);
            newval=lives_strdup_printf("%s/%s",prefs->tmpdir,new_handle);
#else
            oldval=lives_strdup_printf("%s\\%s",prefs->tmpdir,mainw->files[i]->handle);
            newval=lives_strdup_printf("%s\\%s",prefs->tmpdir,new_handle);
#endif

            lives_mv(oldval,newval);
            lives_free(oldval);
            lives_free(newval);

            if (mainw->com_failed) {
              end_threaded_dialog();
              lives_free(ordfile);
              return FALSE;
            }

            got_new_handle=TRUE;

            lives_snprintf(mainw->files[i]->handle,256,"%s",new_handle);
#ifndef IS_MINGW
            dfile=lives_build_filename(prefs->tmpdir,mainw->files[i]->handle,".status",NULL);
#else
            dfile=lives_build_filename(prefs->tmpdir,mainw->files[i]->handle,"status",NULL);
#endif
            lives_snprintf(mainw->files[i]->info_file,PATH_MAX,"%s",dfile);
            lives_free(dfile);
          }

          ord_entry=lives_strdup_printf("%s\n",mainw->files[i]->handle);
          lives_write(ord_fd,ord_entry,strlen(ord_entry),FALSE);
          lives_free(ord_entry);

        }

        cliplist=cliplist->next;
      }

      if (mainw->write_failed) {
        retval=do_write_failed_error_s_with_retry(ordfile,NULL,NULL);
      }

    }

  } while (retval==LIVES_RESPONSE_RETRY);

  close(ord_fd);
  lives_free(ordfile);

  lives_chdir(cwd,FALSE);
  lives_free(cwd);

  if (retval==LIVES_RESPONSE_CANCEL) {
    end_threaded_dialog();
    return FALSE;
  }

  if (got_new_handle&&!strlen(old_set)) migrate_layouts(NULL,mainw->set_name);

  if (strlen(old_set)&&strcmp(old_set,mainw->set_name)) {
    layout_map_dir=lives_build_filename(prefs->tmpdir,old_set,"layouts",LIVES_DIR_SEPARATOR_S,NULL);
    layout_map_file=lives_build_filename(layout_map_dir,"layout.map",NULL);
    // update details for layouts - needs_set, current_layout_map and affected_layout_map
    if (lives_file_test(layout_map_file,LIVES_FILE_TEST_EXISTS)) {
      migrate_layouts(old_set,mainw->set_name);
      // save updated layout.map (with new handles), we will move it below

      save_layout_map(NULL,NULL,NULL,layout_map_dir);

      got_new_handle=FALSE;
    }
    lives_free(layout_map_file);
    lives_free(layout_map_dir);

    if (is_append) {
      osetn=lives_build_filename(prefs->tmpdir,old_set,"layouts","layout.map",NULL);
      nsetn=lives_build_filename(prefs->tmpdir,mainw->set_name,"layouts","layout.map",NULL);

      //append current layout.map to target one
      lives_cat(osetn,nsetn,TRUE);

      lives_rm(osetn);

      lives_free(osetn);
      lives_free(nsetn);
    }

    osetn=lives_build_filename(prefs->tmpdir,old_set,"layouts",NULL);
    nsetn=lives_build_filename(prefs->tmpdir,mainw->set_name,NULL);

    // move any layouts from old set to new (including layout.map)
    lives_cp_keep_perms(osetn,nsetn);

    lives_free(osetn);
    lives_free(nsetn);

    osetn=lives_build_filename(prefs->tmpdir,old_set,NULL);
    lives_rmdir(osetn,FALSE);
    lives_free(osetn);
  }


  if (!mainw->was_set&&!strcmp(old_set,mainw->set_name)) {
    // set name was set by export or save layout, now we need to update our layout map
    layout_map_dir=lives_build_filename(prefs->tmpdir,old_set,"layouts",LIVES_DIR_SEPARATOR_S,NULL);
    layout_map_file=lives_build_filename(layout_map_dir,"layout.map",NULL);
    if (lives_file_test(layout_map_file,LIVES_FILE_TEST_EXISTS)) save_layout_map(NULL,NULL,NULL,layout_map_dir);
    mainw->was_set=TRUE;
    got_new_handle=FALSE;
    lives_free(layout_map_dir);
    lives_free(layout_map_file);
    if (mainw->multitrack!=NULL&&!mainw->multitrack->changed) recover_layout_cancelled(FALSE);
  }

  if (mainw->current_layouts_map!=NULL&&strcmp(old_set,mainw->set_name)&&!mainw->is_exiting) {
    // warn the user about layouts if the set name changed
    // but, don't bother the user with errors if we are exiting
    add_lmap_error(LMAP_INFO_SETNAME_CHANGED,old_set,mainw->set_name,0,0,0.,FALSE);
    popup_lmap_errors(NULL,NULL);
  }

  lives_notify(LIVES_OSC_NOTIFY_CLIPSET_SAVED,old_set);

  lives_free(old_set);
  if (!mainw->no_exit) {
    mainw->leave_files=TRUE;
    if (mainw->multitrack!=NULL&&!mainw->only_close) mt_memory_free();
    else if (mainw->multitrack!=NULL) wipe_layout(mainw->multitrack);
    lives_exit(0);
  } else end_threaded_dialog();

  lives_widget_set_sensitive(mainw->vj_load_set, TRUE);
  return TRUE;
}


char *on_load_set_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  // get set name (use a modified rename window)
  char *set_name=NULL;
  int resp;

  if (mainw->multitrack!=NULL) {
    if (mainw->multitrack->idlefunc>0) {
      lives_source_remove(mainw->multitrack->idlefunc);
      mainw->multitrack->idlefunc=0;
    }
    mt_desensitise(mainw->multitrack);
  }

  renamew=create_rename_dialog(3);

  resp=lives_dialog_run(LIVES_DIALOG(renamew->dialog));

  if (resp==LIVES_RESPONSE_OK) {
    set_name=U82F(lives_entry_get_text(LIVES_ENTRY(renamew->entry)));
  }

  // need to clean up renamew
  lives_widget_destroy(renamew->dialog);
  lives_widget_context_update();
  lives_free(renamew);
  renamew=NULL;

  if (resp==LIVES_RESPONSE_OK) {
    if (!is_legal_set_name(set_name,TRUE)) {
      lives_free(set_name);
      set_name=NULL;
    } else {
      if (user_data==NULL) {
        reload_set(set_name);
        lives_free(set_name);
        return NULL;
      }
    }
  }

  return set_name;
}


boolean reload_set(const char *set_name) {
  // this is the main clip set loader

  // CLIP SET LOADER

  // setname should be in filesystem encoding

  FILE *orderfile;

  char *msg;
  char *com;
  char *ordfile;
  char *subfname;
  char vid_open_dir[PATH_MAX];
  char *cwd;

  boolean added_recovery=FALSE;
  boolean needs_update=FALSE;
  boolean keep_threaded_dialog=FALSE;

  int last_file=-1,new_file=-1;
  int current_file=mainw->current_file;
  int clipnum=0;

  memset(mainw->set_name,0,1);

  // check if set is locked
  if (!check_for_lock_file(set_name,0)) {
    d_print_failed();
    if (mainw->multitrack!=NULL) {
      mainw->current_file=mainw->multitrack->render_file;
      mt_sensitise(mainw->multitrack);
      mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
    }
    return FALSE;
  }

  lives_snprintf(mainw->msg,256,"none");

  // check if we already have a threaded dialog running (i.e. we are called from startup)
  if (mainw->threaded_dialog) keep_threaded_dialog=TRUE;

  if (prefs->show_gui && !keep_threaded_dialog) {
    char *tmp;
    msg=lives_strdup_printf(_("Loading clips from set %s"),(tmp=F2U8(set_name)));
    do_threaded_dialog(msg,FALSE);
    lives_free(msg);
    lives_free(tmp);
  }

  ordfile=lives_build_filename(prefs->tmpdir,set_name,"order",NULL);
  orderfile=fopen(ordfile,"r"); // no we can't assert this, because older sets did not have this file
  lives_free(ordfile);

  mainw->suppress_dprint=TRUE;
  mainw->read_failed=FALSE;

  lives_snprintf(vid_open_dir,PATH_MAX,"%s",mainw->vid_load_dir);

  cwd=lives_get_current_dir();

  while (1) {
    if (prefs->show_gui) {
      threaded_dialog_spin(0.);
    }

    if (mainw->cached_list!=NULL) {
      lives_list_free_strings(mainw->cached_list);
      lives_list_free(mainw->cached_list);
      mainw->cached_list=NULL;
    }

    if (orderfile==NULL) {
      // old style (pre 0.9.6)
      com=lives_strdup_printf("%s get_next_in_set \"%s\" \"%s\" %d",prefs->backend_sync,mainw->msg,
                              set_name,capable->mainpid);
      lives_system(com,FALSE);
      lives_free(com);

      if (strlen(mainw->msg)>0&&(strncmp(mainw->msg,"none",4))) {

        if ((new_file=mainw->first_free_file)==-1) {
          recover_layout_map(MAX_FILES);

          if (!keep_threaded_dialog) end_threaded_dialog();

          mainw->suppress_dprint=FALSE;
          too_many_files();
          if (mainw->multitrack!=NULL) {
            mainw->current_file=mainw->multitrack->render_file;
            polymorph(mainw->multitrack,POLY_NONE);
            polymorph(mainw->multitrack,POLY_CLIPS);
            mt_sensitise(mainw->multitrack);
            mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
          }
          lives_free(cwd);
          return FALSE;
        }
        mainw->current_file=new_file;
        get_handle_from_info_file(new_file);
      }
    } else {
      if (lives_fgets(mainw->msg,512,orderfile)==NULL) clear_mainw_msg();
      else memset(mainw->msg+strlen(mainw->msg)-strlen("\n"),0,1);
    }


    if (strlen(mainw->msg)==0||(!strncmp(mainw->msg,"none",4))) {
      mainw->suppress_dprint=FALSE;

      if (!keep_threaded_dialog) end_threaded_dialog();

      if (orderfile!=NULL) fclose(orderfile);

      mainw->current_file=current_file;

      if (last_file>0) {
        threaded_dialog_spin(0.);
        switch_to_file(current_file,last_file);
        threaded_dialog_spin(0.);
      }

      if (clipnum==0) {
        do_set_noclips_error(set_name);
      } else {
        char *tmp;
        reset_clipmenu();
        lives_widget_set_sensitive(mainw->vj_load_set, FALSE);

        lives_snprintf(mainw->set_name,128,"%s",set_name);

        // MUST set set_name before calling this
        recover_layout_map(MAX_FILES);

        d_print(_("%d clips and %d layouts were recovered from set (%s).\n"),
                clipnum,lives_list_length(mainw->current_layouts_map),(tmp=F2U8(set_name)));
        lives_free(tmp);

        lives_notify(LIVES_OSC_NOTIFY_CLIPSET_OPENED,mainw->set_name);

      }

      threaded_dialog_spin(0.);
      if (mainw->multitrack==NULL) {
        if (mainw->is_ready) {
          if (clipnum>0&&mainw->current_file>0) {
            load_start_image(cfile->start);
            load_end_image(cfile->end);
          }
          lives_widget_context_update();
        }
      } else {
        mainw->current_file=mainw->multitrack->render_file;
        polymorph(mainw->multitrack,POLY_NONE);
        polymorph(mainw->multitrack,POLY_CLIPS);
        mt_sensitise(mainw->multitrack);
        mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
      }

      if (!keep_threaded_dialog) end_threaded_dialog();
      lives_free(cwd);
      return TRUE;
    }

    mainw->was_set=TRUE;

    if (prefs->crash_recovery&&!added_recovery) {
      char *recovery_entry=lives_build_filename(set_name,"*",NULL);
      add_to_recovery_file(recovery_entry);
      lives_free(recovery_entry);
      added_recovery=TRUE;
    }

    if (orderfile!=NULL) {
      // newer style (0.9.6+)
      char *tfile;
      char *clipdir=lives_build_filename(prefs->tmpdir,mainw->msg,NULL);
      if (!lives_file_test(clipdir,LIVES_FILE_TEST_IS_DIR)) {
        lives_free(clipdir);
        continue;
      }
      lives_free(clipdir);
      threaded_dialog_spin(0.);
      if ((new_file=mainw->first_free_file)==-1) {
        mainw->suppress_dprint=FALSE;

        if (!keep_threaded_dialog) end_threaded_dialog();

        too_many_files();

        if (mainw->multitrack!=NULL) {
          mainw->current_file=mainw->multitrack->render_file;
          polymorph(mainw->multitrack,POLY_NONE);
          polymorph(mainw->multitrack,POLY_CLIPS);
          mt_sensitise(mainw->multitrack);
          mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
        }

        recover_layout_map(MAX_FILES);
        lives_free(cwd);
        return FALSE;
      }
      mainw->current_file=new_file;
      cfile=(lives_clip_t *)(lives_malloc(sizeof(lives_clip_t)));
      cfile->cb_src=-1;
      lives_snprintf(cfile->handle,256,"%s",mainw->msg);
      cfile->clip_type=CLIP_TYPE_DISK; // the default

      // lock the set
#ifndef IS_MINGW
      tfile=lives_strdup_printf("%s/%s/lock.%d",prefs->tmpdir,set_name,capable->mainpid);
#else
      tfile=lives_strdup_printf("%s\\%s\\lock.%d",prefs->tmpdir,set_name,capable->mainpid);
#endif
      lives_touch(tfile);
      lives_free(tfile);
    }

    //create a new cfile and fill in the details
    create_cfile();
    threaded_dialog_spin(0.);

    // get file details
    read_headers(".");
    threaded_dialog_spin(0.);

    // if the clip has a frame_index file, then it is CLIP_TYPE_FILE
    // and we must load the frame_index and locate a suitable decoder plugin

    if (load_frame_index(mainw->current_file)) {
      // CLIP_TYPE_FILE
      if (!reload_clip(mainw->current_file)) continue;
    } else {
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
    open_set_file(set_name,++clipnum);
    threaded_dialog_spin(0.);

    if (needs_update) {
      save_clip_values(mainw->current_file);
      needs_update=FALSE;
    }

    if (mainw->cached_list!=NULL) {
      lives_list_free_strings(mainw->cached_list);
      lives_list_free(mainw->cached_list);
      mainw->cached_list=NULL;
    }

    if (prefs->autoload_subs) {
      subfname=lives_build_filename(prefs->tmpdir,cfile->handle,"subs.",LIVES_FILE_EXT_SRT,NULL);
      if (lives_file_test(subfname,LIVES_FILE_TEST_EXISTS)) {
        subtitles_init(cfile,subfname,SUBTITLE_TYPE_SRT);
      } else {
        lives_free(subfname);
        subfname=lives_build_filename(prefs->tmpdir,cfile->handle,"subs.",LIVES_FILE_EXT_SUB,NULL);
        if (lives_file_test(subfname,LIVES_FILE_TEST_EXISTS)) {
          subtitles_init(cfile,subfname,SUBTITLE_TYPE_SUB);
        }
      }
      lives_free(subfname);
      threaded_dialog_spin(0.);
    }

    get_total_time(cfile);
    if (cfile->achans) cfile->aseek_pos=(int64_t)((double)(cfile->frameno-1.)/
                                          cfile->fps*cfile->arate*cfile->achans*cfile->asampsize/8);

    // add to clip menu
    threaded_dialog_spin(0.);
    add_to_clipmenu();
    get_next_free_file();
    cfile->start=cfile->frames>0?1:0;
    cfile->end=cfile->frames;
    cfile->is_loaded=TRUE;
    cfile->changed=TRUE;
    lives_rm(cfile->info_file);
    set_main_title(cfile->name,0);

    if (mainw->multitrack==NULL) {
      if (mainw->current_file>0) {
        resize(1);
        load_start_image(cfile->start);
        load_end_image(cfile->end);
        lives_widget_context_update();
      }
    }

    if (mainw->multitrack!=NULL&&mainw->multitrack->is_ready) {
      mainw->current_file=mainw->multitrack->render_file;
      mt_init_clips(mainw->multitrack,new_file,TRUE);
      lives_widget_context_update();
      mt_clip_select(mainw->multitrack,TRUE);
    }

    lives_notify(LIVES_OSC_NOTIFY_CLIP_OPENED,"");
  }

  // should never reach here
  return TRUE;
}



void on_cleardisk_activate(LiVESWidget *widget, livespointer user_data) {
  // recover disk space

  int64_t bytes=0,fspace;
  int64_t ds_warn_level=mainw->next_ds_warn_level;

  char *markerfile;
  char **array;
  char *com;

  int current_file=mainw->current_file;
  int marker_fd;
  int retval=0;

  register int i;

  mainw->next_ds_warn_level=0; /// < avoid nested warnings

  if (user_data!=NULL) lives_widget_hide(lives_widget_get_toplevel(LIVES_WIDGET(user_data)));

  mainw->tried_ds_recover=TRUE; ///< indicates we tried ds recovery already

  mainw->add_clear_ds_adv=TRUE; ///< auto reset by do_warning_dialog()
  if (!do_warning_dialog(
        _("LiVES will attempt to recover some disk space.\nYou should ONLY run this if you have no other copies of LiVES running on this machine.\nClick OK to proceed.\n"))) {
    mainw->next_ds_warn_level=ds_warn_level;;
    return;
  }

  d_print(_("Cleaning up disk space..."));

  // get a temporary clip for receiving data from backend
  if (!get_temp_handle(mainw->first_free_file,TRUE)) {
    d_print_failed();
    mainw->next_ds_warn_level=ds_warn_level;
    return;
  }

  cfile->cb_src=current_file;

  if (mainw->multitrack!=NULL) {
    if (mainw->multitrack->idlefunc>0) {
      lives_source_remove(mainw->multitrack->idlefunc);
      mainw->multitrack->idlefunc=0;
    }
    mt_desensitise(mainw->multitrack);
  }


  for (i=0; i<MAX_FILES; i++) {
    // mark all free-floating files (directories) which we do not want to remove
    // we do error checking here

    if ((i==mainw->current_file)||(mainw->files[i]!=NULL&&(mainw->files[i]->clip_type==CLIP_TYPE_DISK||
                                   mainw->files[i]->clip_type==CLIP_TYPE_FILE))) {
      markerfile=lives_build_filename(prefs->tmpdir,mainw->files[i]->handle,"set.",NULL);

      do {
        retval=0;
        marker_fd=creat(markerfile,S_IRUSR|S_IWUSR);
        if (marker_fd<0) {
          retval=do_write_failed_error_s_with_retry(markerfile,lives_strerror(errno),NULL);
        }
      } while (retval==LIVES_RESPONSE_RETRY);

      close(marker_fd);
      lives_free(markerfile);
      if (mainw->files[i]->undo_action!=UNDO_NONE) {
        markerfile=lives_build_filename(prefs->tmpdir,mainw->files[i]->handle,"noprune",NULL);
        do {
          retval=0;
          marker_fd=creat(markerfile,S_IRUSR|S_IWUSR);
          if (marker_fd<0) {
            retval=do_write_failed_error_s_with_retry(markerfile,lives_strerror(errno),NULL);
          }
        } while (retval==LIVES_RESPONSE_RETRY);
        close(marker_fd);
        lives_free(markerfile);
      }
    }
  }


  // get space before
  fspace=get_fs_free(prefs->tmpdir);

  // call "smogrify bg_weed" to do the actual cleanup
  // its parameters are the handle of a temp file, and opts mask

  if (retval!=LIVES_RESPONSE_CANCEL) {
    mainw->com_failed=FALSE;
    lives_rm(cfile->info_file);
    com=lives_strdup_printf("%s bg_weed \"%s\" %d",prefs->backend,cfile->handle,prefs->clear_disk_opts);
    lives_system(com,FALSE);
    lives_free(com);

    if (!mainw->com_failed) {

      // show a progress dialog, not cancellable
      do_progress_dialog(TRUE,FALSE,_("Recovering disk space"));

      array=lives_strsplit(mainw->msg,"|",2);
      bytes=strtol(array[1],NULL,10);
      if (bytes<0) bytes=0;
      lives_strfreev(array);

    }
  }


  // close the temporary clip
  com=lives_strdup_printf("%s close \"%s\"",prefs->backend,cfile->handle);
  lives_system(com,FALSE);
  lives_free(com);
  lives_free(cfile);
  cfile=NULL;
  if (mainw->first_free_file==-1||mainw->first_free_file>mainw->current_file)
    mainw->first_free_file=mainw->current_file;

  // remove the protective markers
  for (i=0; i<MAX_FILES; i++) {
    if (mainw->files[i]!=NULL&&mainw->files[i]->clip_type==CLIP_TYPE_DISK) {
      markerfile=lives_build_filename(prefs->tmpdir,mainw->files[i]->handle,"set.",NULL);
      lives_rm(markerfile);
      lives_free(markerfile);
      if (mainw->files[i]->undo_action!=UNDO_NONE) {
        markerfile=lives_build_filename(prefs->tmpdir,mainw->files[i]->handle,"noprune",NULL);
        lives_rm(markerfile);
        lives_free(markerfile);
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

  if (retval!=LIVES_RESPONSE_CANCEL&&!mainw->com_failed) {
    d_print_done();
    do_info_dialog(lives_strdup_printf(_("%s of disk space was recovered.\n"),
                                       lives_format_storage_space_string((uint64_t)bytes)));
    if (user_data!=NULL) lives_widget_set_sensitive(lives_widget_get_toplevel(LIVES_WIDGET(user_data)),FALSE);
  } else d_print_failed();

  mainw->next_ds_warn_level=ds_warn_level;;

}


void on_cleardisk_advanced_clicked(LiVESWidget *widget, livespointer user_data) {
  // make cleardisk adv window

  // show various options and OK/Cancel button

  // on OK set clear_disk opts
  int response;
  LiVESWidget *dialog;
  do {
    dialog=create_cleardisk_advanced_dialog();
    lives_widget_show_all(dialog);
    response=lives_dialog_run(LIVES_DIALOG(dialog));
    lives_widget_destroy(dialog);
    if (response==LIVES_RESPONSE_RETRY) prefs->clear_disk_opts=0;
  } while (response==LIVES_RESPONSE_RETRY);

  set_int_pref("clear_disk_opts",prefs->clear_disk_opts);
}


void on_show_keys_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  do_keys_window();
}


void on_vj_reset_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  LiVESList *clip_list=mainw->cliplist;

  boolean bad_header=FALSE;

  int i;

  //mainw->soft_debug=TRUE;

  do_threaded_dialog(_("Resetting frame rates and frame values..."),FALSE);

  while (clip_list!=NULL) {
    i=LIVES_POINTER_TO_INT(clip_list->data);
    mainw->files[i]->pb_fps=mainw->files[i]->fps;
    mainw->files[i]->frameno=1;
    mainw->files[i]->aseek_pos=0;

    save_clip_value(i,CLIP_DETAILS_PB_FPS,&mainw->files[i]->pb_fps);
    if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
    save_clip_value(i,CLIP_DETAILS_PB_FRAMENO,&mainw->files[i]->frameno);
    if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
    threaded_dialog_spin((double)i/(double)mainw->clips_available);

    if (bad_header) {
      if (!do_header_write_error(i)) break;
    } else clip_list=clip_list->next;
  }


  mainw->noswitch=FALSE; // just in case...

  end_threaded_dialog();

}


void on_show_messages_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  do_messages_window();
}


void on_show_file_info_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  char buff[512];
  lives_clipinfo_t *filew;

  char *sigs,*ends,*tmp;

  if (mainw->current_file==-1) return;

  filew = create_clip_info_window(cfile->achans,FALSE);

  if (cfile->frames>0) {
    // type
    lives_snprintf(buff,512,_("\nExternal: %s\nInternal: %s (%d bpp) / %s\n"),cfile->type,
                   (tmp=lives_strdup((cfile->clip_type==CLIP_TYPE_YUV4MPEG||
                                      cfile->clip_type==CLIP_TYPE_VIDEODEV)?(_("buffered")):
                                     (cfile->img_type==IMG_TYPE_JPEG?"jpeg":"png"))),cfile->bpp,"pcm");
    lives_free(tmp);
    lives_text_view_set_text(LIVES_TEXT_VIEW(filew->textview_type),buff, -1);
    // fps
    lives_snprintf(buff,512,"\n  %.3f%s",cfile->fps,cfile->ratio_fps?"...":"");

    lives_text_view_set_text(LIVES_TEXT_VIEW(filew->textview_fps),buff, -1);
    // image size
    lives_snprintf(buff,512,"\n  %dx%d",cfile->hsize,cfile->vsize);
    lives_text_view_set_text(LIVES_TEXT_VIEW(filew->textview_size),buff, -1);
    // frames
    if ((cfile->opening&&!cfile->opening_audio&&cfile->frames==0)||cfile->frames==123456789) {
      lives_snprintf(buff,512,"%s",_("\n  Opening..."));
    } else {
      lives_snprintf(buff,512,"\n  %d",cfile->frames);

      if (cfile->frame_index!=NULL) {
        int fvirt=count_virtual_frames(cfile->frame_index,1,cfile->frames);
        char *tmp=lives_strdup_printf(_("\n(%d virtual)"),fvirt);
        lives_strappend(buff,512,tmp);
        lives_free(tmp);
        tmp=lives_strdup_printf(_("\n(%d decoded)"),cfile->frames-fvirt);
        lives_strappend(buff,512,tmp);
        lives_free(tmp);
      }

    }
    lives_text_view_set_text(LIVES_TEXT_VIEW(filew->textview_frames),buff, -1);
    // video time
    if ((cfile->opening&&!cfile->opening_audio&&cfile->frames==0)||cfile->frames==123456789) {
      lives_snprintf(buff,512,"%s",_("\n  Opening..."));
    } else {
      lives_snprintf(buff,512,_("\n  %.2f sec."),cfile->video_time);
    }
    lives_text_view_set_text(LIVES_TEXT_VIEW(filew->textview_vtime),buff, -1);
    // file size
    if (cfile->f_size>=0l) {
      char *file_ds=lives_format_storage_space_string((uint64_t)cfile->f_size);
      lives_snprintf(buff,512,"\n  %s",file_ds);
      lives_free(file_ds);
    } else lives_snprintf(buff,512,"%s",_("\n  Unknown"));
    lives_text_view_set_text(LIVES_TEXT_VIEW(filew->textview_fsize),buff, -1);
  }

  if (cfile->achans>0) {
    if (cfile->opening) {
      lives_snprintf(buff,512,"%s",_("\n  Opening..."));
    } else {
      lives_snprintf(buff,512,_("\n  %.2f sec."),cfile->laudio_time);
    }
    lives_text_view_set_text(LIVES_TEXT_VIEW(filew->textview_ltime),buff, -1);

    if (cfile->signed_endian&AFORM_UNSIGNED) sigs=lives_strdup(_("unsigned"));
    else sigs=lives_strdup(_("signed"));

    if (cfile->signed_endian&AFORM_BIG_ENDIAN) ends=lives_strdup(_("big-endian"));
    else ends=lives_strdup(_("little-endian"));

    lives_snprintf(buff,512,_("  %d Hz %d bit\n%s %s"),cfile->arate,cfile->asampsize,sigs,ends);
    lives_text_view_set_text(LIVES_TEXT_VIEW(filew->textview_lrate),buff, -1);

    lives_free(sigs);
    lives_free(ends);
  }

  if (cfile->achans>1) {
    if (cfile->signed_endian&AFORM_UNSIGNED) sigs=lives_strdup(_("unsigned"));
    else sigs=lives_strdup(_("signed"));

    if (cfile->signed_endian&AFORM_BIG_ENDIAN) ends=lives_strdup(_("big-endian"));
    else ends=lives_strdup(_("little-endian"));

    lives_snprintf(buff,512,_("  %d Hz %d bit\n%s %s"),cfile->arate,cfile->asampsize,sigs,ends);
    lives_text_view_set_text(LIVES_TEXT_VIEW(filew->textview_rrate),buff, -1);

    lives_free(sigs);
    lives_free(ends);

    if (cfile->opening) {
      lives_snprintf(buff,512,"%s",_("\n  Opening..."));
    } else {
      lives_snprintf(buff,512,_("\n  %.2f sec."),cfile->raudio_time);
    }
    lives_text_view_set_text(LIVES_TEXT_VIEW(filew->textview_rtime),buff, -1);
  }

}


void on_show_file_comments_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  do_comments_dialog(mainw->current_file,NULL);
}



void on_show_clipboard_info_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  int current_file=mainw->current_file;
  mainw->current_file=0;
  on_show_file_info_activate(menuitem,user_data);
  mainw->current_file=current_file;
}


void switch_clip(int type, int newclip, boolean force) {
  // generic switch clip callback

  // This is the new single entry function for switching clips.
  // It should eventually replace switch_to_file() and do_quick_switch()

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
        weed_generator_end((weed_plant_t *)mainw->files[mainw->blend_file]->ext_src);
        mainw->osc_block=FALSE;
      }
      mainw->blend_file=newclip;
      mainw->whentostop=NEVER_STOP;
      if (mainw->ce_thumbs&&mainw->active_sa_clips==SCREEN_AREA_BACKGROUND) ce_thumbs_highlight_current_clip();
    }
    return;
  }

  // switch fg clip

  if (!force&&(newclip==mainw->current_file&&(mainw->playing_file==-1||mainw->playing_file==newclip))) return;
  if (!cfile->is_loaded) mainw->cancelled=CANCEL_NO_PROPOGATE;

  if (mainw->playing_file>-1) {
    mainw->pre_src_file=newclip;
    mainw->new_clip=newclip;
  } else {
    if (force&&newclip==mainw->current_file) mainw->current_file=0;
    switch_to_file(mainw->current_file,newclip);
  }
}


void switch_clip_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  // switch clips from the clips menu

  register int i;
  if (mainw->current_file<1||mainw->preview||(mainw->is_processing&&cfile->is_loaded)||mainw->cliplist==NULL) return;

  for (i=1; i<MAX_FILES; i++) {
    if (mainw->files[i]!=NULL) {
      if (LIVES_MENU_ITEM(menuitem)==LIVES_MENU_ITEM(mainw->files[i]->menuentry)&&
          lives_check_menu_item_get_active(LIVES_CHECK_MENU_ITEM(mainw->files[i]->menuentry))) {
        switch_clip(0,i,FALSE);
        return;
      }
    }
  }
}



void on_about_activate(LiVESMenuItem *menuitem, livespointer user_data) {

#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2,14,0)
  char *license = lives_strdup(_(
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

  char *comments= lives_strdup(_("A video editor and VJ program."));
  char *title= lives_strdup(_("About LiVES"));

  char *translator_credits = lives_strdup(_("translator_credits"));

#if GTK_CHECK_VERSION(3,0,0)
  char *authors[2]= {"salsaman@gmail.com",NULL};
#else
  gtk_about_dialog_set_url_hook(activate_url, NULL, NULL);
  gtk_about_dialog_set_email_hook(activate_url, NULL, NULL);
#endif

  gtk_show_about_dialog(LIVES_WINDOW(mainw->LiVES),
                        "logo", NULL,
                        "name", "LiVES",
                        "version", LiVES_VERSION,
                        "comments",comments,
                        "copyright", "(C) 2002-2016 salsaman <salsaman@gmail.com> and others",
                        "website", "http://lives.sourceforge.net",
                        "license", license,
                        "title", title,
                        "translator_credits", translator_credits,
#if GTK_CHECK_VERSION(3,0,0)
                        "authors", authors,
                        "license-type", GTK_LICENSE_GPL_3_0,
#endif
                        NULL);

  lives_free(translator_credits);
  lives_free(comments);
  lives_free(title);
  lives_free(license);
  return;
#endif
#endif

  char *mesg;
  mesg=lives_strdup_printf(
         _("LiVES Version %s\n(c) G. Finch (salsaman) %s\n\nReleased under the GPL 3 or later (http://www.gnu.org/licenses/gpl.txt)\nLiVES is distributed WITHOUT WARRANTY\n\nContact the author at:\nsalsaman@gmail.com\nHomepage: http://lives.sourceforge.net"),
         LiVES_VERSION,"2002-2016");
  do_error_dialog(mesg);
  lives_free(mesg);

}



void show_manual_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  show_manual_section(NULL,NULL);
}



void email_author_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  activate_url_inner(LIVES_AUTHOR_EMAIL);
}


void report_bug_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  activate_url_inner(LIVES_BUG_URL);
}


void suggest_feature_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  activate_url_inner(LIVES_FEATURE_URL);
}


void help_translate_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  activate_url_inner(LIVES_TRANSLATE_URL);
}



void donate_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  const char *link=lives_strdup_printf("%s%s",LIVES_DONATE_URL,user_data==NULL?"":(char *)user_data);
  activate_url_inner(link);
}





#if GTK_CHECK_VERSION(3,0,0)
boolean expose_fsplayarea_event(LiVESWidget *widget, lives_painter_t *cr, livespointer user_data) {
  //LiVESXEventExpose *event=NULL;
#else
boolean expose_fsplayarea_event(LiVESWidget *widget, LiVESXEventExpose *event) {
  lives_painter_t *cr=NULL;
#endif

  boolean dest_cr=FALSE;

  LiVESPixbuf *pixbuf=(LiVESPixbuf *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(widget), "pixbuf");

  int fwidth, fheight, owidth, oheight, width, height;
  int offs_x,offs_y;

  if (cr==NULL) {
    cr=lives_painter_create_from_widget(widget);
    dest_cr=TRUE;
  }

  fwidth=lives_widget_get_allocation_width(widget);
  fheight=lives_widget_get_allocation_height(widget);

  owidth=width=lives_pixbuf_get_width(pixbuf);
  oheight=height=lives_pixbuf_get_height(pixbuf);

  calc_maxspect(fwidth,fheight,&width,&height);

  width=(width>>1)<<1;
  height=(height>>1)<<1;

  if (width>owidth && height>oheight) {
    width=owidth;
    height=oheight;
  }

  lives_painter_set_source_rgb(cr,0.,0.,0.);
  lives_painter_rectangle(cr,0.,0.,fwidth,fheight);

  lives_painter_fill(cr);
  lives_painter_paint(cr);

  offs_x=(fwidth-width)/2;
  offs_y=(fheight-height)/2;

  lives_painter_set_source_pixbuf(cr, pixbuf, offs_x, offs_y);
  lives_painter_fill(cr);
  lives_painter_paint(cr);

  if (dest_cr) lives_painter_destroy(cr);

  return TRUE;
}






void on_fs_preview_clicked(LiVESWidget *widget, livespointer user_data) {
  // file selector preview
  double start_time=0.;

  uint64_t xwin=0;

  FILE *ifile=NULL;

  lives_painter_t *cr;

  char **array;

  int preview_frames=1000000000;
  int preview_type=LIVES_POINTER_TO_INT(user_data);

  int height=0,width=0;
  int fwidth,fheight,owidth,oheight;

  pid_t pid=capable->mainpid;
  int alarm_handle;
  int retval;
  boolean timeout;

  char *info_file,*thm_dir;
  char *file_open_params=NULL;
  char *tmp,*tmp2;
  char *com;
  char *dfile;

  end_fs_preview();

  if (mainw->in_fs_preview) {

#ifndef IS_MINGW
    com=lives_strdup_printf("%s stopsubsub thm%d 2>/dev/null",prefs->backend_sync,pid);
    lives_system(com,TRUE);
#else
    // get pid from backend
    FILE *rfile;
    ssize_t rlen;
    char val[16];
    int xpid;
    com=lives_strdup_printf("%s get_pid_for_handle thm%d",prefs->backend_sync,pid);
    rfile=popen(com,"r");
    rlen=fread(val,1,16,rfile);
    pclose(rfile);
    memset(val+rlen,0,1);
    xpid=atoi(val);
    if (xpid!=0)
      lives_win32_kill_subprocesses(xpid,TRUE);
#endif
    lives_free(com);

    lives_widget_context_update();
  }

  if (preview_type==LIVES_PREVIEW_TYPE_RANGE) {
    // open selection
    start_time=mainw->fx1_val;
    preview_frames=(int)mainw->fx2_val;
  } else {
    // open file
    lives_snprintf(file_name,PATH_MAX,"%s",
                   (tmp=lives_filename_to_utf8((tmp2=lives_file_chooser_get_filename(LIVES_FILE_CHOOSER(lives_widget_get_toplevel(widget)))),
                        -1,NULL,NULL,NULL)));
    lives_free(tmp);
    lives_free(tmp2);

  }


#ifndef IS_MINGW
  info_file=lives_strdup_printf("%s/thm%d/.status",prefs->tmpdir,capable->mainpid);
#else
  info_file=lives_strdup_printf("%s/thm%d/status",prefs->tmpdir,capable->mainpid);
#endif
  lives_rm(info_file);

  if (preview_type==LIVES_PREVIEW_TYPE_VIDEO_AUDIO||preview_type==LIVES_PREVIEW_TYPE_IMAGE_ONLY) {

    preview_frames=1000000000;

    clear_mainw_msg();

    if (capable->has_identify) {
      mainw->error=FALSE;

      // make thumb from any image file
      com=lives_strdup_printf("%s make_thumb thm%d %d %d \"%s\" \"%s\"",prefs->backend_sync,pid,DEFAULT_FRAME_HSIZE,
                              DEFAULT_FRAME_VSIZE,prefs->image_ext,(tmp=lives_filename_from_utf8(file_name,-1,NULL,NULL,NULL)));
      lives_free(tmp);
      lives_system(com,FALSE);
      lives_free(com);

      do {
        retval=0;
        timeout=FALSE;
        alarm_handle=lives_alarm_set(LIVES_DEFAULT_TIMEOUT);

        while (!((ifile=fopen(info_file,"r")) || (timeout=lives_alarm_get(alarm_handle)))) {
          lives_widget_context_update();
          lives_usleep(prefs->sleep_time);
        }

        lives_alarm_clear(alarm_handle);

        if (!timeout) {
          mainw->read_failed=FALSE;
          lives_fgets(mainw->msg,512,ifile);
          fclose(ifile);
        }

        if (timeout || mainw->read_failed) {
          retval=do_read_failed_error_s_with_retry(info_file,NULL,NULL);
        }

      } while (retval==LIVES_RESPONSE_RETRY);

      if (retval==LIVES_RESPONSE_CANCEL) {
        mainw->read_failed=FALSE;
        return;
      }

      if (!mainw->error) {
        array=lives_strsplit(mainw->msg,"|",3);
        width=atoi(array[1]);
        height=atoi(array[2]);
        lives_strfreev(array);
      } else height=width=0;
      mainw->error=FALSE;
    } else {
      height=width=0;
    }

    if (height*width) {
      // draw image
      LiVESError *error=NULL;
      char *thumb=lives_strdup_printf("%s/thm%d/%08d.%s",prefs->tmpdir,pid,1,prefs->image_ext);
      LiVESPixbuf *pixbuf=lives_pixbuf_new_from_file((tmp=lives_filename_from_utf8(thumb,-1,NULL,NULL,NULL)),&error);
      lives_free(thumb);
      lives_free(tmp);

      if (error==NULL) {
        lives_widget_object_set_data(LIVES_WIDGET_OBJECT(mainw->fs_playarea),"pixbuf",pixbuf);
#if GTK_CHECK_VERSION(3,0,0)
        expose_fsplayarea_event(mainw->fs_playarea,NULL,NULL);
#else
        expose_fsplayarea_event(mainw->fs_playarea,NULL);
#endif
        lives_signal_connect(LIVES_GUI_OBJECT(mainw->fs_playarea), LIVES_WIDGET_EXPOSE_EVENT,
                             LIVES_GUI_CALLBACK(expose_fsplayarea_event),
                             NULL);
      } else {
        lives_error_free(error);
      }

    }
    lives_free(info_file);

    thm_dir=lives_strdup_printf("%s/thm%d",prefs->tmpdir,capable->mainpid);
    lives_rmdir(thm_dir,TRUE);
    lives_free(thm_dir);
  }


  if (!(height*width)&&preview_type!=LIVES_PREVIEW_TYPE_IMAGE_ONLY) {
    // media preview

    if (!capable->has_mplayer&&!(capable->has_mplayer2||capable->has_mpv)) {
      char *msg;
      if (capable->has_identify) {
        msg=lives_strdup(_("\n\nYou need to install mplayer or mplayer2 to be able to preview this file.\n"));
      } else {
        msg=lives_strdup(_("\n\nYou need to install mplayer, mplayer2 or imageMagick to be able to preview this file.\n"));
      }
      do_blocking_error_dialog(msg);
      lives_free(msg);
      return;
    }

#ifndef IS_MINGW
    dfile=lives_strdup_printf("%s/fsp%d/",prefs->tmpdir,capable->mainpid);
#else
    dfile=lives_strdup_printf("%s\\fsp%d\\",prefs->tmpdir,capable->mainpid);
#endif

    lives_mkdir_with_parents(dfile,S_IRWXU);

#ifndef IS_MINGW
    info_file=lives_strdup_printf("%s.status",dfile);
#else
    info_file=lives_strdup_printf("%sstatus",dfile);
#endif

    lives_free(dfile);

    if (preview_type!=LIVES_PREVIEW_TYPE_AUDIO_ONLY) {
      lives_widget_set_bg_color(mainw->fs_playarea, LIVES_WIDGET_STATE_NORMAL, &palette->black);
      lives_widget_set_bg_color(mainw->fs_playframe, LIVES_WIDGET_STATE_NORMAL, &palette->black);
      lives_widget_set_bg_color(mainw->fs_playalign, LIVES_WIDGET_STATE_NORMAL, &palette->black);
    }

    mainw->in_fs_preview=TRUE;

    // get width and height of clip
    com=lives_strdup_printf("%s get_details fsp%d \"%s\" \"%s\" %d %d",prefs->backend,capable->mainpid,
                            (tmp=lives_filename_from_utf8(file_name,-1,NULL,NULL,NULL)),
                            prefs->image_ext,FALSE,FALSE);


    mainw->com_failed=FALSE;
    lives_system(com,FALSE);
    lives_free(com);

    if (mainw->com_failed) {
      end_fs_preview();
      lives_free(info_file);
      return;
    }


    do {
      retval=0;
      timeout=FALSE;
      clear_mainw_msg();

      alarm_handle=lives_alarm_set(LIVES_DEFAULT_TIMEOUT);

      while (!((ifile=fopen(info_file,"r")) || (timeout=lives_alarm_get(alarm_handle)))
             &&mainw->in_fs_preview) {
        lives_widget_context_update();
        threaded_dialog_spin(0.);
        lives_usleep(prefs->sleep_time);
      }

      lives_alarm_clear(alarm_handle);

      if (!mainw->in_fs_preview) {
        // user cancelled
        end_fs_preview();
        lives_free(info_file);
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
    } while (retval==LIVES_RESPONSE_RETRY);

    if (mainw->msg!=NULL&&get_token_count(mainw->msg,'|')>6) {
      array=lives_strsplit(mainw->msg,"|",-1);
      width=atoi(array[4]);
      height=atoi(array[5]);
      lives_strfreev(array);
    } else {
      width=DEFAULT_FRAME_HSIZE;
      height=DEFAULT_FRAME_VSIZE;
    }

    lives_rm(info_file);

    if (preview_type!=LIVES_PREVIEW_TYPE_AUDIO_ONLY) {

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

      cr = lives_painter_create_from_widget(mainw->fs_playarea);

      lives_painter_set_source_rgb(cr,0.,0.,0.);
      lives_painter_rectangle(cr,0,0,fwidth+20,fheight+20);

      lives_painter_fill(cr);
      lives_painter_paint(cr);
      lives_painter_destroy(cr);

      lives_widget_context_update();
    }

    if (prefs->audio_player==AUD_PLAYER_JACK) {
      file_open_params=lives_strdup_printf("%s %s -ao jack",mainw->file_open_params!=NULL?
                                           mainw->file_open_params:"",get_deinterlace_string());
    } else if (prefs->audio_player==AUD_PLAYER_PULSE) {
      file_open_params=lives_strdup_printf("%s %s -ao pulse",mainw->file_open_params!=NULL?
                                           mainw->file_open_params:"",get_deinterlace_string());
    } else {
      file_open_params=lives_strdup_printf("%s %s -ao null",mainw->file_open_params!=NULL?
                                           mainw->file_open_params:"",get_deinterlace_string());
    }


    if (preview_type==LIVES_PREVIEW_TYPE_VIDEO_AUDIO||preview_type==LIVES_PREVIEW_TYPE_RANGE) {
      xwin=lives_widget_get_xwinid(mainw->fs_playarea,"Unsupported display type for preview.");
      if (xwin==-1) {
        end_fs_preview();
        lives_free(info_file);
        return;
      }
    }


    if (file_open_params!=NULL) {
      com=lives_strdup_printf("%s fs_preview fsp%d %"PRIu64" %d %d %.2f %d \"%s\" \"%s\"",prefs->backend,capable->mainpid,
                              xwin, width, height, start_time, preview_frames,
                              (tmp=lives_filename_from_utf8(file_name,-1,NULL,NULL,NULL)),file_open_params);

    } else {
      com=lives_strdup_printf("%s fs_preview fsp%d %"PRIu64" %d %d %.2f %d \"%s\"",prefs->backend,capable->mainpid,
                              xwin, width, height, start_time, preview_frames,
                              (tmp=lives_filename_from_utf8(file_name,-1,NULL,NULL,NULL)));
    }

    lives_free(tmp);

    if (preview_type!=LIVES_PREVIEW_TYPE_AUDIO_ONLY) lives_widget_set_app_paintable(mainw->fs_playarea,TRUE);

    mainw->com_failed=FALSE;

    mainw->in_fs_preview=TRUE;
    lives_system(com,FALSE);
    lives_free(com);

    if (mainw->com_failed) {
      end_fs_preview();
      lives_free(info_file);
      return;
    }

    // loop here until preview has finished, or the user presses OK or Cancel

    while ((!(ifile=fopen(info_file,"r")))&&mainw->in_fs_preview&&mainw->fc_buttonresponse==LIVES_RESPONSE_NONE) {
      lives_widget_context_update();
      lives_usleep(prefs->sleep_time);
    }

    if (ifile!=NULL) {
      fclose(ifile);
    }

    end_fs_preview();
    lives_free(info_file);
  }

  if (file_open_params!=NULL) lives_free(file_open_params);
}




void on_open_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  // OPEN A FILE (single or multiple)
  LiVESWidget *chooser;
  int resp;

  if (mainw->multitrack!=NULL) {
    if (mainw->multitrack->idlefunc>0) {
      lives_source_remove(mainw->multitrack->idlefunc);
      mainw->multitrack->idlefunc=0;
    }
    mt_desensitise(mainw->multitrack);
    lives_widget_set_sensitive(mainw->multitrack->playall,TRUE);
    lives_widget_set_sensitive(mainw->m_playbutton, TRUE);
  }

  chooser=choose_file_with_preview(strlen(mainw->vid_load_dir)?mainw->vid_load_dir:NULL,NULL,NULL,LIVES_FILE_SELECTION_VIDEO_AUDIO_MULTI);

  resp=lives_dialog_run(LIVES_DIALOG(chooser));

  end_fs_preview();
  mainw->fs_playarea=NULL;


  if (resp==LIVES_RESPONSE_ACCEPT) on_ok_file_open_clicked(LIVES_FILE_CHOOSER(chooser),NULL);
  else on_filechooser_cancel_clicked(chooser);
}




void on_ok_file_open_clicked(LiVESFileChooser *chooser, LiVESSList *fnames) {
  // this is also called from drag target

  LiVESSList *ofnames;
  char *tmp;

  if (chooser!=NULL) {
    fnames=lives_file_chooser_get_filenames(chooser);

    lives_widget_destroy(LIVES_WIDGET(chooser));

    if (fnames==NULL) return;

    if (fnames->data==NULL) {
      lives_list_free_strings((LiVESList *)fnames);
      lives_slist_free(fnames);
      return;
    }

    lives_snprintf(mainw->vid_load_dir,PATH_MAX,"%s",(char *)fnames->data);
    get_dirname(mainw->vid_load_dir);

    if (mainw->multitrack==NULL) lives_widget_queue_draw(mainw->LiVES);
    else lives_widget_queue_draw(mainw->multitrack->window);
    lives_widget_context_update();

    if (prefs->save_directories) {
      set_pref("vid_load_dir",(tmp=lives_filename_from_utf8(mainw->vid_load_dir,-1,NULL,NULL,NULL)));
      lives_free(tmp);
    }

    mainw->cancelled=CANCEL_NONE;
  }

  ofnames=fnames;

  while (fnames!=NULL&&mainw->cancelled==CANCEL_NONE) {
    lives_snprintf(file_name,PATH_MAX,"%s",(char *)fnames->data);
    lives_free((livespointer)fnames->data);
    open_file(file_name);
    fnames=fnames->next;
  }

  lives_slist_free(ofnames);

  mainw->opening_multi=FALSE;
  mainw->img_concat_clip=-1;

  if (mainw->multitrack!=NULL) {
    polymorph(mainw->multitrack,POLY_NONE);
    polymorph(mainw->multitrack,POLY_CLIPS);
    mt_sensitise(mainw->multitrack);
    mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
  }

}



#ifdef GUI_GTK
// TODO
// files dragged onto target from outside - try to open them
void drag_from_outside(LiVESWidget *widget, GdkDragContext *dcon, int x, int y,
                       GtkSelectionData *data, uint32_t info, uint32_t time, livespointer user_data) {
  GSList *fnames=NULL;
#if GTK_CHECK_VERSION(3,0,0)
  char *filelist=(char *)gtk_selection_data_get_data(data);
#else
  char *filelist=(char *)data->data;
#endif
  char *nfilelist,**array;
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
  } else {
    if (!lives_widget_is_sensitive(mainw->open)) {
      gtk_drag_finish(dcon,FALSE,FALSE,time);
      return;
    }
  }

  nfilelist=subst(filelist,"file://","");

  nfiles=get_token_count(nfilelist,'\n');
  array=lives_strsplit(nfilelist,"\n",nfiles);
  lives_free(nfilelist);

  for (i=0; i<nfiles; i++) {
    fnames=lives_slist_append(fnames,array[i]);
  }

  on_ok_file_open_clicked(NULL,fnames);

  // fn will free array elements and fnames

  lives_free(array);

  gtk_drag_finish(dcon,TRUE,FALSE,time);
}
#endif



void on_opensel_range_ok_clicked(LiVESButton *button, livespointer user_data) {
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

  LiVESWidget *opensel_dialog = create_opensel_dialog();
  lives_widget_show_all(opensel_dialog);
  mainw->fx1_val=0.;
  mainw->fx2_val=1000;

}




void end_fs_preview(void) {
  // clean up if we were playing a preview - should be called from all callbacks
  // where there is a possibility of fs preview still playing
  char *com;

  LiVESPixbuf *pixbuf=NULL;

  if (mainw->fs_playarea!=NULL) {
    pixbuf=(LiVESPixbuf *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(mainw->fs_playarea), "pixbuf");
    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(mainw->fs_playarea),"pixbuf",NULL);
    lives_widget_set_app_paintable(mainw->fs_playarea,FALSE);
  }

  if (pixbuf!=NULL) lives_object_unref(pixbuf);

  if (mainw->in_fs_preview) {
#ifndef IS_MINGW
    com=lives_strdup_printf("%s stopsubsub fsp%d 2>/dev/null",prefs->backend_sync,capable->mainpid);
    lives_system(com,TRUE);
#else
    // get pid from backend
    FILE *rfile;
    ssize_t rlen;
    char val[16];
    int pid;
    com=lives_strdup_printf("%s get_pid_for_handle fsp%d",prefs->backend_sync,capable->mainpid);
    rfile=popen(com,"r");
    rlen=fread(val,1,16,rfile);
    pclose(rfile);
    memset(val+rlen,0,1);
    pid=atoi(val);

    lives_win32_kill_subprocesses(pid,TRUE);
#endif
    lives_free(com);
    com=lives_strdup_printf("%s close fsp%d",prefs->backend,capable->mainpid);
    lives_system(com,TRUE);
    lives_free(com);

    mainw->in_fs_preview=FALSE;

  }
}


void on_save_textview_clicked(LiVESButton *button, livespointer user_data) {
  LiVESTextView *textview=(LiVESTextView *)user_data;
  char *filt[]= {"*.txt",NULL};
  int fd;
  char *btext;
  char *save_file;

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
    do_write_failed_error_s(save_file,lives_strerror(errno));
    lives_free(save_file);
    return;
  }

  btext=lives_text_view_get_text(textview);

  lives_general_button_clicked(button,NULL);

  mainw->write_failed=FALSE;
  lives_write(fd,btext,strlen(btext),FALSE);
  lives_free(btext);

  close(fd);

  if (mainw->write_failed) {
    do_write_failed_error_s(save_file,lives_strerror(errno));
  } else {
    char *msg=lives_strdup_printf(_("Text was saved as\n%s\n"),save_file);
    do_blocking_error_dialog(msg);
    lives_free(msg);
  }

  lives_free(save_file);

}




void on_filechooser_cancel_clicked(LiVESWidget *widget) {

  lives_widget_destroy(widget);
  mainw->fs_playarea=NULL;

  if (mainw->multitrack!=NULL) {
    mt_sensitise(mainw->multitrack);
    mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
  } else if (mainw->current_file>0 && !cfile->opening) {
    get_play_times();
  }
}




void on_cancel_opensel_clicked(LiVESButton  *button, livespointer user_data) {
  end_fs_preview();
  lives_general_button_clicked(button,NULL);
  mainw->fs_playarea=NULL;

  if (mainw->multitrack!=NULL) {
    mt_sensitise(mainw->multitrack);
    mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
  }

}



void on_cancel_keep_button_clicked(LiVESButton *button, livespointer user_data) {
  // Cancel/Keep from progress dialog
  char *com=NULL;
  uint32_t keep_frames=0;
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

  if ((!mainw->effects_paused||cfile->nokeep)&&(mainw->multitrack==NULL||
      (mainw->multitrack!=NULL&&(!mainw->multitrack->is_rendering||
                                 !mainw->preview)))) {
    // Cancel
    if (mainw->cancel_type==CANCEL_SOFT) {
      // cancel in record audio
      mainw->cancelled=CANCEL_USER;
      d_print_cancelled();
      return;
    } else if (mainw->cancel_type==CANCEL_KILL) {
      // kill processes and subprocesses working on cfile
#ifndef IS_MINGW
      com=lives_strdup_printf("%s stopsubsub \"%s\" 2>/dev/null",prefs->backend_sync,cfile->handle);
#else
      com=lives_strdup_printf("%s get_pid_for_handle \"%s\"",prefs->backend_sync,cfile->handle);
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
        lives_free(com);
        com=NULL;
        pid=atoi(val);

        lives_win32_kill_subprocesses(pid,TRUE);
#endif
      }

      // resume for next time
      if (mainw->effects_paused) {
        if (com!=NULL) lives_free(com);
        com=lives_strdup_printf("%s resume \"%s\"",prefs->backend_sync,cfile->handle);
        lives_system(com,FALSE);
      }

    }

    mainw->cancelled=CANCEL_USER;

    if (mainw->is_rendering) {
      cfile->frames=0;
      d_print_cancelled();
    } else {
      // see if there was a message from backend

      if (mainw->cancel_type!=CANCEL_SOFT) {
        if ((infofile=fopen(cfile->info_file,"r"))>0) {
          mainw->read_failed=FALSE;
          lives_fgets(mainw->msg,512,infofile);
          fclose(infofile);
        }

        if (strncmp(mainw->msg,"completed",9)) {
          d_print_cancelled();
        } else {
          // processing finished before we could cancel
          mainw->cancelled=CANCEL_NONE;
        }
      } else d_print_cancelled();
    }
  } else {
    // Keep
    if (mainw->cancel_type==CANCEL_SOFT) {
      mainw->cancelled=CANCEL_KEEP;
      return;
    }
    if (!mainw->is_rendering) {
      keep_frames=cfile->proc_ptr->frames_done-cfile->progress_start+cfile->start-1+mainw->internal_messaging*2;
      if (mainw->internal_messaging && atoi(mainw->msg)>cfile->proc_ptr->frames_done)
        keep_frames=atoi(mainw->msg)-cfile->progress_start+cfile->start-1+2;
    } else keep_frames=cfile->frames+1;
    if (keep_frames>mainw->internal_messaging) {
      d_print(P_("%d frame is enough !\n","%d frames are enough !\n",keep_frames-cfile->start),
              keep_frames-cfile->start);

      lives_set_cursor_style(LIVES_CURSOR_BUSY,NULL);
      if (!mainw->internal_messaging) {
#ifndef IS_MINGW
        com=lives_strdup_printf("%s stopsubsub \"%s\" 2>/dev/null",prefs->backend_sync,cfile->handle);
        lives_system(com,TRUE);
#else
        // get pid from backend
        FILE *rfile;
        ssize_t rlen;
        char val[16];
        int pid;
        com=lives_strdup_printf("%s get_pid_for_handle \"%s\"",prefs->backend_sync,cfile->handle);
        rfile=popen(com,"r");
        rlen=fread(val,1,16,rfile);
        pclose(rfile);
        memset(val+rlen,0,1);
        pid=atoi(val);

        lives_win32_kill_subprocesses(pid,TRUE);
#endif
        lives_free(com);

        com=lives_strdup_printf("%s resume \"%s\"",prefs->backend_sync,cfile->handle);
        lives_system(com,FALSE);
        lives_free(com);
        if (!mainw->keep_pre) com=lives_strdup_printf("%s mv_mgk \"%s\" %d %d \"%s\"",prefs->backend,cfile->handle,
                                    cfile->start,keep_frames-1,get_image_ext_for_type(cfile->img_type));
        else {
          com=lives_strdup_printf("%s mv_pre \"%s\" %d %d \"%s\" &",prefs->backend_sync,cfile->handle,
                                  cfile->start,keep_frames-1,get_image_ext_for_type(cfile->img_type));
          mainw->keep_pre=FALSE;
        }
      } else {
        mainw->internal_messaging=FALSE;
        if (!mainw->keep_pre) com=lives_strdup_printf("%s mv_mgk \"%s\" %d %d \"%s\"",prefs->backend,cfile->handle,
                                    cfile->start,keep_frames-1,get_image_ext_for_type(cfile->img_type));
        else {
          com=lives_strdup_printf("%s mv_pre \"%s\" %d %d \"%s\" &",prefs->backend_sync,cfile->handle,
                                  cfile->start,keep_frames-1,get_image_ext_for_type(cfile->img_type));
          mainw->keep_pre=FALSE;
        }
      }
      if (!mainw->is_rendering||mainw->multitrack!=NULL) {
        lives_rm(cfile->info_file);
        lives_system(com,FALSE);
        cfile->undo_end=keep_frames-1;
      } else mainw->cancelled=CANCEL_KEEP;
      lives_set_cursor_style(LIVES_CURSOR_NORMAL,NULL);
    } else {
      // no frames there, nothing to keep
      d_print_cancelled();

#ifndef IS_MINGW
      com=lives_strdup_printf("%s stopsubsub \"%s\" 2>/dev/null",prefs->backend_sync,cfile->handle);
#else
      com=lives_strdup_printf("%s get_pid_for_handle \"%s\"",prefs->backend_sync,cfile->handle);
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
        lives_free(com);

        com=lives_strdup_printf("%s resume \"%s\"",prefs->backend_sync,cfile->handle);
        lives_system(com,FALSE);
      }
      mainw->cancelled=CANCEL_USER;
    }
  }

  if (com!=NULL) lives_free(com);
}




void on_details_button_clicked(void) {
  text_window *textwindow;
  textwindow=create_text_window(_("Encoder Debug Output"),lives_text_view_get_text(mainw->optextview),NULL);
  lives_widget_show_all(textwindow->dialog);
}


void on_full_screen_pressed(LiVESButton *button, livespointer user_data) {
  // toolbar button (full screen)
  lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->full_screen),!mainw->fs);
}


void on_full_screen_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  char buff[PATH_MAX];
  LiVESWidget *fs_img;
  char *fnamex;

  if (mainw->current_file>-1&&!cfile->frames&&mainw->multitrack==NULL) return;

  if (user_data==NULL) {
    mainw->fs=!mainw->fs;
  }

  if (mainw->current_file==-1) return;

  fnamex=lives_build_filename(prefs->prefix_dir,ICON_DIR,"fullscreen.png",NULL);
  lives_snprintf(buff,PATH_MAX,"%s",fnamex);
  lives_free(fnamex);

  fs_img=lives_image_new_from_file(buff);
  lives_widget_show(fs_img);
  if (!mainw->fs) {
    if (lives_file_test(buff,LIVES_FILE_TEST_EXISTS)) {
      LiVESPixbuf *pixbuf=lives_image_get_pixbuf(LIVES_IMAGE(fs_img));
      lives_pixbuf_saturate_and_pixelate(pixbuf,pixbuf,0.2,FALSE);
    }
    lives_widget_set_tooltip_text(mainw->t_fullscreen,_("Fullscreen playback (f)"));
  } else lives_widget_set_tooltip_text(mainw->t_fullscreen,_("Fullscreen playback off (f)"));

  lives_tool_button_set_icon_widget(LIVES_TOOL_BUTTON(mainw->t_fullscreen),fs_img);

  if (mainw->playing_file>-1) {
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
        } else {
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
        load_frame_image(cfile->frameno);
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

        lives_container_set_border_width(LIVES_CONTAINER(mainw->playframe), widget_opts.border_width);

        lives_widget_set_sensitive(mainw->fade,TRUE);
        lives_widget_set_sensitive(mainw->dsize,TRUE);

        lives_widget_show(mainw->t_bckground);
        lives_widget_show(mainw->t_double);

        resize(1);
        if (mainw->multitrack==NULL) {
          if (cfile->is_loaded) {
            load_start_image(cfile->start);
            load_end_image(cfile->end);
          } else {
            load_start_image(0);
            load_end_image(0);
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

        play_window_set_title();

        if (mainw->opwx>-1) {
          //opwx and opwy were stored when we first switched to full screen
          lives_window_move(LIVES_WINDOW(mainw->play_window), mainw->opwx, mainw->opwy);
          mainw->opwx=-1;
          mainw->opwy=-1;
        } else {
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
            load_frame_image(cfile->frameno);
            mainw->frame_layer=frame_layer;
          }
        }
      } else {
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
        } else {
          lives_frame_set_label(LIVES_FRAME(mainw->playframe), "");
        }
      }
      if ((cfile->frames==1||cfile->play_paused)&&!mainw->noswitch&&mainw->multitrack==NULL&&
          (cfile->clip_type==CLIP_TYPE_DISK||cfile->clip_type==CLIP_TYPE_FILE)) {
        weed_plant_t *frame_layer=mainw->frame_layer;
        mainw->frame_layer=NULL;
        load_frame_image(cfile->frameno);
        mainw->frame_layer=frame_layer;
      }
    }
  } else {
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


void on_double_size_pressed(LiVESButton *button, livespointer user_data) {
  // toolbar button (separate window)
  lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->dsize),!mainw->double_size);
}



void on_double_size_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  char buff[PATH_MAX];
  LiVESWidget *sngl_img;
  char *fnamex;

  if (mainw->multitrack!=NULL||(mainw->current_file>-1&&cfile->frames==0&&user_data==NULL)) return;

  if (user_data==NULL) {
    mainw->double_size=!mainw->double_size;
  }

  if (mainw->current_file==-1) return;

  if (user_data!=NULL) {
    // change the blank window icons
    if (!mainw->double_size) {
      fnamex=lives_build_filename(prefs->prefix_dir,ICON_DIR,"zoom-in.png",NULL);
      lives_snprintf(buff,PATH_MAX,"%s",fnamex);
      lives_free(fnamex);
      sngl_img=lives_image_new_from_file(buff);
      lives_widget_set_tooltip_text(mainw->t_double,_("Double size (d)"));
    } else {
      fnamex=lives_build_filename(prefs->prefix_dir,ICON_DIR,"zoom-out.png",NULL);
      lives_snprintf(buff,PATH_MAX,"%s",fnamex);
      lives_free(fnamex);
      sngl_img=lives_image_new_from_file(buff);
      lives_widget_set_tooltip_text(mainw->t_double,_("Single size (d)"));
    }

    if (lives_file_test(buff,LIVES_FILE_TEST_EXISTS)) {
      LiVESPixbuf *pixbuf=lives_image_get_pixbuf(LIVES_IMAGE(sngl_img));
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
        resize_play_window();
        play_window_set_title();
      } else {
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
          load_frame_image(cfile->frameno);
          mainw->frame_layer=frame_layer;
        }
      }
    } else {
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
      } else {
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
        }
      }
    }
  }
}



void on_sepwin_pressed(LiVESButton *button, livespointer user_data) {
  if (mainw->go_away) return;

  // toolbar button (separate window)
  if (mainw->multitrack!=NULL) {
    lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->multitrack->sepwin),!mainw->sep_win);
    lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->sepwin),mainw->sep_win);
  } else lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->sepwin),!mainw->sep_win);
}



void on_sepwin_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  LiVESWidget *sep_img;
  LiVESWidget *sep_img2;

  char buff[PATH_MAX];
  char *fnamex;

  if (mainw->go_away) return;

  mainw->sep_win=!mainw->sep_win;

  if (mainw->multitrack!=NULL) {
    if (mainw->playing_file==-1) return;
    unpaint_lines(mainw->multitrack);
    mainw->multitrack->redraw_block=TRUE; // stop pb cursor from updating
    mt_show_current_frame(mainw->multitrack, FALSE);
  }

  fnamex=lives_build_filename(prefs->prefix_dir,ICON_DIR,"sepwin.png",NULL);
  lives_snprintf(buff,PATH_MAX,"%s",fnamex);
  lives_free(fnamex);

  sep_img=lives_image_new_from_file(buff);
  sep_img2=lives_image_new_from_file(buff);

  if (mainw->sep_win) {
    lives_widget_set_tooltip_text(mainw->m_sepwinbutton,_("Hide the play window (s)"));
    lives_widget_set_tooltip_text(mainw->t_sepwin,_("Hide the play window (s)"));
  } else {
    if (lives_file_test(buff,LIVES_FILE_TEST_EXISTS)) {
      LiVESPixbuf *pixbuf=lives_image_get_pixbuf(LIVES_IMAGE(sep_img));
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
    } else {
      kill_play_window();
    }
    if (mainw->multitrack!=NULL&&mainw->playing_file==-1) {
      activate_mt_preview(mainw->multitrack); // show frame preview
    }
  } else {
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
            lives_container_set_border_width(LIVES_CONTAINER(mainw->playframe), widget_opts.border_width);
            unfade_background();
            lives_widget_show(mainw->frame1);
            lives_widget_show(mainw->frame2);
            lives_widget_show(mainw->eventbox3);
            lives_widget_show(mainw->eventbox4);
            if (cfile->is_loaded) {
              load_start_image(cfile->start);
              load_end_image(cfile->end);
            } else {
              load_start_image(0);
              load_end_image(0);
            }
          }
          if (mainw->fs&&!mainw->faded) {
            resize(1);
          } else {
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

        mainw->pw_scroll_func=lives_signal_connect(LIVES_GUI_OBJECT(mainw->play_window), LIVES_WIDGET_SCROLL_EVENT,
                              LIVES_GUI_CALLBACK(on_mouse_scroll),
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
            play_window_set_title();
            unfade_background();
          }

          resize(1);
          resize_play_window();
        }

        if (mainw->play_window!=NULL&&LIVES_IS_XWINDOW(lives_widget_get_xwindow(mainw->play_window))) {
          hide_cursor(lives_widget_get_xwindow(mainw->play_window));
          lives_widget_set_app_paintable(mainw->play_window,TRUE);
        }
        if (cfile->frames==1||cfile->play_paused) {
          lives_widget_context_update();
          if (mainw->play_window!=NULL&&LIVES_IS_XWINDOW(lives_widget_get_xwindow(mainw->play_window))&&
              !mainw->noswitch&&mainw->multitrack==NULL&&(cfile->clip_type==CLIP_TYPE_DISK||
                  cfile->clip_type==CLIP_TYPE_FILE)) {
            weed_plant_t *frame_layer=mainw->frame_layer;
            mainw->frame_layer=NULL;
            load_frame_image(cfile->frameno);
            mainw->frame_layer=frame_layer;
          }
        }

      } else {
        // switch from separate window during playback
        if (cfile->frames>0&&mainw->multitrack==NULL) {
          lives_widget_show(mainw->playframe);
        }
        if (!mainw->fs&&mainw->multitrack==NULL) {
          if (!mainw->double_size) {
            resize(1);
          } else {
            if (palette->style&STYLE_1) {
              lives_widget_hide(mainw->sep_image);
            }
            lives_widget_hide(mainw->scrolledwindow);
            resize(2);
          }

          lives_widget_queue_draw(mainw->playframe);
          lives_widget_context_update();
          mainw->pwidth=lives_widget_get_allocation_width(mainw->playframe)-H_RESIZE_ADJUST;
          mainw->pheight=lives_widget_get_allocation_height(mainw->playframe)-V_RESIZE_ADJUST;
        } else {
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
          load_frame_image(cfile->frameno);
          mainw->frame_layer=frame_layer;
        }
      }
    }
  }
}


void on_showfct_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  prefs->show_framecount=!prefs->show_framecount;
  if (mainw->playing_file>-1) {
    if (!mainw->fs||(prefs->play_monitor!=prefs->gui_monitor&&mainw->play_window!=NULL)) {
      if (prefs->show_framecount) {
        lives_widget_show(mainw->framebar);
      } else {
        lives_widget_hide(mainw->framebar);
      }
    }
  }
}



void on_sticky_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  // type is SEPWIN_TYPE_STICKY (shown even when not playing)
  // or SEPWIN_TYPE_NON_STICKY (shown only when playing)


  if (prefs->sepwin_type==SEPWIN_TYPE_NON_STICKY) {
    prefs->sepwin_type=SEPWIN_TYPE_STICKY;
    if (mainw->sep_win) {
      if (mainw->playing_file==-1) {
        make_play_window();
      }
    } else {
      if (!(mainw->play_window==NULL)) {
        play_window_set_title();
      }
    }
  } else {
    if (prefs->sepwin_type==SEPWIN_TYPE_STICKY) {
      prefs->sepwin_type=SEPWIN_TYPE_NON_STICKY;
      if (mainw->sep_win) {
        if (mainw->playing_file==-1) {
          kill_play_window();
        } else {
          play_window_set_title();
        }
      }
    }
  }
}



void on_fade_pressed(LiVESButton *button, livespointer user_data) {
  // toolbar button (unblank background)
  if (mainw->fs&&(mainw->play_window==NULL||prefs->play_monitor==prefs->gui_monitor)) return;
  lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->fade),!mainw->faded);
}



void on_fade_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  mainw->faded=!mainw->faded;
  if (mainw->playing_file>-1&&(!mainw->fs||(prefs->play_monitor!=prefs->gui_monitor&&mainw->play_window!=NULL))) {
    if (mainw->faded) fade_background();
    else unfade_background();
  }
}




void on_showsubs_toggled(LiVESObject *obj, livespointer user_data) {
  prefs->show_subtitles=!prefs->show_subtitles;
  if (mainw->current_file>0&&mainw->multitrack==NULL) {
    if (mainw->play_window!=NULL) {
      load_preview_image(FALSE);
    }
    load_start_image(cfile->start);
    load_end_image(cfile->end);
  }
}



void on_boolean_toggled(LiVESObject *obj, livespointer user_data) {
  boolean *ppref=(boolean *)user_data;
  *ppref=!*ppref;
}


void on_loop_video_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  if (mainw->current_file==0) return;
  mainw->loop=!mainw->loop;
  lives_widget_set_sensitive(mainw->playclip, mainw->playing_file==-1&&clipboard!=NULL);
  if (mainw->current_file>-1) find_when_to_stop();
}


void on_loop_button_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  if (mainw->multitrack!=NULL) {
    lives_signal_handler_block(mainw->multitrack->loop_continue, mainw->multitrack->loop_cont_func);
    lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->multitrack->loop_continue),!mainw->loop_cont);
    lives_signal_handler_unblock(mainw->multitrack->loop_continue, mainw->multitrack->loop_cont_func);
  }
  lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->loop_continue), !mainw->loop_cont);
}


void on_loop_cont_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  char buff[PATH_MAX];
  LiVESWidget *loop_img;
  char *fnamex;

  mainw->loop_cont=!mainw->loop_cont;

  fnamex=lives_build_filename(prefs->prefix_dir,ICON_DIR,"loop.png",NULL);
  lives_snprintf(buff,PATH_MAX,"%s",fnamex);
  lives_free(fnamex);

  loop_img=lives_image_new_from_file(buff);

  if (mainw->loop_cont) {
    lives_widget_set_tooltip_text(mainw->m_loopbutton,_("Switch continuous looping off (o)"));
  } else {
    if (lives_file_test(buff,LIVES_FILE_TEST_EXISTS)) {
      LiVESPixbuf *pixbuf=lives_image_get_pixbuf(LIVES_IMAGE(loop_img));
      if (pixbuf!=NULL) lives_pixbuf_saturate_and_pixelate(pixbuf,pixbuf,0.2,FALSE);
    }
    lives_widget_set_tooltip_text(mainw->m_loopbutton,_("Switch continuous looping on (o)"));
  }

  lives_widget_show(loop_img);
  lives_tool_button_set_icon_widget(LIVES_TOOL_BUTTON(mainw->m_loopbutton),loop_img);

  lives_widget_set_sensitive(mainw->playclip, !(clipboard==NULL));
  if (mainw->current_file>-1) find_when_to_stop();
  else mainw->whentostop=NEVER_STOP;

#ifdef ENABLE_JACK
  if (prefs->audio_player==AUD_PLAYER_JACK) {
    if (mainw->jackd!=NULL&&(mainw->loop_cont||mainw->whentostop==NEVER_STOP)) {
      if (mainw->ping_pong&&prefs->audio_opts&AUDIO_OPTS_FOLLOW_FPS) mainw->jackd->loop=AUDIO_LOOP_PINGPONG;
      else mainw->jackd->loop=AUDIO_LOOP_FORWARD;
    } else if (mainw->jackd!=NULL) mainw->jackd->loop=AUDIO_LOOP_NONE;
  }
#endif
#ifdef HAVE_PULSE_AUDIO
  if (prefs->audio_player==AUD_PLAYER_PULSE) {
    if (mainw->pulsed!=NULL&&(mainw->loop_cont||mainw->whentostop==NEVER_STOP)) {
      if (mainw->ping_pong&&prefs->audio_opts&AUDIO_OPTS_FOLLOW_FPS) mainw->pulsed->loop=AUDIO_LOOP_PINGPONG;
      else mainw->pulsed->loop=AUDIO_LOOP_FORWARD;
    } else if (mainw->pulsed!=NULL) mainw->pulsed->loop=AUDIO_LOOP_NONE;
  }
#endif

}


void on_ping_pong_activate(LiVESMenuItem *menuitem, livespointer user_data) {
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


void on_volume_slider_value_changed(LiVESScaleButton *sbutton, livespointer user_data) {
  char *ttip;
  mainw->volume=lives_scale_button_get_value(sbutton);
  ttip=lives_strdup_printf(_("Audio volume (%.2f)"),mainw->volume);
  lives_widget_set_tooltip_text(mainw->vol_toolitem,_(ttip));
  lives_free(ttip);
}



void on_mute_button_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  if (mainw->multitrack!=NULL) {
    lives_signal_handler_block(mainw->multitrack->mute_audio, mainw->multitrack->mute_audio_func);
    lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->multitrack->mute_audio),!mainw->mute);
    lives_signal_handler_unblock(mainw->multitrack->mute_audio, mainw->multitrack->mute_audio_func);
  }
  lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->mute_audio), !mainw->mute);
}


boolean mute_audio_callback(LiVESAccelGroup *group, LiVESObject *obj, uint32_t keyval, LiVESXModifierType mod, livespointer user_data) {
  lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->mute_audio), !mainw->mute);
  return TRUE;
}


void on_mute_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  char buff[PATH_MAX];
  LiVESWidget *mute_img;
  LiVESWidget *mute_img2=NULL;
  char *fnamex;

  mainw->mute=!mainw->mute;

  // change the mute icon
  fnamex=lives_build_filename(prefs->prefix_dir,ICON_DIR,"volume_mute.png",NULL);
  lives_snprintf(buff,PATH_MAX,"%s",fnamex);
  lives_free(fnamex);

  mute_img=lives_image_new_from_file(buff);
  if (mainw->preview_box!=NULL) mute_img2=lives_image_new_from_file(buff);
  if (mainw->mute) {
    lives_widget_set_tooltip_text(mainw->m_mutebutton,_("Unmute the audio (z)"));
    if (mainw->preview_box!=NULL) lives_widget_set_tooltip_text(mainw->p_mutebutton,_("Unmute the audio (z)"));
  } else {
    if (lives_file_test(buff,LIVES_FILE_TEST_EXISTS)) {
      LiVESPixbuf *pixbuf=lives_image_get_pixbuf(LIVES_IMAGE(mute_img));
      if (pixbuf!=NULL) lives_pixbuf_saturate_and_pixelate(pixbuf,pixbuf,0.2,FALSE);
      if (mainw->preview_box!=NULL) {
        pixbuf=lives_image_get_pixbuf(LIVES_IMAGE(mute_img2));
        if (pixbuf!=NULL) lives_pixbuf_saturate_and_pixelate(pixbuf,pixbuf,0.2,FALSE);
      }
    }
    lives_widget_set_tooltip_text(mainw->m_mutebutton,_("Mute the audio (z)"));
    if (mainw->preview_box!=NULL) lives_widget_set_tooltip_text(mainw->p_mutebutton,_("Mute the audio (z)"));
  }

  lives_widget_show(mute_img);

  lives_tool_button_set_icon_widget(LIVES_TOOL_BUTTON(mainw->m_mutebutton),mute_img);

  if (mainw->preview_box!=NULL) {
    lives_widget_show(mute_img2);
    //lives_button_set_image(LIVES_BUTTON(mainw->p_mutebutton),mute_img2); // doesn't work (gtk+ bug ?)
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


void on_spin_value_changed(LiVESSpinButton *spinbutton, livespointer user_data) {
  // TODO - use array
  switch (LIVES_POINTER_TO_INT(user_data)) {
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


void on_spin_start_value_changed(LiVESSpinButton *spinbutton, livespointer user_data) {
  // generic
  // TODO - use array
  switch (LIVES_POINTER_TO_INT(user_data)) {
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


void on_spin_step_value_changed(LiVESSpinButton *spinbutton, livespointer user_data) {
  // generic
  // TODO - use array
  switch (LIVES_POINTER_TO_INT(user_data)) {
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


void on_spin_end_value_changed(LiVESSpinButton *spinbutton, livespointer user_data) {
  // generic
  // TODO - use array
  switch (LIVES_POINTER_TO_INT(user_data)) {
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



void on_rev_clipboard_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  // reverse the clipboard
  char *com;
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

  d_print(_("Reversing clipboard..."));
  com=lives_strdup_printf("%s reverse \"%s\" %d %d \"%s\"",prefs->backend,clipboard->handle,1,clipboard->frames,
                          get_image_ext_for_type(cfile->img_type));

  lives_rm(cfile->info_file);
  mainw->com_failed=FALSE;
  lives_system(com,FALSE);
  lives_free(com);

  if (!mainw->com_failed) {
    cfile->progress_start=1;
    cfile->progress_end=cfile->frames;

    // show a progress dialog, not cancellable
    do_progress_dialog(TRUE,FALSE,_("Reversing clipboard"));
  }

  if (mainw->com_failed||mainw->error) d_print_failed();
  else {
    if (clipboard->frame_index!=NULL) reverse_frame_index(0);
    d_print_done();
  }

  mainw->current_file=current_file;
  sensitize();


}




void on_load_subs_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  char *subfile;
  char **const filt=LIVES_SUBS_FILTER;
  char filename[512];
  char *subfname,*isubfname;
  lives_subtitle_type_t subtype=SUBTITLE_TYPE_NONE;
  char *lfile_name;
  char *ttl;

  if (cfile->subt!=NULL) if (!do_existing_subs_warning()) return;

  // try to repaint the screen, as it may take a few seconds to get a directory listing
  lives_widget_context_update();

  ttl=lives_strdup(_("Load Subtitles"));

  if (strlen(mainw->vid_load_dir)) {
    subfile=choose_file(mainw->vid_load_dir,NULL,filt,LIVES_FILE_CHOOSER_ACTION_OPEN,ttl,NULL);
  } else subfile=choose_file(NULL,NULL,filt,LIVES_FILE_CHOOSER_ACTION_OPEN,ttl,NULL);
  lives_free(ttl);

  if (subfile==NULL) return; // cancelled

  lives_snprintf(filename,512,"%s",subfile);
  lives_free(subfile);

  get_filename(filename,FALSE); // strip extension
  isubfname=lives_strdup_printf("%s.%s",filename,LIVES_FILE_EXT_SRT);
  lfile_name=lives_filename_from_utf8(isubfname,-1,NULL,NULL,NULL);

  if (lives_file_test(lfile_name,LIVES_FILE_TEST_EXISTS)) {
    subfname=lives_build_filename(prefs->tmpdir,cfile->handle,"subs.",LIVES_FILE_EXT_SRT,NULL);
    subtype=SUBTITLE_TYPE_SRT;
  } else {
    lives_free(isubfname);
    lives_free(lfile_name);
    isubfname=lives_strdup_printf("%s.%s",filename,LIVES_FILE_EXT_SUB);
    lfile_name=lives_filename_from_utf8(isubfname,-1,NULL,NULL,NULL);

    if (lives_file_test(isubfname,LIVES_FILE_TEST_EXISTS)) {
      subfname=lives_build_filename(prefs->tmpdir,cfile->handle,"subs.",LIVES_FILE_EXT_SUB,NULL);
      subtype=SUBTITLE_TYPE_SUB;
    } else {
      lives_free(isubfname);
      do_invalid_subs_error();
      lives_free(lfile_name);
      return;
    }
  }

  if (cfile->subt!=NULL) {
    // erase any existing subs
    on_erase_subs_activate(NULL,NULL);
    subtitles_free(cfile);
  }

  mainw->com_failed=FALSE;
  lives_cp(lfile_name,subfname);

  if (mainw->com_failed) {
    lives_free(subfname);
    lives_free(isubfname);
    lives_free(lfile_name);
    return;
  }

  subtitles_init(cfile,subfname,subtype);
  lives_free(subfname);

  // force update
  switch_to_file(0,mainw->current_file);

  d_print(_("Loaded subtitle file: %s\n"),isubfname);

  lives_free(isubfname);
  lives_free(lfile_name);
}





void on_save_subs_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  char *subfile;
  char xfname[512];
  char xfname2[512];

  LiVESEntry *entry=(LiVESEntry *)user_data;

  // try to repaint the screen, as it may take a few seconds to get a directory listing
  lives_widget_context_update();

  lives_snprintf(xfname,512,"%s",mainw->subt_save_file);
  get_dirname(xfname);

  lives_snprintf(xfname2,512,"%s",mainw->subt_save_file);
  get_basename(xfname2);

  subfile=choose_file(xfname,xfname2,NULL,LIVES_FILE_CHOOSER_ACTION_SAVE,NULL,NULL);

  if (subfile==NULL) return; // cancelled

  if (check_file(subfile,FALSE))
    lives_entry_set_text(entry,subfile);

  lives_free(subfile);
}



void on_erase_subs_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  char *sfname;

  if (cfile->subt==NULL) return;

  if (menuitem!=NULL)
    if (!do_erase_subs_warning()) return;

  switch (cfile->subt->type) {
  case SUBTITLE_TYPE_SRT:
    sfname=lives_build_filename(prefs->tmpdir,cfile->handle,"subs.",LIVES_FILE_EXT_SRT,NULL);
    break;

  case SUBTITLE_TYPE_SUB:
    sfname=lives_build_filename(prefs->tmpdir,cfile->handle,"subs.",LIVES_FILE_EXT_SUB,NULL);
    break;

  default:
    return;
  }

  subtitles_free(cfile);

  lives_rm(sfname);
  lives_free(sfname);

  if (menuitem!=NULL) {
    // force update
    switch_to_file(0,mainw->current_file);

    d_print(_("Subtitles were erased.\n"));
  }
}





void on_load_audio_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  LiVESWidget *chooser;

  char **filt=LIVES_AUDIO_LOAD_FILTER;

  int resp;

  if (mainw->multitrack!=NULL) {
    if (mainw->multitrack->idlefunc>0) {
      lives_source_remove(mainw->multitrack->idlefunc);
      mainw->multitrack->idlefunc=0;
    }
    mt_desensitise(mainw->multitrack);
    lives_widget_set_sensitive(mainw->multitrack->playall,TRUE);
    lives_widget_set_sensitive(mainw->m_playbutton, TRUE);
  }

  chooser=choose_file_with_preview(strlen(mainw->audio_dir)?mainw->audio_dir:NULL,_("Select Audio File"),filt,
                                   LIVES_FILE_SELECTION_AUDIO_ONLY);

  resp=lives_dialog_run(LIVES_DIALOG(chooser));

  end_fs_preview();
  mainw->fs_playarea=NULL;

  if (resp!=LIVES_RESPONSE_ACCEPT) on_filechooser_cancel_clicked(chooser);
  else on_open_new_audio_clicked(LIVES_FILE_CHOOSER(chooser),NULL);

}



void on_open_new_audio_clicked(LiVESFileChooser *chooser, livespointer user_data) {
  // open audio file
  // also called from osc.c

  char *a_type;
  char *com,*tmp;
  char **array;

  int oundo_start;
  int oundo_end;
  int israw=1;
  int asigned,aendian;

  boolean has_lmap_error=FALSE;
  boolean bad_header=FALSE;
  boolean preparse=FALSE;
  boolean gotit=FALSE;

  register int i;

  if (!(prefs->warning_mask&WARN_MASK_LAYOUT_DELETE_AUDIO)) {
    if ((mainw->xlays=layout_audio_is_affected(mainw->current_file,0.))!=NULL) {
      if (!do_warning_dialog(
            _("\nLoading new audio may cause missing audio in some multitrack layouts.\nAre you sure you wish to continue ?\n."))) {
        lives_list_free_strings(mainw->xlays);
        lives_list_free(mainw->xlays);
        mainw->xlays=NULL;
        if (mainw->multitrack!=NULL) {
          mt_sensitise(mainw->multitrack);
          mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
        }
        return;
      }
      add_lmap_error(LMAP_ERROR_DELETE_AUDIO,cfile->name,(livespointer)cfile->layout_map,mainw->current_file,0,0.,
                     cfile->stored_layout_frame>0.);
      has_lmap_error=TRUE;
      lives_list_free_strings(mainw->xlays);
      lives_list_free(mainw->xlays);
      mainw->xlays=NULL;
    }
  }

  if (!has_lmap_error&&!(prefs->warning_mask&WARN_MASK_LAYOUT_ALTER_AUDIO)&&
      (mainw->xlays=layout_audio_is_affected(mainw->current_file,0.))) {
    if (!do_layout_alter_audio_warning()) {
      lives_list_free_strings(mainw->xlays);
      lives_list_free(mainw->xlays);
      mainw->xlays=NULL;
      if (mainw->multitrack!=NULL) {
        mt_sensitise(mainw->multitrack);
        mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
      }
      return;
    }
    add_lmap_error(LMAP_ERROR_ALTER_AUDIO,cfile->name,(livespointer)cfile->layout_map,mainw->current_file,0,0.,
                   cfile->stored_layout_audio>0.);
    has_lmap_error=TRUE;
    lives_list_free_strings(mainw->xlays);
    lives_list_free(mainw->xlays);
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
    char *filename=lives_file_chooser_get_filename(chooser);
    lives_snprintf(file_name,PATH_MAX,"%s",(tmp=lives_filename_to_utf8(filename,-1,NULL,NULL,NULL)));
    lives_free(filename);
    lives_free(tmp);
  } else lives_snprintf(file_name,PATH_MAX,"%s",(char *)user_data);

  lives_snprintf(mainw->audio_dir,PATH_MAX,"%s",file_name);
  get_dirname(mainw->audio_dir);
  end_fs_preview();
  lives_widget_destroy(LIVES_WIDGET(chooser));
  lives_widget_context_update();
  mainw->fs_playarea=NULL;

  a_type=get_extension(file_name);

  if (strlen(a_type)) {
    char **filt=LIVES_AUDIO_LOAD_FILTER;
    for (i=0; filt[i]!=NULL; i++) {
      if (!lives_ascii_strcasecmp(a_type,filt[i]+2)) gotit=TRUE; // skip past "*." in filt
    }
  }

  if (gotit) {
    com=lives_strdup_printf("%s audioopen \"%s\" \"%s\"",prefs->backend,cfile->handle,
                            (tmp=lives_filename_from_utf8(file_name,-1,NULL,NULL,NULL)));
    lives_free(tmp);
  } else {
    do_audio_import_error();
    mainw->noswitch=FALSE;
    if (mainw->multitrack!=NULL) {
      mt_sensitise(mainw->multitrack);
      mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
    }
    return;
  }

  if (!lives_ascii_strncasecmp(a_type,LIVES_FILE_EXT_WAV,3)) israw=0;

  if (capable->has_mplayer||capable->has_mplayer2||capable->has_mpv) {
    if (read_file_details(file_name,TRUE)) {
      array=lives_strsplit(mainw->msg,"|",15);
      cfile->arate=atoi(array[9]);
      cfile->achans=atoi(array[10]);
      cfile->asampsize=atoi(array[11]);
      cfile->signed_endian=get_signed_endian(atoi(array[12]), atoi(array[13]));
      lives_strfreev(array);
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

  d_print(""); // force switchtext
  d_print(_("Opening audio %s, type %s..."),file_name,a_type);

  lives_rm(cfile->info_file);
  mainw->com_failed=FALSE;
  lives_system(com,FALSE);
  lives_free(com);

  if (mainw->com_failed) {
    cfile->arate=cfile->undo_arate;
    cfile->achans=cfile->undo_achans;
    cfile->asampsize=cfile->undo_asampsize;
    cfile->signed_endian=cfile->undo_signed_endian;
    cfile->arps=cfile->undo_arps;
    cfile->undo_start=oundo_start;
    cfile->undo_end=oundo_end;
    sensitize();
    if (mainw->multitrack!=NULL) {
      mt_sensitise(mainw->multitrack);
      mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
    }
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

  if (!(do_progress_dialog(TRUE,TRUE,_("Opening audio")))) {
    lives_widget_queue_draw(mainw->LiVES);
    lives_widget_context_update();
    mainw->cancelled=CANCEL_NONE;
    mainw->error=FALSE;
    mainw->com_failed=FALSE;
    lives_rm(cfile->info_file);
    com=lives_strdup_printf("%s cancel_audio \"%s\"",prefs->backend,cfile->handle);
    lives_system(com,FALSE);
    do_auto_dialog(_("Cancelling"),0);
    lives_free(com);
    cfile->opening_audio=cfile->opening=cfile->opening_only_audio=FALSE;
    cfile->arate=cfile->undo_arate;
    cfile->achans=cfile->undo_achans;
    cfile->asampsize=cfile->undo_asampsize;
    cfile->signed_endian=cfile->undo_signed_endian;
    cfile->arps=cfile->undo_arps;
    cfile->undo_start=oundo_start;
    cfile->undo_end=oundo_end;
    sensitize();
    if (mainw->multitrack!=NULL) {
      mt_sensitise(mainw->multitrack);
      mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
    }
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
    array=lives_strsplit(mainw->msg,"|",7);
    cfile->arate=atoi(array[1]);
    cfile->achans=atoi(array[2]);
    cfile->asampsize=atoi(array[3]);
    cfile->signed_endian=get_signed_endian(atoi(array[4]), atoi(array[5]));
    cfile->afilesize=strtol(array[6],NULL,10);
    lives_strfreev(array);

    if (cfile->undo_arate>0) cfile->arps=cfile->undo_arps/cfile->undo_arate*cfile->arate;
    else cfile->arps=cfile->arate;
  }


  if (cfile->afilesize==0) {
    d_print_failed();

    mainw->cancelled=CANCEL_NONE;
    mainw->error=FALSE;
    lives_rm(cfile->info_file);

    com=lives_strdup_printf("%s cancel_audio \"%s\"",prefs->backend,cfile->handle);

    mainw->com_failed=FALSE;
    lives_system(com,FALSE);
    lives_free(com);

    if (!mainw->com_failed) do_auto_dialog(_("Cancelling"),0);

    cfile->arate=cfile->undo_arate;
    cfile->achans=cfile->undo_achans;
    cfile->asampsize=cfile->undo_asampsize;
    cfile->signed_endian=cfile->undo_signed_endian;
    cfile->arps=cfile->undo_arps;
    cfile->undo_start=oundo_start;
    cfile->undo_end=oundo_end;
    sensitize();
    if (mainw->multitrack!=NULL) {
      mt_sensitise(mainw->multitrack);
      mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
    }
    reget_afilesize(mainw->current_file);
    get_play_times();
    mainw->noswitch=FALSE;
    return;
  }

  cfile->changed=TRUE;
  d_print_done();

  d_print(P_("New audio: %d Hz %d channel %d bps\n","New audio: %d Hz %d channels %d bps\n",cfile->achans),
          cfile->arate,cfile->achans,cfile->asampsize);

  mainw->com_failed=FALSE;
  mainw->cancelled=CANCEL_NONE;
  mainw->error=FALSE;
  lives_rm(cfile->info_file);

  com=lives_strdup_printf("%s commit_audio \"%s\" %d",prefs->backend,cfile->handle,israw);
  lives_system(com,FALSE);
  lives_free(com);

  if (mainw->com_failed) {
    cfile->arate=cfile->undo_arate;
    cfile->achans=cfile->undo_achans;
    cfile->asampsize=cfile->undo_asampsize;
    cfile->signed_endian=cfile->undo_signed_endian;
    cfile->arps=cfile->undo_arps;
    cfile->undo_start=oundo_start;
    cfile->undo_end=oundo_end;
    sensitize();
    if (mainw->multitrack!=NULL) {
      mt_sensitise(mainw->multitrack);
      mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
    }
    reget_afilesize(mainw->current_file);
    get_play_times();
    mainw->noswitch=FALSE;
    return;
  }

  if (!do_auto_dialog(_("Committing audio"),0)) {
    //cfile->may_be_damaged=TRUE;
    d_print_failed();
    if (mainw->multitrack!=NULL) {
      mt_sensitise(mainw->multitrack);
      mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
    }
    return;
  }


  if (prefs->save_directories) {
    set_pref("audio_dir",mainw->audio_dir);
  }
  if (!prefs->conserve_space) {
    cfile->undo_action=UNDO_NEW_AUDIO;
    set_undoable(_("New Audio"),TRUE);
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

  switch_to_file(mainw->current_file,mainw->current_file);

  mainw->noswitch=FALSE;

  if (mainw->multitrack!=NULL) {
    mt_sensitise(mainw->multitrack);
    mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
  }

}




void on_load_cdtrack_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  LiVESWidget *cdtrack_dialog;

  lives_widget_context_update();

  if (!strlen(prefs->cdplay_device)) {
    do_cd_error_dialog();
    return;
  }

  cdtrack_dialog = create_cdtrack_dialog(0,NULL);
  lives_widget_show_all(cdtrack_dialog);
  mainw->fx1_val=1;

}


void on_eject_cd_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  char *com;

  lives_widget_context_update();

  if (!strlen(prefs->cdplay_device)) {
    do_cd_error_dialog();
    return;
  }

  com=lives_strdup_printf("eject \"%s\"",prefs->cdplay_device);

  lives_system(com,TRUE);
  lives_free(com);
}


void on_load_cdtrack_ok_clicked(LiVESButton *button, livespointer user_data) {
  char *com;
  char **array;

  boolean was_new=FALSE;

  int new_file=mainw->first_free_file;
  int asigned,endian;

  boolean has_lmap_error=FALSE;
  boolean bad_header=FALSE;

  lives_general_button_clicked(button,NULL);

  if (mainw->current_file>-1) {
    if (!(prefs->warning_mask&WARN_MASK_LAYOUT_DELETE_AUDIO)) {
      if ((mainw->xlays=layout_audio_is_affected(mainw->current_file,0.))!=NULL) {
        if (!do_warning_dialog(
              _("\nLoading new audio may cause missing audio in some multitrack layouts.\nAre you sure you wish to continue ?\n."))) {
          lives_list_free_strings(mainw->xlays);
          lives_list_free(mainw->xlays);
          mainw->xlays=NULL;
          return;
        }
        add_lmap_error(LMAP_ERROR_DELETE_AUDIO,cfile->name,(livespointer)cfile->layout_map,mainw->current_file,0,0.,
                       cfile->stored_layout_audio>0.);
        has_lmap_error=TRUE;
        lives_list_free_strings(mainw->xlays);
        lives_list_free(mainw->xlays);
        mainw->xlays=NULL;
      }
    }

    if (!has_lmap_error&&!(prefs->warning_mask&WARN_MASK_LAYOUT_ALTER_AUDIO)&&
        (mainw->xlays=layout_audio_is_affected(mainw->current_file,0.))!=NULL) {
      if (!do_layout_alter_audio_warning()) {
        lives_list_free_strings(mainw->xlays);
        lives_list_free(mainw->xlays);
        mainw->xlays=NULL;
        return;
      }
      add_lmap_error(LMAP_ERROR_ALTER_AUDIO,cfile->name,(livespointer)cfile->layout_map,mainw->current_file,0,0.,
                     cfile->stored_layout_audio>0.);
      has_lmap_error=TRUE;
      lives_list_free_strings(mainw->xlays);
      lives_list_free(mainw->xlays);
      mainw->xlays=NULL;
    }
  }

  d_print(_("Opening CD track %d from %s..."),(int)mainw->fx1_val,prefs->cdplay_device);

  if (mainw->current_file==-1) {
    if (!get_new_handle(new_file,lives_strdup_printf(_("CD track %d"),(int)mainw->fx1_val))) {
      return;
    }

    mainw->current_file=new_file;
    lives_snprintf(cfile->type,40,"CD track %d on %s",(int)mainw->fx1_val,prefs->cdplay_device);
    get_play_times();
    add_to_clipmenu();
    was_new=TRUE;
    cfile->opening=cfile->opening_audio=cfile->opening_only_audio=TRUE;
    cfile->hsize=DEFAULT_FRAME_HSIZE;
    cfile->vsize=DEFAULT_FRAME_VSIZE;
  } else {
    mainw->noswitch=TRUE;

    cfile->undo_arate=cfile->arate;
    cfile->undo_achans=cfile->achans;
    cfile->undo_asampsize=cfile->asampsize;
    cfile->undo_signed_endian=cfile->signed_endian;
    cfile->undo_arps=cfile->arps;
  }

  com=lives_strdup_printf("%s cdopen \"%s\" %d",prefs->backend,cfile->handle,(int)mainw->fx1_val);

  lives_rm(cfile->info_file);
  mainw->com_failed=FALSE;
  lives_system(com,FALSE);
  lives_free(com);

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


  if (!(do_progress_dialog(TRUE,TRUE,_("Opening CD track...")))) {
    lives_widget_queue_draw(mainw->LiVES);
    lives_widget_context_update();

    if (!was_new) {
      mainw->com_failed=FALSE;
      mainw->cancelled=CANCEL_NONE;
      mainw->error=FALSE;
      lives_rm(cfile->info_file);

      com=lives_strdup_printf("%s cancel_audio \"%s\"",prefs->backend,cfile->handle);
      lives_system(com,FALSE);
      lives_free(com);

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
    d_print(_("Error loading CD track\n"));

    lives_widget_queue_draw(mainw->LiVES);
    lives_widget_context_update();

    if (!was_new) {
      com=lives_strdup_printf("%s cancel_audio \"%s\"",prefs->backend,cfile->handle);
      mainw->cancelled=CANCEL_NONE;
      mainw->error=FALSE;
      lives_rm(cfile->info_file);
      mainw->com_failed=FALSE;
      lives_system(com,FALSE);
      lives_free(com);

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

  array=lives_strsplit(mainw->msg,"|",5);
  cfile->arate=atoi(array[1]);
  cfile->achans=atoi(array[2]);
  cfile->asampsize=atoi(array[3]);
  cfile->afilesize=strtol(array[4],NULL,10);
  lives_strfreev(array);

  if (!was_new&&cfile->undo_arate>0) cfile->arps=cfile->undo_arps/cfile->undo_arate*cfile->arate;
  else cfile->arps=cfile->arate;

  asigned=!(cfile->signed_endian&AFORM_UNSIGNED);
  endian=cfile->signed_endian&AFORM_BIG_ENDIAN;

  if (cfile->afilesize==0l) {
    d_print(_("Error loading CD track\n"));

    if (!was_new) {
      com=lives_strdup_printf("%s cancel_audio \"%s\"",prefs->backend,cfile->handle);
      mainw->com_failed=FALSE;
      mainw->cancelled=CANCEL_NONE;
      mainw->error=FALSE;
      lives_rm(cfile->info_file);
      lives_system(com,FALSE);
      lives_free(com);

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
  lives_rm(cfile->info_file);

  com=lives_strdup_printf("%s commit_audio \"%s\"",prefs->backend,cfile->handle);
  lives_system(com, FALSE);
  lives_free(com);

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
  d_print(P_("New audio: %d Hz %d channel %d bps\n","New audio: %d Hz %d channels %d bps\n",cfile->achans),
          cfile->arate,cfile->achans,cfile->asampsize);

  if (!was_new) {
    if (!prefs->conserve_space) {
      cfile->undo_action=UNDO_NEW_AUDIO;
      set_undoable(_("New Audio"),TRUE);
    }
  }

  lives_widget_set_sensitive(mainw->loop_video,TRUE);
  mainw->noswitch=FALSE;

  lives_notify(LIVES_OSC_NOTIFY_CLIP_OPENED,"");

}

void on_load_vcd_ok_clicked(LiVESButton *button, livespointer         user_data) {
  lives_general_button_clicked(button,NULL);
  if (LIVES_POINTER_TO_INT(user_data)==1) {
    lives_snprintf(file_name,PATH_MAX,"dvd://%d",(int)mainw->fx1_val);
    if (mainw->file_open_params!=NULL) lives_free(mainw->file_open_params);
    mainw->file_open_params=lives_strdup_printf("-chapter %d -aid %d",(int)mainw->fx2_val,(int)mainw->fx3_val);
  } else {
    lives_snprintf(file_name,PATH_MAX,"vcd://%d",(int)mainw->fx1_val);
  }
  open_sel_range_activate();
}



void popup_lmap_errors(LiVESMenuItem *menuitem, livespointer user_data) {
  // popup layout map errors dialog
  LiVESWidget *dialog_action_area,*vbox;
  LiVESWidget *button;
  text_window *textwindow;

  if (prefs->warning_mask&WARN_MASK_LAYOUT_POPUP) return;

  widget_opts.expand=LIVES_EXPAND_NONE;
  textwindow=create_text_window(_("Layout Errors"),NULL,mainw->layout_textbuffer);
  widget_opts.expand=LIVES_EXPAND_DEFAULT;

  dialog_action_area = lives_dialog_get_action_area(LIVES_DIALOG(textwindow->dialog));
  if (LIVES_IS_BUTTON_BOX(dialog_action_area)) lives_button_box_set_layout(LIVES_BUTTON_BOX(dialog_action_area), LIVES_BUTTONBOX_SPREAD);

  vbox = lives_dialog_get_content_area(LIVES_DIALOG(textwindow->dialog));

  add_warn_check(LIVES_BOX(vbox),WARN_MASK_LAYOUT_POPUP);

  button = lives_button_new_from_stock(LIVES_STOCK_CLOSE,_("_Close Window"));

  lives_dialog_add_action_widget(LIVES_DIALOG(textwindow->dialog), button, LIVES_RESPONSE_OK);

  lives_signal_connect(LIVES_GUI_OBJECT(button), LIVES_WIDGET_CLICKED_SIGNAL,
                       LIVES_GUI_CALLBACK(lives_general_button_clicked),
                       textwindow);

  lives_container_set_border_width(LIVES_CONTAINER(button), widget_opts.border_width);
  lives_widget_set_can_focus_and_default(button);

  textwindow->clear_button = lives_button_new_from_stock(LIVES_STOCK_CLEAR,_("Clear _Errors"));

  lives_dialog_add_action_widget(LIVES_DIALOG(textwindow->dialog), textwindow->clear_button, LIVES_RESPONSE_CANCEL);

  lives_signal_connect(LIVES_GUI_OBJECT(textwindow->clear_button), LIVES_WIDGET_CLICKED_SIGNAL,
                       LIVES_GUI_CALLBACK(on_lerrors_clear_clicked),
                       LIVES_INT_TO_POINTER(FALSE));

  lives_container_set_border_width(LIVES_CONTAINER(textwindow->clear_button), widget_opts.border_width);
  lives_widget_set_can_focus_and_default(textwindow->clear_button);

  textwindow->delete_button = lives_button_new_from_stock(LIVES_STOCK_DELETE,_("_Delete affected layouts"));

  lives_dialog_add_action_widget(LIVES_DIALOG(textwindow->dialog), textwindow->delete_button, LIVES_RESPONSE_CANCEL);

  lives_container_set_border_width(LIVES_CONTAINER(textwindow->delete_button), widget_opts.border_width);
  lives_widget_set_can_focus_and_default(textwindow->delete_button);

  lives_signal_connect(LIVES_GUI_OBJECT(textwindow->delete_button), LIVES_WIDGET_CLICKED_SIGNAL,
                       LIVES_GUI_CALLBACK(on_lerrors_delete_clicked),
                       NULL);

  lives_widget_show_all(textwindow->dialog);

}



void on_rename_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  renamew=create_rename_dialog(1);
  lives_widget_show_all(renamew->dialog);
}


void on_rename_set_name(LiVESButton *button, livespointer user_data) {
  char title[256];
  boolean bad_header=FALSE;

  if (user_data==NULL) {
    lives_snprintf(title,256,"%s",lives_entry_get_text(LIVES_ENTRY(renamew->entry)));
    lives_widget_destroy(renamew->dialog);
    lives_free(renamew);
  } else lives_snprintf(title,256,"%s",(char *)user_data);

  if (!(strlen(title))) return;

  set_menu_text(cfile->menuentry,title,FALSE);
  lives_snprintf(cfile->name,256,"%s",title);

  if (user_data==NULL) {
    set_main_title(title,0);
  }

  save_clip_value(mainw->current_file,CLIP_DETAILS_CLIPNAME,cfile->name);
  if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;

  if (bad_header) do_header_write_error(mainw->current_file);
  cfile->was_renamed=TRUE;
}


void on_toy_activate(LiVESMenuItem *menuitem, livespointer user_data) {
#ifdef ENABLE_OSC
#ifndef IS_MINGW
  char string[PATH_MAX];
#endif
  char *com;
#endif

  if (menuitem!=NULL&&mainw->toy_type==LIVES_POINTER_TO_INT(user_data)) {
    // switch is off
    user_data=LIVES_INT_TO_POINTER(LIVES_TOY_NONE);
  }

  switch (mainw->toy_type) {
  // old status
  case LIVES_TOY_AUTOLIVES:
    lives_signal_handler_block(mainw->toy_autolives, mainw->toy_func_autolives);
    lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->toy_autolives),FALSE);
    lives_signal_handler_unblock(mainw->toy_autolives, mainw->toy_func_autolives);

    if (mainw->toy_alives_pgid>1) {
      lives_killpg(mainw->toy_alives_pgid,LIVES_SIGHUP);
    }

    // switch off rte so as not to cause alarm
    if (mainw->autolives_reset_fx)
      rte_on_off_callback(NULL,NULL,0,(LiVESXModifierType)0,LIVES_INT_TO_POINTER(0));
    mainw->autolives_reset_fx=FALSE;
    break;

  case LIVES_TOY_MAD_FRAMES:
    lives_signal_handler_block(mainw->toy_random_frames, mainw->toy_func_random_frames);
    lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->toy_random_frames),FALSE);
    lives_signal_handler_unblock(mainw->toy_random_frames, mainw->toy_func_random_frames);
    if (mainw->playing_file>-1) {
      if (mainw->faded) {
        lives_widget_hide(mainw->start_image);
        lives_widget_hide(mainw->end_image);
      }
      load_start_image(cfile->start);
      load_end_image(cfile->end);
    }
    break;
  case LIVES_TOY_TV:
    lives_signal_handler_block(mainw->toy_tv, mainw->toy_func_lives_tv);
    lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->toy_tv),FALSE);
    lives_signal_handler_unblock(mainw->toy_tv, mainw->toy_func_lives_tv);
    break;
  default:
    lives_signal_handler_block(mainw->toy_none, mainw->toy_func_none);
    lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->toy_none),FALSE);
    lives_signal_handler_unblock(mainw->toy_none, mainw->toy_func_none);
    break;
  }

  mainw->toy_type=(lives_toy_t)LIVES_POINTER_TO_INT(user_data);

  switch (mainw->toy_type) {
  case LIVES_TOY_NONE:
    lives_signal_handler_block(mainw->toy_none, mainw->toy_func_none);
    lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->toy_none),TRUE);
    lives_signal_handler_unblock(mainw->toy_none, mainw->toy_func_none);
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

      on_toy_activate(NULL, LIVES_INT_TO_POINTER(LIVES_TOY_NONE));
      return;
    }

#ifndef IS_MINGW
    // search for autolives.pl
    if (!capable->has_autolives) {
      get_location("autolives.pl",string,PATH_MAX);
      if (strlen(string)) capable->has_autolives=TRUE;
      else {
        do_no_autolives_error();
        on_toy_activate(NULL, LIVES_INT_TO_POINTER(LIVES_TOY_NONE));
        return;
      }
    }
#endif

    // chek if osc is started; if not ask permission
    if (!prefs->osc_udp_started) {
      if (!lives_ask_permission(LIVES_PERM_OSC_PORTS)) {
        // permission not given
        on_toy_activate(NULL, LIVES_INT_TO_POINTER(LIVES_TOY_NONE));
        return;
      }

      // try: start up osc
      prefs->osc_udp_started=lives_osc_init(prefs->osc_udp_port);
      if (!prefs->osc_udp_started) {
        on_toy_activate(NULL, LIVES_INT_TO_POINTER(LIVES_TOY_NONE));
        return;
      }
    }

    // TODO *** store full fx state and restore it
    if (mainw->rte==EFFECT_NONE) {
      mainw->autolives_reset_fx=TRUE;
    }

#ifndef IS_MINGW
    com=lives_strdup_printf("autolives.pl localhost %d %d >/dev/null 2>&1",prefs->osc_udp_port,prefs->osc_udp_port-1);
#else
    com=lives_strdup_printf("START /MIN /B perl \"%s\\bin\\autolives.pl\" localhost %d %d >NUL 2>&1",
                            prefs->prefix_dir,prefs->osc_udp_port,prefs->osc_udp_port-1);
#endif

    mainw->toy_alives_pgid=lives_fork(com);

    lives_free(com);

    break;
#endif
  case LIVES_TOY_MAD_FRAMES:
    break;
  case LIVES_TOY_TV:
    // load in the lives TV clip
    deduce_file(LIVES_TV_CHANNEL1,0.,0);

    // if we choose to discard it, discard it....otherwise keep it
    if (prefs->discard_tv) {
      close_current_file(0);
    } else {
      // keep it
      int current_file=mainw->current_file;
      char *com=lives_strdup_printf("%s commit_audio \"%s\"",prefs->backend,cfile->handle);
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



void on_preview_spinbutton_changed(LiVESSpinButton *spinbutton, livespointer user_data) {
  // update the play window preview
  int preview_frame=lives_spin_button_get_value_as_int(spinbutton);
  if ((preview_frame)==mainw->preview_frame) return;
  mainw->preview_frame=preview_frame;
  load_preview_image(TRUE);
}


void on_prv_link_toggled(LiVESToggleButton *togglebutton, livespointer user_data) {
  if (!lives_toggle_button_get_active(togglebutton)) return;
  mainw->prv_link=LIVES_POINTER_TO_INT(user_data);
  if (mainw->is_processing&&(mainw->prv_link==PRV_START||mainw->prv_link==PRV_END)) {
    // block spinbutton in play window
    lives_widget_set_sensitive(mainw->preview_spinbutton,FALSE);
  } else {
    lives_widget_set_sensitive(mainw->preview_spinbutton,TRUE);
  }
  load_preview_image(FALSE);
  lives_widget_grab_focus(mainw->preview_spinbutton);
}


void on_spinbutton_start_value_changed(LiVESSpinButton *spinbutton, livespointer user_data) {
  int start,ostart=cfile->start;

  if (mainw->playing_file==-1&&mainw->current_file==0) return;

  if ((start=lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(spinbutton)))==cfile->start) return;
  cfile->start=start;

  if (mainw->selwidth_locked) {
    cfile->end+=cfile->start-ostart;
    if (cfile->end>cfile->frames) {
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_start),cfile->start-cfile->end+cfile->frames);
    } else {
      mainw->selwidth_locked=FALSE;
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_end),cfile->end);
      load_end_image(cfile->end);
      mainw->selwidth_locked=TRUE;
    }
  }

  else {
    if ((cfile->start==1||cfile->end==cfile->frames)&&!(cfile->start==1&&cfile->end==cfile->frames)) {
      lives_widget_set_sensitive(mainw->select_invert,TRUE);
    } else {
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



void on_spinbutton_end_value_changed(LiVESSpinButton *spinbutton, livespointer user_data) {
  int end,oend=cfile->end;

  if (mainw->playing_file==-1&&mainw->current_file==0) return;


  if ((end=lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(spinbutton)))==cfile->end) return;
  cfile->end=end;

  if (mainw->selwidth_locked) {
    cfile->start+=cfile->end-oend;
    if (cfile->start<1) {
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_end),cfile->end-cfile->start+1);
    } else {
      mainw->selwidth_locked=FALSE;
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_start),cfile->start);
      load_start_image(cfile->start);
      mainw->selwidth_locked=TRUE;
    }
  }

  else {
    if ((cfile->start==1||cfile->end==cfile->frames)&&!(cfile->start==1&&cfile->end==cfile->frames)) {
      lives_widget_set_sensitive(mainw->select_invert,TRUE);
    } else {
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
boolean expose_vid_event(LiVESWidget *widget, lives_painter_t *cr, livespointer user_data) {
  LiVESXEventExpose *event=NULL;
  boolean dest_cr=FALSE;
#else
boolean expose_vid_event(LiVESWidget *widget, LiVESXEventExpose *event) {
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


  if (event!=NULL) {
    if (event->count>0) return TRUE;
    ex=event->area.x;
    ey=event->area.y;
    ew=event->area.width;
    eh=event->area.height;
  } else {
    ex=ey=0;
    ew=lives_widget_get_allocation_width(mainw->video_draw);
    eh=lives_widget_get_allocation_height(mainw->video_draw);
  }


  if (mainw->video_drawable!=NULL) {
    // check if a resize happened

    width = lives_painter_image_surface_get_width(mainw->video_drawable);

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

    lives_painter_render_background(mainw->video_draw,cr2,0,0,
                                    lives_widget_get_allocation_width(mainw->video_draw),
                                    lives_widget_get_allocation_height(mainw->video_draw));
    lives_painter_destroy(cr2);
  }

  lives_painter_set_source_surface(cr, mainw->video_drawable,0.,0.);
  lives_painter_rectangle(cr,ex,ey,ew,eh);
  lives_painter_fill(cr);

  if (dest_cr) lives_painter_destroy(cr);

  return TRUE;

}


static void redraw_laudio(lives_painter_t *cr, int ex, int ey, int ew, int eh) {
  int width;

  if (mainw->laudio_drawable!=NULL) {
    // check if a resize happened

    width = lives_painter_image_surface_get_width(mainw->laudio_drawable);

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
    lives_painter_t *cr=lives_painter_create(mainw->laudio_drawable);

    lives_painter_render_background(mainw->laudio_draw,cr,0,0,
                                    lives_widget_get_allocation_width(mainw->video_draw),
                                    lives_widget_get_allocation_height(mainw->video_draw));
    lives_painter_destroy(cr);
  }

  lives_painter_set_source_surface(cr, mainw->laudio_drawable,0.,0.);
  lives_painter_rectangle(cr,ex,ey,ew,eh);
  lives_painter_fill(cr);


}




static void redraw_raudio(lives_painter_t *cr, int ex, int ey, int ew, int eh) {

  int width;

  if (mainw->raudio_drawable!=NULL) {
    // check if a resize happened

    width = lives_painter_image_surface_get_width(mainw->raudio_drawable);

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

  if (mainw->current_file==-1) {
    lives_painter_t *cr=lives_painter_create(mainw->raudio_drawable);
    lives_painter_render_background(mainw->raudio_draw,cr,0,0,
                                    lives_widget_get_allocation_width(mainw->video_draw),
                                    lives_widget_get_allocation_height(mainw->video_draw));
    lives_painter_destroy(cr);

  }



  if (1||mainw->current_file==-1) mainw->blank_raudio_drawable=mainw->raudio_drawable;
  else cfile->raudio_drawable=mainw->raudio_drawable;

  lives_painter_set_source_surface(cr, mainw->raudio_drawable,0.,0.);
  lives_painter_rectangle(cr,ex,ey,ew,eh);
  lives_painter_fill(cr);

}





#if GTK_CHECK_VERSION(3,0,0)
boolean expose_laud_event(LiVESWidget *widget, lives_painter_t *cr, livespointer user_data) {
  LiVESXEventExpose *event=NULL;
  boolean need_cr=TRUE;
#else
boolean expose_laud_event(LiVESWidget *widget, LiVESXEventExpose *event) {
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
  } else {
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
boolean expose_raud_event(LiVESWidget *widget, lives_painter_t *cr, livespointer user_data) {
  LiVESXEventExpose *event=NULL;
  boolean need_cr=FALSE;
#else
boolean expose_raud_event(LiVESWidget *widget, LiVESXEventExpose *event) {
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
  } else {
    ex=ey=0;
    ew=lives_widget_get_allocation_width(mainw->raudio_draw);
    eh=lives_widget_get_allocation_height(mainw->raudio_draw);
  }

  if (need_cr) cr=lives_painter_create_from_widget(mainw->raudio_draw);

  redraw_raudio(cr,ex,ey,ew,eh);

  if (need_cr) lives_painter_destroy(cr);

  return TRUE;

}



boolean config_event(LiVESWidget *widget, LiVESXEventConfigure *event, livespointer user_data) {
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



void on_effects_paused(LiVESButton *button, livespointer user_data) {
  char *com=NULL;
  int64_t xticks;
#ifdef IS_MINGW
  int pid;
#endif

  if (mainw->iochan!=NULL||cfile->opening) {
    // pause during encoding (if we start using mainw->iochan for other things, this will
    // need changing...)

    if (!mainw->effects_paused) {
#ifndef IS_MINGW
      com=lives_strdup_printf("%s stopsubsub \"%s\" SIGTSTP 2>/dev/null",prefs->backend_sync,cfile->handle);
      lives_system(com,TRUE);
#else
      FILE *rfile;
      ssize_t rlen;
      char val[16];

      // get pid from backend
      com=lives_strdup_printf("%s get_pid_for_handle \"%s\"",prefs->backend_sync,cfile->handle);
      rfile=popen(com,"r");
      rlen=fread(val,1,16,rfile);
      pclose(rfile);
      memset(val+rlen,0,1);
      pid=atoi(val);

      lives_win32_suspend_resume_process(pid,TRUE);
#endif
      lives_free(com);
      com=NULL;

      if (!cfile->opening) {
        lives_button_set_label(LIVES_BUTTON(button),_("Resume"));
        lives_label_set_text(LIVES_LABEL(cfile->proc_ptr->label2),_("\nPaused\n(click Resume to continue processing)"));
        d_print(_("paused..."));
      }
    }

    else {
#ifndef IS_MINGW
      com=lives_strdup_printf("%s stopsubsub \"%s\" SIGCONT 2>/dev/null",prefs->backend_sync,cfile->handle);
      lives_system(com,TRUE);
#else
      FILE *rfile;
      ssize_t rlen;
      char val[16];

      // get pid from backend
      com=lives_strdup_printf("%s get_pid_for_handle \"%s\"",prefs->backend_sync,cfile->handle);
      rfile=popen(com,"r");
      rlen=fread(val,1,16,rfile);
      pclose(rfile);
      memset(val+rlen,0,1);
      pid=atoi(val);

      lives_win32_suspend_resume_process(pid,FALSE);
#endif
      lives_free(com);
      com=NULL;

      if (!cfile->opening) {
        lives_button_set_label(LIVES_BUTTON(button),_("Paus_e"));
        lives_label_set_text(LIVES_LABEL(cfile->proc_ptr->label2),_("\nPlease Wait"));
        d_print(_("resumed..."));
      }
    }
  }

  if (mainw->iochan==NULL) {
    // pause during effects processing or opening
#ifdef USE_MONOTONIC_TIME
    xticks=(lives_get_monotonic_time()-mainw->origusecs)*U_SEC_RATIO;
#else
    gettimeofday(&tv, NULL);
    xticks=U_SECL*(tv.tv_sec-mainw->origsecs)+tv.tv_usec*U_SEC_RATIO-mainw->origusecs*U_SEC_RATIO;
#endif
    if (!mainw->effects_paused) {
      mainw->timeout_ticks-=xticks;
      com=lives_strdup_printf("%s pause \"%s\"",prefs->backend_sync,cfile->handle);
      if (!mainw->preview) {
        lives_button_set_label(LIVES_BUTTON(button),_("Resume"));
        if (!cfile->nokeep) {
          char *tmp,*ltext;

          if (!cfile->opening) {
            ltext=lives_strdup(_("Keep"));
          } else {
            ltext=lives_strdup(_("Enough"));
          }
          lives_button_set_label(LIVES_BUTTON(cfile->proc_ptr->cancel_button),ltext);
          lives_label_set_text(LIVES_LABEL(cfile->proc_ptr->label2),
                               (tmp=lives_strdup_printf
                                    (_("\nPaused\n(click %s to keep what you have and stop)\n(click Resume to continue processing)"),
                                     ltext)));
          lives_free(tmp);
          lives_free(ltext);
        }
        d_print(_("paused..."));
      }
#ifdef ENABLE_JACK
      if (mainw->jackd!=NULL&&mainw->jackd_read!=NULL&&mainw->jackd_read->in_use)
        lives_widget_hide(cfile->proc_ptr->stop_button);
#endif
#ifdef HAVE_PULSE_AUDIO
      if (mainw->pulsed!=NULL&&mainw->pulsed_read!=NULL&&mainw->pulsed_read->in_use)
        lives_widget_hide(cfile->proc_ptr->stop_button);
#endif
    } else {
      mainw->timeout_ticks+=xticks;
      com=lives_strdup_printf("%s resume \"%s\"",prefs->backend_sync,cfile->handle);
      if (!mainw->preview) {
        if (cfile->opening||!cfile->nokeep) lives_button_set_label(LIVES_BUTTON(button),_("Pause/_Enough"));
        else lives_button_set_label(LIVES_BUTTON(button),_("Paus_e"));
        lives_button_set_label(LIVES_BUTTON(cfile->proc_ptr->cancel_button), _("Cancel"));
        lives_label_set_text(LIVES_LABEL(cfile->proc_ptr->label2),_("\nPlease Wait"));
        d_print(_("resumed..."));
      }
#ifdef ENABLE_JACK
      if (mainw->jackd!=NULL&&mainw->jackd_read!=NULL&&mainw->jackd_read->in_use)
        lives_widget_show(cfile->proc_ptr->stop_button);
#endif
#ifdef HAVE_PULSE_AUDIO
      if (mainw->pulsed!=NULL&&mainw->pulsed_read!=NULL&&mainw->pulsed_read->in_use)
        lives_widget_show(cfile->proc_ptr->stop_button);
#endif
    }

    if (!cfile->opening&&!mainw->internal_messaging
        && !(
#ifdef ENABLE_JACK
          (mainw->jackd!=NULL&&mainw->jackd_read!=NULL&&mainw->jackd_read->in_use)
#else
          0
#endif
          ||
#ifdef HAVE_PULSE_AUDIO
          (mainw->pulsed!=NULL&&mainw->pulsed_read!=NULL&&mainw->pulsed->in_use)
#else
          0
#endif
        )) {
      lives_system(com,FALSE);
    }
  }
  if (com!=NULL) lives_free(com);
  mainw->effects_paused=!mainw->effects_paused;

}



void on_preview_clicked(LiVESButton *button, livespointer user_data) {
  // play an effect/tool preview
  // IMPORTANT: cfile->undo_start and cfile->undo_end determine which frames
  // should be played

  weed_plant_t *filter_map=mainw->filter_map; // back this up in case we are rendering
  weed_plant_t *afilter_map=mainw->afilter_map; // back this up in case we are rendering
  weed_plant_t *audio_event=mainw->audio_event;

  short oaudp=prefs->audio_player;

  uint64_t old_rte; //TODO - block better
  int64_t xticks;

  static boolean in_preview_func=FALSE;

  boolean resume_after;
  boolean ointernal_messaging=mainw->internal_messaging;

  int ostart=cfile->start;
  int oend=cfile->end;

  int toy_type=mainw->toy_type;

  int current_file=mainw->current_file;

  if (in_preview_func) {
    // this is a special value of cancel - don't propogate it to "open"
    mainw->cancelled=CANCEL_NO_PROPOGATE;
    return;
  }

  in_preview_func=TRUE;

  mainw->preview=TRUE;
  old_rte=mainw->rte;
#ifdef USE_MONOTONIC_TIME
  xticks=(lives_get_monotonic_time()-mainw->origusecs)*U_SEC_RATIO;
#else
  gettimeofday(&tv, NULL);
  xticks=U_SECL*(tv.tv_sec-mainw->origsecs)+tv.tv_usec*U_SEC_RATIO-mainw->origusecs*U_SEC_RATIO;
#endif
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
      if (mainw->multitrack==NULL&&prefs->show_gui) lives_widget_show(mainw->LiVES);

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
        lives_widget_set_sensitive(mainw->m_playbutton, FALSE);
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
      } else {
        mainw->play_start=1;
        mainw->play_end=INT_MAX;
      }
    } else {
      if (!mainw->is_processing&&!mainw->is_rendering) {
        mainw->play_start=cfile->start=cfile->undo_start;
        mainw->play_end=cfile->end=cfile->undo_end;
      } else {
        mainw->play_start=calc_frame_from_time(mainw->current_file,event_list_get_start_secs(cfile->event_list));
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
      lives_widget_set_sensitive(mainw->showfct,FALSE);
    }

    desensitize();

    if (cfile->opening||cfile->opening_only_audio) {
      lives_widget_hide(cfile->proc_ptr->processing);
      if (mainw->multitrack==NULL&&!cfile->opening_audio) {
        load_start_image(0);
        load_end_image(0);
      }
      resize(1);
    }

    if (ointernal_messaging) {
      lives_system("sync;sync;sync",TRUE);
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
      } else switch_to_file((mainw->current_file=0),current_file);
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
      lives_widget_show(cfile->proc_ptr->processing);
      lives_button_set_label(LIVES_BUTTON(button),_("Preview"));
      lives_widget_set_sensitive(cfile->proc_ptr->pause_button,TRUE);
      lives_widget_set_sensitive(cfile->proc_ptr->cancel_button,TRUE);
    }
    mainw->preview=FALSE;
    desensitize();
    procw_desensitize();

    if (!cfile->opening) {
      lives_widget_set_sensitive(mainw->showfct,TRUE);
    } else {
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

  if (mainw->preview_box!=NULL) lives_widget_set_tooltip_text(mainw->p_playbutton, _("Preview"));
  lives_widget_set_tooltip_text(mainw->m_playbutton,_("Preview"));

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
    lives_widget_queue_draw(mainw->LiVES);
  }
#ifdef USE_MONOTONIC_TIME
  xticks=(lives_get_monotonic_time()-mainw->origusecs)*U_SEC_RATIO;
#else
  gettimeofday(&tv, NULL);
  xticks=U_SECL*(tv.tv_sec-mainw->origsecs)+tv.tv_usec*U_SEC_RATIO-mainw->origusecs*U_SEC_RATIO;
#endif
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


void changed_fps_during_pb(LiVESSpinButton   *spinbutton, livespointer user_data) {
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
    freeze_callback(NULL,NULL,0,(LiVESXModifierType)0,NULL);
    return;
  }

  if (cfile->pb_fps==0.) {
    freeze_callback(NULL,NULL,0,(LiVESXModifierType)0,NULL);
    return;
  }

}


boolean on_mouse_scroll(LiVESWidget *widget, LiVESXEventScroll *event, livespointer user_data) {
  LiVESXModifierType kstate;
  uint32_t type=1;

  if (!mainw->interactive) return FALSE;

  if (!prefs->mouse_scroll_clips||mainw->noswitch) return FALSE;

  if (mainw->multitrack!=NULL) {
    if (event->direction==LIVES_SCROLL_UP) mt_prevclip(NULL,NULL,0,(LiVESXModifierType)0,user_data);
    else if (event->direction==LIVES_SCROLL_DOWN) mt_nextclip(NULL,NULL,0,(LiVESXModifierType)0,user_data);
    return FALSE;
  }

  kstate=(LiVESXModifierType)event->state;

  if (kstate==LIVES_SHIFT_MASK) type=2; // bg
  else if (kstate==LIVES_CONTROL_MASK) type=0; // fg or bg

  if (event->direction==LIVES_SCROLL_UP) prevclip_callback(NULL,NULL,0,(LiVESXModifierType)0,LIVES_INT_TO_POINTER(type));
  else if (event->direction==LIVES_SCROLL_DOWN) nextclip_callback(NULL,NULL,0,(LiVESXModifierType)0,LIVES_INT_TO_POINTER(type));
  return FALSE;
}



// next few functions are for the timer bars
boolean on_mouse_sel_update(LiVESWidget *widget, LiVESXEventMotion *event, livespointer user_data) {

  if (!mainw->interactive) return FALSE;

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
    } else if (mainw->sel_move==SEL_MOVE_END||(mainw->sel_move==SEL_MOVE_AUTO&&sel_current>mainw->sel_start)) {
      sel_current=calc_frame_from_time2(mainw->current_file,
                                        (double)x/(double)lives_widget_get_allocation_width(mainw->vidbar)*cfile->total_time);
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_end),sel_current-1);
    }
  }
  return FALSE;
}


boolean on_mouse_sel_reset(LiVESWidget *widget, LiVESXEventButton *event, livespointer user_data) {

  if (!mainw->interactive) return FALSE;

  if (mainw->current_file<=0) return FALSE;
  mainw->sel_start=0;
  if (!mainw->mouse_blocked) {
    lives_signal_handler_block(mainw->eventbox2,mainw->mouse_fn1);
    mainw->mouse_blocked=TRUE;
  }
  return FALSE;
}


boolean on_mouse_sel_start(LiVESWidget *widget, LiVESXEventButton *event, livespointer user_data) {
  int x;

  if (!mainw->interactive) return FALSE;

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
        } else {
          mainw->sel_start=calc_frame_from_time2(mainw->current_file,
                                                 (double)x/(double)lives_widget_get_allocation_width(mainw->vidbar)*cfile->total_time);
          lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_end),mainw->sel_start-1);
          mainw->sel_move=SEL_MOVE_END;
        }
      } else {
        // locked selection
        if (mainw->sel_start>cfile->end) {
          // past end
          if (cfile->end+cfile->end-cfile->start+1<=cfile->frames) {
            lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_end),cfile->end+cfile->end-cfile->start+1);
            mainw->sel_move=SEL_MOVE_START;
          }
        } else {
          if (mainw->sel_start>=cfile->start) {
            if (mainw->sel_start>cfile->start+(cfile->end-cfile->start+1)/2) {
              // nearer to end
              lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_end),mainw->sel_start);
              mainw->sel_move=SEL_MOVE_END;
            } else {
              // nearer to start
              lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_start),mainw->sel_start);
              mainw->sel_move=SEL_MOVE_START;
            }
          } else {
            // before start
            if (cfile->start-cfile->end+cfile->start-1>=1) {
              lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_end),cfile->start-1);
              mainw->sel_move=SEL_MOVE_END;
            }
          }
        }
      }
    }
  }
  if (mainw->mouse_blocked) {// stops a warning if the user clicks around a lot...
    lives_signal_handler_unblock(mainw->eventbox2,mainw->mouse_fn1);
    mainw->mouse_blocked=FALSE;
  }
  return FALSE;
}


boolean on_hrule_enter(LiVESWidget *widget, LiVESXEventCrossing *event, livespointer user_data) {
  if (!mainw->interactive) return FALSE;

  if (mainw->cursor_style!=LIVES_CURSOR_NORMAL) return FALSE;
  lives_set_cursor_style(LIVES_CURSOR_CENTER_PTR,widget);
  return FALSE;
}


boolean on_hrule_update(LiVESWidget *widget, LiVESXEventMotion *event, livespointer user_data) {
  int x;

  if (!mainw->interactive) return FALSE;

  if (mainw->current_file<=0) return FALSE;

  lives_widget_get_pointer((LiVESXDevice *)mainw->mgeom[prefs->gui_monitor>0?prefs->gui_monitor-1:0].mouse_device,
                           mainw->LiVES, &x, NULL);
  if (x<0) x=0;

  // figure out where ptr should be even when > cfile->frames

  if ((lives_ruler_set_value(LIVES_RULER(mainw->hruler),(cfile->pointer_time=
                               calc_time_from_frame(mainw->current_file,calc_frame_from_time
                                   (mainw->current_file,(double)x/lives_widget_get_allocation_width(mainw->vidbar)*cfile->total_time)))))<=0.)
    lives_ruler_set_value(LIVES_RULER(mainw->hruler),(cfile->pointer_time=(double)x/
                          lives_widget_get_allocation_width(mainw->vidbar)*cfile->total_time));
  lives_widget_queue_draw(mainw->hruler);
  get_play_times();
  return FALSE;
}


boolean on_hrule_reset(LiVESWidget *widget, LiVESXEventButton  *event, livespointer user_data) {
  //button release
  int x;

  if (!mainw->interactive) return FALSE;

  if (mainw->current_file<=0) return FALSE;

  lives_widget_get_pointer((LiVESXDevice *)mainw->mgeom[prefs->gui_monitor>0?prefs->gui_monitor-1:0].mouse_device,
                           mainw->LiVES, &x, NULL);
  if (x<0) x=0;
  if ((lives_ruler_set_value(LIVES_RULER(mainw->hruler),(cfile->pointer_time=
                               calc_time_from_frame(mainw->current_file,
                                   calc_frame_from_time(mainw->current_file,(double)x/
                                       lives_widget_get_allocation_width(mainw->vidbar)*cfile->total_time)))))<=0.)
    lives_ruler_set_value(LIVES_RULER(mainw->hruler),(cfile->pointer_time=(double)x/
                          lives_widget_get_allocation_width(mainw->vidbar)*cfile->total_time));

  if (!mainw->hrule_blocked) {
    lives_signal_handler_block(mainw->eventbox5,mainw->hrule_func);
    mainw->hrule_blocked=TRUE;
  }
  if (cfile->pointer_time>0.) {
    lives_widget_set_sensitive(mainw->rewind, TRUE);
    lives_widget_set_sensitive(mainw->trim_to_pstart, (cfile->achans*cfile->frames>0));
    lives_widget_set_sensitive(mainw->m_rewindbutton, TRUE);
    if (mainw->preview_box!=NULL) {
      lives_widget_set_sensitive(mainw->p_rewindbutton, TRUE);
    }
  } else {
    lives_widget_set_sensitive(mainw->rewind, FALSE);
    lives_widget_set_sensitive(mainw->trim_to_pstart, FALSE);
    lives_widget_set_sensitive(mainw->m_rewindbutton, FALSE);
    if (mainw->preview_box!=NULL) {
      lives_widget_set_sensitive(mainw->p_rewindbutton, FALSE);
    }
  }
  lives_widget_queue_draw(mainw->hruler);
  get_play_times();
  return FALSE;
}


boolean on_hrule_set(LiVESWidget *widget, LiVESXEventButton *event, livespointer user_data) {
  // button press
  int x;
  int frame;

  if (!mainw->interactive) return FALSE;

  if (mainw->current_file<=0) return FALSE;

  lives_widget_get_pointer((LiVESXDevice *)mainw->mgeom[prefs->gui_monitor>0?prefs->gui_monitor-1:0].mouse_device,
                           mainw->LiVES, &x, NULL);
  if (x<0) x=0;

  frame=calc_frame_from_time(mainw->current_file,(double)x/lives_widget_get_allocation_width(mainw->vidbar)*cfile->total_time);

  if ((lives_ruler_set_value(LIVES_RULER(mainw->hruler),(cfile->pointer_time=
                               calc_time_from_frame(mainw->current_file,frame))))<=0.)
    lives_ruler_set_value(LIVES_RULER(mainw->hruler),(cfile->pointer_time=(double)x/
                          lives_widget_get_allocation_width(mainw->vidbar)*cfile->total_time));
  lives_widget_queue_draw(mainw->hruler);
  get_play_times();
  if (mainw->hrule_blocked) {
    lives_signal_handler_unblock(mainw->eventbox5,mainw->hrule_func);
    mainw->hrule_blocked=FALSE;
  }

  if (mainw->playing_file==-1&&mainw->play_window!=NULL&&cfile->is_loaded) {
    if (mainw->prv_link==PRV_PTR&&mainw->preview_frame!=frame)
      load_preview_image(FALSE);
  }

  return FALSE;
}



boolean frame_context(LiVESWidget *widget, LiVESXEventButton *event, livespointer which) {
  //popup a context menu when we right click on a frame

  LiVESWidget *save_frame_as;
  LiVESWidget *menu;

  int frame=0;

  if (!mainw->interactive) return FALSE;

  // check if a file is loaded
  if (mainw->current_file<=0) return FALSE;

  if (mainw->multitrack!=NULL && mainw->multitrack->event_list==NULL) return FALSE;

  // only accept right mouse clicks

  if (event->button!=3) return FALSE;

  if (mainw->multitrack==NULL) {
    switch (LIVES_POINTER_TO_INT(which)) {
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
  lives_menu_set_title(LIVES_MENU(menu),_("Selected Frame"));

  if (palette->style&STYLE_1) {
    lives_widget_set_bg_color(menu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
    lives_widget_set_fg_color(menu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars_fore);
  }

  if (cfile->frames>0||mainw->multitrack!=NULL) {
    save_frame_as = lives_menu_item_new_with_mnemonic(_("_Save frame as..."));
    lives_signal_connect(LIVES_GUI_OBJECT(save_frame_as), LIVES_WIDGET_ACTIVATE_SIGNAL,
                         LIVES_GUI_CALLBACK(save_frame),
                         LIVES_INT_TO_POINTER(frame));


    if (capable->has_convert&&capable->has_composite)
      lives_container_add(LIVES_CONTAINER(menu), save_frame_as);

  }

  lives_widget_show_all(menu);
  lives_menu_popup(LIVES_MENU(menu), event);

  return FALSE;
}



void on_slower_pressed(LiVESButton *button, livespointer user_data) {
  double change=1.,new_fps;

  int type=0;

  lives_clip_t *sfile=cfile;

  if (user_data!=NULL) {
    type=LIVES_POINTER_TO_INT(user_data);
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

  change*=PB_CHANGE_RATE*sfile->pb_fps;

  if (sfile->pb_fps==0.) return;
  if (sfile->pb_fps>0.) {
    if (sfile->pb_fps<0.1||sfile->pb_fps<change) sfile->pb_fps=change;
    new_fps=sfile->pb_fps-change;
    if (sfile==cfile) lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_pb_fps),new_fps);
    else sfile->pb_fps=new_fps;
  } else {
    if (sfile->pb_fps>change) sfile->pb_fps=change;
    if (sfile==cfile) lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_pb_fps),(sfile->pb_fps-change));
    else sfile->pb_fps-=change;
  }



}


void on_faster_pressed(LiVESButton *button, livespointer user_data) {
  double change=1.;
  int type=0;

  lives_clip_t *sfile=cfile;

  if (user_data!=NULL) {
    type=LIVES_POINTER_TO_INT(user_data);
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
  } else {
    if (sfile->pb_fps==-FPS_MAX) return;
    if (sfile->pb_fps<-FPS_MAX-change) sfile->pb_fps=-FPS_MAX-change;
    if (sfile==cfile) lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_pb_fps),(sfile->pb_fps+change));
    else sfile->pb_fps=sfile->pb_fps+change;
  }

}



void on_back_pressed(LiVESButton *button, livespointer user_data) {
  double change_speed=cfile->pb_fps*(double)KEY_RPT_INTERVAL*PB_SCRATCH_VALUE;

  if (mainw->playing_file==-1||mainw->internal_messaging||(mainw->is_processing&&cfile->is_loaded)) return;
  if (mainw->record&&!(prefs->rec_opts&REC_FRAMES)) return;
  if (cfile->next_event!=NULL) return;

  mainw->deltaticks-=(int64_t)(change_speed*3.*mainw->period);
  mainw->scratch=SCRATCH_BACK;

}

void on_forward_pressed(LiVESButton *button, livespointer user_data) {
  double change_speed=cfile->pb_fps*(double)KEY_RPT_INTERVAL*PB_SCRATCH_VALUE;

  if (mainw->playing_file==-1||mainw->internal_messaging||(mainw->is_processing&&cfile->is_loaded)) return;
  if (mainw->record&&!(prefs->rec_opts&REC_FRAMES)) return;
  if (cfile->next_event!=NULL) return;

  mainw->deltaticks+=(int64_t)(change_speed*mainw->period);
  mainw->scratch=SCRATCH_FWD;

}


boolean freeze_callback(LiVESAccelGroup *group, LiVESObject *obj, uint32_t keyval, LiVESXModifierType mod, livespointer user_data) {
  if (mainw->playing_file==-1||(mainw->is_processing&&cfile->is_loaded)) return TRUE;
  if (mainw->record&&!(prefs->rec_opts&REC_FRAMES)) return TRUE;

  if (group!=NULL) mainw->rte_keys=-1;

  if (cfile->play_paused) {
    cfile->pb_fps=cfile->freeze_fps;
    if (cfile->pb_fps!=0.) mainw->period=U_SEC/cfile->pb_fps;
    else mainw->period=INT_MAX;
    cfile->play_paused=FALSE;
  } else {
    cfile->freeze_fps=cfile->pb_fps;
    cfile->play_paused=TRUE;
    cfile->pb_fps=0.;
    mainw->deltaticks=0;
    if (!mainw->noswitch&&(cfile->clip_type==CLIP_TYPE_DISK||cfile->clip_type==CLIP_TYPE_FILE)) {
      weed_plant_t *frame_layer=mainw->frame_layer;
      mainw->frame_layer=NULL;
      load_frame_image(cfile->frameno);
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


boolean nervous_callback(LiVESAccelGroup *group, LiVESObject *obj, uint32_t keyval, LiVESXModifierType mod, livespointer clip_number) {
  if (mainw->multitrack!=NULL) return FALSE;
  mainw->nervous=!mainw->nervous;
  return TRUE;
}


boolean show_sync_callback(LiVESAccelGroup *group, LiVESObject *obj, uint32_t keyval, LiVESXModifierType mod, livespointer clip_number) {
  double avsync;

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
  } else return FALSE;

  avsync-=(mainw->actual_frame-1.)/cfile->fps;

  last_dprint_file=mainw->last_dprint_file;
  mainw->no_switch_dprint=TRUE;
  d_print(_("Audio is ahead of video by %.4f secs. at frame %d, with fps %.4f\n"),
          avsync,mainw->actual_frame,cfile->pb_fps);
  mainw->no_switch_dprint=FALSE;
  mainw->last_dprint_file=last_dprint_file;
  return TRUE;
}



boolean storeclip_callback(LiVESAccelGroup *group, LiVESObject *obj, uint32_t keyval, LiVESXModifierType mod, livespointer clip_number) {
  // ctrl-fn key will store a clip for higher switching
  int clip=LIVES_POINTER_TO_INT(clip_number)-1;
  register int i;

  if (!mainw->interactive) return TRUE;

  if (clip>=FN_KEYS-1) {
    // last fn key will clear all
    for (i=0; i<FN_KEYS-1; i++) {
      mainw->clipstore[i]=0;
    }
    return TRUE;
  }

  if (mainw->clipstore[clip]<1||mainw->files[mainw->clipstore[clip]]==NULL) {
    mainw->clipstore[clip]=mainw->current_file;
  } else {
    switch_clip(0,mainw->clipstore[clip],FALSE);
  }
  return TRUE;
}



void on_toolbar_hide(LiVESButton *button, livespointer user_data) {
  lives_widget_hide(mainw->tb_hbox);
  fullscreen_internal();
  future_prefs->show_tool=FALSE;
}





void on_capture_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  int curr_file=mainw->current_file;
  char *com;
  char **array;
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
      lives_source_remove(mainw->multitrack->idlefunc);
      mainw->multitrack->idlefunc=0;
    }
    mt_desensitise(mainw->multitrack);
  }

  if (prefs->rec_desktop_audio&&((prefs->audio_player==AUD_PLAYER_JACK&&capable->has_jackd)||
                                 (prefs->audio_player==AUD_PLAYER_PULSE&&capable->has_pulse_audio))) {
    resaudw=create_resaudw(8,NULL,NULL);
  } else {
    resaudw=create_resaudw(9,NULL,NULL);
  }
  response=lives_dialog_run(LIVES_DIALOG(resaudw->dialog));

  if (response!=LIVES_RESPONSE_OK) {
    lives_widget_destroy(resaudw->dialog);

    if (mainw->multitrack!=NULL) {
      mt_sensitise(mainw->multitrack);
      mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
    }

    return;
  }

  if (prefs->rec_desktop_audio&&((prefs->audio_player==AUD_PLAYER_JACK&&capable->has_jackd)||
                                 (prefs->audio_player==AUD_PLAYER_PULSE&&capable->has_pulse_audio))) {
    mainw->rec_arate=(int)atoi(lives_entry_get_text(LIVES_ENTRY(resaudw->entry_arate)));
    mainw->rec_achans=(int)atoi(lives_entry_get_text(LIVES_ENTRY(resaudw->entry_achans)));
    mainw->rec_asamps=(int)atoi(lives_entry_get_text(LIVES_ENTRY(resaudw->entry_asamps)));

    if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(resaudw->rb_unsigned))) {
      mainw->rec_signed_endian=AFORM_UNSIGNED;
    }
    if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(resaudw->rb_bigend))) {
      mainw->rec_signed_endian|=AFORM_BIG_ENDIAN;
    }
  } else {
    mainw->rec_arate=mainw->rec_achans=mainw->rec_asamps=mainw->rec_signed_endian=0;
  }

  mainw->rec_fps=lives_spin_button_get_value(LIVES_SPIN_BUTTON(resaudw->fps_spinbutton));

  if (!lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(resaudw->unlim_radiobutton))) {
    rec_end_time=(lives_spin_button_get_value(LIVES_SPIN_BUTTON(resaudw->hour_spinbutton))*60.
                  +lives_spin_button_get_value(LIVES_SPIN_BUTTON(resaudw->minute_spinbutton)))*60.
                 +lives_spin_button_get_value(LIVES_SPIN_BUTTON(resaudw->second_spinbutton));
    mainw->rec_vid_frames=(rec_end_time*mainw->rec_fps+.5);
  } else mainw->rec_vid_frames=-1;

  lives_widget_destroy(resaudw->dialog);
  lives_widget_context_update();
  if (resaudw!=NULL) lives_free(resaudw);
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

  if (!(do_warning_dialog(
          _("Capture an External Window:\n\nClick on 'OK', then click on any window to capture it\nClick 'Cancel' to cancel\n\n")))) {
    if (prefs->show_gui) {
      if (mainw->multitrack==NULL) lives_widget_show(mainw->LiVES);
      else lives_widget_show(mainw->multitrack->window);
    }
    d_print(_("External window was released.\n"));
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

  com=lives_strdup_printf("%s get_window_id \"%s\"",prefs->backend,cfile->handle);
  mainw->com_failed=FALSE;
  lives_system(com,FALSE);
  lives_free(com);

  if (mainw->com_failed) {
    if (prefs->show_gui) lives_widget_show(mainw->LiVES);
    lives_widget_context_update();
    if (mainw->multitrack!=NULL) {
      mt_sensitise(mainw->multitrack);
      mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
    }
    return;
  }

  do_progress_dialog(TRUE,FALSE,_("Click on a Window to Capture it\nPress 'q' to stop recording"));

  if (get_token_count(mainw->msg,'|')<6) {
    if (prefs->show_gui) lives_widget_show(mainw->LiVES);
    lives_widget_context_update();
    if (mainw->multitrack!=NULL) {
      mt_sensitise(mainw->multitrack);
      mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
    }
    return;
  }

  array=lives_strsplit(mainw->msg,"|",-1);
#if GTK_CHECK_VERSION(3,0,0) || defined GUI_QT
  mainw->foreign_id=(Window)atoi(array[1]);
#else
  mainw->foreign_id=(GdkNativeWindow)atoi(array[1]);
#endif
  mainw->foreign_width=atoi(array[2]);
  mainw->foreign_height=atoi(array[3]);
  mainw->foreign_bpp=atoi(array[4]);
  mainw->foreign_visual=lives_strdup(array[5]);
  lives_strfreev(array);

  com=lives_strdup_printf("%s close \"%s\"",prefs->backend,cfile->handle);
  lives_system(com,TRUE);
  lives_free(com);
  lives_free(cfile);
  cfile=NULL;
  if (mainw->first_free_file==-1||mainw->first_free_file>mainw->current_file) mainw->first_free_file=mainw->current_file;

  mainw->current_file=curr_file;
  ////////////////////////////////////////

  d_print(_("\nExternal window captured. Width=%d, height=%d, bpp=%d. *Do not resize*\n\nStop or 'q' to finish.\n(Default of %.3f frames per second will be used.)\n"),
          mainw->foreign_width,mainw->foreign_height,mainw->foreign_bpp,mainw->rec_fps);

  // start another copy of LiVES and wait for it to return values
  com=lives_strdup_printf("%s -capture %d %u %d %d %s %d %d %.4f %d %d %d %d \"%s\"",capable->myname_full,capable->mainpid,
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
    do_error_dialog(_("LiVES was unable to capture this window. Sorry.\n"));
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
  char *capfilename=lives_strdup_printf(".capture.%d",mainw->foreign_key);
  char *capfile=lives_build_filename(prefs->tmpdir,capfilename,NULL);

  char buf[32];

  boolean retval;
  int capture_fd;
  register int i;


  retval=prepare_to_play_foreign();

  if (mainw->foreign_visual!=NULL) lives_free(mainw->foreign_visual);

  if (!retval) exit(2);

  mainw->record_foreign=TRUE;  // for now...

  play_file();

  // pass the handle and frames back to the caller
  capture_fd=creat(capfile,S_IRUSR|S_IWUSR);
  if (capture_fd<0) {
    lives_free(capfile);
    exit(1);
  }

  for (i=1; i<MAX_FILES; i++) {
    if (mainw->files[i]==NULL) break;
    lives_write(capture_fd,mainw->files[i]->handle,strlen(mainw->files[i]->handle),TRUE);
    lives_write(capture_fd,"|",1,TRUE);
    lives_snprintf(buf,32,"%d",cfile->frames);
    lives_write(capture_fd,buf,strlen(buf),TRUE);
    lives_write(capture_fd,"|",1,TRUE);
  }

  close(capture_fd);
  lives_free(capfilename);
  lives_free(capfile);
  exit(0);

}




// TODO - move all encoder related stuff from here and plugins.c into encoders.c
void on_encoder_ofmt_changed(LiVESCombo *combo, livespointer user_data) {
  // change encoder format in the encoder plugin
  render_details *rdet = (render_details *)user_data;

  LiVESList *ofmt_all=NULL;

  char **array;
  char *new_fmt;

  int counter;
  register int i;

  if (rdet == NULL) {
    new_fmt = lives_combo_get_active_text(LIVES_COMBO(prefsw->ofmt_combo));
  } else {
    new_fmt = lives_combo_get_active_text(LIVES_COMBO(rdet->ofmt_combo));
  }

  if (!strlen(new_fmt) || !strcmp(new_fmt, mainw->string_constants[LIVES_STRING_CONSTANT_ANY])) {
    lives_free(new_fmt);
    return;
  }

  if ((ofmt_all=plugin_request_by_line(PLUGIN_ENCODERS,future_prefs->encoder.name,"get_formats"))!=NULL) {
    // get details for the current format
    counter = 0;
    for (i=0; i<lives_list_length(ofmt_all); i++) {
      if (get_token_count((char *)lives_list_nth_data(ofmt_all,i),'|')>2) {
        array=lives_strsplit((char *)lives_list_nth_data(ofmt_all,i),"|",-1);

        if (!strcmp(array[1],new_fmt)) {
          if (prefsw!=NULL) {
            lives_signal_handler_block(prefsw->ofmt_combo, prefsw->encoder_ofmt_fn);
            lives_combo_set_active_index(LIVES_COMBO(prefsw->ofmt_combo), counter);
            lives_signal_handler_unblock(prefsw->ofmt_combo, prefsw->encoder_ofmt_fn);
          }
          if (rdet!=NULL) {
            lives_signal_handler_block(rdet->ofmt_combo, rdet->encoder_ofmt_fn);
            lives_combo_set_active_index(LIVES_COMBO(rdet->ofmt_combo), counter);
            lives_signal_handler_unblock(rdet->ofmt_combo, rdet->encoder_ofmt_fn);
          }
          lives_snprintf(future_prefs->encoder.of_name,51,"%s",array[0]);
          lives_snprintf(future_prefs->encoder.of_desc,128,"%s",array[1]);

          future_prefs->encoder.of_allowed_acodecs=atoi(array[2]);
          lives_snprintf(future_prefs->encoder.of_restrict,1024,"%s",array[3]);
          lives_strfreev(array);
          break;
        }
        lives_strfreev(array);
        counter++;
      }
    }
    if (ofmt_all!=NULL) {
      lives_list_free_strings(ofmt_all);
      lives_list_free(ofmt_all);
    }
    lives_free(new_fmt);

    if (rdet!=NULL&&prefsw==NULL) {
      if (strcmp(prefs->encoder.of_name,future_prefs->encoder.of_name)) {
        rdet->enc_changed=TRUE;
        lives_snprintf(prefs->encoder.of_name,51,"%s",future_prefs->encoder.of_name);
        lives_snprintf(prefs->encoder.of_desc,128,"%s",future_prefs->encoder.of_desc);
        lives_snprintf(prefs->encoder.of_restrict,1024,"%s",future_prefs->encoder.of_restrict);
        prefs->encoder.of_allowed_acodecs=future_prefs->encoder.of_allowed_acodecs;
        set_pref("output_type",prefs->encoder.of_name);
      }
    }
    set_acodec_list_from_allowed(prefsw,rdet);
  } else {
    do_plugin_encoder_error(future_prefs->encoder.name);
  }
}







// TODO - move all this to audio.c

void on_export_audio_activate(LiVESMenuItem *menuitem, livespointer user_data) {

  char *filt[]= {"*.wav",NULL};
  char *filename,*file_name;
  char *com,*tmp;

  double start,end;

  int nrate=cfile->arps;
  int asigned=!(cfile->signed_endian&AFORM_UNSIGNED);

  if (cfile->end>0&&!LIVES_POINTER_TO_INT(user_data)) {
    filename = choose_file(strlen(mainw->audio_dir)?mainw->audio_dir:NULL,NULL,
                           filt,LIVES_FILE_CHOOSER_ACTION_SAVE,_("Export Selected Audio as..."),NULL);
  } else {
    filename = choose_file(strlen(mainw->audio_dir)?mainw->audio_dir:NULL,NULL,
                           filt,LIVES_FILE_CHOOSER_ACTION_SAVE,_("Export Audio as..."),NULL);
  }

  if (filename==NULL) return;
  file_name=ensure_extension(filename,".wav");
  lives_free(filename);

  if (!check_file(file_name,FALSE)) {
    lives_free(file_name);
    return;
  }

  // warn if arps!=arate
  if ((prefs->audio_player==AUD_PLAYER_SOX||is_realtime_aplayer(prefs->audio_player))&&cfile->arate!=cfile->arps) {
    if (do_warning_dialog(
          _("\n\nThe audio playback speed has been altered for this clip.\nClick 'OK' to export at the new speed, or 'Cancel' to export at the original rate.\n"))) {
      nrate=cfile->arate;
    }
  }

  if (cfile->start*cfile->end>0&&!LIVES_POINTER_TO_INT(user_data)) {
    lives_snprintf(mainw->msg,256,_("Exporting audio frames %d to %d as %s..."),cfile->start,cfile->end,file_name);
    start=calc_time_from_frame(mainw->current_file,cfile->start);
    end=calc_time_from_frame(mainw->current_file,cfile->end);
  } else {
    lives_snprintf(mainw->msg,256,_("Exporting audio as %s..."),file_name);
    start=0.;
    end=0.;
  }

  d_print(mainw->msg);

  com=lives_strdup_printf("%s export_audio \"%s\" %.8f %.8f %d %d %d %d %d \"%s\"",prefs->backend,cfile->handle,
                          start,end,cfile->arps,cfile->achans,cfile->asampsize,asigned,nrate,
                          (tmp=lives_filename_from_utf8(file_name,-1,NULL,NULL,NULL)));
  lives_free(tmp);

  lives_rm(cfile->info_file);
  mainw->com_failed=FALSE;
  lives_system(com,FALSE);
  lives_free(com);

  if (mainw->com_failed) {
    lives_free(file_name);
    d_print_failed();
    return;
  }

  cfile->op_dir=lives_filename_from_utf8((tmp=get_dir(file_name)),-1,NULL,NULL,NULL);
  lives_free(tmp);

  do_progress_dialog(TRUE, FALSE, _("Exporting audio"));

  if (mainw->error) {
    d_print_failed();
    do_error_dialog(mainw->msg);
  } else {
    d_print_done();
    lives_snprintf(mainw->audio_dir,PATH_MAX,"%s",file_name);
    get_dirname(mainw->audio_dir);
  }
  lives_free(file_name);
}


void on_append_audio_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  LiVESWidget *chooser;

  char **const filt=LIVES_AUDIO_LOAD_FILTER;

  char *com,*tmp,*tmp2;
  char *a_type;

  boolean gotit=FALSE;

  int asigned=!(cfile->signed_endian&AFORM_UNSIGNED);
  int aendian=!(cfile->signed_endian&AFORM_BIG_ENDIAN);

  int resp;

  register int i;

  if (!(prefs->warning_mask&WARN_MASK_LAYOUT_ALTER_AUDIO)&&
      (mainw->xlays=layout_audio_is_affected(mainw->current_file,0.))!=NULL) {
    if (!do_layout_alter_audio_warning()) {
      lives_list_free_strings(mainw->xlays);
      lives_list_free(mainw->xlays);
      mainw->xlays=NULL;
      return;
    }
    add_lmap_error(LMAP_ERROR_ALTER_AUDIO,cfile->name,(livespointer)cfile->layout_map,mainw->current_file,0,0.,
                   cfile->stored_layout_audio>0.);
    lives_list_free_strings(mainw->xlays);
    lives_list_free(mainw->xlays);
    mainw->xlays=NULL;
  }

  chooser=choose_file_with_preview(strlen(mainw->audio_dir)?mainw->audio_dir:NULL,_("Append Audio File"),filt,
                                   LIVES_FILE_SELECTION_AUDIO_ONLY);

  resp=lives_dialog_run(LIVES_DIALOG(chooser));

  end_fs_preview();
  mainw->fs_playarea=NULL;

  if (resp!=LIVES_RESPONSE_ACCEPT) {
    on_filechooser_cancel_clicked(chooser);
    return;
  }

  lives_snprintf(file_name,PATH_MAX,"%s",(tmp=lives_filename_to_utf8((tmp2=lives_file_chooser_get_filename(LIVES_FILE_CHOOSER(chooser))),
                                          -1,NULL,NULL,NULL)));
  lives_free(tmp);
  lives_free(tmp2);

  lives_widget_destroy(LIVES_WIDGET(chooser));

  lives_widget_queue_draw(mainw->LiVES);
  lives_widget_context_update();

  lives_snprintf(mainw->audio_dir,PATH_MAX,"%s",file_name);
  get_dirname(mainw->audio_dir);

  a_type=get_extension(file_name);

  if (strlen(a_type)) {
    char **filt=LIVES_AUDIO_LOAD_FILTER;
    for (i=0; filt[i]!=NULL; i++) {
      if (!lives_ascii_strcasecmp(a_type,filt[i]+2)) gotit=TRUE; // skip past "*." in filt
    }
  }

  if (gotit) {
    com=lives_strdup_printf("%s append_audio \"%s\" \"%s\" %d %d %d %d %d \"%s\"",prefs->backend,cfile->handle,
                            a_type,cfile->arate,
                            cfile->achans,cfile->asampsize,asigned,aendian,
                            (tmp=lives_filename_from_utf8(file_name,-1,NULL,NULL,NULL)));
    lives_free(tmp);
  } else {
    do_audio_import_error();
    if (mainw->multitrack!=NULL) {
      mt_sensitise(mainw->multitrack);
      mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
    }
    return;
  }

  lives_snprintf(mainw->msg,256,_("Appending audio file %s..."),file_name);
  d_print(""); // force switchtext
  d_print(mainw->msg);

  lives_rm(cfile->info_file);
  mainw->com_failed=FALSE;
  lives_system(com,FALSE);
  lives_free(com);

  if (mainw->com_failed) {
    if (mainw->multitrack!=NULL) {
      mt_sensitise(mainw->multitrack);
      mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
    }
    return;
  }

  if (!do_progress_dialog(TRUE, TRUE,_("Appending audio"))) {
    lives_widget_queue_draw(mainw->LiVES);
    lives_widget_context_update();
    mainw->cancelled=CANCEL_NONE;
    mainw->error=FALSE;
    mainw->com_failed=FALSE;
    lives_rm(cfile->info_file);
    com=lives_strdup_printf("%s cancel_audio \"%s\"",prefs->backend,cfile->handle);
    lives_system(com,FALSE);
    if (!mainw->com_failed) {
      do_auto_dialog(_("Cancelling"),0);
      check_backend_return(cfile);
    }
    lives_free(com);
    reget_afilesize(mainw->current_file);
    get_play_times();
    if (mainw->error) d_print_failed();
    if (mainw->multitrack!=NULL) {
      mt_sensitise(mainw->multitrack);
      mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
    }
    return;
  }

  if (mainw->error) {
    d_print_failed();
    do_error_dialog(mainw->msg);
  } else {
    lives_widget_queue_draw(mainw->LiVES);
    lives_widget_context_update();
    com=lives_strdup_printf("%s commit_audio \"%s\"",prefs->backend,cfile->handle);
    mainw->com_failed=FALSE;
    mainw->cancelled=CANCEL_NONE;
    mainw->error=FALSE;
    lives_rm(cfile->info_file);
    lives_system(com,FALSE);
    lives_free(com);

    if (mainw->com_failed) {
      d_print_failed();
      if (mainw->multitrack!=NULL) {
        mt_sensitise(mainw->multitrack);
        mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
      }
      return;
    }

    do_auto_dialog(_("Committing audio"),0);
    check_backend_return(cfile);
    if (mainw->error) {
      d_print_failed();
      if (mainw->cancelled!=CANCEL_ERROR) do_error_dialog(mainw->msg);
    } else {
      get_dirname(file_name);
      lives_snprintf(mainw->audio_dir,PATH_MAX,"%s",file_name);
      reget_afilesize(mainw->current_file);
      cfile->changed=TRUE;
      get_play_times();
      d_print_done();
    }
  }
  cfile->undo_action=UNDO_APPEND_AUDIO;
  set_undoable(_("Append Audio"),!prefs->conserve_space);

  if (mainw->multitrack!=NULL) {
    mt_sensitise(mainw->multitrack);
    mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
  }
}



void on_trim_audio_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  // type 0 == trim selected
  // type 1 == trim to play pointer

  char *com,*msg;

  double start,end;

  boolean has_lmap_error=FALSE;

  int type=LIVES_POINTER_TO_INT(user_data);

  if (type==0) {
    start=calc_time_from_frame(mainw->current_file,cfile->start);
    end=calc_time_from_frame(mainw->current_file,cfile->end+1);
  } else {
    start=0.;
    end=cfile->pointer_time;
  }

  if (!(prefs->warning_mask&WARN_MASK_LAYOUT_DELETE_AUDIO)) {
    if (end<cfile->laudio_time&&(mainw->xlays=layout_audio_is_affected(mainw->current_file,end))!=NULL) {
      if (!do_warning_dialog
          (_("\nDeletion will cause missing audio in some multitrack layouts.\nAre you sure you wish to continue ?\n"))) {
        lives_list_free_strings(mainw->xlays);
        lives_list_free(mainw->xlays);
        mainw->xlays=NULL;
        return;
      }
      add_lmap_error(LMAP_ERROR_DELETE_AUDIO,cfile->name,(livespointer)cfile->layout_map,mainw->current_file,0,end,
                     cfile->stored_layout_audio>end);
      has_lmap_error=TRUE;
      lives_list_free_strings(mainw->xlays);
      lives_list_free(mainw->xlays);
      mainw->xlays=NULL;
    }
  }

  if (!has_lmap_error&&!(prefs->warning_mask&WARN_MASK_LAYOUT_ALTER_AUDIO)&&
      (mainw->xlays=layout_audio_is_affected(mainw->current_file,0.))!=NULL) {
    if (!do_layout_alter_audio_warning()) {
      lives_list_free_strings(mainw->xlays);
      lives_list_free(mainw->xlays);
      mainw->xlays=NULL;
      return;
    }
    add_lmap_error(LMAP_ERROR_ALTER_AUDIO,cfile->name,(livespointer)cfile->layout_map,mainw->current_file,0,0.,
                   cfile->stored_layout_audio>0.);
    has_lmap_error=TRUE;
    lives_list_free_strings(mainw->xlays);
    lives_list_free(mainw->xlays);
    mainw->xlays=NULL;
  }

  if (end>cfile->laudio_time&&end>cfile->raudio_time)
    msg=lives_strdup_printf(_("Padding audio to %.2f seconds..."),end);
  else
    msg=lives_strdup_printf(_("Trimming audio from %.2f to %.2f seconds..."),start,end);

  d_print(msg);
  lives_free(msg);

  com=lives_strdup_printf("%s trim_audio \"%s\" %.8f %.8f %d %d %d %d %d",prefs->backend, cfile->handle,
                          start, end, cfile->arate,
                          cfile->achans, cfile->asampsize, !(cfile->signed_endian&AFORM_UNSIGNED),
                          !(cfile->signed_endian&AFORM_BIG_ENDIAN));
  lives_rm(cfile->info_file);
  mainw->com_failed=FALSE;
  lives_system(com,FALSE);
  lives_free(com);

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
    set_undoable(_("Trim/Pad Audio"),!prefs->conserve_space);
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



void on_fade_audio_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  // type == 0 fade in
  // type == 1 fade out

  double startt,endt,startv,endv,time=0.;
  char *msg,*msg2,*utxt,*com;

  boolean has_lmap_error=FALSE;
  int alarm_handle;
  int type;

  aud_dialog_t *aud_d=NULL;

  if (menuitem!=NULL) {
    cfile->undo1_int=type=LIVES_POINTER_TO_INT(user_data);
    aud_d=create_audfade_dialog(type);
    if (lives_dialog_run(LIVES_DIALOG(aud_d->dialog))==LIVES_RESPONSE_CANCEL) {
      lives_widget_destroy(aud_d->dialog);
      lives_free(aud_d);
      return;
    }

    time=lives_spin_button_get_value(LIVES_SPIN_BUTTON(aud_d->time_spin));

    lives_widget_destroy(aud_d->dialog);
  } else {
    type=cfile->undo1_int;
  }

  if (menuitem==NULL||!aud_d->is_sel) {
    if (menuitem==NULL) {
      endt=cfile->undo1_dbl;
      startt=cfile->undo2_dbl;
    } else {
      if (type==0) {
        cfile->undo2_dbl=startt=0.;
        cfile->undo1_dbl=endt=time;
      } else {
        cfile->undo1_dbl=endt=cfile->laudio_time;
        cfile->undo2_dbl=startt=cfile->laudio_time-time;
      }
    }
  } else {
    cfile->undo2_dbl=startt=((double)cfile->start-1.)/cfile->fps;
    cfile->undo1_dbl=endt=(double)cfile->end/cfile->fps;
  }


  if (type==0) {
    startv=0.;
    endv=1.;
    msg2=lives_strdup(_("Fading audio in"));
    utxt=lives_strdup(_("Fade audio in"));
  } else {
    startv=1.;
    endv=0.;
    msg2=lives_strdup(_("Fading audio out"));
    utxt=lives_strdup(_("Fade audio out"));
  }

  if (menuitem!=NULL) {
    if (!(prefs->warning_mask&WARN_MASK_LAYOUT_ALTER_AUDIO)&&
        (mainw->xlays=layout_audio_is_affected(mainw->current_file,0.))!=NULL) {
      if (!do_layout_alter_audio_warning()) {
        lives_free(msg2);
        lives_free(utxt);
        lives_list_free_strings(mainw->xlays);
        lives_list_free(mainw->xlays);
        mainw->xlays=NULL;
        return;
      }
      add_lmap_error(LMAP_ERROR_ALTER_AUDIO,cfile->name,(livespointer)cfile->layout_map,mainw->current_file,0,0.,
                     cfile->stored_layout_audio>0.);
      has_lmap_error=TRUE;
      lives_list_free_strings(mainw->xlays);
      lives_list_free(mainw->xlays);
      mainw->xlays=NULL;
    }

    if (!aud_d->is_sel)
      msg=lives_strdup_printf(_("%s over %.1f seconds..."),msg2,time);
    else
      msg=lives_strdup_printf(_("%s from time %.2f seconds to %.2f seconds..."),msg2,startt,endt);
    d_print(msg);
    lives_free(msg);
    lives_free(msg2);
  }

  desensitize();
  do_threaded_dialog(_("Fading audio..."),FALSE);
  alarm_handle=lives_alarm_set(1);

  threaded_dialog_spin(0.);
  lives_widget_context_update();
  threaded_dialog_spin(0.);

  if (!prefs->conserve_space) {
    mainw->error=FALSE;
    com=lives_strdup_printf("%s backup_audio \"%s\"",prefs->backend_sync,cfile->handle);
    lives_system(com,FALSE);
    lives_free(com);

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
    lives_usleep(prefs->sleep_time);
  }

  lives_alarm_clear(alarm_handle);

  end_threaded_dialog();
  d_print_done();

  cfile->changed=TRUE;

  if (!prefs->conserve_space) {
    set_undoable(utxt,TRUE);
    cfile->undo_action=UNDO_FADE_AUDIO;
  }
  lives_free(utxt);
  sensitize();

  if (has_lmap_error) popup_lmap_errors(NULL,NULL);
  if (aud_d!=NULL) lives_free(aud_d);

}





boolean on_del_audio_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  double start,end;
  char *com,*msg=NULL;
  boolean has_lmap_error=FALSE;
  boolean bad_header=FALSE;

  if (menuitem==NULL) {
    // undo/redo
    start=cfile->undo1_dbl;
    end=cfile->undo2_dbl;
  } else {
    if (LIVES_POINTER_TO_INT(user_data)) {

      if (!(prefs->warning_mask&WARN_MASK_LAYOUT_DELETE_AUDIO)) {
        if ((mainw->xlays=layout_audio_is_affected(mainw->current_file,0.))!=NULL) {
          if (!do_warning_dialog
              (_("\nDeletion will cause missing audio in some multitrack layouts.\nAre you sure you wish to continue ?\n"))) {
            lives_list_free_strings(mainw->xlays);
            lives_list_free(mainw->xlays);
            mainw->xlays=NULL;
            return FALSE;
          }
          add_lmap_error(LMAP_ERROR_DELETE_AUDIO,cfile->name,(livespointer)cfile->layout_map,mainw->current_file,0,0.,
                         cfile->stored_layout_audio>0.);
          has_lmap_error=TRUE;
          lives_list_free_strings(mainw->xlays);
          lives_list_free(mainw->xlays);
          mainw->xlays=NULL;
        }
      }

      if (!has_lmap_error&&!(prefs->warning_mask&WARN_MASK_LAYOUT_ALTER_AUDIO)&&
          (mainw->xlays=layout_audio_is_affected(mainw->current_file,0.))!=NULL) {
        if (!do_layout_alter_audio_warning()) {
          lives_list_free_strings(mainw->xlays);
          lives_list_free(mainw->xlays);
          mainw->xlays=NULL;
          return FALSE;
        }
        add_lmap_error(LMAP_ERROR_ALTER_AUDIO,cfile->name,(livespointer)cfile->layout_map,mainw->current_file,0,0.,
                       cfile->stored_layout_audio>0.);
        has_lmap_error=TRUE;
        lives_list_free_strings(mainw->xlays);
        lives_list_free(mainw->xlays);
        mainw->xlays=NULL;
      }

      if (!cfile->frames) {
        if (do_warning_dialog(_("\nDeleting all audio will close this file.\nAre you sure ?"))) close_current_file(0);
        return FALSE;
      }
      msg=lives_strdup(_("Deleting all audio..."));
      start=end=0.;
    } else {
      start=calc_time_from_frame(mainw->current_file,cfile->start);
      end=calc_time_from_frame(mainw->current_file,cfile->end+1);
      msg=lives_strdup_printf(_("Deleting audio from %.2f to %.2f seconds..."),start,end);
      start*=(double)cfile->arate/(double)cfile->arps;
      end*=(double)cfile->arate/(double)cfile->arps;

      if (!(prefs->warning_mask&WARN_MASK_LAYOUT_SHIFT_AUDIO)) {
        if ((mainw->xlays=layout_audio_is_affected(mainw->current_file,end))!=NULL) {
          if (!do_warning_dialog
              (_("\nDeletion will cause audio to shift in some multitrack layouts.\nAre you sure you wish to continue ?\n"))) {
            lives_list_free_strings(mainw->xlays);
            lives_list_free(mainw->xlays);
            mainw->xlays=NULL;
            return FALSE;
          }
          add_lmap_error(LMAP_ERROR_SHIFT_AUDIO,cfile->name,(livespointer)cfile->layout_map,mainw->current_file,0,start,
                         cfile->stored_layout_audio>end);
          has_lmap_error=TRUE;
          lives_list_free_strings(mainw->xlays);
          lives_list_free(mainw->xlays);
          mainw->xlays=NULL;
        }
      }

      if (!has_lmap_error&&!(prefs->warning_mask&WARN_MASK_LAYOUT_DELETE_AUDIO)) {
        if ((mainw->xlays=layout_audio_is_affected(mainw->current_file,start))!=NULL) {
          if (!do_warning_dialog
              (_("\nDeletion will cause missing audio in some multitrack layouts.\nAre you sure you wish to continue ?\n"))) {
            lives_list_free_strings(mainw->xlays);
            lives_list_free(mainw->xlays);
            mainw->xlays=NULL;
            return FALSE;
          }
          add_lmap_error(LMAP_ERROR_DELETE_AUDIO,cfile->name,(livespointer)cfile->layout_map,mainw->current_file,0,start,
                         cfile->stored_layout_audio>start);
          has_lmap_error=TRUE;
          lives_list_free_strings(mainw->xlays);
          lives_list_free(mainw->xlays);
          mainw->xlays=NULL;
        }
      }

      if (!has_lmap_error&&!(prefs->warning_mask&WARN_MASK_LAYOUT_ALTER_AUDIO)&&
          (mainw->xlays=layout_audio_is_affected(mainw->current_file,0.))!=NULL) {
        if (!do_layout_alter_audio_warning()) {
          lives_list_free_strings(mainw->xlays);
          lives_list_free(mainw->xlays);
          mainw->xlays=NULL;
          return FALSE;
        }
        add_lmap_error(LMAP_ERROR_ALTER_AUDIO,cfile->name,(livespointer)cfile->layout_map,mainw->current_file,0,0.,
                       cfile->stored_layout_audio>0.);
        has_lmap_error=TRUE;
        lives_list_free_strings(mainw->xlays);
        lives_list_free(mainw->xlays);
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
    lives_free(msg);
  }

  com=lives_strdup_printf("%s delete_audio \"%s\" %.8f %.8f %d %d %d", prefs->backend,
                          cfile->handle, start, end, cfile->arps,
                          cfile->achans, cfile->asampsize);
  lives_rm(cfile->info_file);
  mainw->com_failed=FALSE;
  lives_system(com,FALSE);
  lives_free(com);

  if (mainw->com_failed) {
    if (menuitem!=NULL) d_print_failed();
    return FALSE;
  }

  do_progress_dialog(TRUE, FALSE, _("Deleting Audio"));

  if (mainw->error) {
    if (menuitem!=NULL) d_print_failed();
    return FALSE;
  }

  set_undoable(_("Delete Audio"),TRUE);
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


void on_rb_audrec_time_toggled(LiVESToggleButton *togglebutton, livespointer user_data) {
  _resaudw *resaudw=(_resaudw *)user_data;
  if (lives_toggle_button_get_active(togglebutton)) {
    lives_widget_set_sensitive(resaudw->hour_spinbutton,TRUE);
    lives_widget_set_sensitive(resaudw->minute_spinbutton,TRUE);
    lives_widget_set_sensitive(resaudw->second_spinbutton,TRUE);
  } else {
    lives_widget_set_sensitive(resaudw->hour_spinbutton,FALSE);
    lives_widget_set_sensitive(resaudw->minute_spinbutton,FALSE);
    lives_widget_set_sensitive(resaudw->second_spinbutton,FALSE);
  }
}


void on_recaudclip_activate(LiVESMenuItem *menuitem, livespointer user_data) {

  if (!is_realtime_aplayer(prefs->audio_player)) {
    do_nojack_rec_error();
    return;
  }

  mainw->fx1_val=DEFAULT_AUDIO_RATE;
  mainw->fx2_val=DEFAULT_AUDIO_CHANS;
  mainw->fx3_val=DEFAULT_AUDIO_SAMPS;
  mainw->fx4_val=mainw->endian;
  mainw->rec_end_time=-1.;
  resaudw=create_resaudw(5,NULL,NULL);
  lives_widget_show(resaudw->dialog);
}


static boolean has_lmap_error_recsel;


void on_recaudsel_activate(LiVESMenuItem *menuitem, livespointer user_data) {

  if (!is_realtime_aplayer(prefs->audio_player)) {
    do_nojack_rec_error();
    return;
  }

  has_lmap_error_recsel=FALSE;
  if ((prefs->warning_mask&WARN_MASK_LAYOUT_ALTER_AUDIO)&&
      (mainw->xlays=layout_audio_is_affected(mainw->current_file,0.))!=NULL) {
    if (!do_layout_alter_audio_warning()) {
      has_lmap_error_recsel=FALSE;
      lives_list_free_strings(mainw->xlays);
      lives_list_free(mainw->xlays);
      mainw->xlays=NULL;
      return;
    }
    add_lmap_error(LMAP_ERROR_ALTER_AUDIO,cfile->name,(livespointer)cfile->layout_map,mainw->current_file,0,0.,
                   cfile->stored_layout_audio>0.);
    has_lmap_error_recsel=TRUE;
    lives_list_free_strings(mainw->xlays);
    lives_list_free(mainw->xlays);
    mainw->xlays=NULL;
  }

  mainw->rec_end_time=(cfile->end-cfile->start+1.)/cfile->fps;

  if (cfile->achans>0) {
    mainw->fx1_val=cfile->arate;
    mainw->fx2_val=cfile->achans;
    mainw->fx3_val=cfile->asampsize;
    mainw->fx4_val=cfile->signed_endian;
    resaudw=create_resaudw(7,NULL,NULL);
  } else {
    mainw->fx1_val=DEFAULT_AUDIO_RATE;
    mainw->fx2_val=DEFAULT_AUDIO_CHANS;
    mainw->fx3_val=DEFAULT_AUDIO_SAMPS;
    mainw->fx4_val=mainw->endian;
    resaudw=create_resaudw(6,NULL,NULL);
  }
  lives_widget_show_all(resaudw->dialog);
}



void on_recaudclip_ok_clicked(LiVESButton *button, livespointer user_data) {
#ifdef RT_AUDIO
  weed_timecode_t ins_pt;
  double aud_start,aud_end,vel=1.,vol=1.;

  int asigned=1,aendian=1;
  int old_file=mainw->current_file,new_file;
  int type=LIVES_POINTER_TO_INT(user_data);
  int oachans=0,oarate=0,oarps=0,ose=0,oasamps=0;
  boolean backr=FALSE;

  char *com;


  // type == 0 - new clip
  // type == 1 - existing clip

  if (type==1) d_print(""); // show switch message, if appropriate

  mainw->current_file=mainw->first_free_file;
  if (!get_new_handle(mainw->current_file,NULL)) {
    mainw->current_file=old_file;
    return;
  }

  cfile->is_loaded=TRUE;

  cfile->arps=cfile->arate=(int)atoi(lives_entry_get_text(LIVES_ENTRY(resaudw->entry_arate)));
  cfile->achans=(int)atoi(lives_entry_get_text(LIVES_ENTRY(resaudw->entry_achans)));
  cfile->asampsize=(int)atoi(lives_entry_get_text(LIVES_ENTRY(resaudw->entry_asamps)));

  if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(resaudw->unlim_radiobutton))) {
    mainw->rec_end_time=-1.;
    mainw->rec_samples=-1;
  } else {
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
  lives_widget_destroy(resaudw->dialog);
  lives_widget_context_update();
  if (resaudw!=NULL) lives_free(resaudw);
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
    lives_snprintf(cfile->type,40,"Audio");
    add_to_clipmenu();

    lives_notify(LIVES_OSC_NOTIFY_CLIP_OPENED,"");

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
    mainw->jackd_read->in_use=TRUE;
  }
#endif
#ifdef HAVE_PULSE_AUDIO
  if (prefs->audio_player==AUD_PLAYER_PULSE) {
    pulse_rec_audio_to_clip(mainw->current_file,old_file,type==0?RECA_NEW_CLIP:RECA_EXISTING);
    mainw->pulsed_read->in_use=TRUE;
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
      com=lives_strdup_printf("%s backup_audio \"%s\"",prefs->backend_sync,mainw->files[old_file]->handle);
      lives_system(com,FALSE);
      lives_free(com);

      if (mainw->error) {
        end_threaded_dialog();
        d_print_failed();
        return;
      }

    }


    mainw->read_failed=mainw->write_failed=FALSE;
    if (mainw->read_failed_file!=NULL) lives_free(mainw->read_failed_file);
    mainw->read_failed_file=NULL;


    // copy audio from old clip to current
    render_audio_segment(1,&(mainw->current_file),old_file,&vel,&aud_start,ins_pt,
                         ins_pt+(weed_timecode_t)((aud_end-aud_start)*U_SEC),&vol,vol,vol,NULL);

    end_threaded_dialog();
    close_current_file(old_file);

    if (mainw->write_failed) {
      // on failure
      int outfile=(mainw->multitrack!=NULL?mainw->multitrack->render_file:mainw->current_file);
      char *outfilename=lives_build_filename(prefs->tmpdir,mainw->files[outfile]->handle,"audio",NULL);
      do_write_failed_error_s(outfilename,NULL);

      if (!prefs->conserve_space&&type==1) {
        // try to recover backup
        com=lives_strdup_printf("%s undo_audio \"%s\"",prefs->backend_sync,mainw->files[old_file]->handle);
        lives_system(com,FALSE);
        lives_free(com);
        backr=TRUE;
      }
    }

    if (mainw->read_failed) {
      do_read_failed_error_s(mainw->read_failed_file,NULL);
      if (mainw->read_failed_file!=NULL) lives_free(mainw->read_failed_file);
      mainw->read_failed_file=NULL;
      if (!prefs->conserve_space&&type==1&&!backr) {
        // try to recover backup
        com=lives_strdup_printf("%s undo_audio \"%s\"",prefs->backend_sync,mainw->files[old_file]->handle);
        lives_system(com,FALSE);
        lives_free(com);
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
      set_undoable(_("Record new audio"),TRUE);
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


boolean on_ins_silence_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  double start=0,end=0;
  char *com;
  boolean has_lmap_error=FALSE;
  boolean has_new_audio=FALSE;

  if (!cfile->achans) {
    has_new_audio=TRUE;
  }

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
    if (lives_dialog_run(LIVES_DIALOG(resaudw->dialog))!=LIVES_RESPONSE_OK) return FALSE;
    if (mainw->error) {
      mainw->error=FALSE;
      return FALSE;
    }

    cfile->undo_arate=cfile->arate;
    cfile->undo_signed_endian=cfile->signed_endian;
    cfile->undo_achans=cfile->achans;
    cfile->undo_asampsize=cfile->asampsize;
  }


  if (menuitem!=NULL) {
    start=calc_time_from_frame(mainw->current_file,cfile->start);
    end=calc_time_from_frame(mainw->current_file,cfile->end+1);

    if (!(prefs->warning_mask&WARN_MASK_LAYOUT_SHIFT_AUDIO)) {
      if ((mainw->xlays=layout_audio_is_affected(mainw->current_file,start))!=NULL) {
        if (!do_warning_dialog(_("\nInsertion will cause audio to shift in some multitrack layouts.\nAre you sure you wish to continue ?\n"))) {
          lives_list_free_strings(mainw->xlays);
          lives_list_free(mainw->xlays);
          mainw->xlays=NULL;
          if (has_new_audio) cfile->achans=cfile->arate=cfile->asampsize=cfile->arps=0;
          return FALSE;
        }
        add_lmap_error(LMAP_ERROR_SHIFT_AUDIO,cfile->name,(livespointer)cfile->layout_map,mainw->current_file,0,start,
                       cfile->stored_layout_audio>start);
        has_lmap_error=TRUE;
        lives_list_free_strings(mainw->xlays);
        lives_list_free(mainw->xlays);
        mainw->xlays=NULL;
      }
    }

    if (!has_lmap_error&&!(prefs->warning_mask&WARN_MASK_LAYOUT_ALTER_AUDIO)
        &&(mainw->xlays=layout_audio_is_affected(mainw->current_file,0.))!=NULL) {
      if (!do_layout_alter_audio_warning()) {
        lives_list_free_strings(mainw->xlays);
        lives_list_free(mainw->xlays);
        mainw->xlays=NULL;
        if (has_new_audio) cfile->achans=cfile->arate=cfile->asampsize=cfile->arps=0;
        return FALSE;
      }
      add_lmap_error(LMAP_ERROR_ALTER_AUDIO,cfile->name,(livespointer)cfile->layout_map,mainw->current_file,0,start,
                     cfile->stored_layout_audio>0.);
      has_lmap_error=TRUE;
      lives_list_free_strings(mainw->xlays);
      lives_list_free(mainw->xlays);
      mainw->xlays=NULL;
    }

    d_print(""); // force switchtext
    d_print(_("Inserting silence from %.2f to %.2f seconds..."),start,end);
  }

  cfile->undo1_dbl=start;
  start*=(double)cfile->arate/(double)cfile->arps;
  cfile->undo2_dbl=end;
  end*=(double)cfile->arate/(double)cfile->arps;

  // with_sound is 2 (audio only), therfore start, end, where, are in seconds. rate is -ve to indicate silence
  com=lives_strdup_printf("%s insert \"%s\" \"%s\" %.8f 0. %.8f \"%s\" 2 0 0 0 0 %d %d %d %d %d",
                          prefs->backend, cfile->handle,
                          get_image_ext_for_type(cfile->img_type), start, end-start, cfile->handle, -cfile->arps,
                          cfile->achans, cfile->asampsize, !(cfile->signed_endian&AFORM_UNSIGNED),
                          !(cfile->signed_endian&AFORM_BIG_ENDIAN));

  lives_rm(cfile->info_file);
  mainw->com_failed=FALSE;
  lives_system(com,FALSE);
  lives_free(com);

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

  if (has_new_audio) {
    cfile->arate=cfile->arps=cfile->undo_arate;
    cfile->signed_endian=cfile->undo_signed_endian;
    cfile->achans=cfile->undo_achans;
    cfile->asampsize=cfile->undo_asampsize;
  }

  set_undoable(_("Insert Silence"),TRUE);
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



void on_ins_silence_details_clicked(LiVESButton *button, livespointer user_data) {
  int asigned=1,aendian=1;
  boolean bad_header=FALSE;

  cfile->arps=cfile->arate=(int)atoi(lives_entry_get_text(LIVES_ENTRY(resaudw->entry_arate)));
  cfile->achans=(int)atoi(lives_entry_get_text(LIVES_ENTRY(resaudw->entry_achans)));
  cfile->asampsize=(int)atoi(lives_entry_get_text(LIVES_ENTRY(resaudw->entry_asamps)));
  if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(resaudw->rb_unsigned))) {
    asigned=0;
  }
  if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(resaudw->rb_bigend))) {
    aendian=0;
  }
  cfile->signed_endian=get_signed_endian(asigned,aendian);
  lives_widget_destroy(resaudw->dialog);
  lives_widget_context_update();
  if (resaudw!=NULL) lives_free(resaudw);
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


void on_lerrors_clear_clicked(LiVESButton *button, livespointer user_data) {
  boolean close=LIVES_POINTER_TO_INT(user_data);

  if (mainw->multitrack!=NULL) {
    if (mainw->multitrack->idlefunc>0) {
      lives_source_remove(mainw->multitrack->idlefunc);
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


void on_lerrors_delete_clicked(LiVESButton *button, livespointer user_data) {
  int num_maps=lives_list_length(mainw->affected_layouts_map);
  char *msg=lives_strdup_printf(P_("\nDelete %d layout...are you sure ?\n","\nDelete %d layouts...are you sure ?\n",num_maps),num_maps);

  if (mainw->multitrack!=NULL) {
    if (mainw->multitrack->idlefunc>0) {
      lives_source_remove(mainw->multitrack->idlefunc);
      mainw->multitrack->idlefunc=0;
    }
    mt_desensitise(mainw->multitrack);
  }

  if (!do_warning_dialog(msg)) {
    lives_free(msg);
    if (mainw->multitrack!=NULL) {
      mt_sensitise(mainw->multitrack);
      mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
    }
    return;
  }

  lives_free(msg);
  remove_layout_files(mainw->affected_layouts_map);
  on_lerrors_clear_clicked(button,LIVES_INT_TO_POINTER(TRUE));
}


