// effects.c
// LiVES (lives-exe)
// (c) G. Finch 2003 - 2011
// Released under the GPL 3 or later
// see file ../COPYING for licensing details

#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#ifdef HAVE_SYSTEM_WEED
#include "weed/weed.h"
#include "weed/weed-palettes.h"
#include "weed/weed-effects.h"
#include "weed/weed-host.h"
#else
#include "../libweed/weed.h"
#include "../libweed/weed-palettes.h"
#include "../libweed/weed-effects.h"
#include "../libweed/weed-host.h"
#endif

#include "main.h"
#include "effects.h"
#include "paramwindow.h"
#include "support.h"
#include "cvirtual.h"

//////////// Effects ////////////////


#ifdef HAVE_YUV4MPEG
#include "lives-yuv4mpeg.h"
#endif


#include "rte_window.h"

static int framecount;

static weed_plant_t *resize_instance=NULL;

///////////////////////////////////////////////////


// Rendered effects


gboolean do_effect(lives_rfx_t *rfx, gboolean is_preview) {
  // returns FALSE if the user cancelled
  // leave_info_file is set if a preview turned into actual processing: ie. no params were changed after the preview
  // preview generates .pre files instead of .mgk, so needs special post-processing

  gint oundo_start=cfile->undo_start;
  gint oundo_end=cfile->undo_end;
  gchar effectstring[128];
  gdouble old_pb_fps=cfile->pb_fps;

  gchar *text;
  gchar *fxcommand=NULL,*cmd;
  gint current_file=mainw->current_file;

  gint new_file=current_file;
  gint ldfile;

  gboolean got_no_frames=FALSE;
  gchar *tmp;

  if (rfx->num_in_channels==0&&!is_preview) current_file=mainw->pre_src_file;

  if (is_preview) {
    // generators start at 1, even though they have no initial frames
    cfile->progress_start=cfile->undo_start=cfile->start+(rfx->num_in_channels==0?1:0);
    cfile->progress_end=cfile->undo_end=cfile->end;
  }
  else if (rfx->num_in_channels!=2) {
    cfile->progress_start=cfile->undo_start=cfile->start;
    cfile->progress_end=cfile->undo_end=cfile->end;
  }

  if (!mainw->internal_messaging&&!mainw->keep_pre) {
    gchar *pdefault;
    gchar *plugin_name;

    if (rfx->status==RFX_STATUS_BUILTIN) plugin_name=g_build_filename(prefs->lib_dir,PLUGIN_EXEC_DIR,
								      PLUGIN_RENDERED_EFFECTS_BUILTIN,rfx->name,NULL);
    else plugin_name=g_strdup(rfx->name);

    if (rfx->num_in_channels==2) {
      // transition has a few extra bits
      pdefault=g_strdup_printf ("%s %d %d %d %d %d %s %s %d \"%s/%s\"",cfile->handle,rfx->status,
				cfile->progress_start,cfile->progress_end,cfile->hsize,cfile->vsize,
				cfile->img_type==IMG_TYPE_JPEG?"jpg":"png",clipboard->img_type==IMG_TYPE_JPEG?
				"jpg":"png",clipboard->start,prefs->tmpdir,clipboard->handle);
    }
    else {
      pdefault=g_strdup_printf ("%s %d %d %d %d %d %s",cfile->handle,rfx->status,cfile->progress_start,
				cfile->progress_end,cfile->hsize,cfile->vsize,cfile->img_type==IMG_TYPE_JPEG?"jpg":"png");
    }
    // and append params
    if (is_preview) {
      cmd=g_strdup("pfxrender");
      mainw->show_procd=FALSE;
    }
    else cmd=g_strdup("fxrender");
    fxcommand=g_strconcat ("smogrify ",cmd,"_",plugin_name," ", pdefault, (tmp=param_marshall (rfx, FALSE)), NULL);

    g_free(plugin_name);
    g_free(cmd);
    g_free(pdefault);
    g_free(tmp);
  }

  if (!mainw->keep_pre) unlink(cfile->info_file);

  if (!mainw->internal_messaging&&!mainw->keep_pre) {
    if (cfile->frame_index_back!=NULL) {
      g_free(cfile->frame_index_back);
      cfile->frame_index_back=NULL;
    }

    lives_system(fxcommand,FALSE);
    g_free (fxcommand);
  }
  else {
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
    tmp=g_strdup(_ ("%s all frames..."));
    text=g_strdup_printf(tmp,_(rfx->action_desc));
  }
  else {
    if (rfx->num_in_channels==2) {
      tmp=g_strdup(_ ("%s clipboard into frames %d to %d..."));
      text=g_strdup_printf(tmp,_(rfx->action_desc),cfile->progress_start,cfile->progress_end);
    }
    else {
      if (rfx->num_in_channels==0) {
	mainw->no_switch_dprint=TRUE;
	if (mainw->gen_to_clipboard) {
	  tmp=g_strdup(_("%s to clipboard..."));
	  text=g_strdup_printf(tmp,_(rfx->action_desc));
	}
	else {
	  tmp=g_strdup(_("%s to new clip..."));
	  text=g_strdup_printf(tmp,_(rfx->action_desc));
	}
      } 
      else {
	tmp=g_strdup(_ ("%s frames %d to %d..."));
	text=g_strdup_printf(tmp,_(rfx->action_desc),cfile->start,cfile->end);
      }
    }
  }

  if (!mainw->no_switch_dprint) d_print(""); // force switch text
  ldfile=mainw->last_dprint_file;

  d_print(text);
  g_free(text);
  g_free(tmp);
  mainw->last_dprint_file=ldfile;

  cfile->redoable=cfile->undoable=FALSE;
  gtk_widget_set_sensitive (mainw->redo, FALSE);
  gtk_widget_set_sensitive (mainw->undo, FALSE);

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
    tmp=g_strdup(_ ("%s clipboard with selection"));
    g_snprintf (effectstring,128,tmp,_ (rfx->action_desc));
  }
  else if (rfx->num_in_channels==0) {
    if (mainw->gen_to_clipboard) {
      tmp=g_strdup(_ ("%s to clipboard"));
      g_snprintf (effectstring,128,tmp,_ (rfx->action_desc));
    }
    else {
      tmp=g_strdup(_ ("%s to new clip"));
      g_snprintf (effectstring,128,tmp,_ (rfx->action_desc));
    }
  }
  else {
    tmp=g_strdup(_ ("%s frames %d to %d"));
    g_snprintf (effectstring,128,tmp,_ (rfx->action_desc),cfile->undo_start,cfile->undo_end);
  }
  g_free(tmp);

  if (cfile->clip_type==CLIP_TYPE_FILE&&rfx->status!=RFX_STATUS_WEED) {
    // pull a batch of frames for the backend to start processing
    cfile->fx_frame_pump=cfile->start;
  }
  else cfile->fx_frame_pump=0;

  if (!do_progress_dialog(TRUE,TRUE,effectstring)||mainw->error) {
    mainw->last_dprint_file=ldfile;
    do_rfx_cleanup(rfx);
    mainw->show_procd=TRUE;
    mainw->keep_pre=FALSE;
    if (mainw->error) {
      do_error_dialog (mainw->msg);
      d_print_failed();
      mainw->last_dprint_file=ldfile;
    }
    cfile->undo_start=oundo_start;
    cfile->undo_end=oundo_end;
    cfile->pb_fps=old_pb_fps;
    mainw->internal_messaging=FALSE;
    mainw->resizing=FALSE;
    cfile->nokeep=FALSE;
    cfile->fx_frame_pump=0;

    if (rfx->num_in_channels==0) {
      mainw->suppress_dprint=TRUE;
      close_current_file(current_file);
      mainw->suppress_dprint=FALSE;
    }
    else mainw->current_file=current_file;

    mainw->is_generating=FALSE;
    mainw->no_switch_dprint=FALSE;

    if (mainw->multitrack!=NULL) {
      mainw->pre_src_file=-1;
    }

    return FALSE;
  }

  do_rfx_cleanup(rfx);

  mainw->resizing=FALSE;
  cfile->nokeep=FALSE;
  cfile->fx_frame_pump=0;

  if (!mainw->gen_to_clipboard) {
    gtk_widget_set_sensitive (mainw->undo, TRUE);
    if (rfx->num_in_channels>0) cfile->undoable=TRUE;
    cfile->pb_fps=old_pb_fps;
    mainw->internal_messaging=FALSE;
    if (rfx->num_in_channels>0) gtk_widget_set_sensitive (mainw->select_last, TRUE);
    if (rfx->num_in_channels>0) set_undoable (_ (rfx->menu_text),TRUE);
  }

  mainw->show_procd=TRUE;
  
  if (rfx->props&RFX_PROPS_MAY_RESIZE||rfx->num_in_channels==0) {
    // get new frame size
    if (rfx->status!=RFX_STATUS_WEED) { 
      gint numtok=get_token_count (mainw->msg,'|');
      
      if (numtok>1) {
	gchar **array=g_strsplit(mainw->msg,"|",numtok);
	// [0] is "completed"
	cfile->hsize=atoi (array[1]);
	cfile->vsize=atoi (array[2]);
	if (rfx->num_in_channels==0) {
	  cfile->fps=cfile->pb_fps=strtod(array[3],NULL);
	  if (cfile->fps==0.) cfile->fps=cfile->pb_fps=prefs->default_fps;
	  cfile->end=cfile->frames=atoi(array[4]);
	  cfile->bpp=cfile->img_type==IMG_TYPE_JPEG?24:32;
	}
	g_strfreev(array);
      }
      if (rfx->num_in_channels==0) {
	cfile->progress_start=1;
	cfile->progress_end=cfile->frames;
      }
    }
    else {
      int error;
      weed_plant_t *first_out=get_enabled_channel(rfx->source,0,FALSE);
      weed_plant_t *first_ot=weed_get_plantptr_value(first_out,"template",&error);
      cfile->hsize=weed_get_int_value(first_ot,"host_width",&error);
      cfile->vsize=weed_get_int_value(first_ot,"host_height",&error);
    }

    if (rfx->num_in_channels>0) {
      if (cfile->hsize==cfile->ohsize&&cfile->vsize==cfile->ovsize) cfile->undo_action=UNDO_EFFECT;
      else {
	gboolean bad_header=FALSE;
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

  if (mainw->keep_pre) {
    // this comes from a preview which then turned into processing
    gchar *com=g_strdup_printf("smogrify mv_pre %s %d %d %s",cfile->handle,cfile->progress_start,
			       cfile->progress_end,cfile->img_type==IMG_TYPE_JPEG?"jpg":"png");
    lives_system(com,FALSE);
    g_free(com);
    mainw->keep_pre=FALSE;
  }

  if (rfx->num_in_channels==0) {
    if (rfx->props&RFX_PROPS_BATCHG) {
      // batch mode generators need some extra processing
      gchar *imgdir=g_strdup_printf("%s%s",prefs->tmpdir,cfile->handle);
      gint img_file=mainw->current_file;
      
      mainw->suppress_dprint=TRUE;
      open_file_sel(imgdir,0,0);
      g_free(imgdir);
      new_file=mainw->current_file;
      
      if (new_file!=img_file) {
	mainw->current_file=img_file;
	
	g_snprintf(mainw->files[new_file]->name,256,"%s",cfile->name);
	g_snprintf(mainw->files[new_file]->file_name,256,"%s",cfile->file_name);
	set_menu_text(mainw->files[new_file]->menuentry,cfile->name,FALSE);
	
	mainw->files[new_file]->fps=mainw->files[new_file]->pb_fps=cfile->fps;
      }
      else got_no_frames=TRUE;
      
      close_current_file(current_file);
      mainw->suppress_dprint=FALSE;
      
      if (!got_no_frames) mainw->current_file=new_file;
    }
    else {
      gchar *tfile=g_strdup_printf("%s/%s/%08d.%s",prefs->tmpdir,cfile->handle,cfile->frames,prefs->image_ext);

      if (!g_file_test (tfile, G_FILE_TEST_EXISTS)) {
	get_frame_count(mainw->current_file);
	cfile->end=cfile->frames;
      }
      g_free(tfile);
    }

    if (got_no_frames||cfile->frames==0) {
      mainw->is_generating=FALSE;
      if (!mainw->cancelled) {
	do_error_dialog(_("\nNo frames were generated.\n"));
	d_print_failed();
      }
      else d_print_cancelled();
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
      
      w_memcpy(clipboard,cfile,sizeof(file));
      cfile->is_loaded=TRUE;
      mainw->suppress_dprint=TRUE;
      mainw->only_close=TRUE;
      
      close_current_file(current_file);
      
      mainw->suppress_dprint=FALSE;
      mainw->only_close=FALSE;
      
      new_file=current_file;

      mainw->untitled_number--;
      
    }
    else {
      if (!(rfx->props&RFX_PROPS_BATCHG)) {
	// gen to new file
	cfile->is_loaded=TRUE;
	add_to_winmenu();
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
      
#ifdef ENABLE_OSC
    lives_osc_notify(LIVES_OSC_NOTIFY_CLIP_OPENED,"");
#endif
    }
    mainw->is_generating=FALSE;
  }

  if (!mainw->gen_to_clipboard) cfile->changed=TRUE;
  if (mainw->multitrack==NULL) {
    if (new_file!=-1) switch_to_file ((mainw->current_file=0),new_file);
  }
  else {
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


  

lives_render_error_t realfx_progress (gboolean reset) {
  static int i;
  GError *error=NULL;
  gchar oname[256];
  GdkPixbuf *pixbuf;
  gchar *com;
  gint64 frameticks;
  weed_plant_t *layer;
  int weed_error;
  int layer_palette;
  int retval=0;
  static int write_error;

  // this is called periodically from do_processing_dialog for internal effects

  if (reset) {
    i=cfile->start;
    clear_mainw_msg();

    if (cfile->clip_type==CLIP_TYPE_FILE) {
      if (cfile->frame_index_back!=NULL) g_free(cfile->frame_index_back);
      cfile->frame_index_back=frame_index_copy(cfile->frame_index,cfile->frames);
    }
    write_error=0;
    return LIVES_RENDER_READY;
  }

  if (mainw->effects_paused) return LIVES_RENDER_EFFECTS_PAUSED;

  // sig_progress...
  g_snprintf (mainw->msg,256,"%d",i);
  // load, effect, save frame

  // skip resizing virtual frames
  if (resize_instance!=NULL&&is_virtual_frame(mainw->current_file,i)) {
    if (++i>cfile->end) {
      mainw->internal_messaging=FALSE;
      g_snprintf(mainw->msg,9,"completed");
    }
    return LIVES_RENDER_COMPLETE;
  }

  layer=weed_plant_new(WEED_PLANT_CHANNEL);
  weed_set_int_value(layer,"clip",mainw->current_file);
  weed_set_int_value(layer,"frame",i);

  frameticks=(i-cfile->start+1.)/cfile->fps*U_SECL;

  if (!pull_frame(layer,cfile->img_type==IMG_TYPE_JPEG?"jpg":"png",frameticks)) {
    // do_read_failed_error_s() cannot be used here as we dont know the filename
    g_snprintf (mainw->msg,256,"error|missing image %d",i);
    return LIVES_RENDER_WARNING_READ_FRAME;
  }

  layer=on_rte_apply (layer, 0, 0, (weed_timecode_t)frameticks);
  layer_palette=weed_get_int_value(layer,"current_palette",&weed_error);

  if (cfile->img_type==IMG_TYPE_JPEG&&layer_palette!=WEED_PALETTE_RGB24&&layer_palette!=WEED_PALETTE_RGBA32) 
    convert_layer_palette(layer,WEED_PALETTE_RGB24,0);
  else if (cfile->img_type==IMG_TYPE_PNG&&layer_palette!=WEED_PALETTE_RGBA32) 
    convert_layer_palette(layer,WEED_PALETTE_RGBA32,0);

  if (resize_instance==NULL) resize_layer(layer,cfile->hsize,cfile->vsize,GDK_INTERP_HYPER);
  pixbuf=layer_to_pixbuf(layer);
  weed_plant_free(layer);

  g_snprintf(oname,256,"%s/%s/%08d.mgk",prefs->tmpdir,cfile->handle,i);

  do {
    lives_pixbuf_save (pixbuf, oname, cfile->img_type, 100, &error);

    if (error!=NULL) {
      retval=do_write_failed_error_s_with_retry(oname,error->message,NULL);
      g_error_free(error);
      error=NULL;
      if (retval!=LIVES_RETRY) write_error=LIVES_RENDER_ERROR_WRITE_FRAME;
    }
  } while (retval==LIVES_RETRY);
  
  gdk_pixbuf_unref (pixbuf);

  if (cfile->clip_type==CLIP_TYPE_FILE) {
    cfile->frame_index[i-1]=-1;
  }
  
  if (++i>cfile->end) {
    com=g_strdup_printf ("smogrify mv_mgk %s %d %d %s",cfile->handle,cfile->start,
			 cfile->end,cfile->img_type==IMG_TYPE_JPEG?"jpg":"png");
    lives_system (com,FALSE);
    g_free (com);
    mainw->internal_messaging=FALSE;

    if (cfile->clip_type==CLIP_TYPE_FILE) {
      if (!check_if_non_virtual(mainw->current_file)) save_frame_index(mainw->current_file);
    }
    return LIVES_RENDER_COMPLETE;
  }
  if (write_error) return write_error;
  return LIVES_RENDER_PROCESSING;
}




gboolean on_realfx_activate_inner(gint type, lives_rfx_t *rfx) {
  // type can be 0 - apply current realtime effects
  // 1 - resize (using weed filter)
  gboolean retval;

  if (type==1) resize_instance=rfx->source;
  else resize_instance=NULL;

  mainw->internal_messaging=TRUE;
  framecount=0;
  mainw->progress_fn=&realfx_progress;
  mainw->progress_fn (TRUE);

  retval=do_effect (rfx,FALSE);

  resize_instance=NULL;
  return retval;

}




void on_realfx_activate (GtkMenuItem *menuitem, gpointer user_data) {
  gint type;

  if (menuitem!=NULL) type=0;
  else type=1;

  on_realfx_activate_inner(type,(lives_rfx_t *)user_data);
}





weed_plant_t *on_rte_apply (weed_plant_t *layer, int opwidth, int opheight, weed_timecode_t tc) {
  // realtime effects
  weed_plant_t **layers,*retlayer;
  int i;

  if (mainw->foreign) return NULL;

  layers=(weed_plant_t **)g_malloc(3*sizeof(weed_plant_t *));

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
  }
  else layers[1]=NULL;

  if (resize_instance!=NULL) {
    weed_plant_t *init_event=weed_plant_new(WEED_PLANT_EVENT);
    weed_set_int_value(init_event,"in_tracks",0);
    weed_set_int_value(init_event,"out_tracks",0);

    weed_apply_instance(resize_instance,init_event,layers,0,0,tc);
    retlayer=layers[0];

    weed_plant_free(init_event);
  }
  else {
    retlayer=weed_apply_effects(layers,mainw->filter_map,tc,opwidth,opheight,mainw->pchains);
  }

  // all our pixel_data will have been free'd already
  for (i=0;layers[i]!=NULL;i++) {
    if (layers[i]!=retlayer) weed_plant_free(layers[i]);
  }
  g_free(layers);

  return retlayer;
}





void deinterlace_frame(weed_plant_t *layer, weed_timecode_t tc) {
  int deint_idx;
  weed_plant_t *deint_filter,*deint_instance,*init_event;
  weed_plant_t **layers;

  if (mainw->fx_candidates[FX_CANDIDATE_DEINTERLACE].delegate==-1) return;

  deint_idx=GPOINTER_TO_INT(g_list_nth_data(mainw->fx_candidates[FX_CANDIDATE_DEINTERLACE].list,
					    mainw->fx_candidates[FX_CANDIDATE_DEINTERLACE].delegate));

  deint_filter=get_weed_filter(deint_idx);

  deint_instance=weed_instance_from_filter(deint_filter);

  layers=(weed_plant_t **)g_malloc(2*sizeof(weed_plant_t *));

  layers[1]=NULL;

  layers[0]=layer;
  
  init_event=weed_plant_new(WEED_PLANT_EVENT);
  weed_set_int_value(init_event,"in_tracks",0);
  weed_set_int_value(init_event,"out_tracks",0);
  
  weed_apply_instance(deint_instance,init_event,layers,0,0,tc);
  
  weed_plant_free(init_event);
  weed_call_deinit_func(deint_instance);
  weed_instance_unref(deint_instance);

  g_free(layers);
}










weed_plant_t *get_blend_layer(weed_timecode_t tc) {
  file *blend_file;
  weed_plant_t *layer;
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

  blend_file->frameno=calc_new_playback_position(mainw->blend_file,blend_tc,&ntc);

  blend_tc=ntc;

  layer=weed_plant_new(WEED_PLANT_CHANNEL);
  weed_set_int_value(layer,"clip",mainw->blend_file);
  weed_set_int_value(layer,"frame",blend_file->frameno);
  if (!pull_frame(layer,blend_file->img_type==IMG_TYPE_JPEG?"jpg":"png",tc)) {
    weed_plant_free(layer);
    layer=NULL;
  }
  return layer;
}


////////////////////////////////////////////////////////////////////
// keypresses





gboolean rte_on_off_callback (GtkAccelGroup *group, GObject *obj, guint keyval, GdkModifierType mod, gpointer user_data)
{
// this is the callback which happens when a rte is keyed
  gint key=GPOINTER_TO_INT(user_data);
  guint new_rte=GU641<<(key-1);

  if (key==EFFECT_NONE) {
    // switch up/down keys to default (fps change)
    weed_deinit_all();
  }
  else {
    mainw->rte^=new_rte;
    if (mainw->rte&new_rte) {
      // switch is ON
      // WARNING - if we start playing because a generator was started, we block here
      mainw->osc_block=TRUE;
      mainw->last_grabable_effect=key-1;

      if (rte_window!=NULL) rtew_set_keych(key-1,TRUE);
      if (!(weed_init_effect(key-1))) {
	// ran out of instance slots, no effect assigned, or some other error
	mainw->rte^=new_rte;
	if (rte_window!=NULL&&group!=NULL) rtew_set_keych(key-1,FALSE);
      }
      mainw->osc_block=FALSE;
      return TRUE;
    }
    else {
      // effect is OFF
      mainw->osc_block=TRUE;
      weed_deinit_effect(key-1);
      if (mainw->rte&(GU641<<(key-1))) mainw->rte^=(GU641<<(key-1));
      if (rte_window!=NULL&&group!=NULL) rtew_set_keych(key-1,FALSE);
      mainw->osc_block=FALSE;
    }
  }
  if (mainw->current_file>0&&cfile->play_paused&&!mainw->noswitch) {
    load_frame_image (cfile->frameno);
  }
  return TRUE;
}



gboolean rte_on_off_callback_hook (GtkToggleButton *button, gpointer user_data) {
  rte_on_off_callback (NULL, NULL, 0, 0, user_data);
  return TRUE;
}


gboolean grabkeys_callback (GtkAccelGroup *group, GObject *obj, guint keyval, GdkModifierType mod, gpointer user_data) {
  // assign the keys to the last key-grabable effect 
  mainw->rte_keys=mainw->last_grabable_effect;
  mainw->osc_block=TRUE;
  if (rte_window!=NULL) {
    if (group!=NULL) rtew_set_keygr(mainw->rte_keys);
  }
  mainw->blend_factor=weed_get_blend_factor(mainw->rte_keys);
  mainw->osc_block=FALSE;
  return TRUE;
}



gboolean textparm_callback (GtkAccelGroup *group, GObject *obj, guint keyval, GdkModifierType mod, gpointer user_data) {
  // keyboard linked to first string parameter, until TAB is pressed
  mainw->rte_textparm=get_textparm();
  return TRUE;
}



gboolean grabkeys_callback_hook (GtkToggleButton *button, gpointer user_data) {
  if (!gtk_toggle_button_get_active(button)) return TRUE;
  mainw->last_grabable_effect=GPOINTER_TO_INT(user_data);
  grabkeys_callback (NULL, NULL, 0, 0, user_data);
  return TRUE;
}


gboolean rtemode_callback (GtkAccelGroup *group, GObject *obj, guint keyval, GdkModifierType mod, gpointer user_data) {
  if (mainw->rte_keys==-1) return TRUE;
  rte_key_setmode(0,-1);
  mainw->blend_factor=weed_get_blend_factor(mainw->rte_keys);
  return TRUE;
}


gboolean rtemode_callback_hook (GtkToggleButton *button, gpointer user_data) {
  gint key_mode=GPOINTER_TO_INT(user_data);
  int modes=rte_getmodespk();
  gint key=(gint)(key_mode/modes);
  gint mode=key_mode-key*modes;

  if (!gtk_toggle_button_get_active(button)) return TRUE;

  rte_key_setmode(key+1,mode);
  return TRUE;
}


gboolean swap_fg_bg_callback (GtkAccelGroup *group, GObject *obj, guint keyval, GdkModifierType mod, gpointer user_data) {
  gint old_file=mainw->current_file;

  if (mainw->playing_file<1||mainw->num_tr_applied==0||mainw->noswitch||mainw->blend_file==-1||
      mainw->blend_file==mainw->current_file||mainw->files[mainw->blend_file]==NULL||mainw->preview||
      mainw->noswitch||(mainw->is_processing&&cfile->is_loaded)) {
    return TRUE;
  }

  do_quick_switch (mainw->blend_file);

  mainw->blend_file=old_file;

  rte_swap_fg_bg();
  return TRUE;

  // **TODO - for weed, invert all transition parameters for any active effects
}






//////////////////////////////////////////////////////////////



