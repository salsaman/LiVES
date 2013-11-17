// saveplay.c
// LiVES (lives-exe)
// (c) G. Finch 2003 - 2013
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

#ifdef HAVE_SYSTEM_WEED
#include <weed/weed.h>
#include <weed/weed-host.h>
#include <weed/weed-palettes.h>
#else
#include "../libweed/weed.h"
#include "../libweed/weed-host.h"
#include "../libweed/weed-palettes.h"
#endif

#include <unistd.h>
#include <stdlib.h>

#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "main.h"
#include "callbacks.h"
#include "support.h"
#include "resample.h"
#include "effects.h"
#include "audio.h"
#include "htmsocket.h"
#include "cvirtual.h"


boolean save_clip_values(int which) {
  gchar *lives_header;

  int asigned;
  int endian;
  int retval;
#ifndef IS_MINGW
  mode_t xumask;
#endif

  if (which==0||which==mainw->scrap_file||which==mainw->ascrap_file) return TRUE;

#ifndef IS_MINGW
  xumask=umask(DEF_FILE_UMASK);
#endif

  asigned=!(mainw->files[which]->signed_endian&AFORM_UNSIGNED);
  endian=mainw->files[which]->signed_endian&AFORM_BIG_ENDIAN;
  lives_header=g_build_filename(prefs->tmpdir,mainw->files[which]->handle,"header.lives",NULL);

  do {
    mainw->clip_header=fopen(lives_header,"w");

    if (mainw->clip_header==NULL) {
      retval=do_write_failed_error_s_with_retry(lives_header,g_strerror(errno),NULL);
      if (retval==LIVES_CANCEL) {
	g_free(lives_header);
#ifndef IS_MINGW
	umask(xumask);
#endif
	return FALSE;
      }
    }

    else {
      mainw->files[which]->header_version=LIVES_CLIP_HEADER_VERSION;

      do {
	retval=0;
	set_signal_handlers((SignalHandlerPointer)defer_sigint);
	save_clip_value(which,CLIP_DETAILS_HEADER_VERSION,&mainw->files[which]->header_version);
	if (mainw->com_failed||mainw->write_failed) break;
	save_clip_value(which,CLIP_DETAILS_BPP,&mainw->files[which]->bpp);
	if (mainw->com_failed||mainw->write_failed) break;
	save_clip_value(which,CLIP_DETAILS_FPS,&mainw->files[which]->fps);
	if (mainw->com_failed||mainw->write_failed) break;
	save_clip_value(which,CLIP_DETAILS_PB_FPS,&mainw->files[which]->pb_fps);
	if (mainw->com_failed||mainw->write_failed) break;
	save_clip_value(which,CLIP_DETAILS_WIDTH,&mainw->files[which]->hsize);
	if (mainw->com_failed||mainw->write_failed) break;
	save_clip_value(which,CLIP_DETAILS_HEIGHT,&mainw->files[which]->vsize);
	if (mainw->com_failed||mainw->write_failed) break;
	save_clip_value(which,CLIP_DETAILS_INTERLACE,&mainw->files[which]->interlace);
	if (mainw->com_failed||mainw->write_failed) break;
	save_clip_value(which,CLIP_DETAILS_UNIQUE_ID,&mainw->files[which]->unique_id);
	if (mainw->com_failed||mainw->write_failed) break;
	save_clip_value(which,CLIP_DETAILS_ARATE,&mainw->files[which]->arps);
	if (mainw->com_failed||mainw->write_failed) break;
	save_clip_value(which,CLIP_DETAILS_PB_ARATE,&mainw->files[which]->arate);
	if (mainw->com_failed||mainw->write_failed) break;
	save_clip_value(which,CLIP_DETAILS_ACHANS,&mainw->files[which]->achans);
	if (mainw->com_failed||mainw->write_failed) break;
	save_clip_value(which,CLIP_DETAILS_ASIGNED,&asigned);
	if (mainw->com_failed||mainw->write_failed) break;
	save_clip_value(which,CLIP_DETAILS_AENDIAN,&endian);
	if (mainw->com_failed||mainw->write_failed) break;
	save_clip_value(which,CLIP_DETAILS_ASAMPS,&mainw->files[which]->asampsize);
	if (mainw->com_failed||mainw->write_failed) break;
	save_clip_value(which,CLIP_DETAILS_FRAMES,&mainw->files[which]->frames);
	if (mainw->com_failed||mainw->write_failed) break;
	save_clip_value(which,CLIP_DETAILS_TITLE,mainw->files[which]->title);
	if (mainw->com_failed||mainw->write_failed) break;
	save_clip_value(which,CLIP_DETAILS_AUTHOR,mainw->files[which]->author);
	if (mainw->com_failed||mainw->write_failed) break;
	save_clip_value(which,CLIP_DETAILS_COMMENT,mainw->files[which]->comment);
	if (mainw->com_failed||mainw->write_failed) break;
	save_clip_value(which,CLIP_DETAILS_PB_FRAMENO,&mainw->files[which]->frameno);
	if (mainw->com_failed||mainw->write_failed) break;
	save_clip_value(which,CLIP_DETAILS_CLIPNAME,mainw->files[which]->name);
	if (mainw->com_failed||mainw->write_failed) break;
	save_clip_value(which,CLIP_DETAILS_FILENAME,mainw->files[which]->file_name);
	if (mainw->com_failed||mainw->write_failed) break;
	save_clip_value(which,CLIP_DETAILS_KEYWORDS,mainw->files[which]->keywords);
      } while (FALSE);

      if (mainw->signal_caught) catch_sigint(mainw->signal_caught);
      set_signal_handlers((SignalHandlerPointer)catch_sigint);
      
      if (mainw->com_failed||mainw->write_failed) {
	fclose(mainw->clip_header);
	retval=do_write_failed_error_s_with_retry(lives_header,NULL,NULL);
      }

    }
  } while (retval==LIVES_RETRY);
  
  g_free(lives_header);
#ifndef IS_MINGW
  umask(xumask);
#endif

  fclose(mainw->clip_header);
  mainw->clip_header=NULL;

  if (retval==LIVES_CANCEL) return FALSE;

  return TRUE;
}


boolean read_file_details(const gchar *file_name, boolean is_audio) {
  // get preliminary details

  // is_audio set to TRUE prevents us from checking for images, and deleting the (existing) first frame
  // therefore it is IMPORTANT to set it when loading new audio for an existing clip !

  FILE *infofile;
  int alarm_handle;
  int retval;
  boolean timeout;
  gchar *tmp,*com=g_strdup_printf("%s get_details \"%s\" \"%s\" \"%s\" %d %d",prefs->backend_sync,cfile->handle,
				  (tmp=g_filename_from_utf8(file_name,-1,NULL,NULL,NULL)),
				  get_image_ext_for_type(cfile->img_type),mainw->opening_loc,is_audio);
  g_free(tmp);

  mainw->com_failed=FALSE;
  unlink(cfile->info_file);
  lives_system(com,FALSE);
  g_free(com);

  if (mainw->com_failed) {
    return FALSE;
  }

  if (mainw->opening_loc) 
    return do_progress_dialog(TRUE,TRUE,_ ("Examining file header"));


  threaded_dialog_spin();

  do {
    retval=0;
    timeout=FALSE;
    clear_mainw_msg();

#define LIVES_LONGER_TIMEOUT  (30 * U_SEC) // 30 second timeout

    alarm_handle=lives_alarm_set(LIVES_LONGER_TIMEOUT);
    
    while (!((infofile=fopen(cfile->info_file,"r")) || (timeout=lives_alarm_get(alarm_handle)))) {
      lives_widget_context_update();
      threaded_dialog_spin();
      g_usleep(prefs->sleep_time);
    }
    
    lives_alarm_clear(alarm_handle);
    
    if (!timeout) {
      mainw->read_failed=FALSE;
      lives_fgets(mainw->msg,512,infofile);
      fclose(infofile);
    }

    if (timeout||mainw->read_failed) {
      retval=do_read_failed_error_s_with_retry(cfile->info_file,NULL,NULL);
    }
  } while (retval==LIVES_RETRY);

  threaded_dialog_spin();
  return TRUE;
}

const gchar *get_deinterlace_string(void) {
  if (mainw->open_deint) return "-vf pp=ci";
  else return "";
}


void deduce_file(const gchar *file_name, double start, int end) {
  // this is a utility function to deduce whether we are dealing with a file, 
  // a selection, a backup, or a location
  gchar short_file_name[PATH_MAX];
  mainw->img_concat_clip=-1;

  if (g_strrstr(file_name,"://")!=NULL&&strncmp (file_name,"dvd://",6)) {
    mainw->opening_loc=TRUE;
    open_file(file_name);
    mainw->opening_loc=FALSE;
  }
  else {
    g_snprintf(short_file_name,PATH_MAX,"%s",file_name);
    if (!(strcmp(file_name+strlen(file_name)-4,".lv1"))) {
      restore_file(file_name);
    }
    else {
      open_file_sel(file_name,start,end);
    }
  }
}


void open_file (const gchar *file_name) {
  // this function should be called to open a whole file
  open_file_sel(file_name,0.,0);
}



static boolean rip_audio_cancelled(int old_file, weed_plant_t *mt_pb_start_event, 
				   boolean mt_has_audio_file) {

  if (mainw->cancelled==CANCEL_KEEP) {
    // user clicked "enough"
    mainw->cancelled=CANCEL_NONE;
    return TRUE;
  }

  end_threaded_dialog();

  d_print("\n");
  d_print_cancelled();
  close_current_file(old_file);
  
  mainw->noswitch=FALSE;

  if (mainw->multitrack!=NULL) {
    mainw->multitrack->pb_start_event=mt_pb_start_event;
    mainw->multitrack->has_audio_file=mt_has_audio_file;
  }

  if (mainw->file_open_params!=NULL) g_free (mainw->file_open_params);
  mainw->file_open_params=NULL;
  lives_set_cursor_style(LIVES_CURSOR_NORMAL,NULL);
  return FALSE;
}


#define AUDIO_FRAMES_TO_READ 100

void open_file_sel(const gchar *file_name, double start, int frames) {
  gchar msg[256],loc[256];
  gchar *tmp=NULL;
  gchar *isubfname=NULL;
  gchar *fname=g_strdup(file_name),*msgstr;
  gchar *com;

  int withsound=1;
  int old_file=mainw->current_file;
  int new_file=old_file;

  int achans,arate,arps,asampsize;
  int current_file;

  boolean mt_has_audio_file=TRUE;

  const lives_clip_data_t *cdata;

  weed_plant_t *mt_pb_start_event=NULL;

  if (old_file==-1||!cfile->opening) {
    new_file=mainw->first_free_file;

    if (!get_new_handle(new_file,fname)) {
      g_free(fname);
      return;
    }
    g_free(fname);
    
    lives_set_cursor_style(LIVES_CURSOR_BUSY,NULL);
    lives_widget_context_update();

    if (frames==0) {
      com=g_strdup_printf(_ ("Opening %s"),file_name);
    }
    else {
      com=g_strdup_printf(_ ("Opening %s start time %.2f sec. frames %d"),file_name,start,frames);
    }
    d_print(""); // exhaust "switch" message

    d_print(com);
    g_free(com);
    
    if (!mainw->save_with_sound) {
      d_print(_ (" without sound"));
      withsound=0;
    }
    
    mainw->noswitch=TRUE;
    mainw->current_file=new_file;

    if (mainw->multitrack!=NULL) {
      // set up for opening preview
      mt_pb_start_event=mainw->multitrack->pb_start_event;
      mt_has_audio_file=mainw->multitrack->has_audio_file;
      mainw->multitrack->pb_start_event=NULL;
      mainw->multitrack->has_audio_file=TRUE;
    }

    if (!strcmp(prefs->image_ext,"png")) cfile->img_type=IMG_TYPE_PNG;
    
    if (prefs->instant_open) {
      // cd to clip directory - so decoder plugins can write temp files
      gchar *ppath=g_build_filename(prefs->tmpdir,cfile->handle,NULL);
      gchar *cwd=g_get_current_dir();
      lives_chdir(ppath,FALSE);
      g_free(ppath);
	
      cdata=get_decoder_cdata(cfile,prefs->disabled_decoders);

      lives_chdir(cwd,FALSE);
      g_free(cwd);

      if (cfile->ext_src!=NULL) {
	lives_decoder_t *dplug=(lives_decoder_t *)cfile->ext_src;
	cfile->opening=TRUE;
	cfile->clip_type=CLIP_TYPE_FILE;
	
	if (cdata->frame_width>0) {
	  cfile->hsize=cdata->frame_width;
	  cfile->vsize=cdata->frame_height;
	}
	else {
	  cfile->hsize=cdata->width;
	  cfile->vsize=cdata->height;
	}

	cfile->frames=cdata->nframes;
	
	snprintf(cfile->author,256,"%s",cdata->author);
	snprintf(cfile->title,256,"%s",cdata->title);
	snprintf(cfile->comment,256,"%s",cdata->comment);

	if (frames>0&&cfile->frames>frames) cfile->frames=frames;
	
	cfile->start=1;
	cfile->end=cfile->frames;
	create_frame_index(mainw->current_file,TRUE,cfile->fps*(start==0?0:start-1),frames==0?cfile->frames:frames);

	cfile->arate=cfile->arps=cdata->arate;
	cfile->achans=cdata->achans;
	cfile->asampsize=cdata->asamps;
	
	cfile->signed_endian=get_signed_endian(cdata->asigned, capable->byte_order==LIVES_LITTLE_ENDIAN);

	if (cfile->achans>0&&(dplug->decoder->rip_audio)!=NULL&&withsound==1) {
	  // call rip_audio() in the decoder plugin
	  // the plugin gets a chance to do any internal cleanup in rip_audio_cleanup()

	  int64_t stframe=cfile->fps*start+.5;
	  int64_t maxframe=stframe+(frames==0)?cfile->frames:frames;
	  int64_t nframes=AUDIO_FRAMES_TO_READ;
	  gchar *afile=g_strdup_printf("%s/%s/audiodump.pcm",prefs->tmpdir,cfile->handle);

	  msgstr=g_strdup_printf(_("Opening audio for %s"),file_name);

	  if (mainw->playing_file==-1) resize(1);

	  mainw->cancelled=CANCEL_NONE;

	  cfile->opening_only_audio=TRUE;
	  if (mainw->playing_file==-1) do_threaded_dialog(msgstr,TRUE);

	  do {
	    if (stframe+nframes>maxframe) nframes=maxframe-stframe;
	    if (nframes<=0) break;
	    (dplug->decoder->rip_audio)(cdata,afile,stframe,nframes,NULL);
	    threaded_dialog_spin();
	    stframe+=nframes;
	  } while (mainw->cancelled==CANCEL_NONE);

	  if (dplug->decoder->rip_audio_cleanup!=NULL) {
	    (dplug->decoder->rip_audio_cleanup)(cdata);
	  }

	  if (mainw->cancelled!=CANCEL_NONE) {
	    if (!rip_audio_cancelled(old_file,mt_pb_start_event,mt_has_audio_file)) {
	      g_free(afile);
	      return;
	    }
	  }

	  end_threaded_dialog();
	  g_free(msgstr);

	  cfile->opening_only_audio=FALSE;
	  g_free(afile);
	}
	else {
	  cfile->arate=0.;
	  cfile->achans=cfile->asampsize=0;
	}

	cfile->fps=cfile->pb_fps=cdata->fps;
	d_print("\n");

	if (cfile->achans==0&&withsound==1) {
	  if (0) {
	  /*if (!capable->has_mplayer) {
	    do_mplayer_audio_warning();
	    }*/
	  }
	  else {

	    mainw->com_failed=FALSE;

	    // check if we have audio
	    read_file_details(file_name,FALSE);
	    unlink (cfile->info_file);
	  
	    if (mainw->com_failed) return;

	    if (strlen(mainw->msg)>0) add_file_info (cfile->handle,TRUE);

	    if (cfile->achans>0) {
	      // plugin returned no audio, try with mplayer
	      if (mainw->file_open_params==NULL) mainw->file_open_params=g_strdup("");
	      com=g_strdup_printf("%s open \"%s\" \"%s\" %d \"%s\" %.2f %d \"%s\"",prefs->backend,cfile->handle,
				  (tmp=g_filename_from_utf8 (file_name,-1,NULL,NULL,NULL)),-1,
				  prefs->image_ext,start,frames,mainw->file_open_params);

	      g_free(tmp);


	      cfile->op_dir=g_filename_from_utf8((tmp=get_dir(file_name)),-1,NULL,NULL,NULL);
	      g_free(tmp);

	      unlink (cfile->info_file);
	      lives_system(com,FALSE);
	      g_free(com);
	      tmp=NULL;
	    
	      // if we have a quick-opening file, display the first and last frames now
	      // for some codecs this can be helpful since we can locate the last frame while audio is loading
	      if (cfile->clip_type==CLIP_TYPE_FILE&&mainw->playing_file==-1) resize(1);

	      mainw->effects_paused=FALSE; // set to TRUE if user clicks "Enough"

	      msgstr=g_strdup_printf(_("Opening audio"),file_name);
	      if (!do_progress_dialog(TRUE,TRUE,msgstr)) {
		// user cancelled or switched to another clip
	      
		g_free(msgstr);
	      
		mainw->opening_frames=-1;
	      
		if (mainw->multitrack!=NULL) {
		  mainw->multitrack->pb_start_event=mt_pb_start_event;
		  mainw->multitrack->has_audio_file=mt_has_audio_file;
		}

		if (mainw->cancelled==CANCEL_NO_PROPOGATE) {
		  lives_set_cursor_style(LIVES_CURSOR_NORMAL,NULL);
		  mainw->cancelled=CANCEL_NONE;
		  return;
		}
	      
		// cancelled

		if (mainw->cancelled!=CANCEL_ERROR) {
#ifndef IS_MINGW
		  // clean up our temp files
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

		if (mainw->file_open_params!=NULL) g_free (mainw->file_open_params);
		mainw->file_open_params=NULL;
		close_current_file(old_file);
		lives_set_cursor_style(LIVES_CURSOR_NORMAL,NULL);
		return;
	      }
	      if (mainw->error==0) add_file_info (cfile->handle,TRUE);
	      mainw->error=0;
	      g_free(msgstr);

	      cfile->opening=FALSE;
	      reget_afilesize(mainw->current_file);
	      get_total_time(cfile);

	      if (prefs->auto_trim_audio) {
		if ((cdata->sync_hint&SYNC_HINT_VIDEO_PAD_START)&&cdata->video_start_time<=1.) {
		  // pad with blank frames at start
		  int extra_frames=cdata->video_start_time*cfile->fps+.5;
		  insert_blank_frames(mainw->current_file,extra_frames,0);
		  load_start_image(cfile->start);
		  load_end_image(cfile->end);
		}
		if ((cdata->sync_hint&SYNC_HINT_VIDEO_PAD_END)&&(double)cfile->frames/cfile->fps<cfile->laudio_time) {
		  // pad with blank frames at end
		  int extra_frames=(cfile->laudio_time-(double)cfile->frames/cfile->fps)*cfile->fps+.5;
		  insert_blank_frames(mainw->current_file,extra_frames,cfile->frames);
		  cfile->end=cfile->frames;
		  load_end_image(cfile->end);
		}
		if (cfile->total_time>cfile->video_time) {
		  if (cdata->sync_hint&SYNC_HINT_AUDIO_TRIM_START) {
		    cfile->undo1_dbl=0.;
		    cfile->undo2_dbl=cfile->total_time-cfile->video_time;
		    msgstr=g_strdup_printf(_ ("Auto trimming %.2f seconds of audio at start..."),cfile->undo2_dbl);
		    d_print(msgstr);
		    g_free(msgstr);
		    if (on_del_audio_activate(NULL,NULL)) d_print_done();
		    else d_print("\n");
		    cfile->changed=FALSE;
		  }
		  if (cdata->sync_hint&SYNC_HINT_AUDIO_TRIM_END) {
		    cfile->undo1_dbl=cfile->laudio_time;
		    cfile->undo2_dbl=cfile->total_time-cfile->video_time;
		    msgstr=g_strdup_printf(_ ("Auto trimming %.2f seconds of audio at end..."),cfile->undo2_dbl);
		    d_print(msgstr);
		    g_free(msgstr);
		    if (on_del_audio_activate(NULL,NULL)) d_print_done();
		    else d_print("\n");
		    cfile->changed=FALSE;
		  }
		}
		if (!mainw->effects_paused&&cfile->afilesize>0&&cfile->total_time>cfile->laudio_time) {
		  if (cdata->sync_hint&SYNC_HINT_AUDIO_PAD_START) {
		    cfile->undo1_dbl=0.;
		    cfile->undo2_dbl=cfile->total_time-cfile->laudio_time;
		    cfile->undo_arate=cfile->arate;
		    cfile->undo_signed_endian=cfile->signed_endian;
		    cfile->undo_achans=cfile->achans;
		    cfile->undo_asampsize=cfile->asampsize;
		    cfile->undo_arps=cfile->arps;
		    msgstr=g_strdup_printf(_ ("Auto padding with %.2f seconds of silence at start..."),cfile->undo2_dbl);
		    d_print(msgstr);
		    g_free(msgstr);
		    if (on_ins_silence_activate(NULL,NULL)) d_print_done();
		    else d_print("\n");
		    cfile->changed=FALSE;
		  }
		  if (cdata->sync_hint&SYNC_HINT_AUDIO_PAD_END) {
		    cfile->undo1_dbl=cfile->laudio_time;
		    cfile->undo2_dbl=cfile->total_time-cfile->laudio_time;
		    cfile->undo_arate=cfile->arate;
		    cfile->undo_signed_endian=cfile->signed_endian;
		    cfile->undo_achans=cfile->achans;
		    cfile->undo_asampsize=cfile->asampsize;
		    cfile->undo_arps=cfile->arps;
		    msgstr=g_strdup_printf(_ ("Auto padding with %.2f seconds of silence at end..."),cfile->undo2_dbl);
		    d_print(msgstr);
		    g_free(msgstr);
		    if (on_ins_silence_activate(NULL,NULL)) d_print_done();
		    else d_print("\n");
		    cfile->changed=FALSE;
		  }
		}
	      }
	    }
	  }
	}
      }

      get_mime_type(cfile->type,40,cdata);

      save_frame_index(mainw->current_file);

      if (tmp!=NULL) g_free(tmp);
      tmp=NULL;
    }

    
    if (cfile->ext_src!=NULL) {
      if (mainw->open_deint) {
	// override what the plugin says
	cfile->deinterlace=TRUE;
	cfile->interlace=LIVES_INTERLACE_TOP_FIRST; // guessing
	save_clip_value(mainw->current_file,CLIP_DETAILS_INTERLACE,&cfile->interlace);
	if (mainw->com_failed||mainw->write_failed) do_header_write_error(mainw->current_file);
      }
    }

    else {
      // get the file size, etc. (frames is just a guess here)
      if (!read_file_details(file_name,FALSE)) {
	// user cancelled
	close_current_file(old_file);
	mainw->noswitch=FALSE;
	if (mainw->multitrack!=NULL) {
	  mainw->multitrack->pb_start_event=mt_pb_start_event;
	  mainw->multitrack->has_audio_file=mt_has_audio_file;
	}
	if (mainw->file_open_params!=NULL) g_free (mainw->file_open_params);
	mainw->file_open_params=NULL;
	lives_set_cursor_style(LIVES_CURSOR_NORMAL,NULL);
	return;
      }
      unlink (cfile->info_file);

      // we must set this before calling add_file_info
      cfile->opening=TRUE;
      mainw->opening_frames=-1;

      if (!add_file_info (cfile->handle,FALSE)) {
	close_current_file(old_file);
	mainw->noswitch=FALSE;
	if (mainw->multitrack!=NULL) {
	  mainw->multitrack->pb_start_event=mt_pb_start_event;
	  mainw->multitrack->has_audio_file=mt_has_audio_file;
	}
	if (mainw->file_open_params!=NULL) g_free (mainw->file_open_params);
	mainw->file_open_params=NULL;
	lives_set_cursor_style(LIVES_CURSOR_NORMAL,NULL);
	return;
      }

      if (frames>0&&cfile->frames>frames) cfile->end=cfile->undo_end=cfile->frames=frames;
      
      // be careful, here we switch from mainw->opening_loc to cfile->opening_loc
      if (mainw->opening_loc) {
	cfile->opening_loc=TRUE;
	mainw->opening_loc=FALSE;
      }
    
      if (cfile->f_size>prefs->warn_file_size*1000000.&&mainw->is_ready&&frames==0) {
	gchar *fsize_ds=lives_format_storage_space_string((uint64_t)cfile->f_size);
	gchar *warn=g_strdup_printf(_ ("\nLiVES is not currently optimised for larger file sizes.\nYou are advised (for now) to start with a smaller file, or to use the 'Open File Selection' option.\n(Filesize=%s)\n\nAre you sure you wish to continue ?"),fsize_ds);
	g_free(fsize_ds);
	if (!do_warning_dialog_with_check(warn,WARN_MASK_FSIZE)) {
	  g_free(warn);
	  close_current_file(old_file);
	  mainw->noswitch=FALSE;
	  if (mainw->multitrack!=NULL) {
	    mainw->multitrack->pb_start_event=mt_pb_start_event;
	    mainw->multitrack->has_audio_file=mt_has_audio_file;
	  }
	  lives_set_cursor_style(LIVES_CURSOR_NORMAL,NULL);
	  return;
	}
	g_free(warn);
	d_print(_ (" - please be patient."));
      }
      
      d_print("\n");
#if defined DEBUG
      g_print("open_file: dpd in\n");
#endif
    }

    // set undo_start and undo_end for preview
    cfile->undo_start=1;
    cfile->undo_end=cfile->frames;

    if (cfile->achans>0) {
      cfile->opening_audio=TRUE;
    }

    // these will get reset as we have no audio file yet, so preserve them
    achans=cfile->achans;
    arate=cfile->arate;
    arps=cfile->arps;
    asampsize=cfile->asampsize;
    cfile->old_frames=cfile->frames;
    cfile->frames=0;

    // we need this FALSE here, otherwise we will switch straight back here...
    cfile->opening=FALSE;

    // force a resize
    current_file=mainw->current_file;

    if (mainw->playing_file>-1) {
      do_quick_switch (current_file);
    }
    else {
      switch_to_file((mainw->current_file=(cfile->clip_type!=CLIP_TYPE_FILE)?old_file:current_file),current_file);
    }

    cfile->opening=TRUE;
    cfile->achans=achans;
    cfile->arate=arate;
    cfile->arps=arps;
    cfile->asampsize=asampsize;
    cfile->frames=cfile->old_frames;

    if (cfile->frames<=0) {
      cfile->undo_end=cfile->frames=123456789;
    }
    if (cfile->hsize*cfile->vsize==0) {
      cfile->frames=0;
    }

    if (mainw->multitrack==NULL) get_play_times();

    add_to_winmenu();
    set_main_title(cfile->file_name,0);

    mainw->effects_paused=FALSE;

    if (cfile->ext_src==NULL) {
      if (mainw->file_open_params==NULL) mainw->file_open_params=g_strdup("");

      tmp=g_strconcat(mainw->file_open_params,get_deinterlace_string(),NULL);
      g_free(mainw->file_open_params);
      mainw->file_open_params=tmp;
      
      com=g_strdup_printf("%s open \"%s\" \"%s\" %d \"%s\" %.2f %d \"%s\"",prefs->backend,cfile->handle,
			  (tmp=g_filename_from_utf8 (file_name,-1,NULL,NULL,NULL)),withsound,
			  prefs->image_ext,start,frames,mainw->file_open_params);
      unlink (cfile->info_file);
      lives_system(com,FALSE);
      g_free(com);
      g_free(tmp);
      
      if (mainw->toy_type==LIVES_TOY_TV) {
	// for LiVES TV we do an auto-preview
	mainw->play_start=cfile->start=cfile->undo_start;
	mainw->play_end=cfile->end=cfile->undo_end;
	mainw->preview=TRUE;
	do {
	  desensitize();
	  procw_desensitize();
	  on_playsel_activate (NULL, NULL);
	} while (mainw->cancelled==CANCEL_KEEP_LOOPING);
	mainw->preview=FALSE;
	on_toy_activate(NULL,GINT_TO_POINTER(LIVES_TOY_NONE));
	g_free (mainw->file_open_params);
	mainw->file_open_params=NULL;
	mainw->cancelled=CANCEL_NONE;
	lives_set_cursor_style(LIVES_CURSOR_NORMAL,NULL);
	mainw->noswitch=FALSE;
	return;
      }
    }

    //  loading:

    // 'entry point' when we switch back
    
    // spin until loading is complete
    // afterwards, mainw->msg will contain file details
    cfile->progress_start=cfile->progress_end=0;
    
    // (also check for cancel)
    msgstr=g_strdup_printf(_("Opening %s"),file_name);
    
    if (cfile->ext_src==NULL&&mainw->toy_type!=LIVES_TOY_TV) {
      if (!do_progress_dialog(TRUE,TRUE,msgstr)) {
	// user cancelled or switched to another clip
#ifdef IS_MINGW
	FILE *rfile;
	ssize_t rlen;
	char val[16];
	int pid;
#endif

	g_free(msgstr);
	
	mainw->opening_frames=-1;
	mainw->effects_paused=FALSE;
	
	if (mainw->cancelled==CANCEL_NO_PROPOGATE) {
	  mainw->cancelled=CANCEL_NONE;
	  lives_set_cursor_style(LIVES_CURSOR_NORMAL,NULL);
	  mainw->noswitch=FALSE;
	  return;
	}
	
	// cancelled
	// clean up our temp files
#ifndef IS_MINGW
	com=g_strdup_printf("%s stopsubsub \"%s\" 2>/dev/null",prefs->backend_sync,cfile->handle);
	lives_system(com,TRUE);
#else
	// get pid from backend
	com=g_strdup_printf("%s get_pid_for_handle \"%s\"",prefs->backend_sync,cfile->handle);
	rfile=popen(com,"r");
	rlen=fread(val,1,16,rfile);
	pclose(rfile);
	memset(val+rlen,0,1);
	pid=atoi(val);
	
	lives_win32_kill_subprocesses(pid,TRUE);
#endif
	g_free(com);
	if (mainw->file_open_params!=NULL) g_free (mainw->file_open_params);
	mainw->file_open_params=NULL;
	close_current_file(old_file);
	if (mainw->multitrack!=NULL) {
	  mainw->multitrack->pb_start_event=mt_pb_start_event;
	  mainw->multitrack->has_audio_file=mt_has_audio_file;
	}
	mainw->noswitch=FALSE;
	lives_set_cursor_style(LIVES_CURSOR_NORMAL,NULL);
	return;
      }
    }
    g_free(msgstr);
  }

  if (cfile->ext_src!=NULL&&cfile->achans>0) {
    gchar *afile=g_strdup_printf("%s/%s/audiodump.pcm",prefs->tmpdir,cfile->handle);
    gchar *ofile=g_strdup_printf("%s/%s/audio",prefs->tmpdir,cfile->handle);
    rename(afile,ofile);
    g_free(afile);
    g_free(ofile);
  }

  cfile->opening=cfile->opening_audio=cfile->opening_only_audio=FALSE;
  mainw->opening_frames=-1;
  mainw->effects_paused=FALSE;

#if defined DEBUG
  g_print("Out of dpd\n");
#endif

  if (mainw->multitrack!=NULL) {
    mainw->multitrack->pb_start_event=mt_pb_start_event;
    mainw->multitrack->has_audio_file=mt_has_audio_file;
  }

  // mainw->error is TRUE if we could not open the file
  if (mainw->error) {
    do_blocking_error_dialog(mainw->msg);
    d_print_failed();
    close_current_file(old_file);
    mainw->noswitch=FALSE;
    if (mainw->file_open_params!=NULL) g_free (mainw->file_open_params);
    mainw->file_open_params=NULL;
    lives_set_cursor_style(LIVES_CURSOR_NORMAL,NULL);
    return;
  }

  if (cfile->opening_loc) {
    cfile->changed=TRUE;
    cfile->opening_loc=FALSE;
  }

  else {
    if (prefs->autoload_subs) {
      gchar filename[512];
      gchar *subfname;
      lives_subtitle_type_t subtype=SUBTITLE_TYPE_NONE;

      g_snprintf(filename,512,"%s",file_name);
      get_filename(filename,FALSE); // strip extension
      isubfname=g_strdup_printf("%s.srt",filename);
      if (g_file_test(isubfname,G_FILE_TEST_EXISTS)) {
	subfname=g_build_filename(prefs->tmpdir,cfile->handle,"subs.srt",NULL);
	subtype=SUBTITLE_TYPE_SRT;
      }
      else {
	g_free(isubfname);
	isubfname=g_strdup_printf("%s.sub",filename);
	if (g_file_test(isubfname,G_FILE_TEST_EXISTS)) {
	  subfname=g_build_filename(prefs->tmpdir,cfile->handle,"subs.sub",NULL);
	  subtype=SUBTITLE_TYPE_SUB;
	}
      }
      if (subtype!=SUBTITLE_TYPE_NONE) {
#ifndef IS_MINGW
	com=g_strdup_printf("/bin/cp \"%s\" \"%s\"",isubfname,subfname);
#else
	com=g_strdup_printf("cp.exe \"%s\" \"%s\"",isubfname,subfname);
#endif
	mainw->com_failed=FALSE;
	lives_system(com,FALSE);
	g_free(com);
	if (!mainw->com_failed) 
	  subtitles_init(cfile,subfname,subtype);
	g_free(subfname);
      }
      else {
	g_free(isubfname);
	isubfname=NULL;
      }
    }
  }


  // now file should be loaded...get full details
  cfile->is_loaded=TRUE;

  if (cfile->ext_src==NULL) add_file_info(cfile->handle,FALSE);
  else {
    add_file_info(NULL,FALSE);
    cfile->f_size=sget_file_size((gchar *)file_name);
  }

  if (cfile->frames<=0) {
    if (cfile->afilesize==0l) {
      // we got neither video nor audio...
      g_snprintf (msg,256,"%s",_ 
		  ("\n\nLiVES was unable to extract either video or audio.\nPlease check the terminal window for more details.\n"));
      get_location ("mplayer",loc,256);
      if (!capable->has_mplayer) {
	g_strappend (msg,256,_ ("\n\nYou may need to install mplayer to open this file.\n"));
      }
      else if (strcmp (prefs->video_open_command,loc)) {
	g_strappend (msg,256,_ ("\n\nPlease check the setting of Video open command in\nTools|Preferences|Decoding\n"));
      }

      do_error_dialog(msg);
      d_print_failed();
      close_current_file(old_file);
      mainw->noswitch=FALSE;
      if (mainw->multitrack!=NULL) {
	mainw->multitrack->pb_start_event=mt_pb_start_event;
	mainw->multitrack->has_audio_file=mt_has_audio_file;
      }
      if (mainw->file_open_params!=NULL) g_free (mainw->file_open_params);
      mainw->file_open_params=NULL;
      lives_set_cursor_style(LIVES_CURSOR_NORMAL,NULL);
      return;
    }
    cfile->frames=0;
  }

  current_file=mainw->current_file;

  if (isubfname!=NULL) {
    tmp=g_strdup_printf(_("Loaded subtitle file: %s\n"),isubfname);
    d_print(tmp);
    g_free(tmp);
    g_free(isubfname);
  }


#ifdef ENABLE_OSC
    lives_osc_notify(LIVES_OSC_NOTIFY_CLIP_OPENED,"");
#endif

  if (prefs->show_recent&&!mainw->is_generating) {
    add_to_recent(file_name,start,frames,mainw->file_open_params);
  }
  if (mainw->file_open_params!=NULL) g_free (mainw->file_open_params);
  mainw->file_open_params=NULL;

  if (!strcmp(cfile->type,"Frames")||!strcmp(cfile->type,"jpeg")||!strcmp(cfile->type,"png")||!strcmp(cfile->type,"Audio")) {
    cfile->is_untitled=TRUE;
  }

  if (cfile->frames==1&&(!strcmp(cfile->type,"jpeg")||!strcmp(cfile->type,"png"))) {
    if (mainw->img_concat_clip==-1) mainw->img_concat_clip=mainw->current_file;
    else if (prefs->concat_images) {
      // insert this image into our image clip, close this file

      com=g_strdup_printf("%s insert \"%s\" \"%s\" %d 1 1 \"%s\" 0 %d %d %d",prefs->backend,
			  mainw->files[mainw->img_concat_clip]->handle,
			  get_image_ext_for_type(mainw->files[mainw->img_concat_clip]->img_type),
			  mainw->files[mainw->img_concat_clip]->frames,
			  cfile->handle,mainw->files[mainw->img_concat_clip]->frames,
			  mainw->files[mainw->img_concat_clip]->hsize,mainw->files[mainw->img_concat_clip]->vsize);

      mainw->current_file=mainw->img_concat_clip;

      unlink (cfile->info_file);

      mainw->cancelled=CANCEL_NONE;
      mainw->error=FALSE;
      mainw->com_failed=FALSE;

      lives_system(com,FALSE);
      g_free(com);

      do_auto_dialog(_("Adding image..."),2);

      if (current_file!=mainw->img_concat_clip) {
	mainw->current_file=current_file;
	close_current_file(mainw->img_concat_clip);
      }

      if (mainw->cancelled||mainw->error) {
	goto load_done;
      }

      cfile->frames++;
      cfile->end++;

      g_signal_handler_block(mainw->spinbutton_end,mainw->spin_end_func);
      lives_spin_button_set_range(LIVES_SPIN_BUTTON(mainw->spinbutton_end),cfile->frames==0?0:1,cfile->frames);
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_end),cfile->end);
      g_signal_handler_unblock(mainw->spinbutton_end,mainw->spin_end_func);
       
      g_signal_handler_block(mainw->spinbutton_start,mainw->spin_start_func);
      lives_spin_button_set_range(LIVES_SPIN_BUTTON(mainw->spinbutton_start),cfile->frames==0?0:1,cfile->frames);
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_start),cfile->start);
      g_signal_handler_unblock(mainw->spinbutton_start,mainw->spin_start_func);
      lives_set_cursor_style(LIVES_CURSOR_NORMAL,NULL);
      mainw->noswitch=FALSE;

      return;
    }
  }

  // set new style file details
  if (!save_clip_values(current_file)) {
    close_current_file(old_file);
    mainw->noswitch=FALSE;
    return;
  }

  if (prefs->crash_recovery) add_to_recovery_file(cfile->handle);

load_done:
  mainw->noswitch=FALSE;

  if (mainw->multitrack==NULL) {
    // update widgets
    switch_to_file((mainw->current_file=0),current_file);
  }
  else {
    lives_mt *multi=mainw->multitrack;
    mainw->multitrack=NULL; // allow getting of afilesize
    reget_afilesize (mainw->current_file);
    mainw->multitrack=multi;
    get_total_time(cfile);
    if (!mainw->is_generating) mainw->current_file=mainw->multitrack->render_file;
    mt_init_clips(mainw->multitrack,current_file,TRUE);
    lives_widget_context_update();
    mt_clip_select(mainw->multitrack,TRUE);
  }
  lives_set_cursor_style(LIVES_CURSOR_NORMAL,NULL);
}



static void save_subs_to_file(file *sfile, gchar *fname) {
  gchar *msg,*ext;
  lives_subtitle_type_t otype,itype;

  if (sfile->subt==NULL) return;

  itype=sfile->subt->type;

  ext=get_extension(fname);

  if (!strcmp(ext,"sub")) otype=SUBTITLE_TYPE_SUB;
  else if (!strcmp(ext,"srt")) otype=SUBTITLE_TYPE_SRT;
  else otype=itype;

  g_free(ext);

  // TODO - use sfile->subt->save_fn
  switch (otype) {
    case SUBTITLE_TYPE_SUB:
      save_sub_subtitles(sfile,(double)(sfile->start-1)/sfile->fps,(double)sfile->end/sfile->fps,
			 (double)(sfile->start-1)/sfile->fps,fname);
      break;

    case SUBTITLE_TYPE_SRT:
      save_srt_subtitles(sfile,(double)(sfile->start-1)/sfile->fps,(double)sfile->end/sfile->fps,
			 (double)(sfile->start-1)/sfile->fps,fname);
      break;
      
    default:
    return;
  }

  msg=g_strdup_printf(_("Subtitles were saved as %s\n"),mainw->subt_save_file);
  d_print(msg);
  g_free(msg);
}






boolean get_handle_from_info_file(int index) {
  // called from get_new_handle to get the 'real' file handle
  // because until we know the handle we can't use the normal info file yet

  // return FALSE if we time out or get an error or the user cancels

  FILE *infofile;
  int alarm_handle;
  int retval;
  boolean timeout;

  do {
    retval=0;
    timeout=FALSE;
    clear_mainw_msg();

#define LIVES_MEDIUM_TIMEOUT  (10 * U_SEC) // 60 sec timeout

    alarm_handle=lives_alarm_set(LIVES_MEDIUM_TIMEOUT);

    while (!((infofile=fopen(mainw->first_info_file,"r")) || (timeout=lives_alarm_get(alarm_handle)))) {
      g_usleep(prefs->sleep_time);
    }

    lives_alarm_clear(alarm_handle);
  
    if (!timeout) {
      mainw->read_failed=FALSE;
      lives_fgets(mainw->msg,256,infofile);
      fclose(infofile);
    }

    if (timeout || mainw->read_failed) {
      retval=do_read_failed_error_s_with_retry(mainw->first_info_file,NULL,NULL);
    }
  } while (retval==LIVES_RETRY);
  
  if (retval==LIVES_CANCEL) {
    mainw->read_failed=FALSE;
    return FALSE;
  }

  unlink(mainw->first_info_file);

  if (!strncmp(mainw->msg,"error|",6)) {
    handle_backend_errors();
    return FALSE;
  }

  if (mainw->files[index]==NULL) {
    mainw->files[index]=(file *)(g_malloc(sizeof(file)));
    mainw->files[index]->clip_type=CLIP_TYPE_DISK; // the default
  }
  g_snprintf(mainw->files[index]->handle,256,"%s",mainw->msg);

  return TRUE;
}




void save_frame (GtkMenuItem *menuitem, gpointer user_data) {
  int frame;
  // save a single frame from a clip
  gchar *filt[2];
  gchar *ttl;
  char *filename;

  filt[0]=g_strdup_printf ("*.%s",get_image_ext_for_type(cfile->img_type));
  filt[1]=NULL;

  frame=GPOINTER_TO_INT(user_data);

  if (frame>0)
    ttl=g_strdup_printf (_("LiVES: Save Frame %d as..."),frame);

  else
    ttl=g_strdup (_("LiVES: Save Frame as..."));


  filename=choose_file(strlen(mainw->image_dir)?mainw->image_dir:NULL,NULL,filt,LIVES_FILE_CHOOSER_ACTION_SAVE,ttl,NULL);

  g_free (filt[0]);
  g_free (ttl);

  if (filename==NULL||!strlen(filename)) return;


  if (!save_frame_inner(mainw->current_file,GPOINTER_TO_INT (frame),filename,-1,-1,FALSE)) return;

  g_snprintf(mainw->image_dir,PATH_MAX,"%s",filename);
  get_dirname(mainw->image_dir);
  if (prefs->save_directories) {
    gchar *tmp;
    set_pref ("image_dir",(tmp=g_filename_from_utf8(mainw->image_dir,-1,NULL,NULL,NULL)));
    g_free(tmp);
  }

}






void save_file (int clip, int start, int end, const char *filename) {
  // save clip from frame start to frame end
  file *sfile=mainw->files[clip],*nfile=NULL;

  double aud_start=0.,aud_end=0.;

  const char *n_file_name;
  gchar *fps_string;
  gchar *extra_params=NULL;
#ifndef IS_MINGW
  gchar *redir=g_strdup("1>&2 2>/dev/null");
#else
  gchar *redir=g_strdup("1>&2 2>NUL");
#endif
  gchar *new_stderr_name=NULL;
  gchar *mesg,*bit,*tmp;
  gchar *com;
  gchar *full_file_name=NULL;
  gchar *enc_exec_name,*cmd;

  int new_stderr=-1;
  int retval;
  int startframe=1;
  int current_file=mainw->current_file;
  int asigned=!(sfile->signed_endian&AFORM_UNSIGNED); // 1 is signed (in backend)
  int aendian=(sfile->signed_endian&AFORM_BIG_ENDIAN); // 2 is bigend
  int arate;
  int new_file=-1;

  GError *gerr=NULL;

  struct stat filestat;

  time_t file_mtime=0;

  uint64_t fsize;

  GtkWidget *hbox;

  boolean safe_symlinks=prefs->safe_symlinks;
  boolean not_cancelled;
  boolean output_exists=FALSE;
  boolean save_all=FALSE;
  boolean resb;

  if (start==1&&end==sfile->frames) save_all=TRUE;

  // new handling for save selection:
  // symlink images 1 - n to the encoded frames
  // symlinks are now created in /tmp (for dynebolic)
  // then encode the symlinked frames

  if (filename==NULL) {
    // prompt for encoder type/output format
    if (prefs->show_rdet) {
      int response;
      rdet=create_render_details(1); // WARNING !! - rdet is global in events.h
      response=lives_dialog_run(LIVES_DIALOG(rdet->dialog));
      lives_widget_hide (rdet->dialog);
      
      if (response==GTK_RESPONSE_CANCEL) {
	lives_widget_destroy (rdet->dialog);
	g_free(rdet->encoder_name);
	g_free(rdet);
	rdet=NULL;
	if (resaudw!=NULL) g_free(resaudw);
	resaudw=NULL;
	return;
      }
    }
  }

  // get file extension
  check_encoder_restrictions (TRUE,FALSE,save_all);

  hbox = lives_hbox_new (FALSE, 0);
  mainw->fx1_bool=TRUE;
  add_suffix_check(LIVES_BOX(hbox),prefs->encoder.of_def_ext);
  lives_widget_show_all(hbox);

  if (filename==NULL) {
    gchar *ttl=g_strdup (_("LiVES: Save Clip as..."));
    do {
      n_file_name=choose_file(mainw->vid_save_dir,NULL,NULL,LIVES_FILE_CHOOSER_ACTION_SAVE,ttl,hbox);
      if (n_file_name==NULL) return;
    } while (!strlen(n_file_name));
    g_snprintf(mainw->vid_save_dir,PATH_MAX,"%s",n_file_name);
    get_dirname(mainw->vid_save_dir);
    if (prefs->save_directories) {
      set_pref ("vid_save_dir",(tmp=g_filename_from_utf8(mainw->vid_save_dir,-1,NULL,NULL,NULL)));
      g_free(tmp);
    }
    g_free(ttl);
  }
  else n_file_name=filename;

  //append default extension (if necessary)
  if (!strlen (prefs->encoder.of_def_ext)) {
    // encoder adds its own extension
    if (strrchr(n_file_name,'.')!=NULL) {
      memset ((void *)strrchr (n_file_name,'.'),0,1);
    }
  }
  else {
    if (mainw->fx1_bool&&(strlen(n_file_name)<=strlen(prefs->encoder.of_def_ext)||
			  strncmp(n_file_name+strlen(n_file_name)-strlen(prefs->encoder.of_def_ext)-1,".",1)||
			  strcmp(n_file_name+strlen(n_file_name)-strlen(prefs->encoder.of_def_ext),
				 prefs->encoder.of_def_ext))) {
      full_file_name=g_strconcat(n_file_name,".",prefs->encoder.of_def_ext,NULL);
    }
  }

  if (full_file_name==NULL) {
    full_file_name=g_strdup (n_file_name);
  }

  if (filename==NULL) {
    if (!check_file(full_file_name,strcmp(full_file_name,n_file_name))) {
      g_free(full_file_name);
      if (rdet!=NULL) {
	lives_widget_destroy (rdet->dialog);
	g_free(rdet->encoder_name);
	g_free(rdet);
	rdet=NULL;
	if (resaudw!=NULL) g_free(resaudw);
	resaudw=NULL;
      }
      return;
    }
    sfile->orig_file_name=FALSE;
    if (!strlen (sfile->comment)) {
      g_snprintf (sfile->comment,251,"Created with LiVES");
    }
    if (!do_comments_dialog(sfile,full_file_name)) {
	g_free(full_file_name);
	if (rdet!=NULL) {
	  lives_widget_destroy (rdet->dialog);
	  g_free(rdet->encoder_name);
	  g_free(rdet);
	  rdet=NULL;
	  if (resaudw!=NULL) g_free(resaudw);
	  resaudw=NULL;
	}
	if (mainw->subt_save_file!=NULL) g_free(mainw->subt_save_file);
	mainw->subt_save_file=NULL;
	return;
    }
  }
  else if (!mainw->osc_auto&&sfile->orig_file_name) {
    gchar *warn=g_strdup(_("Saving your video could lead to a loss of quality !\nYou are strongly advised to 'Save As' to a new file.\n\nDo you still wish to continue ?"));
    if (!do_warning_dialog_with_check(warn,WARN_MASK_SAVE_QUALITY)) {
	g_free(warn);
	g_free(full_file_name);
	if (rdet!=NULL) {
	  lives_widget_destroy (rdet->dialog);
	  g_free(rdet->encoder_name);
	  g_free(rdet);
	  if (resaudw!=NULL) g_free(resaudw);
	  resaudw=NULL;
	  rdet=NULL;
	}
	if (mainw->subt_save_file!=NULL) g_free(mainw->subt_save_file);
	mainw->subt_save_file=NULL;
	return;
    }
    g_free(warn);
  }


  if (rdet!=NULL) {
    lives_widget_destroy (rdet->dialog);
    g_free(rdet->encoder_name);
    g_free(rdet);
    rdet=NULL;
    if (resaudw!=NULL) g_free(resaudw);
    resaudw=NULL;
  }


  if (sfile->arate*sfile->achans) {
    aud_start=calc_time_from_frame (clip,start*sfile->arps/sfile->arate);
    aud_end=calc_time_from_frame (clip,(end+1)*sfile->arps/sfile->arate);
  }


  // get extra params for encoder

  if (!sfile->ratio_fps) {
    fps_string=g_strdup_printf ("%.3f",sfile->fps);
  }
  else {
    fps_string=g_strdup_printf ("%.8f",sfile->fps);
  }

  enc_exec_name=g_build_filename(prefs->lib_dir,PLUGIN_EXEC_DIR,PLUGIN_ENCODERS,prefs->encoder.name,NULL);

#ifdef IS_MINGW
  //cmd=g_strdup_printf("START perl ");  // real windows : does encode but no redir
  cmd=g_strdup("perl ");   // mingw : does encode, but no redirection
#else
  cmd=g_strdup("");
#endif

  arate=sfile->arate;

  if (!mainw->save_with_sound||prefs->encoder.of_allowed_acodecs==0) {
    arate=0;
  }

  // get extra parameters for saving
  if (prefs->encoder.capabilities&HAS_RFX) {

    // pull at least one frame so we know the file ext
    if (sfile->clip_type==CLIP_TYPE_FILE) {
      resb=virtual_to_images(clip,start,end,TRUE,NULL);
      
      if (!resb) {
	if (mainw->subt_save_file!=NULL) g_free(mainw->subt_save_file);
	mainw->subt_save_file=NULL;
	d_print_file_error_failed();
	return;
      }
    }

    if (prefs->encoder.capabilities&ENCODER_NON_NATIVE) {
      com=g_strdup_printf("%s save get_rfx %s \"%s\" %s \"%s\" %d %d %d %d %d %d %.4f %.4f",prefs->backend,
			  sfile->handle,enc_exec_name,
			  fps_string,(tmp=g_filename_from_utf8(full_file_name,-1,NULL,NULL,NULL)),
			  start,end,arate,sfile->achans,sfile->asampsize,asigned|aendian,aud_start,aud_end);
      g_free(tmp);
    }
    else {
      com=g_strdup_printf("%s\"%s\" save get_rfx %s \"\" %s \"%s\" %d %d %d %d %d %d %.4f %.4f",
			  cmd, enc_exec_name,sfile->handle,
			  fps_string,(tmp=g_filename_from_utf8(full_file_name,-1,NULL,NULL,NULL)),
			  start,end,arate,sfile->achans,sfile->asampsize,asigned|aendian,aud_start,aud_end);
      g_free(tmp);
    }
    extra_params=plugin_run_param_window(com,NULL,NULL);
    g_free(com);

    if (extra_params==NULL) {
      if (fx_dialog[1]!=NULL) {
	lives_widget_destroy(fx_dialog[1]);
	fx_dialog[1]=NULL;
      }
      g_free(fps_string);
      switch_to_file(mainw->current_file,current_file);
      if (mainw->subt_save_file!=NULL) g_free(mainw->subt_save_file);
      mainw->subt_save_file=NULL;
      return;
    }
  }


  if (!save_all&&!safe_symlinks) {
    // we are saving a selection - make symlinks from a temporary clip

    if ((new_file=mainw->first_free_file)==-1) {
      too_many_files();
      if (mainw->subt_save_file!=NULL) g_free(mainw->subt_save_file);
      mainw->subt_save_file=NULL;
      return;
    }

    // create new clip
    if (!get_new_handle(new_file,g_strdup (_("selection")))) {
      if (mainw->subt_save_file!=NULL) g_free(mainw->subt_save_file);
      mainw->subt_save_file=NULL;
      return;
    }

    if (sfile->clip_type==CLIP_TYPE_FILE) {
      mainw->cancelled=CANCEL_NONE;
      cfile->progress_start=1;
      cfile->progress_end=count_virtual_frames(sfile->frame_index,start,end);
      do_threaded_dialog(_("Pulling frames from clip"),TRUE);
      resb=virtual_to_images(clip,start,end,TRUE,NULL);
      end_threaded_dialog();
      
      if (mainw->cancelled!=CANCEL_NONE||!resb) {
	mainw->cancelled=CANCEL_USER;
	if (mainw->subt_save_file!=NULL) g_free(mainw->subt_save_file);
	mainw->subt_save_file=NULL;
	if (!resb) d_print_file_error_failed();
	return;
      }
    }

    mainw->effects_paused=FALSE;

    nfile=mainw->files[new_file];
    nfile->hsize=sfile->hsize;
    nfile->vsize=sfile->vsize;
    cfile->progress_start=nfile->start=1;
    cfile->progress_end=nfile->frames=nfile->end=end-start+1;
    nfile->fps=sfile->fps;
    nfile->arps=sfile->arps;
    nfile->arate=sfile->arate;
    nfile->achans=sfile->achans;
    nfile->asampsize=sfile->asampsize;
    nfile->signed_endian=sfile->signed_endian;
    nfile->img_type=sfile->img_type;

    com=g_strdup_printf ("%s link_frames \"%s\" %d %d %.8f %.8f %d %d %d %d %d \"%s\"",prefs->backend,nfile->handle,
			 start,end,aud_start,aud_end,nfile->arate,nfile->achans,nfile->asampsize,
			 !(nfile->signed_endian&AFORM_UNSIGNED),!(nfile->signed_endian&AFORM_BIG_ENDIAN),sfile->handle);

    unlink(nfile->info_file);
    mainw->com_failed=FALSE;
    lives_system(com,FALSE);
    g_free(com);

    // TODO - eliminate this
    mainw->current_file=new_file;

    if (mainw->com_failed) {
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
      lives_system (g_strdup_printf("%s close \"%s\"",prefs->backend,cfile->handle),TRUE);
      g_free (cfile);
      cfile=NULL;
      if (mainw->first_free_file==-1||mainw->first_free_file>new_file) 
	mainw->first_free_file=new_file;
      switch_to_file(mainw->current_file,current_file);
      d_print_cancelled();
      if (mainw->subt_save_file!=NULL) g_free(mainw->subt_save_file);
      mainw->subt_save_file=NULL;
      return;
    }


    cfile->nopreview=TRUE;
    if (!(do_progress_dialog(TRUE,TRUE,_("Linking selection")))) {
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
      lives_system ((tmp=g_strdup_printf("%s close \"%s\"",prefs->backend,cfile->handle)),TRUE);
      g_free(tmp);
      g_free (cfile);
      cfile=NULL;
      if (mainw->first_free_file==-1||mainw->first_free_file>new_file) 
	mainw->first_free_file=new_file;
      switch_to_file(mainw->current_file,current_file);
      if (mainw->error) d_print_failed();
      else d_print_cancelled();
      if (mainw->subt_save_file!=NULL) g_free(mainw->subt_save_file);
      mainw->subt_save_file=NULL;
      return;
    }

    aud_start=calc_time_from_frame (new_file,1)*nfile->arps/nfile->arate;
    aud_end=calc_time_from_frame (new_file,nfile->frames+1)*nfile->arps/nfile->arate;
    cfile->nopreview=FALSE;

  }
  else mainw->current_file=clip; // for encoder restns

  if (rdet!=NULL) rdet->is_encoding=TRUE;


  if (!check_encoder_restrictions(FALSE,FALSE,save_all)) {
    if (!save_all&&!safe_symlinks) {
#ifdef IS_MINGW
      // kill any active processes: for other OSes the backend does this
      // get pid from backend
      FILE *rfile;
      ssize_t rlen;
      char val[16];
      int pid;
      com=g_strdup_printf("%s get_pid_for_handle \"%s\"",prefs->backend_sync,nfile->handle);
      rfile=popen(com,"r");
      rlen=fread(val,1,16,rfile);
      pclose(rfile);
      memset(val+rlen,0,1);
      pid=atoi(val);
      
      lives_win32_kill_subprocesses(pid,TRUE);
#endif
      lives_system ((com=g_strdup_printf("%s close \"%s\"",prefs->backend,nfile->handle)),TRUE);
      g_free(com);
      g_free (nfile);
      mainw->files[new_file]=NULL;
      if (mainw->first_free_file==-1||new_file) mainw->first_free_file=new_file;
    }
    switch_to_file(mainw->current_file,current_file);
    d_print_cancelled();
    if (mainw->subt_save_file!=NULL) g_free(mainw->subt_save_file);
    mainw->subt_save_file=NULL;
    return;
  }


  if (!save_all&&safe_symlinks) {
    // we are saving a selection - make symlinks in /tmp

    startframe=-1;

    if (sfile->clip_type==CLIP_TYPE_FILE) {
      mainw->cancelled=CANCEL_NONE;
      cfile->progress_start=1;
      cfile->progress_end=count_virtual_frames(sfile->frame_index,start,end);

      do_threaded_dialog(_("Pulling frames from clip"),TRUE);
      resb=virtual_to_images(clip,start,end,TRUE,NULL);
      end_threaded_dialog();
      
      if (mainw->cancelled!=CANCEL_NONE||!resb) {
	mainw->cancelled=CANCEL_USER;
	if (mainw->subt_save_file!=NULL) g_free(mainw->subt_save_file);
	mainw->subt_save_file=NULL;
	if (!resb) d_print_file_error_failed();
	return;
      }
    }

    com=g_strdup_printf ("%s link_frames \"%s\" %d %d %.8f %.8f %d %d %d %d %d",prefs->backend,sfile->handle,
			 start,end,aud_start,aud_end,sfile->arate,sfile->achans,sfile->asampsize,
			 !(sfile->signed_endian&AFORM_UNSIGNED),!(sfile->signed_endian&AFORM_BIG_ENDIAN));

    unlink(sfile->info_file);
    mainw->com_failed=FALSE;
    lives_system(com,FALSE);
    g_free(com);

    mainw->current_file=clip;

    if (mainw->com_failed) {
      com=g_strdup_printf("%s clear_symlinks \"%s\"",prefs->backend_sync,cfile->handle);
      lives_system (com,TRUE);
      g_free (com);
      cfile->nopreview=FALSE;
      switch_to_file(mainw->current_file,current_file);
      d_print_cancelled();
      if (mainw->subt_save_file!=NULL) g_free(mainw->subt_save_file);
      mainw->subt_save_file=NULL;
      return;
    }


    cfile->nopreview=TRUE;
    if (!(do_progress_dialog(TRUE,TRUE,_("Linking selection")))) {
      com=g_strdup_printf("%s clear_symlinks \"%s\"",prefs->backend_sync,cfile->handle);
      lives_system (com,TRUE);
      g_free (com);
      cfile->nopreview=FALSE;
      switch_to_file(mainw->current_file,current_file);
      if (mainw->error) d_print_failed();
      else d_print_cancelled();
      if (mainw->subt_save_file!=NULL) g_free(mainw->subt_save_file);
      mainw->subt_save_file=NULL;
      return;
    }

    aud_start=calc_time_from_frame (clip,1)*sfile->arps/sfile->arate;
    aud_end=calc_time_from_frame (clip,end-start+1)*sfile->arps/sfile->arate;
    cfile->nopreview=FALSE;
  }


  if (save_all) {
    if (sfile->clip_type==CLIP_TYPE_FILE) {
      mainw->cancelled=CANCEL_NONE;
      cfile->progress_start=1;
      cfile->progress_end=count_virtual_frames(sfile->frame_index,1,sfile->frames);
      do_threaded_dialog(_("Pulling frames from clip"),TRUE);
      resb=virtual_to_images(clip,1,sfile->frames,TRUE,NULL);
      end_threaded_dialog();
      
      if (mainw->cancelled!=CANCEL_NONE||!resb) {
	mainw->cancelled=CANCEL_USER;
	if (mainw->subt_save_file!=NULL) g_free(mainw->subt_save_file);
	mainw->subt_save_file=NULL;
	if (!resb) d_print_file_error_failed();
	switch_to_file(mainw->current_file,current_file);
	return;
      }
    }
  }


  if (!mainw->save_with_sound||prefs->encoder.of_allowed_acodecs==0) {
    bit=g_strdup(_ (" (with no sound)\n"));
  }
  else {
    bit=g_strdup("\n");
  }

  if (!save_all) {
    mesg=g_strdup_printf(_ ("Saving frames %d to %d%s as \"%s\" : encoder = %s : format = %s..."),
			 start,end,bit,full_file_name,prefs->encoder.name,prefs->encoder.of_desc);
  } // end selection
  else {
    mesg=g_strdup_printf(_ ("Saving frames 1 to %d%s as \"%s\" : encoder %s : format = %s..."),
			 sfile->frames,bit,full_file_name,prefs->encoder.name,prefs->encoder.of_desc);
  }
  g_free (bit);
  

  mainw->no_switch_dprint=TRUE;
  d_print (mesg);
  mainw->no_switch_dprint=FALSE;
  g_free (mesg);


  if (prefs->show_gui) {
    // open a file for stderr

#ifndef IS_MINGW
    new_stderr_name=g_build_filename(prefs->tmpdir,cfile->handle,".debug_out",NULL);
#else
    new_stderr_name=g_build_filename(prefs->tmpdir,cfile->handle,"debug_out",NULL);
#endif
    g_free(redir);

    do {
      retval=0;
      new_stderr=open(new_stderr_name,O_CREAT|O_RDONLY|O_TRUNC|O_SYNC,S_IRUSR|S_IWUSR);
      if (new_stderr<0) {
	retval=do_write_failed_error_s_with_retry(new_stderr_name,g_strerror(errno),NULL);
	if (retval==LIVES_CANCEL) redir=g_strdup("1>&2");
      }
      else {

#ifdef IS_MINGW
	setmode(new_stderr,O_BINARY);
	mainw->iochan=g_io_channel_win32_new_fd(new_stderr);
	redir=g_strdup_printf("2>&1 >\"%s\"",new_stderr_name);
#else
	mainw->iochan=g_io_channel_unix_new(new_stderr);
	redir=g_strdup_printf("2>\"%s\"",new_stderr_name);
#endif

	g_io_channel_set_encoding (mainw->iochan, NULL, NULL);
	g_io_channel_set_buffer_size(mainw->iochan,0);
	g_io_channel_set_flags(mainw->iochan,G_IO_FLAG_NONBLOCK,&gerr);
	if (gerr!=NULL) g_error_free(gerr);
	gerr=NULL;
	
	mainw->optextview=create_output_textview();
      }
    } while (retval==LIVES_RETRY);
  }
  else {
    g_free(redir);
    redir=g_strdup("1>&2");
  }

  if (g_file_test ((tmp=g_filename_from_utf8(full_file_name,-1,NULL,NULL,NULL)), G_FILE_TEST_EXISTS)) {
    stat(tmp,&filestat);
    file_mtime=filestat.st_mtime;
  }
  g_free(tmp);


  // re-read values in case they were resampled

  if (arate!=0) arate=cfile->arate;

  if (!cfile->ratio_fps) {
    fps_string=g_strdup_printf ("%.3f",cfile->fps);
  }
  else {
    fps_string=g_strdup_printf ("%.8f",cfile->fps);
  }

  // if startframe is -ve, we will use the links created for safe_symlinks - in /tmp
  // for non-safe symlinks, cfile will be our new links file
  // for save_all, cfile will be sfile

  if (prefs->encoder.capabilities&ENCODER_NON_NATIVE) {
    com=g_strdup_printf("%s save \"%s\" \"%s\" \"%s\" \"%s\" %d %d %d %d %d %d %.4f %.4f %s %s",prefs->backend,
			cfile->handle,
			enc_exec_name,fps_string,(tmp=g_filename_from_utf8(full_file_name,-1,NULL,NULL,NULL)),
			startframe,cfile->frames,arate,cfile->achans,cfile->asampsize,
			asigned|aendian,aud_start,aud_end,(extra_params==NULL)?"":extra_params,redir);
    g_free(tmp);
  }
  else {
    // for native (perl) plugins we go via the plugin
    com=g_strdup_printf("%s\"%s\" save \"%s\" \"%s\" \"%s\" \"%s\" %d %d %d %d %d %d %.4f %.4f %s %s",
			cmd, enc_exec_name, cfile->handle, "",
			fps_string,(tmp=g_filename_from_utf8(full_file_name,-1,NULL,NULL,NULL)),
			startframe,cfile->frames,arate,cfile->achans,cfile->asampsize,
			asigned|aendian,aud_start,aud_end,(extra_params==NULL?"":extra_params),redir);

    g_free(tmp);
  }
  g_free (fps_string);

  if (extra_params!=NULL) g_free(extra_params);
  extra_params=NULL;

  mainw->effects_paused=FALSE;
  cfile->nokeep=TRUE;
  
  unlink(cfile->info_file);
  mainw->write_failed=FALSE;
  save_file_comments(current_file);

  lives_system(com,FALSE);
  g_free(com);

  if (mainw->com_failed||mainw->write_failed) {
    not_cancelled=FALSE;
    mainw->error=TRUE;
  }


  if (!mainw->error) {
    gchar *pluginstr;

    not_cancelled=do_progress_dialog(TRUE,TRUE,_ ("Saving [can take a long time]"));
    mesg=g_strdup (mainw->msg);
    
    if (mainw->iochan!=NULL) {
      // flush last of stdout/stderr from plugin
      
      lives_fsync(new_stderr);
      pump_io_chan(mainw->iochan);
      
      g_io_channel_shutdown(mainw->iochan,FALSE,&gerr);
      g_io_channel_unref(mainw->iochan);
      
      if (gerr!=NULL) g_error_free(gerr);
      close(new_stderr);
      unlink(new_stderr_name);
      g_free(new_stderr_name);
      g_free(redir);
    }
    
    mainw->effects_paused=FALSE;
    cfile->nokeep=FALSE;
    
    pluginstr=g_build_filename(prefs->lib_dir, PLUGIN_EXEC_DIR, NULL);

    com=g_strdup_printf("%s plugin_clear \"%s\" 1 %d \"%s\" \"%s\" \"%s\"",prefs->backend_sync,cfile->handle,
			cfile->frames, pluginstr, PLUGIN_ENCODERS, prefs->encoder.name);
    g_free(pluginstr);

    lives_system(com,FALSE);
    g_free(com);
  }
  else {
    if (mainw->iochan!=NULL) {
      // flush last of stdout/stderr from plugin
      
      lives_fsync(new_stderr);
      pump_io_chan(mainw->iochan);
      
      g_io_channel_shutdown(mainw->iochan,FALSE,&gerr);
      g_io_channel_unref(mainw->iochan);
      
      if (gerr!=NULL) g_error_free(gerr);
      close(new_stderr);
      unlink(new_stderr_name);
      g_free(new_stderr_name);
      g_free(redir);
    }
  }

  g_free(enc_exec_name);
  g_free(cmd);

  if (not_cancelled) {
    if (mainw->error) {
      mainw->no_switch_dprint=TRUE;
      d_print_failed();
      mainw->no_switch_dprint=FALSE;
      g_free(full_file_name);
      if (!save_all&&!safe_symlinks) {
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
	lives_system ((com=g_strdup_printf("%s close \"%s\"",prefs->backend,cfile->handle)),TRUE);
	g_free(com);
	g_free (cfile);
	cfile=NULL;
	if (mainw->first_free_file==-1||mainw->first_free_file>mainw->current_file)
	  mainw->first_free_file=mainw->current_file;
      }
      else if (!save_all&&safe_symlinks) {
	com=g_strdup_printf("%s clear_symlinks \"%s\"",prefs->backend_sync,cfile->handle);
	lives_system (com,TRUE);
	g_free (com);
      }

      switch_to_file(mainw->current_file,current_file);
      do_blocking_error_dialog(mesg);
      g_free (mesg);

      if (mainw->iochan!=NULL) {
	mainw->iochan=NULL;
	g_object_unref(mainw->optextview);
      }

      if (mainw->subt_save_file!=NULL) g_free(mainw->subt_save_file);
      mainw->subt_save_file=NULL;
      sensitize();
      return;
    }
    g_free (mesg);


    if (g_file_test ((tmp=g_filename_from_utf8(full_file_name,-1,NULL,NULL,NULL)), G_FILE_TEST_EXISTS)) {
      g_free(tmp);
      stat((tmp=g_filename_from_utf8(full_file_name,-1,NULL,NULL,NULL)),&filestat);
      if (filestat.st_size>0) output_exists=TRUE;
    }
    if (!output_exists||file_mtime==filestat.st_mtime) {
      g_free(tmp);

      mainw->no_switch_dprint=TRUE;
      d_print_failed();
      mainw->no_switch_dprint=FALSE;
      g_free(full_file_name);
      if (!save_all&&!safe_symlinks) {
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
	lives_system ((com=g_strdup_printf("%s close \"%s\"",prefs->backend,cfile->handle)),TRUE);
	g_free(com);
	g_free (cfile);
	cfile=NULL;
	if (mainw->first_free_file==-1||mainw->first_free_file>mainw->current_file) 
	  mainw->first_free_file=mainw->current_file;
      }
      else if (!save_all&&safe_symlinks) {
	com=g_strdup_printf("%s clear_symlinks \"%s\"",prefs->backend_sync,cfile->handle);
	lives_system (com,TRUE);
	g_free (com);
      }

      switch_to_file(mainw->current_file,current_file);
      do_blocking_error_dialog(_ ("\n\nEncoder error - output file was not created !\n"));

      if (mainw->iochan!=NULL) {
	mainw->iochan=NULL;
	g_object_unref(mainw->optextview);
      }

      if (mainw->subt_save_file!=NULL) g_free(mainw->subt_save_file);
      mainw->subt_save_file=NULL;
      sensitize();

      if (mainw->error) d_print_failed();

      return;
    }
    g_free(tmp);

    if (save_all) {

      if (prefs->enc_letterbox) {
	// replace letterboxed frames with maxspect frames
	int iwidth=sfile->ohsize;
	int iheight=sfile->ovsize;
	boolean bad_header=FALSE;

	com=g_strdup_printf("%s mv_mgk \"%s\" %d %d \"%s\" 1",prefs->backend,sfile->handle,1,sfile->frames,
			    get_image_ext_for_type(sfile->img_type));

	unlink(sfile->info_file);
	lives_system(com,FALSE);

	do_progress_dialog(TRUE,FALSE,_ ("Clearing letterbox"));

	if (mainw->error) {
	  //	  cfile->may_be_damaged=TRUE;
	  d_print_failed();
	  return;
	}

	calc_maxspect(sfile->hsize,sfile->vsize,&iwidth,&iheight);

	sfile->hsize=iwidth;
	sfile->vsize=iheight;

	save_clip_value(clip,CLIP_DETAILS_WIDTH,&sfile->hsize);
	if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
	save_clip_value(clip,CLIP_DETAILS_HEIGHT,&sfile->vsize);
	if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
	if (bad_header) do_header_write_error(mainw->current_file);

      }



      g_snprintf(sfile->save_file_name,PATH_MAX,"%s",full_file_name);
      sfile->changed=FALSE;

      // save was successful
      sfile->f_size=sget_file_size (full_file_name);

      if (sfile->is_untitled) {
	sfile->is_untitled=FALSE;
      }
      if (!sfile->was_renamed) {
	set_menu_text(sfile->menuentry,full_file_name,FALSE);
	g_snprintf(sfile->name,256,"%s",full_file_name);
      }
      set_main_title(cfile->name,0);
      add_to_recent (full_file_name,0.,0,NULL);
    }
    else {
      if (!safe_symlinks) {
#ifdef IS_MINGW
	// kill any active processes: for other OSes the backend does this
	// get pid from backend
	FILE *rfile;
	ssize_t rlen;
	char val[16];
	int pid;
	com=g_strdup_printf("%s get_pid_for_handle \"%s\"",prefs->backend_sync,nfile->handle);
	rfile=popen(com,"r");
	rlen=fread(val,1,16,rfile);
	pclose(rfile);
	memset(val+rlen,0,1);
	pid=atoi(val);
	
	lives_win32_kill_subprocesses(pid,TRUE);
#endif
	lives_system ((com=g_strdup_printf("%s close \"%s\"",prefs->backend,nfile->handle)),TRUE);
	g_free(com);
	g_free (nfile);
	mainw->files[new_file]=NULL;
	if (mainw->first_free_file==-1||mainw->first_free_file>mainw->current_file) 
	  mainw->first_free_file=new_file;
      }
      else {
	com=g_strdup_printf("%s clear_symlinks \"%s\"",prefs->backend_sync,cfile->handle);
	lives_system (com,TRUE);
	g_free (com);
      }
    }
  }

  switch_to_file(mainw->current_file,current_file);

  if (mainw->iochan!=NULL) {
    int logfd;

    // save the logfile in tempdir

#ifndef IS_MINGW
    gchar *logfile=g_strdup_printf("%s/encoder_log_%d_%d.txt",prefs->tmpdir,lives_getuid(),lives_getgid());
    if ((logfd=creat(logfile,S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH))!=-1) {
#else
    gchar *logfile=g_strdup_printf("%s\\encoder_log_%d_%d.txt",prefs->tmpdir,lives_getuid(),lives_getgid());
    if ((logfd=creat(logfile,S_IRUSR|S_IWUSR))!=-1) {
#endif
      gchar *btext=text_view_get_text(mainw->optextview);
      lives_write(logfd,btext,strlen(btext),TRUE);  // not really important if it fails
      g_free(btext);
      close (logfd);
    }
    g_free(logfile);
    g_object_unref(mainw->optextview);
    mainw->iochan=NULL;
  }

  if (not_cancelled) {
    gchar *fsize_ds;
    mainw->no_switch_dprint=TRUE;
    d_print_done();

    // get size of file and show it

    fsize=sget_file_size(full_file_name);
    fsize_ds=lives_format_storage_space_string(fsize);
    mesg=g_strdup_printf(_("File size was %s\n"),fsize_ds);
    g_free(fsize_ds);
    d_print(mesg);
    g_free(mesg);

    if (mainw->subt_save_file!=NULL) {
      save_subs_to_file(sfile,mainw->subt_save_file);
      g_free(mainw->subt_save_file);
      mainw->subt_save_file=NULL;
    }

    mainw->no_switch_dprint=FALSE;
#ifdef ENABLE_OSC
    lives_osc_notify(LIVES_OSC_NOTIFY_SUCCESS,
		     (mesg=g_strdup_printf("encode %d \"%s\"",clip,
					   (tmp=g_filename_from_utf8(full_file_name,-1,NULL,NULL,NULL)))));
    g_free(tmp);
    g_free(mesg);
#endif

  }
  g_free(full_file_name);


}



void play_file (void) {
  // play the current clip from 'mainw->play_start' to 'mainw->play_end'
  GClosure *freeze_closure;

  short audio_player=prefs->audio_player;

  weed_plant_t *pb_start_event=NULL;

#ifdef GDK_WINDOWING_X11
  uint64_t awinid=-1;
#endif

  gchar *com;
  gchar *com2=g_strdup (" ");
  gchar *com3=g_strdup (" ");
  gchar *stopcom=NULL;
  gchar *msg;
  gchar *stfile;
  gchar *xtrabit,*title;
#ifdef GDK_WINDOWING_X11
  gchar *tmp;
#endif

  boolean mute;

#ifdef RT_AUDIO
  boolean exact_preview=FALSE;
#endif
  boolean has_audio_buffers=FALSE;

  int arate;

  int asigned=!(cfile->signed_endian&AFORM_UNSIGNED);
  int aendian=!(cfile->signed_endian&AFORM_BIG_ENDIAN);

  int current_file=mainw->current_file;
  int audio_end=0;

  int loop=0;


  if (audio_player!=AUD_PLAYER_JACK&&audio_player!=AUD_PLAYER_PULSE) mainw->aud_file_to_kill=mainw->current_file;
  else mainw->aud_file_to_kill=-1;


#ifdef ENABLE_JACK
  if (!mainw->preview&&!mainw->foreign) jack_pb_start();
#endif

  if (mainw->multitrack==NULL) mainw->must_resize=FALSE;
  mainw->ext_playback=FALSE;
  mainw->deltaticks=0;

  mainw->rec_aclip=-1;

  if (mainw->pre_src_file==-2) mainw->pre_src_file=mainw->current_file;
  mainw->pre_src_audio_file=mainw->current_file;

  // enable the freeze button
  lives_accel_group_connect (GTK_ACCEL_GROUP (mainw->accel_group), LIVES_KEY_BackSpace, (GdkModifierType)LIVES_CONTROL_MASK, 
			   (GtkAccelFlags)0, (freeze_closure=g_cclosure_new (G_CALLBACK (freeze_callback),NULL,NULL)));

  if (mainw->multitrack!=NULL) {
    mainw->event_list=mainw->multitrack->event_list;
    pb_start_event=mainw->multitrack->pb_start_event;
#ifdef RT_AUDIO
    exact_preview=mainw->multitrack->exact_preview;
#endif
  }

  if (mainw->record) {
    if (mainw->preview) {
      mainw->record=FALSE;
      d_print (_ ("recording aborted by preview.\n"));
    }
    else if (mainw->current_file==0) {
      mainw->record=FALSE;
      d_print (_ ("recording aborted by clipboard playback.\n"));
    }
    else {
      d_print(_ ("Recording performance..."));
      mainw->clip_switched=FALSE;
      // TODO
      if (mainw->current_file>0&&(cfile->undo_action==UNDO_RESAMPLE||cfile->undo_action==UNDO_RENDER)) {
	lives_widget_set_sensitive (mainw->undo,FALSE);
	lives_widget_set_sensitive (mainw->redo,FALSE);
	cfile->undoable=cfile->redoable=FALSE;
      }
    }
  }
  // set performance at right place
  else if (mainw->event_list!=NULL) cfile->next_event=get_first_event(mainw->event_list);

  // note, here our start is in frames, in save_file it is in seconds !
  // TODO - check if we can change it to seconds here too

  mainw->audio_start=mainw->audio_end=0;

  if (mainw->event_list!=NULL) {
    // play performance data
    if (event_list_get_end_secs (mainw->event_list)>cfile->frames/cfile->fps&&!mainw->playing_sel) {
      mainw->audio_end=(event_list_get_end_secs (mainw->event_list)*cfile->fps+1.)*cfile->arate/cfile->arps;
    }
  }
  
  if (mainw->audio_end==0) {
    mainw->audio_start=(calc_time_from_frame(mainw->current_file,mainw->play_start)*cfile->fps+1.)*cfile->arate/cfile->arps;
    mainw->audio_end=(calc_time_from_frame(mainw->current_file,mainw->play_end)*cfile->fps+1.)*cfile->arate/cfile->arps;
    if (!mainw->playing_sel) {
      mainw->audio_end=0;
    }
  }


  if (!cfile->opening_audio&&!mainw->loop) {
    // if we are opening audio or looping we just play to the end of audio,
    // otherwise...
    audio_end=mainw->audio_end;
  }




  if (mainw->multitrack==NULL) {
    if (!mainw->preview) {
      gtk_frame_set_label(GTK_FRAME(mainw->playframe),_ ("Play"));
    }
    else {
      gtk_frame_set_label(GTK_FRAME(mainw->playframe),_ ("Preview"));
    }
    
    if (palette->style&STYLE_1) {
      lives_widget_set_fg_color(gtk_frame_get_label_widget(GTK_FRAME(mainw->playframe)), 
			   LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
    }
    
    // blank the background if asked to
    if ((mainw->faded||(mainw->fs&&!mainw->sep_win))&&(cfile->frames>0||mainw->foreign)) {
      fade_background();
    }

    if ((!mainw->sep_win||(!mainw->faded&&(prefs->sepwin_type!=1)))&&(cfile->frames>0||mainw->foreign)) {
      // show the frame in the main window
      lives_widget_show(mainw->playframe);
      lives_widget_context_update();
    }
  }

  if (mainw->multitrack==NULL) {
    // plug the plug into the playframe socket if we need to
    add_to_playframe();
  }
  
  arate=cfile->arate;

  mute=mainw->mute;

  if (audio_player!=AUD_PLAYER_JACK&&audio_player!=AUD_PLAYER_PULSE) {
    if (cfile->achans==0||mainw->is_rendering) mainw->mute=TRUE;
    if (mainw->mute&&!cfile->opening_only_audio) arate=arate?-arate:-1;
  }

  cfile->frameno=mainw->play_start;
  cfile->pb_fps=cfile->fps;
  if (mainw->reverse_pb) {
    cfile->pb_fps=-cfile->pb_fps;
    cfile->frameno=mainw->play_end;
  }
  mainw->reverse_pb=FALSE;

  cfile->play_paused=FALSE;
  mainw->period=U_SEC/cfile->pb_fps;

  if (audio_player==AUD_PLAYER_JACK) audio_cache_init();

  if (mainw->blend_file!=-1&&mainw->files[mainw->blend_file]==NULL) mainw->blend_file=-1;

  if (mainw->num_tr_applied>0&&!mainw->preview&&mainw->blend_file>-1) {
    // reset frame counter for blend_file
    mainw->files[mainw->blend_file]->frameno=1;
    mainw->files[mainw->blend_file]->aseek_pos=0;
  }

  lives_widget_set_sensitive(mainw->m_stopbutton,TRUE);
  mainw->playing_file=mainw->current_file;

  if (!mainw->preview||!cfile->opening) {
    enable_record();
    desensitize();
  }

  if (mainw->record) {
    if (mainw->event_list!=NULL) event_list_free (mainw->event_list);
    mainw->event_list=add_filter_init_events(NULL,0);
  }

  if (mainw->double_size&&mainw->multitrack==NULL) {
    lives_widget_hide(mainw->scrolledwindow);
  }

  lives_widget_set_sensitive (mainw->stop, TRUE);

  if (mainw->multitrack==NULL) lives_widget_set_sensitive (mainw->m_playbutton, FALSE);
  else if (!cfile->opening) {
    if (!mainw->is_processing) mt_swap_play_pause(mainw->multitrack,TRUE);
    else {
      lives_widget_set_sensitive(mainw->multitrack->playall,FALSE);
      lives_widget_set_sensitive(mainw->m_playbutton,FALSE);
    }
  }

  lives_widget_set_sensitive (mainw->m_playselbutton, FALSE);
  lives_widget_set_sensitive (mainw->m_rewindbutton, FALSE);
  lives_widget_set_sensitive (mainw->m_mutebutton, (audio_player==AUD_PLAYER_JACK||audio_player==AUD_PLAYER_PULSE||
						  mainw->multitrack!=NULL));

  lives_widget_set_sensitive (mainw->m_loopbutton, (!cfile->achans||mainw->mute||mainw->multitrack!=NULL||
						  mainw->loop_cont||audio_player==AUD_PLAYER_JACK||
						  audio_player==AUD_PLAYER_PULSE)&&mainw->current_file>0);
  lives_widget_set_sensitive (mainw->loop_continue, (!cfile->achans||mainw->mute||mainw->loop_cont||
						   audio_player==AUD_PLAYER_JACK||audio_player==AUD_PLAYER_PULSE)
			    &&mainw->current_file>0);

  if (cfile->frames==0&&mainw->multitrack==NULL) {
    if (mainw->preview_box!=NULL&&lives_widget_get_parent(mainw->preview_box)!=NULL) {
      lives_container_remove (LIVES_CONTAINER (mainw->play_window), mainw->preview_box);

      mainw->pw_scroll_func=g_signal_connect (GTK_OBJECT (mainw->play_window), "scroll_event",
					      G_CALLBACK (on_mouse_scroll),
					      NULL);

    }
  }
  else {
    if (mainw->sep_win) {
      // create a separate window for the internal player if requested
      if (prefs->sepwin_type==0) {
	// needed
	block_expose();
	make_play_window();
	unblock_expose();
      }
      else {
	if (mainw->multitrack==NULL) {
	  if (mainw->preview_box!=NULL&&lives_widget_get_parent(mainw->preview_box)!=NULL) {
	    lives_container_remove (LIVES_CONTAINER (mainw->play_window), mainw->preview_box);

	    mainw->pw_scroll_func=g_signal_connect (GTK_OBJECT (mainw->play_window), "scroll_event",
						    G_CALLBACK (on_mouse_scroll),
						    NULL);
	  }
	}

	if (mainw->multitrack==NULL||mainw->fs) {
	  gchar *xtrabit,*title;
	  resize_play_window();
	  if (mainw->sepwin_scale!=100.) xtrabit=g_strdup_printf(_(" (%d %% scale)"),(int)mainw->sepwin_scale);
	  else xtrabit=g_strdup("");
	  title=g_strdup_printf(_("LiVES: - Play Window%s"),xtrabit);
	  if (mainw->play_window!=NULL)
	    lives_window_set_title (LIVES_WINDOW (mainw->play_window), title);
	  g_free(title);
	  g_free(xtrabit);
	}

	// needed
	if (mainw->multitrack==NULL) {
	  block_expose();
	  mainw->noswitch=TRUE;
	  lives_widget_context_update();
	  mainw->noswitch=FALSE;
	  unblock_expose();
	}
	else {
	  // this doesn't get called if we don't call resize_play_window()
	  if (mainw->play_window!=NULL) {
	    lives_window_present (LIVES_WINDOW (mainw->play_window));
	    gdk_window_raise(lives_widget_get_xwindow(mainw->play_window));
	  }
	}
      }
    }

    if (mainw->play_window!=NULL) {
      hide_cursor (lives_widget_get_xwindow(mainw->play_window));
      lives_widget_set_app_paintable(mainw->play_window,TRUE);
      if (mainw->vpp!=NULL&&!(mainw->vpp->capabilities&VPP_LOCAL_DISPLAY)&&mainw->fs) 
	lives_window_set_title (LIVES_WINDOW (mainw->play_window),_("LiVES: - Streaming"));
      else {
	gchar *title,*xtrabit;
	if (mainw->sepwin_scale!=100.) xtrabit=g_strdup_printf(_(" (%d %% scale)"),(int)mainw->sepwin_scale);
	else xtrabit=g_strdup("");
	title=g_strdup_printf(_("LiVES: - Play Window%s"),xtrabit);
	lives_window_set_title (LIVES_WINDOW (mainw->play_window), title);
	g_free(title);
	g_free(xtrabit);
      }
    }
  
    if (!mainw->foreign&&!mainw->sep_win) {
      hide_cursor(lives_widget_get_xwindow(mainw->playarea));
    }
    
    // pwidth and pheight are playback width and height
    if (!mainw->sep_win&&!mainw->foreign) {
      do {
	mainw->pwidth=lives_widget_get_allocation_width(mainw->playframe)-H_RESIZE_ADJUST;
	mainw->pheight=lives_widget_get_allocation_height(mainw->playframe)-V_RESIZE_ADJUST;
	if (mainw->pwidth*mainw->pheight==0) {
	  lives_widget_queue_draw (mainw->playframe);
	  mainw->noswitch=TRUE;
	  lives_widget_context_update();
	  mainw->noswitch=FALSE;
	}
      } while (mainw->pwidth*mainw->pheight==0);
      // double size
      if (mainw->double_size) {
	frame_size_update();
      }
    }

    if (mainw->vpp!=NULL&&mainw->vpp->fheight>-1&&mainw->vpp->fwidth>-1) {
      // fixed o/p size for stream
      if (!(mainw->vpp->fwidth*mainw->vpp->fheight)) {
	mainw->vpp->fwidth=cfile->hsize;
	mainw->vpp->fheight=cfile->vsize;
      }
      mainw->pwidth=mainw->vpp->fwidth;
      mainw->pheight=mainw->vpp->fheight;
    }
    
    if (mainw->fs&&!mainw->sep_win&&cfile->frames>0) {
      fullscreen_internal();
    }

  }

  // moved down because xdg-screensaver requires a mapped windowID
  if (prefs->stop_screensaver) {
    g_free (com2);
    com2=NULL;
#ifdef GDK_WINDOWING_X11
    if (!prefs->show_gui&&prefs->show_playwin&&mainw->play_window!=NULL) {
      awinid=lives_widget_get_xwinid(mainw->play_window,NULL);
    }
    else if (prefs->show_gui) {
      if (mainw->multitrack!=NULL) {
	awinid=lives_widget_get_xwinid(mainw->multitrack->window,NULL);
      }
      else if (mainw->LiVES!=NULL) {
	awinid=lives_widget_get_xwinid(mainw->LiVES,NULL);
      }
    }

    com2=g_strdup("xset s off 2>/dev/null; xset -dpms 2>/dev/null ;");
    
    if (capable->has_gconftool_2) {
      gchar *xnew=g_strdup(" gconftool-2 --set --type bool /apps/gnome-screensaver/idle_activation_enabled false 2>/dev/null ;");
      tmp=g_strconcat (com2,xnew,NULL);
      g_free(com2);
      g_free(xnew);
      com2=tmp;
    }
    if (capable->has_xdg_screensaver&&awinid!=-1) {
      gchar *xnew=g_strdup_printf(" xdg-screensaver suspend %"PRIu64" 2>/dev/null ;",awinid);
      tmp=g_strconcat (com2,xnew,NULL);
      g_free(com2);
      g_free(xnew);
      com2=tmp;
    }
#else
    if (capable->has_gconftool_2) {
      com2=g_strdup("gconftool-2 --set --type bool /apps/gnome-screensaver/idle_activation_enabled false 2>/dev/null ;");
    }
    else com2=g_strdup("");
#endif
    if (com2==NULL) com2=g_strdup("");
  }


  if (!mainw->foreign&&prefs->midisynch&&!mainw->preview) {
    g_free (com3);
    com3=g_strdup  ("midistart");
  }
  com=g_strconcat (com2,com3,NULL);
  if (strlen (com)) {
    // allow this to fail - not all sub-commands may be present
    lives_system (com,TRUE);
  }
  g_free (com); g_free (com2); g_free (com3);
  com3=g_strdup (" ");
  com2=NULL;
  com=NULL;



  lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_pb_fps),cfile->pb_fps);
    
  mainw->last_blend_file=-1;

  // show the framebar
  if (mainw->multitrack==NULL&&(prefs->show_framecount&&
				(!mainw->fs||(prefs->gui_monitor!=prefs->play_monitor&&mainw->sep_win!=0)||
				 (mainw->vpp!=NULL&&mainw->sep_win&&!(mainw->vpp->capabilities&VPP_LOCAL_DISPLAY)))&&
				((!mainw->preview&&(cfile->frames>0||mainw->foreign))||cfile->opening))) {
    lives_widget_show(mainw->framebar);
  }

  cfile->play_paused=FALSE;
  mainw->actual_frame=0;

  mainw->currticks=0;

  // reinit all active effects
  if (!mainw->preview&&!mainw->is_rendering&&!mainw->foreign) weed_reinit_all();

  if (!mainw->foreign&&(!(prefs->audio_src==AUDIO_SRC_EXT&&
			((audio_player==AUD_PLAYER_JACK) ||
			 (audio_player==AUD_PLAYER_PULSE))))) {
    cfile->aseek_pos=(long)((double)(mainw->play_start-1.)/cfile->fps*cfile->arate)*cfile->achans*(cfile->asampsize/8);

    // start up our audio player (jack or pulse)
    if (audio_player==AUD_PLAYER_JACK) {
#ifdef ENABLE_JACK
      if (mainw->jackd!=NULL) jack_aud_pb_ready(mainw->current_file);
#endif
    }
    else if (audio_player==AUD_PLAYER_PULSE) {
#ifdef HAVE_PULSE_AUDIO
      if (mainw->pulsed!=NULL) pulse_aud_pb_ready(mainw->current_file);
#endif
    }
    else if (cfile->achans>0) {
      // sox or mplayer audio - run as background process

      if (mainw->loop_cont) {
	// tell audio to loop forever
	loop=-1;
      }

      stfile=g_build_filename(prefs->tmpdir,cfile->handle,".stoploop",NULL);
      unlink (stfile);
    
      if (cfile->achans>0||(!cfile->is_loaded&&!mainw->is_generating)) {
	if (loop) {
	  g_free (com3);
#ifndef IS_MINGW
	  com3=g_strdup_printf ("/bin/touch \"%s\" 2>/dev/null;",stfile);
#else
	  com3=g_strdup_printf ("touch.exe \"%s\" 2>NUL;",stfile);
#endif
	}
	
	if (cfile->achans>0) {
	  com2=g_strdup_printf("%s stop_audio %s",prefs->backend_sync,cfile->handle);
	}
    
	stopcom=g_strconcat (com3,com2,NULL);
      }

      g_free(stfile);

#ifndef IS_MINGW
      stfile=g_build_filename(prefs->tmpdir,cfile->handle,".status.play",NULL);
#else
      stfile=g_build_filename(prefs->tmpdir,cfile->handle,"status.play",NULL);
#endif

      g_snprintf(cfile->info_file,PATH_MAX,"%s",stfile);
      g_free(stfile);
      if (cfile->clip_type==CLIP_TYPE_DISK) unlink(cfile->info_file);

      // PLAY

      if (cfile->clip_type==CLIP_TYPE_DISK&&cfile->opening) {
	  com=g_strdup_printf("%s play_opening_preview \"%s\" %.3f %d %d %d %d %d %d %d %d",prefs->backend,
			      cfile->handle,cfile->fps,mainw->audio_start,audio_end,0,
			      arate,cfile->achans,cfile->asampsize,asigned,aendian);
      }
      else {
	// this is only used now for sox or mplayer audio player
	com=g_strdup_printf("%s play %s %.3f %d %d %d %d %d %d %d %d",prefs->backend,cfile->handle,
			    cfile->fps,mainw->audio_start,audio_end,loop,
			    arate,cfile->achans,cfile->asampsize,asigned,aendian);
      }
      if (mainw->multitrack==NULL&&com!=NULL) lives_system(com,FALSE);
    }
  }

  g_free (com3);

  // if recording, set up recorder (jack or pulse)
  if (!mainw->foreign&&!mainw->preview&&(prefs->audio_src==AUDIO_SRC_EXT||(mainw->record&&mainw->agen_key!=0))&&
      ((audio_player==AUD_PLAYER_JACK) ||
       (audio_player==AUD_PLAYER_PULSE))) {
    mainw->rec_samples=-1; // record unlimited
    if (mainw->record) {
      // creat temp clip
      open_ascrap_file();
      if (mainw->ascrap_file!=-1) {
	
	mainw->rec_aclip=mainw->ascrap_file;
	mainw->rec_avel=1.;
	mainw->rec_aseek=0;
      }
    }
    if (audio_player==AUD_PLAYER_JACK) {
#ifdef ENABLE_JACK
      if (prefs->audio_src==AUDIO_SRC_EXT&&mainw->jackd!=NULL) {
	if (mainw->agen_key!=0) {
	  mainw->jackd->playing_file=mainw->current_file;
	  mainw->jackd->frames_written=0;
	  mainw->jackd->in_use=TRUE;
	  jack_rec_audio_to_clip(mainw->ascrap_file, -1, RECA_GENERATED);
	}
	else {
	  jack_rec_audio_to_clip(mainw->ascrap_file, -1, RECA_EXTERNAL);
	}
      }
      mainw->jackd->frames_written=0;
#endif
    }
    if (audio_player==AUD_PLAYER_PULSE) {
#ifdef HAVE_PULSE_AUDIO
      if (prefs->audio_src==AUDIO_SRC_EXT&&mainw->pulsed!=NULL) {
	if (mainw->agen_key!=0) {
	  mainw->pulsed->playing_file=mainw->current_file;
	  mainw->pulsed->frames_written=0;
	  mainw->pulsed->in_use=TRUE;
	  pulse_rec_audio_to_clip(mainw->ascrap_file, -1, RECA_GENERATED);
	}
	else {
	  pulse_rec_audio_to_clip(mainw->ascrap_file, -1, RECA_EXTERNAL);
	}
      }
      mainw->pulsed->frames_written=0;
#endif
    }
  }




  find_when_to_stop();

  if (mainw->foreign||weed_playback_gen_start()) {

#ifdef ENABLE_OSC
    lives_osc_notify(LIVES_OSC_NOTIFY_PLAYBACK_STARTED,"");
    if (mainw->osc_auto) lives_osc_notify_success(NULL);
#endif
    
#ifdef ENABLE_JACK
    if (mainw->event_list!=NULL&&!mainw->record&&audio_player==AUD_PLAYER_JACK&&mainw->jackd!=NULL&&
	!(mainw->preview&&mainw->is_processing&&
	  !(mainw->multitrack!=NULL&&mainw->preview&&mainw->multitrack->is_rendering))) {
      // if playing an event list, we switch to audio memory buffer mode
      if (mainw->multitrack!=NULL) init_jack_audio_buffers(cfile->achans,cfile->arate,exact_preview);
      else init_jack_audio_buffers(DEFAULT_AUDIO_CHANS,DEFAULT_AUDIO_RATE,FALSE);
      has_audio_buffers=TRUE;
    }
#endif    
#ifdef HAVE_PULSE_AUDIO
    if (mainw->event_list!=NULL&&!mainw->record&&audio_player==AUD_PLAYER_PULSE&&mainw->pulsed!=NULL&&
	!(mainw->preview&&mainw->is_processing&&
	  !(mainw->multitrack!=NULL&&mainw->preview&&mainw->multitrack->is_rendering))) {
      // if playing an event list, we switch to audio memory buffer mode
      if (mainw->multitrack!=NULL) init_pulse_audio_buffers(cfile->achans,cfile->arate,exact_preview);
      else init_pulse_audio_buffers(DEFAULT_AUDIO_CHANS,DEFAULT_AUDIO_RATE,FALSE);
      has_audio_buffers=TRUE;
    }
#endif    

    mainw->abufs_to_fill=0;

    //play until stopped or a stream finishes
    do {
      mainw->cancelled=CANCEL_NONE;

      if (mainw->event_list!=NULL&&!mainw->record) {
	if (pb_start_event==NULL) pb_start_event=get_first_event(mainw->event_list);

	if (has_audio_buffers) {

#ifdef ENABLE_JACK
	  if (audio_player==AUD_PLAYER_JACK) {
	    int i;
	    mainw->write_abuf=0;
	    
	    // fill our audio buffers now
	    // this will also get our effects state

	    // reset because audio sync may have set it
	    if (mainw->multitrack!=NULL) mainw->jackd->abufs[0]->arate=cfile->arate;
	    fill_abuffer_from(mainw->jackd->abufs[0],mainw->event_list,pb_start_event,exact_preview);
	    for (i=1;i<prefs->num_rtaudiobufs;i++) {
	      // reset because audio sync may have set it
	      if (mainw->multitrack!=NULL) mainw->jackd->abufs[i]->arate=cfile->arate;
	      fill_abuffer_from(mainw->jackd->abufs[i],mainw->event_list,NULL,FALSE);
	    }
	    
	    pthread_mutex_lock(&mainw->abuf_mutex);
	    mainw->jackd->read_abuf=0;
	    mainw->abufs_to_fill=0;
	    pthread_mutex_unlock(&mainw->abuf_mutex);
	    mainw->jackd->in_use=TRUE;
	  }
#endif
#ifdef HAVE_PULSE_AUDIO
	  if (audio_player==AUD_PLAYER_PULSE) {
	    int i;
	    mainw->write_abuf=0;
	    
	    // fill our audio buffers now
	    // this will also get our effects state
	    fill_abuffer_from(mainw->pulsed->abufs[0],mainw->event_list,pb_start_event,exact_preview);
	    for (i=1;i<prefs->num_rtaudiobufs;i++) {
	      fill_abuffer_from(mainw->pulsed->abufs[i],mainw->event_list,NULL,FALSE);
	    }
	    
	    pthread_mutex_lock(&mainw->abuf_mutex);
	    mainw->pulsed->read_abuf=0;
	    mainw->abufs_to_fill=0;
	    pthread_mutex_unlock(&mainw->abuf_mutex);
	    mainw->pulsed->in_use=TRUE;
	    
	  }
#endif
	  // let transport roll
	  mainw->video_seek_ready=TRUE;
	}

      }

      if (mainw->multitrack==NULL||mainw->multitrack->pb_start_event==NULL) {
	do_progress_dialog(FALSE,FALSE,NULL);

	// reset audio buffers
#ifdef ENABLE_JACK
	if (audio_player==AUD_PLAYER_JACK&&mainw->jackd!=NULL) {
	  // must do this before deinit fx
	  pthread_mutex_lock(&mainw->abuf_mutex);
	  mainw->jackd->read_abuf=-1;
	  pthread_mutex_unlock(&mainw->abuf_mutex);
	}
#endif
#ifdef HAVE_PULSE_AUDIO
	if (audio_player==AUD_PLAYER_PULSE&&mainw->pulsed!=NULL) {
	  // must do this before deinit fx
	  pthread_mutex_lock(&mainw->abuf_mutex);
	  mainw->pulsed->read_abuf=-1;
	  pthread_mutex_unlock(&mainw->abuf_mutex);
	}
#endif

      }
      else {
	// play from middle of mt timeline
	cfile->next_event=mainw->multitrack->pb_start_event;

	if (!has_audio_buffers) {
	  // no audio buffering
	  // get just effects state
	  get_audio_and_effects_state_at(mainw->multitrack->event_list,mainw->multitrack->pb_start_event,FALSE,
					 mainw->multitrack->exact_preview);
	}

	do_progress_dialog(FALSE,FALSE,NULL);

	// reset audio read buffers
#ifdef ENABLE_JACK
	if (audio_player==AUD_PLAYER_JACK&&mainw->jackd!=NULL) {
	  // must do this before deinit fx
	  pthread_mutex_lock(&mainw->abuf_mutex);
	  mainw->jackd->read_abuf=-1;
	  pthread_mutex_unlock(&mainw->abuf_mutex);
	}
#endif
#ifdef HAVE_PULSE_AUDIO
	if (audio_player==AUD_PLAYER_PULSE&&mainw->pulsed!=NULL) {
	  // must do this before deinit fx
	  pthread_mutex_lock(&mainw->abuf_mutex);
	  mainw->pulsed->read_abuf=-1;
	  pthread_mutex_unlock(&mainw->abuf_mutex);
	}
#endif

	// realtime effects off
	deinit_render_effects();

	cfile->next_event=NULL;

	// multitrack loop - go back to loop start position unless external transport moved us
	if (mainw->scratch==SCRATCH_NONE) {
	  mainw->multitrack->pb_start_event=mainw->multitrack->pb_loop_event;
	}
      }
      if (mainw->multitrack!=NULL) pb_start_event=mainw->multitrack->pb_start_event;
    } while (mainw->multitrack!=NULL&&(mainw->loop_cont||mainw->scratch!=SCRATCH_NONE)&&
	     (mainw->cancelled==CANCEL_NONE||mainw->cancelled==CANCEL_EVENT_LIST_END));
    mainw->osc_block=TRUE;
    mainw->rte_textparm=NULL;

    mainw->playing_file=-1;
  }
    
  // play completed

  mainw->video_seek_ready=FALSE;

#ifdef ENABLE_JACK
  if (audio_player==AUD_PLAYER_JACK&&(mainw->jackd!=NULL||mainw->jackd_read!=NULL)) {

    if (mainw->jackd_read!=NULL||mainw->aud_rec_fd!=-1) jack_rec_audio_end(TRUE);

    // send jack transport stop
    if (!mainw->preview&&!mainw->foreign) jack_pb_stop();

    // tell jack client to close audio file
    if (mainw->jackd!=NULL&&mainw->jackd->playing_file>0) {
      boolean timeout;
      int alarm_handle=lives_alarm_set(LIVES_ACONNECT_TIMEOUT);
      while (!(timeout=lives_alarm_get(alarm_handle))&&jack_get_msgq(mainw->jackd)!=NULL) {
	sched_yield(); // wait for seek
      }
      if (timeout) jack_try_reconnect();
      lives_alarm_clear(alarm_handle);

      jack_message.command=ASERVER_CMD_FILE_CLOSE;
      jack_message.data=NULL;
      jack_message.next=NULL;
      mainw->jackd->msgq=&jack_message;

    }
    if (mainw->record&&(prefs->rec_opts&REC_AUDIO)) {
      weed_plant_t *event=get_last_frame_event(mainw->event_list);
      insert_audio_event_at(mainw->event_list,event,-1,1,0.,0.); // audio switch off
    }

  }
  else {
#endif
#ifdef HAVE_PULSE_AUDIO
    if (audio_player==AUD_PLAYER_PULSE&&(mainw->pulsed!=NULL||mainw->pulsed_read!=NULL)) {

    if (mainw->pulsed_read!=NULL||mainw->aud_rec_fd!=-1) pulse_rec_audio_end(TRUE);

    // tell pulse client to close audio file
    if (mainw->pulsed!=NULL&&(mainw->pulsed->playing_file>0||mainw->pulsed->fd>0)) {
      boolean timeout;
      int alarm_handle=lives_alarm_set(LIVES_ACONNECT_TIMEOUT);
      while (!(timeout=lives_alarm_get(alarm_handle))&&pulse_get_msgq(mainw->pulsed)!=NULL) {
	sched_yield(); // wait for seek
      }
      if (timeout) pulse_try_reconnect();
      lives_alarm_clear(alarm_handle);

      pulse_message.command=ASERVER_CMD_FILE_CLOSE;
      pulse_message.data=NULL;
      pulse_message.next=NULL;
      mainw->pulsed->msgq=&pulse_message;
    }
    if (mainw->record&&(prefs->rec_opts&REC_AUDIO)) {
      weed_plant_t *event=get_last_frame_event(mainw->event_list);
      insert_audio_event_at(mainw->event_list,event,-1,1,0.,0.); // audio switch off
    }

  }
  else {
#endif
    if (audio_player!=AUD_PLAYER_JACK&&audio_player!=AUD_PLAYER_PULSE&&stopcom!=NULL) {
      // kill sound(if still playing)
      lives_system(stopcom,TRUE);
      mainw->aud_file_to_kill=-1;
      g_free (stopcom);
    }
#ifdef ENABLE_JACK
  }
#endif
#ifdef HAVE_PULSE_AUDIO
  }
#endif

  if (com!=NULL) g_free(com);
  mainw->actual_frame=0;

  if (audio_player==AUD_PLAYER_JACK) audio_cache_end();

#ifdef ENABLE_OSC
  lives_osc_notify(LIVES_OSC_NOTIFY_PLAYBACK_STOPPED,"");
#endif

  mainw->video_seek_ready=FALSE;


  // PLAY FINISHED...
  // allow this to fail - not all sub-commands may be present
  if (prefs->stop_screensaver) {

#ifdef GDK_WINDOWING_X11
    com=g_strdup("xset s on 2>/dev/null; xset +dpms 2>/dev/null ;");
    
    if (capable->has_gconftool_2) {
      gchar *xnew=g_strdup(" gconftool-2 --set --type bool /apps/gnome-screensaver/idle_activation_enabled true 2>/dev/null ;");
      tmp=g_strconcat (com,xnew,NULL);
      g_free(com);
      g_free(xnew);
      com=tmp;
    }
    if (capable->has_xdg_screensaver&&awinid!=-1) {
      gchar *xnew=g_strdup_printf(" xdg-screensaver resume %"PRIu64" 2>/dev/null ;",awinid);
      tmp=g_strconcat (com,xnew,NULL);
      g_free(com);
      g_free(xnew);
      com=tmp;
    }
#else
    if (capable->has_gconftool_2) {
      com=g_strdup("gconftool-2 --set --type bool /apps/gnome-screensaver/idle_activation_enabled true 2>/dev/null ;");
    }
    else com=g_strdup("");
#endif
    if (com!=NULL) {
      lives_system(com,TRUE);
      g_free(com);
    }
  }

  if (!mainw->foreign&&prefs->midisynch) lives_system ("midistop",TRUE);

  if (mainw->ext_playback) {
    vid_playback_plugin_exit();
  }
  // we could have started by playing a generator, which could've been closed
  if (mainw->files[current_file]==NULL) current_file=mainw->current_file;

  if (audio_player!=AUD_PLAYER_JACK&&audio_player!=AUD_PLAYER_PULSE) {
    // wait for audio_ended...
    if (cfile->achans>0&&com2!=NULL) {
      wait_for_stop(com2);
      mainw->aud_file_to_kill=-1;
    }
    if (com2!=NULL) g_free(com2);
  }

  if (mainw->current_file>-1) {
#ifndef IS_MINGW
    stfile=g_build_filename(prefs->tmpdir,cfile->handle,".status",NULL);
#else
    stfile=g_build_filename(prefs->tmpdir,cfile->handle,"status",NULL);
#endif
    g_snprintf(cfile->info_file,PATH_MAX,"%s",stfile);
    g_free(stfile);
  }

  if (mainw->foreign) {
    // recording from external window capture

    mainw->pwidth=lives_widget_get_allocation_width(mainw->playframe)-H_RESIZE_ADJUST;
    mainw->pheight=lives_widget_get_allocation_height(mainw->playframe)-V_RESIZE_ADJUST;

    cfile->hsize=mainw->pwidth;
    cfile->vsize=mainw->pheight;

    gdk_window_set_keep_above(mainw->foreign_window,FALSE);

    lives_widget_context_update();

    return;
  }

  if (mainw->toy_type==LIVES_TOY_AUTOLIVES) {
    on_toy_activate(NULL, GINT_TO_POINTER(LIVES_TOY_NONE));
  }

  lives_widget_hide(mainw->playarea);

  // unblank the background
  if ((mainw->faded||mainw->fs)&&mainw->multitrack==NULL) {
    unfade_background();
  }

  // resize out of double size
  if ((mainw->double_size&&!mainw->fs)&&mainw->multitrack==NULL) {
    resize(1);
    if (mainw->play_window!=NULL) {
      resize_play_window();
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
    if (palette->style&STYLE_1) {
      lives_widget_show(mainw->sep_image);
    }
    lives_widget_show(mainw->scrolledwindow);
  }

  // switch out of full screen mode
  if (mainw->fs&&mainw->multitrack==NULL) {
    lives_widget_show(mainw->frame1);
    lives_widget_show(mainw->frame2);
    lives_widget_show(mainw->eventbox3);
    lives_widget_show(mainw->eventbox4);
    lives_widget_show(mainw->sep_image);
    gtk_frame_set_label(GTK_FRAME(mainw->playframe),_ ("Preview"));
    lives_container_set_border_width (LIVES_CONTAINER (mainw->playframe), widget_opts.border_width);
    resize(1);
    lives_widget_show(mainw->t_bckground);
    lives_widget_show(mainw->t_double);
  }

  if (prefs->show_gui&&(lives_widget_get_allocation_height(mainw->eventbox)+lives_widget_get_allocation_height(mainw->menubar)
			>mainw->scr_height-2||lives_widget_get_allocation_width(mainw->LiVES)>mainw->scr_width-2)) {
    int wx,wy;
    // the screen grew too much...remaximise it
    lives_window_unmaximize (LIVES_WINDOW(mainw->LiVES));
    mainw->noswitch=TRUE;
    lives_widget_context_update();
    mainw->noswitch=FALSE;
    lives_window_get_position (LIVES_WINDOW (mainw->LiVES),&wx,&wy);
    if (prefs->gui_monitor==0) lives_window_move(LIVES_WINDOW(mainw->LiVES),0,0);
    lives_window_maximize (LIVES_WINDOW(mainw->LiVES));
  }
  
  if (mainw->multitrack==NULL) {
    lives_widget_hide(mainw->playframe);
    lives_widget_show(mainw->frame1);
    lives_widget_show(mainw->frame2);
    lives_widget_show(mainw->eventbox3);
    lives_widget_show(mainw->eventbox4);
    disable_record();
    
    lives_container_set_border_width (LIVES_CONTAINER (mainw->playframe), widget_opts.border_width);
  }

  if (audio_player!=AUD_PLAYER_JACK&&audio_player!=AUD_PLAYER_PULSE) mainw->mute=mute;

  if (!mainw->preview||!cfile->opening) {
    sensitize();
  }
  if (mainw->current_file>-1&&cfile->opening) {
    lives_widget_set_sensitive (mainw->mute_audio, cfile->achans>0);
    lives_widget_set_sensitive (mainw->loop_continue, TRUE);
    lives_widget_set_sensitive (mainw->loop_video, cfile->achans>0&&cfile->frames>0);
  }

  if (mainw->cancelled!=CANCEL_USER_PAUSED) {
    lives_widget_set_sensitive (mainw->stop, FALSE);
    lives_widget_set_sensitive (mainw->m_stopbutton, FALSE);
  }

  if (mainw->multitrack==NULL) {
    // update screen for internal players
    lives_widget_hide(mainw->framebar);
    lives_entry_set_text(LIVES_ENTRY(mainw->framecounter),"");
    lives_image_set_from_pixbuf(GTK_IMAGE(mainw->play_image),NULL);
  }

  // kill the separate play window
  if (mainw->play_window!=NULL) {
    lives_window_unfullscreen(LIVES_WINDOW(mainw->play_window));
    if (prefs->sepwin_type==0) {
      kill_play_window();
    }
    else {
      // or resize it back to single size
      if (mainw->current_file>-1&&cfile->is_loaded&&cfile->frames>0&&!mainw->is_rendering&&
	  (cfile->clip_type!=CLIP_TYPE_GENERATOR)) {
	if (mainw->preview_box==NULL) {
	  // create the preview in the sepwin
	  g_signal_handler_disconnect (mainw->play_window, mainw->pw_scroll_func);
	  make_preview_box();
	}
	if (mainw->current_file!=current_file) {
	  // now we have to guess how to center the play window
	  mainw->opwx=mainw->opwy=-1;
	  mainw->preview_frame=0;
	}
      }

      if (mainw->play_window!=NULL) {
	if (mainw->multitrack==NULL) {
	  mainw->playing_file=-2;
	  resize_play_window();
	  mainw->playing_file=-1;

	  if (mainw->sepwin_scale!=100.) xtrabit=g_strdup_printf(_(" (%d %% scale)"),(int)mainw->sepwin_scale);
	  else xtrabit=g_strdup("");
	  title=g_strdup_printf("%s%s",lives_window_get_title(LIVES_WINDOW
							    (mainw->multitrack==NULL?mainw->LiVES:
							     mainw->multitrack->window)),xtrabit);
	  lives_window_set_title(LIVES_WINDOW(mainw->play_window),title);
	  g_free(title);
	  g_free(xtrabit);

	  lives_widget_queue_draw (mainw->LiVES);
	  mainw->noswitch=TRUE;

	  lives_widget_context_update();
	  if (mainw->play_window!=NULL) {
	    gchar *title,*xtrabit;
	    
	    //load_preview_image(FALSE);
	    
	    mainw->noswitch=FALSE;
	    if (mainw->sepwin_scale!=100.) xtrabit=g_strdup_printf(_(" (%d %% scale)"),(int)mainw->sepwin_scale);
	    else xtrabit=g_strdup("");
	    title=g_strdup_printf("%s%s",lives_window_get_title(LIVES_WINDOW
							      (mainw->multitrack==NULL?mainw->LiVES:
							       mainw->multitrack->window)),xtrabit);
	    lives_window_set_title(LIVES_WINDOW(mainw->play_window),title);
	    g_free(title);
	    g_free(xtrabit);

	    lives_window_present (LIVES_WINDOW (mainw->play_window));
	    gdk_window_raise(lives_widget_get_xwindow(mainw->play_window));
	    unhide_cursor (lives_widget_get_xwindow(mainw->play_window));
	  }
	}
      }
    }

    // free the last frame image
    if (mainw->frame_layer!=NULL) {
      weed_layer_free(mainw->frame_layer);
      mainw->frame_layer=NULL;
    }
  }
  
  if (!mainw->foreign) {
    unhide_cursor(lives_widget_get_xwindow(mainw->playarea));
  }

  if (mainw->current_file>-1) cfile->play_paused=FALSE;

  if (mainw->blend_file!=-1&&mainw->blend_file!=mainw->current_file&&mainw->files[mainw->blend_file]!=NULL&&mainw->files[mainw->blend_file]->clip_type==CLIP_TYPE_GENERATOR) {
    int xcurrent_file=mainw->current_file;
    weed_bg_generator_end ((weed_plant_t *)mainw->files[mainw->blend_file]->ext_src);
    mainw->current_file=xcurrent_file;
  }

  mainw->filter_map=NULL;
  mainw->afilter_map=NULL;
  mainw->audio_event=NULL;

  // disable the freeze key
  lives_accel_group_disconnect (GTK_ACCEL_GROUP (mainw->accel_group), freeze_closure);
  
  if (mainw->multitrack==NULL) lives_widget_show(mainw->scrolledwindow);

  if (mainw->current_file>-1) {
    if (mainw->toy_type==LIVES_TOY_MAD_FRAMES&&!cfile->opening) {
      load_start_image (cfile->start);
      load_end_image (cfile->end);
    }
  }
  if (prefs->show_player_stats) {
    if (mainw->fps_measure>0.) {
      msg=g_strdup_printf (_ ("Average FPS was %.4f\n"),mainw->fps_measure);
      d_print (msg);
      g_free (msg);
    }
  }
  if (mainw->size_warn) {
    do_error_dialog (_ ("\n\nSome frames in this clip are wrongly sized.\nYou should click on Tools--->Resize All\nand resize all frames to the current size.\n"));
    mainw->size_warn=FALSE;
  }
  mainw->is_processing=mainw->preview;

  // TODO - ????
  if (mainw->current_file>-1&&cfile->clip_type==CLIP_TYPE_DISK&&cfile->frames==0&&mainw->record_perf) {
    g_signal_handler_block(mainw->record_perf,mainw->record_perf_func);
    lives_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mainw->record_perf),FALSE);
    g_signal_handler_unblock(mainw->record_perf,mainw->record_perf_func);
  }

  // TODO - can this be done earlier ?
  if (mainw->cancelled==CANCEL_APP_QUIT) on_quit_activate (NULL,NULL);

  // end record performance

#ifdef ENABLE_JACK
  if (audio_player==AUD_PLAYER_JACK&&mainw->jackd!=NULL) {
    boolean timeout;
    int alarm_handle=lives_alarm_set(LIVES_ACONNECT_TIMEOUT);
    while (!(timeout=lives_alarm_get(alarm_handle))&&jack_get_msgq(mainw->jackd)!=NULL) {
      sched_yield(); // wait for seek
    }
    if (timeout) jack_try_reconnect();
    
    lives_alarm_clear(alarm_handle);

    mainw->jackd->in_use=FALSE;

    if (has_audio_buffers) {
      free_jack_audio_buffers();
      audio_free_fnames();
    }


  }
#endif
#ifdef HAVE_PULSE_AUDIO
  if (audio_player==AUD_PLAYER_PULSE&&mainw->pulsed!=NULL) {
    boolean timeout;
    int alarm_handle=lives_alarm_set(LIVES_ACONNECT_TIMEOUT);
    while (!(timeout=lives_alarm_get(alarm_handle))&&pulse_get_msgq(mainw->pulsed)!=NULL) {
      sched_yield(); // wait for seek
    }
    
    if (timeout) pulse_try_reconnect();
    
    lives_alarm_clear(alarm_handle);
    
    mainw->pulsed->in_use=FALSE;

    if (has_audio_buffers) {
      free_pulse_audio_buffers();
      audio_free_fnames();
    }

  }
#endif

  if (mainw->bad_aud_file!=NULL) {
    // we got an error recording audio
    do_write_failed_error_s(mainw->bad_aud_file,NULL);
    g_free(mainw->bad_aud_file);
    mainw->bad_aud_file=NULL;
  }

  // need to do this here, in case we want to preview a generator which will close to -1
  if (mainw->record) deal_with_render_choice(TRUE);
  mainw->record_paused=mainw->record_starting=FALSE;
  

  if (!mainw->preview&&cfile->clip_type==CLIP_TYPE_GENERATOR) {
    mainw->osc_block=TRUE;
    weed_generator_end ((weed_plant_t *)cfile->ext_src);
    mainw->osc_block=FALSE;
    if (mainw->multitrack==NULL) {
      if (mainw->files[current_file]!=NULL) switch_to_file (mainw->current_file,current_file);
      else if (mainw->pre_src_file!=-1) switch_to_file (mainw->current_file,mainw->pre_src_file);
    }
  }

  if (mainw->multitrack==NULL) mainw->osc_block=FALSE;

  reset_clip_menu();

  disable_record();

  if (mainw->multitrack==NULL&&mainw->current_file>-1)
    set_main_title(cfile->name,0);

  if (mainw->play_window!=NULL) {
    resize_play_window();
    //load_preview_image(FALSE);
  }
}


boolean get_temp_handle(int index, boolean create) {
  // we can call this to get a temp handle for returning info from the backend
  // this function is also called from get_new_handle to create a permanent handle
  // for an opened file

  // if a temp handle is required, pass in index as mainw->first_free_file, and
  // call 'smogrify close cfile->handle' on it after use, then restore mainw->current_file

  // returns FALSE if we couldn't write to tempdir

  // WARNING: this function changes mainw->current_file, unless it returns FALSE (could not create cfile)


  gchar *com;
  boolean is_unique;
  int current_file=mainw->current_file;

  if (index==-1) {
    too_many_files();
    return FALSE;
  }

  do {
    mainw->current_file=-1; // stop update of start/end frames

    is_unique=TRUE;

    com=g_strdup_printf("%s new %d",prefs->backend_sync,getpid());

    lives_system(com,TRUE);

    g_free(com);
    // ignore return value here, as it will be dealt with in get_handle_from_info_file()
    mainw->current_file=current_file;

    //get handle from info file, we will also malloc a new "file" struct here
    if (!get_handle_from_info_file(index)) {
      // timed out
      if (mainw->files[index]!=NULL) g_free(mainw->files[index]);
      mainw->files[index]=NULL;
      return FALSE;
    }

    mainw->current_file=index;

    if (strlen(mainw->set_name)>0) {
      gchar *setclipdir=g_build_filename(prefs->tmpdir,mainw->set_name,"clips",cfile->handle,NULL);
      if (g_file_test(setclipdir,G_FILE_TEST_IS_DIR)) is_unique=FALSE;
      g_free(setclipdir);
    }

  } while (!is_unique);

  if (create) create_cfile();

  return TRUE;
}



void create_cfile(void) {
  gchar *stfile;

  // any cfile (clip) initialisation goes in here
  cfile->menuentry=NULL;
  cfile->start=cfile->end=0;
  cfile->old_frames=cfile->frames=0;
  g_snprintf(cfile->type,40,"%s",_ ("Unknown"));
  cfile->f_size=0l;
  cfile->achans=0;
  cfile->arate=0;
  cfile->arps=0;
  cfile->afilesize=0l;
  cfile->asampsize=0;
  cfile->undoable=FALSE;
  cfile->redoable=FALSE;
  cfile->changed=FALSE;
  cfile->hsize=cfile->vsize=cfile->ohsize=cfile->ovsize=0;
  cfile->fps=cfile->pb_fps=prefs->default_fps;
  cfile->events[0]=NULL;
  cfile->insert_start=cfile->insert_end=0;
  cfile->is_untitled=TRUE;
  cfile->was_renamed=FALSE;
  cfile->undo_action=UNDO_NONE;
  cfile->opening_audio=cfile->opening=cfile->opening_only_audio=FALSE;
  cfile->pointer_time=0.;
  cfile->restoring=cfile->opening_loc=cfile->nopreview=cfile->is_loaded=FALSE;
  cfile->video_time=cfile->total_time=cfile->laudio_time=cfile->raudio_time=0.;
  cfile->freeze_fps=0.;
  cfile->frameno=cfile->last_frameno=0;
  cfile->proc_ptr=NULL;
  cfile->progress_start=cfile->progress_end=0;
  cfile->play_paused=cfile->nokeep=FALSE;
  cfile->undo_start=cfile->undo_end=0;
  cfile->rowstride=0; // unknown
  cfile->ext_src=NULL;
  cfile->clip_type=CLIP_TYPE_DISK;
  cfile->ratio_fps=FALSE;
  cfile->aseek_pos=0;
  cfile->unique_id=lives_random();
  cfile->layout_map=NULL;
  cfile->frame_index=cfile->frame_index_back=NULL;
  cfile->fx_frame_pump=0;
  cfile->stored_layout_frame=0;
  cfile->stored_layout_audio=0.;
  cfile->stored_layout_fps=0.;
  cfile->stored_layout_idx=-1;
  cfile->interlace=LIVES_INTERLACE_NONE;
  cfile->subt=NULL;
  cfile->op_dir=NULL;
  cfile->op_ds_warn_level=0;
  cfile->no_proc_sys_errors=cfile->no_proc_read_errors=cfile->no_proc_write_errors=FALSE;
  cfile->keep_without_preview=FALSE;

  if (!strcmp(prefs->image_ext,"jpg")) cfile->img_type=IMG_TYPE_JPEG;
  else cfile->img_type=IMG_TYPE_PNG;

  cfile->bpp=(cfile->img_type==IMG_TYPE_JPEG)?24:32;
  cfile->deinterlace=FALSE;

  cfile->play_paused=FALSE;
  cfile->header_version=LIVES_CLIP_HEADER_VERSION;

  cfile->event_list=cfile->event_list_back=NULL;
  cfile->next_event=NULL;

  memset(cfile->name,0,1);
  memset(cfile->mime_type,0,1);
  memset(cfile->file_name,0,1);
  memset(cfile->save_file_name,0,1);

  memset (cfile->comment,0,1);
  memset (cfile->author,0,1);
  memset (cfile->title,0,1);
  memset (cfile->keywords,0,1);

  cfile->signed_endian=AFORM_UNKNOWN;
  g_snprintf(cfile->undo_text,32,"%s",_ ("_Undo"));
  g_snprintf(cfile->redo_text,32,"%s",_ ("_Redo"));

#ifndef IS_MINGW
  stfile=g_build_filename(prefs->tmpdir,cfile->handle,".status",NULL);
#else
  stfile=g_build_filename(prefs->tmpdir,cfile->handle,"status",NULL);
#endif

  g_snprintf(cfile->info_file,PATH_MAX,"%s",stfile);
  g_free(stfile);
  
  cfile->laudio_drawable=NULL;
  cfile->raudio_drawable=NULL;

  // remember to set cfile->is_loaded=TRUE !!!!!!!!!!
}


boolean
get_new_handle (int index, const gchar *name) {
  // here is where we first initialize for the clipboard
  // and for paste_as_new, and restore
  // pass in name as NULL or "" and it will be set with an untitled number

  // this function *does not* change mainw->current_file, or add to the menu
  // or update mainw->clips_available
  gchar *xname;

  int current_file=mainw->current_file;
  if (!get_temp_handle(index,TRUE)) return FALSE;

  // note : don't need to update first_free_file for the clipboard 
  if (index!=0) {
    get_next_free_file();
  }

  if (name==NULL||!strlen(name)) {
    cfile->is_untitled=TRUE;
    xname=g_strdup_printf(_ ("Untitled%d"),mainw->untitled_number++);
  }
  else xname=g_strdup(name);

  g_snprintf(cfile->file_name,PATH_MAX,"%s",xname);
  g_snprintf(cfile->name,256,"%s",xname);
  mainw->current_file=current_file;

  g_free(xname);
  return TRUE;
}



boolean add_file_info(const gchar *check_handle, boolean aud_only) {
  // file information has been retrieved, set struct cfile with details
  // contained in mainw->msg. We do this twice, once before opening the file, once again after.
  // The first time, frames and afilesize may not be correct.
  int pieces;

  gchar *mesg,*mesg1;
  gchar **array;
  gchar *test_fps_string1;
  gchar *test_fps_string2;

 
  if (!strcmp(mainw->msg,"killed")) {
    get_frame_count(mainw->current_file);
    cfile->frames--; // just in case last frame is damaged
    reget_afilesize(mainw->current_file);
    mesg=g_strdup_printf(_("%d frames are enough !\n"),cfile->frames);
    d_print(mesg);
    g_free(mesg);
  }
  else {
    if (check_handle!=NULL) {
      if (mainw->msg==NULL||get_token_count(mainw->msg,'|')==1) return FALSE;

      array=g_strsplit(mainw->msg,"|",-1);
    
      // sanity check handle against status file
      // (this should never happen...)
    
      if (strcmp(check_handle,array[1])) {
	LIVES_ERROR("Handle!=statusfile !");
	mesg=g_strdup_printf(_("\nError getting file info for clip %s.\nBad things may happen with this clip.\n"),
			     check_handle);
	do_error_dialog(mesg);
	g_free(mesg);
	g_strfreev(array);
	return FALSE;
      }
    
      if (!aud_only) {
	cfile->frames=atoi(array[2]);
	g_snprintf(cfile->type,40,"%s",array[3]);
	cfile->hsize=atoi(array[4]);
	cfile->vsize=atoi(array[5]);
	cfile->bpp=atoi(array[6]);
	cfile->pb_fps=cfile->fps=g_strtod(array[7],NULL);
      
	cfile->f_size=strtol(array[8],NULL,10);
      }

      cfile->arps=cfile->arate=atoi(array[9]);
      cfile->achans=atoi(array[10]);
      cfile->asampsize=atoi(array[11]);
      cfile->signed_endian=get_signed_endian(atoi (array[12]), atoi (array[13]));
      cfile->afilesize=strtol(array[14],NULL,10);
    
      pieces=get_token_count (mainw->msg,'|');
    
      if (!strlen(cfile->title)&&pieces>14&&array[15]!=NULL) {
	g_snprintf (cfile->title,256,"%s",g_strstrip(array[15]));
      }
      if (!strlen(cfile->author)&&pieces>15&&array[16]!=NULL) {
	g_snprintf (cfile->author,256,"%s",g_strstrip(array[16]));
      }
      if (!strlen(cfile->comment)&&pieces>16&&array[17]!=NULL) {
	g_snprintf (cfile->comment,256,"%s",g_strstrip(array[17]));
      }
    
      g_strfreev(array);
    }
  }
  cfile->video_time=0;

  if (aud_only) return TRUE;

  test_fps_string1=g_strdup_printf ("%.3f00000",cfile->fps);
  test_fps_string2=g_strdup_printf ("%.8f",cfile->fps);

  if (strcmp (test_fps_string1,test_fps_string2)) {
    cfile->ratio_fps=TRUE;
  }
  else {
    cfile->ratio_fps=FALSE;
  }
  g_free (test_fps_string1);
  g_free (test_fps_string2);

  if (!mainw->save_with_sound) {
    cfile->arps=cfile->arate=cfile->achans=cfile->asampsize=0;
    cfile->afilesize=0l;
  }

  if (cfile->frames<=0) {
    if (cfile->afilesize==0l&&cfile->is_loaded) {
      // we got no video or audio...
      return FALSE;
    }
    cfile->start=cfile->end=cfile->undo_start=cfile->undo_end=0;
  }
  else {
  // start with all selected
    cfile->start=1;
    cfile->end=cfile->frames;
    cfile->undo_start=cfile->start;
    cfile->undo_end=cfile->end;
  }

  cfile->orig_file_name=TRUE;
  cfile->is_untitled=FALSE;

  // some files give us silly frame rates, even single frames...
  // fps of 1000. is used for some streams (i.e. play each frame as it is received)
  if (cfile->fps==0.||cfile->fps==1000.||(cfile->frames<2&&cfile->is_loaded)) {
      double xduration=0.;

      if (cfile->ext_src!=NULL&&cfile->fps>0) {
	  xduration=cfile->frames/cfile->fps;
      }

    if (!(cfile->afilesize*cfile->asampsize*cfile->arate*cfile->achans)||cfile->frames<2) {
      if (cfile->frames!=1) {
      mesg=g_strdup_printf(_ ("\nPlayback speed not found or invalid ! Using default fps of %.3f fps. \nDefault can be set in Tools | Preferences | Misc.\n"),prefs->default_fps);
      d_print(mesg);
      g_free(mesg);
      }
      cfile->pb_fps=cfile->fps=prefs->default_fps;
    }
    else {
      cfile->laudio_time=cfile->raudio_time=cfile->afilesize/cfile->asampsize*8./cfile->arate/cfile->achans;
      cfile->pb_fps=cfile->fps=1.*(int)(cfile->frames/cfile->laudio_time);
      if (cfile->fps>FPS_MAX||cfile->fps<1.) {
	cfile->pb_fps=cfile->fps=prefs->default_fps;
      }
      mesg=g_strdup_printf(_ ("Playback speed was adjusted to %.3f frames per second to fit audio.\n"),cfile->fps);
      d_print(mesg);
      g_free(mesg);
    }

    if (xduration>0.) {
	lives_clip_data_t *cdata=((lives_decoder_t *)cfile->ext_src)->cdata;
	// we should not (!) do this, but we don t have proper handling for variable fps clips
	cdata->nframes=cfile->frames=xduration*cfile->fps;
	cdata->fps=cfile->fps;
    }

  }

  cfile->video_time=(double)cfile->frames/cfile->fps;

  if (cfile->opening) return TRUE;

  if (cfile->bpp==256) {
    mesg1=g_strdup_printf(_ ("Frames=%d type=%s size=%dx%d *bpp=Greyscale* fps=%.3f\nAudio:"),cfile->frames,cfile->type,cfile->hsize,cfile->vsize,cfile->fps);
  }
  else {
    if (cfile->bpp!=32) cfile->bpp=24; // assume RGB24  *** TODO - check
    mesg1=g_strdup_printf(_ ("Frames=%d type=%s size=%dx%d bpp=%d fps=%.3f\nAudio:"),cfile->frames,cfile->type,cfile->hsize,cfile->vsize,cfile->bpp,cfile->fps);
  }

  if (cfile->achans==0) {
    mesg=g_strdup_printf (_ ("%s none\n"),mesg1);
  }
  else {
    mesg=g_strdup_printf(P_ ("%s %d Hz %d channel %d bps\n","%s %d Hz %d channels %d bps\n",cfile->achans),
			 mesg1,cfile->arate,cfile->achans,cfile->asampsize);
  }
  d_print(mesg);
  g_free(mesg1);
  g_free(mesg);

  // get the author,title,comments
  if (strlen (cfile->author)) {
    mesg=g_strdup_printf(_ (" - Author: %s\n"),cfile->comment);
    d_print(mesg);
    g_free(mesg);
  }
  if (strlen (cfile->title)) {
    mesg=g_strdup_printf(_ (" - Title: %s\n"),cfile->comment);
    d_print(mesg);
    g_free(mesg);
  }
  if (strlen (cfile->comment)) {
    mesg=g_strdup_printf(_ (" - Comment: %s\n"),cfile->comment);
    d_print(mesg);
    g_free(mesg);
  }

  return TRUE;
}


boolean save_file_comments (int fileno) {
  // save the comments etc for smogrify
  int retval;
  int comment_fd;
  gchar *comment_file=g_strdup_printf ("%s/%s/.comment",prefs->tmpdir,cfile->handle);
  file *sfile=mainw->files[fileno];

  unlink (comment_file);

  do {
    retval=0;
    comment_fd=creat(comment_file,S_IRUSR|S_IWUSR);
    if (comment_fd<0) {
      mainw->write_failed=TRUE;
      retval=do_write_failed_error_s_with_retry(comment_file,g_strerror(errno),NULL);
    }
    else {
      mainw->write_failed=FALSE;
      lives_write(comment_fd,sfile->title,strlen (sfile->title),TRUE);
      lives_write(comment_fd,"||%",3,TRUE);
      lives_write(comment_fd,sfile->author,strlen (sfile->author),TRUE);
      lives_write(comment_fd,"||%",3,TRUE);
      lives_write(comment_fd,sfile->comment,strlen (sfile->comment),TRUE);
      
      close (comment_fd);
      
      if (mainw->write_failed) {
	retval=do_write_failed_error_s_with_retry(comment_file,NULL,NULL);
      }
    }
  } while (retval==LIVES_RETRY);

  g_free (comment_file);

  if (mainw->write_failed) return FALSE;

  return TRUE;
}





void
wait_for_stop (const gchar *stop_command) {
  FILE *infofile;

  // only used for audio player mplayer or audio player sox

# define SECOND_STOP_TIME 0.1
# define STOP_GIVE_UP_TIME 1.0

  double time_waited=0.;
  boolean sent_second_stop=FALSE;
  
  // send another stop if necessary
  mainw->noswitch=TRUE;
  while (!(infofile=fopen(cfile->info_file,"r"))) {
    lives_widget_context_update();
    g_usleep(prefs->sleep_time);
    time_waited+=1000000./prefs->sleep_time;
    if (time_waited>SECOND_STOP_TIME&&!sent_second_stop) {
      lives_system(stop_command,TRUE);
      sent_second_stop=TRUE;
    }
    
    if (time_waited>STOP_GIVE_UP_TIME) {
      // give up waiting, but send a last try...
      lives_system(stop_command,TRUE);
      break;
    }
  }
  mainw->noswitch=FALSE;
  if (infofile) fclose (infofile);
}


boolean save_frame_inner(int clip, int frame, const gchar *file_name, int width, int height, boolean from_osc) {
  // save 1 frame as an image (uses imagemagick to convert)
  // width==-1, height==-1 to use "natural" values

  file *sfile=mainw->files[clip];
  gchar full_file_name[PATH_MAX];
  gchar *com,*tmp;

  boolean allow_over=FALSE;
  int result;

  if (!from_osc&&strrchr(file_name,'.')==NULL) {
    g_snprintf(full_file_name,PATH_MAX,"%s.%s",file_name,get_image_ext_for_type(sfile->img_type));
  }
  else {
    g_snprintf(full_file_name,PATH_MAX,"%s",file_name);
    if (!allow_over) allow_over=TRUE;
  }

  // TODO - allow overwriting in sandbox
  if (from_osc&&g_file_test (full_file_name, G_FILE_TEST_EXISTS)) return FALSE;

  if (!check_file(full_file_name,!allow_over)) return FALSE;

  tmp=g_filename_from_utf8 (full_file_name,-1,NULL,NULL,NULL);

  if (mainw->multitrack==NULL) {
    com=g_strdup_printf(_ ("Saving frame %d as %s..."),frame,full_file_name);
    d_print(com);
    g_free(com);
    
    if (sfile->clip_type==CLIP_TYPE_FILE) {
      boolean resb=virtual_to_images(clip,frame,frame,FALSE,NULL);
      if (!resb) {
	d_print_file_error_failed();
	return FALSE;
      }
    }

    com=g_strdup_printf("%s save_frame %s %d %s %d %d",prefs->backend_sync,sfile->handle,
			frame,tmp,width,height);
    result=lives_system(com,FALSE);
    g_free(com);
    g_free(tmp);
    
    if (result==256) {
      d_print_file_error_failed();
      do_file_perm_error(full_file_name);
      return FALSE;
    }

    if (result==0) {
      d_print_done();
      return TRUE;
    }
  }
  else {
    // multitrack mode
    GError *gerr=NULL;
    LiVESPixbuf *pixbuf;
    int retval;

    mt_show_current_frame(mainw->multitrack,TRUE);
    resize_layer(mainw->frame_layer,sfile->hsize,sfile->vsize,LIVES_INTERP_BEST,WEED_PALETTE_RGB24,0);
    convert_layer_palette(mainw->frame_layer,WEED_PALETTE_RGB24,0);
    pixbuf=layer_to_pixbuf(mainw->frame_layer);
    weed_plant_free(mainw->frame_layer);
    mainw->frame_layer=NULL;

    do {
      retval=0;
      if (sfile->img_type==IMG_TYPE_JPEG) lives_pixbuf_save(pixbuf, tmp, IMG_TYPE_JPEG, 100, FALSE, &gerr);
      else if (sfile->img_type==IMG_TYPE_PNG) lives_pixbuf_save(pixbuf, tmp, IMG_TYPE_PNG, 100, FALSE, &gerr);

      if (gerr!=NULL) {
	retval=do_write_failed_error_s_with_retry(full_file_name,gerr->message,NULL);
	g_error_free(gerr);
	gerr=NULL;
      }

    } while (retval==LIVES_RETRY);

    free(tmp);
    lives_object_unref(pixbuf);
  }

  // some other error condition
  return FALSE;
}


void backup_file(int clip, int start, int end, const gchar *file_name) {
  file *sfile=mainw->files[clip];
  gchar **array;

  gchar title[256];
  gchar full_file_name[PATH_MAX];

  gchar *com,*tmp;

  boolean with_perf=FALSE;
  boolean retval,allow_over;

  int withsound=1;
  int current_file=mainw->current_file;

  if (strrchr(file_name,'.')==NULL) {
    g_snprintf(full_file_name,PATH_MAX,"%s.lv1",file_name);
    allow_over=FALSE;
   }
  else {
    g_snprintf(full_file_name,PATH_MAX,"%s",file_name);
    allow_over=TRUE;
  }

  // check if file exists
  if (!check_file(full_file_name,allow_over)) return;

  // create header files
  retval=write_headers(sfile); // for pre LiVES 0.9.6
  retval=save_clip_values(clip); // new style (0.9.6+)

  if (!retval) return;

  //...and backup
  get_menu_text(sfile->menuentry,title);
  com=g_strdup_printf(_ ("Backing up %s to %s"),title,full_file_name);
  d_print(com);
  g_free(com);

  if (!mainw->save_with_sound) {
    d_print(_ (" without sound"));
    withsound=0;
  }

  d_print("...");
  cfile->progress_start=1;
  cfile->progress_end=sfile->frames;

  if (sfile->clip_type==CLIP_TYPE_FILE) {
    boolean resb;
    mainw->cancelled=CANCEL_NONE;
    cfile->progress_start=1;
    cfile->progress_end=count_virtual_frames(sfile->frame_index,1,sfile->frames);
    do_threaded_dialog(_("Pulling frames from clip"),TRUE);
    resb=virtual_to_images(clip,1,sfile->frames,TRUE,NULL);
    end_threaded_dialog();

    if (mainw->cancelled!=CANCEL_NONE||!resb) {
      sensitize();
      mainw->cancelled=CANCEL_USER;
      cfile->nopreview=FALSE;
      if (!resb) d_print_file_error_failed();
      else d_print_cancelled();
      return;
    }
  }

  com=g_strdup_printf("%s backup %s %d %d %d %s",prefs->backend,sfile->handle,withsound,
		      start,end,(tmp=g_filename_from_utf8 (full_file_name,-1,NULL,NULL,NULL)));

  // TODO
  mainw->current_file=clip;

  unlink (cfile->info_file);
  cfile->nopreview=TRUE;
  mainw->com_failed=FALSE;
  lives_system(com,FALSE);
  g_free(tmp);

  if (mainw->com_failed) {
    mainw->com_failed=FALSE;
    mainw->current_file=current_file;
    return;
  }

  cfile->op_dir=g_filename_from_utf8((tmp=get_dir(full_file_name)),-1,NULL,NULL,NULL);
  g_free(tmp);

  if (!(do_progress_dialog(TRUE,TRUE,_ ("Backing up")))||mainw->error) {
    if (mainw->error) {
      d_print_failed();
    }

    // cancelled - clear up files
    cfile->nopreview=FALSE;
    g_free (com);

    // using restore details in the 'wrong' way here...it will also clear files
    com=g_strdup_printf("%s restore_details %s",prefs->backend,cfile->handle);
    unlink (cfile->info_file);

    lives_system(com,FALSE);
    // auto-d
    g_free(com);

    //save_clip_values(mainw->current_file);
    mainw->current_file=current_file;
    return;
  }

  cfile->nopreview=FALSE;
  g_free(com);

  mainw->current_file=current_file;

  if (mainw->error) {
    do_error_dialog(mainw->msg);
    d_print_failed();
    return;
  }

  if (with_perf) {
    d_print(_ ("performance data was backed up..."));
  }

  array=g_strsplit(mainw->msg,"|",3);
  sfile->f_size=strtol(array[1],NULL,10);
  g_strfreev(array);

  g_snprintf(sfile->file_name,PATH_MAX,"%s",full_file_name);
  if (!sfile->was_renamed) {
    g_snprintf(sfile->name,256,"%s",full_file_name);
    set_main_title(cfile->name,0);
    set_menu_text(sfile->menuentry,full_file_name,FALSE);
  }
  add_to_recent (full_file_name,0.,0,NULL);

  sfile->changed=FALSE;
  // set is_untitled to stop users from saving with a .lv1 extension
  sfile->is_untitled=TRUE;
  d_print_done();
}


boolean write_headers (file *file) {
  // this function is included only for backwards compatibility with ancient builds of LiVES
  //

  int retval;
  int header_fd;
  gchar *hdrfile;

  // save the file details
  hdrfile=g_build_filename(prefs->tmpdir,file->handle,"header",NULL);

  do {
    retval=0;
    header_fd=creat(hdrfile,S_IRUSR|S_IWUSR);
    if (header_fd<0) {
      retval=do_write_failed_error_s_with_retry(hdrfile,g_strerror(errno),NULL);
    }
    else {
      mainw->write_failed=FALSE;
      
      lives_write_le(header_fd,&cfile->bpp,4,TRUE);
      lives_write_le(header_fd,&cfile->fps,8,TRUE);
      lives_write_le(header_fd,&cfile->hsize,4,TRUE);
      lives_write_le(header_fd,&cfile->vsize,4,TRUE);
      lives_write_le(header_fd,&cfile->arps,4,TRUE);
      lives_write_le(header_fd,&cfile->signed_endian,4,TRUE);
      lives_write_le(header_fd,&cfile->arate,4,TRUE);
      lives_write_le(header_fd,&cfile->unique_id,8,TRUE);
      lives_write_le(header_fd,&cfile->achans,4,TRUE);
      lives_write_le(header_fd,&cfile->asampsize,4,TRUE);
      
      lives_write(header_fd,LiVES_VERSION,strlen(LiVES_VERSION),TRUE);
      close(header_fd);
      
      if (mainw->write_failed) retval=do_write_failed_error_s_with_retry(hdrfile,NULL,NULL);
      
    }
  } while (retval==LIVES_RETRY);


  g_free(hdrfile);

  if (retval!=LIVES_CANCEL) {
    // more file details (since version 0.7.5)
    hdrfile=g_build_filename(prefs->tmpdir,file->handle,"header2",NULL);

    do {
      retval=0;
      header_fd=creat(hdrfile,S_IRUSR|S_IWUSR);
    
      if (header_fd<0) {
	retval=do_write_failed_error_s_with_retry(hdrfile,g_strerror(errno),NULL);
      }
      else {
	mainw->write_failed=FALSE;
	lives_write_le(header_fd,&file->frames,4,TRUE);
	lives_write(header_fd,&file->title,256,TRUE);
	lives_write(header_fd,&file->author,256,TRUE);
	lives_write(header_fd,&file->comment,256,TRUE);
	close(header_fd);
      }
      
      if (mainw->write_failed) retval=do_write_failed_error_s_with_retry(hdrfile,NULL,NULL);
    } while (retval==LIVES_RETRY);

    g_free(hdrfile);
  }

  if (retval==LIVES_CANCEL) {
    mainw->write_failed=FALSE;
    return FALSE;
  }
  return TRUE;

}


boolean read_headers(const gchar *file_name) {
  // file_name is only used to get the file size on the disk
  FILE *infofile;
  gchar **array;
  gchar buff[1024];
  gchar version[32];
  gchar *com,*tmp;
  gchar *old_hdrfile=g_build_filename(prefs->tmpdir,cfile->handle,"header",NULL);
  gchar *lives_header=g_build_filename(prefs->tmpdir,cfile->handle,"header.lives",NULL);

  int header_size;
  int version_hash;
  int pieces;
  int header_fd;
  int alarm_handle;
  int retval2;

  lives_clip_details_t detail;

  boolean timeout;
  boolean retval,retvala;

  ssize_t sizhead=8*4+8+8;

  time_t old_time=0,new_time=0;
  struct stat mystat;

  // TODO - remove this some time before 2038...
  if (!stat(old_hdrfile,&mystat)) old_time=mystat.st_mtime;
  if (!stat(lives_header,&mystat)) new_time=mystat.st_mtime;
  ///////////////

  if (old_time<=new_time) {
    do {
      retval2=0;

      detail=CLIP_DETAILS_FRAMES;
      if (get_clip_value(mainw->current_file,detail,&cfile->frames,0)) {
	int asigned,aendian;
	gchar *tmp;
	int alarm_handle;

	// use new style header (LiVES 0.9.6+)
	g_free(old_hdrfile);

	// clean up and get file sizes
	com=g_strdup_printf("%s restore_details %s %s %d",prefs->backend_sync,cfile->handle,
			    (tmp=g_filename_from_utf8 (file_name,-1,NULL,NULL,NULL)),!strcmp (file_name,"."));

	mainw->com_failed=FALSE;
	lives_system(com,FALSE);
	g_free(com);
	g_free(tmp);
     
	if (mainw->com_failed) {
	  mainw->com_failed=FALSE;
	  return FALSE;
	}


	do {
	  retval2=0;
	  timeout=FALSE;
	  memset(buff,0,1);

#define LIVES_RESTORE_TIMEOUT  (30 * U_SEC) // 30 sec

	  alarm_handle=lives_alarm_set(LIVES_RESTORE_TIMEOUT);

	  while (!((infofile=fopen(cfile->info_file,"r")) || (timeout=lives_alarm_get(alarm_handle)))) {
	    g_usleep(prefs->sleep_time);
	  }
	  
	  lives_alarm_clear(alarm_handle);
	  
	  if (!timeout) {
	    mainw->read_failed=FALSE;
	    lives_fgets(buff,1024,infofile);
	    fclose(infofile);
	  }

	  if (timeout || mainw->read_failed) {
	    retval2=do_read_failed_error_s_with_retry(cfile->info_file,NULL,NULL);
	  }
	} while (retval2==LIVES_RETRY);

	if (retval2==LIVES_CANCEL) {
	  return FALSE;
	}

	pieces=get_token_count (buff,'|');

	if (pieces>3) {
	  array=g_strsplit(buff,"|",pieces);
	
	  cfile->f_size=strtol(array[1],NULL,10);
	  cfile->afilesize=strtol(array[2],NULL,10);
	  if (!strcmp(array[3],"jpg")) cfile->img_type=IMG_TYPE_JPEG;
	  else cfile->img_type=IMG_TYPE_PNG;
	  g_strfreev(array);
	}

	threaded_dialog_spin();

	do {
	  retval2=0;
	  if (!cache_file_contents(lives_header)) {
	    retval2=do_read_failed_error_s_with_retry(lives_header,NULL,NULL);
	  }
	} while (retval2==LIVES_RETRY);

	g_free(lives_header);

	threaded_dialog_spin();

	detail=CLIP_DETAILS_HEADER_VERSION;
	retval=get_clip_value(mainw->current_file,detail,&cfile->header_version,16);
	if (retval) {
	  detail=CLIP_DETAILS_BPP;
	  retval=get_clip_value(mainw->current_file,detail,&cfile->bpp,0);
	}
	if (retval) {
	  detail=CLIP_DETAILS_FPS;
	  retval=get_clip_value(mainw->current_file,detail,&cfile->fps,0);
	}
	if (retval) {
	  detail=CLIP_DETAILS_PB_FPS;
	  retval=get_clip_value(mainw->current_file,detail,&cfile->pb_fps,0);
	  if (!retval) {
	    retval=TRUE;
	    cfile->pb_fps=cfile->fps;
	  }
	}
	if (retval) {
	  retval=get_clip_value(mainw->current_file,CLIP_DETAILS_PB_FRAMENO,&cfile->frameno,0);
	  if (!retval) {
	    retval=TRUE;
	    cfile->frameno=1;
	  }
	}
	if (retval) {
	  detail=CLIP_DETAILS_WIDTH;
	  retval=get_clip_value(mainw->current_file,detail,&cfile->hsize,0);
	}
	if (retval) {
	  detail=CLIP_DETAILS_HEIGHT;
	  retval=get_clip_value(mainw->current_file,detail,&cfile->vsize,0);
	}
	if (retval) {
	  detail=CLIP_DETAILS_CLIPNAME;
	  get_clip_value(mainw->current_file,detail,cfile->name,256);
	}
	if (retval) {
	  detail=CLIP_DETAILS_FILENAME;
	  get_clip_value(mainw->current_file,detail,cfile->file_name,PATH_MAX);
	}

	if (retval) {
	  detail=CLIP_DETAILS_ACHANS;
	  retvala=get_clip_value(mainw->current_file,detail,&cfile->achans,0);
	  if (!retvala) cfile->achans=0;
	}

	if (cfile->achans==0) retvala=FALSE;
	else retvala=TRUE;

	if (retval&&retvala) {
	  detail=CLIP_DETAILS_ARATE;
	  retvala=get_clip_value(mainw->current_file,detail,&cfile->arps,0);
	}

	if (!retvala) cfile->arps=cfile->achans=cfile->arate=cfile->asampsize=0;
	if (cfile->arps==0) retvala=FALSE; 

	if (retvala&&retval) {
	  detail=CLIP_DETAILS_PB_ARATE;
	  retvala=get_clip_value(mainw->current_file,detail,&cfile->arate,0);
	  if (!retvala) {
	    retvala=TRUE;
	    cfile->arate=cfile->arps;
	  }
	}
	if (retvala&&retval) {
	  detail=CLIP_DETAILS_ASIGNED;
	  retval=get_clip_value(mainw->current_file,detail,&asigned,0);
	}
	if (retvala&&retval) {
	  detail=CLIP_DETAILS_AENDIAN;
	  retval=get_clip_value(mainw->current_file,detail,&aendian,0);
	}

	cfile->signed_endian=asigned+aendian;

	if (retvala&&retval) {
	  detail=CLIP_DETAILS_ASAMPS;
	  retval=get_clip_value(mainw->current_file,detail,&cfile->asampsize,0);
	}
	
	get_clip_value(mainw->current_file,CLIP_DETAILS_TITLE,cfile->title,256);
	get_clip_value(mainw->current_file,CLIP_DETAILS_AUTHOR,cfile->author,256);
	get_clip_value(mainw->current_file,CLIP_DETAILS_COMMENT,cfile->comment,256);
	get_clip_value(mainw->current_file,CLIP_DETAILS_KEYWORDS,cfile->keywords,1024);
	get_clip_value(mainw->current_file,CLIP_DETAILS_INTERLACE,&cfile->interlace,0);
	if (cfile->interlace!=LIVES_INTERLACE_NONE) cfile->deinterlace=TRUE; // user must have forced this

	if (!retval) {
	  if (mainw->cached_list!=NULL) {
	    retval2=do_header_missing_detail_error(mainw->current_file,detail);
	  }
	  else {
	    retval2=do_header_read_error_with_retry(mainw->current_file);
	  }
	}
	else return TRUE;
      }
      else {
	if (mainw->cached_list!=NULL) {
	  retval2=do_header_missing_detail_error(mainw->current_file,CLIP_DETAILS_FRAMES);
	}
	else {
	  retval2=do_header_read_error_with_retry(mainw->current_file);
	}
      }
    } while (retval2==LIVES_RETRY);
    return FALSE; // retval2==LIVES_CANCEL
  }

  // old style headers (pre 0.9.6)
  g_free(lives_header);

  do {
    retval=0;
    mainw->read_failed=FALSE;
    memset (version,0,32);
    memset (buff,0,1024);
    
    header_fd=open(old_hdrfile,O_RDONLY);

    if (header_fd<0) {
      retval=do_read_failed_error_s_with_retry(old_hdrfile,g_strerror(errno),NULL);
    }
    else {
      mainw->read_failed=FALSE;
      header_size=get_file_size(header_fd);
  
      if (header_size<sizhead) {
	g_free(old_hdrfile);
	close (header_fd);
	return FALSE;
      }
      else {
	mainw->read_failed=FALSE;
	lives_read_le(header_fd,&cfile->bpp,4,FALSE);
	if (!mainw->read_failed)
	  lives_read_le(header_fd,&cfile->fps,8,FALSE);
	if (!mainw->read_failed)
	  lives_read_le(header_fd,&cfile->hsize,4,FALSE);
	if (!mainw->read_failed)
	  lives_read_le(header_fd,&cfile->vsize,4,FALSE);
	if (!mainw->read_failed)
	  lives_read_le(header_fd,&cfile->arps,4,FALSE);
	if (!mainw->read_failed)
	  lives_read_le(header_fd,&cfile->signed_endian,4,FALSE);
	if (!mainw->read_failed)
	  lives_read_le(header_fd,&cfile->arate,4,FALSE);
	if (!mainw->read_failed)
	  lives_read_le(header_fd,&cfile->unique_id,8,FALSE);
	if (!mainw->read_failed)
	  lives_read_le(header_fd,&cfile->achans,4,FALSE);
	if (!mainw->read_failed)
	  lives_read_le(header_fd,&cfile->asampsize,4,FALSE);
      
	if (header_size>sizhead) {
	  if (header_size-sizhead>31) {
	    if (!mainw->read_failed)
	      lives_read(header_fd,&version,31,FALSE);
	    version[31]='\0';
	  }
	  else {
	    if (!mainw->read_failed)
	      lives_read(header_fd,&version,header_size-sizhead,FALSE);
	    version[header_size-sizhead]='\0';
	  }
	}
      }
      close(header_fd);
    }

    if (mainw->read_failed) {
      retval=do_read_failed_error_s_with_retry(old_hdrfile,NULL,NULL);
      if (retval==LIVES_CANCEL) {
	g_free(old_hdrfile);
	return FALSE;
      }
    }
  } while (retval==LIVES_RETRY);

  g_free(old_hdrfile);

  // handle version changes
  version_hash=verhash(version);
  if (version_hash<7001) {
    cfile->arps=cfile->arate;
    cfile->signed_endian=mainw->endian;
  }
  
  com=g_strdup_printf("%s restore_details %s %s %d",prefs->backend_sync,cfile->handle,
		      (tmp=g_filename_from_utf8 (file_name,-1,NULL,NULL,NULL)),!strcmp (file_name,"."));
  mainw->com_failed=FALSE;
  lives_system(com,FALSE);
  g_free(com);
  g_free(tmp);

  if (mainw->com_failed) {
    mainw->com_failed=FALSE;
    return FALSE;
  }

#define LIVES_RESTORE_TIMEOUT  (30 * U_SEC) // 120 sec timeout

  do {
    retval2=0;
    timeout=FALSE;

    alarm_handle=lives_alarm_set(LIVES_RESTORE_TIMEOUT);

    while (!((infofile=fopen(cfile->info_file,"r")) || (timeout=lives_alarm_get(alarm_handle)))) {
      g_usleep(prefs->sleep_time);
    }

    lives_alarm_clear(alarm_handle);
  
    if (!timeout) {
      mainw->read_failed=FALSE;
      lives_fgets(buff,1024,infofile);
      fclose(infofile);
    }

    if (timeout || mainw->read_failed) {
      retval2=do_read_failed_error_s_with_retry(cfile->info_file,NULL,NULL);
    }
  
  } while (retval2==LIVES_RETRY);

  if (retval2==LIVES_CANCEL) {
    mainw->read_failed=FALSE;
    return FALSE;
  }

  pieces=get_token_count (buff,'|');
  array=g_strsplit(buff,"|",pieces);
  cfile->f_size=strtol(array[1],NULL,10);
  cfile->afilesize=strtol(array[2],NULL,10);

  if (!strcmp(array[3],"jpg")) cfile->img_type=IMG_TYPE_JPEG;
  else cfile->img_type=IMG_TYPE_PNG;

  cfile->frames=atoi(array[4]);

  cfile->bpp=(cfile->img_type==IMG_TYPE_JPEG)?24:32;
  
  if (pieces>4&&array[5]!=NULL) {
    g_snprintf (cfile->title,256,"%s",g_strstrip(array[4]));
  }
  if (pieces>5&&array[6]!=NULL) {
    g_snprintf (cfile->author,256,"%s",g_strstrip(array[5]));
  }
  if (pieces>6&&array[7]!=NULL) {
    g_snprintf (cfile->comment,256,"%s",g_strstrip(array[6]));
  }
  
  g_strfreev(array);
  return TRUE;
}


void open_set_file (const gchar *set_name, int clipnum) {
  gchar name[256];
  boolean needs_update=FALSE;

  if (mainw->current_file<1) return;

  memset (name,0,256);

  if (mainw->cached_list!=NULL) {
    boolean retval;
    // LiVES 0.9.6+

    retval=get_clip_value(mainw->current_file,CLIP_DETAILS_PB_FPS,&cfile->pb_fps,0);
    if (!retval) {
      cfile->pb_fps=cfile->fps;
    }
    retval=get_clip_value(mainw->current_file,CLIP_DETAILS_PB_FRAMENO,&cfile->frameno,0);
    if (!retval) {
      cfile->frameno=1;
    }

    retval=get_clip_value(mainw->current_file,CLIP_DETAILS_CLIPNAME,name,256);
    if (!retval) {
      g_snprintf(name,256,_ ("Untitled%d"),mainw->untitled_number++);
      needs_update=TRUE;
    }
    retval=get_clip_value(mainw->current_file,CLIP_DETAILS_UNIQUE_ID,&cfile->unique_id,0);
    if (!retval) {
      cfile->unique_id=lives_random();
      needs_update=TRUE;
    }
    retval=get_clip_value(mainw->current_file,CLIP_DETAILS_INTERLACE,&cfile->interlace,0);
    if (!retval) {
      cfile->interlace=LIVES_INTERLACE_NONE;
      needs_update=TRUE;
    }
    if (cfile->interlace!=LIVES_INTERLACE_NONE) cfile->deinterlace=TRUE;

  }
  else {
    // pre 0.9.6 <- ancient code
    ssize_t nlen;
    int set_fd;
    int pb_fps;
    int retval;
    gchar *setfile=g_strdup_printf("%s/%s/set.%s",prefs->tmpdir,cfile->handle,set_name);

    do {
      retval=0;
      if ((set_fd=open(setfile,O_RDONLY))>-1) {
	// get perf_start
	if ((nlen=lives_read_le(set_fd,&pb_fps,4,TRUE))>0) {
	  cfile->pb_fps=pb_fps/1000.;
	  lives_read_le(set_fd,&cfile->frameno,4,TRUE);
	  lives_read(set_fd,name,256,TRUE);
	}
	close (set_fd);
      }
      else retval=do_read_failed_error_s_with_retry(setfile,g_strerror(errno),NULL);
    } while (retval==LIVES_RETRY);

    g_free (setfile);
    needs_update=TRUE;
  }

  if (strlen(name)==0) {
    g_snprintf (name,256,"set_clip %.3d",clipnum);
  }
  if (strlen(mainw->set_name)&&strcmp (name+strlen (name)-1,")")) {
    g_snprintf (cfile->name,256,"%s (%s)",name,set_name);
  }
  else {
    g_snprintf (cfile->name,256,"%s",name);
  }

  if (needs_update) save_clip_values(mainw->current_file);

}



void restore_file(const gchar *file_name) {
  gchar *com=g_strdup("dummy");
  gchar *mesg,*mesg1,*tmp;
  boolean is_OK=TRUE;
  gchar *fname=g_strdup(file_name);

  int old_file=mainw->current_file,current_file;
  int new_file=mainw->first_free_file;
  boolean not_cancelled;

  gchar *subfname;

  // create a new file
  if (!get_new_handle(new_file,fname)) {
    return;
  }
  

  mesg=g_strdup_printf(_ ("Restoring %s..."),file_name);
  d_print(mesg);
  g_free(mesg);
  
  mainw->current_file=new_file;
  
  cfile->hsize=mainw->def_width;
  cfile->vsize=mainw->def_height;
  
  switch_to_file((mainw->current_file=old_file),new_file);
  set_main_title(cfile->file_name,0);
  
  
  com=g_strdup_printf("%s restore %s %s",prefs->backend,cfile->handle,
		      (tmp=g_filename_from_utf8(file_name,-1,NULL,NULL,NULL)));

  mainw->com_failed=FALSE;
  unlink (cfile->info_file);

  lives_system(com,FALSE);

  g_free(tmp);
  g_free(com);
  
  if (mainw->com_failed) {
    mainw->com_failed=FALSE;
    close_current_file(old_file);
    return;
  }

  cfile->restoring=TRUE;
  not_cancelled=do_progress_dialog(TRUE,TRUE,_ ("Restoring"));
  cfile->restoring=FALSE;

  if (mainw->error||!not_cancelled) {
    if (mainw->error && mainw->cancelled!=CANCEL_ERROR) {
      do_blocking_error_dialog (mainw->msg);
    }
    close_current_file(old_file);
    return;
  }
  
  // call function to return rest of file details
  //fsize, afilesize and frames
  is_OK=read_headers(file_name);

  if (mainw->cached_list!=NULL) {
    g_list_free_strings(mainw->cached_list);
    g_list_free(mainw->cached_list);
    mainw->cached_list=NULL;
  }

  if (!is_OK) {
    mesg=g_strdup_printf(_ ("\n\nThe file %s is corrupt.\nLiVES was unable to restore it.\n"),file_name);
    do_blocking_error_dialog(mesg);
    g_free(mesg);
    
    d_print_failed();
    close_current_file(old_file);
    return;
  }
  if (!check_frame_count(mainw->current_file)) get_frame_count(mainw->current_file);
  
  // add entry to window menu
  // TODO - do this earlier and allow switching during restore
  add_to_winmenu();

  if (prefs->show_recent) {
    add_to_recent(file_name,0.,0,NULL);
  }

  if (cfile->frames>0) {
    cfile->start=1;
  }
  else {
    cfile->start=0;
  }
  cfile->end=cfile->frames;
  cfile->arps=cfile->arate;
  cfile->pb_fps=cfile->fps;
  cfile->opening=FALSE;
  cfile->proc_ptr=NULL;

  cfile->changed=FALSE;

  if (prefs->autoload_subs) {
    subfname=g_strdup_printf("%s/%s/subs.srt",prefs->tmpdir,cfile->handle);
    if (g_file_test(subfname,G_FILE_TEST_EXISTS)) {
      subtitles_init(cfile,subfname,SUBTITLE_TYPE_SRT);
    }
    else {
      g_free(subfname);
      subfname=g_strdup_printf("%s/%s/subs.sub",prefs->tmpdir,cfile->handle);
      if (g_file_test(subfname,G_FILE_TEST_EXISTS)) {
	subtitles_init(cfile,subfname,SUBTITLE_TYPE_SUB);
      }
    }
    g_free(subfname);
  }

  g_snprintf(cfile->type,40,"Frames");
  mesg1=g_strdup_printf(_ ("Frames=%d type=%s size=%dx%d bpp=%d fps=%.3f\nAudio:"),cfile->frames,cfile->type,cfile->hsize,cfile->vsize,cfile->bpp,cfile->fps);

  if (cfile->afilesize==0l) {
    cfile->achans=0;
    mesg=g_strdup_printf (_ ("%s none\n"),mesg1);
  }
  else {
    mesg=g_strdup_printf(P_ ("%s %d Hz %d channel %d bps\n","%s %d Hz %d channels %d bps\n",cfile->achans),
			 mesg1,cfile->arate,cfile->achans,cfile->asampsize);
  }
  d_print(mesg);
  g_free(mesg);
  g_free(mesg1);

  cfile->is_loaded=TRUE;
  current_file=mainw->current_file;

  // set new bpp
  cfile->bpp=(cfile->img_type==IMG_TYPE_JPEG)?24:32;

  if (!save_clip_values(current_file)) {
    close_current_file(old_file);
    return;
  }

  if (prefs->crash_recovery) add_to_recovery_file(cfile->handle);

  switch_to_file((mainw->current_file=old_file),current_file);

#ifdef ENABLE_OSC
    lives_osc_notify(LIVES_OSC_NOTIFY_CLIP_OPENED,"");
#endif

}


int save_event_frames(void) {
  // when doing a resample, we save a list of frames for the back end to do
  // a reorder

  // here we also update the frame_index for clips of type CLIP_TYPE_FILE

  gchar *hdrfile=g_strdup_printf("%s/%s/event.frames",prefs->tmpdir,cfile->handle);

  int header_fd,i=0;
  int retval;
  int perf_start,perf_end;
  int nevents;

  if (cfile->event_list==NULL) {
    unlink (hdrfile);
    return -1;
  }
  
  perf_start=(int)(cfile->fps*event_list_get_start_secs (cfile->event_list))+1;
  perf_end=perf_start+(nevents=count_events (cfile->event_list,FALSE,0,0))-1;
  
  event_list_to_block (cfile->event_list,nevents);

  if (cfile->frame_index!=NULL) {
    int xframes=cfile->frames;

    if (cfile->frame_index_back!=NULL) g_free(cfile->frame_index_back);
    cfile->frame_index_back=cfile->frame_index;
    cfile->frame_index=NULL;

    create_frame_index(mainw->current_file,FALSE,0,nevents);

    for (i=0;i<nevents;i++) {
      cfile->frame_index[i]=cfile->frame_index_back[(cfile->events[0]+i)->value-1];
    }

    cfile->frames=nevents;
    if (!check_if_non_virtual(mainw->current_file,1,cfile->frames)) save_frame_index(mainw->current_file);
    cfile->frames=xframes;
  }

  do {
    retval=0;
    header_fd=creat(hdrfile,S_IRUSR|S_IWUSR);
    if (header_fd<0) {
      retval=do_write_failed_error_s_with_retry(hdrfile,g_strerror(errno),NULL);
    }
    else {
      // use machine endian.
      // When we call "smogrify reorder", we will pass the endianness as 3rd parameter

      mainw->write_failed=FALSE;
      lives_write(header_fd,&perf_start,4,FALSE);
      
      if (!(cfile->events[0]==NULL)) {
	for (i=0;i<=perf_end-perf_start;i++) {
	  if (mainw->write_failed) break;
	  lives_write(header_fd,&((cfile->events[0]+i)->value),4,TRUE);
	}
	g_free (cfile->events[0]);
	cfile->events[0]=NULL;
      }

      if (mainw->write_failed) {
	retval=do_write_failed_error_s_with_retry(hdrfile,NULL,NULL);
      }

      close(header_fd);

    }
  } while (retval==LIVES_RETRY);


  if (retval==LIVES_CANCEL) {
    i=-1;
  }

  g_free(hdrfile);
  return i;
}



/////////////////////////////////////////////////
/// scrap file
///  the scrap file is used during recording to dump any streamed (non-disk) clips to
/// during render/preview we load frames from the scrap file, but only as necessary


/// ascrap file
/// this is used to record external audio during playback with record on (if the user requests this)
/// afterwards the audio from it can be rendered/played back


static double scrap_mb;  // MB written to frame file
static double ascrap_mb;  // MB written to audio file
static uint64_t free_mb; // MB free to write

boolean open_scrap_file (void) {
  // create a scrap file for recording generated video frames
  int current_file=mainw->current_file;
  int new_file=mainw->first_free_file;
  
  gchar *dir;
  gchar *scrap_handle;

  if (!get_temp_handle (new_file,TRUE)) return FALSE;
  get_next_free_file();

  mainw->scrap_file=mainw->current_file=new_file;

  g_snprintf(cfile->type,40,"scrap");
  cfile->frames=0;

  scrap_handle=g_strdup_printf("scrap|%s",cfile->handle);

  add_to_recovery_file(scrap_handle);

  mainw->cliplist = g_list_append (mainw->cliplist, GINT_TO_POINTER (mainw->current_file));

  g_free(scrap_handle);

  dir=g_build_filename(prefs->tmpdir,cfile->handle,NULL);
  free_mb=(double)get_fs_free(dir)/1000000.;
  g_free(dir);

  mainw->current_file=current_file;

  scrap_mb=0.;
  if (mainw->ascrap_file==-1) ascrap_mb=0.;

  return TRUE;
}



boolean open_ascrap_file (void) {
  // create a scrap file for recording audio
  int current_file=mainw->current_file;
  int new_file=mainw->first_free_file;
  
  gchar *dir;
  gchar *ascrap_handle;

  if (!get_temp_handle (new_file,TRUE)) return FALSE;
  get_next_free_file();

  mainw->ascrap_file=mainw->current_file=new_file;

  g_snprintf(cfile->type,40,"ascrap");

  cfile->frames=0;
  cfile->opening=FALSE;

  ascrap_handle=g_strdup_printf("ascrap|%s",cfile->handle);

  add_to_recovery_file(ascrap_handle);

  mainw->cliplist = g_list_append (mainw->cliplist, GINT_TO_POINTER (mainw->current_file));

  g_free(ascrap_handle);

  dir=g_build_filename(prefs->tmpdir,cfile->handle,NULL);
  free_mb=(double)get_fs_free(dir)/1000000.;
  g_free(dir);

  mainw->current_file=current_file;

  ascrap_mb=0.;
  if (mainw->scrap_file==-1) scrap_mb=0.;

  return TRUE;
}






boolean load_from_scrap_file(weed_plant_t *layer, int frame) {
  // load raw frame data from scrap file

  // this will also set cfile width and height - for letterboxing etc.

  // return FALSE if the frame does not exist/we are unable to read it


  int width,height,palette,nplanes;
  int clamping,subspace,sampling;
  int *rowstrides;

  int i,fd;

  gchar *oname=g_strdup_printf ("%s/%s/%08d.scrap",prefs->tmpdir,mainw->files[mainw->scrap_file]->handle,frame);


  ssize_t bytes;
  ssize_t tsize;

  void **pdata;

  fd=open(oname,O_RDONLY);

  if (fd==-1) return FALSE;

#ifdef IS_MINGW
  setmode(fd,O_BINARY);
#endif

  bytes=lives_read_le(fd,&palette,4,TRUE);
  if (bytes<sizint) return FALSE;

  weed_set_int_value(layer,"current_palette",palette);

  if (weed_palette_is_yuv_palette(palette)) {

    bytes=lives_read_le(fd,&clamping,4,TRUE);
    if (bytes<4) return FALSE;

    weed_set_int_value(layer,"YUV_clamping",clamping);

    bytes=lives_read_le(fd,&subspace,4,TRUE);
    if (bytes<4) return FALSE;
    
    weed_set_int_value(layer,"YUV_subspace",subspace);

    bytes=lives_read_le(fd,&sampling,4,TRUE);
    if (bytes<4) return FALSE;
    
    weed_set_int_value(layer,"YUV_sampling",sampling);
  }


  bytes=lives_read_le(fd,&width,4,TRUE);
  if (bytes<4) return FALSE;

  weed_set_int_value(layer,"width",width);


  bytes=lives_read_le(fd,&height,4,TRUE);
  if (bytes<4) return FALSE;

  weed_set_int_value(layer,"height",height);


  nplanes=weed_palette_get_numplanes(palette);

  rowstrides=(int *)g_malloc(nplanes*sizint);

  for (i=0;i<nplanes;i++) {
    bytes=lives_read_le(fd,&rowstrides[i],4,TRUE);
    if (bytes<4) {
      g_free(rowstrides);
      return FALSE;
    }
  }

  weed_set_int_array(layer,"rowstrides",nplanes,rowstrides);

  pdata=(void **)g_malloc(nplanes*sizeof(void *));

  for (i=0;i<nplanes;i++) {
    pdata[i]=NULL;
  }

  weed_set_voidptr_array(layer,"pixel_data",nplanes,pdata);

  for (i=0;i<nplanes;i++) {
    tsize=rowstrides[i]*height*weed_palette_get_plane_ratio_vertical(palette,i);
    pdata[i]=g_malloc(tsize);
    bytes=read(fd,pdata[i],tsize);
    if (bytes<tsize) {
      g_free(rowstrides);
      g_free(pdata);
      return FALSE;
    }
  }

  g_free(rowstrides);

  weed_set_voidptr_array(layer,"pixel_data",nplanes,pdata);
  g_free(pdata);


  close (fd);

  return TRUE;
}



int save_to_scrap_file (weed_plant_t *layer) {
  // returns frame number

  // dump the raw frame data to a file (in machine endian format)

  // format is:
  // (int)palette,[(int)YUV_clamping,(int)YUV_subspace,(int)YUV_sampling,](int)rowstrides[],(int)height,(char *)pixel_data[]

  // sampling, clamping and subspace are only written for YUV palettes

  // we also check if there is enough free space left; if not, recording is paused


  int fd;
  int flags=O_WRONLY|O_CREAT|O_TRUNC;
  
  int width,height,palette,nplanes,error;
  int *rowstrides;

  int clamping,subspace,sampling;

  int i;

  boolean wrtable=TRUE;

  void **pdata;

  gchar *oname=g_strdup_printf ("%s/%s/%08d.scrap",prefs->tmpdir,mainw->files[mainw->scrap_file]->handle,
				mainw->files[mainw->scrap_file]->frames+1);

  gchar *framecount;

  struct stat filestat;

#ifdef O_NOATIME
  flags|=O_NOATIME;
#endif

  fd=open(oname,flags,S_IRUSR|S_IWUSR);

  if (fd==-1) {
    g_free(oname);
    return mainw->files[mainw->scrap_file]->frames;
  }

#ifdef IS_MINGW
  setmode(fd,O_BINARY);
#endif

  mainw->write_failed=FALSE;

  // write current_palette, rowstrides and height
  palette=weed_get_int_value(layer,"current_palette",&error);
  lives_write_le(fd,&palette,4,TRUE);

  if (mainw->write_failed) {
    g_free(oname);
    return mainw->files[mainw->scrap_file]->frames;
  }

  if (weed_palette_is_yuv_palette(palette)) {
    if (weed_plant_has_leaf(layer,"YUV_clamping")) {
      clamping=weed_get_int_value(layer,"YUV_clamping",&error);
    }
    else clamping=WEED_YUV_CLAMPING_CLAMPED;
    lives_write_le(fd,&clamping,4,TRUE);

    if (weed_plant_has_leaf(layer,"YUV_subspace")) {
      subspace=weed_get_int_value(layer,"YUV_subspace",&error);
    }
    else subspace=WEED_YUV_SUBSPACE_YUV;
    lives_write_le(fd,&subspace,4,TRUE);

    if (weed_plant_has_leaf(layer,"YUV_sampling")) {
      sampling=weed_get_int_value(layer,"YUV_sampling",&error);
    }
    else sampling=WEED_YUV_SAMPLING_DEFAULT;
    lives_write_le(fd,&sampling,4,TRUE);
  }

  width=weed_get_int_value(layer,"width",&error);
  lives_write_le(fd,&width,4,TRUE);

  height=weed_get_int_value(layer,"height",&error);
  lives_write_le(fd,&height,4,TRUE);

  nplanes=weed_palette_get_numplanes(palette);
  
  rowstrides=weed_get_int_array(layer,"rowstrides",&error);
  
  for (i=0;i<nplanes;i++) {
    lives_write_le(fd,&rowstrides[i],4,TRUE);
  }
  
   
  // now write pixel_data planes
  
  pdata=weed_get_voidptr_array(layer,"pixel_data",&error);
  
  for (i=0;i<nplanes;i++) {
    lives_write(fd,pdata[i],rowstrides[i]*height*weed_palette_get_plane_ratio_vertical(palette,i),TRUE);
  }
  
  fstat(fd,&filestat);

  scrap_mb+=(double)(filestat.st_size)/1000000.;

  // check free space every 1000 frames or every 10 MB of audio (TODO ****)
  if (mainw->files[mainw->scrap_file]->frames%1000==0) {
    gchar *dir=g_build_filename(prefs->tmpdir,mainw->files[mainw->scrap_file]->handle,NULL);
    free_mb=(double)get_fs_free(dir)/1000000.;
    if (free_mb==0) wrtable=is_writeable_dir(dir);
    g_free(dir);
  }

  if ((!mainw->fs||prefs->play_monitor!=prefs->gui_monitor)&&prefs->show_framecount) {
    if ((scrap_mb+ascrap_mb)<(double)free_mb*.75) {
      // TRANSLATORS: rec(ord) %.2f M(ega)B(ytes)
      framecount=g_strdup_printf(_("rec %.2f MB"),scrap_mb+ascrap_mb);
    }
    else {
      // warn if scrap_file > 3/4 of free space
      // TRANSLATORS: !rec(ord) %.2f M(ega)B(ytes)
      if (wrtable)
	framecount=g_strdup_printf(_("!rec %.2f MB"),scrap_mb+ascrap_mb);
      else
	// TRANSLATORS: rec(ord) ?? M(ega)B(ytes)
	framecount=g_strdup(_("rec ?? MB"));
    }
    lives_entry_set_text(LIVES_ENTRY(mainw->framecounter),framecount);
    g_free(framecount);
  }

  lives_fsync(fd); // try to sync file access, to make saving smoother
  close(fd);

  weed_free(rowstrides);
  weed_free(pdata);

  g_free(oname);

  // check if we have enough free space left on the volume
  if ((int64_t)(((double)free_mb-(scrap_mb+ascrap_mb))/1000.)<prefs->rec_stop_gb) {
    // check free space again
    gchar *dir=g_build_filename(prefs->tmpdir,mainw->files[mainw->scrap_file]->handle,NULL);
    free_mb=(double)get_fs_free(dir)/1000000.;
    if (free_mb==0) wrtable=is_writeable_dir(dir);
    else wrtable=TRUE;

    if (wrtable) {
      if ((int64_t)(((double)free_mb-(scrap_mb+ascrap_mb))/1000.)<prefs->rec_stop_gb) {
	if (mainw->record&&!mainw->record_paused) {
	  gchar *msg=g_strdup_printf(_("\nRECORDING WAS PAUSED BECAUSE FREE DISK SPACE in %s IS BELOW %d GB !\nRecord stop level can be set in Preferences.\n"),dir,prefs->rec_stop_gb);
	  d_print(msg);
	  g_free(msg);
	  on_record_perf_activate(NULL,NULL);
	}
      }
      g_free(dir);
    }
  }
  return ++mainw->files[mainw->scrap_file]->frames;

}



void close_scrap_file (void) {
  int current_file=mainw->current_file;

  if (mainw->scrap_file==-1) return;

  mainw->current_file=mainw->scrap_file;
  close_current_file(current_file);

  mainw->cliplist=g_list_remove (mainw->cliplist, GINT_TO_POINTER (mainw->scrap_file));

  if (prefs->crash_recovery) rewrite_recovery_file();

  mainw->scrap_file=-1;
}



void close_ascrap_file (void) {
  int current_file=mainw->current_file;

  if (mainw->ascrap_file==-1) return;

  mainw->current_file=mainw->ascrap_file;
  close_current_file(current_file);

  mainw->cliplist=g_list_remove (mainw->cliplist, GINT_TO_POINTER (mainw->ascrap_file));

  if (prefs->crash_recovery) rewrite_recovery_file();

  mainw->ascrap_file=-1;
}



void recover_layout_map(int numclips) {
  // load global layout map for a set and assign entries to clips [mainw->files[i]->layout_map]
  GList *omlist,*mlist,*lmap_node,*lmap_node_next,*lmap_entry_list,*lmap_entry_list_next;

  layout_map *lmap_entry;

  gchar **array;
  gchar *check_handle;
  
  if (numclips>MAX_FILES) numclips=MAX_FILES;

  if ((omlist=load_layout_map())!=NULL) {
    int i;

    mlist=omlist;

    // assign layout map to clips
    for (i=1;i<=numclips;i++) {
      if (mainw->files[i]==NULL) continue;
      lmap_node=mlist;
      while (lmap_node!=NULL) {
	lmap_node_next=lmap_node->next;
	lmap_entry=(layout_map *)lmap_node->data;

	check_handle=g_strdup(mainw->files[i]->handle);

	if (strstr(lmap_entry->handle,"/")==NULL) {
	  g_free(check_handle);
	  check_handle=g_path_get_basename(mainw->files[i]->handle);
	}

	if (!strcmp(check_handle,lmap_entry->handle)&&(mainw->files[i]->unique_id==lmap_entry->unique_id)) {
	  // check handle and unique id match
	  // got a match, assign list to layout_map and delete this node
	  lmap_entry_list=lmap_entry->list;
	  while (lmap_entry_list!=NULL) {
	    lmap_entry_list_next=lmap_entry_list->next;
	    array=g_strsplit((gchar *)lmap_entry_list->data,"|",-1);
	    if (!g_file_test(array[0],G_FILE_TEST_EXISTS)) {
	      // layout file has been deleted, remove this entry
	      if (lmap_entry_list->prev!=NULL) lmap_entry_list->prev->next=lmap_entry_list_next;
	      else lmap_entry->list=lmap_node_next;
	      if (lmap_entry_list_next!=NULL) lmap_entry_list_next->prev=lmap_entry_list->prev;
	      g_free(lmap_entry_list->data);
	      //g_free(lmap_entry_list);    // i don't know why, but this causes a segfault
	    }
	    g_strfreev(array);
	    lmap_entry_list=lmap_entry_list_next;
	  }
	  mainw->files[i]->layout_map=lmap_entry->list;
	  g_free(lmap_entry->handle);
	  g_free(lmap_entry->name);
	  g_free(lmap_entry);
	  if (lmap_node->prev!=NULL) lmap_node->prev->next=lmap_node_next;
	  else mlist=lmap_node_next;
	  if (lmap_node_next!=NULL) lmap_node_next->prev=lmap_node->prev;
	  //g_free(lmap_node);   // i don't know why, but this causes a segfault
	}

	g_free(check_handle);
	lmap_node=lmap_node_next;
      }
    }
  
    lmap_node=mlist;
    while (lmap_node!=NULL) {
      lmap_entry=(layout_map *)lmap_node->data;
      if (lmap_entry->name!=NULL) g_free(lmap_entry->name);
      if (lmap_entry->handle!=NULL) g_free(lmap_entry->handle);
      if (lmap_entry->list!=NULL) {
	g_list_free_strings(lmap_entry->list);
	g_list_free(lmap_entry->list);
      }
      lmap_node=lmap_node->next;
    }
    if (omlist!=NULL) g_list_free(omlist);

  }
}






static boolean recover_files(gchar *recovery_file, boolean auto_recover) {
  FILE *rfile;
  gchar buff[256],*buffptr;
  gchar *clipdir;
  gchar *cwd=g_get_current_dir();

  int retval;
  int new_file,clipnum=0;
  int last_current_file=-1;

  boolean last_was_normal_file=FALSE;
  boolean is_scrap;
  boolean is_ascrap;
  boolean did_set_check=FALSE;
  boolean needs_update=FALSE;
  boolean is_ready;

  const lives_clip_data_t *cdata=NULL;

  splash_end();

  if (mainw->multitrack!=NULL) {
    if (mainw->multitrack->idlefunc>0) {
      g_source_remove(mainw->multitrack->idlefunc);
      mainw->multitrack->idlefunc=0;
    }
    mt_desensitise(mainw->multitrack);
  }

  if (!auto_recover) {
    if (!do_warning_dialog
	(_("\nFiles from a previous run of LiVES were found.\nDo you want to attempt to recover them ?\n"))) {
      unlink(recovery_file);

      if (mainw->multitrack!=NULL) {
	mt_sensitise(mainw->multitrack);
      }

      return FALSE;
    }
  }

  do {
    retval=0;
    rfile=fopen(recovery_file,"r");
    if (!rfile) {
      retval=do_read_failed_error_s_with_retry(recovery_file,g_strerror(errno),NULL);
      if (retval==LIVES_CANCEL) return FALSE;
    }
  } while (retval==LIVES_RETRY);

  
  mainw->is_ready=TRUE;
  do_threaded_dialog(_("Recovering files"),FALSE);
  mainw->is_ready=FALSE;

  d_print(_("Recovering files..."));
  threaded_dialog_spin();
  
  mainw->suppress_dprint=TRUE;
  
  while (1) {
    threaded_dialog_spin();
    is_scrap=FALSE;
    is_ascrap=FALSE;
    
    if (mainw->cached_list!=NULL) {
      g_list_free_strings(mainw->cached_list);
      g_list_free(mainw->cached_list);
      mainw->cached_list=NULL;
    }

    mainw->read_failed=FALSE;
    
    if (lives_fgets(buff,256,rfile)==NULL) {
      int current_file=mainw->current_file;
      if (last_was_normal_file&&mainw->multitrack==NULL) {
	if (current_file!=-1) switch_to_file((mainw->current_file=0),current_file);
      }
      reset_clip_menu();
      lives_widget_context_update();
      threaded_dialog_spin();

      if (mainw->read_failed) {
	do_read_failed_error_s(recovery_file,NULL);
      }
      break;
    }

    memset(buff+strlen(buff)-strlen("\n"),0,1);

    if (!strcmp(buff+strlen(buff)-1,"*")) {
      // set to be opened
      memset(buff+strlen(buff)-2,0,1);
      last_was_normal_file=FALSE;
      if (!is_legal_set_name(buff,TRUE)) continue;
      g_snprintf(mainw->set_name,128,"%s",buff);

      if (!on_load_set_ok(NULL,GINT_TO_POINTER(TRUE))) {
	fclose(rfile);
	end_threaded_dialog();

	if (strlen(mainw->set_name)>0) recover_layout_map(mainw->current_file);

	if (mainw->multitrack!=NULL) {
	  mainw->current_file=mainw->multitrack->render_file;
	  polymorph(mainw->multitrack,POLY_NONE);
	  polymorph(mainw->multitrack,POLY_CLIPS);
	  mt_sensitise(mainw->multitrack);
	}

	mainw->suppress_dprint=FALSE;
	d_print_failed();
	return TRUE;
      }
    }
    else {
      // load single file

      if (!strncmp(buff,"scrap|",6)) {
	is_scrap=TRUE;
	buffptr=buff+6;
      }
      else if (!strncmp(buff,"ascrap|",7)) {
	is_ascrap=TRUE;
	buffptr=buff+7;
      }
      else {
	buffptr=buff;
      }

      clipdir=g_build_filename(prefs->tmpdir,buffptr,NULL);

      if (!g_file_test(clipdir,G_FILE_TEST_IS_DIR)) {
	g_free(clipdir);
	continue;
      }
      g_free(clipdir);
      if ((new_file=mainw->first_free_file)==-1) {
	fclose(rfile);
	end_threaded_dialog();
	too_many_files();

	if (strlen(mainw->set_name)>0) recover_layout_map(mainw->current_file);

	if (mainw->multitrack!=NULL) {
	  mainw->current_file=mainw->multitrack->render_file;
	  polymorph(mainw->multitrack,POLY_NONE);
	  polymorph(mainw->multitrack,POLY_CLIPS);
	  mt_sensitise(mainw->multitrack);
	  mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
	}

	mainw->suppress_dprint=FALSE;
	d_print_failed();
	return TRUE;
      }
      // TODO - dirsep
      if (strstr(buffptr,"/clips/")) {
	gchar **array;
	threaded_dialog_spin();
	array=g_strsplit(buffptr,"/clips/",-1);
	mainw->was_set=TRUE;
	g_snprintf(mainw->set_name,128,"%s",array[0]);
	g_strfreev(array);

	if (!did_set_check&&!check_for_lock_file(mainw->set_name,0)) {
	  do_set_locked_warning(mainw->set_name);
	  did_set_check=TRUE;
	}

	threaded_dialog_spin();
      }
      last_was_normal_file=TRUE;
      mainw->current_file=new_file;
      threaded_dialog_spin();
      cfile=(file *)(g_malloc(sizeof(file)));
      g_snprintf(cfile->handle,256,"%s",buffptr);
      cfile->clip_type=CLIP_TYPE_DISK; // the default

      //create a new cfile and fill in the details
      create_cfile();
      threaded_dialog_spin();

      last_current_file=mainw->current_file;

      if (!is_scrap) {
	if (!is_ascrap) {
	  // get file details
	  read_headers(".");
	}
	else {
	  lives_clip_details_t detail;
	  int asigned,aendian;
	  detail=CLIP_DETAILS_ACHANS;
	  retval=get_clip_value(mainw->current_file,detail,&cfile->achans,0);
	  if (!retval) cfile->achans=0;
	  
	  if (cfile->achans==0) retval=FALSE;
	  else retval=TRUE;
	  
	  if (retval) {
	    detail=CLIP_DETAILS_ARATE;
	    retval=get_clip_value(mainw->current_file,detail,&cfile->arps,0);
	  }

	  if (!retval) cfile->arps=cfile->achans=cfile->arate=cfile->asampsize=0;
	  if (cfile->arps==0) retval=FALSE; 

	  cfile->arate=cfile->arps;
	  
	  if (retval) {
	    detail=CLIP_DETAILS_ASIGNED;
	    retval=get_clip_value(mainw->current_file,detail,&asigned,0);
	  }
	  if (retval) {
	    detail=CLIP_DETAILS_AENDIAN;
	    retval=get_clip_value(mainw->current_file,detail,&aendian,0);
	  }
	  
	  cfile->signed_endian=asigned+aendian;
	  
	  if (retval) {
	    detail=CLIP_DETAILS_ASAMPS;
	    retval=get_clip_value(mainw->current_file,detail,&cfile->asampsize,0);
	  }

	  if (!retval) {
	    mainw->first_free_file=mainw->current_file;
	    continue;
	  }
	  mainw->ascrap_file=mainw->current_file;
	}
      }
      else {
	mainw->scrap_file=mainw->current_file;
      }

      if (mainw->current_file<1) continue;

      if (load_frame_index(mainw->current_file)) {
	// cd to clip directory - so decoder plugins can write temp files
	gchar *ppath=g_build_filename(prefs->tmpdir,cfile->handle,NULL);
	boolean next=FALSE;

	lives_chdir(ppath,FALSE);
	g_free(ppath);

	while (1) {
	  threaded_dialog_spin();
	  if ((cdata=get_decoder_cdata(cfile,NULL))==NULL) {
	    next=FALSE;
	    if (mainw->error) {
	      if (do_original_lost_warning(cfile->file_name)) {
		
		// TODO ** - show layout errors
		next=TRUE;
	      }
	    }
	    else {
	      do_no_decoder_error(cfile->file_name);
	      next=TRUE;
	    }
	  }
	  threaded_dialog_spin();
	  break;
	}
	if (next) {
	  close_current_file(last_current_file);
	  continue;
	}
	cfile->clip_type=CLIP_TYPE_FILE;
	get_mime_type(cfile->type,40,cdata);
      }
    
      if (cfile->ext_src!=NULL) {
	//cdata=clone_cdata(mainw->current_file); // testing only
	//((lives_decoder_t *)cfile->ext_src)->cdata=cdata;
	check_clip_integrity(mainw->current_file,cdata);
      }
      else {
	if (is_scrap||!check_frame_count(mainw->current_file)) {
	  get_frame_count(mainw->current_file);
	  needs_update=TRUE;
	}
	if (!is_scrap&&cfile->frames>0&&(cfile->hsize*cfile->vsize==0)) {
	  get_frames_sizes(mainw->current_file,1);
	  needs_update=TRUE;
	}
	if (is_ascrap&&cfile->afilesize==0) reget_afilesize(mainw->current_file);
      }
  
      if (!is_scrap&&!is_ascrap) {
	// read the playback fps, play frame, and name
	threaded_dialog_spin();
	open_set_file (mainw->set_name,++clipnum);
	threaded_dialog_spin();
	
	if (mainw->cached_list!=NULL) {
	  threaded_dialog_spin();
	  g_list_free_strings(mainw->cached_list);
	  g_list_free(mainw->cached_list);
	  threaded_dialog_spin();
	  mainw->cached_list=NULL;
	}
	
	if (mainw->current_file<1) continue;
	
	get_total_time (cfile);
	if (cfile->achans) cfile->aseek_pos=(int64_t)((double)(cfile->frameno-1.)/cfile->fps*cfile->arate*
						   cfile->achans*(cfile->asampsize/8));

	if (needs_update) {
	  save_clip_values(mainw->current_file);
	  needs_update=FALSE;
	}
	
	// add to clip menu
	threaded_dialog_spin();
	add_to_winmenu();
	get_next_free_file();
	cfile->start=cfile->frames>0?1:0;
	cfile->end=cfile->frames;
	cfile->is_loaded=TRUE;
	cfile->changed=TRUE;
	unlink (cfile->info_file);
	set_main_title(cfile->name,0);
	
	if (mainw->multitrack!=NULL) {
	  int current_file=mainw->current_file;
	  lives_mt *multi=mainw->multitrack;
	  mainw->multitrack=NULL;
	  reget_afilesize (mainw->current_file);
	  mainw->multitrack=multi;
	  get_total_time(cfile);
	  mainw->current_file=mainw->multitrack->render_file;
	  mt_init_clips(mainw->multitrack,current_file,TRUE);
	  lives_widget_context_update();
	  mt_clip_select(mainw->multitrack,TRUE);
	  lives_widget_context_update();
	  mainw->current_file=current_file;
	}
	
	threaded_dialog_spin();
	
#ifdef ENABLE_OSC
	lives_osc_notify(LIVES_OSC_NOTIFY_CLIP_OPENED,"");
#endif
      }
      else {
	mainw->cliplist = g_list_append (mainw->cliplist, GINT_TO_POINTER (mainw->current_file));
	get_next_free_file();
      }
    }
  }

  lives_chdir(cwd,FALSE);
  g_free(cwd);

  end_threaded_dialog();

  fclose(rfile);

  if (mainw->current_file!=-1) 
    if (strlen(mainw->set_name)>0) recover_layout_map(mainw->current_file);

  if (mainw->multitrack!=NULL) {
    mainw->current_file=mainw->multitrack->render_file;
    polymorph(mainw->multitrack,POLY_NONE);
    polymorph(mainw->multitrack,POLY_CLIPS);
    mt_sensitise(mainw->multitrack);
    mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
  }
  else {
    int start_file=mainw->current_file;
    if (mainw->current_file>1&&mainw->current_file==mainw->ascrap_file&&mainw->files[mainw->current_file-1]!=NULL) {
      start_file--;
    }
    if (mainw->current_file>1&&mainw->current_file==mainw->scrap_file&&mainw->files[mainw->current_file-1]!=NULL) {
      start_file--;
    }
    if (mainw->current_file>1&&mainw->current_file==mainw->ascrap_file&&mainw->files[mainw->current_file-1]!=NULL) {
      start_file--;
    }
    if (start_file!=mainw->current_file) {
      switch_to_file(mainw->current_file,start_file);
    }
  }

  mainw->suppress_dprint=FALSE;
  d_print_done();
  is_ready=mainw->is_ready;
  mainw->is_ready=TRUE;
  lives_set_cursor_style(LIVES_CURSOR_NORMAL,NULL);
  mainw->is_ready=is_ready;
  return TRUE;
}





void add_to_recovery_file (const gchar *handle) {
#ifndef IS_MINGW
  gchar *com=g_strdup_printf("/bin/echo \"%s\" >> \"%s\"",handle,mainw->recovery_file);
#else
  gchar *com=g_strdup_printf("echo.exe \"%s\" >> \"%s\"",handle,mainw->recovery_file);
#endif
  mainw->com_failed=FALSE;
  lives_system(com,FALSE);
  g_free(com);

  if (mainw->com_failed) {
    mainw->com_failed=FALSE;
    return;
  }

  if ((mainw->multitrack!=NULL&&mainw->multitrack->event_list!=NULL)||mainw->stored_event_list!=NULL) 
    write_backup_layout_numbering(mainw->multitrack);
}



void rewrite_recovery_file(void) {
  // part of the crash recovery system
  GList *clist=mainw->cliplist;
  gchar *recovery_entry;

  boolean opened=FALSE;

  int recovery_fd=-1;
  int retval;

  register int i;

  if (clist==NULL) {
    unlink(mainw->recovery_file);
    return;
  }

  do {
    retval=0;
    mainw->write_failed=FALSE;
    opened=FALSE;
    recovery_fd=-1;

    while (clist!=NULL) {
      i=GPOINTER_TO_INT(clist->data);
      if (mainw->files[i]->clip_type==CLIP_TYPE_FILE||mainw->files[i]->clip_type==CLIP_TYPE_DISK) {
	if (i!=mainw->scrap_file) recovery_entry=g_strdup_printf("%s\n",mainw->files[i]->handle);
	else recovery_entry=g_strdup_printf("scrap|%s\n",mainw->files[i]->handle);

	if (!opened) recovery_fd=creat(mainw->recovery_file,S_IRUSR|S_IWUSR);
	if (recovery_fd<0) retval=do_write_failed_error_s_with_retry(mainw->recovery_file,g_strerror(errno),NULL);
	else {
	  opened=TRUE;
	  lives_write(recovery_fd,recovery_entry,strlen(recovery_entry),TRUE);
	  if (mainw->write_failed) retval=do_write_failed_error_s_with_retry(mainw->recovery_file,NULL,NULL);
	}
	g_free(recovery_entry);
      }
      if (mainw->write_failed) break;
      clist=clist->next;
    }

  } while (retval==LIVES_RETRY);

  if (!opened) unlink(mainw->recovery_file);
  else if (recovery_fd>=0) close(recovery_fd);

  if ((mainw->multitrack!=NULL&&mainw->multitrack->event_list!=NULL)||mainw->stored_event_list!=NULL) 
    write_backup_layout_numbering(mainw->multitrack);

}


boolean check_for_recovery_files (boolean auto_recover) {
  uint32_t recpid=0;

  ssize_t bytes;

  gchar *recovery_file,*recovery_numbering_file;
  gchar *info_file=g_strdup_printf("%s/.recovery.%d",prefs->tmpdir,getpid());
  gchar *com;

  boolean retval=FALSE;

  int info_fd;
  int lgid=lives_getgid();
  int luid=lives_getuid();

  pid_t lpid=lives_getpid();

  com=g_strdup_printf("%s get_recovery_file %d %d %s recovery> \"%s\"",prefs->backend_sync,luid,lgid,
			     capable->myname,info_file);
  
  unlink(info_file);
  mainw->com_failed=FALSE;
  lives_system(com,FALSE);
  g_free(com);

  if (mainw->com_failed) {
    mainw->com_failed=FALSE;
    return FALSE;
  }

  info_fd=open(info_file,O_RDONLY);
  if (info_fd>-1) {
    if ((bytes=read(info_fd,mainw->msg,256))>0) {
      memset(mainw->msg+bytes,0,1);
      if ((recpid=atoi(mainw->msg))>0) {
	
      }
    }
    close(info_fd);
  }
  unlink(info_file);
  g_free(info_file);

  if (recpid==0) return FALSE;
  
  retval=recover_files((recovery_file=g_strdup_printf("%s/recovery.%d.%d.%d",prefs->tmpdir,luid,
						      lgid,recpid)),auto_recover);
  unlink(recovery_file);
  g_free(recovery_file);
  
  mainw->com_failed=FALSE;

  // check for layout recovery file
  recovery_file=g_strdup_printf("%s/layout.%d.%d.%d",prefs->tmpdir,luid,lgid,recpid);
  if (g_file_test (recovery_file, G_FILE_TEST_EXISTS)) {
    // move files temporarily to stop them being cleansed
#ifndef IS_MINGW
    com=g_strdup_printf("/bin/mv \"%s\" \"%s/.layout.%d.%d.%d\"",recovery_file,prefs->tmpdir,luid,
			lgid,lpid);
#else
    com=g_strdup_printf("mv.exe \"%s\" \"%s/.layout.%d.%d.%d\"",recovery_file,prefs->tmpdir,luid,
			lgid,lpid);
#endif
    lives_system(com,FALSE);
    g_free(com);
    recovery_numbering_file=g_strdup_printf("%s/layout_numbering.%d.%d.%d",prefs->tmpdir,luid,
					    lgid,recpid);
#ifndef IS_MINGW
    com=g_strdup_printf("/bin/mv \"%s\" \"%s/.layout_numbering.%d.%d.%d\"",recovery_numbering_file,prefs->tmpdir,
			luid,lgid,lpid);
#else
    com=g_strdup_printf("mv.exe \"%s\" \"%s/.layout_numbering.%d.%d.%d\"",recovery_numbering_file,prefs->tmpdir,
			luid,lgid,lpid);
#endif
    lives_system(com,FALSE);
    g_free(com);
    g_free(recovery_numbering_file);
    mainw->recoverable_layout=TRUE;
  }
  else {
    if (mainw->scrap_file!=-1) close_scrap_file();
    if (mainw->ascrap_file!=-1) close_ascrap_file();
  }
  g_free(recovery_file);
  
  if (mainw->com_failed) return FALSE;

  com=g_strdup_printf("%s clean_recovery_files %d %d \"%s\"",prefs->backend_sync,luid,lgid,capable->myname);
  lives_system(com,FALSE);
  g_free(com);

  if (mainw->recoverable_layout) {
    recovery_file=g_strdup_printf("%s/.layout.%d.%d.%d",prefs->tmpdir,luid,lgid,lpid);
#ifndef IS_MINGW
    com=g_strdup_printf("/bin/mv \"%s\" \"%s/layout.%d.%d.%d\"",recovery_file,prefs->tmpdir,luid,
			lgid,lpid);
#else
    com=g_strdup_printf("mv.exe \"%s\" \"%s/layout.%d.%d.%d\"",recovery_file,prefs->tmpdir,luid,
			lgid,lpid);
#endif
    lives_system(com,FALSE);
    g_free(com);
    g_free(recovery_file);

    recovery_numbering_file=g_strdup_printf("%s/.layout_numbering.%d.%d.%d",prefs->tmpdir,luid,lgid,lpid);
#ifndef IS_MINGW
    com=g_strdup_printf("/bin/mv \"%s\" \"%s/layout_numbering.%d.%d.%d\"",recovery_numbering_file,prefs->tmpdir,luid,
			lgid,lpid);
#else
    com=g_strdup_printf("mv.exe \"%s\" \"%s/layout_numbering.%d.%d.%d\"",recovery_numbering_file,prefs->tmpdir,luid,
			lgid,lpid);
#endif
    lives_system(com,FALSE);
    g_free(com);
    g_free(recovery_numbering_file);
  }

  rewrite_recovery_file();

  if (!mainw->recoverable_layout) do_after_crash_warning();

  return retval;
}

