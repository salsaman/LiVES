// effects.c
// LiVES (lives-exe)
// (c) G. Finch 2003 - 2014
// Released under the GPL 3 or later
// see file ../COPYING for licensing details

#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#ifdef HAVE_SYSTEM_WEED
#include <weed/weed.h>
#include <weed/weed-palettes.h>
#include <weed/weed-effects.h>
#include <weed/weed-host.h>
#else
#include "../libweed/weed.h"
#include "../libweed/weed-palettes.h"
#include "../libweed/weed-effects.h"
#include "../libweed/weed-host.h"
#endif

#include "main.h"
#include "effects.h"
#include "interface.h"
#include "paramwindow.h"
#include "support.h"
#include "cvirtual.h"
#include "resample.h"
#include "ce_thumbs.h"


//////////// Effects ////////////////


#ifdef HAVE_YUV4MPEG
#include "lives-yuv4mpeg.h"
#endif


#include "rte_window.h"

static int framecount;

static weed_plant_t *resize_instance=NULL;

static boolean apply_audio_fx;

///////////////////////////////////////////////////

// generic


char *lives_fx_cat_to_text(lives_fx_cat_t cat, boolean plural) {

  // return value should be free'd after use
  switch (cat) {

  // main categories
  case LIVES_FX_CAT_VIDEO_GENERATOR:
    if (!plural) return (lives_strdup(_("generator")));
    else return (lives_strdup(_("Generators")));
  case LIVES_FX_CAT_AUDIO_GENERATOR:
    if (!plural) return (lives_strdup(_("audio generator")));
    else return (lives_strdup(_("Audio Generators")));
  case LIVES_FX_CAT_AV_GENERATOR:
    if (!plural) return (lives_strdup(_("audio/video generator")));
    else return (lives_strdup(_("Audio/Video Generators")));
  case LIVES_FX_CAT_DATA_GENERATOR:
    if (!plural) return (lives_strdup(_("data generator")));
    else return (lives_strdup(_("Data Generators")));
  case LIVES_FX_CAT_DATA_VISUALISER:
    if (!plural) return (lives_strdup(_("data visualiser")));
    else return (lives_strdup(_("Data Visualisers")));
  case LIVES_FX_CAT_DATA_PROCESSOR:
    if (!plural) return (lives_strdup(_("data processor")));
    else return (lives_strdup(_("Data Processors")));
  case LIVES_FX_CAT_DATA_SOURCE:
    if (!plural) return (lives_strdup(_("data source")));
    else return (lives_strdup(_("Data Sources")));
  case LIVES_FX_CAT_TRANSITION:
    if (!plural) return (lives_strdup(_("transition")));
    else return (lives_strdup(_("Transitions")));
  case LIVES_FX_CAT_EFFECT:
    if (!plural) return (lives_strdup(_("effect")));
    else return (lives_strdup(_("Effects")));
  case LIVES_FX_CAT_UTILITY:
    if (!plural) return (lives_strdup(_("utility")));
    else return (lives_strdup(_("Utilities")));
  case LIVES_FX_CAT_COMPOSITOR:
    if (!plural) return (lives_strdup(_("compositor")));
    else return (lives_strdup(_("Compositors")));
  case LIVES_FX_CAT_TAP:
    if (!plural) return (lives_strdup(_("tap")));
    else return (lives_strdup(_("Taps")));
  case LIVES_FX_CAT_SPLITTER:
    if (!plural) return (lives_strdup(_("splitter")));
    else return (lives_strdup(_("Splitters")));
  case LIVES_FX_CAT_CONVERTER:
    if (!plural) return (lives_strdup(_("converter")));
    else return (lives_strdup(_("Converters")));
  case LIVES_FX_CAT_ANALYSER:
    if (!plural) return (lives_strdup(_("analyser")));
    else return (lives_strdup(_("Analysers")));


  // subcategories
  case LIVES_FX_CAT_AV_TRANSITION:
    if (!plural) return (lives_strdup(_("audio/video")));
    else return (lives_strdup(_("Audio/Video Transitions")));
  case LIVES_FX_CAT_VIDEO_TRANSITION:
    if (!plural) return (lives_strdup(_("video only")));
    else return (lives_strdup(_("Video only Transitions")));
  case LIVES_FX_CAT_AUDIO_TRANSITION:
    if (!plural) return (lives_strdup(_("audio only")));
    else return (lives_strdup(_("Audio only Transitions")));
  case LIVES_FX_CAT_AUDIO_MIXER:
    if (!plural) return (lives_strdup(_("audio")));
    else return (lives_strdup(_("Audio Mixers")));
  case LIVES_FX_CAT_AUDIO_EFFECT:
    if (!plural) return (lives_strdup(_("audio")));
    else return (lives_strdup(_("Audio Effects")));
  case LIVES_FX_CAT_VIDEO_EFFECT:
    if (!plural) return (lives_strdup(_("video")));
    else return (lives_strdup(_("Video Effects")));
  case LIVES_FX_CAT_AUDIO_VOL:
    if (!plural) return (lives_strdup(_("audio volume controller")));
    else return (lives_strdup(_("Audio Volume Controllers")));
  case LIVES_FX_CAT_VIDEO_ANALYSER:
    if (!plural) return (lives_strdup(_("video analyser")));
    else return (lives_strdup(_("Video analysers")));
  case LIVES_FX_CAT_AUDIO_ANALYSER:
    if (!plural) return (lives_strdup(_("audio analyser")));
    else return (lives_strdup(_("Audio Analysers")));


  default:
    return (lives_strdup(_("unknown")));
  }
}



// Rendered effects


boolean do_effect(lives_rfx_t *rfx, boolean is_preview) {
  // returns FALSE if the user cancelled
  // leave_info_file is set if a preview turned into actual processing: ie. no params were changed after the preview
  // preview generates .pre files instead of .mgk, so needs special post-processing

  int oundo_start=cfile->undo_start;
  int oundo_end=cfile->undo_end;
  char effectstring[128];
  double old_pb_fps=cfile->pb_fps;

  char *text;
  char *fxcommand=NULL,*cmd;
  int current_file=mainw->current_file;

  int new_file=current_file;
  int ldfile;

  boolean got_no_frames=FALSE;
  char *tmp;

  if (rfx->num_in_channels==0&&!is_preview) current_file=mainw->pre_src_file;

  if (is_preview) {
    // generators start at 1, even though they have no initial frames
    cfile->progress_start=cfile->undo_start=rfx->num_in_channels==0?1:cfile->start;
    cfile->progress_end=cfile->undo_end=cfile->end;
  } else if (rfx->num_in_channels!=2) {
    cfile->progress_start=cfile->undo_start=cfile->start;
    cfile->progress_end=cfile->undo_end=cfile->end;
  }

  if (!mainw->internal_messaging&&!mainw->keep_pre) {
    char *pdefault;
    char *plugin_name;

    if (rfx->status==RFX_STATUS_BUILTIN) plugin_name=lives_build_filename(prefs->lib_dir,PLUGIN_EXEC_DIR,
          PLUGIN_RENDERED_EFFECTS_BUILTIN,rfx->name,NULL);
    else plugin_name=lives_strdup(rfx->name);

    if (rfx->num_in_channels==2) {
      // transition has a few extra bits
      pdefault=lives_strdup_printf("%s %d %d %d %d %d %s %s %d \"%s/%s\"",cfile->handle,rfx->status,
                                   cfile->progress_start,cfile->progress_end,cfile->hsize,cfile->vsize,
                                   get_image_ext_for_type(cfile->img_type),get_image_ext_for_type(clipboard->img_type),
                                   clipboard->start,prefs->tmpdir,clipboard->handle);
    } else {
      pdefault=lives_strdup_printf("%s %d %d %d %d %d %s",cfile->handle,rfx->status,cfile->progress_start,
                                   cfile->progress_end,cfile->hsize,cfile->vsize,get_image_ext_for_type(cfile->img_type));
    }
    // and append params
    if (is_preview) {
      cmd=lives_strdup("pfxrender");
      mainw->show_procd=FALSE;
    } else cmd=lives_strdup("fxrender");
    fxcommand=lives_strconcat(prefs->backend," \"",cmd,"_",plugin_name,"\" ", pdefault,
                              (tmp=param_marshall(rfx, FALSE)), NULL);

    lives_free(plugin_name);
    lives_free(cmd);
    lives_free(pdefault);
    lives_free(tmp);
  }

  if (!mainw->keep_pre) unlink(cfile->info_file);

  if (!mainw->internal_messaging&&!mainw->keep_pre) {
    if (cfile->frame_index_back!=NULL) {
      lives_free(cfile->frame_index_back);
      cfile->frame_index_back=NULL;
    }
    lives_system(fxcommand,FALSE);
    lives_free(fxcommand);
  } else {
    if (mainw->num_tr_applied>0&&mainw->blend_file>0&&mainw->files[mainw->blend_file]!=NULL&&
        mainw->files[mainw->blend_file]->clip_type!=CLIP_TYPE_GENERATOR) {
      mainw->files[mainw->blend_file]->frameno=mainw->files[mainw->blend_file]->start-1;
    }
  }
  mainw->effects_paused=FALSE;

  if (is_preview) {
    cfile->undo_start=oundo_start;
    cfile->undo_end=oundo_end;
    mainw->current_file=current_file;
    return TRUE;
  }

  if (rfx->props&RFX_PROPS_MAY_RESIZE) {
    tmp=lives_strdup(_("%s all frames..."));
    text=lives_strdup_printf(tmp,_(rfx->action_desc));
  } else {
    if (rfx->num_in_channels==2) {
      tmp=lives_strdup(_("%s clipboard into frames %d to %d..."));
      text=lives_strdup_printf(tmp,_(rfx->action_desc),cfile->progress_start,cfile->progress_end);
    } else {
      if (rfx->num_in_channels==0) {
        mainw->no_switch_dprint=TRUE;
        if (mainw->gen_to_clipboard) {
          tmp=lives_strdup(_("%s to clipboard..."));
          text=lives_strdup_printf(tmp,_(rfx->action_desc));
        } else {
          tmp=lives_strdup(_("%s to new clip..."));
          text=lives_strdup_printf(tmp,_(rfx->action_desc));
        }
      } else {
        tmp=lives_strdup(_("%s frames %d to %d..."));
        text=lives_strdup_printf(tmp,_(rfx->action_desc),cfile->start,cfile->end);
      }
    }
  }

  if (!mainw->no_switch_dprint) d_print(""); // force switch text
  ldfile=mainw->last_dprint_file;

  d_print(text);
  lives_free(text);
  lives_free(tmp);
  mainw->last_dprint_file=ldfile;

  cfile->redoable=cfile->undoable=FALSE;
  lives_widget_set_sensitive(mainw->redo, FALSE);
  lives_widget_set_sensitive(mainw->undo, FALSE);

  cfile->undo_action=UNDO_EFFECT;

  if (rfx->props&RFX_PROPS_MAY_RESIZE) {
    cfile->ohsize=cfile->hsize;
    cfile->ovsize=cfile->vsize;
    mainw->resizing=TRUE;
    cfile->nokeep=TRUE;
  }

  // 'play' as fast as we possibly can
  cfile->pb_fps=1000000.;

  if (rfx->num_in_channels==2) {
    tmp=lives_strdup(_("%s clipboard with selection"));
    lives_snprintf(effectstring,128,tmp,_(rfx->action_desc));
  } else if (rfx->num_in_channels==0) {
    if (mainw->gen_to_clipboard) {
      tmp=lives_strdup(_("%s to clipboard"));
      lives_snprintf(effectstring,128,tmp,_(rfx->action_desc));
    } else {
      tmp=lives_strdup(_("%s to new clip"));
      lives_snprintf(effectstring,128,tmp,_(rfx->action_desc));
    }
  } else {
    tmp=lives_strdup(_("%s frames %d to %d"));
    lives_snprintf(effectstring,128,tmp,_(rfx->action_desc),cfile->undo_start,cfile->undo_end);
  }
  lives_free(tmp);

  if (cfile->clip_type==CLIP_TYPE_FILE&&rfx->status!=RFX_STATUS_WEED) {
    // pull a batch of frames for the backend to start processing
    cfile->fx_frame_pump=cfile->start;
  } else cfile->fx_frame_pump=0;

  if (rfx->props&RFX_PROPS_MAY_RESIZE||rfx->num_in_channels==0) {
    if (rfx->status==RFX_STATUS_WEED) {
      // set out_channel dimensions for resizers / generators
      int error;
      weed_plant_t *first_out=get_enabled_channel((weed_plant_t *)rfx->source,0,FALSE);
      weed_plant_t *first_ot=weed_get_plantptr_value(first_out,"template",&error);
      weed_set_int_value(first_out,"width",weed_get_int_value(first_ot,"host_width",&error));
      weed_set_int_value(first_out,"height",weed_get_int_value(first_ot,"host_height",&error));
    }
  }


  if (!do_progress_dialog(TRUE,TRUE,effectstring)||mainw->error) {
    mainw->last_dprint_file=ldfile;
    mainw->show_procd=TRUE;
    mainw->keep_pre=FALSE;
    if (mainw->error) {
      if (mainw->cancelled!=CANCEL_ERROR) do_error_dialog(mainw->msg);
      d_print_failed();
      mainw->last_dprint_file=ldfile;
    }
    if (mainw->cancelled!=CANCEL_KEEP) {
      cfile->undo_start=oundo_start;
      cfile->undo_end=oundo_end;
    }
    cfile->pb_fps=old_pb_fps;
    mainw->internal_messaging=FALSE;
    mainw->resizing=FALSE;
    cfile->nokeep=FALSE;
    cfile->fx_frame_pump=0;

    if (cfile->start==0) {
      cfile->start=1;
      cfile->end=cfile->frames;
    }

    if (rfx->num_in_channels==0&&mainw->current_file!=current_file) {
      mainw->suppress_dprint=TRUE;
      close_current_file(current_file);
      mainw->suppress_dprint=FALSE;
    } else {
      mainw->current_file=current_file;
      do_rfx_cleanup(rfx);
    }

    mainw->is_generating=FALSE;
    mainw->no_switch_dprint=FALSE;

    if (mainw->multitrack!=NULL) {
      mainw->pre_src_file=-1;
    }

    return FALSE;
  }

  if (cfile->start==0) {
    cfile->start=1;
    cfile->end=cfile->frames;
  }

  do_rfx_cleanup(rfx);

  mainw->resizing=FALSE;
  cfile->nokeep=FALSE;
  cfile->fx_frame_pump=0;

  if (!mainw->gen_to_clipboard) {
    lives_widget_set_sensitive(mainw->undo, TRUE);
    if (rfx->num_in_channels>0) cfile->undoable=TRUE;
    cfile->pb_fps=old_pb_fps;
    mainw->internal_messaging=FALSE;
    if (rfx->num_in_channels>0) lives_widget_set_sensitive(mainw->select_last, TRUE);
    if (rfx->num_in_channels>0) set_undoable(rfx->menu_text,TRUE);
  }

  mainw->show_procd=TRUE;

  if (rfx->props&RFX_PROPS_MAY_RESIZE||rfx->num_in_channels==0) {
    // get new frame size
    if (rfx->status!=RFX_STATUS_WEED) {
      int numtok=get_token_count(mainw->msg,'|');

      if (numtok>1) {
        char **array=lives_strsplit(mainw->msg,"|",numtok);
        // [0] is "completed"
        cfile->hsize=atoi(array[1]);
        cfile->vsize=atoi(array[2]);
        if (rfx->num_in_channels==0) {
          cfile->fps=cfile->pb_fps=strtod(array[3],NULL);
          if (cfile->fps==0.) cfile->fps=cfile->pb_fps=prefs->default_fps;
          cfile->end=cfile->frames=atoi(array[4]);
          cfile->bpp=cfile->img_type==IMG_TYPE_JPEG?24:32;
        }
        lives_strfreev(array);
      }
      if (rfx->num_in_channels==0) {
        cfile->progress_start=1;
        cfile->progress_end=cfile->frames;
      }
    } else {
      int error;
      weed_plant_t *first_out=get_enabled_channel((weed_plant_t *)rfx->source,0,FALSE);
      weed_plant_t *first_ot=weed_get_plantptr_value(first_out,"template",&error);
      cfile->hsize=weed_get_int_value(first_ot,"host_width",&error);
      cfile->vsize=weed_get_int_value(first_ot,"host_height",&error);
    }

    if (rfx->num_in_channels>0) {
      if (cfile->hsize==cfile->ohsize&&cfile->vsize==cfile->ovsize) cfile->undo_action=UNDO_EFFECT;
      else {
        boolean bad_header=FALSE;
        save_clip_value(mainw->current_file,CLIP_DETAILS_WIDTH,&cfile->hsize);
        if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
        save_clip_value(mainw->current_file,CLIP_DETAILS_HEIGHT,&cfile->vsize);
        if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;
        cfile->undo_action=UNDO_RESIZABLE;
        if (bad_header) do_header_write_error(mainw->current_file);
      }
    }
  }

  mainw->cancelled=CANCEL_NONE;
  mainw->error=FALSE;

  if (mainw->keep_pre) {
    // this comes from a preview which then turned into processing
    char *com=lives_strdup_printf("%s mv_pre \"%s\" %d %d \"%s\"",prefs->backend_sync, cfile->handle,cfile->progress_start,
                                  cfile->progress_end,get_image_ext_for_type(cfile->img_type));

    unlink(cfile->info_file);
    mainw->cancelled=CANCEL_NONE;
    lives_system(com,FALSE);
    lives_free(com);
    mainw->keep_pre=FALSE;

    check_backend_return(cfile);

    if (mainw->error) {
      if (!mainw->cancelled) {
        do_info_dialog(_("\nNo frames were generated.\n"));
        d_print_failed();
      } else if (mainw->cancelled!=CANCEL_ERROR) d_print_cancelled();
      else d_print_failed();

      if (rfx->num_in_channels==0) {
        mainw->is_generating=FALSE;

        if (mainw->current_file!=current_file) {
          mainw->suppress_dprint=TRUE;
          close_current_file(current_file);
          mainw->suppress_dprint=FALSE;
        }

        mainw->current_file=current_file;
        mainw->last_dprint_file=ldfile;

        if (mainw->multitrack!=NULL) {
          mainw->current_file=mainw->multitrack->render_file;
        }
      }
      mainw->no_switch_dprint=FALSE;
      return FALSE;
    }
  }

  if (rfx->num_in_channels==0) {
    if (rfx->props&RFX_PROPS_BATCHG) {
      // batch mode generators need some extra processing
      char *imgdir=lives_strdup_printf("%s%s",prefs->tmpdir,cfile->handle);
      int img_file=mainw->current_file;

      mainw->suppress_dprint=TRUE;
      open_file_sel(imgdir,0,0);
      lives_free(imgdir);
      new_file=mainw->current_file;

      if (new_file!=img_file) {
        mainw->current_file=img_file;

        lives_snprintf(mainw->files[new_file]->name,256,"%s",cfile->name);
        lives_snprintf(mainw->files[new_file]->file_name,PATH_MAX,"%s",cfile->file_name);
        set_menu_text(mainw->files[new_file]->menuentry,cfile->name,FALSE);

        mainw->files[new_file]->fps=mainw->files[new_file]->pb_fps=cfile->fps;
      } else got_no_frames=TRUE;

      close_current_file(current_file);
      mainw->suppress_dprint=FALSE;

      if (!got_no_frames) mainw->current_file=new_file;
    } else {
      char *tfile=make_image_file_name(cfile,cfile->frames,prefs->image_ext);

      if (!lives_file_test(tfile, LIVES_FILE_TEST_EXISTS)) {
        get_frame_count(mainw->current_file);
        cfile->end=cfile->frames;
      }
      lives_free(tfile);
    }

    if (got_no_frames||cfile->frames==0) {
      mainw->is_generating=FALSE;
      if (!mainw->cancelled) {
        do_info_dialog(_("\nNo frames were generated.\n"));
        d_print_failed();
      } else d_print_cancelled();
      if (!got_no_frames) {
        mainw->suppress_dprint=TRUE;
        close_current_file(current_file);
        mainw->suppress_dprint=FALSE;
      }
      mainw->last_dprint_file=ldfile;
      mainw->no_switch_dprint=FALSE;
      if (mainw->multitrack!=NULL) mainw->current_file=mainw->multitrack->render_file;
      return FALSE;
    }

    if (mainw->gen_to_clipboard) {
      // here we will copy all values to the clipboard, including the handle
      // then close the current file without deleting the frames

      init_clipboard();

      lives_memcpy(clipboard,cfile,sizeof(lives_clip_t));
      cfile->is_loaded=TRUE;
      mainw->suppress_dprint=TRUE;
      mainw->only_close=TRUE;

      close_current_file(current_file);

      mainw->suppress_dprint=FALSE;
      mainw->only_close=FALSE;

      new_file=current_file;

      mainw->untitled_number--;

    } else {
      if (!(rfx->props&RFX_PROPS_BATCHG)) {
        // gen to new file
        cfile->is_loaded=TRUE;
        add_to_clipmenu();
        if (!save_clip_values(new_file)) {
          close_current_file(current_file);
          return FALSE;
        }

        if (prefs->crash_recovery) add_to_recovery_file(cfile->handle);

        if (mainw->multitrack!=NULL) {
          mt_init_clips(mainw->multitrack,mainw->current_file,TRUE);
          mt_clip_select(mainw->multitrack,TRUE);
        }

      }
      lives_notify(LIVES_OSC_NOTIFY_CLIP_OPENED,"");
    }
    mainw->is_generating=FALSE;
  }

  if (!mainw->gen_to_clipboard) cfile->changed=TRUE;
  if (mainw->multitrack==NULL) {
    if (new_file!=-1) {
      lives_sync();
      switch_to_file((mainw->current_file=0),new_file);
    }
  } else {
    mainw->current_file=mainw->multitrack->render_file;
    mainw->pre_src_file=-1;
  }

  d_print_done();
  mainw->no_switch_dprint=FALSE;
  mainw->gen_to_clipboard=FALSE;
  mainw->last_dprint_file=ldfile;

  return TRUE;
}



// realtime fx




lives_render_error_t realfx_progress(boolean reset) {
  static lives_render_error_t write_error;

  LiVESError *error=NULL;

  char oname[PATH_MAX];

  LiVESPixbuf *pixbuf;

  int64_t frameticks;

  weed_plant_t *layer;

  char *com,*tmp;

  static int i;

  int weed_error;
  int layer_palette;
  int retval;


  // this is called periodically from do_processing_dialog for internal effects

  if (reset) {
    i=cfile->start;
    clear_mainw_msg();

    if (cfile->clip_type==CLIP_TYPE_FILE) {
      if (cfile->frame_index_back!=NULL) lives_free(cfile->frame_index_back);
      cfile->frame_index_back=frame_index_copy(cfile->frame_index,cfile->frames,0);
    }
    write_error=LIVES_RENDER_ERROR_NONE;
    return LIVES_RENDER_READY;
  }

  if (mainw->effects_paused) return LIVES_RENDER_EFFECTS_PAUSED;

  // sig_progress...
  lives_snprintf(mainw->msg,256,"%d",i);
  // load, effect, save frame

  // skip resizing virtual frames
  if (resize_instance!=NULL&&is_virtual_frame(mainw->current_file,i)) {
    if (++i>cfile->end) {
      mainw->internal_messaging=FALSE;
      lives_snprintf(mainw->msg,9,"completed");
    }
    mainw->rowstride_alignment_hint=1;
    return LIVES_RENDER_COMPLETE;
  }

  if (has_video_filters(FALSE)||resize_instance!=NULL) {
    mainw->rowstride_alignment=mainw->rowstride_alignment_hint;

    layer=weed_plant_new(WEED_PLANT_CHANNEL);
    weed_set_int_value(layer,"clip",mainw->current_file);
    weed_set_int_value(layer,"frame",i);

    frameticks=(i-cfile->start+1.)/cfile->fps*U_SECL;

    if (!pull_frame(layer,get_image_ext_for_type(cfile->img_type),frameticks)) {
      // do_read_failed_error_s() cannot be used here as we dont know the filename
      lives_snprintf(mainw->msg,256,"error|missing image %d",i);
      return LIVES_RENDER_WARNING_READ_FRAME;
    }

    layer=on_rte_apply(layer, 0, 0, (weed_timecode_t)frameticks);

    if (!has_video_filters(TRUE)||resize_instance!=NULL) {
      layer_palette=weed_get_int_value(layer,"current_palette",&weed_error);

      if (resize_instance==NULL) resize_layer(layer,cfile->hsize,cfile->vsize,LIVES_INTERP_BEST,layer_palette,0);

      if (cfile->img_type==IMG_TYPE_JPEG&&layer_palette!=WEED_PALETTE_RGB24&&layer_palette!=WEED_PALETTE_RGBA32) {
        convert_layer_palette(layer,WEED_PALETTE_RGB24,0);
        layer_palette=WEED_PALETTE_RGB24;
      } else if (cfile->img_type==IMG_TYPE_PNG&&layer_palette!=WEED_PALETTE_RGBA32) {
        convert_layer_palette(layer,WEED_PALETTE_RGBA32,0);
        layer_palette=WEED_PALETTE_RGBA32;
      }

      pixbuf=layer_to_pixbuf(layer);
      weed_plant_free(layer);

      tmp=make_image_file_name(cfile,i,LIVES_FILE_EXT_MGK);
      lives_snprintf(oname,PATH_MAX,"%s",tmp);
      lives_free(tmp);

      do {
        retval=0;
        lives_pixbuf_save(pixbuf, oname, cfile->img_type, 100, TRUE, &error);

        if (error!=NULL) {
          retval=do_write_failed_error_s_with_retry(oname,error->message,NULL);
          lives_error_free(error);
          error=NULL;
          if (retval!=LIVES_RESPONSE_RETRY) write_error=LIVES_RENDER_ERROR_WRITE_FRAME;
        }
      } while (retval==LIVES_RESPONSE_RETRY);


      lives_object_unref(pixbuf);

      if (cfile->clip_type==CLIP_TYPE_FILE) {
        cfile->frame_index[i-1]=-1;
      }
    } else weed_plant_free(layer);
  }
  if (apply_audio_fx) {
    if (!apply_rte_audio((double)cfile->arate/(double)cfile->fps+(double)rand()/.5/(double)(RAND_MAX))) {
      return LIVES_RENDER_ERROR_WRITE_AUDIO;
    }
  }

  if (++i>cfile->end) {
    if (resize_instance!=NULL||(has_video_filters(FALSE)&&!has_video_filters(TRUE))) {
      mainw->error=FALSE;
      mainw->cancelled=CANCEL_NONE;
      com=lives_strdup_printf("%s mv_mgk \"%s\" %d %d \"%s\"",prefs->backend,cfile->handle,cfile->start,
                              cfile->end,get_image_ext_for_type(cfile->img_type));
      lives_system(com,FALSE);
      lives_free(com);
      mainw->internal_messaging=FALSE;

      check_backend_return(cfile);

      if (mainw->error) write_error=LIVES_RENDER_ERROR_WRITE_FRAME;
      //cfile->may_be_damaged=TRUE;
      else {
        if (cfile->clip_type==CLIP_TYPE_FILE) {
          if (!check_if_non_virtual(mainw->current_file,1,cfile->frames)) save_frame_index(mainw->current_file);
        }
        mainw->rowstride_alignment_hint=1;
        return LIVES_RENDER_COMPLETE;
      }
    } else {
      sprintf(mainw->msg,"%s","completed");
      mainw->rowstride_alignment_hint=1;
      return LIVES_RENDER_COMPLETE;
    }
  }
  if (write_error) return write_error;
  return LIVES_RENDER_PROCESSING;
}




boolean on_realfx_activate_inner(int type, lives_rfx_t *rfx) {
  // type can be 0 - apply current realtime effects
  // 1 - resize (using weed filter)
  boolean retval;

  boolean has_new_audio=FALSE;

  apply_audio_fx=FALSE;

  if (type==0&&((cfile->achans>0&&prefs->audio_src==AUDIO_SRC_INT&&has_audio_filters(AF_TYPE_ANY))||mainw->agen_key!=0)) {
    if (mainw->agen_key!=0&&cfile->achans==0) {
      // apply audio gen to clip with no audio - prompt for audio settings
      resaudw=create_resaudw(2,NULL,NULL);
      lives_widget_context_update();
      lives_xwindow_raise(lives_widget_get_xwindow(resaudw->dialog));

      if (lives_dialog_run(LIVES_DIALOG(resaudw->dialog))!=LIVES_RESPONSE_OK) return FALSE;
      if (mainw->error) {
        mainw->error=FALSE;
        return FALSE;
      }
      has_new_audio=TRUE;
    }
    apply_audio_fx=TRUE;
    if (!apply_rte_audio_init()) return FALSE;

  }

  if (type==1) resize_instance=(weed_plant_t *)rfx->source;
  else resize_instance=NULL;

  mainw->internal_messaging=TRUE;
  framecount=0;

  mainw->rowstride_alignment_hint=1;

  mainw->progress_fn=&realfx_progress;
  mainw->progress_fn(TRUE);

  weed_reinit_all();

  retval=do_effect(rfx,FALSE);

  if (apply_audio_fx) {
    apply_rte_audio_end(!retval);

    if (retval) {
      if (!has_video_filters(FALSE)||!has_video_filters(TRUE)) cfile->undo_action=UNDO_NEW_AUDIO;

      cfile->undo_achans=cfile->achans;
      cfile->undo_arate=cfile->arate;
      cfile->undo_arps=cfile->arps;
      cfile->undo_asampsize=cfile->asampsize;
      cfile->undo_signed_endian=cfile->signed_endian;

    } else {
      if (has_new_audio) cfile->achans=cfile->asampsize=cfile->arate=cfile->arps=0;
      else {
        char *com=lives_strdup_printf("%s undo_audio %s",prefs->backend_sync,cfile->handle);
        mainw->com_failed=FALSE;
        unlink(cfile->info_file);
        lives_system(com,FALSE);
        lives_free(com);
      }
    }
    reget_afilesize(mainw->current_file);
  }

  resize_instance=NULL;
  return retval;

}




void on_realfx_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  int type=1;

  boolean has_lmap_error=FALSE;

  // type can be 0 - apply current realtime effects
  // 1 - resize (using weed filter)

  if (menuitem!=NULL) {
    type=0;

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

  if (!on_realfx_activate_inner(type,(lives_rfx_t *)user_data)) return;

  if (has_lmap_error) popup_lmap_errors(NULL,NULL);


}





weed_plant_t *on_rte_apply(weed_plant_t *layer, int opwidth, int opheight, weed_timecode_t tc) {
  // realtime effects
  weed_plant_t **layers,*retlayer;
  int i;

  if (mainw->foreign) return NULL;

  layers=(weed_plant_t **)lives_malloc(3*sizeof(weed_plant_t *));

  layers[2]=NULL;

  layers[0]=layer;

  if (mainw->blend_file>-1&&mainw->num_tr_applied>0&&(mainw->files[mainw->blend_file]==NULL||
      (mainw->files[mainw->blend_file]->clip_type==CLIP_TYPE_DISK&&
       (!mainw->files[mainw->blend_file]->frames||
        !mainw->files[mainw->blend_file]->is_loaded)))) {
    // invalid blend file
    mainw->blend_file=mainw->current_file;
  }

  if (mainw->num_tr_applied&&mainw->blend_file!=mainw->current_file&&
      mainw->blend_file!=-1&&mainw->files[mainw->blend_file]!=NULL&&resize_instance==NULL) {
    layers[1]=get_blend_layer(tc);
  } else layers[1]=NULL;

  if (resize_instance!=NULL) {
    lives_filter_error_t filter_error;
    weed_plant_t *init_event=weed_plant_new(WEED_PLANT_EVENT);
    weed_set_int_value(init_event,"in_tracks",0);
    weed_set_int_value(init_event,"out_tracks",0);

    filter_error=weed_apply_instance(resize_instance,init_event,layers,0,0,tc);
    filter_error=filter_error; // stop compiler complaining
    retlayer=layers[0];
    weed_plant_free(init_event);
  } else {
    retlayer=weed_apply_effects(layers,mainw->filter_map,tc,opwidth,opheight,mainw->pchains);
  }

  // all our pixel_data will have been free'd already
  for (i=0; layers[i]!=NULL; i++) {
    if (layers[i]!=retlayer) weed_plant_free(layers[i]);
  }
  lives_free(layers);

  return retlayer;
}





void deinterlace_frame(weed_plant_t *layer, weed_timecode_t tc) {
  weed_plant_t **layers;

  weed_plant_t *deint_filter,*deint_instance,*next_inst,*init_event;

  int deint_idx,error;

  if (mainw->fx_candidates[FX_CANDIDATE_DEINTERLACE].delegate==-1) return;

  deint_idx=LIVES_POINTER_TO_INT(lives_list_nth_data(mainw->fx_candidates[FX_CANDIDATE_DEINTERLACE].list,
                                 mainw->fx_candidates[FX_CANDIDATE_DEINTERLACE].delegate));

  deint_filter=get_weed_filter(deint_idx);

  deint_instance=weed_instance_from_filter(deint_filter);

  layers=(weed_plant_t **)lives_malloc(2*sizeof(weed_plant_t *));

  layers[1]=NULL;

  layers[0]=layer;

  init_event=weed_plant_new(WEED_PLANT_EVENT);
  weed_set_int_value(init_event,"in_tracks",0);
  weed_set_int_value(init_event,"out_tracks",0);

deint1:

  weed_apply_instance(deint_instance,init_event,layers,0,0,tc);

  if (weed_plant_has_leaf(deint_instance,"host_next_instance")) next_inst=weed_get_plantptr_value(deint_instance,"host_next_instance",&error);
  else next_inst=NULL;

  weed_call_deinit_func(deint_instance);
  weed_instance_unref(deint_instance);

  if (next_inst!=NULL) {
    deint_instance=next_inst;
    goto deint1;
  }

  weed_plant_free(init_event);

  lives_free(layers);
}










weed_plant_t *get_blend_layer(weed_timecode_t tc) {
  lives_clip_t *blend_file;
  static weed_timecode_t blend_tc=0;
  weed_timecode_t ntc=tc;

  if (mainw->blend_file==-1||mainw->files[mainw->blend_file]==NULL) return NULL;
  blend_file=mainw->files[mainw->blend_file];

  if (mainw->blend_file!=mainw->last_blend_file) {
    // mainw->last_blend_file is set to -1 on playback start
    mainw->last_blend_file=mainw->blend_file;
    blend_tc=tc;
  }

  blend_file->last_frameno=blend_file->frameno;

  blend_file->frameno=calc_new_playback_position(mainw->blend_file,blend_tc,(uint64_t *)&ntc);

  blend_tc=ntc;

  mainw->blend_layer=weed_plant_new(WEED_PLANT_CHANNEL);
  weed_set_int_value(mainw->blend_layer,"clip",mainw->blend_file);
  weed_set_int_value(mainw->blend_layer,"frame",blend_file->frameno);

  pull_frame_threaded(mainw->blend_layer,get_image_ext_for_type(blend_file->img_type),tc);

  return mainw->blend_layer;
}


////////////////////////////////////////////////////////////////////
// keypresses





boolean rte_on_off_callback(LiVESAccelGroup *group, LiVESObject *obj, uint32_t keyval, LiVESXModifierType mod, livespointer user_data) {
  // this is the callback which happens when a rte is keyed
  int key=LIVES_POINTER_TO_INT(user_data);
  uint64_t new_rte;

  if (!mainw->interactive && group!=NULL) return TRUE;

  mainw->fx_is_auto=FALSE;

  mainw->osc_block=TRUE;

  if (key<0) {
    mainw->fx_is_auto=TRUE;
    key=-key;
  }

  new_rte=GU641<<(key-1);

  if (key==EFFECT_NONE) {
    // switch up/down keys to default (fps change)
    weed_deinit_all(FALSE);
  } else {

    // the idea here is this gets set if a generator starts play, because in weed_init_effect() we will run playback
    // and then we come out of there and do not wish to set the key on
    mainw->gen_started_play=FALSE;

    if (!(mainw->rte&new_rte)) {
      // switch is ON
      // WARNING - if we start playing because a generator was started, we block here
      if (!(weed_init_effect(key-1))) {
        // ran out of instance slots, no effect assigned, or some other error
        pthread_mutex_lock(&mainw->event_list_mutex);
        if (mainw->rte&new_rte) mainw->rte^=new_rte;
        pthread_mutex_unlock(&mainw->event_list_mutex);
        if (rte_window!=NULL) rtew_set_keych(key-1,FALSE);
        if (mainw->ce_thumbs) ce_thumbs_set_keych(key-1,FALSE);
        mainw->osc_block=FALSE;
        return TRUE;
      }


      if (!mainw->gen_started_play) {
        if (!(mainw->rte&new_rte)) mainw->rte|=new_rte;

        mainw->last_grabbable_effect=key-1;
        if (rte_window!=NULL) rtew_set_keych(key-1,TRUE);
        if (mainw->ce_thumbs) {
          ce_thumbs_set_keych(key-1,TRUE);

          // if effect was auto (from ACTIVATE data connection), leave all param boxes
          // otherwise, remove any which are not "pinned"
          if (!mainw->fx_is_auto) ce_thumbs_add_param_box(key-1,!mainw->fx_is_auto);
        }
      }
    } else {
      // effect is OFF
      weed_deinit_effect(key-1);
      pthread_mutex_lock(&mainw->event_list_mutex);
      if (mainw->rte&new_rte) mainw->rte^=new_rte;
      pthread_mutex_unlock(&mainw->event_list_mutex);
      if (rte_window!=NULL) rtew_set_keych(key-1,FALSE);
      if (mainw->ce_thumbs) ce_thumbs_set_keych(key-1,FALSE);
    }
  }

  mainw->osc_block=FALSE;
  mainw->fx_is_auto=FALSE;

  if (mainw->current_file>0&&cfile->play_paused&&!mainw->noswitch) {
    load_frame_image(cfile->frameno);
  }

  if (mainw->playing_file==-1&&mainw->current_file>0&&((has_video_filters(FALSE)&&!has_video_filters(TRUE))||
      (cfile->achans>0&&prefs->audio_src==AUDIO_SRC_INT&&has_audio_filters(AF_TYPE_ANY))||
      mainw->agen_key!=0)) {

    lives_widget_set_sensitive(mainw->rendered_fx[0].menuitem,TRUE);
  } else lives_widget_set_sensitive(mainw->rendered_fx[0].menuitem,FALSE);

  if (key>0&&!mainw->fx_is_auto) {
    // user override any ACTIVATE data connection
    override_if_active_input(key);

    // if this is an outlet for ACTIVATE, disable the override now
    end_override_if_activate_output(key);
  }

  return TRUE;
}



boolean rte_on_off_callback_hook(LiVESToggleButton *button, livespointer user_data) {
  rte_on_off_callback(NULL, NULL, 0, (LiVESXModifierType)0, user_data);
  return TRUE;
}


boolean grabkeys_callback(LiVESAccelGroup *group, LiVESObject *obj, uint32_t keyval, LiVESXModifierType mod, livespointer user_data) {
  // assign the keys to the last key-grabbable effect
  mainw->rte_keys=mainw->last_grabbable_effect;
  mainw->osc_block=TRUE;
  if (rte_window!=NULL) {
    if (group!=NULL) rtew_set_keygr(mainw->rte_keys);
  }
  mainw->blend_factor=weed_get_blend_factor(mainw->rte_keys);
  mainw->osc_block=FALSE;
  return TRUE;
}



boolean textparm_callback(LiVESAccelGroup *group, LiVESObject *obj, uint32_t keyval, LiVESXModifierType mod, livespointer user_data) {
  // keyboard linked to first string parameter, until TAB is pressed
  mainw->rte_textparm=get_textparm();
  return TRUE;
}



boolean grabkeys_callback_hook(LiVESToggleButton *button, livespointer user_data) {
  if (!lives_toggle_button_get_active(button)) return TRUE;
  mainw->last_grabbable_effect=LIVES_POINTER_TO_INT(user_data);
  grabkeys_callback(NULL, NULL, 0, (LiVESXModifierType)0, user_data);
  return TRUE;
}


boolean rtemode_callback(LiVESAccelGroup *group, LiVESObject *obj, uint32_t keyval, LiVESXModifierType mod, livespointer user_data) {
  // "m" mode key
  if (mainw->rte_keys==-1) return TRUE;
  rte_key_setmode(0,-1);
  mainw->blend_factor=weed_get_blend_factor(mainw->rte_keys);
  return TRUE;
}


boolean rtemode_callback_hook(LiVESToggleButton *button, livespointer user_data) {
  int key_mode=LIVES_POINTER_TO_INT(user_data);
  int modes=rte_getmodespk();
  int key=(int)(key_mode/modes);
  int mode=key_mode-key*modes;

  if (!lives_toggle_button_get_active(button)) return TRUE;

  rte_key_setmode(key+1,mode);
  return TRUE;
}


boolean swap_fg_bg_callback(LiVESAccelGroup *group, LiVESObject *obj, uint32_t keyval, LiVESXModifierType mod, livespointer user_data) {
  int old_file=mainw->current_file;

  if (mainw->playing_file<1||mainw->num_tr_applied==0||mainw->noswitch||mainw->blend_file==-1||
      mainw->blend_file==mainw->current_file||mainw->files[mainw->blend_file]==NULL||mainw->preview||
      mainw->noswitch||(mainw->is_processing&&cfile->is_loaded)) {
    return TRUE;
  }

  do_quick_switch(mainw->blend_file);

  mainw->blend_file=old_file;

  rte_swap_fg_bg();

  if (mainw->ce_thumbs&&(mainw->active_sa_clips==SCREEN_AREA_BACKGROUND||mainw->active_sa_clips==SCREEN_AREA_FOREGROUND))
    ce_thumbs_highlight_current_clip();

  return TRUE;

  // **TODO - for weed, invert all transition parameters for any active effects
}






//////////////////////////////////////////////////////////////



