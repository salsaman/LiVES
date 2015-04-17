// resample.c
// LiVES
// (c) G. Finch 2004 - 2015 <salsaman@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details


// functions for reordering, resampling video and audio


#ifdef HAVE_SYSTEM_WEED
#include <weed/weed.h>
#include <weed/weed-host.h>
#else
#include "../libweed/weed.h"
#include "../libweed/weed-host.h"
#endif

#include "main.h"
#include "resample.h"
#include "support.h"
#include "callbacks.h"
#include "effects.h"
#include "audio.h"
#include "cvirtual.h"

static int reorder_width=0;
static int reorder_height=0;
static boolean reorder_leave_back=FALSE;

/////////////////////////////////////////////////////

LIVES_INLINE weed_timecode_t q_gint64(weed_timecode_t in, double fps) {
  if (in>(weed_timecode_t)0) return ((weed_timecode_t)((long double)in/(long double)U_SEC*(long double)fps+(long double).5)/
                                       (long double)fps)*(weed_timecode_t)U_SECL; // quantise to frame timing
  if (in<(weed_timecode_t)0) return ((weed_timecode_t)((long double)in/(long double)U_SEC*(long double)fps-(long double).5)/
                                       (long double)fps)*(weed_timecode_t)U_SECL; // quantise to frame timing
  return (weed_timecode_t)0;
}

LIVES_INLINE weed_timecode_t q_gint64_floor(weed_timecode_t in, double fps) {
  if (in!=(weed_timecode_t)0) return ((weed_timecode_t)((long double)in/(long double)U_SEC*(long double)fps)/(long double)fps)*
                                       (weed_timecode_t)U_SECL; // quantise to frame timing
  return (weed_timecode_t)0;
}

LIVES_INLINE weed_timecode_t q_dbl(double in, double fps) {
  if (in>0.) return ((weed_timecode_t)((long double)in*(long double)fps+(long double).5)/(long double)fps)*
                      (weed_timecode_t)U_SECL; // quantise to frame timing
  if (in<0.) return ((weed_timecode_t)((long double)in*(long double)fps-(long double).5)/(long double)fps)*
                      (weed_timecode_t)U_SECL; // quantise to frame timing
  return (weed_timecode_t)0;
}


LIVES_INLINE int count_resampled_frames(int in_frames, double orig_fps, double resampled_fps) {
  int res_frames;
  if (resampled_fps<orig_fps) return ((res_frames=(int)((double)in_frames/orig_fps*resampled_fps))<1)?1:res_frames;
  else return ((res_frames=(int)((double)in_frames/orig_fps*resampled_fps+.49999))<1)?1:res_frames;
}

/////////////////////////////////////////////////////

boolean auto_resample_resize(int width,int height,double fps,int fps_num,int fps_denom, int arate,
                             int asigned, boolean swap_endian) {
  // do a block atomic: resample audio, then resample video/resize or joint resample/resize

  char *com,*msg=NULL;
  int current_file=mainw->current_file;
  boolean audio_resampled=FALSE;
  boolean video_resampled=FALSE;
  boolean video_resized=FALSE;
  boolean bad_header=FALSE;

  int frames=cfile->frames;

  reorder_leave_back=FALSE;

  if (asigned!=0||(arate>0&&arate!=cfile->arate)||swap_endian) {
    cfile->undo1_int=arate;
    cfile->undo2_int=cfile->achans;
    cfile->undo3_int=cfile->asampsize;
    cfile->undo1_uint=cfile->signed_endian;

    if (asigned==1&&(cfile->signed_endian&AFORM_UNSIGNED)==AFORM_UNSIGNED) cfile->undo1_uint^=AFORM_UNSIGNED;
    else if (asigned==2&&(cfile->signed_endian&AFORM_UNSIGNED)!=AFORM_UNSIGNED) cfile->undo1_uint|=AFORM_UNSIGNED;

    if (swap_endian) {
      if (cfile->signed_endian&AFORM_BIG_ENDIAN) cfile->undo1_uint^=AFORM_BIG_ENDIAN;
      else cfile->undo1_uint|=AFORM_BIG_ENDIAN;
    }

    on_resaudio_ok_clicked(NULL,NULL);
    if (mainw->error) return FALSE;
    audio_resampled=TRUE;
  }

  else {
    cfile->undo1_int=cfile->arate;
    cfile->undo2_int=cfile->achans;
    cfile->undo3_int=cfile->asampsize;
    cfile->undo4_int=cfile->arps;
    cfile->undo1_uint=cfile->signed_endian;
  }

  if (fps_denom>0) {
    fps=(fps_num*1.)/(fps_denom*1.);
  }
  if (fps>0.&&fps!=cfile->fps) {
    // FPS CHANGE...
    if ((width!=cfile->hsize||height!=cfile->vsize)&&width*height>0) {
      // CHANGING SIZE..
      if (fps>cfile->fps) {
        boolean rs_builtin;
        lives_rfx_t *resize_rfx;

        // we will have more frames...
        // ...do resize first
        if (mainw->fx_candidates[FX_CANDIDATE_RESIZER].delegate==-1||prefs->enc_letterbox) {
          cfile->ohsize=cfile->hsize;
          cfile->ovsize=cfile->vsize;

          if (cfile->clip_type==CLIP_TYPE_FILE) {
            cfile->fx_frame_pump=1;
          }

          if (!prefs->enc_letterbox) {
            com=lives_strdup_printf("%s resize_all \"%s\" %d %d %d \"%s\"",prefs->backend,
                                    cfile->handle,cfile->frames,width,height,
                                    get_image_ext_for_type(cfile->img_type));
            msg=lives_strdup_printf(_("Resizing frames 1 to %d"),cfile->frames);
          } else {
            int iwidth=cfile->hsize,iheight=cfile->vsize;
            calc_maxspect(width,height,&iwidth,&iheight);

            if (iwidth==cfile->hsize&&iheight==cfile->vsize) {
              iwidth=-iwidth;
              iheight=-iheight;
            }

            reorder_leave_back=TRUE;
            com=lives_strdup_printf("%s resize_all \"%s\" %d %d %d \"%s\" %d %d",prefs->backend,cfile->handle,
                                    cfile->frames,width,height,
                                    get_image_ext_for_type(cfile->img_type),iwidth,iheight);
            msg=lives_strdup_printf(_("Resizing/letterboxing frames 1 to %d"),cfile->frames);
          }

          cfile->progress_start=1;
          cfile->progress_end=cfile->frames;

          unlink(cfile->info_file);
          mainw->com_failed=FALSE;
          lives_system(com,FALSE);
          lives_free(com);

          if (mainw->com_failed) return FALSE;

          mainw->resizing=TRUE;
          rs_builtin=TRUE;
        } else {
          // use resize plugin
          int error;
          weed_plant_t *first_out;
          weed_plant_t *ctmpl;

          rs_builtin=FALSE;
          resize_rfx=mainw->fx_candidates[FX_CANDIDATE_RESIZER].rfx;
          first_out=get_enabled_channel((weed_plant_t *)resize_rfx->source,0,FALSE);
          ctmpl=weed_get_plantptr_value(first_out,"template",&error);
          weed_set_int_value(ctmpl,"host_width",width);
          weed_set_int_value(ctmpl,"host_height",height);
        }

        cfile->nokeep=TRUE;

        if ((rs_builtin&&!do_progress_dialog(TRUE,TRUE,msg))||(!rs_builtin&&!on_realfx_activate_inner(1,resize_rfx))) {
          mainw->resizing=FALSE;
          lives_free(msg);
          cfile->undo_action=UNDO_ATOMIC_RESAMPLE_RESIZE;

          cfile->hsize=width;
          cfile->vsize=height;

          cfile->undo1_dbl=cfile->fps;
          cfile->undo_start=1;
          cfile->undo_end=cfile->frames;
          cfile->fx_frame_pump=0;
          on_undo_activate(NULL,NULL);
          cfile->nokeep=FALSE;
          return FALSE;
        }
        lives_free(msg);

        mainw->resizing=FALSE;
        cfile->fx_frame_pump=0;

        cfile->hsize=width;
        cfile->vsize=height;

        save_clip_value(mainw->current_file,CLIP_DETAILS_WIDTH,&cfile->hsize);
        if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
        save_clip_value(mainw->current_file,CLIP_DETAILS_HEIGHT,&cfile->vsize);
        if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
        if (bad_header) do_header_write_error(mainw->current_file);

        cfile->undo1_dbl=fps;
        cfile->undo_start=1;
        cfile->undo_end=frames;

        // now resample

        // special "cheat" mode for LiVES
        reorder_leave_back=TRUE;

        reorder_width=width;
        reorder_height=height;

        mainw->resizing=TRUE;
        on_resample_vid_ok(NULL,NULL);

        reorder_leave_back=FALSE;
        cfile->undo_action=UNDO_ATOMIC_RESAMPLE_RESIZE;
        if (mainw->error) {
          on_undo_activate(NULL,NULL);
          return FALSE;
        }

        video_resized=TRUE;
        video_resampled=TRUE;
      } else {
        // fewer frames
        // do resample *with* resize
        cfile->ohsize=cfile->hsize;
        cfile->ovsize=cfile->vsize;
        cfile->undo1_dbl=fps;

        // another special "cheat" mode for LiVES
        reorder_width=width;
        reorder_height=height;

        mainw->resizing=TRUE;
        on_resample_vid_ok(NULL,NULL);
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
        if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
        save_clip_value(mainw->current_file,CLIP_DETAILS_HEIGHT,&cfile->vsize);
        if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
        if (bad_header) do_header_write_error(mainw->current_file);

        video_resampled=TRUE;
        video_resized=TRUE;
      }
    } else {
      //////////////////////////////////////////////////////////////////////////////////
      cfile->undo1_dbl=fps;
      cfile->undo_start=1;
      cfile->undo_end=cfile->frames;

      reorder_width=width;
      reorder_height=height;

      on_resample_vid_ok(NULL,NULL);

      reorder_width=reorder_height=0;
      reorder_leave_back=FALSE;

      if (audio_resampled) cfile->undo_action=UNDO_ATOMIC_RESAMPLE_RESIZE;
      if (mainw->error) {
        on_undo_activate(NULL,NULL);
        return FALSE;
      }
      //////////////////////////////////////////////////////////////////////////////////////
      video_resampled=TRUE;
    }
  } else {
    boolean rs_builtin=TRUE;
    lives_rfx_t *resize_rfx;
    // NO FPS CHANGE
    if ((width!=cfile->hsize||height!=cfile->vsize)&&width*height>0) {
      // no fps change - just a normal resize
      cfile->undo_start=1;
      cfile->undo_end=cfile->frames;

      if (mainw->fx_candidates[FX_CANDIDATE_RESIZER].delegate==-1||prefs->enc_letterbox) {
        // use builtin resize

        if (cfile->clip_type==CLIP_TYPE_FILE) {
          cfile->fx_frame_pump=1;
        }

        if (!prefs->enc_letterbox) {
          com=lives_strdup_printf("%s resize_all \"%s\" %d %d %d \"%s\"",prefs->backend,
                                  cfile->handle,cfile->frames,width,height,
                                  get_image_ext_for_type(cfile->img_type));
          msg=lives_strdup_printf(_("Resizing frames 1 to %d"),cfile->frames);
        } else {
          int iwidth=cfile->hsize,iheight=cfile->vsize;
          calc_maxspect(width,height,&iwidth,&iheight);

          if (iwidth==cfile->hsize&&iheight==cfile->vsize) {
            iwidth=-iwidth;
            iheight=-iheight;
          }

          com=lives_strdup_printf("%s resize_all \"%s\" %d %d %d \"%s\" %d %d",prefs->backend,
                                  cfile->handle,cfile->frames,width,height,
                                  get_image_ext_for_type(cfile->img_type),iwidth,iheight);
          msg=lives_strdup_printf(_("Resizing/letterboxing frames 1 to %d"),cfile->frames);
        }

        cfile->progress_start=1;
        cfile->progress_end=cfile->frames;

        cfile->ohsize=cfile->hsize;
        cfile->ovsize=cfile->vsize;

        cfile->undo1_dbl=cfile->fps;

        unlink(cfile->info_file);
        mainw->com_failed=FALSE;
        lives_system(com,FALSE);
        lives_free(com);

        if (mainw->com_failed) return FALSE;

        mainw->resizing=TRUE;
      } else {
        int error;
        weed_plant_t *first_out;
        weed_plant_t *ctmpl;

        rs_builtin=FALSE;
        resize_rfx=mainw->fx_candidates[FX_CANDIDATE_RESIZER].rfx;
        first_out=get_enabled_channel((weed_plant_t *)resize_rfx->source,0,FALSE);
        ctmpl=weed_get_plantptr_value(first_out,"template",&error);
        weed_set_int_value(ctmpl,"host_width",width);
        weed_set_int_value(ctmpl,"host_height",height);
      }

      cfile->nokeep=TRUE;

      if ((rs_builtin&&!do_progress_dialog(TRUE,TRUE,msg))||(!rs_builtin&&!on_realfx_activate_inner(1,resize_rfx))) {
        mainw->resizing=FALSE;
        if (msg!=NULL) lives_free(msg);
        cfile->hsize=width;
        cfile->vsize=height;
        if (audio_resampled) cfile->undo_action=UNDO_ATOMIC_RESAMPLE_RESIZE;
        else {
          cfile->undo_action=UNDO_RESIZABLE;
          set_undoable(_("Resize"),TRUE);
        }
        cfile->fx_frame_pump=0;
        on_undo_activate(NULL,NULL);
        cfile->nokeep=FALSE;
        return FALSE;
      }

      cfile->hsize=width;
      cfile->vsize=height;

      lives_free(msg);
      mainw->resizing=FALSE;
      cfile->fx_frame_pump=0;

      save_clip_value(mainw->current_file,CLIP_DETAILS_WIDTH,&cfile->hsize);
      if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
      save_clip_value(mainw->current_file,CLIP_DETAILS_HEIGHT,&cfile->vsize);
      if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
      if (bad_header) do_header_write_error(mainw->current_file);

      if (audio_resampled) cfile->undo_action=UNDO_ATOMIC_RESAMPLE_RESIZE;
      else {
        cfile->undo_action=UNDO_RESIZABLE;
        set_undoable(_("Resize"),TRUE);
      }
      video_resized=TRUE;
      switch_to_file((mainw->current_file=0),current_file);
    }
  }

  if (cfile->undo_action==UNDO_ATOMIC_RESAMPLE_RESIZE) {
    // just in case we missed anything...

    set_undoable(_("Resample/Resize"),TRUE);
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

WARN_UNUSED weed_plant_t *
quantise_events(weed_plant_t *in_list, double qfps, boolean allow_gap) {
  // new style event system, now we quantise from event_list_t *in_list to *out_list with period tl/U_SEC

  // the timecode of the midpoint of our last frame events will match as near as possible the old length
  // but out_list will have regular period of tl microseconds

  // for optimal resampling we compare the midpoints of each frame

  // only FRAME events are moved, other event types retain the same timecodes

  weed_plant_t *out_list;
  weed_plant_t *last_audio_event=NULL;
  weed_plant_t *event,*last_frame_event,*penultimate_frame_event,*next_frame_event,*shortcut=NULL;
  weed_timecode_t out_tc=0,in_tc=-1,nearest_tc=LONG_MAX;
  boolean is_first=TRUE;
  weed_timecode_t tc_end,tp;
  int *out_clips=NULL,*out_frames=NULL;
  int numframes=0;
  weed_timecode_t tl=q_dbl(1./qfps,qfps);
  int error;
  boolean needs_audio=FALSE,add_audio=FALSE;
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

  tc_end=get_event_timecode(last_frame_event);

  penultimate_frame_event=get_prev_frame_event(last_frame_event);

  // tp is the duration of the last frame
  if (penultimate_frame_event!=NULL) {
    tp=get_event_timecode(penultimate_frame_event);
    tp=tc_end-tp;
  } else {
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

    while (event!=NULL&&!WEED_EVENT_IS_FRAME(event)) {
      // copy non-FRAME events
      if (event_copy_and_insert(event,out_list)==NULL) {
        do_memory_error_dialog();
        event_list_free(out_list);
        return NULL;
      }
      is_first=FALSE;
      event=get_next_event(event);
    }

    // now we are dealing with a FRAME event
    if (event!=NULL) {
      if (last_audio_event!=event&&WEED_EVENT_IS_AUDIO_FRAME(event)) {
        last_audio_event=event;
        needs_audio=TRUE;
        if (aclips!=NULL) lives_free(aclips);
        if (aseeks!=NULL) lives_free(aseeks);
        num_aclips=weed_leaf_num_elements(event,"audio_clips");
        aclips=weed_get_int_array(event,"audio_clips",&error);
        aseeks=weed_get_double_array(event,"audio_seeks",&error);
      }
      in_tc=get_event_timecode(event);
      if ((next_frame_event=get_next_frame_event(event))!=NULL) {
        tp=get_event_timecode(next_frame_event);
        tp-=in_tc;
      } else {
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
      if (out_clips!=NULL) lives_free(out_clips);
      if (out_frames!=NULL) lives_free(out_frames);

      numframes=weed_leaf_num_elements(event,"clips");
      out_clips=weed_get_int_array(event,"clips",&error);
      out_frames=weed_get_int_array(event,"frames",&error);
      if (last_audio_event==event&&needs_audio) add_audio=TRUE;
      if (error==WEED_ERROR_MEMORY_ALLOCATION) {
        do_memory_error_dialog();
        event_list_free(out_list);
        return NULL;
      }

      nearest_tc=(out_tc+tl)-in_tc;
      if (event!=NULL) event=get_next_event(event);
      allow_gap=FALSE;
    } else {
      // event is after slot, or we reached the end of in_list

      //  in some cases we allow a gap before writing our first FRAME out event
      if (!(is_first&&allow_gap)) {
        if (in_tc-(out_tc+tl)<nearest_tc) {
          if (event!=NULL) {
            if (out_clips!=NULL) lives_free(out_clips);
            if (out_frames!=NULL) lives_free(out_frames);

            numframes=weed_leaf_num_elements(event,"clips");
            out_clips=weed_get_int_array(event,"clips",&error);
            out_frames=weed_get_int_array(event,"frames",&error);
            if (last_audio_event==event&&needs_audio) add_audio=TRUE;
            if (error==WEED_ERROR_MEMORY_ALLOCATION) {
              do_memory_error_dialog();
              event_list_free(out_list);
              return NULL;
            }
          }
        }
        if (out_clips!=NULL) {
          if (insert_frame_event_at(out_list,out_tc,numframes,out_clips,out_frames,&shortcut)==NULL) {
            do_memory_error_dialog();
            event_list_free(out_list);
            return NULL;
          }
          if (add_audio) {
            weed_set_int_array(shortcut,"audio_clips",num_aclips,aclips);
            weed_set_double_array(shortcut,"audio_seeks",num_aclips,aseeks);
            needs_audio=add_audio=FALSE;
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

  if (event!=NULL&&WEED_EVENT_IS_FRAME(event)) event=get_next_event(event);

  while (event!=NULL&&!WEED_EVENT_IS_FRAME(event)) {
    // copy remaining non-FRAME events
    if (event_copy_and_insert(event,out_list)==NULL) {
      do_memory_error_dialog();
      event_list_free(out_list);
      return NULL;
    }
    event=get_next_event(event);
  }

  if (get_first_frame_event(out_list)==NULL) {
    // make sure we have at least one frame
    if ((event=get_last_frame_event(in_list))!=NULL) {

      if (out_clips!=NULL) lives_free(out_clips);
      if (out_frames!=NULL) lives_free(out_frames);

      numframes=weed_leaf_num_elements(event,"clips");
      out_clips=weed_get_int_array(event,"clips",&error);
      out_frames=weed_get_int_array(event,"frames",&error);

      if (insert_frame_event_at(out_list,0.,numframes,out_clips,out_frames,NULL)==NULL) {
        do_memory_error_dialog();
        event_list_free(out_list);
        return NULL;
      }
      if (get_first_event(out_list)==NULL) weed_set_voidptr_value(out_list,"first",get_last_event(out_list));
    }
  }

  if (out_clips!=NULL) lives_free(out_clips);
  if (out_frames!=NULL) lives_free(out_frames);

  if (aclips!=NULL) lives_free(aclips);
  if (aseeks!=NULL) lives_free(aseeks);

  return out_list;
}


//////////////////////////////////////////////////////////////////


static void on_reorder_activate(int rwidth, int rheight) {
  char *msg;
  boolean has_lmap_error=FALSE;

  if (!(prefs->warning_mask&WARN_MASK_LAYOUT_ALTER_FRAMES)&&(mainw->xlays=layout_frame_is_affected(mainw->current_file,1))!=NULL) {
    if (!do_layout_alter_frames_warning()) {
      lives_list_free_strings(mainw->xlays);
      lives_list_free(mainw->xlays);
      mainw->xlays=NULL;
      return;
    }
    add_lmap_error(LMAP_ERROR_ALTER_FRAMES,cfile->name,(livespointer)cfile->layout_map,mainw->current_file,0,0.,cfile->stored_layout_frame>0);
    has_lmap_error=TRUE;
    lives_list_free_strings(mainw->xlays);
    lives_list_free(mainw->xlays);
    mainw->xlays=NULL;
  }

  cfile->old_frames=cfile->frames;

  //  we  do the reorder in reorder_frames()
  // this will clear event_list and set it in event_list_back
  if ((cfile->frames=reorder_frames(rwidth,rheight))<0) {
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
  if (mainw->com_failed||mainw->write_failed) do_header_write_error(mainw->current_file);

  switch_to_file(mainw->current_file,mainw->current_file);
  if (mainw->current_file>0) {
    d_print_done();
    msg=lives_strdup_printf(_("Length of video is now %d frames.\n"),cfile->frames);
  } else {
    msg=lives_strdup_printf(_("Clipboard was resampled to %d frames.\n"),cfile->frames);
  }

  d_print(msg);
  lives_free(msg);

  if (has_lmap_error) popup_lmap_errors(NULL,NULL);

  if (mainw->sl_undo_mem!=NULL&&cfile->stored_layout_frame!=0) {
    // need to invalidate undo/redo stack, in case file was used in some layout undo
    stored_event_list_free_undos();
  }


}




void on_resample_audio_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  // show the playback rate - real audio rate is cfile->arps
  mainw->fx1_val=cfile->arate;
  mainw->fx2_val=cfile->achans;
  mainw->fx3_val=cfile->asampsize;
  mainw->fx4_val=cfile->signed_endian;
  resaudw=create_resaudw(1,NULL,NULL);
  lives_widget_show(resaudw->dialog);
}



void on_resaudio_ok_clicked(LiVESButton *button, LiVESEntry *entry) {
  char *com;

  boolean noswitch=mainw->noswitch;
  boolean has_lmap_error=FALSE;

  int arate,achans,asampsize,arps;
  int asigned=1,aendian=1;
  int cur_signed,cur_endian;

  if (button!=NULL) {
    arps=arate=(int)atoi(lives_entry_get_text(LIVES_ENTRY(resaudw->entry_arate)));
    achans=(int)atoi(lives_entry_get_text(LIVES_ENTRY(resaudw->entry_achans)));
    asampsize=(int)atoi(lives_entry_get_text(LIVES_ENTRY(resaudw->entry_asamps)));
    if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(resaudw->rb_unsigned))) {
      asigned=0;
    }
    if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(resaudw->rb_bigend))) {
      aendian=0;
    }

    lives_widget_destroy(resaudw->dialog);
    mainw->noswitch=TRUE;
    lives_widget_context_update();
    mainw->noswitch=noswitch;
    lives_free(resaudw);

    if (arate<=0) {
      do_error_dialog(_("\n\nNew rate must be greater than 0\n"));
      return;
    }
  } else {
    // called from on_redo or other places
    arate=arps=cfile->undo1_int;
    achans=cfile->undo2_int;
    asampsize=cfile->undo3_int;
    asigned=!(cfile->undo1_uint&AFORM_UNSIGNED);
    aendian=!(cfile->undo1_uint&AFORM_BIG_ENDIAN);
  }

  if (!(prefs->warning_mask&WARN_MASK_LAYOUT_ALTER_AUDIO)&&(mainw->xlays=layout_audio_is_affected
      (mainw->current_file,0.))) {
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

  // store old values for undo/redo
  cfile->undo1_int=cfile->arate;
  cfile->undo2_int=cfile->achans;
  cfile->undo3_int=cfile->asampsize;
  cfile->undo4_int=cfile->arps;
  cfile->undo1_uint=cfile->signed_endian;

  cur_signed=!(cfile->signed_endian&AFORM_UNSIGNED);
  cur_endian=!(cfile->signed_endian&AFORM_BIG_ENDIAN);

  if (!(arate==cfile->arate&&arps==cfile->arps&&achans==cfile->achans&&asampsize==cfile->asampsize&&
        asigned==cur_signed&&aendian==cur_endian)) {
    if (cfile->arps!=cfile->arate) {
      double audio_stretch=(double)cfile->arps/(double)cfile->arate;
      // pb rate != real rate - stretch to pb rate and resample
      unlink(cfile->info_file);
      com=lives_strdup_printf("%s resample_audio \"%s\" %d %d %d %d %d %d %d %d %d %d %.4f",prefs->backend,
                              cfile->handle,cfile->arps,
                              cfile->achans,cfile->asampsize,cur_signed,cur_endian,arps,cfile->achans,cfile->asampsize,
                              cur_signed,cur_endian,audio_stretch);
      mainw->com_failed=FALSE;
      lives_system(com,FALSE);
      if (mainw->com_failed) return;
      do_progress_dialog(TRUE,FALSE,_("Resampling audio"));  // TODO - allow cancel ??
      lives_free(com);
      cfile->arate=cfile->arps=arps;
    } else {
      unlink(cfile->info_file);
      com=lives_strdup_printf("%s resample_audio \"%s\" %d %d %d %d %d %d %d %d %d %d",prefs->backend,
                              cfile->handle,cfile->arps,
                              cfile->achans,cfile->asampsize,cur_signed,cur_endian,arps,achans,asampsize,asigned,aendian);
      mainw->com_failed=FALSE;
      mainw->cancelled=CANCEL_NONE;
      mainw->error=FALSE;
      unlink(cfile->info_file);
      lives_system(com,FALSE);
      check_backend_return(cfile);
      if (mainw->com_failed) return;
      do_progress_dialog(TRUE,FALSE,_("Resampling audio"));
      lives_free(com);

    }
  }

  cfile->arate=arate;
  cfile->achans=achans;
  cfile->asampsize=asampsize;
  cfile->arps=arps;
  cfile->signed_endian=get_signed_endian(asigned, aendian);
  cfile->changed=TRUE;

  cfile->undo_action=UNDO_AUDIO_RESAMPLE;
  mainw->error=FALSE;
  reget_afilesize(mainw->current_file);

  if (cfile->afilesize==0l) {
    do_error_dialog(_("LiVES was unable to resample the audio as requested.\n"));
    on_undo_activate(NULL,NULL);
    set_undoable(_("Resample Audio"),FALSE);
    mainw->error=TRUE;
    return;
  }
  set_undoable(_("Resample Audio"),!prefs->conserve_space);

  save_clip_values(mainw->current_file);

  switch_to_file(mainw->current_file,mainw->current_file);

  d_print("");  // force printing of switch message

  d_print(_("Audio was resampled to %d Hz, %d channels, %d bit"),arate,achans,asampsize);

  if (cur_signed!=asigned) {
    if (asigned==1) {
      d_print(_(", signed"));
    } else {
      d_print(_(", unsigned"));
    }
  }
  if (cur_endian!=aendian) {
    if (aendian==1) {
      d_print(_(", little-endian"));
    } else {
      d_print(_(", big-endian"));
    }
  }
  d_print("\n");
  if (has_lmap_error) popup_lmap_errors(NULL,NULL);

  if (mainw->sl_undo_mem!=NULL&&cfile->stored_layout_audio>0.) {
    // need to invalidate undo/redo stack, in case file was used in some layout undo
    stored_event_list_free_undos();
  }

}




static void on_resaudw_achans_changed(LiVESWidget *widg, livespointer user_data) {
  _resaudw *resaudw=(_resaudw *)user_data;
  char *tmp;

  if (!lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(widg))) {
    lives_widget_set_sensitive(resaudw->rb_signed,FALSE);
    lives_widget_set_sensitive(resaudw->rb_unsigned,FALSE);
    lives_widget_set_sensitive(resaudw->rb_bigend,FALSE);
    lives_widget_set_sensitive(resaudw->rb_littleend,FALSE);
    lives_widget_set_sensitive(resaudw->entry_arate,FALSE);
    lives_widget_set_sensitive(resaudw->entry_asamps,FALSE);
    lives_widget_set_sensitive(resaudw->entry_achans,FALSE);
    if (prefsw!=NULL) {
      lives_widget_set_sensitive(prefsw->pertrack_checkbutton,FALSE);
      lives_widget_set_sensitive(prefsw->backaudio_checkbutton,FALSE);
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->pertrack_checkbutton),FALSE);
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->backaudio_checkbutton),FALSE);
    } else if (rdet!=NULL) {
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(rdet->pertrack_checkbutton),FALSE);
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(rdet->backaudio_checkbutton),FALSE);
      lives_widget_set_sensitive(rdet->pertrack_checkbutton,FALSE);
      lives_widget_set_sensitive(rdet->backaudio_checkbutton,FALSE);
    }
  } else {
    if (atoi(lives_entry_get_text(LIVES_ENTRY(resaudw->entry_asamps)))!=8) {
      lives_widget_set_sensitive(resaudw->rb_bigend,TRUE);
      lives_widget_set_sensitive(resaudw->rb_littleend,TRUE);
    }
    lives_widget_set_sensitive(resaudw->entry_arate,TRUE);
    lives_widget_set_sensitive(resaudw->entry_asamps,TRUE);
    lives_widget_set_sensitive(resaudw->entry_achans,TRUE);
    if (prefsw!=NULL) {
      lives_widget_set_sensitive(prefsw->pertrack_checkbutton,TRUE);
      lives_widget_set_sensitive(prefsw->backaudio_checkbutton,TRUE);
    }
    if (rdet!=NULL) {
      lives_widget_set_sensitive(rdet->backaudio_checkbutton,TRUE);
      lives_widget_set_sensitive(rdet->pertrack_checkbutton,TRUE);
    }

    tmp=lives_strdup_printf("%d",DEFAULT_AUDIO_CHANS);
    lives_entry_set_text(LIVES_ENTRY(resaudw->entry_achans),tmp);
    lives_free(tmp);

  }
}




void
on_resaudw_asamps_changed(LiVESWidget *irrelevant, livespointer rubbish) {
  if (atoi(lives_entry_get_text(LIVES_ENTRY(resaudw->entry_asamps)))==8) {
    lives_widget_set_sensitive(resaudw->rb_bigend,FALSE);
    lives_widget_set_sensitive(resaudw->rb_littleend,FALSE);
    lives_widget_set_sensitive(resaudw->rb_signed,FALSE);
    lives_widget_set_sensitive(resaudw->rb_unsigned,TRUE);
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(resaudw->rb_unsigned),TRUE);
  } else {
    lives_widget_set_sensitive(resaudw->rb_bigend,TRUE);
    lives_widget_set_sensitive(resaudw->rb_littleend,TRUE);
    if (atoi(lives_entry_get_text(LIVES_ENTRY(resaudw->entry_asamps)))==16) {
      lives_widget_set_sensitive(resaudw->rb_signed,TRUE);
      lives_widget_set_sensitive(resaudw->rb_unsigned,FALSE);
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(resaudw->rb_signed),TRUE);
    }
  }
}



void
on_resample_video_activate(LiVESMenuItem     *menuitem,
                           livespointer         user_data) {
  // change speed from the menu
  create_new_pb_speed(2);
  mainw->fx1_val=cfile->fps;
}



void on_resample_vid_ok(LiVESButton *button, LiVESEntry *entry) {
  int i;
  int old_frames;
  int ostart=cfile->start;
  int oend=cfile->end;
  double oundo1_dbl=cfile->undo1_dbl;
  char *msg;
  weed_timecode_t in_time=0;
  double old_fps=cfile->fps;
  boolean ratio_fps;
  boolean bad_header=FALSE;
  weed_plant_t *real_back_list=NULL;
  weed_plant_t *new_event_list=NULL;

  mainw->error=FALSE;

  if (button!=NULL) {
    lives_general_button_clicked(button,NULL);
    if (mainw->fx1_val==0.) mainw->fx1_val=1.;
  } else {
    mainw->fx1_val=cfile->undo1_dbl;
  }

  if (mainw->current_file<0||cfile->frames==0) return;

  if (mainw->fx1_val==cfile->fps&&cfile->event_list==NULL) return;

  real_back_list=cfile->event_list;

  if (cfile->event_list==NULL) {
    for (i=1; i<=cfile->frames; i++) {
      new_event_list=append_frame_event(new_event_list,in_time,1,&(mainw->current_file),&i);
      if (new_event_list==NULL) {
        do_memory_error_dialog();
        return;
      }
      in_time+=(weed_timecode_t)(1./cfile->fps*U_SEC+.5);
    }
    cfile->event_list=new_event_list;
  }
  cfile->undo1_dbl=cfile->fps;

  if (cfile->event_list_back!=NULL) event_list_free(cfile->event_list_back);
  cfile->event_list_back=cfile->event_list;

  //QUANTISE
  new_event_list=quantise_events(cfile->event_list_back,mainw->fx1_val,real_back_list!=NULL);
  if (new_event_list==NULL) return; // memory error
  cfile->event_list=new_event_list;

  if (real_back_list==NULL) event_list_free(cfile->event_list_back);
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
    msg=lives_strdup_printf(_("Resampling video at %.8f frames per second..."),mainw->fx1_val);
  } else {
    msg=lives_strdup_printf(_("Resampling video at %.3f frames per second..."),mainw->fx1_val);
  }
  if (mainw->current_file>0) {
    d_print(msg);
  }
  lives_free(msg);

  old_frames=cfile->frames;

  // must set these before calling reorder
  cfile->start=(int)((cfile->start-1.)/old_fps*mainw->fx1_val+1.);
  if ((cfile->end=(int)((cfile->end*mainw->fx1_val)/old_fps+.49999))<cfile->start) cfile->end=cfile->start;

  cfile->undo_action=UNDO_RESAMPLE;
  // REORDER
  // this calls reorder_frames, which sets event_list_back==event_list, and clears event_list
  on_reorder_activate(reorder_width,reorder_height);

  if (cfile->frames<=0||mainw->cancelled!=CANCEL_NONE) {
    // reordering error...
    cfile->event_list=real_back_list;
    if (cfile->event_list_back!=NULL) event_list_free(cfile->event_list_back);
    cfile->event_list_back=NULL;
    cfile->frames=old_frames;
    cfile->start=ostart;
    cfile->end=oend;
    load_start_image(cfile->start);
    load_end_image(cfile->end);
    cfile->undo1_dbl=oundo1_dbl;
    sensitize();
    mainw->error=TRUE;
    if (cfile->frames<0) do_error_dialog(_("Reordering error !\n"));
    return;
  }

  if (cfile->event_list_back!=NULL) event_list_free(cfile->event_list_back);
  cfile->event_list_back=real_back_list;

  cfile->ratio_fps=ratio_fps;
  cfile->pb_fps=cfile->fps=mainw->fx1_val;
  cfile->old_frames=old_frames;

  set_undoable(_("Resample"),TRUE);

  save_clip_value(mainw->current_file,CLIP_DETAILS_FPS,&cfile->fps);
  if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
  save_clip_value(mainw->current_file,CLIP_DETAILS_PB_FPS,&cfile->fps);
  if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
  if (bad_header) do_header_write_error(mainw->current_file);

  switch_to_file(mainw->current_file,mainw->current_file);

}



///////// GUI stuff /////////////////////////////////////////////////////



_resaudw *create_resaudw(short type, render_details *rdet, LiVESWidget *top_vbox) {
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
  // type 11 == rte audio gen as rfx

  LiVESWidget *dialog_vbox=NULL;
  LiVESWidget *vboxx;
  LiVESWidget *vbox2;
  LiVESWidget *frame;
  LiVESWidget *label_aud=NULL;
  LiVESWidget *combo_entry2;
  LiVESWidget *combo_entry3;
  LiVESWidget *combo_entry1;
  LiVESWidget *vseparator;
  LiVESWidget *radiobutton_u1;
  LiVESWidget *radiobutton_s1;
  LiVESWidget *vbox;
  LiVESWidget *radiobutton_b1;
  LiVESWidget *radiobutton_l1;
  LiVESWidget *combo4;
  LiVESWidget *combo5;
  LiVESWidget *combo6;
  LiVESWidget *dialog_action_area;
  LiVESWidget *cancelbutton;
  LiVESWidget *okbutton;
  LiVESWidget *label;
  LiVESWidget *hseparator;
  LiVESWidget *radiobutton;
  LiVESWidget *hbox;
  LiVESWidget *hbox2;

  LiVESAccelGroup *accel_group=NULL;

  LiVESSList *s1_group=NULL;
  LiVESSList *e1_group=NULL;
  LiVESSList *s2_group=NULL;
  LiVESSList *e2_group=NULL;
  LiVESSList *rbgroup = NULL;

  LiVESList *channels = NULL;
  LiVESList *sampsize = NULL;
  LiVESList *rate = NULL;

  double secs=0.;

  char *tmp;

  int hours=0,mins=0;
  int aendian;

  boolean chans_fixed=FALSE;
  boolean is_8bit;

  _resaudw *resaudw=(_resaudw *)(lives_malloc(sizeof(_resaudw)));

  if (type==10) {
    if (mainw->multitrack!=NULL) chans_fixed=TRUE; // TODO *
    type=3;
  }

  if (type>5&&type!=11&&mainw->rec_end_time!=-1.) {
    hours=(int)(mainw->rec_end_time/3600.);
    mins=(int)((mainw->rec_end_time-(hours*3600.))/60.);
    secs=mainw->rec_end_time-hours*3600.-mins*60.;
  }

  channels = lives_list_append(channels, (livespointer)"1");
  channels = lives_list_append(channels, (livespointer)"2");

  sampsize = lives_list_append(sampsize, (livespointer)"8");
  sampsize = lives_list_append(sampsize, (livespointer)"16");

  rate = lives_list_append(rate, (livespointer)"5512");
  rate = lives_list_append(rate, (livespointer)"8000");
  rate = lives_list_append(rate, (livespointer)"11025");
  rate = lives_list_append(rate, (livespointer)"22050");
  rate = lives_list_append(rate, (livespointer)"32000");
  rate = lives_list_append(rate, (livespointer)"44100");
  rate = lives_list_append(rate, (livespointer)"48000");
  rate = lives_list_append(rate, (livespointer)"88200");
  rate = lives_list_append(rate, (livespointer)"96000");
  rate = lives_list_append(rate, (livespointer)"128000");

  if (type<3||type>4) {
    char *title=NULL;

    if (type==1) {
      title=lives_strdup(_("LiVES: - Resample Audio"));
    } else if (type==2) {
      title=lives_strdup(_("LiVES: - Insert Silence"));
    } else if (type==5||type==11||type==6||type==7) {
      title=lives_strdup(_("LiVES: - New Clip Audio"));
    } else if (type==9||type==8) {
      title=lives_strdup(_("LiVES: - External Clip Settings"));
    }

    resaudw->dialog = lives_standard_dialog_new(title,FALSE,-1,-1);
    lives_free(title);

    accel_group = LIVES_ACCEL_GROUP(lives_accel_group_new());
    lives_window_add_accel_group(LIVES_WINDOW(resaudw->dialog), accel_group);

    if (prefs->show_gui) {
      lives_window_set_transient_for(LIVES_WINDOW(resaudw->dialog),LIVES_WINDOW(mainw->LiVES));
    }

    dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(resaudw->dialog));

    vboxx = lives_vbox_new(FALSE, 0);

    lives_box_pack_start(LIVES_BOX(dialog_vbox), vboxx, TRUE, TRUE, 0);
  } else vboxx=top_vbox;

  if (type==1) {
    frame = lives_frame_new(NULL);

    lives_box_pack_start(LIVES_BOX(vboxx), frame, TRUE, TRUE, 0);

    if (palette->style&STYLE_1) {
      lives_widget_set_bg_color(frame, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
    }

    hbox2 = lives_hbox_new(FALSE, 0);
    lives_widget_show(hbox2);
    lives_container_add(LIVES_CONTAINER(frame), hbox2);
    lives_container_set_border_width(LIVES_CONTAINER(hbox2), widget_opts.packing_width);

    tmp=lives_strdup_printf("%d",(int)mainw->fx1_val);

    combo_entry2 = lives_standard_entry_new(_("Rate (Hz) "),FALSE,tmp,10,6,LIVES_BOX(hbox2),NULL);
    lives_free(tmp);

    lives_editable_set_editable(LIVES_EDITABLE(combo_entry2), FALSE);
    lives_widget_set_can_focus(combo_entry2, FALSE);

    tmp=lives_strdup_printf("%d",(int)mainw->fx2_val);
    combo_entry3 = lives_standard_entry_new(_("Channels"),FALSE,tmp,6,2,LIVES_BOX(hbox2),NULL);
    lives_free(tmp);

    lives_editable_set_editable(LIVES_EDITABLE(combo_entry3), FALSE);
    lives_widget_set_can_focus(combo_entry3, FALSE);

    tmp=lives_strdup_printf("%d",(int)mainw->fx3_val);
    combo_entry1 = lives_standard_entry_new(_("Sample Size "),FALSE,tmp,6,2,LIVES_BOX(hbox2),NULL);
    lives_free(tmp);

    lives_editable_set_editable(LIVES_EDITABLE(combo_entry1), FALSE);
    lives_widget_set_can_focus(combo_entry1, FALSE);

    vseparator = lives_vseparator_new();
    lives_widget_show(vseparator);
    lives_box_pack_start(LIVES_BOX(hbox2), vseparator, FALSE, FALSE, widget_opts.packing_width);

    vbox = lives_vbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(hbox2), vbox, FALSE, FALSE, 0);

    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

    radiobutton_s1 = lives_standard_radio_button_new(_("Signed"),FALSE,s1_group,LIVES_BOX(hbox),NULL);
    s1_group = lives_radio_button_get_group(LIVES_RADIO_BUTTON(radiobutton_s1));

    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

    radiobutton_u1 = lives_standard_radio_button_new(_("Unsigned"),FALSE,s1_group,LIVES_BOX(hbox),NULL);

    aendian=mainw->fx4_val;

    if (aendian&AFORM_UNSIGNED) {
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(radiobutton_u1), TRUE);
    } else {
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(radiobutton_s1), TRUE);
    }

    lives_widget_set_sensitive(radiobutton_u1, FALSE);
    lives_widget_set_sensitive(radiobutton_s1, FALSE);

    vseparator = lives_vseparator_new();
    lives_box_pack_start(LIVES_BOX(hbox2), vseparator, FALSE, FALSE, widget_opts.packing_width);

    vbox = lives_vbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(hbox2), vbox, FALSE, FALSE, 0);

    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

    radiobutton_l1 = lives_standard_radio_button_new(_("Little Endian"),FALSE,e1_group,LIVES_BOX(hbox),NULL);
    e1_group = lives_radio_button_get_group(LIVES_RADIO_BUTTON(radiobutton_l1));

    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

    radiobutton_b1 = lives_standard_radio_button_new(_("Big Endian"),FALSE,e1_group,LIVES_BOX(hbox),NULL);

    if (aendian&AFORM_BIG_ENDIAN) {
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(radiobutton_b1), TRUE);
    } else {
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(radiobutton_l1), TRUE);
    }

    lives_widget_set_sensitive(radiobutton_b1, FALSE);
    lives_widget_set_sensitive(radiobutton_l1, FALSE);

    label = lives_standard_label_new(_("Current"));

    lives_frame_set_label_widget(LIVES_FRAME(frame), label);

  }

  resaudw->aud_checkbutton = NULL;

  if (type<9||type==11) {
    frame = lives_frame_new(NULL);

    if (type==4) lives_box_pack_start(LIVES_BOX(vboxx), frame, FALSE, FALSE, widget_opts.packing_height);
    else lives_box_pack_start(LIVES_BOX(vboxx), frame, TRUE, TRUE, 0);

    if (palette->style&STYLE_1) {
      lives_widget_set_bg_color(frame, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
    }

    vbox2 = lives_vbox_new(FALSE, 0);
    lives_container_add(LIVES_CONTAINER(frame), vbox2);

    if (type>2&&type<5) {
      resaudw->aud_hbox = lives_hbox_new(FALSE, 0);
      lives_box_pack_start(LIVES_BOX(vbox2), resaudw->aud_hbox, FALSE, FALSE, 0);

      resaudw->aud_checkbutton = lives_standard_check_button_new(_("_Enable audio"),TRUE,LIVES_BOX(resaudw->aud_hbox),NULL);

      if (rdet!=NULL) lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(resaudw->aud_checkbutton), rdet->achans>0);
      else lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(resaudw->aud_checkbutton), prefs->mt_def_achans>0);
    } else resaudw->aud_checkbutton = lives_check_button_new();

    hbox2 = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(vbox2), hbox2, FALSE, FALSE, widget_opts.packing_height);
    lives_container_set_border_width(LIVES_CONTAINER(hbox2), widget_opts.border_width);

    vbox = lives_vbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(hbox2), vbox, FALSE, FALSE, widget_opts.packing_width);

    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);
    lives_container_set_border_width(LIVES_CONTAINER(hbox), widget_opts.border_width);

    combo4 = lives_standard_combo_new(_("Rate (Hz) "),type>=3&&type!=11,rate,LIVES_BOX(hbox),NULL);

    resaudw->entry_arate = lives_combo_get_entry(LIVES_COMBO(combo4));

    lives_entry_set_width_chars(LIVES_ENTRY(resaudw->entry_arate), 6);
    if (type==7) lives_widget_set_sensitive(combo4,FALSE);

    if (type<3||(type>4&&type<8)||type==11) tmp=lives_strdup_printf("%d",(int)mainw->fx1_val);
    else if (type==8) tmp=lives_strdup_printf("%d",DEFAULT_AUDIO_RATE);
    else if (type==3) tmp=lives_strdup_printf("%d",rdet->arate);
    else tmp=lives_strdup_printf("%d",prefs->mt_def_arate);
    lives_entry_set_text(LIVES_ENTRY(resaudw->entry_arate),tmp);
    lives_free(tmp);

    combo5 = lives_standard_combo_new((type>=3&&type!=11?(_("_Channels")):(_("Channels"))),type>=3&&type!=11,
                                      channels,LIVES_BOX(hbox),NULL);


    if (type==7) lives_widget_set_sensitive(combo5,FALSE);

    resaudw->entry_achans = lives_combo_get_entry(LIVES_COMBO(combo5));
    lives_entry_set_width_chars(LIVES_ENTRY(resaudw->entry_achans), 2);

    if (type<3||(type>4&&type<8)||type==11) tmp=lives_strdup_printf("%d",(int)mainw->fx2_val);
    else if (type==8) tmp=lives_strdup_printf("%d",DEFAULT_AUDIO_CHANS);
    else if (type==3) tmp=lives_strdup_printf("%d",rdet->achans);
    else tmp=lives_strdup_printf("%d",prefs->mt_def_achans==0?DEFAULT_AUDIO_CHANS:prefs->mt_def_achans);
    lives_entry_set_text(LIVES_ENTRY(resaudw->entry_achans),tmp);
    lives_free(tmp);

    if (chans_fixed) {
      lives_widget_set_sensitive(resaudw->entry_achans,FALSE);
      lives_widget_set_sensitive(combo5,FALSE);
    }


    combo6 = lives_standard_combo_new((type>=3&&type!=11?(_("_Sample Size")):(_("Sample Size"))),type>=3&&type!=11,
                                      sampsize,LIVES_BOX(hbox),NULL);

    if (type==7) lives_widget_set_sensitive(combo6,FALSE);

    resaudw->entry_asamps = lives_combo_get_entry(LIVES_COMBO(combo6));
    lives_entry_set_max_length(LIVES_ENTRY(resaudw->entry_asamps), 2);
    lives_editable_set_editable(LIVES_EDITABLE(resaudw->entry_asamps), FALSE);
    lives_entry_set_width_chars(LIVES_ENTRY(resaudw->entry_asamps), 2);

    if (type<3||(type>4&&type<8)||type==11) tmp=lives_strdup_printf("%d",(int)mainw->fx3_val);
    else if (type==8) tmp=lives_strdup_printf("%d",DEFAULT_AUDIO_SAMPS);
    else if (type==3) tmp=lives_strdup_printf("%d",rdet->asamps);
    else tmp=lives_strdup_printf("%d",prefs->mt_def_asamps);
    lives_entry_set_text(LIVES_ENTRY(resaudw->entry_asamps),tmp);

    if (!strcmp(tmp,"8")) is_8bit=TRUE;
    else is_8bit=FALSE;

    lives_free(tmp);

    vseparator = lives_vseparator_new();
    lives_widget_show(vseparator);
    if (type!=4) lives_box_pack_start(LIVES_BOX(hbox2), vseparator, FALSE, FALSE, widget_opts.packing_width);

    vbox = lives_vbox_new(FALSE, 0);
    if (type!=4) lives_box_pack_start(LIVES_BOX(hbox2), vbox, FALSE, FALSE, 0);

    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

    resaudw->rb_signed = lives_standard_radio_button_new(_("Signed"),FALSE,s2_group,LIVES_BOX(hbox),NULL);
    s2_group = lives_radio_button_get_group(LIVES_RADIO_BUTTON(resaudw->rb_signed));

    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(resaudw->rb_signed), TRUE);
    if (type==7||is_8bit) lives_widget_set_sensitive(resaudw->rb_signed,FALSE);


    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

    resaudw->rb_unsigned = lives_standard_radio_button_new(_("Unsigned"),FALSE,s2_group,LIVES_BOX(hbox),NULL);

    if (type==7||!is_8bit) lives_widget_set_sensitive(resaudw->rb_unsigned,FALSE);

    if (type<3||(type>4&&type<8)||type==11) aendian=mainw->fx4_val;
    else if (type==8) aendian=DEFAULT_AUDIO_SIGNED16|((capable->byte_order==LIVES_BIG_ENDIAN)?AFORM_BIG_ENDIAN:0);
    else if (type==3) aendian=rdet->aendian;
    else aendian=prefs->mt_def_signed_endian;

    if (aendian&AFORM_UNSIGNED) {
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(resaudw->rb_unsigned), TRUE);
    } else {
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(resaudw->rb_signed), TRUE);
    }

    vseparator = lives_vseparator_new();
    lives_box_pack_start(LIVES_BOX(hbox2), vseparator, FALSE, FALSE, widget_opts.packing_width);

    vbox = lives_vbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(hbox2), vbox, FALSE, FALSE, 0);

    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

    resaudw->rb_littleend = lives_standard_radio_button_new(_("Little Endian"),FALSE,e2_group,LIVES_BOX(hbox),NULL);
    e2_group = lives_radio_button_get_group(LIVES_RADIO_BUTTON(resaudw->rb_littleend));

    if (type==7) lives_widget_set_sensitive(resaudw->rb_littleend,FALSE);

    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

    resaudw->rb_bigend = lives_standard_radio_button_new(_("Big Endian"),FALSE,e2_group,LIVES_BOX(hbox),NULL);

    if (type==7) lives_widget_set_sensitive(resaudw->rb_bigend,FALSE);

    if (aendian&AFORM_BIG_ENDIAN) {
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(resaudw->rb_bigend), TRUE);
    } else {
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(resaudw->rb_littleend), TRUE);
    }

    if (!strcmp(lives_entry_get_text(LIVES_ENTRY(resaudw->entry_asamps)),"8")) {
      lives_widget_set_sensitive(resaudw->rb_littleend, FALSE);
      lives_widget_set_sensitive(resaudw->rb_bigend, FALSE);
    }

    lives_signal_connect(LIVES_GUI_OBJECT(resaudw->entry_asamps), LIVES_WIDGET_CHANGED_SIGNAL,
                         LIVES_GUI_CALLBACK(on_resaudw_asamps_changed),
                         NULL);

    if (type>=3&&type!=11) label_aud = lives_standard_label_new(_("Audio"));
    else label_aud = lives_standard_label_new(_("New"));

    if (type==3&&type!=11&&palette->style&STYLE_1) lives_widget_set_bg_color(frame, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);

    lives_frame_set_label_widget(LIVES_FRAME(frame), label_aud);

  }

  if (type>7&&type!=11) {
    frame = lives_frame_new(NULL);
    lives_widget_show(frame);
    lives_box_pack_start(LIVES_BOX(vboxx), frame, TRUE, TRUE, 0);

    if (palette->style&STYLE_1) {
      lives_widget_set_bg_color(frame, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
    }

    hbox = lives_hbox_new(FALSE, 0);
    lives_widget_show(hbox);
    lives_container_add(LIVES_CONTAINER(frame), hbox);
    lives_container_set_border_width(LIVES_CONTAINER(hbox), widget_opts.border_width);


    resaudw->fps_spinbutton = lives_standard_spin_button_new(_("_Frames Per Second "),TRUE,
                              prefs->default_fps,1.,FPS_MAX,1.,1.,3,LIVES_BOX(hbox),NULL);


    label = lives_standard_label_new(_("Video"));

    lives_frame_set_label_widget(LIVES_FRAME(frame), label);
  }

  if (type>4&&type!=11) {
    lives_box_set_spacing(LIVES_BOX(dialog_vbox),widget_opts.packing_height*3);

    hbox = lives_hbox_new(FALSE, 0);
    lives_widget_show(hbox);
    lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, TRUE, TRUE, widget_opts.packing_height);

    if (type!=6&&type!=7) {

      radiobutton=lives_standard_radio_button_new(_("Record for maximum:  "),FALSE,rbgroup,LIVES_BOX(hbox),NULL);
      rbgroup = lives_radio_button_get_group(LIVES_RADIO_BUTTON(radiobutton));

      resaudw->hour_spinbutton = lives_standard_spin_button_new(_(" hours  "),FALSE,hours,
                                 0.,hours>23?hours:23,1.,1.,0,LIVES_BOX(hbox),NULL);

      resaudw->minute_spinbutton = lives_standard_spin_button_new(_(" minutes  "),FALSE,mins,0.,59.,1.,1.,0,LIVES_BOX(hbox),NULL);

      resaudw->second_spinbutton = lives_standard_spin_button_new(_(" seconds  "),FALSE,secs,0.,59.,1.,1.,0,LIVES_BOX(hbox),NULL);


      hbox = lives_hbox_new(FALSE, 0);
      lives_widget_show(hbox);
      lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, TRUE, TRUE, widget_opts.packing_height);

      resaudw->unlim_radiobutton=lives_standard_radio_button_new(_("Unlimited"),FALSE,rbgroup,LIVES_BOX(hbox),NULL);
      rbgroup = lives_radio_button_get_group(LIVES_RADIO_BUTTON(resaudw->unlim_radiobutton));

      lives_signal_connect(LIVES_GUI_OBJECT(radiobutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                           LIVES_GUI_CALLBACK(on_rb_audrec_time_toggled),
                           (livespointer)resaudw);

      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(resaudw->unlim_radiobutton),(type==5||type>7)&&type!=11);

    }

    if (type<8||type==11) {
      hseparator = lives_hseparator_new();
      lives_widget_show(hseparator);
      lives_box_pack_start(LIVES_BOX(dialog_vbox), hseparator, TRUE, TRUE, 0);

      label=lives_standard_label_new(_("Click OK to begin recording, or Cancel to quit."));

      lives_box_pack_start(LIVES_BOX(dialog_vbox), label, TRUE, TRUE, 0);
    }
  }


  if (type<3||type>4) {
    dialog_action_area = lives_dialog_get_action_area(LIVES_DIALOG(resaudw->dialog));
    lives_widget_show(dialog_action_area);
    lives_button_box_set_layout(LIVES_BUTTON_BOX(dialog_action_area), LIVES_BUTTONBOX_END);

    cancelbutton = lives_button_new_from_stock(LIVES_STOCK_CANCEL);

    lives_dialog_add_action_widget(LIVES_DIALOG(resaudw->dialog), cancelbutton, LIVES_RESPONSE_CANCEL);
    lives_widget_set_can_focus_and_default(cancelbutton);

    if (accel_group!=NULL) lives_widget_add_accelerator(cancelbutton, LIVES_WIDGET_CLICKED_SIGNAL, accel_group,
          LIVES_KEY_Escape, (LiVESXModifierType)0, (LiVESAccelFlags)0);


    okbutton = lives_button_new_from_stock(LIVES_STOCK_OK);

    lives_dialog_add_action_widget(LIVES_DIALOG(resaudw->dialog), okbutton, LIVES_RESPONSE_OK);
    lives_widget_set_can_focus_and_default(okbutton);
    lives_widget_grab_default(okbutton);

    if (type<8||type==11) {
      lives_signal_connect(LIVES_GUI_OBJECT(cancelbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                           LIVES_GUI_CALLBACK(lives_general_button_clicked),
                           resaudw);

      if (type==1) {
        lives_signal_connect(LIVES_GUI_OBJECT(okbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                             LIVES_GUI_CALLBACK(on_resaudio_ok_clicked),
                             NULL);
      } else if (type==2||type==11) {
        lives_signal_connect(LIVES_GUI_OBJECT(okbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                             LIVES_GUI_CALLBACK(on_ins_silence_details_clicked),
                             NULL);
      } else if (type==5) {
        lives_signal_connect(LIVES_GUI_OBJECT(okbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                             LIVES_GUI_CALLBACK(on_recaudclip_ok_clicked),
                             LIVES_INT_TO_POINTER(0));
      } else if (type==6||type==7) {
        lives_signal_connect(LIVES_GUI_OBJECT(okbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                             LIVES_GUI_CALLBACK(on_recaudclip_ok_clicked),
                             LIVES_INT_TO_POINTER(1));
      }

    }

    lives_widget_show_all(resaudw->dialog);

  } else {
    if (type>2&&type<5) {
      lives_signal_connect_after(LIVES_GUI_OBJECT(resaudw->aud_checkbutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                                 LIVES_GUI_CALLBACK(on_resaudw_achans_changed),
                                 (livespointer)resaudw);
      on_resaudw_achans_changed(resaudw->aud_checkbutton,(livespointer)resaudw);
    }
  }

  lives_widget_show_all(vboxx);

  if (type==2) lives_widget_hide(label_aud);

  lives_list_free(channels);
  lives_list_free(sampsize);
  lives_list_free(rate);

  return resaudw;
}



void create_new_pb_speed(short type) {
  // type 1 = change speed
  // type 2 = resample

  LiVESWidget *new_pb_speed;
  LiVESWidget *dialog_vbox;
  LiVESWidget *vbox;
  LiVESWidget *hbox;
  LiVESWidget *ca_hbox;
  LiVESWidget *label;
  LiVESWidget *label2;
  LiVESWidget *radiobutton1=NULL;
  LiVESWidget *radiobutton2=NULL;
  LiVESWidget *spinbutton_pb_speed;
  LiVESWidget *spinbutton_pb_time=NULL;
  LiVESWidget *dialog_action_area;
  LiVESWidget *cancelbutton;
  LiVESWidget *change_pb_ok;
  LiVESWidget *change_audio_speed;

  LiVESAccelGroup *accel_group;

  LiVESSList *rbgroup = NULL;

  char label_text[256];

  char *title=NULL;

  if (type==1) {
    title=lives_strdup(_("LiVES: - Change playback speed"));
  } else {
    title=lives_strdup(_("LiVES: - Resample Video"));
  }

  new_pb_speed = lives_standard_dialog_new(title,FALSE,-1,-1);
  lives_free(title);

  lives_container_set_border_width(LIVES_CONTAINER(new_pb_speed), widget_opts.border_width*2);

  accel_group = LIVES_ACCEL_GROUP(lives_accel_group_new());
  lives_window_add_accel_group(LIVES_WINDOW(new_pb_speed), accel_group);

  if (prefs->show_gui) {
    lives_window_set_transient_for(LIVES_WINDOW(new_pb_speed),LIVES_WINDOW(mainw->LiVES));
  }

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(new_pb_speed));

  vbox = lives_vbox_new(FALSE, 0);

  lives_box_pack_start(LIVES_BOX(dialog_vbox), vbox, TRUE, TRUE, widget_opts.packing_height*2);

  if (type==1) {
    lives_snprintf(label_text,256,
                   _("\n\nCurrent playback speed is %.3f frames per second.\n\nPlease enter the desired playback speed\nin _frames per second"),
                   cfile->fps);
  } else if (type==2) {
    lives_snprintf(label_text,256,
                   _("\n\nCurrent playback speed is %.3f frames per second.\n\nPlease enter the _resampled rate\nin frames per second"),
                   cfile->fps);
  }

  label=lives_standard_label_new_with_mnemonic(label_text,NULL);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), label, FALSE, FALSE, widget_opts.packing_height);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

  if (type==2) {
    add_fill_to_box(LIVES_BOX(hbox));
    spinbutton_pb_speed = lives_standard_spin_button_new(NULL,FALSE,cfile->fps,1.,FPS_MAX,.01,.1,3,LIVES_BOX(hbox),NULL);
    add_fill_to_box(LIVES_BOX(hbox));
  } else {
    radiobutton1 = lives_standard_radio_button_new(NULL,FALSE,rbgroup,LIVES_BOX(hbox),NULL);
    rbgroup=lives_radio_button_get_group(LIVES_RADIO_BUTTON(radiobutton1));


    spinbutton_pb_speed = lives_standard_spin_button_new(NULL,FALSE,cfile->fps,1.,FPS_MAX,.01,.1,3,LIVES_BOX(hbox),NULL);

    label2=lives_standard_label_new_with_mnemonic(_("OR enter the desired clip length in _seconds"),NULL);
    lives_box_pack_start(LIVES_BOX(vbox), label2, TRUE, TRUE, widget_opts.packing_height);

    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

    radiobutton2 = lives_standard_radio_button_new(NULL,FALSE,rbgroup,LIVES_BOX(hbox),NULL);


    spinbutton_pb_time = lives_standard_spin_button_new(NULL,FALSE,
                         (double)((int)(cfile->frames/cfile->fps*100.))/100.,
                         1./FPS_MAX, cfile->frames, 1., 10., 2, LIVES_BOX(hbox),NULL);

    lives_label_set_mnemonic_widget(LIVES_LABEL(label2), spinbutton_pb_time);

  }

  lives_label_set_mnemonic_widget(LIVES_LABEL(label), spinbutton_pb_speed);


  ca_hbox = lives_hbox_new(FALSE, 0);
  change_audio_speed = lives_standard_check_button_new
                       (_("Change the _audio speed as well"),TRUE,LIVES_BOX(ca_hbox),NULL);

  lives_box_pack_start(LIVES_BOX(vbox), ca_hbox, TRUE, TRUE, widget_opts.packing_height);


  dialog_action_area = lives_dialog_get_action_area(LIVES_DIALOG(new_pb_speed));
  lives_button_box_set_layout(LIVES_BUTTON_BOX(dialog_action_area), LIVES_BUTTONBOX_END);

  cancelbutton = lives_button_new_from_stock(LIVES_STOCK_CANCEL);
  lives_dialog_add_action_widget(LIVES_DIALOG(new_pb_speed), cancelbutton, LIVES_RESPONSE_CANCEL);
  lives_widget_set_can_focus(cancelbutton,TRUE);

  lives_widget_add_accelerator(cancelbutton, LIVES_WIDGET_CLICKED_SIGNAL, accel_group,
                               LIVES_KEY_Escape, (LiVESXModifierType)0, (LiVESAccelFlags)0);

  change_pb_ok = lives_button_new_from_stock(LIVES_STOCK_OK);
  lives_dialog_add_action_widget(LIVES_DIALOG(new_pb_speed), change_pb_ok, LIVES_RESPONSE_OK);
  lives_widget_set_can_focus_and_default(change_pb_ok);
  lives_widget_grab_default(change_pb_ok);
  lives_widget_grab_focus(spinbutton_pb_speed);


  reorder_leave_back=FALSE;

  lives_signal_connect(LIVES_GUI_OBJECT(change_audio_speed), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(on_boolean_toggled),
                       &mainw->fx1_bool);
  lives_signal_connect(LIVES_GUI_OBJECT(cancelbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                       LIVES_GUI_CALLBACK(lives_general_button_clicked),
                       NULL);
  if (type==1) {
    lives_signal_connect(LIVES_GUI_OBJECT(change_pb_ok), LIVES_WIDGET_CLICKED_SIGNAL,
                         LIVES_GUI_CALLBACK(on_change_speed_ok_clicked),
                         NULL);
  } else if (type==2) {
    lives_signal_connect(LIVES_GUI_OBJECT(change_pb_ok), LIVES_WIDGET_CLICKED_SIGNAL,
                         LIVES_GUI_CALLBACK(on_resample_vid_ok),
                         NULL);
  }
  lives_signal_connect_after(LIVES_GUI_OBJECT(spinbutton_pb_speed), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                             LIVES_GUI_CALLBACK(on_spin_value_changed),
                             LIVES_INT_TO_POINTER(1));

  if (type==1) {
    lives_signal_connect_after(LIVES_GUI_OBJECT(spinbutton_pb_time), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                               LIVES_GUI_CALLBACK(on_spin_value_changed),
                               LIVES_INT_TO_POINTER(2));
    lives_signal_connect_after(LIVES_GUI_OBJECT(spinbutton_pb_speed), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                               LIVES_GUI_CALLBACK(widget_act_toggle),
                               radiobutton1);
    lives_signal_connect_after(LIVES_GUI_OBJECT(spinbutton_pb_time), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                               LIVES_GUI_CALLBACK(widget_act_toggle),
                               radiobutton2);
    lives_signal_connect(LIVES_GUI_OBJECT(radiobutton2), LIVES_WIDGET_TOGGLED_SIGNAL,
                         LIVES_GUI_CALLBACK(on_boolean_toggled),
                         &mainw->fx2_bool);
  }

  lives_widget_show_all(new_pb_speed);

  if (type!=1||cfile->achans==0) {
    lives_widget_hide(ca_hbox);
  }

}



void
on_change_speed_activate(LiVESMenuItem     *menuitem,
                         livespointer         user_data) {
  // change speed from the menu
  create_new_pb_speed(1);
  mainw->fx1_bool=mainw->fx2_bool=FALSE;
  mainw->fx1_val=cfile->fps;
}



void on_change_speed_ok_clicked(LiVESButton *button, livespointer user_data) {
  double arate=cfile->arate/cfile->fps;
  char *msg;
  boolean has_lmap_error=FALSE;
  boolean bad_header=FALSE;

  // change playback rate
  if (button!=NULL) {
    lives_general_button_clicked(button,NULL);
  }

  if (mainw->fx2_bool) {
    mainw->fx1_val=(double)((int)((double)cfile->frames/mainw->fx2_val*1000.+.5))/1000.;
    if (mainw->fx1_val<1.) mainw->fx1_val=1.;
    if (mainw->fx1_val>FPS_MAX) mainw->fx1_val=FPS_MAX;
  }

  if (!(prefs->warning_mask&WARN_MASK_LAYOUT_DELETE_FRAMES)&&mainw->fx1_val>cfile->fps) {
    int new_frames=count_resampled_frames(cfile->frames,mainw->fx1_val,cfile->fps);
    if ((mainw->xlays=layout_frame_is_affected(mainw->current_file,new_frames))!=NULL) {
      if (!do_warning_dialog(
            _("\nSpeeding up the clip will cause missing frames in some multitrack layouts.\nAre you sure you wish to change the speed ?\n"))) {
        lives_list_free_strings(mainw->xlays);
        lives_list_free(mainw->xlays);
        mainw->xlays=NULL;
        return;
      }
      add_lmap_error(LMAP_ERROR_DELETE_FRAMES,cfile->name,(livespointer)cfile->layout_map,mainw->current_file,new_frames,0.,
                     new_frames<=count_resampled_frames(cfile->stored_layout_frame,cfile->stored_layout_fps,cfile->fps));
      has_lmap_error=TRUE;
      lives_list_free_strings(mainw->xlays);
      lives_list_free(mainw->xlays);
      mainw->xlays=NULL;
    }
  }

  if (mainw->fx1_bool&&!(prefs->warning_mask&WARN_MASK_LAYOUT_DELETE_AUDIO)&&mainw->fx1_val>cfile->fps) {
    int new_frames=count_resampled_frames(cfile->frames,mainw->fx1_val,cfile->fps);
    if ((mainw->xlays=layout_audio_is_affected(mainw->current_file,(new_frames-1.)/cfile->fps))!=NULL) {
      if (!do_warning_dialog(
            _("\nSpeeding up the clip will cause missing audio in some multitrack layouts.\nAre you sure you wish to change the speed ?\n"))) {
        lives_list_free_strings(mainw->xlays);
        lives_list_free(mainw->xlays);
        mainw->xlays=NULL;
        return;
      }
      add_lmap_error(LMAP_ERROR_DELETE_AUDIO,cfile->name,(livespointer)cfile->layout_map,mainw->current_file,0,(new_frames-1.)/cfile->fps,
                     (new_frames-1.)/cfile->fps<cfile->stored_layout_audio);
      has_lmap_error=TRUE;
      lives_list_free_strings(mainw->xlays);
      lives_list_free(mainw->xlays);
      mainw->xlays=NULL;
    }
  }

  if (!has_lmap_error&&!(prefs->warning_mask&WARN_MASK_LAYOUT_SHIFT_FRAMES)&&
      (mainw->xlays=layout_frame_is_affected(mainw->current_file,1))!=NULL) {
    if (!do_warning_dialog(
          _("\nChanging the speed will cause frames to shift in some multitrack layouts.\nAre you sure you wish to continue ?\n"))) {
      lives_list_free_strings(mainw->xlays);
      lives_list_free(mainw->xlays);
      mainw->xlays=NULL;
      return;
    }
    add_lmap_error(LMAP_ERROR_SHIFT_FRAMES,cfile->name,(livespointer)cfile->layout_map,mainw->current_file,0,0.,cfile->stored_layout_frame>0);
    has_lmap_error=TRUE;
    lives_list_free_strings(mainw->xlays);
    lives_list_free(mainw->xlays);
    mainw->xlays=NULL;
  }

  if (mainw->fx1_bool&&!has_lmap_error&&!(prefs->warning_mask&WARN_MASK_LAYOUT_SHIFT_AUDIO)&&
      (mainw->xlays=layout_audio_is_affected(mainw->current_file,0.))!=NULL) {
    if (!do_warning_dialog(
          _("\nChanging the speed will cause audio to shift in some multitrack layouts.\nAre you sure you wish to continue ?\n"))) {
      lives_list_free_strings(mainw->xlays);
      lives_list_free(mainw->xlays);
      mainw->xlays=NULL;
      return;
    }
    add_lmap_error(LMAP_ERROR_SHIFT_AUDIO,cfile->name,(livespointer)cfile->layout_map,mainw->current_file,0,0.,cfile->stored_layout_audio>0.);
    has_lmap_error=TRUE;
    lives_list_free_strings(mainw->xlays);
    lives_list_free(mainw->xlays);
    mainw->xlays=NULL;
  }

  if (!has_lmap_error&&!(prefs->warning_mask&WARN_MASK_LAYOUT_ALTER_FRAMES)&&
      (mainw->xlays=layout_frame_is_affected(mainw->current_file,1))!=NULL) {
    if (!do_layout_alter_frames_warning()) {
      lives_list_free_strings(mainw->xlays);
      lives_list_free(mainw->xlays);
      mainw->xlays=NULL;
      return;
    }
    add_lmap_error(LMAP_ERROR_ALTER_FRAMES,cfile->name,(livespointer)cfile->layout_map,mainw->current_file,0,0.,cfile->stored_layout_frame>0);
    has_lmap_error=TRUE;
    lives_list_free_strings(mainw->xlays);
    lives_list_free(mainw->xlays);
    mainw->xlays=NULL;
  }

  if (mainw->fx1_bool&&!has_lmap_error&&!(prefs->warning_mask&WARN_MASK_LAYOUT_ALTER_AUDIO)&&
      (mainw->xlays=layout_audio_is_affected(mainw->current_file,0.))!=NULL) {
    if (!do_layout_alter_audio_warning()) {
      lives_list_free_strings(mainw->xlays);
      lives_list_free(mainw->xlays);
      mainw->xlays=NULL;
      return;
    }
    add_lmap_error(LMAP_ERROR_ALTER_AUDIO,cfile->name,(livespointer)cfile->layout_map,mainw->current_file,0,0.,cfile->stored_layout_audio>0.);
    has_lmap_error=TRUE;
    lives_list_free_strings(mainw->xlays);
    lives_list_free(mainw->xlays);
    mainw->xlays=NULL;
  }

  if (button==NULL) {
    mainw->fx1_bool=!(cfile->undo1_int==cfile->arate);
    mainw->fx1_val=cfile->undo1_dbl;
  }

  set_undoable(_("Speed Change"),TRUE);
  cfile->undo1_dbl=cfile->fps;
  cfile->undo1_int=cfile->arate;
  cfile->undo_action=UNDO_CHANGE_SPEED;

  if (mainw->fx1_val==0.) mainw->fx1_val=1.;
  cfile->pb_fps=cfile->fps=mainw->fx1_val;
  if (mainw->fx1_bool) {
    cfile->arate=(int)(arate*cfile->fps+.5);
    msg=lives_strdup_printf(_("Changed playback speed to %.3f frames per second and audio to %d Hz.\n"),cfile->fps,cfile->arate);
  } else {
    msg=lives_strdup_printf(_("Changed playback speed to %.3f frames per second.\n"),cfile->fps);
  }
  d_print(msg);
  lives_free(msg);

  cfile->ratio_fps=FALSE;

  save_clip_value(mainw->current_file,CLIP_DETAILS_FPS,&cfile->fps);
  if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
  save_clip_value(mainw->current_file,CLIP_DETAILS_PB_FPS,&cfile->fps);
  if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
  save_clip_value(mainw->current_file,CLIP_DETAILS_PB_ARATE,&cfile->arate);
  if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
  if (bad_header) do_header_write_error(mainw->current_file);

  switch_to_file(mainw->current_file,mainw->current_file);

  if (has_lmap_error) popup_lmap_errors(NULL,NULL);

  if (mainw->sl_undo_mem!=NULL&&cfile->stored_layout_frame!=0) {
    // need to invalidate undo/redo stack, in case file was used in some layout undo
    stored_event_list_free_undos();
  }

}






int reorder_frames(int rwidth, int rheight) {
  int new_frames=cfile->old_frames;
  int cur_frames=cfile->frames;
  char **array;
  char *com;

  if (rwidth*rheight==0) com=lives_strdup_printf("%s reorder \"%s\" \"%s\" %d 0 0 %d %d",prefs->backend,cfile->handle,
                               get_image_ext_for_type(cfile->img_type),!mainw->endian,
                               reorder_leave_back,cfile->frames);
  else {
    if (!prefs->enc_letterbox) {
      com=lives_strdup_printf("%s reorder \"%s\" \"%s\" %d %d %d 0 %d",prefs->backend,cfile->handle,
                              get_image_ext_for_type(cfile->img_type),!mainw->endian,rwidth,rheight,cfile->frames);
    } else {
      int iwidth=cfile->hsize,iheight=cfile->vsize;
      calc_maxspect(rwidth,rheight,&iwidth,&iheight);

      if (iwidth==cfile->hsize&&iheight==cfile->vsize) {
        iwidth=-iwidth;
        iheight=-iheight;
      }

      com=lives_strdup_printf("%s reorder \"%s\" \"%s\" %d %d %d %d %d %d %d",prefs->backend,cfile->handle,
                              get_image_ext_for_type(cfile->img_type),!mainw->endian,rwidth,rheight,
                              reorder_leave_back,cfile->frames,iwidth,iheight);
    }
  }

  cfile->frames=0;

  cfile->progress_start=1;
  cfile->progress_end=save_event_frames();  // we convert cfile->event_list to a block and save it

  if (cfile->progress_end==-1) return -cur_frames; // save_event_frames failed

  if (cur_frames>cfile->progress_end) cfile->progress_end=cur_frames;

  cfile->next_event=NULL;
  if (cfile->event_list!=NULL) {
    if (cfile->event_list_back!=NULL) event_list_free(cfile->event_list_back);
    cfile->event_list_back=cfile->event_list;
    cfile->event_list=NULL;
  }

  unlink(cfile->info_file);
  mainw->error=FALSE;
  mainw->com_failed=FALSE;
  lives_system(com,FALSE);
  if (mainw->com_failed) return -cur_frames;

  if (cfile->undo_action==UNDO_RESAMPLE) {
    if (mainw->current_file>0) {
      cfile->nopreview=cfile->nokeep=TRUE;
      if (!do_progress_dialog(TRUE,TRUE,_("Resampling video"))) {
        cfile->nopreview=cfile->nokeep=FALSE;
        return cur_frames;
      }
      cfile->nopreview=cfile->nokeep=FALSE;
    } else {
      do_progress_dialog(TRUE,FALSE,_("Resampling clipboard video"));
    }
  } else {
    cfile->nopreview=cfile->nokeep=TRUE;
    if (!do_progress_dialog(TRUE,TRUE,_("Reordering frames"))) {
      cfile->nopreview=cfile->nokeep=FALSE;
      return cur_frames;
    }
    cfile->nopreview=cfile->nokeep=FALSE;
  }
  lives_free(com);

  if (mainw->error) {
    if (mainw->cancelled!=CANCEL_ERROR) do_error_dialog(_("\n\nLiVES was unable to reorder the frames."));
    deorder_frames(new_frames,FALSE);
    new_frames=-new_frames;
  } else {
    array=lives_strsplit(mainw->msg,"|",2);

    new_frames=atoi(array[1]);
    lives_strfreev(array);

    if (cfile->frames>new_frames) {
      new_frames=cfile->frames;
    }
  }

  return new_frames;
}



int deorder_frames(int old_frames, boolean leave_bak) {
  char *com;
  weed_timecode_t time_start;
  int perf_start,perf_end;

  if (cfile->event_list!=NULL) return cfile->frames;

  cfile->event_list=cfile->event_list_back;
  cfile->event_list_back=NULL;

  if (cfile->event_list==NULL) {
    perf_start=1;
    perf_end=old_frames;
  } else {
    time_start=get_event_timecode(get_first_event(cfile->event_list));
    perf_start=(int)(cfile->fps*(double)time_start/U_SEC)+1;
    perf_end=perf_start+count_events(cfile->event_list,FALSE,0,0)-1;
  }
  com=lives_strdup_printf("%s deorder \"%s\" %d %d %d \"%s\" %d",prefs->backend,cfile->handle,
                          perf_start,cfile->frames,perf_end,
                          get_image_ext_for_type(cfile->img_type),leave_bak);

  unlink(cfile->info_file);
  mainw->com_failed=FALSE;
  lives_system(com,TRUE);
  if (mainw->com_failed) return cfile->frames;

  do_progress_dialog(TRUE,FALSE,_("Deordering frames"));
  lives_free(com);


  // check for EOF

  if (cfile->frame_index_back!=NULL) {
    int current_frames=cfile->frames;
    cfile->frames=old_frames;
    restore_frame_index_back(mainw->current_file);
    cfile->frames=current_frames;
  }

  return old_frames;
}


boolean resample_clipboard(double new_fps) {
  // resample the clipboard video - if we already did it once, it is
  // quicker the second time
  char *msg,*com;
  int current_file=mainw->current_file;

  mainw->no_switch_dprint=TRUE;

  if (clipboard->undo1_dbl==new_fps&&!prefs->conserve_space) {
    int new_frames;
    double old_fps=clipboard->fps;

    if (new_fps==clipboard->fps) {
      mainw->no_switch_dprint=FALSE;
      return TRUE;
    }

    // we already resampled to this fps
    new_frames=count_resampled_frames(clipboard->frames,clipboard->fps,new_fps);

    mainw->current_file=0;

    // copy .mgk to .img_ext and .img_ext to .bak (i.e redo the resample)
    com=lives_strdup_printf("%s redo \"%s\" %d %d \"%s\"",prefs->backend,cfile->handle,1,new_frames,
                            get_image_ext_for_type(cfile->img_type));
    unlink(cfile->info_file);
    mainw->com_failed=FALSE;
    lives_system(com,FALSE);

    if (mainw->com_failed) {
      mainw->no_switch_dprint=FALSE;
      d_print_failed();
      return FALSE;
    }

    cfile->progress_start=1;
    cfile->progress_end=new_frames;
    cfile->old_frames=cfile->frames;
    // show a progress dialog, not cancellable
    do_progress_dialog(TRUE,FALSE,_("Resampling clipboard video"));
    lives_free(com);
    cfile->frames=new_frames;
    cfile->undo_action=UNDO_RESAMPLE;
    cfile->fps=cfile->undo1_dbl;
    cfile->undo1_dbl=old_fps;
    msg=lives_strdup_printf(_("Clipboard was resampled to %d frames.\n"),cfile->frames);
    d_print(msg);
    lives_free(msg);
    mainw->current_file=current_file;
  } else {
    if (clipboard->undo1_dbl<clipboard->fps) {
      int old_frames=count_resampled_frames(clipboard->frames,clipboard->fps,clipboard->undo1_dbl);
      mainw->current_file=0;
      com=lives_strdup_printf("%s undo \"%s\" %d %d \"%s\"",prefs->backend,cfile->handle,old_frames+1,cfile->frames,
                              get_image_ext_for_type(cfile->img_type));
      unlink(cfile->info_file);
      lives_system(com,FALSE);
      cfile->progress_start=old_frames+1;
      cfile->progress_end=cfile->frames;
      // show a progress dialog, not cancellable
      do_progress_dialog(TRUE,FALSE,_("Resampling clipboard video"));
      lives_free(com);
    }

    // resample to cfile fps
    mainw->current_file=current_file;
    clipboard->undo1_dbl=new_fps;

    if (new_fps==clipboard->fps) {
      mainw->no_switch_dprint=FALSE;
      return TRUE;
    }

    mainw->current_file=0;
    on_resample_vid_ok(NULL,NULL);
    mainw->current_file=current_file;
    if (clipboard->fps!=new_fps) {
      d_print(_("resampling error..."));
      mainw->error=1;
      mainw->no_switch_dprint=FALSE;
      return FALSE;
    }
    // clipboard->fps now holds new_fps, clipboard->undo1_dbl holds orig fps
    // BUT we will later undo this, then clipboard->fps will hold orig fps,
    // clipboard->undo1_dbl will hold resampled fps

  }

  mainw->no_switch_dprint=FALSE;
  return TRUE;
}
