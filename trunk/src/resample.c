// resample.c
// LiVES
// (c) G. Finch 2004 - 2008 <salsaman@xs4all.nl>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details


// functions for reordering, resampling video and audio

#include "../libweed/weed.h"
#include "../libweed/weed-host.h"

#include "main.h"
#include "resample.h"
#include "support.h"
#include "callbacks.h"
#include "effects.h"
#include "audio.h"
#include "cvirtual.h"

static gint reorder_width=0;
static gint reorder_height=0;
static gboolean reorder_leave_back=FALSE;

/////////////////////////////////////////////////////



inline weed_timecode_t q_gint64 (weed_timecode_t in, gdouble fps) {
  if (in>0) return ((weed_timecode_t)((long double)in/(long double)U_SEC*(long double)fps+(long double).5)/(long double)fps)*(weed_timecode_t)U_SECL; // quantise to frame timing
  if (in<0) return ((weed_timecode_t)((long double)in/(long double)U_SEC*(long double)fps-(long double).5)/(long double)fps)*(weed_timecode_t)U_SECL; // quantise to frame timing
  return (weed_timecode_t)0;
}

inline weed_timecode_t q_dbl (gdouble in, gdouble fps) {
  if (in>0) return ((weed_timecode_t)((long double)in*(long double)fps+(long double).5)/(long double)fps)*(weed_timecode_t)U_SECL; // quantise to frame timing
  if (in<0) return ((weed_timecode_t)((long double)in*(long double)fps-(long double).5)/(long double)fps)*(weed_timecode_t)U_SECL; // quantise to frame timing
  return (weed_timecode_t)0;
}

inline gint count_resampled_frames (gint in_frames, gdouble orig_fps, gdouble resampled_fps) {
  gint res_frames;
  return ((res_frames=(gint)((gdouble)in_frames/orig_fps*resampled_fps+.49999))<1)?1:res_frames;
}

/////////////////////////////////////////////////////

gboolean auto_resample_resize (gint width,gint height,gdouble fps,gint fps_num,gint fps_denom, gint arate) {
  // do a block atomic: resample audio, then resample video/resize or joint resample/resize

  gchar *com,*msg=NULL;
  gint current_file=mainw->current_file;
  gboolean audio_resampled=FALSE;
  gboolean video_resampled=FALSE;
  gboolean video_resized=FALSE;
  gint frames=cfile->frames;

  reorder_leave_back=FALSE;

  if (arate>0&&arate!=cfile->arate) {
    cfile->undo1_int=arate;
    cfile->undo2_int=cfile->achans;
    cfile->undo3_int=cfile->asampsize;
    cfile->undo1_uint=cfile->signed_endian;
    on_resaudio_ok_clicked (NULL,NULL);
    if (mainw->error) return FALSE;
    audio_resampled=TRUE;
  }

  if (fps_denom>0) {
    fps=(fps_num*1.)/(fps_denom*1.);
  }
  if (fps>0.&&fps!=cfile->fps) {
    // FPS CHANGE...
    if ((width!=cfile->hsize||height!=cfile->vsize)&&width*height>0) {
      // CHANGING SIZE..
      if (fps>cfile->fps) {
	gboolean rs_builtin;
	lives_rfx_t *resize_rfx;

	// we will have more frames...
	// ...do resize first
	if (mainw->fx_candidates[FX_CANDIDATE_RESIZER].delegate==-1) {
	  cfile->ohsize=cfile->hsize;
	  cfile->ovsize=cfile->vsize;

	  if (cfile->clip_type==CLIP_TYPE_FILE) {
	    cfile->fx_frame_pump=1;
	  }

	  com=g_strdup_printf ("smogrify resize_all %s %d %d %d",cfile->handle,cfile->frames,width,height);
	  
	  cfile->progress_start=1;
	  cfile->progress_end=cfile->frames;
	  
	  unlink(cfile->info_file);
	  dummyvar=system(com);
	  g_free (com);
	  
	  msg=g_strdup_printf(_("Resizing frames 1 to %d"),cfile->frames);
	  
	  mainw->resizing=TRUE;
	  rs_builtin=TRUE;
	}
	else {
	  int error;
	  weed_plant_t *first_out;
	  weed_plant_t *ctmpl;
	  
	  rs_builtin=FALSE;
	  resize_rfx=mainw->fx_candidates[FX_CANDIDATE_RESIZER].rfx;
	  first_out=get_enabled_channel(resize_rfx->source,0,FALSE);
	  ctmpl=weed_get_plantptr_value(first_out,"template",&error);
	  weed_set_int_value(ctmpl,"host_width",width);
	  weed_set_int_value(ctmpl,"host_height",height);
	}
	
	if ((rs_builtin&&!do_progress_dialog(TRUE,TRUE,msg))||(!rs_builtin&&!on_realfx_activate_inner(1,resize_rfx))) {
	  mainw->resizing=FALSE;
	  g_free(msg);
	  cfile->undo_action=UNDO_ATOMIC_RESAMPLE_RESIZE;
	  if (!audio_resampled) {
	    cfile->undo1_int=cfile->arate;
	    cfile->undo2_int=cfile->achans;
	    cfile->undo3_int=cfile->asampsize;
	    cfile->undo4_int=cfile->arps;
	    cfile->undo1_uint=cfile->signed_endian;
	  }
	  cfile->hsize=width;
	  cfile->vsize=height;
	  
	  cfile->undo1_dbl=cfile->fps;
	  cfile->undo_start=1;
	  cfile->undo_end=cfile->frames;
	  cfile->fx_frame_pump=0;
	  on_undo_activate(NULL,NULL);
	  return FALSE;
	}
	g_free(msg);
	    
	mainw->resizing=FALSE;
	cfile->fx_frame_pump=0;

	cfile->hsize=width;
	cfile->vsize=height;
	
	save_clip_value(mainw->current_file,CLIP_DETAILS_WIDTH,&cfile->hsize);
	save_clip_value(mainw->current_file,CLIP_DETAILS_HEIGHT,&cfile->vsize);

	cfile->undo1_dbl=fps;
	cfile->undo_start=1;
	cfile->undo_end=frames;

	// now resample

	// special "cheat" mode for LiVES
	reorder_leave_back=TRUE;
	    
	on_resample_vid_ok (NULL,NULL);

	reorder_leave_back=FALSE;
	cfile->undo_action=UNDO_ATOMIC_RESAMPLE_RESIZE;
	if (mainw->error) {
	  on_undo_activate(NULL,NULL);
	  return FALSE;
	}

	video_resized=TRUE;
	video_resampled=TRUE;
      }
      else {
	// fewer frames
	// do resample *with* resize
	cfile->ohsize=cfile->hsize;
	cfile->ovsize=cfile->vsize;
	cfile->undo1_dbl=fps;

	// another special "cheat" mode for LiVES
	reorder_width=width;
	reorder_height=height;

	mainw->resizing=TRUE;
	on_resample_vid_ok (NULL,NULL);
	mainw->resizing=FALSE;

	reorder_width=reorder_height=0;
	cfile->undo_action=UNDO_ATOMIC_RESAMPLE_RESIZE;
	cfile->hsize=width;
	cfile->vsize=height;

	if (mainw->error) {
	  on_undo_activate(NULL,NULL);
	  return FALSE;
	}
	save_clip_value(mainw->current_file,CLIP_DETAILS_WIDTH,&cfile->hsize);
	save_clip_value(mainw->current_file,CLIP_DETAILS_HEIGHT,&cfile->vsize);

       	video_resampled=TRUE;
	video_resized=TRUE;
      }
    }
    else {
      //////////////////////////////////////////////////////////////////////////////////
      cfile->undo1_dbl=fps;
      cfile->undo_start=1;
      cfile->undo_end=cfile->frames;
      
      on_resample_vid_ok (NULL,NULL);
      
      if (audio_resampled) cfile->undo_action=UNDO_ATOMIC_RESAMPLE_RESIZE;
      if (mainw->error) {
	on_undo_activate(NULL,NULL);
	return FALSE;
      }
      //////////////////////////////////////////////////////////////////////////////////////
      video_resampled=TRUE;
    }
  }
  else {
    gboolean rs_builtin=TRUE;
    lives_rfx_t *resize_rfx;
    // NO FPS CHANGE
    if ((width!=cfile->hsize||height!=cfile->vsize)&&width*height>0) {
      // no fps change - just a normal resize
      cfile->undo_start=1;
      cfile->undo_end=cfile->frames;

      if (mainw->fx_candidates[FX_CANDIDATE_RESIZER].delegate==-1) {
	// use builtin resize

	if (cfile->clip_type==CLIP_TYPE_FILE) {
	  cfile->fx_frame_pump=1;
	}

	com=g_strdup_printf ("smogrify resize_all %s %d %d %d",cfile->handle,cfile->frames,width,height);
	
	cfile->progress_start=1;
	cfile->progress_end=cfile->frames;
	
	cfile->ohsize=cfile->hsize;
	cfile->ovsize=cfile->vsize;
	
	cfile->undo1_dbl=cfile->fps;
	
	unlink(cfile->info_file);
	dummyvar=system(com);
	g_free (com);
	
	msg=g_strdup_printf(_("Resizing frames 1 to %d"),cfile->frames);
	
	mainw->resizing=TRUE;
      }
      else {
	int error;
	weed_plant_t *first_out;
	weed_plant_t *ctmpl;
	
	rs_builtin=FALSE;
	resize_rfx=mainw->fx_candidates[FX_CANDIDATE_RESIZER].rfx;
	first_out=get_enabled_channel(resize_rfx->source,0,FALSE);
	ctmpl=weed_get_plantptr_value(first_out,"template",&error);
	weed_set_int_value(ctmpl,"host_width",width);
	weed_set_int_value(ctmpl,"host_height",height);
      }

      if ((rs_builtin&&!do_progress_dialog(TRUE,TRUE,msg))||(!rs_builtin&&!on_realfx_activate_inner(1,resize_rfx))) {
	mainw->resizing=FALSE;
	if (msg!=NULL) g_free(msg);
	cfile->hsize=width;
	cfile->vsize=height;
	if (audio_resampled) cfile->undo_action=UNDO_ATOMIC_RESAMPLE_RESIZE;
	else {
	  cfile->undo_action=UNDO_RESIZABLE;
	  set_undoable (_ ("Resize"),TRUE);
	}
	cfile->fx_frame_pump=0;
	on_undo_activate (NULL,NULL);
	return FALSE;
      }

      cfile->hsize=width;
      cfile->vsize=height;

      g_free(msg);
      mainw->resizing=FALSE;
      cfile->fx_frame_pump=0;

      save_clip_value(mainw->current_file,CLIP_DETAILS_WIDTH,&cfile->hsize);
      save_clip_value(mainw->current_file,CLIP_DETAILS_HEIGHT,&cfile->vsize);

      if (audio_resampled) cfile->undo_action=UNDO_ATOMIC_RESAMPLE_RESIZE;
      else {
	cfile->undo_action=UNDO_RESIZABLE;
	set_undoable (_ ("Resize"),TRUE);
      }
      video_resized=TRUE;
      switch_to_file ((mainw->current_file=0),current_file);
    }
  }

  if (cfile->undo_action==UNDO_ATOMIC_RESAMPLE_RESIZE) {
    // just in case we missed anything...
    
    set_undoable (_ ("Resample/Resize"),TRUE);
    if (!audio_resampled) {
      cfile->undo1_int=cfile->arate;
      cfile->undo2_int=cfile->achans;
      cfile->undo3_int=cfile->asampsize;
      cfile->undo4_int=cfile->arps;
      cfile->undo1_uint=cfile->signed_endian;
    }
    if (!video_resized) {
      cfile->ohsize=cfile->hsize;
      cfile->ovsize=cfile->vsize;
    }
    if (!video_resampled) {
      cfile->undo1_dbl=cfile->fps;
    }
    cfile->undo_start=1;
    cfile->undo_end=frames;
  }
  return TRUE;
}


//////////////////////////////////////////////////////////////////

weed_plant_t *
quantise_events (weed_plant_t *in_list, gdouble qfps, gboolean allow_gap) { 
  // new style event system, now we quantise from event_list_t *in_list to *out_list with period tl/U_SEC

  // the timecode of the midpoint of our last frame events will match as near as possible the old length
  // but out_list will have regular period of tl microseconds

  // for optimal resampling we compare the midpoints of each frame

  // only FRAME events are moved, other event types retain the same timecodes

  weed_plant_t *out_list;
  weed_plant_t *event,*last_frame_event,*penultimate_frame_event,*next_frame_event,*shortcut=NULL;
  weed_timecode_t out_tc=0,in_tc=-1,nearest_tc=LONG_MAX;
  gboolean is_first=TRUE;
  weed_timecode_t tc_end,tp;
  int *out_clips=NULL,*out_frames=NULL;
  int numframes=0;
  weed_timecode_t tl=q_dbl(1./qfps,qfps);
  int error;
  gboolean needs_audio=FALSE;
  int *aclips=NULL;
  double *aseeks=NULL;
  int num_aclips=0;

  if (in_list==NULL) return NULL;

  out_list=weed_plant_new(WEED_PLANT_EVENT_LIST);
  weed_add_plant_flags(out_list,WEED_LEAF_READONLY_PLUGIN);
  weed_set_voidptr_value(out_list,"first",NULL);
  weed_set_voidptr_value(out_list,"last",NULL);
  weed_set_double_value(out_list,"fps",qfps);

  last_frame_event=get_last_frame_event(in_list);

  tc_end=get_event_timecode (last_frame_event);

  penultimate_frame_event=get_prev_frame_event(last_frame_event);

  // tp is the duration of the last frame
  if (penultimate_frame_event!=NULL) {
    tp=get_event_timecode (penultimate_frame_event);
    tp=tc_end-tp;
  }
  else {
    // only one event, use cfile->fps
    tp=(weed_timecode_t)(U_SEC/cfile->fps);
  }

  tc_end+=tp;

  event=get_first_event(in_list);

#ifdef RESAMPLE_USE_MIDPOINTS
  // our "slot" is the middle of the out frame
  tl/=2;
#endif

  // length of old clip is assumed as last timecode + (last timecode - previous timecode)
  // we fill out frames until the new slot>=old length


  while ((out_tc+tl)<=tc_end) {
    // walk list of in events

    while (event!=NULL&&get_event_hint(event)!=WEED_EVENT_HINT_FRAME) {
      // copy non-FRAME events
      if (event_copy_and_insert (event,out_list)==NULL) {
	do_memory_error_dialog();
	event_list_free(out_list);
	return NULL;
      }
      is_first=FALSE;
      event=get_next_event(event);
    }

    // now we are dealing with a FRAME event
    if (event!=NULL) {
      if (weed_plant_has_leaf(event,"audio_clips")) {
	needs_audio=TRUE;
	if (aclips!=NULL) weed_free(aclips);
	if (aseeks!=NULL) weed_free(aseeks);
	num_aclips=weed_leaf_num_elements(event,"audio_clips");
	aclips=weed_get_int_array(event,"audio_clips",&error);
	aseeks=weed_get_double_array(event,"audio_seeks",&error);
	if (error==WEED_ERROR_MEMORY_ALLOCATION) {
	  do_memory_error_dialog();
	  event_list_free(out_list);
	  return NULL;
	}
      }
      in_tc=get_event_timecode (event);
      if ((next_frame_event=get_next_frame_event(event))!=NULL) {
	tp=get_event_timecode (next_frame_event);
	tp-=in_tc;
      }
      else {
	// only one event, use cfile->fps
	tp=(weed_timecode_t)(U_SEC/cfile->fps);
      }
#ifdef RESAMPLE_USE_MIDPOINTS
      // calc mid-point of in frame
      in_tc+=tp/2;
#else
      in_tc+=tp;
#endif
    }

    if (in_tc<=(out_tc+tl)&&event!=NULL) {
      // event is before slot, note it and get next event
      if (out_clips!=NULL) weed_free(out_clips);
      if (out_frames!=NULL) weed_free(out_frames);

      numframes=weed_leaf_num_elements(event,"clips");
      out_clips=weed_get_int_array(event,"clips",&error);
      out_frames=weed_get_int_array(event,"frames",&error);
      if (error==WEED_ERROR_MEMORY_ALLOCATION) {
	do_memory_error_dialog();
	event_list_free(out_list);
	return NULL;
      }

      nearest_tc=(out_tc+tl)-in_tc;
      if (event!=NULL) event=get_next_event(event);
      allow_gap=FALSE;
    }   
    else {
      // event is after slot, or we reached the end of in_list
      
      //  in some cases we allow a gap before writing our first FRAME out event
      if (!(is_first&&allow_gap)) {
	if (in_tc-(out_tc+tl)<nearest_tc) {
	  if (event!=NULL) {
	    if (out_clips!=NULL) weed_free(out_clips);
	    if (out_frames!=NULL) weed_free(out_frames);

	    numframes=weed_leaf_num_elements(event,"clips");
	    out_clips=weed_get_int_array(event,"clips",&error);
	    out_frames=weed_get_int_array(event,"frames",&error);
	    if (error==WEED_ERROR_MEMORY_ALLOCATION) {
	      do_memory_error_dialog();
	      event_list_free(out_list);
	      return NULL;
	    }
	  }
	}
	if (out_clips!=NULL) {
	  if (insert_frame_event_at (out_list,out_tc,numframes,out_clips,out_frames,&shortcut)==NULL) {
	    do_memory_error_dialog();
	    event_list_free(out_list);
	    return NULL;
	  }
	  if (needs_audio) {
	    weed_set_int_array(shortcut,"audio_clips",num_aclips,aclips);
	    weed_set_double_array(shortcut,"audio_seeks",num_aclips,aseeks);
	    needs_audio=FALSE;
	  }
	  nearest_tc=LONG_MAX;
	  if (is_first) {
	    weed_set_voidptr_value(out_list,"first",get_last_event(out_list));
	    is_first=FALSE;
	  }
	}
      }
#ifdef RESAMPLE_USE_MIDPOINTS
      out_tc+=tl*2;
#else
      out_tc+=tl;
#endif
      out_tc=q_gint64(out_tc,qfps);
    }
  }

  if (event!=NULL&&get_event_hint(event)==WEED_EVENT_HINT_FRAME) event=get_next_event(event);

  while (event!=NULL&&get_event_hint(event)!=WEED_EVENT_HINT_FRAME) {
    // copy remaining non-FRAME events
    if (event_copy_and_insert (event,out_list)==NULL) {
      do_memory_error_dialog();
      event_list_free(out_list);
      return NULL;
    }
    event=get_next_event(event);
  }
  
  if (get_first_frame_event(out_list)==NULL) {
    // make sure we have at least one frame
    if ((event=get_last_frame_event(in_list))!=NULL) {

      if (out_clips!=NULL) weed_free(out_clips);
      if (out_frames!=NULL) weed_free(out_frames);
    
      numframes=weed_leaf_num_elements(event,"clips");
      out_clips=weed_get_int_array(event,"clips",&error);
      out_frames=weed_get_int_array(event,"frames",&error);

      if (insert_frame_event_at (out_list,0.,numframes,out_clips,out_frames,NULL)==NULL) {
	do_memory_error_dialog();
	event_list_free(out_list);
	return NULL;
      }
      if (get_first_event(out_list)==NULL) weed_set_voidptr_value(out_list,"first",get_last_event(out_list));
    }
  }

  if (out_clips!=NULL) weed_free(out_clips);
  if (out_frames!=NULL) weed_free(out_frames);

  if (aclips!=NULL) weed_free(aclips);
  if (aseeks!=NULL) weed_free(aseeks);

  return out_list;
}


//////////////////////////////////////////////////////////////////


static void on_reorder_activate (void) {
  gchar *msg;
  gboolean has_lmap_error=FALSE;

  if (!(prefs->warning_mask&WARN_MASK_LAYOUT_ALTER_FRAMES)&&cfile->layout_map!=NULL&&layout_frame_is_affected(mainw->current_file,1)) {
    if (!do_layout_alter_frames_warning()) {
      return;
    }
    add_lmap_error(LMAP_ERROR_ALTER_FRAMES,cfile->name,(gpointer)cfile->layout_map,mainw->current_file,0,0.);
    has_lmap_error=TRUE;
  }

  cfile->old_frames=cfile->frames;

  //  we  do the reorder in reorder_frames()
  // this will clear event_list and set it in event_list_back
  if ((cfile->frames=reorder_frames())<0) {
    // reordering error
    if (!(cfile->undo_action==UNDO_RESAMPLE)) {
     cfile->frames=-cfile->frames;
    }
    return;
  }

  if (mainw->cancelled!=CANCEL_NONE) {
    return;
  }

  if (cfile->start>cfile->frames) {
    cfile->start=cfile->frames;
  }

  if (cfile->end>cfile->frames) {
    cfile->end=cfile->frames;
  }

  cfile->event_list=NULL;
  cfile->next_event=NULL;
  
  save_clip_value(mainw->current_file,CLIP_DETAILS_FRAMES,&cfile->frames);

  switch_to_file(mainw->current_file,mainw->current_file);
  if (mainw->current_file>0) {
    d_print_done();
    msg=g_strdup_printf(_ ("Length of video is now %d frames.\n"),cfile->frames);
  }
  else {
    msg=g_strdup_printf(_ ("Clipboard was resampled to %d frames.\n"),cfile->frames);
  }

  d_print(msg);
  g_free(msg);

  if (has_lmap_error) popup_lmap_errors(NULL,NULL);


}




void
on_resample_audio_activate (GtkMenuItem     *menuitem,
			    gpointer         user_data)
{
   // show the playback rate - real audio rate is cfile->arps
  mainw->fx1_val=cfile->arate;
  mainw->fx2_val=cfile->achans;
  mainw->fx3_val=cfile->asampsize;
  mainw->fx4_val=cfile->signed_endian;
  resaudw=create_resaudw(1,NULL,NULL);
  gtk_widget_show (resaudw->dialog);

}

void
on_resaudio_ok_clicked                      (GtkButton *button,
					     GtkEntry *entry)
{
  gchar *com,*msg;  
  gint arate,achans,asampsize,arps;
  int asigned=1,aendian=1;
  gint cur_signed,cur_endian;
  gboolean noswitch=mainw->noswitch;
  gboolean has_lmap_error=FALSE;

  if (button!=NULL) {
    arps=arate=(gint)atoi (gtk_entry_get_text(GTK_ENTRY(resaudw->entry_arate)));
    achans=(gint)atoi (gtk_entry_get_text(GTK_ENTRY(resaudw->entry_achans)));
    asampsize=(gint)atoi (gtk_entry_get_text(GTK_ENTRY(resaudw->entry_asamps)));
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(resaudw->rb_unsigned))) {
      asigned=0;
    }
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(resaudw->rb_bigend))) {
      aendian=0;
    }

    gtk_widget_destroy (resaudw->dialog);
    mainw->noswitch=TRUE;
    while (g_main_context_iteration(NULL,FALSE));
    mainw->noswitch=noswitch;
    g_free (resaudw);
    
    if (arate<=0) {
      do_error_dialog (_ ("\n\nNew rate must be greater than 0\n"));
      return;
    }
  }
  else {
    // called from on_redo or other places
    arate=arps=cfile->undo1_int;
    achans=cfile->undo2_int;
    asampsize=cfile->undo3_int;
    asigned=!(cfile->undo1_uint&AFORM_UNSIGNED);
    aendian=!(cfile->undo1_uint&AFORM_BIG_ENDIAN);
  }

  if (!(prefs->warning_mask&WARN_MASK_LAYOUT_ALTER_AUDIO)&&cfile->layout_map!=NULL&&layout_audio_is_affected(mainw->current_file,0.)) {
    if (!do_layout_alter_audio_warning()) {
      return;
    }
    add_lmap_error(LMAP_ERROR_ALTER_AUDIO,cfile->name,(gpointer)cfile->layout_map,mainw->current_file,0,0.);
    has_lmap_error=TRUE;
  }

  // store old values for undo/redo
  cfile->undo1_int=cfile->arate;
  cfile->undo2_int=cfile->achans;
  cfile->undo3_int=cfile->asampsize;
  cfile->undo4_int=cfile->arps;
  cfile->undo1_uint=cfile->signed_endian;

  cur_signed=!(cfile->signed_endian&AFORM_UNSIGNED);
  cur_endian=!(cfile->signed_endian&AFORM_BIG_ENDIAN);

  if (!(arate==cfile->arate&&arps==cfile->arps&&achans==cfile->achans&&asampsize==cfile->asampsize&&asigned==cur_signed&&aendian==cur_endian)) {
    if (cfile->arps!=cfile->arate) {
      gdouble audio_stretch=(gdouble)cfile->arps/(gdouble)cfile->arate;
     // pb rate != real rate - stretch to pb rate and resample 
      unlink (cfile->info_file);
      com=g_strdup_printf ("smogrify resample_audio %s %d %d %d %d %d %d %d %d %d %d %.4f",cfile->handle,cfile->arps,cfile->achans,cfile->asampsize,cur_signed,cur_endian,arps,cfile->achans,cfile->asampsize,cur_signed,cur_endian,audio_stretch);
      dummyvar=system (com);
      do_progress_dialog (TRUE,FALSE,_ ("Resampling audio")); // TODO - allow cancel ??
      g_free (com);
      cfile->arate=cfile->arps=arps;
    }
    else {
      unlink (cfile->info_file);
      com=g_strdup_printf ("smogrify resample_audio %s %d %d %d %d %d %d %d %d %d %d",cfile->handle,cfile->arps,cfile->achans,cfile->asampsize,cur_signed,cur_endian,arps,achans,asampsize,asigned,aendian);
      dummyvar=system (com);
      do_progress_dialog (TRUE,FALSE,_ ("Resampling audio"));
      g_free (com);
    }
  }

  cfile->arate=arate;
  cfile->achans=achans;
  cfile->asampsize=asampsize;
  cfile->arps=arps;
  cfile->signed_endian=get_signed_endian (asigned, aendian);
  cfile->changed=TRUE;

  cfile->undo_action=UNDO_AUDIO_RESAMPLE;
  mainw->error=FALSE;
  reget_afilesize(mainw->current_file);

  if (cfile->afilesize==0l) {
    do_error_dialog (_ ("LiVES was unable to resample the audio as requested.\n"));
    on_undo_activate (NULL,NULL);
    set_undoable (_("Resample Audio"),FALSE);
    mainw->error=TRUE;
    return;
  }
  set_undoable (_("Resample Audio"),!prefs->conserve_space);

  save_clip_values(mainw->current_file);

  switch_to_file(mainw->current_file,mainw->current_file);

  msg=g_strdup_printf (_ ("Audio was resampled to %d Hz, %d channels, %d bit"),arate,achans,asampsize);
  d_print (msg);
  g_free (msg);
  if (cur_signed!=asigned) {
    if (asigned==1) {
      d_print (_ (", signed"));
    }
    else {
      d_print (_ (", unsigned"));
    }
  }
  if (cur_endian!=aendian) {
    if (aendian==1) {
      d_print (_ (", little-endian"));
    }
    else {
      d_print (_ (", big-endian"));
    }
  }
  d_print ("\n");
  if (has_lmap_error) popup_lmap_errors(NULL,NULL);
}




static void on_resaudw_achans_changed (GtkWidget *widg, gpointer user_data) {
  _resaudw *resaudw=(_resaudw *)user_data;
  gchar *tmp;

  if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widg))) {
    gtk_widget_set_sensitive (resaudw->rb_signed,FALSE);
    gtk_widget_set_sensitive (resaudw->rb_unsigned,FALSE);
    gtk_widget_set_sensitive (resaudw->rb_bigend,FALSE);
    gtk_widget_set_sensitive (resaudw->rb_littleend,FALSE);
    gtk_widget_set_sensitive (resaudw->entry_arate,FALSE);
    gtk_widget_set_sensitive (resaudw->entry_asamps,FALSE);
    gtk_widget_set_sensitive (resaudw->entry_achans,FALSE);
    if (prefsw!=NULL) {
      gtk_widget_set_sensitive (prefsw->pertrack_checkbutton,FALSE);
      gtk_widget_set_sensitive (prefsw->backaudio_checkbutton,FALSE);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(prefsw->pertrack_checkbutton),FALSE);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(prefsw->backaudio_checkbutton),FALSE);
    }
    else if (rdet!=NULL) {
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(rdet->pertrack_checkbutton),FALSE);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(rdet->backaudio_checkbutton),FALSE);
      gtk_widget_set_sensitive(rdet->pertrack_checkbutton,FALSE);
      gtk_widget_set_sensitive(rdet->backaudio_checkbutton,FALSE);
    }
  }
  else {
    if (atoi (gtk_entry_get_text (GTK_ENTRY (resaudw->entry_asamps)))!=8) {
      gtk_widget_set_sensitive (resaudw->rb_bigend,TRUE);
      gtk_widget_set_sensitive (resaudw->rb_littleend,TRUE);
    }
    gtk_widget_set_sensitive (resaudw->rb_signed,TRUE);
    gtk_widget_set_sensitive (resaudw->rb_unsigned,TRUE);
    gtk_widget_set_sensitive (resaudw->entry_arate,TRUE);
    gtk_widget_set_sensitive (resaudw->entry_asamps,TRUE);
    gtk_widget_set_sensitive (resaudw->entry_achans,TRUE);
    if (prefsw!=NULL) {
      gtk_widget_set_sensitive (prefsw->pertrack_checkbutton,TRUE);
      gtk_widget_set_sensitive (prefsw->backaudio_checkbutton,TRUE);
    }
    if (rdet!=NULL) {
      gtk_widget_set_sensitive(rdet->backaudio_checkbutton,TRUE);
      gtk_widget_set_sensitive(rdet->pertrack_checkbutton,TRUE);
    }

    tmp=g_strdup_printf ("%d",DEFAULT_AUDIO_CHANS);
    gtk_entry_set_text (GTK_ENTRY (resaudw->entry_achans),tmp);
    g_free (tmp);

  }
}




void 
on_resaudw_asamps_changed (GtkWidget *irrelevant, gpointer rubbish) {
  if (atoi (gtk_entry_get_text (GTK_ENTRY (resaudw->entry_asamps)))==8) {
    gtk_widget_set_sensitive (resaudw->rb_bigend,FALSE);
    gtk_widget_set_sensitive (resaudw->rb_littleend,FALSE);
  }
  else {
    gtk_widget_set_sensitive (resaudw->rb_bigend,TRUE);
    gtk_widget_set_sensitive (resaudw->rb_littleend,TRUE);
  }
}



void
on_resample_video_activate (GtkMenuItem     *menuitem,
			    gpointer         user_data)
{
  // change speed from the menu
  create_new_pb_speed(2);
  mainw->fx1_val=cfile->fps;
}



void
on_resample_vid_ok (GtkButton *button, GtkEntry *entry)
{
  int i;
  gint old_frames;
  gint ostart=cfile->start;
  gint oend=cfile->end;
  gdouble oundo1_dbl=cfile->undo1_dbl;
  gchar *msg;
  weed_timecode_t in_time=0;
  gdouble old_fps=cfile->fps;
  gboolean ratio_fps;
  weed_plant_t *real_back_list=NULL;
  weed_plant_t *new_event_list=NULL;

  mainw->error=FALSE;

  if (button!=NULL) {
    gtk_widget_destroy(gtk_widget_get_toplevel(GTK_WIDGET(button)));
    if (mainw->fx1_val==0.) mainw->fx1_val=1.;
  }
  else {
    mainw->fx1_val=cfile->undo1_dbl;
  }

  if (mainw->current_file<0||cfile->frames==0) return;

  if (mainw->fx1_val==cfile->fps&&cfile->event_list==NULL) return;

  real_back_list=cfile->event_list;

  if (cfile->event_list==NULL) {
    for (i=1;i<=cfile->frames;i++) {
      new_event_list=append_frame_event (new_event_list,in_time,1,&(mainw->current_file),&i);
      if (new_event_list==NULL) {
	do_memory_error_dialog();
	return;
      }
      in_time+=(weed_timecode_t)(1./cfile->fps*U_SEC+.5);
    }
    cfile->event_list=new_event_list;
  }
  cfile->undo1_dbl=cfile->fps;

  if (cfile->event_list_back!=NULL) event_list_free (cfile->event_list_back);
  cfile->event_list_back=cfile->event_list;

  //QUANTISE
  new_event_list=quantise_events(cfile->event_list_back,mainw->fx1_val,real_back_list!=NULL);
  if (new_event_list==NULL) return; // memory error
  cfile->event_list=new_event_list;

  if (real_back_list==NULL) event_list_free (cfile->event_list_back);
  cfile->event_list_back=NULL;

  // TODO - end_threaded_dialog

  if (cfile->event_list==NULL) {
    do_memory_error_dialog();
    cfile->event_list=real_back_list;
    cfile->undo1_dbl=oundo1_dbl;
    mainw->error=TRUE;
    return;
  }

  if (mainw->multitrack!=NULL) return;

  ratio_fps=check_for_ratio_fps(mainw->fx1_val);

  // we have now quantised to fixed fps; we have come here from reorder

  if (ratio_fps) {
    // got a ratio
    msg=g_strdup_printf(_ ("Resampling video at %.8f frames per second..."),mainw->fx1_val);
  }
  else {
    msg=g_strdup_printf(_ ("Resampling video at %.3f frames per second..."),mainw->fx1_val);
  }
  if (mainw->current_file>0) {
    d_print(msg);
  }
  g_free(msg);

  old_frames=cfile->frames;

  // must set these before calling reorder
  cfile->start=(gint)((cfile->start-1.)/old_fps*mainw->fx1_val+1.);
  if ((cfile->end=(gint)((cfile->end*mainw->fx1_val)/old_fps+.49999))<cfile->start) cfile->end=cfile->start;

  cfile->undo_action=UNDO_RESAMPLE;
  // REORDER
  // this calls reorder_frames, which sets event_list_back==event_list, and clears event_list
  on_reorder_activate();
  
  if (cfile->frames<=0||mainw->cancelled!=CANCEL_NONE) {
    // reordering error...
    cfile->event_list=real_back_list;
    if (cfile->event_list_back!=NULL) event_list_free (cfile->event_list_back);
    cfile->event_list_back=NULL;
    cfile->frames=old_frames;
    cfile->start=ostart;
    cfile->end=oend;
    load_start_image(cfile->start);
    load_end_image(cfile->end);
    cfile->undo1_dbl=oundo1_dbl;
    sensitize();
    mainw->error=TRUE;
    if (cfile->frames<0) do_error_dialog (_("Reordering error !\n"));
    return;
  }

  if (cfile->event_list_back!=NULL) event_list_free (cfile->event_list_back);
  cfile->event_list_back=real_back_list;

  cfile->ratio_fps=ratio_fps;
  cfile->pb_fps=cfile->fps=mainw->fx1_val;
  cfile->old_frames=old_frames;

  set_undoable (_("Resample"),TRUE);

  save_clip_value(mainw->current_file,CLIP_DETAILS_FPS,&cfile->fps);
  save_clip_value(mainw->current_file,CLIP_DETAILS_PB_FPS,&cfile->fps);

  switch_to_file(mainw->current_file,mainw->current_file);

}



///////// GUI stuff /////////////////////////////////////////////////////



_resaudw *
create_resaudw (gshort type, render_details *rdet, GtkWidget *top_vbox) {
  // type 1 == resample
  // type 2 == insert silence
  // type 3 == enter multitrack or encode or render to clip
  // type 4 == prefs/multitrack
  // type 5 == new clip record/record to selection with no existing audio
  // type 6 == record to clip with no existing audio
  // type 7 == record to clip with existing audio (show time only)
  // type 8 == grab external window, with audio
  // type 9 == grab external, no audio
  // type 10 == change inside multitrack

  GtkWidget *dialog_vbox=NULL;
  GtkWidget *vbox21;
  GtkWidget *frame6;
  GtkWidget *hbox24;
  GtkWidget *label94;
  GtkWidget *combo_entry2;
  GtkWidget *label95;
  GtkWidget *combo_entry3;
  GtkWidget *label96;
  GtkWidget *combo_entry1;
  GtkWidget *vseparator3;
  GtkWidget *vbox22;
  GtkWidget *radiobutton26;
  GSList *radiobutton26_group = NULL;
  GtkWidget *radiobutton27;
  GtkWidget *vseparator4;
  GtkWidget *vbox23;
  GtkWidget *radiobutton28;
  GSList *radiobutton28_group = NULL;
  GtkWidget *radiobutton29;
  GtkWidget *label92;
  GtkWidget *frame7;
  GtkWidget *hbox25;
  GtkWidget *label98;
  GtkWidget *combo4;
  GtkWidget *label99;
  GtkWidget *combo5;
  GtkWidget *label100;
  GtkWidget *combo6;
  GtkWidget *vseparator5;
  GtkWidget *vbox24;
  GSList *radiobutton30_group = NULL;
  GSList *rbgroup = NULL;
  GtkWidget *vseparator6;
  GtkWidget *vbox25;
  GSList *radiobutton32_group = NULL;
  GtkWidget *label93;
  GtkWidget *dialog_action_area15;
  GtkWidget *cancelbutton13;
  GtkWidget *okbutton12;
  GtkWidget *label;
  GtkWidget *hsep;
  GtkWidget *radiobutton;
  GtkWidget *hbox;
  GtkWidget *hbox2;
  GtkWidget *xvbox;
  GtkWidget *eventbox;
  GtkObject *spinbutton_adj;
  GList *channels = NULL;
  GList *sampsize = NULL;
  GList *rate = NULL;
  gchar *tmp;

  gint hours=0,mins=0;
  gdouble secs=0.;

  gint aendian;

  gboolean chans_fixed=FALSE;

  if (type==10) {
    if (mainw->multitrack!=NULL) chans_fixed=TRUE; // TODO *
    type=3;
  }

  if (type>5&&mainw->rec_end_time!=-1.) {
    hours=(gint)(mainw->rec_end_time/3600.);
    mins=(gint)((mainw->rec_end_time-(hours*3600.))/60.);
    secs=mainw->rec_end_time-hours*3600.-mins*60.;
  }

  _resaudw *resaudw=(_resaudw*)(g_malloc(sizeof(_resaudw)));

  //if (type>2&&type<5) channels=g_list_append(channels,"0");
  channels = g_list_append (channels, "1");
  channels = g_list_append (channels, "2");

  sampsize = g_list_append (sampsize, "8");
  sampsize = g_list_append (sampsize, "16");

  rate = g_list_append (rate, "5512");
  rate = g_list_append (rate, "8000");
  rate = g_list_append (rate, "11025");
  rate = g_list_append (rate, "22050");
  rate = g_list_append (rate, "32000");
  rate = g_list_append (rate, "44100");
  rate = g_list_append (rate, "48000");
  rate = g_list_append (rate, "88200");
  rate = g_list_append (rate, "96000");
  rate = g_list_append (rate, "128000");

  if (type<3||type>4) {
    resaudw->dialog = gtk_dialog_new ();
    gtk_container_set_border_width (GTK_CONTAINER (resaudw->dialog), 10);
    if (type==1) {
      gtk_window_set_title (GTK_WINDOW (resaudw->dialog), _("LiVES: - Resample Audio"));
    }
    else if (type==2) {
      gtk_window_set_title (GTK_WINDOW (resaudw->dialog), _("LiVES: - Insert Silence"));
    }
    else if (type==5) {
      gtk_window_set_title (GTK_WINDOW (resaudw->dialog), _("LiVES: - New Clip Audio"));
    }
    else if (type==9||type==8) {
      gtk_window_set_title (GTK_WINDOW (resaudw->dialog), _("LiVES: - External Clip Settings"));
    }
    gtk_window_set_position (GTK_WINDOW (resaudw->dialog), GTK_WIN_POS_CENTER);
    gtk_window_set_modal (GTK_WINDOW (resaudw->dialog), TRUE);
    gtk_window_set_transient_for(GTK_WINDOW(resaudw->dialog),GTK_WINDOW(mainw->LiVES));

    dialog_vbox = GTK_DIALOG (resaudw->dialog)->vbox;
    gtk_widget_show (dialog_vbox);
    if (palette->style&STYLE_1) {
      gtk_widget_modify_bg (resaudw->dialog, GTK_STATE_NORMAL, &palette->normal_back);
    }

    vbox21 = gtk_vbox_new (FALSE, 0);
    gtk_widget_show (vbox21);
    gtk_box_pack_start (GTK_BOX (dialog_vbox), vbox21, TRUE, TRUE, 0);
  }
  else if (type==3) vbox21=GTK_DIALOG(rdet->dialog)->vbox;
  else vbox21=top_vbox;

  frame6 = gtk_frame_new (NULL);
 
  if (type==1) {

    gtk_widget_show (frame6);
    gtk_box_pack_start (GTK_BOX (vbox21), frame6, TRUE, TRUE, 0);
    
    if (palette->style&STYLE_1) {
      gtk_widget_modify_bg (frame6, GTK_STATE_NORMAL, &palette->normal_back);
    }
    
    hbox24 = gtk_hbox_new (FALSE, 0);
    gtk_widget_show (hbox24);
    gtk_container_add (GTK_CONTAINER (frame6), hbox24);
    gtk_container_set_border_width (GTK_CONTAINER (hbox24), 10);
    
    label94 = gtk_label_new (_("Rate (Hz) "));
    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg(label94, GTK_STATE_NORMAL, &palette->normal_fore);
    }
    gtk_widget_show (label94);
    gtk_box_pack_start (GTK_BOX (hbox24), label94, FALSE, FALSE, 0);
    gtk_label_set_justify (GTK_LABEL (label94), GTK_JUSTIFY_LEFT);
    
    combo_entry2 = gtk_entry_new();
    gtk_widget_show (combo_entry2);
    gtk_box_pack_start (GTK_BOX (hbox24), combo_entry2, TRUE, TRUE, 1);

    gtk_editable_set_editable (GTK_EDITABLE (combo_entry2), FALSE);
    gtk_entry_set_width_chars (GTK_ENTRY (combo_entry2), 8);
    
    tmp=g_strdup_printf ("%d",(gint)mainw->fx1_val);

    gtk_entry_set_text (GTK_ENTRY (combo_entry2),tmp);
    g_free (tmp);
    GTK_WIDGET_UNSET_FLAGS (combo_entry2, GTK_CAN_FOCUS);
    
    label95 = gtk_label_new (_("         Channels "));
    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg(label95, GTK_STATE_NORMAL, &palette->normal_fore);
    }
    gtk_widget_show (label95);
    gtk_box_pack_start (GTK_BOX (hbox24), label95, FALSE, FALSE, 0);
    gtk_label_set_justify (GTK_LABEL (label95), GTK_JUSTIFY_LEFT);
    
    combo_entry3 = gtk_entry_new();
    gtk_widget_show (combo_entry3);
    gtk_box_pack_start (GTK_BOX (hbox24), combo_entry3, TRUE, TRUE, 1);
    
    gtk_editable_set_editable (GTK_EDITABLE (combo_entry3), FALSE);
    gtk_entry_set_width_chars (GTK_ENTRY (combo_entry3), 3);
    
    tmp=g_strdup_printf ("%d",(gint)mainw->fx2_val);
    gtk_entry_set_text (GTK_ENTRY (combo_entry3),tmp);
    g_free (tmp);
    GTK_WIDGET_UNSET_FLAGS (combo_entry3, GTK_CAN_FOCUS);
    
    label96 = gtk_label_new (_("        Sample Size "));
    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg(label96, GTK_STATE_NORMAL, &palette->normal_fore);
    }
    gtk_widget_show (label96);
    gtk_box_pack_start (GTK_BOX (hbox24), label96, FALSE, FALSE, 0);
    gtk_label_set_justify (GTK_LABEL (label96), GTK_JUSTIFY_LEFT);

    combo_entry1 = gtk_entry_new();
    gtk_widget_show (combo_entry1);
    gtk_box_pack_start (GTK_BOX (hbox24), combo_entry1, TRUE, TRUE, 1);

    gtk_editable_set_editable (GTK_EDITABLE (combo_entry1), FALSE);
    gtk_entry_set_width_chars (GTK_ENTRY (combo_entry1), 3);

    tmp=g_strdup_printf ("%d",(gint)mainw->fx3_val);
    gtk_entry_set_text (GTK_ENTRY (combo_entry1),tmp);
    g_free (tmp);
    GTK_WIDGET_UNSET_FLAGS (combo_entry1, GTK_CAN_FOCUS);

    vseparator3 = gtk_vseparator_new ();
    gtk_widget_show (vseparator3);
    gtk_box_pack_start (GTK_BOX (hbox24), vseparator3, FALSE, FALSE, 12);

    vbox22 = gtk_vbox_new (FALSE, 0);
    gtk_widget_show (vbox22);
    gtk_box_pack_start (GTK_BOX (hbox24), vbox22, TRUE, TRUE, 0);

    hbox = gtk_hbox_new (FALSE, 0);
    gtk_widget_show(hbox);
    gtk_box_pack_start (GTK_BOX (vbox22), hbox, FALSE, FALSE, 10);

    radiobutton26 = gtk_radio_button_new (NULL);
    gtk_widget_show(radiobutton26);
    gtk_radio_button_set_group (GTK_RADIO_BUTTON (radiobutton26), radiobutton26_group);
    radiobutton26_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radiobutton26));

    gtk_box_pack_start (GTK_BOX (hbox), radiobutton26, FALSE, FALSE, 10);

    label=gtk_label_new_with_mnemonic (_ ("Signed"));
    gtk_widget_show(label);
    gtk_label_set_mnemonic_widget (GTK_LABEL (label),radiobutton26);

    eventbox=gtk_event_box_new();
    gtk_widget_show(eventbox);
    gtk_container_add(GTK_CONTAINER(eventbox),label);

    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
      gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
      gtk_widget_modify_bg (eventbox, GTK_STATE_NORMAL, &palette->normal_back);
    }
    gtk_box_pack_start (GTK_BOX (hbox), eventbox, FALSE, FALSE, 10);


    hbox = gtk_hbox_new (FALSE, 0);
    gtk_widget_show(hbox);
    gtk_box_pack_start (GTK_BOX (vbox22), hbox, FALSE, FALSE, 10);

    radiobutton27 = gtk_radio_button_new (NULL);
    gtk_widget_show(radiobutton27);
    gtk_radio_button_set_group (GTK_RADIO_BUTTON (radiobutton27), radiobutton26_group);
    radiobutton26_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radiobutton27));

    gtk_box_pack_start (GTK_BOX (hbox), radiobutton27, FALSE, FALSE, 10);

    label=gtk_label_new_with_mnemonic (_ ("Unsigned"));
    gtk_widget_show(label);
    gtk_label_set_mnemonic_widget (GTK_LABEL (label),radiobutton27);

    eventbox=gtk_event_box_new();
    gtk_widget_show(eventbox);
    gtk_container_add(GTK_CONTAINER(eventbox),label);

    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
      gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
      gtk_widget_modify_bg (eventbox, GTK_STATE_NORMAL, &palette->normal_back);
    }
    gtk_box_pack_start (GTK_BOX (hbox), eventbox, FALSE, FALSE, 10);


    aendian=mainw->fx4_val;

    if (aendian&AFORM_UNSIGNED) {
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radiobutton27), TRUE);
    }
    else {
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radiobutton26), TRUE);
    }

    gtk_widget_set_sensitive (radiobutton26, FALSE);
    gtk_widget_set_sensitive (radiobutton27, FALSE);

    vseparator4 = gtk_vseparator_new ();
    gtk_widget_show (vseparator4);
    gtk_box_pack_start (GTK_BOX (hbox24), vseparator4, FALSE, FALSE, 12);
    
    vbox23 = gtk_vbox_new (FALSE, 0);
    gtk_widget_show (vbox23);
    gtk_box_pack_start (GTK_BOX (hbox24), vbox23, TRUE, TRUE, 0);

    hbox = gtk_hbox_new (FALSE, 0);
    gtk_widget_show(hbox);
    gtk_box_pack_start (GTK_BOX (vbox23), hbox, FALSE, FALSE, 10);

    radiobutton28 = gtk_radio_button_new (NULL);
    gtk_widget_show(radiobutton28);
    gtk_radio_button_set_group (GTK_RADIO_BUTTON (radiobutton28), radiobutton28_group);
    radiobutton28_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radiobutton28));

    gtk_box_pack_start (GTK_BOX (hbox), radiobutton28, FALSE, FALSE, 10);

    label=gtk_label_new_with_mnemonic (_ ("Little Endian"));
    gtk_widget_show(label);
    gtk_label_set_mnemonic_widget (GTK_LABEL (label),radiobutton28);

    eventbox=gtk_event_box_new();
    gtk_widget_show(eventbox);
    gtk_container_add(GTK_CONTAINER(eventbox),label);

    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
      gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
      gtk_widget_modify_bg (eventbox, GTK_STATE_NORMAL, &palette->normal_back);
    }
    gtk_box_pack_start (GTK_BOX (hbox), eventbox, FALSE, FALSE, 10);


    hbox = gtk_hbox_new (FALSE, 0);
    gtk_widget_show(hbox);
    gtk_box_pack_start (GTK_BOX (vbox23), hbox, FALSE, FALSE, 10);

    radiobutton29 = gtk_radio_button_new (NULL);
    gtk_widget_show(radiobutton29);
    gtk_radio_button_set_group (GTK_RADIO_BUTTON (radiobutton29), radiobutton28_group);
    radiobutton28_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radiobutton29));

    gtk_box_pack_start (GTK_BOX (hbox), radiobutton29, FALSE, FALSE, 10);

    label=gtk_label_new_with_mnemonic (_ ("Big Endian"));
    gtk_widget_show(label);
    gtk_label_set_mnemonic_widget (GTK_LABEL (label),radiobutton28);

    eventbox=gtk_event_box_new();
    gtk_widget_show(eventbox);
    gtk_container_add(GTK_CONTAINER(eventbox),label);

    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
      gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
      gtk_widget_modify_bg (eventbox, GTK_STATE_NORMAL, &palette->normal_back);
    }
    gtk_box_pack_start (GTK_BOX (hbox), eventbox, FALSE, FALSE, 10);

    if (aendian&AFORM_BIG_ENDIAN) {
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radiobutton29), TRUE);
    }
    else {
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radiobutton28), TRUE);
    }

    gtk_widget_set_sensitive (radiobutton29, FALSE);
    gtk_widget_set_sensitive (radiobutton28, FALSE);

    label92 = gtk_label_new (_("Current"));
    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg(label92, GTK_STATE_NORMAL, &palette->normal_fore);
    }

    gtk_widget_show (label92);
    gtk_frame_set_label_widget (GTK_FRAME (frame6), label92);
    gtk_label_set_justify (GTK_LABEL (label92), GTK_JUSTIFY_LEFT);
    
  }

  resaudw->aud_checkbutton = NULL;


  if (type<9) {
    frame7 = gtk_frame_new (NULL);
    gtk_widget_show (frame7);
    gtk_box_pack_start (GTK_BOX (vbox21), frame7, TRUE, TRUE, 0);

    if (type!=4) {
      if (palette->style&STYLE_1) {
	gtk_widget_modify_bg (frame7, GTK_STATE_NORMAL, &palette->normal_back);
      }
    }

    xvbox = gtk_vbox_new (FALSE, 0);
    gtk_widget_show(xvbox);
    gtk_container_add (GTK_CONTAINER (frame7), xvbox);
    
    resaudw->aud_checkbutton = gtk_check_button_new ();

    if (type>2&&type<5) {

      eventbox=gtk_event_box_new();
      label=gtk_label_new_with_mnemonic (_("_Enable audio"));
      gtk_label_set_mnemonic_widget (GTK_LABEL (label),resaudw->aud_checkbutton);

      gtk_container_add(GTK_CONTAINER(eventbox),label);
      g_signal_connect (GTK_OBJECT (eventbox), "button_press_event",
			G_CALLBACK (label_act_toggle),
			resaudw->aud_checkbutton);

      if (type!=4) {
	if (palette->style&STYLE_1) {
	  gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
	  gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
	  gtk_widget_modify_bg (eventbox, GTK_STATE_NORMAL, &palette->normal_back);
	}
      }

      resaudw->aud_hbox = gtk_hbox_new (FALSE, 0);
      gtk_box_pack_start (GTK_BOX (xvbox), resaudw->aud_hbox, FALSE, FALSE, 0);

      gtk_box_pack_start (GTK_BOX (resaudw->aud_hbox), resaudw->aud_checkbutton, FALSE, FALSE, 10);
      gtk_box_pack_start (GTK_BOX (resaudw->aud_hbox), eventbox, FALSE, FALSE, 10);
      GTK_WIDGET_SET_FLAGS (resaudw->aud_checkbutton, GTK_CAN_DEFAULT|GTK_CAN_FOCUS);
      if (rdet!=NULL) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (resaudw->aud_checkbutton), rdet->achans>0);
      else gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (resaudw->aud_checkbutton), prefs->mt_def_achans>0);

      gtk_widget_show_all(resaudw->aud_hbox);
    }

    hbox25 = gtk_hbox_new (FALSE, 0);
    gtk_widget_show (hbox25);
    gtk_box_pack_start (GTK_BOX (xvbox), hbox25, FALSE, FALSE, 0);
    gtk_container_set_border_width (GTK_CONTAINER (hbox25), 10);
    
    if (type>=3) label98 = gtk_label_new_with_mnemonic (_("_Rate (Hz) "));
    else label98 = gtk_label_new (_("Rate (Hz) "));
    if (type!=4) {
      if (palette->style&STYLE_1) {
	gtk_widget_modify_fg(label98, GTK_STATE_NORMAL, &palette->normal_fore);
      }
    }
    gtk_widget_show (label98);
    gtk_box_pack_start (GTK_BOX (hbox25), label98, FALSE, FALSE, 0);
    gtk_label_set_justify (GTK_LABEL (label98), GTK_JUSTIFY_LEFT);
    
    combo4 = gtk_combo_new ();
    gtk_combo_set_popdown_strings (GTK_COMBO (combo4), rate);
    gtk_widget_show (combo4);
    gtk_box_pack_start (GTK_BOX (hbox25), combo4, TRUE, TRUE, 1);
    
    resaudw->entry_arate = GTK_COMBO (combo4)->entry;
    gtk_widget_show (resaudw->entry_arate);
    gtk_entry_set_width_chars (GTK_ENTRY (resaudw->entry_arate), 8);
    if (type==7) gtk_widget_set_sensitive(combo4,FALSE);
  
    if (type<3||(type>4&&type<8)) tmp=g_strdup_printf ("%d",(gint)mainw->fx1_val);
    else if (type==8) tmp=g_strdup_printf ("%d",DEFAULT_AUDIO_RATE);
    else if (type==3) tmp=g_strdup_printf ("%d",rdet->arate);
    else tmp=g_strdup_printf ("%d",prefs->mt_def_arate);
    gtk_entry_set_text (GTK_ENTRY (resaudw->entry_arate),tmp);
    g_free (tmp);
    
    if (type>=3) label99 = gtk_label_new_with_mnemonic (_("    _Channels "));
    else label99 = gtk_label_new (_("    Channels "));
    if (type!=4) {
      if (palette->style&STYLE_1) {
	gtk_widget_modify_fg(label99, GTK_STATE_NORMAL, &palette->normal_fore);
      }
    }
    gtk_widget_show (label99);
    gtk_box_pack_start (GTK_BOX (hbox25), label99, FALSE, FALSE, 0);
    gtk_label_set_justify (GTK_LABEL (label99), GTK_JUSTIFY_LEFT);
    
    combo5 = gtk_combo_new ();
    gtk_combo_set_popdown_strings (GTK_COMBO (combo5), channels);
    gtk_widget_show (combo5);
    gtk_box_pack_start (GTK_BOX (hbox25), combo5, FALSE, FALSE, 0);
    if (type==7) gtk_widget_set_sensitive(combo5,FALSE);
    
    resaudw->entry_achans = GTK_COMBO (combo5)->entry;
    gtk_widget_show (resaudw->entry_achans);
    gtk_editable_set_editable (GTK_EDITABLE (resaudw->entry_achans), FALSE);
    gtk_entry_set_width_chars (GTK_ENTRY (resaudw->entry_achans), 3);
    
    if (type<3||(type>4&&type<8)) tmp=g_strdup_printf ("%d",(gint)mainw->fx2_val);
    else if (type==8) tmp=g_strdup_printf ("%d",DEFAULT_AUDIO_CHANS);
    else if (type==3) tmp=g_strdup_printf ("%d",rdet->achans);
    else tmp=g_strdup_printf ("%d",prefs->mt_def_achans==0?DEFAULT_AUDIO_CHANS:prefs->mt_def_achans);
    gtk_entry_set_text (GTK_ENTRY (resaudw->entry_achans),tmp);
    g_free (tmp);
    
    if (chans_fixed) {
      gtk_widget_set_sensitive(resaudw->entry_achans,FALSE);
      gtk_widget_set_sensitive(combo5,FALSE);
    }

    if (type>=3) label100 = gtk_label_new_with_mnemonic (_("    _Sample Size "));
    else label100 = gtk_label_new (_("    Sample Size "));
    if (type!=4) {
      if (palette->style&STYLE_1) {
	gtk_widget_modify_fg(label100, GTK_STATE_NORMAL, &palette->normal_fore);
      }
    }
    gtk_widget_show (label100);
    gtk_box_pack_start (GTK_BOX (hbox25), label100, FALSE, FALSE, 0);
    gtk_label_set_justify (GTK_LABEL (label100), GTK_JUSTIFY_LEFT);
    
    combo6 = gtk_combo_new ();
    gtk_combo_set_popdown_strings (GTK_COMBO (combo6), sampsize);
    gtk_widget_show (combo6);
    gtk_box_pack_start (GTK_BOX (hbox25), combo6, TRUE, TRUE, 0);
    if (type==7) gtk_widget_set_sensitive(combo6,FALSE);

    resaudw->entry_asamps = GTK_COMBO (combo6)->entry;
    gtk_widget_show (resaudw->entry_asamps);
    gtk_entry_set_max_length (GTK_ENTRY (resaudw->entry_asamps), 2);
    gtk_editable_set_editable (GTK_EDITABLE (resaudw->entry_asamps), FALSE);
    gtk_entry_set_width_chars (GTK_ENTRY (resaudw->entry_asamps), 3);
    
    if (type<3||(type>4&&type<8)) tmp=g_strdup_printf ("%d",(gint)mainw->fx3_val);
    else if (type==8) tmp=g_strdup_printf ("%d",DEFAULT_AUDIO_SAMPS);
    else if (type==3) tmp=g_strdup_printf ("%d",rdet->asamps);
    else tmp=g_strdup_printf ("%d",prefs->mt_def_asamps);
    gtk_entry_set_text (GTK_ENTRY (resaudw->entry_asamps),tmp);
    g_free (tmp);

    vseparator5 = gtk_vseparator_new ();
    gtk_widget_show (vseparator5);
    gtk_box_pack_start (GTK_BOX (hbox25), vseparator5, FALSE, FALSE, 12);

    vbox24 = gtk_vbox_new (FALSE, 0);
    gtk_widget_show (vbox24);
    gtk_box_pack_start (GTK_BOX (hbox25), vbox24, TRUE, TRUE, 0);

    hbox = gtk_hbox_new (FALSE, 0);
    gtk_widget_show(hbox);
    gtk_box_pack_start (GTK_BOX (vbox24), hbox, FALSE, FALSE, 10);

    resaudw->rb_signed = gtk_radio_button_new (NULL);
    gtk_widget_show(resaudw->rb_signed);
    gtk_radio_button_set_group (GTK_RADIO_BUTTON (resaudw->rb_signed), radiobutton30_group);
    radiobutton30_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (resaudw->rb_signed));

    gtk_box_pack_start (GTK_BOX (hbox), resaudw->rb_signed, FALSE, FALSE, 10);

    label=gtk_label_new_with_mnemonic (_ ("Signed"));
    gtk_widget_show(label);
    gtk_label_set_mnemonic_widget (GTK_LABEL (label),resaudw->rb_signed);

    eventbox=gtk_event_box_new();
    gtk_widget_show(eventbox);
    gtk_container_add(GTK_CONTAINER(eventbox),label);
    g_signal_connect (GTK_OBJECT (eventbox), "button_press_event",
		      G_CALLBACK (label_act_toggle),
		      resaudw->rb_signed);
    if (type!=4) {
      if (palette->style&STYLE_1) {
	gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
	gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
	gtk_widget_modify_bg (eventbox, GTK_STATE_NORMAL, &palette->normal_back);
      }
    }
    gtk_box_pack_start (GTK_BOX (hbox), eventbox, FALSE, FALSE, 10);
  
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (resaudw->rb_signed), TRUE);
    if (type==7) gtk_widget_set_sensitive(resaudw->rb_signed,FALSE);
    

    hbox = gtk_hbox_new (FALSE, 0);
    gtk_widget_show(hbox);
    gtk_box_pack_start (GTK_BOX (vbox24), hbox, FALSE, FALSE, 10);

    resaudw->rb_unsigned = gtk_radio_button_new (NULL);
    gtk_widget_show(resaudw->rb_unsigned);
    gtk_radio_button_set_group (GTK_RADIO_BUTTON (resaudw->rb_unsigned), radiobutton30_group);
    radiobutton30_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (resaudw->rb_unsigned));

    gtk_box_pack_start (GTK_BOX (hbox), resaudw->rb_unsigned, FALSE, FALSE, 10);

    label=gtk_label_new_with_mnemonic (_ ("Unsigned"));
    gtk_widget_show(label);
    gtk_label_set_mnemonic_widget (GTK_LABEL (label),resaudw->rb_unsigned);

    eventbox=gtk_event_box_new();
    gtk_widget_show(eventbox);
    gtk_container_add(GTK_CONTAINER(eventbox),label);
    g_signal_connect (GTK_OBJECT (eventbox), "button_press_event",
		      G_CALLBACK (label_act_toggle),
		      resaudw->rb_unsigned);
    if (type!=4) {
      if (palette->style&STYLE_1) {
	gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
	gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
	gtk_widget_modify_bg (eventbox, GTK_STATE_NORMAL, &palette->normal_back);
      }
    }
    gtk_box_pack_start (GTK_BOX (hbox), eventbox, FALSE, FALSE, 10);

    if (type==7) gtk_widget_set_sensitive(resaudw->rb_unsigned,FALSE);

    if (type<3||(type>4&&type<8)) aendian=mainw->fx4_val;
    else if (type==8) aendian=DEFAULT_AUDIO_SIGNED|((G_BYTE_ORDER==G_BIG_ENDIAN)&AFORM_BIG_ENDIAN);
    else if (type==3) aendian=rdet->aendian;
    else aendian=prefs->mt_def_signed_endian;

    if (aendian&AFORM_UNSIGNED) {
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (resaudw->rb_unsigned), TRUE);
    }
    else {
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (resaudw->rb_signed), TRUE);
    }

    vseparator6 = gtk_vseparator_new ();
    gtk_widget_show (vseparator6);
    gtk_box_pack_start (GTK_BOX (hbox25), vseparator6, FALSE, FALSE, 12);

    vbox25 = gtk_vbox_new (FALSE, 0);
    gtk_widget_show (vbox25);
    gtk_box_pack_start (GTK_BOX (hbox25), vbox25, TRUE, TRUE, 0);


    hbox = gtk_hbox_new (FALSE, 0);
    gtk_widget_show(hbox);
    gtk_box_pack_start (GTK_BOX (vbox25), hbox, FALSE, FALSE, 10);

    resaudw->rb_littleend = gtk_radio_button_new (NULL);
    gtk_widget_show(resaudw->rb_littleend);
    gtk_radio_button_set_group (GTK_RADIO_BUTTON (resaudw->rb_littleend), radiobutton32_group);
    radiobutton32_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (resaudw->rb_littleend));

    gtk_box_pack_start (GTK_BOX (hbox), resaudw->rb_littleend, FALSE, FALSE, 10);

    label=gtk_label_new_with_mnemonic (_ ("Little Endian"));
    gtk_widget_show(label);
    gtk_label_set_mnemonic_widget (GTK_LABEL (label),resaudw->rb_littleend);

    eventbox=gtk_event_box_new();
    gtk_widget_show(eventbox);
    gtk_container_add(GTK_CONTAINER(eventbox),label);
    g_signal_connect (GTK_OBJECT (eventbox), "button_press_event",
		      G_CALLBACK (label_act_toggle),
		      resaudw->rb_littleend);
    if (type!=4) {
      if (palette->style&STYLE_1) {
	gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
	gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
	gtk_widget_modify_bg (eventbox, GTK_STATE_NORMAL, &palette->normal_back);
      }
    }
    gtk_box_pack_start (GTK_BOX (hbox), eventbox, FALSE, FALSE, 10);

    if (type==7) gtk_widget_set_sensitive(resaudw->rb_littleend,FALSE);

    hbox = gtk_hbox_new (FALSE, 0);
    gtk_widget_show(hbox);
    gtk_box_pack_start (GTK_BOX (vbox25), hbox, FALSE, FALSE, 10);

    resaudw->rb_bigend = gtk_radio_button_new (NULL);
    gtk_widget_show(resaudw->rb_bigend);
    gtk_radio_button_set_group (GTK_RADIO_BUTTON (resaudw->rb_bigend), radiobutton32_group);
    radiobutton32_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (resaudw->rb_bigend));

    gtk_box_pack_start (GTK_BOX (hbox), resaudw->rb_bigend, FALSE, FALSE, 10);

    label=gtk_label_new_with_mnemonic (_ ("Big Endian"));
    gtk_widget_show(label);
    gtk_label_set_mnemonic_widget (GTK_LABEL (label),resaudw->rb_bigend);

    eventbox=gtk_event_box_new();
    gtk_widget_show(eventbox);
    gtk_container_add(GTK_CONTAINER(eventbox),label);
    g_signal_connect (GTK_OBJECT (eventbox), "button_press_event",
		      G_CALLBACK (label_act_toggle),
		      resaudw->rb_bigend);
    if (type!=4) {
      if (palette->style&STYLE_1) {
	gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
	gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
	gtk_widget_modify_bg (eventbox, GTK_STATE_NORMAL, &palette->normal_back);
      }
    }
    gtk_box_pack_start (GTK_BOX (hbox), eventbox, FALSE, FALSE, 10);

    if (type==7) gtk_widget_set_sensitive(resaudw->rb_bigend,FALSE);

    if (aendian&AFORM_BIG_ENDIAN) {
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (resaudw->rb_bigend), TRUE);
    }
    else {
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (resaudw->rb_littleend), TRUE);
    }

    if (!strcmp(gtk_entry_get_text (GTK_ENTRY (resaudw->entry_asamps)),"8")) {
      gtk_widget_set_sensitive (resaudw->rb_littleend, FALSE);
      gtk_widget_set_sensitive (resaudw->rb_bigend, FALSE);
    }

    g_signal_connect (GTK_OBJECT(resaudw->entry_asamps), "changed",
		      G_CALLBACK (on_resaudw_asamps_changed),
		      NULL);

    if (type>=3) label93 = gtk_label_new (_("Audio"));
    else label93 = gtk_label_new (_("New"));
    if (type!=4) {
      if (palette->style&STYLE_1) {
	gtk_widget_modify_fg(label93, GTK_STATE_NORMAL, &palette->normal_fore);
      }
    }
    gtk_widget_show (label93);

    if (type==3&&palette->style&STYLE_1) gtk_widget_modify_bg(frame7, GTK_STATE_NORMAL, &palette->normal_back);

    gtk_frame_set_label_widget (GTK_FRAME (frame7), label93);
    gtk_label_set_justify (GTK_LABEL (label93), GTK_JUSTIFY_LEFT);
    
  }

  if (type>7) {
    frame7 = gtk_frame_new (NULL);
    gtk_widget_show (frame7);
    gtk_box_pack_start (GTK_BOX (vbox21), frame7, TRUE, TRUE, 0);

    if (palette->style&STYLE_1) {
      gtk_widget_modify_bg (frame7, GTK_STATE_NORMAL, &palette->normal_back);
    }
    
    hbox25 = gtk_hbox_new (FALSE, 0);
    gtk_widget_show (hbox25);
    gtk_container_add (GTK_CONTAINER (frame7), hbox25);
    gtk_container_set_border_width (GTK_CONTAINER (hbox25), 10);
    
    if (type>=3) label98 = gtk_label_new_with_mnemonic (_("_Frames Per Second "));
    gtk_widget_show (label98);
    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg(label98, GTK_STATE_NORMAL, &palette->normal_fore);
    }

    gtk_box_pack_start (GTK_BOX (hbox25), label98, FALSE, FALSE, 0);
    gtk_label_set_justify (GTK_LABEL (label98), GTK_JUSTIFY_LEFT);

    spinbutton_adj = gtk_adjustment_new (prefs->default_fps, 1., FPS_MAX, 1., 1., 0.);
    resaudw->fps_spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_adj), 1, 3);
    gtk_widget_show (resaudw->fps_spinbutton);
    gtk_box_pack_start (GTK_BOX (hbox25), resaudw->fps_spinbutton, FALSE, FALSE, 20);

    label93 = gtk_label_new (_("Video"));
    gtk_widget_show (label93);

    if (palette->style&STYLE_1) {
      gtk_widget_modify_bg(label93, GTK_STATE_NORMAL, &palette->normal_back);
      gtk_widget_modify_fg(label93, GTK_STATE_NORMAL, &palette->normal_fore);
    }

    gtk_frame_set_label_widget (GTK_FRAME (frame7), label93);
    gtk_label_set_justify (GTK_LABEL (label93), GTK_JUSTIFY_LEFT);
  }

  if (type>4) {
    gtk_box_set_spacing(GTK_BOX(dialog_vbox),30);
    
    hbox = gtk_hbox_new (FALSE, 0);
    gtk_widget_show (hbox);
    gtk_box_pack_start (GTK_BOX (dialog_vbox), hbox, TRUE, TRUE, 10);
    
    hbox2 = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (hbox), hbox2, FALSE, FALSE, 10);

    radiobutton=gtk_radio_button_new(rbgroup);
    rbgroup = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radiobutton));

    gtk_box_pack_start (GTK_BOX (hbox2), radiobutton, FALSE, FALSE, 10);

    label=gtk_label_new_with_mnemonic ( _("Record for maximum:  "));
    gtk_label_set_mnemonic_widget (GTK_LABEL (label),radiobutton);

    eventbox=gtk_event_box_new();
    gtk_container_add(GTK_CONTAINER(eventbox),label);
    g_signal_connect (GTK_OBJECT (eventbox), "button_press_event",
		      G_CALLBACK (label_act_toggle),
		      radiobutton);
    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
      gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
      gtk_widget_modify_bg (eventbox, GTK_STATE_NORMAL, &palette->normal_back);
    }
    gtk_box_pack_start (GTK_BOX (hbox2), eventbox, FALSE, FALSE, 10);

    gtk_widget_show_all (hbox2);

    spinbutton_adj = gtk_adjustment_new (hours, 0, hours>23?hours:23, 1, 1, 0);
    resaudw->hour_spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_adj), 1, 0);
    gtk_widget_show (resaudw->hour_spinbutton);
    gtk_box_pack_start (GTK_BOX (hbox), resaudw->hour_spinbutton, TRUE, TRUE, 0);
    
    label=gtk_label_new(_(" hours  "));
    gtk_widget_show(label);
    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    }
    gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);

    spinbutton_adj = gtk_adjustment_new (mins, 0, 59, 1, 1, 0);
    resaudw->minute_spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_adj), 1, 0);
    gtk_widget_show (resaudw->minute_spinbutton);
    gtk_box_pack_start (GTK_BOX (hbox), resaudw->minute_spinbutton, TRUE, TRUE, 0);

    label=gtk_label_new(_(" minutes  "));
    gtk_widget_show(label);
    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    }
    gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);

    spinbutton_adj = gtk_adjustment_new (secs, 0, 59, 1, 1, 0);
    resaudw->second_spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_adj), 1, 2);
    gtk_widget_show (resaudw->second_spinbutton);
    gtk_box_pack_start (GTK_BOX (hbox), resaudw->second_spinbutton, TRUE, TRUE, 0);

    label=gtk_label_new(_(" seconds  "));
    gtk_widget_show(label);
    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    }
    gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);

    hbox2 = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (dialog_vbox), hbox2, FALSE, FALSE, 10);

    resaudw->unlim_radiobutton=gtk_radio_button_new(rbgroup);
    rbgroup = gtk_radio_button_get_group (GTK_RADIO_BUTTON (resaudw->unlim_radiobutton));

    gtk_box_pack_start (GTK_BOX (hbox2), resaudw->unlim_radiobutton, FALSE, FALSE, 10);

    label=gtk_label_new_with_mnemonic ( _("Unlimited"));
    gtk_label_set_mnemonic_widget (GTK_LABEL (label),resaudw->unlim_radiobutton);

    eventbox=gtk_event_box_new();
    gtk_container_add(GTK_CONTAINER(eventbox),label);
    g_signal_connect (GTK_OBJECT (eventbox), "button_press_event",
		      G_CALLBACK (label_act_toggle),
		      resaudw->unlim_radiobutton);
    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
      gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
      gtk_widget_modify_bg (eventbox, GTK_STATE_NORMAL, &palette->normal_back);
    }
    gtk_box_pack_start (GTK_BOX (hbox2), eventbox, FALSE, FALSE, 10);

    gtk_widget_show_all (hbox2);

    g_signal_connect (GTK_OBJECT (radiobutton), "toggled",
                      G_CALLBACK (on_rb_audrec_time_toggled),
                      (gpointer)resaudw);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(resaudw->unlim_radiobutton),type==5||type>7);

    if (type<8) {
      hsep = gtk_hseparator_new ();
      gtk_widget_show (hsep);
      gtk_box_pack_start (GTK_BOX (dialog_vbox), hsep, TRUE, TRUE, 0);
      
      label=gtk_label_new(_("Click OK to begin recording, or Cancel to quit."));
      gtk_widget_show(label);
      if (palette->style&STYLE_1) {
	gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
      }
      gtk_box_pack_start (GTK_BOX (dialog_vbox), label, TRUE, TRUE, 0);
    }
  }


  if (type<3||type>4) {
    dialog_action_area15 = GTK_DIALOG (resaudw->dialog)->action_area;
    gtk_widget_show (dialog_action_area15);
    gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog_action_area15), GTK_BUTTONBOX_END);
    
    cancelbutton13 = gtk_button_new_from_stock ("gtk-cancel");
    gtk_widget_show (cancelbutton13);
    gtk_dialog_add_action_widget (GTK_DIALOG (resaudw->dialog), cancelbutton13, GTK_RESPONSE_CANCEL);
    GTK_WIDGET_SET_FLAGS (cancelbutton13, GTK_CAN_DEFAULT);
    
    okbutton12 = gtk_button_new_from_stock ("gtk-ok");
    gtk_widget_show (okbutton12);
    gtk_dialog_add_action_widget (GTK_DIALOG (resaudw->dialog), okbutton12, GTK_RESPONSE_OK);
    GTK_WIDGET_SET_FLAGS (okbutton12, GTK_CAN_DEFAULT);
    gtk_widget_grab_default (okbutton12);

    if (type<8) {
      g_signal_connect (GTK_OBJECT (cancelbutton13), "clicked",
			G_CALLBACK (on_cancel_button1_clicked),
			resaudw);
      
      if (type==1) {
	g_signal_connect (GTK_OBJECT (okbutton12), "clicked",
			  G_CALLBACK (on_resaudio_ok_clicked),
			  NULL);
      }
      if (type==2) {
	g_signal_connect (GTK_OBJECT (okbutton12), "clicked",
			  G_CALLBACK (on_ins_silence_details_clicked),
			  NULL);
      }
      if (type==5) {
	g_signal_connect (GTK_OBJECT (okbutton12), "clicked",
			  G_CALLBACK (on_recaudclip_ok_clicked),
			  GINT_TO_POINTER(0));
      }
      if (type==6||type==7) {
	g_signal_connect (GTK_OBJECT (okbutton12), "clicked",
			  G_CALLBACK (on_recaudclip_ok_clicked),
			  GINT_TO_POINTER(1));
      }
      
      g_signal_connect (GTK_OBJECT (resaudw->dialog), "delete_event",
			G_CALLBACK (return_true),
			NULL);
    }
  }
  else {
    if (type>2&&type<5) {
      g_signal_connect_after (GTK_OBJECT(resaudw->aud_checkbutton), "toggled",
			      G_CALLBACK (on_resaudw_achans_changed),
			      (gpointer)resaudw);
      on_resaudw_achans_changed(resaudw->aud_checkbutton,(gpointer)resaudw);
    }
  }

  g_list_free (channels);
  g_list_free (sampsize);
  g_list_free (rate);

  return resaudw;
}



void
create_new_pb_speed (gshort type)
{
  // type 1 = change speed
  // type 2 = resample


  GtkWidget *new_pb_speed;
  GtkWidget *dialog_vbox6;
  GtkWidget *vbox6;
  GtkWidget *hbox;
  GtkWidget *eventbox;
  GtkWidget *label;
  GtkWidget *alabel;
  GtkWidget *radiobutton1=NULL;
  GtkWidget *radiobutton2=NULL;
  GSList *rbgroup = NULL;
  GtkObject *spinbutton_pb_speed_adj;
  GtkWidget *spinbutton_pb_speed;
  GtkObject *spinbutton_pb_time_adj;
  GtkWidget *spinbutton_pb_time=NULL;
  GtkWidget *dialog_action_area6;
  GtkWidget *cancelbutton4;
  GtkWidget *change_pb_ok;
  GtkWidget *change_audio_speed;
  gchar label_text[256];

  new_pb_speed = gtk_dialog_new ();
  gtk_container_set_border_width (GTK_CONTAINER (new_pb_speed), 10);
  if (type==1) {
    gtk_window_set_title (GTK_WINDOW (new_pb_speed), _("LiVES: - Change playback speed"));
  }
  else if (type==2) {
    gtk_window_set_title (GTK_WINDOW (new_pb_speed), _("LiVES: - Resample Video"));
  }
  gtk_window_set_position (GTK_WINDOW (new_pb_speed), GTK_WIN_POS_CENTER);
  gtk_window_set_modal (GTK_WINDOW (new_pb_speed), TRUE);
  gtk_window_set_default_size (GTK_WINDOW (new_pb_speed), 300, 200);
  gtk_window_set_transient_for(GTK_WINDOW(new_pb_speed),GTK_WINDOW(mainw->LiVES));

  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(new_pb_speed, GTK_STATE_NORMAL, &palette->normal_back);
    gtk_dialog_set_has_separator(GTK_DIALOG(new_pb_speed),FALSE);
  }

  dialog_vbox6 = GTK_DIALOG (new_pb_speed)->vbox;

  vbox6 = gtk_vbox_new (FALSE, 10);
  gtk_box_pack_start (GTK_BOX (dialog_vbox6), vbox6, TRUE, TRUE, 20);

  if (type==1) {
    g_snprintf(label_text,256,_("\n\nCurrent playback speed is %.3f frames per second.\n\nPlease enter the desired playback speed\nin _frames per second"),cfile->fps);
  }
  else if (type==2) {
    g_snprintf(label_text,256,_("\n\nCurrent playback speed is %.3f frames per second.\n\nPlease enter the _resampled rate\nin frames per second"),cfile->fps);
  }
  label = gtk_label_new_with_mnemonic (label_text);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  spinbutton_pb_speed_adj = gtk_adjustment_new (cfile->fps, 1, FPS_MAX, 0.01, .1, 0.);
  spinbutton_pb_speed = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_pb_speed_adj), 1, 3);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), spinbutton_pb_speed);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox6), label, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (vbox6), hbox, FALSE, FALSE, 0);

  if (type==2) {
    add_fill_to_box(GTK_BOX(hbox));
    gtk_box_pack_start (GTK_BOX (hbox), spinbutton_pb_speed, TRUE, TRUE, 10);
    add_fill_to_box(GTK_BOX(hbox));
  }
  else {
    radiobutton1 = gtk_radio_button_new (NULL);
    rbgroup=gtk_radio_button_get_group (GTK_RADIO_BUTTON (radiobutton1));

    gtk_box_pack_start (GTK_BOX (hbox), radiobutton1, TRUE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (hbox), spinbutton_pb_speed, TRUE, TRUE, 10);

    label=gtk_label_new_with_mnemonic(_("OR enter the desired clip length in _seconds"));
    gtk_box_pack_start (GTK_BOX (vbox6), label, TRUE, TRUE, 0);
    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    }

    hbox = gtk_hbox_new (FALSE, 0);
    radiobutton2 = gtk_radio_button_new (rbgroup);
    gtk_box_pack_start (GTK_BOX (vbox6), hbox, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (hbox), radiobutton2, TRUE, FALSE, 0);


    spinbutton_pb_time_adj = gtk_adjustment_new ((gdouble)((gint)(cfile->frames/cfile->fps*100.))/100., 1./FPS_MAX, cfile->frames, 1., 10., 0.);
    spinbutton_pb_time = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_pb_time_adj), 1, 2);
    gtk_box_pack_start (GTK_BOX (hbox), spinbutton_pb_time, TRUE, TRUE, 10);
    gtk_entry_set_activates_default (GTK_ENTRY ((GtkEntry *)&(GTK_SPIN_BUTTON (spinbutton_pb_time)->entry)), TRUE);
    gtk_label_set_mnemonic_widget (GTK_LABEL (label), spinbutton_pb_time);

  }
  gtk_entry_set_activates_default (GTK_ENTRY ((GtkEntry *)&(GTK_SPIN_BUTTON (spinbutton_pb_speed)->entry)), TRUE);

  hbox = gtk_hbox_new (FALSE, 0);
  change_audio_speed = gtk_check_button_new();
  alabel=gtk_label_new_with_mnemonic (_("Change the _audio speed as well"));
  eventbox=gtk_event_box_new();

  gtk_box_pack_start (GTK_BOX (hbox), change_audio_speed, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), eventbox, FALSE, FALSE, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (alabel),change_audio_speed);
  gtk_container_add(GTK_CONTAINER(eventbox),alabel);
  g_signal_connect (GTK_OBJECT (eventbox), "button_press_event",
		    G_CALLBACK (label_act_toggle),
		    change_audio_speed);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(alabel, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg (eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  gtk_box_pack_start (GTK_BOX (dialog_vbox6), hbox, TRUE, TRUE, 20);


  dialog_action_area6 = GTK_DIALOG (new_pb_speed)->action_area;
  gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog_action_area6), GTK_BUTTONBOX_END);

  cancelbutton4 = gtk_button_new_from_stock ("gtk-cancel");
  gtk_dialog_add_action_widget (GTK_DIALOG (new_pb_speed), cancelbutton4, GTK_RESPONSE_CANCEL);
  GTK_WIDGET_SET_FLAGS (cancelbutton4, GTK_CAN_DEFAULT);

  change_pb_ok = gtk_button_new_from_stock ("gtk-ok");
  gtk_dialog_add_action_widget (GTK_DIALOG (new_pb_speed), change_pb_ok, GTK_RESPONSE_OK);
  GTK_WIDGET_SET_FLAGS (change_pb_ok, GTK_CAN_DEFAULT);
  gtk_widget_grab_default (change_pb_ok);
  gtk_widget_grab_focus(spinbutton_pb_speed);


  reorder_leave_back=FALSE;

  g_signal_connect (GTK_OBJECT (change_audio_speed), "toggled",
		    G_CALLBACK (on_boolean_toggled),
		    GINT_TO_POINTER (1));
  g_signal_connect (GTK_OBJECT (cancelbutton4), "clicked",
		    G_CALLBACK (on_cancel_button1_clicked),
		    NULL);
  if (type==1) {
    g_signal_connect (GTK_OBJECT (change_pb_ok), "clicked",
		      G_CALLBACK (on_change_speed_ok_clicked),
		      NULL);
  }
  else if (type==2) {
    g_signal_connect (GTK_OBJECT (change_pb_ok), "clicked",
		      G_CALLBACK (on_resample_vid_ok),
		      NULL);
  }
  g_signal_connect_after (GTK_OBJECT (spinbutton_pb_speed), "value_changed",
			  G_CALLBACK (on_spin_value_changed),
			  GINT_TO_POINTER (1));

  if (type==1) {
    g_signal_connect_after (GTK_OBJECT (spinbutton_pb_time), "value_changed",
			    G_CALLBACK (on_spin_value_changed),
			    GINT_TO_POINTER (2));
    g_signal_connect_after (GTK_OBJECT (spinbutton_pb_speed), "value_changed",
			    G_CALLBACK (widget_act_toggle),
			    radiobutton1);
    g_signal_connect_after (GTK_OBJECT (spinbutton_pb_time), "value_changed",
			    G_CALLBACK (widget_act_toggle),
			    radiobutton2);
    g_signal_connect (GTK_OBJECT (radiobutton2), "toggled",
		      G_CALLBACK (on_boolean_toggled),
		      GINT_TO_POINTER (2));
  }

  gtk_widget_show_all (new_pb_speed);
  if (type!=1||cfile->achans==0) {
    gtk_widget_hide (change_audio_speed);
    gtk_widget_hide (alabel);
  }

}



void
on_change_speed_activate                (GtkMenuItem     *menuitem,
					 gpointer         user_data)
{
  // change speed from the menu
  create_new_pb_speed(1);
  mainw->fx1_bool=mainw->fx2_bool=FALSE;
  mainw->fx1_val=cfile->fps;
}



void
on_change_speed_ok_clicked                (GtkButton *button,
					   gpointer         user_data)
{
  gdouble arate=cfile->arate/cfile->fps;
  gchar *msg;
  gboolean has_lmap_error=FALSE;

  // change playback rate
  if (button!=NULL) {
    gtk_widget_destroy(gtk_widget_get_toplevel(GTK_WIDGET(button)));
  }

  if (mainw->fx2_bool) {
    mainw->fx1_val=(gdouble)((gint)((gdouble)cfile->frames/mainw->fx2_val*1000.+.5))/1000.;
    if (mainw->fx1_val<1.) mainw->fx1_val=1.;
    if (mainw->fx1_val>FPS_MAX) mainw->fx1_val=FPS_MAX;
  }

  if (!(prefs->warning_mask&WARN_MASK_LAYOUT_DELETE_FRAMES)&&mainw->fx1_val>cfile->fps&&cfile->layout_map!=NULL) {
    gint new_frames=count_resampled_frames(cfile->frames,mainw->fx1_val,cfile->fps);
    if (layout_frame_is_affected(mainw->current_file,new_frames)) {
      if (!do_warning_dialog(_("\nSpeeding up the clip will cause missing frames in some multitrack layouts.\nAre you sure you wish to change the speed ?\n"))) return;
      add_lmap_error(LMAP_ERROR_DELETE_FRAMES,cfile->name,(gpointer)cfile->layout_map,mainw->current_file,new_frames,0.);
      has_lmap_error=TRUE;
    }
  }

  if (mainw->fx1_bool&&!(prefs->warning_mask&WARN_MASK_LAYOUT_DELETE_AUDIO)&&mainw->fx1_val>cfile->fps&&cfile->layout_map!=NULL) {
    gint new_frames=count_resampled_frames(cfile->frames,mainw->fx1_val,cfile->fps);
    if (layout_audio_is_affected(mainw->current_file,(new_frames-1.)/cfile->fps)) {
      if (!do_warning_dialog(_("\nSpeeding up the clip will cause missing audio in some multitrack layouts.\nAre you sure you wish to change the speed ?\n"))) return;
      add_lmap_error(LMAP_ERROR_DELETE_AUDIO,cfile->name,(gpointer)cfile->layout_map,mainw->current_file,0,(new_frames-1.)/cfile->fps);
      has_lmap_error=TRUE;
    }
  }

  if (!has_lmap_error&&!(prefs->warning_mask&WARN_MASK_LAYOUT_SHIFT_FRAMES)&&cfile->layout_map!=NULL&&layout_frame_is_affected(mainw->current_file,1)) {
    if (!do_warning_dialog(_("\nChanging the speed will cause frames to shift some multitrack layouts.\nAre you sure you wish to continue ?\n"))) {
      return;
    }
    add_lmap_error(LMAP_ERROR_SHIFT_FRAMES,cfile->name,(gpointer)cfile->layout_map,mainw->current_file,0,0.);
    has_lmap_error=TRUE;
  }

  if (mainw->fx1_bool&&!has_lmap_error&&!(prefs->warning_mask&WARN_MASK_LAYOUT_SHIFT_AUDIO)&&cfile->layout_map!=NULL&&layout_audio_is_affected(mainw->current_file,0.)) {
    if (!do_warning_dialog(_("\nChanging the speed will cause audio to shift some multitrack layouts.\nAre you sure you wish to continue ?\n"))) {
      return;
    }
    add_lmap_error(LMAP_ERROR_SHIFT_AUDIO,cfile->name,(gpointer)cfile->layout_map,mainw->current_file,0,0.);
    has_lmap_error=TRUE;
  }

  if (!has_lmap_error&&!(prefs->warning_mask&WARN_MASK_LAYOUT_ALTER_FRAMES)&&cfile->layout_map!=NULL&&layout_frame_is_affected(mainw->current_file,1)) {
    if (!do_layout_alter_frames_warning()) {
      return;
    }
    add_lmap_error(LMAP_ERROR_ALTER_FRAMES,cfile->name,(gpointer)cfile->layout_map,mainw->current_file,0,0.);
    has_lmap_error=TRUE;
  }

  if (mainw->fx1_bool&&!has_lmap_error&&!(prefs->warning_mask&WARN_MASK_LAYOUT_ALTER_AUDIO)&&cfile->layout_map!=NULL&&layout_audio_is_affected(mainw->current_file,0.)) {
    if (!do_layout_alter_audio_warning()) {
      return;
    }
    add_lmap_error(LMAP_ERROR_ALTER_AUDIO,cfile->name,(gpointer)cfile->layout_map,mainw->current_file,0,0.);
    has_lmap_error=TRUE;
  }

  if (button==NULL) {
    mainw->fx1_bool=!(cfile->undo1_int==cfile->arate);
    mainw->fx1_val=cfile->undo1_dbl;
  }

  set_undoable (_("Speed Change"),TRUE);
  cfile->undo1_dbl=cfile->fps;
  cfile->undo1_int=cfile->arate;
  cfile->undo_action=UNDO_CHANGE_SPEED;
  
  if (mainw->fx1_val==0.) mainw->fx1_val=1.;
  cfile->pb_fps=cfile->fps=mainw->fx1_val;
  if (mainw->fx1_bool) {
    cfile->arate=(gint)(arate*cfile->fps+.5);
    msg=g_strdup_printf (_ ("Changed playback speed to %.3f frames per second and audio to %d Hz.\n"),cfile->fps,cfile->arate);
  }
  else {
    msg=g_strdup_printf (_ ("Changed playback speed to %.3f frames per second.\n"),cfile->fps);
  }
  d_print (msg);
  g_free (msg);

  cfile->ratio_fps=FALSE;

  save_clip_value(mainw->current_file,CLIP_DETAILS_FPS,&cfile->fps);
  save_clip_value(mainw->current_file,CLIP_DETAILS_PB_FPS,&cfile->fps);
  save_clip_value(mainw->current_file,CLIP_DETAILS_PB_ARATE,&cfile->arate);

  switch_to_file(mainw->current_file,mainw->current_file);

  if (has_lmap_error) popup_lmap_errors(NULL,NULL);
}






gint
reorder_frames(void) {
  int new_frames=cfile->old_frames;
  int cur_frames=cfile->frames;
  gchar **array;
  gchar *com;

  if (reorder_width*reorder_height==0) com=g_strdup_printf("smogrify reorder %s %d 0 0 %d %d",cfile->handle,!mainw->endian,reorder_leave_back,cfile->frames);
  else com=g_strdup_printf("smogrify reorder %s %d %d %d 0 %d",cfile->handle,!mainw->endian,reorder_width,reorder_height,cfile->frames);
  cfile->frames=0;

  cfile->progress_start=1;
  cfile->progress_end=save_event_frames();  // we convert cfile->event_list to a block and save it
  if (cur_frames>cfile->progress_end) cfile->progress_end=cur_frames;

  cfile->next_event=NULL;
  if (cfile->event_list!=NULL) {
    if (cfile->event_list_back!=NULL) event_list_free (cfile->event_list_back);
    cfile->event_list_back=cfile->event_list;
    cfile->event_list=NULL;
  }

  unlink(cfile->info_file);

  dummyvar=system(com);
  if (cfile->undo_action==UNDO_RESAMPLE) {
    if (mainw->current_file>0) {
      cfile->nopreview=cfile->nokeep=TRUE;
      if (!do_progress_dialog(TRUE,TRUE,_ ("Resampling video"))) {
	cfile->nopreview=cfile->nokeep=FALSE;
	return cur_frames;
      }
      cfile->nopreview=cfile->nokeep=FALSE;
    }
    else {
      do_progress_dialog(TRUE,FALSE,_ ("Resampling clipboard video"));
    }
  }
  else {
    cfile->nopreview=cfile->nokeep=TRUE;
    if (!do_progress_dialog(TRUE,TRUE,_ ("Reordering frames"))) {
      cfile->nopreview=cfile->nokeep=FALSE;
      return cur_frames;
    }
    cfile->nopreview=cfile->nokeep=FALSE;
  }
  g_free(com);
  
  if (mainw->error) {
    do_error_dialog (_ ("\n\nLiVES was unable to reorder the frames."));
    deorder_frames(new_frames,FALSE);
    new_frames=-new_frames;
  }
  else {
    array=g_strsplit(mainw->msg,"|",2);
  
    new_frames=atoi(array[1]);
    g_strfreev(array);
    
    if (cfile->frames>new_frames) {
      new_frames=cfile->frames;
    }
  }
  return new_frames;
}



gint
deorder_frames(gint old_frames, gboolean leave_bak) {
  gchar *com;
  weed_timecode_t time_start;
  gint perf_start,perf_end;
  
  if (cfile->event_list!=NULL) return cfile->frames;

  cfile->event_list=cfile->event_list_back;
  cfile->event_list_back=NULL;

  if (cfile->event_list==NULL) {
    perf_start=1;
    perf_end=old_frames;
  }
  else {
    time_start=get_event_timecode (get_first_event(cfile->event_list));
    perf_start=(gint)(cfile->fps*(gdouble)time_start/U_SEC)+1;
    perf_end=perf_start+count_events (cfile->event_list,FALSE)-1;
  }
  com=g_strdup_printf("smogrify deorder %s %d %d %d %d",cfile->handle,perf_start,perf_end,old_frames,leave_bak);

  unlink(cfile->info_file);
  dummyvar=system(com);
  do_progress_dialog(TRUE,FALSE,_ ("Deordering frames"));
  g_free(com);


  if (cfile->frame_index_back!=NULL) {
    restore_frame_index_back(mainw->current_file);
  }

  return old_frames;
}


gboolean resample_clipboard(gdouble new_fps) {
  // resample the clipboard video - if we already did it once, it is
  // quicker the second time
  gchar *msg;
  gint current_file=mainw->current_file;

  if (clipboard->undo1_dbl==cfile->fps&&!prefs->conserve_space) {
    gint old_frames=clipboard->old_frames;
    gdouble old_fps=clipboard->fps;
    gchar *com;
    
    // we already resampled to this fps
    mainw->current_file=0;
    com=g_strdup_printf("smogrify redo %s %d %d",cfile->handle,1,cfile->old_frames);
    unlink(cfile->info_file);
    dummyvar=system(com);
    cfile->progress_start=1;
    cfile->progress_end=cfile->old_frames;
    // show a progress dialog, not cancellable
    do_progress_dialog(TRUE,FALSE,_ ("Resampling clipboard video"));
    g_free(com);
    cfile->frames=old_frames;
    cfile->undo_action=UNDO_RESAMPLE;
    cfile->fps=cfile->undo1_dbl;
    cfile->undo1_dbl=old_fps;
    msg=g_strdup_printf(_ ("Clipboard was resampled to %d frames.\n"),cfile->frames);
    d_print(msg);
    g_free(msg);
    mainw->current_file=current_file;
  }
  else {
    clipboard->undo1_dbl=cfile->fps;
    mainw->current_file=0;
    on_resample_vid_ok(NULL,NULL);
    mainw->current_file=current_file;
    if (clipboard->fps!=cfile->fps) {
      d_print (_ ("resampling error..."));
      mainw->error=1;
      return FALSE;
    }
  }
  clipboard->old_frames=clipboard->frames;
  return TRUE;
}
