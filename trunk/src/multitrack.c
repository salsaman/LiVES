// multitrack.c
// LiVES
// (c) G. Finch 2005 - 2012 <salsaman@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

// multitrack window

// layout is:

// play | clips/params | ctx menu
// ------------------------------
//            timeline
// ------------------------------
//            messages


// the multitrack window is designed to be more-or-less standalone
// it relies on functions in other files for applying effects and rendering
// we use a Weed event list to store our timeline
// (see weed events extension in weed-docs directory

// future plans include timeline plugins, which would generate event lists 
// or adjust the currently playing one
// and it would be nice to be able to read/write event lists in other formats than the default

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
#include "events.h"
#include "callbacks.h"
#include "effects.h"
#include "resample.h"
#include "support.h"
#include "paramwindow.h"
#include "interface.h"
#include "audio.h"
#include "startup.h"

#ifdef ENABLE_GIW
#include "giw/giwvslider.h"
#include "giw/giwled.h"
#endif

#define BORD_HEIGHT 10

static int renumbered_clips[MAX_FILES+1]; // used to match clips from the event recorder with renumbered clips (without gaps)
static gdouble lfps[MAX_FILES+1]; // table of layout fps
static void **pchain; // param chain for currently being edited filter

static GdkColor audcol;
static GdkColor fxcol;

static gint xachans,xarate,xasamps,xse;
static gboolean ptaud;
static gint btaud;

static gint aofile;
static int afd;

static gint dclick_time=0;

static gboolean force_pertrack_audio;
static gint force_backing_tracks;

static gint clips_to_files[MAX_FILES];

static gboolean pb_audio_needs_prerender;
static weed_plant_t *pb_loop_event,*pb_filter_map;

static gboolean mainw_was_ready;

///////////////////////////////////////////////////////////////////

#define LIVES_AVOL_SCALE ((double)1000000.)


static LIVES_INLINE gint mt_file_from_clip(lives_mt *mt, gint clip) {
  return clips_to_files[clip];
}

static LIVES_INLINE gint mt_clip_from_file(lives_mt *mt, gint file) {
  register int i;
  for (i=0;i<MAX_FILES;i++) {
    if (clips_to_files[i]==file) return i;
  }
  return -1;
}

/// return track number for a given block
///
static LIVES_INLINE int get_track_for_block(track_rect *block) {
  return GPOINTER_TO_INT(g_object_get_data(G_OBJECT(block->eventbox),"layer_number"));  
}



static LIVES_INLINE gboolean is_empty_track(GObject *track) {
  return (g_object_get_data(track, "blocks")==NULL);
}

gdouble get_mixer_track_vol(lives_mt *mt, int trackno) {
  gdouble vol=(gdouble)(GPOINTER_TO_INT(g_list_nth_data(mt->audio_vols,trackno)));
  return vol/LIVES_AVOL_SCALE;
}

void set_mixer_track_vol(lives_mt *mt, int trackno, gdouble vol) {
  int x=vol*LIVES_AVOL_SCALE;
  g_list_nth(mt->audio_vols,trackno)->data=GINT_TO_POINTER(x);
}



static gboolean save_event_list_inner(lives_mt *mt, int fd, weed_plant_t *event_list, unsigned char **mem) {
  weed_plant_t *event=get_first_event(event_list);

  threaded_dialog_spin();

  weed_set_int_value(event_list,"width",cfile->hsize);
  weed_set_int_value(event_list,"height",cfile->vsize);
  weed_set_int_value(event_list,"audio_channels",cfile->achans);
  weed_set_int_value(event_list,"audio_rate",cfile->arate);
  weed_set_int_value(event_list,"audio_sample_size",cfile->asampsize);
  if (cfile->signed_endian&AFORM_UNSIGNED) weed_set_boolean_value(event_list,"audio_signed",WEED_FALSE);
  else weed_set_boolean_value(event_list,"audio_signed",WEED_TRUE);
  if (cfile->signed_endian&AFORM_BIG_ENDIAN) weed_set_int_value(event_list,"audio_endian",1);
  else weed_set_int_value(event_list,"audio_endian",0);
  
  if (mt!=NULL&&mt->audio_vols!=NULL&&mt->audio_draws!=NULL) {
    int i;
    gint natracks=g_list_length(mt->audio_draws);
    int *atracks=(int *)g_malloc(natracks*sizint);
    gdouble *avols;
    gint navols;
    for (i=0;i<natracks;i++) {
      atracks[i]=i-mt->opts.back_audio_tracks;
    }
    weed_set_int_array(event_list,"audio_volume_tracks",natracks,atracks);
    g_free(atracks);
    
    if (mt->opts.gang_audio) navols=1+mt->opts.back_audio_tracks;
    else navols=natracks;
    
    avols=(gdouble *)g_malloc(navols*sizeof(double));
    for (i=0;i<navols;i++) {
      avols[i]=get_mixer_track_vol(mt,i);
    }
    weed_set_double_array(event_list,"audio_volume_values",navols,avols);
    g_free(avols);
  }

  if (mem==NULL&&fd<0) return TRUE;

  threaded_dialog_spin();

  mainw->write_failed=FALSE;
  weed_plant_serialise(fd,event_list,mem);
  while (!mainw->write_failed&&event!=NULL) {
    if (WEED_EVENT_IS_FILTER_INIT(event)) weed_set_voidptr_value(event,"event_id",(void *)event);
    weed_plant_serialise(fd,event,mem);
    event=get_next_event(event);
    threaded_dialog_spin();
  }

  if (mainw->write_failed) return FALSE;
  return TRUE;

}


static LiVESPixbuf *make_thumb (lives_mt *mt, int file, int width, int height, int frame, boolean noblanks) {
  LiVESPixbuf *thumbnail=NULL,*pixbuf;
  GError *error=NULL;
  char *buf;

  boolean tried_all=FALSE;

  int nframe,oframe=frame;

  if (file<1) {
    LIVES_WARN("Warning - make thumb for file");
    return NULL;
  }

  if (width<2||height<2) return NULL;

  if (mt->idlefunc>0) g_source_remove(mt->idlefunc);

  do {

    if (mainw->files[file]->frames>0) {
      weed_timecode_t tc=(frame-1.)/mainw->files[file]->fps*U_SECL;
      thumbnail=pull_lives_pixbuf_at_size(file,frame,mainw->files[file]->img_type==IMG_TYPE_JPEG?"jpg":"png",tc,
					  width,height,LIVES_INTERP_BEST);
    }
    else {
      buf=g_build_filename(prefs->prefix_dir,ICON_DIR,"audio.png",NULL);
      pixbuf=lives_pixbuf_new_from_file_at_scale(buf,width,height,FALSE,&error);
      // ...at_scale is inaccurate !
      
      g_free(buf);
      if (error!=NULL||pixbuf==NULL) {
	g_error_free(error);
	if (mt->idlefunc>0) {
	  mt->idlefunc=0;
	  mt->idlefunc=mt_idle_add(mt);
	}
	return NULL;
      }
      
      if (lives_pixbuf_get_width(pixbuf)!=width||lives_pixbuf_get_height(pixbuf)!=height) {
	// ...at_scale is inaccurate
	thumbnail=lives_pixbuf_scale_simple(pixbuf,width,height,LIVES_INTERP_BEST);
	gdk_pixbuf_unref(pixbuf);
      }
      else thumbnail=pixbuf;
    }

    if (tried_all) noblanks=FALSE;

    if (noblanks&&!lives_pixbuf_is_all_black(thumbnail)) noblanks=FALSE;
    if (noblanks) {
      nframe=frame+mainw->files[file]->frames/10.;
      if (nframe==frame) nframe++;
      if (nframe>mainw->files[file]->frames) {
	nframe=oframe;
	tried_all=TRUE;
      }
      frame=nframe;
      if (thumbnail!=NULL) gdk_pixbuf_unref(thumbnail);
      thumbnail=NULL;
    }


  } while (noblanks);

  if (mt->idlefunc>0) {
    mt->idlefunc=0;
    mt->idlefunc=mt_idle_add(mt);
  }

  return thumbnail;
}


static void set_cursor_style(lives_mt *mt, gint cstyle, gint width, gint height, gint clip, gint hsx, gint hsy) {
  GdkPixbuf *pixbuf=NULL;
  guchar *cpixels,*tpixels;
  int i,j,k;
  GdkPixbuf *thumbnail=NULL;
  gint twidth=0,twidth3,twidth4,trow;
  file *sfile=mainw->files[clip];

  int frame_start;
  gdouble frames_width;

  unsigned int cwidth,cheight;

  if (mt->cursor!=NULL) gdk_cursor_unref(mt->cursor);
  mt->cursor=NULL;

  gdk_display_get_maximal_cursor_size(gdk_screen_get_display(gtk_widget_get_screen(mt->window)),&cwidth,&cheight);
  if (width>cwidth) width=cwidth;

  mt->cursor_style=cstyle;
  switch(cstyle) {
  case LIVES_CURSOR_NORMAL:
    gdk_window_set_cursor (mt->window->window, NULL);
    return;
  case LIVES_CURSOR_BUSY:
    mt->cursor=gdk_cursor_new(GDK_WATCH);
    gdk_window_set_cursor (mt->window->window, mt->cursor);
    return;
  case LIVES_CURSOR_BLOCK:
    if (sfile!=NULL&&sfile->frames>0) {
      frame_start=mt->opts.ign_ins_sel?1:sfile->start;
      frames_width=(gdouble)(mt->opts.ign_ins_sel?sfile->frames:sfile->end-sfile->start+1.);

      pixbuf=lives_pixbuf_new (TRUE, width, height);

      for (i=0;i<width;i+=BLOCK_THUMB_WIDTH) {
	// create a small thumb
	twidth=BLOCK_THUMB_WIDTH;
	if ((i+twidth)>width) twidth=width-i;
	if (twidth>=2) {
	  thumbnail=make_thumb(mt,clip,twidth,height,frame_start+(gint)((gdouble)i/(gdouble)width*frames_width),FALSE);
	  // render it in the eventbox
	  if (thumbnail!=NULL) {
	    trow=lives_pixbuf_get_rowstride(thumbnail);
	    twidth=lives_pixbuf_get_width(thumbnail);
	    cpixels=gdk_pixbuf_get_pixels(pixbuf)+(i*4);
	    tpixels=gdk_pixbuf_get_pixels(thumbnail);

	    if (!lives_pixbuf_get_has_alpha(thumbnail)) {
	      twidth3=twidth*3;
	      for (j=0;j<height;j++) {
		for (k=0;k<twidth3;k+=3) {
		  memcpy(cpixels,&tpixels[k],3);
		  memset(cpixels+3,0xFF,1);
		  cpixels+=4;
		}
		tpixels+=trow;
		cpixels+=(width-twidth)<<2;
	      }
	    }
	    else {
	      twidth4=twidth*4;
	      for (j=0;j<height;j++) {
		memcpy(cpixels,tpixels,twidth4);
		tpixels+=trow;
		cpixels+=width<<2;
	      }
	    }
	    gdk_pixbuf_unref(thumbnail);
	  }
	}
      }
      break;
    }
    // fallthrough
  case LIVES_CURSOR_AUDIO_BLOCK:
    pixbuf=lives_pixbuf_new (TRUE, width, height);
    trow=lives_pixbuf_get_rowstride(pixbuf);
    cpixels=gdk_pixbuf_get_pixels(pixbuf);
    for (j=0;j<height;j++) {
      for (k=0;k<width;k++) {
	cpixels[0]=audcol.red;
	cpixels[1]=audcol.green;
	cpixels[2]=audcol.blue;
	cpixels[3]=0xFF;
	cpixels+=4;
      }
      cpixels+=(trow-width*4);
    }
    break;
  case LIVES_CURSOR_FX_BLOCK:
    pixbuf=lives_pixbuf_new (TRUE, width, height);
    trow=lives_pixbuf_get_rowstride(pixbuf);
    cpixels=gdk_pixbuf_get_pixels(pixbuf);
    for (j=0;j<height;j++) {
      for (k=0;k<width;k++) {
	cpixels[0]=fxcol.red;
	cpixels[1]=fxcol.green;
	cpixels[2]=fxcol.blue;
	cpixels[3]=0xFF;
	cpixels+=4;
      }
      cpixels+=(trow-width*4);
    }
    break;
  }

  mt->cursor = gdk_cursor_new_from_pixbuf (gdk_display_get_default(), pixbuf, hsx, hsy);
  gdk_window_set_cursor (mt->window->window, mt->cursor);
  gdk_pixbuf_unref (pixbuf);

}



gboolean write_backup_layout_numbering(lives_mt *mt) {
  // link clip numbers in the auto save event_list to actual clip numbers

  int fd,i,vali,hdlsize;
  gdouble vald;
  gchar *asave_file=g_strdup_printf("%s/layout_numbering.%d.%d.%d",prefs->tmpdir,lives_getuid(),lives_getgid(),
				    lives_getpid());
  GList *clist=mainw->cliplist;

  fd=creat(asave_file,DEF_FILE_PERMS);
  g_free(asave_file);

  mainw->write_failed=FALSE;

  if (fd!=-1) {
    while (mainw->write_failed&&clist!=NULL) {
      i=GPOINTER_TO_INT(clist->data);
      if (mainw->files[i]->clip_type!=CLIP_TYPE_DISK&&mainw->files[i]->clip_type!=CLIP_TYPE_FILE) {
	clist=clist->next;
	continue;

      }
      if (mt!=NULL) {
	lives_write_le(fd,&i,4,TRUE);
	vald=mainw->files[i]->fps;
	lives_write_le(fd,&vald,8,TRUE);
	hdlsize=strlen(mainw->files[i]->handle);
	lives_write_le (fd,&hdlsize,4,TRUE);
	lives_write (fd,&mainw->files[i]->handle,hdlsize,TRUE);
      }
      else {
	vali=mainw->files[i]->stored_layout_idx;
	if (vali!=-1) {
	  lives_write_le(fd,&vali,4,TRUE);
	  vald=mainw->files[i]->fps;
	  lives_write_le(fd,&vald,8,TRUE);
	  hdlsize=strlen(mainw->files[i]->handle);
	  lives_write_le (fd,&hdlsize,4,TRUE);
	  lives_write (fd,&mainw->files[i]->handle,hdlsize,TRUE);
	}
      }
      clist=clist->next;
    }
    
    close(fd);
  }

  if (mainw->write_failed) return FALSE;
  return TRUE;

}


static void renumber_from_backup_layout_numbering(lives_mt *mt) {
  // this is used only for crash recovery

  // layout_numbering simply maps our clip handles to clip numbers in the current layout
  // we assume the order hasnt changed (it cant) and there are no gaps (we have just reloaded)

  //but the numbering may have changed (for example we started last time in mt mode, this time in ce mode)

  int fd,vari,clipn,offs;
  gdouble vard;
  gchar *aload_file=g_strdup_printf("%s/layout_numbering.%d.%d.%d",prefs->tmpdir,lives_getuid(),lives_getgid(),
				    lives_getpid());
  gboolean isfirst=TRUE;
  char buf[256];

  fd=open(aload_file,O_RDONLY);

  if (fd!=-1) {
    while (1) {
      if (lives_read_le(fd,&clipn,4,TRUE)==4) {
	if (isfirst) offs=-clipn+1;
	else isfirst=FALSE;
	if (lives_read_le(fd,&vard,8,TRUE)==8) {

	  if (lives_read_le(fd,&vari,4,TRUE)==4) {
	    // compare the handle - assume clip ordering has not changed
	    if (vari>255) vari=255;
	    if (read(fd,buf,vari)==vari) {
	      memset(buf+vari,0,1);
	      while (mainw->files[clipn+offs]!=NULL&&strcmp(mainw->files[clipn+offs]->handle,buf)) {
		offs++;
	      }
	      if (mainw->files[clipn+offs]==NULL) break;
	      // got a match - index the current clip order -> clip order in layout
	      renumbered_clips[clipn]=clipn+offs;
	      // lfps contains the fps at the time of the crash
	      lfps[clipn+offs]=vard;
	    }
	    else break;
	  }
	  else break;
	}
	else break;
      }
      else break;
    }
    close(fd);
  }
}





static void save_mt_autoback(lives_mt *mt, int64_t stime) {
  // auto backup of the current layout

  // this is called from an idle funtion - if the specified amount of time has passed and
  // the clip has been altered

  struct timeval otv;

  int fd;
  gchar *asave_file=g_strdup_printf("%s/layout.%d.%d.%d",prefs->tmpdir,lives_getuid(),lives_getgid(),lives_getpid());
  lives_mt_poly_state_t poly_state;

  gboolean retval=TRUE;
  int retval2;

  mt_desensitise(mt);

  // flush any pending events
  while (g_main_context_iteration(NULL,FALSE));

  do {
    retval2=0;
    mainw->write_failed=FALSE;

    fd=creat(asave_file,DEF_FILE_PERMS);
    if (fd>=0) {
      add_markers(mt,mt->event_list);
      do_threaded_dialog(_("Auto backup"),FALSE);
      retval=save_event_list_inner(mt,fd,mt->event_list,NULL);
      end_threaded_dialog();
      
      if (retval) retval=write_backup_layout_numbering(mt);
      
      remove_markers(mt->event_list);
      close(fd);
    }
    else mainw->write_failed=TRUE;
    
    poly_state=mt->poly_state;
    if (mt->poly_state!=POLY_IN_OUT) mt->poly_state=POLY_NONE;
    mt_sensitise(mt);
    mt->poly_state=poly_state;
    
    if (!mainw->write_failed) mt->auto_changed=FALSE;
    else mainw->write_failed=FALSE;
    
    if (!retval||mainw->write_failed) {
      retval2=do_write_failed_error_s_with_retry(asave_file,NULL,NULL);
    }
  } while (retval2==LIVES_RETRY);

  g_free(asave_file);

  if (stime==0) {
    gettimeofday(&otv, NULL);
    stime=otv.tv_sec;
  }
    
  mt->auto_back_time=stime;
    

}



static gboolean mt_auto_backup(gpointer user_data) {

  struct timeval otv;
  int64_t stime,diff;
  gboolean had_idlefunc=FALSE;

  lives_mt *mt=(lives_mt *)user_data;

  if (!mt->auto_changed||mt->event_list==NULL||mt->idlefunc==0||prefs->mt_auto_back<0) {
    mt->idlefunc=0;
    return FALSE;
  }

  if (mt->idlefunc>0) {
    g_source_remove(mt->idlefunc);
    mt->idlefunc=0;
    had_idlefunc=TRUE;
  }

  gettimeofday(&otv, NULL);
  stime=otv.tv_sec;

  if (mt->auto_back_time==0) mt->auto_back_time=stime;

  diff=stime-mt->auto_back_time;
  if (diff>=prefs->mt_auto_back) {
    save_mt_autoback(mt,stime);
  }


  if (had_idlefunc) {
    mt->idlefunc=mt_idle_add(mt);
  }

  return TRUE;
}


guint mt_idle_add(lives_mt *mt) {
  if (prefs->mt_auto_back<0) return 0;

  if (prefs->mt_auto_back==0) {
    mt->idlefunc=-1;
    mt_auto_backup(mt);
    return 0;
  }

  if (mt->idlefunc>0) return mt->idlefunc;

  return g_idle_add_full(G_PRIORITY_LOW,mt_auto_backup,mt,NULL);
}


void recover_layout_cancelled(GtkButton *button, gpointer user_data) {
  gchar *eload_file=g_strdup_printf("%s/layout.%d.%d.%d",prefs->tmpdir,lives_getuid(),lives_getgid(),lives_getpid());

  if (button!=NULL) {
    gtk_widget_destroy(gtk_widget_get_toplevel(GTK_WIDGET(button)));
    mainw->recoverable_layout=FALSE;
  }

  unlink(eload_file);
  g_free(eload_file);
  
  eload_file=g_strdup_printf("%s/layout_numbering.%d.%d.%d",prefs->tmpdir,lives_getuid(),lives_getgid(),lives_getpid());
  unlink(eload_file);
  g_free(eload_file);

  if (button!=NULL) do_after_crash_warning();

}



static void mt_load_recovery_layout(lives_mt *mt) {
    gchar *aload_file=g_strdup_printf("%s/layout_numbering.%d.%d.%d",prefs->tmpdir,lives_getuid(),lives_getgid(),
				      lives_getpid());
    gchar *eload_file=g_strdup_printf("%s/layout.%d.%d.%d",prefs->tmpdir,lives_getuid(),lives_getgid(),lives_getpid());

    mt->auto_reloading=TRUE;
    mainw->event_list=mt->event_list=load_event_list(mt,eload_file);
    mt->auto_reloading=FALSE;
    if (mt->event_list!=NULL) {
      unlink(eload_file);
      unlink(aload_file);
      mt_init_tracks(mt,TRUE);
      remove_markers(mt->event_list);
      save_mt_autoback(mt,0);
    }
    else {
      // failed to load
      // keep the faulty layout for forensic purposes
      gchar *com;
      gchar *uldir=g_build_filename(prefs->tmpdir,"unrecoverable_layouts/",NULL);

      com=g_strdup_printf("/bin/mkdir -p \"%s\" 2>/dev/null",uldir);
      lives_system(com,TRUE);
      g_free(com);

      com=g_strdup_printf("/bin/mv \"%s\" \"%s\"",eload_file,uldir);
      lives_system(com,TRUE);
      g_free(com);

      com=g_strdup_printf("/bin/mv \"%s\" \"%s\"",aload_file,uldir);
      lives_system(com,TRUE);
      g_free(com);

      mt->fps=prefs->mt_def_fps;
      g_free(uldir);
    }

    g_free(eload_file);
    g_free(aload_file);

}


void recover_layout(GtkButton *button, gpointer user_data) {
  gtk_widget_destroy(gtk_widget_get_toplevel(GTK_WIDGET(button)));
  while (g_main_context_iteration (NULL,FALSE));
  if (prefs->startup_interface==STARTUP_CE) {
    if (!on_multitrack_activate(NULL,NULL)) {
      multitrack_delete(mainw->multitrack,FALSE);
      do_bad_layout_error();
    }
  }
  else {
    mainw->multitrack->auto_reloading=TRUE;
    set_pref("ar_layout",""); // in case we crash...
    mt_load_recovery_layout(mainw->multitrack);
    mainw->multitrack->auto_reloading=FALSE;
    mt_sensitise(mainw->multitrack);
    mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
  }
  mainw->recoverable_layout=FALSE;

  do_after_crash_warning();

}
  



void **mt_get_pchain(void) {
  return pchain;
}


LIVES_INLINE gchar *get_track_name(lives_mt *mt, int track_num, gboolean is_audio) {
  GtkWidget *xeventbox;
  if (track_num<0) return g_strdup(_("Backing audio")); 
  if (!is_audio) xeventbox=(GtkWidget *)g_list_nth_data(mt->video_draws,track_num);
  else xeventbox=(GtkWidget *)g_list_nth_data(mt->audio_draws,track_num+mt->opts.back_audio_tracks);
  return g_strdup((gchar *)g_object_get_data(G_OBJECT(xeventbox),"track_name"));
}


LIVES_INLINE gdouble get_time_from_x(lives_mt *mt, gint x) {
  gdouble time=(gdouble)x/(gdouble)mt->timeline->allocation.width*(mt->tl_max-mt->tl_min)+mt->tl_min;
  if (time<0.) time=0.;
  else if (time>mt->end_secs+1./mt->fps) time=mt->end_secs+1./mt->fps;
  return q_dbl(time,mt->fps)/U_SEC;
}


LIVES_INLINE void set_params_unchanged(lives_rfx_t *rfx) {
  int i;
  for (i=0;i<rfx->num_params;i++) rfx->params[i].changed=FALSE;
}

static gint get_track_height(lives_mt *mt) {
  GtkWidget *eventbox;
  GList *glist=mt->video_draws;

  while (glist!=NULL) {
    eventbox=(GtkWidget *)glist->data;
    if (GPOINTER_TO_INT(g_object_get_data(G_OBJECT(eventbox),"hidden"))==0) 
      return GTK_WIDGET(eventbox)->allocation.height;
    glist=glist->next;
  }

  return 0;
}


static gboolean is_audio_eventbox(lives_mt *mt, GtkWidget *ebox) {
  GList *slist=mt->audio_draws;

  while (slist!=NULL) {
    if (ebox==slist->data) return TRUE;
    slist=slist->next;
  }
  return FALSE;
}


static void draw_block (lives_mt *mt,track_rect *block, gint x1, gint x2) {
  // x1 is start point of drawing area (in pixels), x2 is width of drawing area (in pixels)

  gdouble tl_span=mt->tl_max-mt->tl_min;
  GtkWidget *eventbox=block->eventbox;
  weed_plant_t *event=block->start_event;
  weed_timecode_t tc=get_event_timecode(event);

  gdouble offset_startd=tc/U_SEC;
  gdouble offset_endd;
  gint offset_start;
  gint offset_end;
  int i,filenum,track;
  GdkPixbuf *thumbnail=NULL;
  int framenum,last_framenum;
  gint width=BLOCK_THUMB_WIDTH;

  gint hidden=(gint)GPOINTER_TO_INT(g_object_get_data (G_OBJECT(eventbox), "hidden"));
  if (hidden) return;

  // block to right of screen
  if (offset_startd>=mt->tl_max) return;

  // block to right of drawing area
  offset_start=(int)((offset_startd-mt->tl_min)/tl_span*eventbox->allocation.width+.5);
  if ((x1>0||x2>0)&&offset_start>(x1+x2)) return;

  offset_endd=get_event_timecode(block->end_event)/U_SEC+(!is_audio_eventbox(mt,eventbox))/cfile->fps;
  offset_end=(offset_endd-mt->tl_min)/tl_span*eventbox->allocation.width;

  //if (offset_end+offset_start>eventbox->allocation.width) offset_end=eventbox->allocation.width-offset_start;

  // end of block before drawing area
  if (offset_end<x1) return;

  switch (block->state) {
  case BLOCK_UNSELECTED:
    track=GPOINTER_TO_INT(g_object_get_data(G_OBJECT(eventbox),"layer_number"));
    if (BLOCK_DRAW_TYPE==BLOCK_DRAW_SIMPLE) {
      gdk_draw_line (GDK_DRAWABLE(eventbox->window), mt->window->style->black_gc, offset_start, 0, offset_start, 
		     eventbox->allocation.height);
      gdk_draw_line (GDK_DRAWABLE(eventbox->window), mt->window->style->black_gc, offset_end, 
		     eventbox->allocation.height, offset_end, 0);
      gdk_draw_line (GDK_DRAWABLE(eventbox->window), mt->window->style->black_gc, offset_start, 0, offset_end, 
		     eventbox->allocation.height);
      gdk_draw_line (GDK_DRAWABLE(eventbox->window), mt->window->style->black_gc, offset_start, 
		     eventbox->allocation.height, offset_end, 0);
      gdk_draw_line (GDK_DRAWABLE(eventbox->window), mt->window->style->black_gc, offset_start, 0, offset_end, 0);
      gdk_draw_line (GDK_DRAWABLE(eventbox->window), mt->window->style->black_gc, offset_start, 
		     eventbox->allocation.height-1, offset_end, eventbox->allocation.height-1);
    }
    else {
      if ((cfile->achans==0||!is_audio_eventbox(mt,eventbox))&&track>-1) {
	cairo_t *cr = gdk_cairo_create (eventbox->window);
	filenum=get_frame_event_clip(block->start_event,track);
	last_framenum=-1;
	for (i=offset_start;i<offset_end;i+=BLOCK_THUMB_WIDTH) {
	  if (i>x2) break;
	  event=get_frame_event_at(mt->event_list,tc,event,FALSE);
	  tc+=tl_span/eventbox->allocation.width*width*U_SEC;
	  if (i+width>=0) {
	    // create a small thumb
	    framenum=get_frame_event_frame(event,track);
	    
	    if (thumbnail!=NULL) gdk_pixbuf_unref(thumbnail);
	    thumbnail=NULL;
	    if (framenum!=last_framenum) thumbnail=make_thumb(mt,filenum,width,eventbox->allocation.height-1,
							      framenum,FALSE);
	    last_framenum=framenum;
	    // render it in the eventbox
	    if (thumbnail!=NULL) {
	      gdk_cairo_set_source_pixbuf (cr, thumbnail, i, 0);
	      if (i+width>offset_end) {
		width=offset_end-i;
		// crop to width
		cairo_new_path(cr);
		cairo_rectangle(cr,0,0,i+width,eventbox->allocation.height-1);
		cairo_clip(cr);
	      }
	      cairo_paint (cr);
	    }
	    if (mainw->playing_file>-1) unpaint_lines(mt);
	    mt->redraw_block=TRUE; // stop drawing cursor during playback
	    if (mainw->playing_file>-1&&mainw->cancelled==CANCEL_NONE) process_one(FALSE);
	    mt->redraw_block=FALSE;
	  }
	}
	if (thumbnail!=NULL) gdk_pixbuf_unref(thumbnail);
	cairo_destroy (cr);
      }
      else {
	set_fg_colour(audcol.red>>8,audcol.green>>8,audcol.blue>>8);
	gdk_draw_rectangle (GDK_DRAWABLE(eventbox->window), mainw->general_gc, TRUE, offset_start, 0, 
			    offset_end-offset_start, eventbox->allocation.height-1);
      }
      gdk_draw_line (GDK_DRAWABLE(eventbox->window), mt->window->style->black_gc, offset_start, 0, offset_start, 
		     eventbox->allocation.height-1);
      gdk_draw_line (GDK_DRAWABLE(eventbox->window), mt->window->style->black_gc, offset_end, 
		     eventbox->allocation.height-1, offset_end, 0);
      gdk_draw_line (GDK_DRAWABLE(eventbox->window), mt->window->style->black_gc, offset_start, 0, offset_end, 0);
      gdk_draw_line (GDK_DRAWABLE(eventbox->window), mt->window->style->black_gc, offset_start, 
		     eventbox->allocation.height-1, offset_end, eventbox->allocation.height-1);
      if (mainw->playing_file>-1) unpaint_lines(mt);
      mt->redraw_block=TRUE; // stop drawing cursor during playback
      if (mainw->playing_file>-1&&mainw->cancelled==CANCEL_NONE) process_one(FALSE);
      mt->redraw_block=FALSE;
    }
    break;
  case BLOCK_SELECTED:
    gdk_draw_rectangle (GDK_DRAWABLE(eventbox->window), mt->window->style->bg_gc[0], TRUE, offset_start, 0, 
			offset_end-offset_start, eventbox->allocation.height-1);
    gdk_draw_line (GDK_DRAWABLE(eventbox->window), mt->window->style->black_gc, offset_start, 0, offset_start, 
		   eventbox->allocation.height-1);
    gdk_draw_line (GDK_DRAWABLE(eventbox->window), mt->window->style->black_gc, offset_end, eventbox->allocation.height-1, 
		   offset_end, 0);
    gdk_draw_line (GDK_DRAWABLE(eventbox->window), mt->window->style->black_gc, offset_start, 0, offset_end, 
		   eventbox->allocation.height-1);
    gdk_draw_line (GDK_DRAWABLE(eventbox->window), mt->window->style->black_gc, offset_start, 
		   eventbox->allocation.height-1, offset_end, 0);
    gdk_draw_line (GDK_DRAWABLE(eventbox->window), mt->window->style->black_gc, offset_start, 0, offset_end, 0);
    gdk_draw_line (GDK_DRAWABLE(eventbox->window), mt->window->style->black_gc, offset_start, 
		   eventbox->allocation.height-1, offset_end, eventbox->allocation.height-1);
    break;
  }
  if (mainw->playing_file>-1) unpaint_lines(mt);
  mt->redraw_block=TRUE; // stop drawing cursor during playback
  if (mainw->playing_file>-1&&mainw->cancelled==CANCEL_NONE) process_one(FALSE);
  mt->redraw_block=FALSE; // stop drawing cursor during playback
}



static void draw_aparams(lives_mt *mt, GtkWidget *eventbox, GList *param_list, weed_plant_t *init_event, gint startx, gint width) {
  // draw audio parameters : currently we overlay coloured lines on the audio track to show the level of 
  // parameters in the audio_volume plugin
  // we only display whichever parameters the user has elected to show


  gdouble dtime;
  weed_plant_t *filter,*inst,*deinit_event;
  gdouble ratio;
  double vald,mind,maxd,*valds;
  int vali,mini,maxi,*valis;
  int i,error,pnum;
  weed_plant_t **in_params,*param,*ptmpl;
  weed_timecode_t tc,start_tc,end_tc;
  int hint;
  gdouble tl_span=mt->tl_max-mt->tl_min;
  gint offset_start,offset_end,startpos;
  GList *plist;
  gchar *fhash;
  gint track;

  void **pchainx=weed_get_voidptr_array(init_event,"in_parameters",&error);

  fhash=weed_get_string_value(init_event,"filter",&error);

  if (fhash==NULL) {
    weed_free(pchainx);
    return;
  }

  filter=get_weed_filter(weed_get_idx_for_hashname(fhash,TRUE));
  weed_free(fhash);

  inst=weed_instance_from_filter(filter);
  in_params=weed_get_plantptr_array(inst,"in_parameters",&error);

  deinit_event=(weed_plant_t *)weed_get_voidptr_value(init_event,"deinit_event",&error);

  start_tc=get_event_timecode(init_event);
  end_tc=get_event_timecode(deinit_event);

  offset_start=(gint)((start_tc/U_SEC-mt->tl_min)/tl_span*eventbox->allocation.width+.5);
  offset_end=(gint)((end_tc/U_SEC-mt->tl_min)/tl_span*eventbox->allocation.width+.5);

  if (offset_end<0||offset_start>eventbox->allocation.width) {
    weed_free(in_params);
    weed_free(pchainx);
    return;
  }

  if (offset_start>startx) startpos=offset_start;
  else startpos=startx;

  track=GPOINTER_TO_INT(g_object_get_data (G_OBJECT(eventbox),"layer_number"))+1;

  for (i=startpos;i<startx+width;i++) {
    dtime=get_time_from_x(mt,i);
    tc=dtime*U_SEC;
    if (tc>=end_tc) break;
    interpolate_params(inst,pchainx,tc);
    plist=param_list;
    while (plist!=NULL) {
      pnum=GPOINTER_TO_INT(plist->data);
      param=in_params[pnum];
      ptmpl=weed_get_plantptr_value(param,"template",&error);
      hint=weed_get_int_value(ptmpl,"hint",&error);
      switch(hint) {
      case WEED_HINT_INTEGER:
	valis=weed_get_int_array(param,"value",&error);
	if (is_perchannel_multiw(in_params[pnum])) vali=valis[track];
	else vali=valis[0];
	mini=weed_get_int_value(ptmpl,"min",&error);
	maxi=weed_get_int_value(ptmpl,"max",&error);
	ratio=(gdouble)(vali-mini)/(gdouble)(maxi-mini);
	weed_free(valis);
	break;
      case WEED_HINT_FLOAT:
	valds=weed_get_double_array(param,"value",&error);
	if (is_perchannel_multiw(in_params[pnum])) vald=valds[track];
	else vald=valds[0];
	mind=weed_get_double_value(ptmpl,"min",&error);
	maxd=weed_get_double_value(ptmpl,"max",&error);
	ratio=(vald-mind)/(maxd-mind);
	weed_free(valds);
	break;
      default:
	continue;
      }
      gdk_draw_point(GDK_DRAWABLE(eventbox->window),mt->window->style->fg_gc[0],i,(1.-ratio)*eventbox->allocation.height);
      plist=plist->next;
    }
  }

  weed_free(pchainx);
  weed_free(in_params);
}



static void redraw_eventbox(lives_mt *mt, GtkWidget *eventbox) {
  GdkPixbuf *bgimg;

  if ((bgimg=(GdkPixbuf *)g_object_get_data(G_OBJECT(eventbox), "bgimg"))!=NULL) {
    gdk_pixbuf_unref(bgimg);
    g_object_set_data(G_OBJECT(eventbox), "bgimg",NULL);
  }

  g_object_set_data (G_OBJECT(eventbox), "drawn",GINT_TO_POINTER(FALSE));
  gtk_widget_queue_draw (eventbox); // redraw the track

  if (is_audio_eventbox(mt,eventbox)) {
    // handle expanded audio
    GtkWidget *xeventbox;
    if (cfile->achans>0) {
      xeventbox=(GtkWidget *)g_object_get_data(G_OBJECT(eventbox),"achan0");
      if ((bgimg=(GdkPixbuf *)g_object_get_data(G_OBJECT(xeventbox), "bgimg"))!=NULL) {
	gdk_pixbuf_unref(bgimg);
	g_object_set_data(G_OBJECT(xeventbox), "bgimg",NULL);
      }
      
      g_object_set_data (G_OBJECT(xeventbox), "drawn",GINT_TO_POINTER(FALSE));
      gtk_widget_queue_draw (xeventbox); // redraw the track

      if (cfile->achans>1) {
	xeventbox=(GtkWidget *)g_object_get_data(G_OBJECT(eventbox),"achan1");
	if ((bgimg=(GdkPixbuf *)g_object_get_data(G_OBJECT(xeventbox), "bgimg"))!=NULL) {
	  gdk_pixbuf_unref(bgimg);
	  g_object_set_data(G_OBJECT(xeventbox), "bgimg",NULL);
	}
	
	g_object_set_data (G_OBJECT(xeventbox), "drawn",GINT_TO_POINTER(FALSE));
	gtk_widget_queue_draw (xeventbox); // redraw the track
      }
    }
  }
}


static gint expose_track_event (GtkWidget *eventbox, GdkEventExpose *event, gpointer user_data) {
  track_rect *block;
  lives_mt *mt=(lives_mt *)user_data;
  GdkRegion *reg=event->region;
  GdkRectangle rect;
  gint hidden;
  gint startx,width;
  GdkPixbuf *bgimage;
  track_rect *sblock=NULL;
  gulong idlefunc;

  if (mt->no_expose) return TRUE;

  gdk_region_get_clipbox(reg,&rect);
  startx=rect.x;
  width=rect.width;

  if (event!=NULL&&event->count>0) {
    return TRUE;
  }

  hidden=(gint)GPOINTER_TO_INT(g_object_get_data (G_OBJECT(eventbox), "hidden"));
  if (hidden!=0) {
    GtkWidget *label=(GtkWidget *)g_object_get_data (G_OBJECT(eventbox), "label");
    gtk_widget_hide(eventbox);
    gtk_widget_hide(label->parent);
    return FALSE;
  }


  idlefunc=mt->idlefunc;
  if (mt->idlefunc>0) g_source_remove(mt->idlefunc);
  mt->idlefunc=0;

  if (width>eventbox->allocation.width-startx) width=eventbox->allocation.width-startx;

  if (GPOINTER_TO_INT(g_object_get_data (G_OBJECT(eventbox), "drawn"))) {
    bgimage=(GdkPixbuf *)g_object_get_data (G_OBJECT(eventbox), "bgimg");
    if (bgimage!=NULL&&lives_pixbuf_get_width(bgimage)>0) {
      cairo_t *cr = gdk_cairo_create (eventbox->window);
      gdk_cairo_set_source_pixbuf (cr, bgimage, startx, 0);
      cairo_paint (cr);
      cairo_destroy (cr);
      if (is_audio_eventbox(mt,eventbox)&&mt->avol_init_event!=NULL&&mt->aparam_view_list!=NULL) 
	draw_aparams(mt,eventbox,mt->aparam_view_list,mt->avol_init_event,startx,width);
      if (mt->block_selected!=NULL) draw_block(mt,mt->block_selected,-1,-1);
      if (idlefunc>0) {
	mt->idlefunc=mt_idle_add(mt);
      }
      return FALSE;
    }
  }
  set_cursor_style(mt,LIVES_CURSOR_BUSY,0,0,0,0,0);

  mt->redraw_block=TRUE; // stop drawing cursor during playback
  gdk_window_clear_area (eventbox->window, startx, 0, width, eventbox->allocation.height);
  mt->redraw_block=FALSE;

  block=(track_rect *)g_object_get_data (G_OBJECT(eventbox), "blocks");

  if (mt->block_selected!=NULL) {
    sblock=mt->block_selected;
    sblock->state=BLOCK_UNSELECTED;
  }

  while (block!=NULL) {
    draw_block(mt,block,startx,width);
    block=block->next;
    mt->redraw_block=TRUE; // stop drawing cursor during playback
    if (mainw->playing_file>-1&&mainw->cancelled==CANCEL_NONE) process_one(FALSE);
    mt->redraw_block=FALSE;
  }

  bgimage=gdk_pixbuf_get_from_drawable(NULL,GDK_DRAWABLE(eventbox->window),NULL,0,0,0,0,
				       eventbox->allocation.width,eventbox->allocation.height);
  if (lives_pixbuf_get_width(bgimage)>0) {
    g_object_set_data (G_OBJECT(eventbox), "drawn",GINT_TO_POINTER(TRUE));
    g_object_set_data (G_OBJECT(eventbox), "bgimg",bgimage);
  }

  if (sblock!=NULL) {
    mt->block_selected=sblock;
    sblock->state=BLOCK_SELECTED;
    draw_block(mt,mt->block_selected,-1,-1);
  }

  if (is_audio_eventbox(mt,eventbox)&&mt->avol_init_event!=NULL&&mt->aparam_view_list!=NULL) 
    draw_aparams(mt,eventbox,mt->aparam_view_list,mt->avol_init_event,startx,width);

  set_cursor_style(mt,LIVES_CURSOR_NORMAL,0,0,0,0,0);

  if (idlefunc>0) {
    mt->idlefunc=mt_idle_add(mt);
  }
  return FALSE;
}



static gchar *mt_params_label(lives_mt *mt) {
  gchar *fname=weed_filter_get_name(mt->current_fx);
  gchar *layer_name;
  gchar *ltext;

  if (has_perchannel_multiw(get_weed_filter(mt->current_fx))) {
    layer_name=get_track_name(mt,mt->current_track,mt->aud_track_selected);
    ltext=g_strdup_printf("%s : parameters for %s",fname,layer_name);
    g_free(layer_name);
  }
  else ltext=g_strdup(fname);
  g_free(fname);

  return ltext;
}

gdouble mt_get_effect_time(lives_mt *mt) {
  return q_gint64(gtk_spin_button_get_value(GTK_SPIN_BUTTON(mt->node_spinbutton))*U_SEC,mt->fps)/U_SEC;
}


static gboolean add_mt_param_box(lives_mt *mt) {
  // here we add a GUI box which will hold effect parameters

  // if we set keep_scale to TRUE, the current time slider is kept
  // this is necessary in case we need to update the parameters without resetting the current timeline value

  // returns TRUE if we have any parameters

  weed_plant_t *deinit_event;
  gdouble fx_start_time,fx_end_time;
  gchar *ltext;
  weed_timecode_t tc;
  int error;
  gboolean res;

  gdouble cur_time=GTK_RULER (mt->timeline)->position;

  tc=get_event_timecode((weed_plant_t *)mt->init_event);
  deinit_event=(weed_plant_t *)weed_get_voidptr_value(mt->init_event,"deinit_event",&error);
  
  fx_start_time=tc/U_SEC;
  fx_end_time=get_event_timecode(deinit_event)/U_SEC;

  if (mt->fx_box!=NULL) {
    gtk_widget_destroy(mt->fx_box);
  }

  mt->fx_box=gtk_vbox_new(FALSE,0);
  gtk_box_pack_end(GTK_BOX(mt->fx_base_box),mt->fx_box,TRUE,TRUE,0);

  ltext=mt_params_label(mt);

  g_signal_handler_block(mt->node_spinbutton,mt->node_adj_func);
  adjustment_configure (GTK_ADJUSTMENT(mt->node_adj), cur_time-fx_start_time, 0., 
			fx_end_time-fx_start_time, 1./mt->fps, 10./mt->fps, 0.);
  g_signal_handler_unblock(mt->node_spinbutton,mt->node_adj_func);

  res=make_param_box(GTK_VBOX (mt->fx_box), mt->current_rfx);

  mt->fx_params_label=gtk_label_new(ltext);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(mt->fx_params_label, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  gtk_box_pack_start (GTK_BOX (mt->fx_box), mt->fx_params_label, FALSE, FALSE, 0);

  g_free(ltext);

  gtk_widget_show_all(mt->fx_base_box);

  if (res) gtk_widget_show(mt->fx_contents_box);
  else gtk_widget_hide(mt->fx_contents_box);

  mt->prev_fx_time=mt_get_effect_time(mt);
  return res;
}


static track_rect *get_block_from_time (GtkWidget *eventbox, gdouble time, lives_mt *mt) {
  // return block (track_rect) at seconds time in eventbox
  weed_timecode_t tc=time*U_SECL;
  track_rect *block=(track_rect *)g_object_get_data (G_OBJECT(eventbox),"blocks");
  tc=q_gint64(tc,mt->fps);

  while (block!=NULL) {
    if (get_event_timecode(block->start_event)>tc) return NULL;
    if (q_gint64(get_event_timecode(block->end_event)+(!is_audio_eventbox(mt,eventbox))*U_SEC/mt->fps,mt->fps)>tc) break;
    block=block->next;
  }
  return block;
}


static int track_to_channel(weed_plant_t *ievent, int track) {
  // given an init_event and a track, we check to see which (if any) channel the track is mapped to

  // if track is not mapped, we return -1

  // note that a track could be mapped to multiple channels; we return only the first instance we find


  int i,error,ntracks=weed_leaf_num_elements(ievent,"in_tracks");
  int *in_tracks;

  if (ntracks==0) return -1;

  in_tracks=weed_get_int_array(ievent,"in_tracks",&error);

  for (i=0;i<ntracks;i++) {
    if (in_tracks[i]==track) {
      g_free(in_tracks);
      return i;
    }
  }
  g_free(in_tracks);
  return -1;
}



static gboolean get_track_index(lives_mt *mt, weed_timecode_t tc) {
  // set mt->track_index to the in_channel index of mt->current_track in "in_tracks" in mt->init_event
  // set -1 if there is no frame for that in_channel, or if mt->current_track lies outside the "in_tracks" of mt->init_event

  // return TRUE if mt->fx_box is redrawn

  int i,error;
  int num_in_tracks;
  int *clips,*in_tracks,numtracks;
  weed_plant_t *event=get_frame_event_at(mt->event_list,tc,NULL,TRUE);
  int opwidth,opheight;

  int track_index=mt->track_index;

  int chindx;
  gboolean retval=FALSE;

  mt->track_index=-1;
  mt->inwidth=mt->inheight=0;

  if (event==NULL) return retval;

  opwidth=cfile->hsize;
  opheight=cfile->vsize;
  calc_maxspect(mt->play_width,mt->play_height,&opwidth,&opheight);

  numtracks=weed_leaf_num_elements(event,"clips");
  clips=weed_get_int_array(event,"clips",&error);

  chindx=track_to_channel(mt->init_event,mt->current_track);

  if (mt->current_track<numtracks&&clips[mt->current_track]<1&&
      (mt->current_rfx==NULL||mt->init_event==NULL||mt->current_rfx->source==NULL||chindx==-1||
       !is_audio_channel_in((weed_plant_t *)mt->current_rfx->source,chindx))) {
    if (track_index!=-1&&mt->fx_box!=NULL) {
      add_mt_param_box(mt);
      retval=TRUE;
    }
    weed_free(clips);
    return retval;
  }

  if ((num_in_tracks=weed_leaf_num_elements(mt->init_event,"in_tracks"))>0) {
    in_tracks=weed_get_int_array(mt->init_event,"in_tracks",&error);
    for (i=0;i<num_in_tracks;i++) {
      if (in_tracks[i]==mt->current_track) {
	mt->track_index=i;
	if (mt->current_track>=0&&mt->current_track<numtracks&&clips[mt->current_track]>-1) {
	  mt->inwidth=mainw->files[clips[mt->current_track]]->hsize*opwidth/cfile->hsize;
	  mt->inheight=mainw->files[clips[mt->current_track]]->vsize*opheight/cfile->vsize;
	}
	if (track_index==-1&&mt->fx_box!=NULL) {
	  add_mt_param_box(mt);
	  retval=TRUE;
	}
	break;
      }
    }
    weed_free(in_tracks);
  }
  weed_free(clips);
  if (track_index!=-1&&mt->track_index==-1&&mt->fx_box!=NULL) {
    add_mt_param_box(mt);
    retval=TRUE;
  }
  return retval;
}




void track_select (lives_mt *mt) {
  int i;
  GtkWidget *labelbox,*ahbox,*eventbox,*oeventbox,*checkbutton=NULL;
  gint hidden=0;
  weed_timecode_t tc;

  if (cfile->achans>0) {
    for (i=0;i<g_list_length(mt->audio_draws);i++) {
      eventbox=(GtkWidget *)g_list_nth_data(mt->audio_draws,i);
      if ((oeventbox=(GtkWidget *)g_object_get_data(G_OBJECT(eventbox),"owner"))!=NULL) 
	hidden=!GPOINTER_TO_INT(g_object_get_data(G_OBJECT(oeventbox),"expanded"));
      if (hidden==0) hidden=GPOINTER_TO_INT(g_object_get_data(G_OBJECT(eventbox),"hidden"));
      if (hidden==0) {
	labelbox=(GtkWidget *)g_object_get_data(G_OBJECT(eventbox),"labelbox");
	ahbox=(GtkWidget *)g_object_get_data(G_OBJECT(eventbox),"ahbox");
	if (mt->current_track==i-mt->opts.back_audio_tracks&&(mt->current_track==-1||mt->aud_track_selected)) {
	  if (labelbox!=NULL) gtk_widget_set_state(labelbox,GTK_STATE_SELECTED);
	  if (ahbox!=NULL) gtk_widget_set_state(ahbox,GTK_STATE_SELECTED);
	  gtk_widget_set_sensitive (mt->jumpback, g_object_get_data(G_OBJECT(eventbox),"blocks")!=NULL);
	  gtk_widget_set_sensitive (mt->jumpnext, g_object_get_data(G_OBJECT(eventbox),"blocks")!=NULL);
	  if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(mt->select_track))) 
	    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mt->select_track),FALSE);
	  gtk_widget_set_sensitive(mt->select_track,FALSE);
	  gtk_widget_set_sensitive(mt->cback_audio,FALSE);
	  gtk_widget_set_sensitive (mt->audio_insert, mt->file_selected>0&&
				    mainw->files[mt->file_selected]->achans>0&&
				    mainw->files[mt->file_selected]->laudio_time>0.);
	  gtk_widget_set_sensitive (mt->insert, FALSE);
	  if (mt->poly_state==POLY_FX_STACK) polymorph(mt,POLY_FX_STACK);
	}
	else {
	  if (labelbox!=NULL&&GTK_IS_WIDGET(labelbox)) gtk_widget_set_state(labelbox,GTK_STATE_NORMAL);
	  if (ahbox!=NULL&&GTK_IS_WIDGET(ahbox)) gtk_widget_set_state(ahbox,GTK_STATE_NORMAL);
	  gtk_widget_set_sensitive(mt->select_track,TRUE);
	  gtk_widget_set_sensitive(mt->cback_audio,TRUE);
	}
      }
    }
  }

  for (i=0;i<mt->num_video_tracks;i++) {
    eventbox=(GtkWidget *)g_list_nth_data(mt->video_draws,i);
    hidden=GPOINTER_TO_INT(g_object_get_data(G_OBJECT(eventbox),"hidden"));
    if (hidden==0) {
      labelbox=(GtkWidget *)g_object_get_data(G_OBJECT(eventbox),"labelbox");
      ahbox=(GtkWidget *)g_object_get_data(G_OBJECT(eventbox),"ahbox");
      if (i==mt->current_track&&!mt->aud_track_selected) {
	if (labelbox!=NULL) gtk_widget_set_state(labelbox,GTK_STATE_SELECTED);
	if (ahbox!=NULL) gtk_widget_set_state(ahbox,GTK_STATE_SELECTED);
	gtk_widget_set_sensitive (mt->jumpback, g_object_get_data(G_OBJECT(eventbox),"blocks")!=NULL);
	gtk_widget_set_sensitive (mt->jumpnext, g_object_get_data(G_OBJECT(eventbox),"blocks")!=NULL);

	checkbutton=(GtkWidget *)g_object_get_data (G_OBJECT(eventbox), "checkbutton");
	
#ifdef ENABLE_GIW
	if ((prefs->lamp_buttons&&!giw_led_get_mode(GIW_LED(checkbutton)))||(!prefs->lamp_buttons&&
#else			
        if (
#endif
	    !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkbutton)))
#ifdef ENABLE_GIW
	    )
#endif
	  {
	  if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(mt->select_track))) 
	    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mt->select_track),FALSE);
	  else on_seltrack_activate(GTK_MENU_ITEM(mt->select_track),mt);
	}
	else {
	  if (!gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(mt->select_track))) 
	    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mt->select_track),TRUE);
	  else on_seltrack_activate(GTK_MENU_ITEM(mt->select_track),mt);
	}
	gtk_widget_set_sensitive (mt->insert, mt->file_selected>0&&mainw->files[mt->file_selected]->frames>0);
	gtk_widget_set_sensitive (mt->adjust_start_end, mt->file_selected>0);
	gtk_widget_set_sensitive (mt->audio_insert, FALSE);
	if (mt->poly_state==POLY_FX_STACK) polymorph(mt,POLY_FX_STACK);
      }
      else {
	if (labelbox!=NULL) gtk_widget_set_state(labelbox,GTK_STATE_NORMAL);
	if (ahbox!=NULL) gtk_widget_set_state(ahbox,GTK_STATE_NORMAL);
      }
    }
    else {
      if (i==mt->current_track) {
	checkbutton=(GtkWidget *)g_object_get_data (G_OBJECT(eventbox), "checkbutton");
#ifdef ENABLE_GIW
	if ((prefs->lamp_buttons&&!giw_led_get_mode(GIW_LED(checkbutton)))||(!prefs->lamp_buttons&&
#else			
        if (
#endif
	    !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkbutton)))
#ifdef ENABLE_GIW
	    )
#endif
	  {
	   if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(mt->select_track))) 
	     gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mt->select_track),FALSE);
	  else on_seltrack_activate(GTK_MENU_ITEM(mt->select_track),mt);
	}
	else {
	  if (!gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(mt->select_track))) 
	    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mt->select_track),TRUE);
	  else on_seltrack_activate(GTK_MENU_ITEM(mt->select_track),mt);
	}
      }
    }
  }

  if (mt->current_rfx!=NULL&&mt->init_event!=NULL&&mt->poly_state==POLY_PARAMS&&
      weed_plant_has_leaf(mt->init_event,"in_tracks")) {
    gboolean xx;
    weed_timecode_t init_tc=get_event_timecode(mt->init_event);
    tc=q_gint64(gtk_spin_button_get_value(GTK_SPIN_BUTTON(mt->node_spinbutton))*U_SEC+init_tc,mt->fps);

    // must be done in this order: interpolate, update, preview
    xx=get_track_index(mt,tc);
    if (mt->track_index!=-1) {
      if (mt->current_track>=0) {
	interpolate_params((weed_plant_t *)mt->current_rfx->source,pchain,tc);
      }
      if (!xx) {
	gboolean aprev=mt->opts.fx_auto_preview;
	mt->opts.fx_auto_preview=FALSE;
	mainw->block_param_updates=TRUE;
	update_visual_params(mt->current_rfx,FALSE);
	mainw->block_param_updates=FALSE;
	mt->opts.fx_auto_preview=aprev;
      }
      if (mt->current_track>=0){
	set_params_unchanged(mt->current_rfx);
	mt_show_current_frame(mt, FALSE);
      }
      if (mt->fx_params_label!=NULL) {
	gchar *ltext=mt_params_label(mt);
	gtk_label_set_text(GTK_LABEL(mt->fx_params_label),ltext);
	g_free(ltext);
      }
    }
    else polymorph(mt,POLY_FX_STACK);
  }

}


static void show_track_info(lives_mt *mt, GtkWidget *eventbox, gint track, gdouble timesecs) {
  gchar *tmp,*tmp1;
  track_rect *block=get_block_from_time(eventbox,timesecs,mt);
  gint filenum;

  clear_context(mt);
  if (cfile->achans==0||!is_audio_eventbox(mt,eventbox)) add_context_label 
							   (mt,(tmp=g_strdup_printf 
								(_("Current track: %s (layer %d)\n"),
								 g_object_get_data(G_OBJECT(eventbox),
										   "track_name"),track)));
  else {
    if (track==-1) add_context_label (mt,(tmp=g_strdup (_("Current track: Backing audio\n"))));
    else add_context_label (mt,(tmp=g_strdup_printf (_("Current track: Layer %d audio\n"),track)));
  }
  g_free(tmp);
  add_context_label (mt,(tmp=g_strdup_printf (_("%.2f sec.\n"),timesecs)));
  g_free(tmp);
  if (block!=NULL) {
    if (cfile->achans==0||!is_audio_eventbox(mt,eventbox)) filenum=get_frame_event_clip(block->start_event,track);
    else filenum=get_audio_frame_clip(block->start_event,track);
    add_context_label (mt,(tmp=g_strdup_printf (_("Source: %s"),(tmp1=g_path_get_basename(mainw->files[filenum]->name)))));
    g_free(tmp);
    g_free(tmp1);
    add_context_label (mt,(_("Right click for context menu.\n")));
  }
  add_context_label (mt,(_("Double click on a block\nto select it.")));
}


static gboolean
atrack_ebox_pressed (GtkWidget *labelbox, GdkEventButton *event, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  gint current_track=mt->current_track;
  mt->current_track=GPOINTER_TO_INT(g_object_get_data (G_OBJECT(labelbox),"layer_number"));
  if (current_track!=mt->current_track) mt->fm_edit_event=NULL;
  mt->aud_track_selected=TRUE;
  track_select(mt);
  show_track_info(mt,(GtkWidget *)g_list_nth_data(mt->audio_draws,mt->current_track+mt->opts.back_audio_tracks),
		  mt->current_track,mt->ptr_time);
  return FALSE;
}




static gboolean
track_ebox_pressed (GtkWidget *labelbox, GdkEventButton *event, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  gint current_track=mt->current_track;
  mt->current_track=GPOINTER_TO_INT(g_object_get_data (G_OBJECT(labelbox),"layer_number"));
  if (current_track!=mt->current_track) mt->fm_edit_event=NULL;
  mt->aud_track_selected=FALSE;
  track_select(mt);
  show_track_info(mt,(GtkWidget *)g_list_nth_data(mt->video_draws,mt->current_track),mt->current_track,mt->ptr_time);
  return FALSE;
}



static gboolean
on_mt_timeline_scroll           (GtkWidget       *widget,
				 GdkEventScroll  *event,
				 gpointer         user_data) {
  // scroll timeline up/down with mouse wheel
  lives_mt *mt=(lives_mt *)user_data;

  gint cval;

  if (!gtk_window_has_toplevel_focus(GTK_WINDOW(mainw->multitrack->window))) return FALSE;

  cval=GTK_ADJUSTMENT(GTK_RANGE(mt->scrollbar)->adjustment)->value;

  if (event->direction==GDK_SCROLL_UP) {
    if (--cval<0) return FALSE;;
  }
  else if (event->direction==GDK_SCROLL_DOWN) {
    if (++cval>=g_list_length(mt->video_draws)) return FALSE;
  }

  gtk_range_set_value(GTK_RANGE(mt->scrollbar),cval);

  return FALSE;
}



static gint get_top_track_for(lives_mt *mt, gint track) {
  // find top track such that all of track fits at the bottom

  GtkWidget *eventbox;
  GList *vdraw;
  gint extras=mt->max_disp_vtracks-1;
  gint hidden,expanded;

  if (mt->opts.back_audio_tracks>0&&mt->audio_draws==NULL) mt->opts.back_audio_tracks=0;
  if (cfile->achans>0&&mt->opts.back_audio_tracks>0) {
    eventbox=(GtkWidget *)mt->audio_draws->data;
    hidden=GPOINTER_TO_INT(g_object_get_data(G_OBJECT(eventbox),"hidden"));
    if (!hidden) {
      extras--;
      expanded=GPOINTER_TO_INT(g_object_get_data(G_OBJECT(eventbox),"expanded"));
      if (expanded) {
	extras-=cfile->achans;
      }
    }
  }

  if (extras<0) return track;

  vdraw=g_list_nth(mt->video_draws,track);
  eventbox=(GtkWidget *)vdraw->data;
  expanded=GPOINTER_TO_INT(g_object_get_data(G_OBJECT(eventbox),"expanded"));
  if (expanded) {
    eventbox=(GtkWidget *)(g_object_get_data(G_OBJECT(eventbox),"atrack"));
    extras--;
    expanded=GPOINTER_TO_INT(g_object_get_data(G_OBJECT(eventbox),"expanded"));
    if (expanded) {
      extras-=cfile->achans;
    }
  }

  if (extras<0) return track;

  vdraw=vdraw->prev;

  while (vdraw!=NULL) {
    eventbox=(GtkWidget *)vdraw->data;
    hidden=GPOINTER_TO_INT(g_object_get_data(G_OBJECT(eventbox),"hidden"))&TRACK_I_HIDDEN_USER;
    expanded=GPOINTER_TO_INT(g_object_get_data(G_OBJECT(eventbox),"expanded"));
    extras--;
    if (expanded) {
      eventbox=(GtkWidget *)(g_object_get_data(G_OBJECT(eventbox),"atrack"));
      extras--;
      expanded=GPOINTER_TO_INT(g_object_get_data(G_OBJECT(eventbox),"expanded"));
      if (expanded) {
	extras-=cfile->achans;
      }
    }
    if (extras<0) break;
    vdraw=vdraw->prev;
    track--;
  }

  if (track<0) track=0;
  return track;

}






void scroll_tracks (lives_mt *mt, gint top_track) {
  int rows=0;
  GList *vdraws=mt->video_draws;
  GList *table_children;
  gulong seltrack_func;
  GtkWidget *eventbox;
  GtkWidget *label;
  GtkWidget *arrow;
  GtkWidget *checkbutton;
  GtkWidget *labelbox;
  GtkWidget *hbox;
  GtkWidget *ahbox;
  gint aud_tracks=0;
  gboolean expanded;
  GtkWidget *xeventbox,*aeventbox;
  gint hidden;
  gulong exp_track_func;

  GTK_ADJUSTMENT(mt->vadjustment)->page_size=mt->max_disp_vtracks;
  GTK_ADJUSTMENT(mt->vadjustment)->upper=mt->num_video_tracks*2-1;
  GTK_ADJUSTMENT(mt->vadjustment)->value=top_track;

  if (top_track<0) top_track=0;
  if (top_track>=g_list_length(mt->video_draws)) top_track=g_list_length(mt->video_draws)-1;

  mt->top_track=top_track;

  // first set all tracks to hidden
  while (vdraws!=NULL) {
    eventbox=(GtkWidget *)vdraws->data;
    hidden=GPOINTER_TO_INT(g_object_get_data(G_OBJECT(eventbox),"hidden"));
    hidden|=TRACK_I_HIDDEN_SCROLLED;
    g_object_set_data(G_OBJECT(eventbox),"hidden",GINT_TO_POINTER(hidden));


    aeventbox=GTK_WIDGET(g_object_get_data(G_OBJECT(eventbox),"atrack"));

    if (aeventbox!=NULL) {
      hidden=GPOINTER_TO_INT(g_object_get_data(G_OBJECT(aeventbox),"hidden"));
      hidden|=TRACK_I_HIDDEN_SCROLLED;
      g_object_set_data(G_OBJECT(aeventbox),"hidden",GINT_TO_POINTER(hidden));
      
      
      xeventbox=(GtkWidget *)g_object_get_data(G_OBJECT(aeventbox),"achan0");
    
      if (xeventbox!=NULL) {
	hidden=GPOINTER_TO_INT(g_object_get_data(G_OBJECT(xeventbox),"hidden"));
	hidden|=TRACK_I_HIDDEN_SCROLLED;
	g_object_set_data(G_OBJECT(xeventbox),"hidden",GINT_TO_POINTER(hidden));
	
      }

      xeventbox=(GtkWidget *)g_object_get_data(G_OBJECT(aeventbox),"achan1");
    
      if (xeventbox!=NULL) {
	hidden=GPOINTER_TO_INT(g_object_get_data(G_OBJECT(xeventbox),"hidden"));
	hidden|=TRACK_I_HIDDEN_SCROLLED;
	g_object_set_data(G_OBJECT(xeventbox),"hidden",GINT_TO_POINTER(hidden));
      }
    }

    vdraws=vdraws->next;
  }

  if (mt->timeline_table!=NULL) {
    gtk_widget_destroy(mt->timeline_table);
  }

  mt->timeline_table = gtk_table_new (mt->max_disp_vtracks, 40, TRUE);

  gtk_container_add (GTK_CONTAINER (mt->tl_eventbox), mt->timeline_table);

  gtk_table_set_row_spacings (GTK_TABLE(mt->timeline_table),5);
  gtk_table_set_col_spacings (GTK_TABLE(mt->timeline_table),0);

  if (mt->opts.back_audio_tracks>0&&mt->audio_draws==NULL) mt->opts.back_audio_tracks=0;

  if (cfile->achans>0&&mt->opts.back_audio_tracks>0) {
    // show our float audio
    if (GPOINTER_TO_INT(g_object_get_data(G_OBJECT(mt->audio_draws->data),"hidden"))==0) {
      aud_tracks++;
      
      expanded=GPOINTER_TO_INT(g_object_get_data(G_OBJECT(mt->audio_draws->data),"expanded"));

      label=(GTK_WIDGET(g_object_get_data(G_OBJECT(mt->audio_draws->data),"label")));
      arrow=(GTK_WIDGET(g_object_get_data(G_OBJECT(mt->audio_draws->data),"arrow")));
      
      labelbox=gtk_event_box_new();
      hbox=gtk_hbox_new(FALSE,10);
      ahbox=gtk_event_box_new();
      
      gtk_widget_set_state(label,GTK_STATE_NORMAL);
      gtk_widget_set_state(arrow,GTK_STATE_NORMAL);

      if (palette->style&STYLE_1) {
	if (palette->style&STYLE_3) {
	  if (labelbox!=NULL) gtk_widget_modify_bg (labelbox, GTK_STATE_SELECTED, &palette->menu_and_bars);
	  if (ahbox!=NULL) gtk_widget_modify_bg (ahbox, GTK_STATE_SELECTED, &palette->menu_and_bars);
	  gtk_widget_modify_fg (label, GTK_STATE_SELECTED, &palette->info_text);
	  gtk_widget_modify_fg (arrow, GTK_STATE_SELECTED, &palette->info_text);
	}
	else {
	  if (labelbox!=NULL) gtk_widget_modify_bg (labelbox, GTK_STATE_SELECTED, &palette->normal_back);
	  if (ahbox!=NULL) gtk_widget_modify_bg (ahbox, GTK_STATE_SELECTED, &palette->normal_back);
	  gtk_widget_modify_fg (label, GTK_STATE_SELECTED, &palette->normal_fore);
	  gtk_widget_modify_fg (arrow, GTK_STATE_SELECTED, &palette->normal_fore);
	}
      }
      gtk_container_add (GTK_CONTAINER (labelbox), hbox);
      gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
      gtk_container_add (GTK_CONTAINER (ahbox), arrow);
      
      gtk_table_attach (GTK_TABLE (mt->timeline_table), labelbox, 1, 6, 0, 1, GTK_FILL, (GtkAttachOptions)0, 0, 0);
      gtk_table_attach (GTK_TABLE (mt->timeline_table), ahbox, 6, 7, 0, 1, GTK_FILL, (GtkAttachOptions)0, 0, 0);
      
      g_object_set_data (G_OBJECT(mt->audio_draws->data),"labelbox",labelbox);
      g_object_set_data (G_OBJECT(mt->audio_draws->data),"ahbox",ahbox);
      g_object_set_data (G_OBJECT(ahbox),"eventbox",mt->audio_draws->data);
      g_object_set_data(G_OBJECT(labelbox),"layer_number",GINT_TO_POINTER(GPOINTER_TO_INT(-1)));
      
      g_signal_connect (GTK_OBJECT (labelbox), "button_press_event",
			G_CALLBACK (atrack_ebox_pressed),
			(gpointer)mt);
      
      g_signal_connect (GTK_OBJECT (ahbox), "button_press_event",
			G_CALLBACK (track_arrow_pressed),
			(gpointer)mt);
      
      gtk_table_attach (GTK_TABLE (mt->timeline_table), (GtkWidget *)mt->audio_draws->data, 7, 40, 0, 1,
			(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			(GtkAttachOptions) (GTK_FILL), 0, 0);

      gtk_widget_modify_bg((GtkWidget *)mt->audio_draws->data, GTK_STATE_NORMAL, &palette->white);

      
      g_signal_connect (GTK_OBJECT (mt->audio_draws->data), "button_press_event",
			G_CALLBACK (on_track_click),
			(gpointer)mt);
      g_signal_connect (GTK_OBJECT (mt->audio_draws->data), "button_release_event",
			G_CALLBACK (on_track_release),
			(gpointer)mt);
      exp_track_func=g_signal_connect_after (GTK_OBJECT (mt->audio_draws->data), "expose_event",
					     G_CALLBACK (expose_track_event),
					     (gpointer)mt);
      g_object_set_data (G_OBJECT(mt->audio_draws->data),"expose_func",(gpointer)exp_track_func);

      if (expanded) {
	xeventbox=(GtkWidget *)g_object_get_data(G_OBJECT(mt->audio_draws->data),"achan0");
	
	gtk_table_attach (GTK_TABLE (mt->timeline_table), xeventbox, 7, 40, 1, 2,
			  (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			  (GtkAttachOptions) (GTK_FILL), 0, 0);
	
	exp_track_func=g_signal_connect_after (GTK_OBJECT (xeventbox), "expose_event",
					       G_CALLBACK (mt_expose_laudtrack_event),
					       (gpointer)mt);
	g_object_set_data (G_OBJECT(xeventbox),"expose_func",(gpointer)exp_track_func);

	gtk_widget_modify_bg(xeventbox, GTK_STATE_NORMAL, &palette->white);
	
	if (cfile->achans>1) {
	  xeventbox=(GtkWidget *)g_object_get_data(G_OBJECT(mt->audio_draws->data),"achan1");
	  
	  gtk_table_attach (GTK_TABLE (mt->timeline_table), xeventbox, 7, 40, 2, 3,
			    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			    (GtkAttachOptions) (GTK_FILL), 0, 0);
	  
	  exp_track_func=g_signal_connect_after (GTK_OBJECT (xeventbox), "expose_event",
						 G_CALLBACK (mt_expose_raudtrack_event),
						 (gpointer)mt);
	  
	  gtk_widget_modify_bg(xeventbox, GTK_STATE_NORMAL, &palette->white);
	  g_object_set_data (G_OBJECT(xeventbox),"expose_func",(gpointer)exp_track_func);
	}
	aud_tracks+=cfile->achans;
      }
    }
  }


  GTK_ADJUSTMENT(mt->vadjustment)->page_size-=aud_tracks;

  vdraws=g_list_nth(mt->video_draws,top_track);

  rows+=aud_tracks;

  while (vdraws!=NULL&&rows<mt->max_disp_vtracks) {
    eventbox=(GtkWidget *)vdraws->data;

    hidden=GPOINTER_TO_INT(g_object_get_data(G_OBJECT(eventbox),"hidden"))&TRACK_I_HIDDEN_USER;
    g_object_set_data(G_OBJECT(eventbox),"hidden",GINT_TO_POINTER(hidden));

    if (hidden==0) {

      label=(GTK_WIDGET(g_object_get_data(G_OBJECT(eventbox),"label")));
      arrow=(GTK_WIDGET(g_object_get_data(G_OBJECT(eventbox),"arrow")));
      checkbutton=(GTK_WIDGET(g_object_get_data(G_OBJECT(eventbox),"checkbutton")));
      labelbox=gtk_event_box_new();
      hbox=gtk_hbox_new(FALSE,10);
      ahbox=gtk_event_box_new();
      gtk_widget_set_state(label,GTK_STATE_NORMAL);
      gtk_widget_set_state(arrow,GTK_STATE_NORMAL);

      if (palette->style&STYLE_1) {
	if (palette->style&STYLE_3) {
	  gtk_widget_modify_bg (labelbox, GTK_STATE_SELECTED, &palette->menu_and_bars);
	  gtk_widget_modify_bg (ahbox, GTK_STATE_SELECTED, &palette->menu_and_bars);
	  gtk_widget_modify_fg (label, GTK_STATE_SELECTED, &palette->info_text);
	  gtk_widget_modify_fg (arrow, GTK_STATE_SELECTED, &palette->info_text);
	  gtk_widget_modify_fg (checkbutton, GTK_STATE_SELECTED, &palette->info_text);
	}
	else {
	  gtk_widget_modify_bg (labelbox, GTK_STATE_SELECTED, &palette->normal_back);
	  gtk_widget_modify_bg (ahbox, GTK_STATE_SELECTED, &palette->normal_back);
	  gtk_widget_modify_fg (label, GTK_STATE_SELECTED, &palette->normal_fore);
	  gtk_widget_modify_fg (arrow, GTK_STATE_SELECTED, &palette->normal_fore);
	  gtk_widget_modify_fg (checkbutton, GTK_STATE_SELECTED, &palette->normal_fore);
	}
#ifdef ENABLE_GIW
	if (prefs->lamp_buttons) {
	  if (0&&palette->style&STYLE_3) {
	    gtk_widget_modify_bg (checkbutton, GTK_STATE_SELECTED, &palette->normal_back);
	  }
	  else {
	    gtk_widget_modify_bg (checkbutton, GTK_STATE_SELECTED, &palette->menu_and_bars);
	  }
	}
#endif
      }


#ifdef ENABLE_GIW
      if (prefs->lamp_buttons) {
	giw_led_set_colors(GIW_LED(checkbutton),palette->light_green,palette->dark_red);
      }
#endif


      g_object_set_data(G_OBJECT(labelbox),"layer_number",GINT_TO_POINTER(GPOINTER_TO_INT(g_object_get_data(G_OBJECT(eventbox),"layer_number"))));

      gtk_container_add (GTK_CONTAINER (labelbox), hbox);
      gtk_box_pack_start (GTK_BOX (hbox), checkbutton, FALSE, FALSE, 0);
      gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
      gtk_container_add (GTK_CONTAINER (ahbox), arrow);

      gtk_table_attach (GTK_TABLE (mt->timeline_table), labelbox, 0, 6, rows, rows+1, GTK_FILL, (GtkAttachOptions)0, 0, 0);
      gtk_table_attach (GTK_TABLE (mt->timeline_table), ahbox, 6, 7, rows, rows+1, GTK_FILL, (GtkAttachOptions)0, 0, 0);
      
      g_object_set_data (G_OBJECT(eventbox),"labelbox",labelbox);
      g_object_set_data (G_OBJECT(eventbox),"ahbox",ahbox);
      g_object_set_data (G_OBJECT(ahbox),"eventbox",eventbox);

      gtk_table_attach (GTK_TABLE (mt->timeline_table), eventbox, 7, 40, rows, rows+1,
			(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			(GtkAttachOptions) (GTK_FILL), 0, 0);

      gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &palette->white);

      if (!prefs->lamp_buttons) {
	seltrack_func=g_signal_connect_after (GTK_OBJECT (checkbutton), "toggled",
					      G_CALLBACK (on_seltrack_toggled),
					      mt);
      }
      else {
	seltrack_func=g_signal_connect_after (GTK_OBJECT (checkbutton), "mode-changed",
					      G_CALLBACK (on_seltrack_toggled),
					      mt);
      }
      g_signal_connect (GTK_OBJECT (labelbox), "button_press_event",
			G_CALLBACK (track_ebox_pressed),
			(gpointer)mt);

      g_object_set_data(G_OBJECT(checkbutton),"tfunc",(gpointer)seltrack_func);

      exp_track_func=g_signal_connect_after (GTK_OBJECT (eventbox), "expose_event",
			      G_CALLBACK (expose_track_event),
			      (gpointer)mt);
      g_object_set_data (G_OBJECT(eventbox),"expose_func",(gpointer)exp_track_func);

      g_signal_connect (GTK_OBJECT (eventbox), "button_press_event",
			G_CALLBACK (on_track_click),
			(gpointer)mt);
      g_signal_connect (GTK_OBJECT (eventbox), "button_release_event",
			G_CALLBACK (on_track_release),
			(gpointer)mt);

      g_signal_connect (GTK_OBJECT (ahbox), "button_press_event",
			G_CALLBACK (track_arrow_pressed),
			(gpointer)mt);
      rows++;

      if (rows==mt->max_disp_vtracks) break;


      if (mt->opts.pertrack_audio&&g_object_get_data(G_OBJECT(eventbox),"expanded")) {

	aeventbox=GTK_WIDGET(g_object_get_data(G_OBJECT(eventbox),"atrack"));

	hidden=GPOINTER_TO_INT(g_object_get_data(G_OBJECT(aeventbox),"hidden"))&TRACK_I_HIDDEN_USER;
	g_object_set_data(G_OBJECT(aeventbox),"hidden",GINT_TO_POINTER(hidden));


	if (hidden==0) {
	  GTK_ADJUSTMENT(mt->vadjustment)->page_size--;
	    
	  expanded=GPOINTER_TO_INT(g_object_get_data(G_OBJECT(aeventbox),"expanded"));
	    
	  label=(GTK_WIDGET(g_object_get_data(G_OBJECT(aeventbox),"label")));
	  arrow=(GTK_WIDGET(g_object_get_data(G_OBJECT(aeventbox),"arrow")));
	  
	  labelbox=gtk_event_box_new();
	  hbox=gtk_hbox_new(FALSE,10);
	  ahbox=gtk_event_box_new();
	  
	  gtk_widget_set_state(label,GTK_STATE_NORMAL);
	  gtk_widget_set_state(arrow,GTK_STATE_NORMAL);
	  
	  if (palette->style&STYLE_1) {
	    if (palette->style&STYLE_3) {
	      gtk_widget_modify_bg (labelbox, GTK_STATE_SELECTED, &palette->menu_and_bars);
	      gtk_widget_modify_bg (ahbox, GTK_STATE_SELECTED, &palette->menu_and_bars);
	      gtk_widget_modify_fg (label, GTK_STATE_SELECTED, &palette->info_text);
	      gtk_widget_modify_fg (arrow, GTK_STATE_SELECTED, &palette->info_text);
	    }
	    else {
	      gtk_widget_modify_bg (labelbox, GTK_STATE_SELECTED, &palette->normal_back);
	      gtk_widget_modify_bg (ahbox, GTK_STATE_SELECTED, &palette->normal_back);
	      gtk_widget_modify_fg (label, GTK_STATE_SELECTED, &palette->normal_fore);
	      gtk_widget_modify_fg (arrow, GTK_STATE_SELECTED, &palette->normal_fore);
	    }
	  }
	  
	  gtk_container_add (GTK_CONTAINER (labelbox), hbox);
	  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
	  gtk_container_add (GTK_CONTAINER (ahbox), arrow);
	  
	  gtk_table_attach (GTK_TABLE (mt->timeline_table), labelbox, 1, 6, rows, rows+1, GTK_FILL, (GtkAttachOptions)0, 0, 0);
	  gtk_table_attach (GTK_TABLE (mt->timeline_table), ahbox, 6, 7, rows, rows+1, GTK_FILL, (GtkAttachOptions)0, 0, 0);
	  
	  g_object_set_data (G_OBJECT(aeventbox),"labelbox",labelbox);
	  g_object_set_data (G_OBJECT(aeventbox),"ahbox",ahbox);
	  g_object_set_data (G_OBJECT(ahbox),"eventbox",aeventbox);
	  g_object_set_data(G_OBJECT(labelbox),"layer_number",GINT_TO_POINTER(GPOINTER_TO_INT(g_object_get_data(G_OBJECT(eventbox),"layer_number"))));
	  gtk_widget_modify_bg(aeventbox, GTK_STATE_NORMAL, &palette->white);
	  
	  g_signal_connect (GTK_OBJECT (labelbox), "button_press_event",
			    G_CALLBACK (atrack_ebox_pressed),
			    (gpointer)mt);
	  
	  g_signal_connect (GTK_OBJECT (ahbox), "button_press_event",
			    G_CALLBACK (track_arrow_pressed),
			    (gpointer)mt);
	  
	  gtk_table_attach (GTK_TABLE (mt->timeline_table), aeventbox, 7, 40, rows, rows+1,
			    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			    (GtkAttachOptions) (GTK_FILL), 0, 0);
	  
	  g_signal_connect (GTK_OBJECT (aeventbox), "button_press_event",
			    G_CALLBACK (on_track_click),
			    (gpointer)mt);
	  g_signal_connect (GTK_OBJECT (aeventbox), "button_release_event",
			    G_CALLBACK (on_track_release),
			    (gpointer)mt);
	  exp_track_func=g_signal_connect_after (GTK_OBJECT (aeventbox), "expose_event",
						 G_CALLBACK (expose_track_event),
						 (gpointer)mt);
	  g_object_set_data (G_OBJECT(aeventbox),"expose_func",(gpointer)exp_track_func);
	  

	  rows++;

	  if (rows==mt->max_disp_vtracks) break;

	  if (expanded) {
	    xeventbox=(GtkWidget *)g_object_get_data(G_OBJECT(aeventbox),"achan0");
	    hidden=GPOINTER_TO_INT(g_object_get_data(G_OBJECT(xeventbox),"hidden"))&TRACK_I_HIDDEN_USER;
	    g_object_set_data(G_OBJECT(xeventbox),"hidden",GINT_TO_POINTER(hidden));
	    
	    if (hidden==0) {
	      GTK_ADJUSTMENT(mt->vadjustment)->page_size--;


	      gtk_table_attach (GTK_TABLE (mt->timeline_table), xeventbox, 7, 40, rows, rows+1,
				(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
				(GtkAttachOptions) (GTK_FILL), 0, 0);
	      
	      exp_track_func=g_signal_connect_after (GTK_OBJECT (xeventbox), "expose_event",
						     G_CALLBACK (mt_expose_laudtrack_event),
						     (gpointer)mt);
	      g_object_set_data (G_OBJECT(xeventbox),"expose_func",(gpointer)exp_track_func);
	      
	      gtk_widget_modify_bg(xeventbox, GTK_STATE_NORMAL, &palette->white);

	      rows++;
	      if (rows==mt->max_disp_vtracks) break;
	    }

	    if (cfile->achans>1) {
	      xeventbox=(GtkWidget *)g_object_get_data(G_OBJECT(aeventbox),"achan1");
	      hidden=GPOINTER_TO_INT(g_object_get_data(G_OBJECT(xeventbox),"hidden"))&TRACK_I_HIDDEN_USER;
	      g_object_set_data(G_OBJECT(xeventbox),"hidden",GINT_TO_POINTER(hidden));
	      
	      if (hidden==0) {
		GTK_ADJUSTMENT(mt->vadjustment)->page_size--;
		
		gtk_table_attach (GTK_TABLE (mt->timeline_table), xeventbox, 7, 40, rows, rows+1,
				  (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
				  (GtkAttachOptions) (GTK_FILL), 0, 0);
		
		gtk_widget_modify_bg(xeventbox, GTK_STATE_NORMAL, &palette->white);
		exp_track_func=g_signal_connect_after (GTK_OBJECT (xeventbox), "expose_event",
						       G_CALLBACK (mt_expose_raudtrack_event),
						       (gpointer)mt);
		
		g_object_set_data (G_OBJECT(xeventbox),"expose_func",(gpointer)exp_track_func);
		
		rows++;
		if (rows==mt->max_disp_vtracks) break;

	      }
	    }
	  }
	}
      }
    }
    vdraws=vdraws->next;
  }
  

  if (GTK_ADJUSTMENT(mt->vadjustment)->page_size<1) GTK_ADJUSTMENT(mt->vadjustment)->page_size=1;

  GTK_ADJUSTMENT(mt->vadjustment)->upper=get_top_track_for(mt,mt->num_video_tracks-1)+GTK_ADJUSTMENT(mt->vadjustment)->page_size;

  table_children=GTK_TABLE(mt->timeline_table)->children;

  while (table_children!=NULL) {
    GtkRequisition req;
    GtkTableChild *child=(GtkTableChild *)table_children->data;
    req=child->widget->requisition;
    gtk_widget_set_size_request(child->widget,req.width,25);
    table_children=table_children->next;
  }
  gtk_widget_show_all(mt->timeline_table);
  gtk_widget_queue_draw (mt->vpaned);

}



gboolean track_arrow_pressed (GtkWidget *ebox, GdkEventButton *event, gpointer user_data) {
  GtkWidget *eventbox=(GtkWidget *)g_object_get_data(G_OBJECT(ebox),"eventbox");
  GtkWidget *arrow=(GtkWidget *)g_object_get_data(G_OBJECT(eventbox),"arrow"),*new_arrow;
  lives_mt *mt=(lives_mt *)user_data;
  gboolean expanded=!(g_object_get_data(G_OBJECT(eventbox),"expanded"));

  if (mt->audio_draws==NULL||(!mt->opts.pertrack_audio&&(mt->opts.back_audio_tracks==0||
							 eventbox!=mt->audio_draws->data))) {
    track_ebox_pressed(eventbox,NULL,mt);
    return FALSE;
  }

  g_object_set_data(G_OBJECT(eventbox),"expanded",GINT_TO_POINTER(expanded));

  if (!expanded) {
    new_arrow = gtk_arrow_new (GTK_ARROW_RIGHT, GTK_SHADOW_OUT);
  }
  else {
    new_arrow = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_OUT);
  }


  gtk_widget_ref(new_arrow);

  g_object_set_data(G_OBJECT(eventbox),"arrow",new_arrow);

  gtk_tooltips_copy(new_arrow,arrow);

  // must do this after we update object data, to avoid a race condition
  gtk_widget_unref(arrow);
  gtk_widget_destroy(arrow);

  scroll_tracks(mt,mt->top_track);
  track_select(mt);
  return FALSE;
}



void multitrack_view_clips (GtkMenuItem *menuitem, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  polymorph (mt,POLY_CLIPS);
}


void multitrack_view_in_out (GtkMenuItem *menuitem, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  if (mt->block_selected==NULL) return;
  polymorph (mt,POLY_IN_OUT);
}


static void time_to_string (lives_mt *mt, gdouble secs, gint length) {
  gint hours,mins,rest;
  gchar *string;

  hours=secs/3600;
  secs-=hours*3600.;
  mins=secs/60;
  secs-=mins*60.;
  rest=(secs-((gint)secs)*1.)*100.+.5;
  secs=(gint)secs*1.;
  string=g_strdup_printf("   %02d:%02d:%02d.%02d",hours,mins,(gint)secs,rest);
  gtk_entry_set_text (GTK_ENTRY (mt->timecode),string);
  g_free(string);
}


static void renumber_clips(void) {
  // remove gaps in our mainw->files array - caused when clips are closed
  // we also ensure each clip has a (non-zero) 64 bit unique_id to help with later id of the clips
  // this is not strictly necessary any more, since we now track clips in the layout by 
  // handle and unique id

  // however, it helps to keep the numbers low if many files have been closed

  // called once when we enter multitrack mode

  int cclip;
  int i=1,j;

  GList *clist;

  gboolean bad_header=FALSE;

  renumbered_clips[0]=0;

  // walk through files mainw->files[cclip]
  // mainw->files[i] points to next non-NULL clip

  // if we find a gap we move i to cclip


  for (cclip=1;i<=MAX_FILES;cclip++) {
    if (mainw->files[cclip]==NULL) {

      if (i!=cclip) {
	mainw->files[cclip]=mainw->files[i];

	for (j=0;j<FN_KEYS-1;j++) {
	  if (mainw->clipstore[j]==i) mainw->clipstore[j]=cclip;
	}

	// we need to change the entries in mainw->cliplist
	clist=mainw->cliplist;
	while (clist!=NULL) {
	  if (GPOINTER_TO_INT(clist->data)==i) {
	    clist->data=GINT_TO_POINTER(cclip);
	    break;
	  }
	  clist=clist->next;
	}

	mainw->files[i]=NULL;
	
	if (mainw->scrap_file==i) mainw->scrap_file=cclip;
	if (mainw->ascrap_file==i) mainw->ascrap_file=cclip;
	if (mainw->current_file==i) mainw->current_file=cclip;

	if (mainw->first_free_file==cclip) mainw->first_free_file++;

	renumbered_clips[i]=cclip;

      }
      // process this clip again
      else cclip--;
    }

    else {
      renumbered_clips[cclip]=cclip;
      if (i==cclip) i++;
    }

    if (mainw->files[cclip]!=NULL&&cclip!=mainw->scrap_file&&cclip!=mainw->ascrap_file&&
	(mainw->files[cclip]->clip_type==CLIP_TYPE_DISK||mainw->files[cclip]->clip_type==CLIP_TYPE_FILE)&&
	mainw->files[cclip]->unique_id==0l) {
      mainw->files[cclip]->unique_id=lives_random();
      save_clip_value(cclip,CLIP_DETAILS_UNIQUE_ID,&mainw->files[cclip]->unique_id);
      if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;

      if (bad_header) do_header_write_error(cclip);
    }

    for (;i<=MAX_FILES;i++) {
      if (mainw->files[i]!=NULL) break;
    }
  }
}


static void rerenumber_clips(const char *lfile) {
  // we loaded an event_list, now we match clip numbers in event_list with our current clips, using the layout map file
  // the renumbering is used for translations in event_list_rectify
  // in mt_init_tracks we alter the clip numbers in the event_list

  // this means if we save again, the clip numbers in the disk event list (*.lay file) may be updated
  // however, since we also have a layout map file (*.map) for the set, this should not be too big an issue



  GList *lmap;
  int i,rnc;
  gchar **array;

  renumbered_clips[0]=0;
  
  for (i=1;i<=MAX_FILES&&mainw->files[i]!=NULL;i++) {
    renumbered_clips[i]=0;
    if (mainw->files[i]!=NULL) lfps[i]=mainw->files[i]->fps;
    else lfps[i]=cfile->fps;
  }

  if (lfile!=NULL) {
    // lfile is supplied layout file name
    for (i=1;i<=MAX_FILES&&mainw->files[i]!=NULL;i++) {
      lmap=mainw->files[i]->layout_map;
      while (lmap!=NULL) {

	// lmap->data starts with layout name
	if (!strncmp((gchar *)lmap->data,lfile,strlen(lfile))) {
	  threaded_dialog_spin();
	  array=g_strsplit((gchar *)lmap->data,"|",-1);
	  threaded_dialog_spin();

	  // piece 2 is the clip number
	  rnc=atoi(array[1]);
	  
	  renumbered_clips[rnc]=i;

	  // original fps
	  lfps[i]=strtod(array[3],NULL);
	  threaded_dialog_spin();
	  g_strfreev(array);
	  threaded_dialog_spin();
	}
	lmap=lmap->next;
      }
    }
  }
  else {
    // current event_list
    for (i=1;i<=MAX_FILES&&mainw->files[i]!=NULL;i++) {
      if (mainw->files[i]->stored_layout_idx!=-1) {
	renumbered_clips[mainw->files[i]->stored_layout_idx]=i;
      }
      lfps[i]=mainw->files[i]->stored_layout_fps;
    }
  }
  
}



void mt_clip_select (lives_mt *mt, gboolean scroll) {
  GList *list=GTK_BOX (mt->clip_inner_box)->children;
  GtkBoxChild *clipbox=NULL;
  gint len;
  int i;
  gboolean was_neg=FALSE;
  mt->file_selected=-1;

  if (list==NULL) return;
  if (mt->poly_state==POLY_FX_STACK&&mt->event_list!=NULL) {
    if (!mt->was_undo_redo) {
      polymorph(mt,POLY_FX_STACK);
    }
  }
  else polymorph(mt,POLY_CLIPS);

  if (mt->clip_selected<0) {
    was_neg=TRUE;
    mt->clip_selected=-mt->clip_selected;
  }

  if (mt->clip_selected>=(len=g_list_length(list))&&!was_neg) mt->clip_selected=0;

  if (was_neg) mt->clip_selected--;

  if (mt->clip_selected<0||(was_neg&&mt->clip_selected==0)) mt->clip_selected=len-1;

  if (mt->clip_selected<0) {
    mt->file_selected=-1;
    return;
  }

  mt->file_selected=mt_file_from_clip(mt,mt->clip_selected);

  for (i=0;i<len;i++) {
    clipbox=(GtkBoxChild *)g_list_nth_data (list,i);
    if (i==mt->clip_selected) {
      GtkAdjustment *adj;
      gint value=(adj=GTK_RANGE(GTK_SCROLLED_WINDOW(mt->clip_scroll)->hscrollbar)->adjustment)->upper*(mt->clip_selected+.5)/len;
      if (scroll) gtk_adjustment_clamp_page(adj,value-adj->page_size/2,value+adj->page_size/2);
      gtk_widget_set_state(clipbox->widget,GTK_STATE_SELECTED);
      gtk_widget_set_sensitive (mt->adjust_start_end, mainw->files[mt->file_selected]->frames>0);
      if (mt->current_track>-1) {
	gtk_widget_set_sensitive (mt->insert, mainw->files[mt->file_selected]->frames>0);
	gtk_widget_set_sensitive (mt->audio_insert, FALSE);
      }
      else {
	gtk_widget_set_sensitive (mt->audio_insert, mainw->files[mt->file_selected]->achans>0);
	gtk_widget_set_sensitive (mt->insert, FALSE);
      }
    }
    else gtk_widget_set_state(clipbox->widget,GTK_STATE_NORMAL);
  }
}


gboolean mt_prevclip (GtkAccelGroup *group, GObject *obj, guint keyval, GdkModifierType mod, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  mt->clip_selected--;
  polymorph(mt,POLY_CLIPS);
  mt_clip_select(mt,TRUE);
  return TRUE;
}

gboolean mt_nextclip (GtkAccelGroup *group, GObject *obj, guint keyval, GdkModifierType mod, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  mt->clip_selected++;
  polymorph(mt,POLY_CLIPS);
  mt_clip_select(mt,TRUE);
  return TRUE;
}


static void set_time_scrollbar(lives_mt *mt) {
  gdouble page=mt->tl_max-mt->tl_min;
  if (mt->end_secs==0.) mt->end_secs=DEF_TIME;

  if (mt->tl_max>mt->end_secs) mt->end_secs=mt->tl_max;

  gtk_range_set_range(GTK_RANGE(mt->time_scrollbar),0.,mt->end_secs);
  gtk_range_set_increments(GTK_RANGE(mt->time_scrollbar),page/4.,page);
  GTK_ADJUSTMENT(mt->hadjustment)->value=mt->tl_min;
  GTK_ADJUSTMENT(mt->hadjustment)->page_size=page;
  gtk_widget_queue_draw(mt->time_scrollbar);
}

static void redraw_all_event_boxes(lives_mt *mt) {
  GList *slist;

  slist=mt->audio_draws;
  while (slist!=NULL) {
    redraw_eventbox(mt,(GtkWidget *)slist->data);
    slist=slist->next;
  }

  slist=mt->video_draws;
  while (slist!=NULL) {
    redraw_eventbox(mt,(GtkWidget *)slist->data);
    slist=slist->next;
  }
}

void set_timeline_end_secs (lives_mt *mt, gdouble secs) {
  gdouble pos=GTK_RULER (mt->timeline)->position;

  mt->end_secs=secs;

  gtk_ruler_set_range (GTK_RULER (mt->timeline), mt->tl_min, mt->tl_max, mt->tl_min, mt->end_secs+1./mt->fps);
  gtk_widget_queue_draw (mt->timeline);
  gtk_widget_queue_draw (mt->timeline_table);
  gtk_spin_button_set_range (GTK_SPIN_BUTTON (mt->spinbutton_start),0.,mt->end_secs);
  gtk_spin_button_set_range (GTK_SPIN_BUTTON (mt->spinbutton_end),0.,mt->end_secs+1./mt->fps);

  set_time_scrollbar(mt);

  GTK_RULER (mt->timeline)->position=pos;

  redraw_all_event_boxes(mt);
}




static weed_timecode_t set_play_position(lives_mt *mt) {
  // get start event
  gboolean has_pb_loop_event=FALSE;
  weed_timecode_t tc;
#ifdef ENABLE_JACK_TRANSPORT
  weed_timecode_t end_tc=event_list_get_end_tc(mt->event_list);
#endif  

  mainw->cancelled=CANCEL_NONE;

#ifdef ENABLE_JACK_TRANSPORT
  if (mainw->jack_can_stop&&(prefs->jack_opts&JACK_OPTS_TIMEBASE_START)&&(prefs->jack_opts&JACK_OPTS_TRANSPORT_CLIENT)) {
    mt->pb_loop_event=get_first_frame_event(mt->event_list);
    has_pb_loop_event=TRUE;
    tc=q_gint64(U_SEC*jack_transport_get_time(),cfile->fps);
    if (!mainw->loop_cont) {
      if (tc>end_tc) {
	mainw->cancelled=CANCEL_VID_END;
	return 0;
      }
    }
    tc%=end_tc;
    mt->is_paused=FALSE;
  }
  else {
#endif
    tc=q_gint64(GTK_RULER(mt->timeline)->position*U_SEC,cfile->fps);
#ifdef ENABLE_JACK_TRANSPORT
  }
#endif
  if (tc>event_list_get_end_tc(mt->event_list)||tc==0) mt->pb_start_event=get_first_frame_event(mt->event_list);
  else {
    mt->pb_start_event=get_frame_event_at(mt->event_list,tc,NULL,TRUE);
  }

  if (!has_pb_loop_event) mt->pb_loop_event=mt->pb_start_event;

  return get_event_timecode(mt->pb_start_event);
}

 
 
 
void mt_show_current_frame(lives_mt *mt, gboolean return_layer) {
  // show preview of current frame in play_box and/or play_window

  // or, if return_layer is TRUE, we just set mainw->frame_layer

  gint current_file;
  gdouble ptr_time=GTK_RULER(mt->timeline)->position;
  gboolean is_rendering=mainw->is_rendering;
  weed_timecode_t curr_tc;
  gint actual_frame;
  weed_plant_t *frame_layer=mainw->frame_layer;
  gboolean internal_messaging=mainw->internal_messaging;
  gboolean needs_idlefunc=FALSE;
  cairo_t *cr;

  if (mt->idlefunc>0) {
    g_source_remove(mt->idlefunc);
    mt->idlefunc=0;
    needs_idlefunc=TRUE;
  }

  if (!return_layer) {
    // show frame image in window
    if (!mt->mt_frame_preview) {
      gboolean sep_win=mainw->sep_win;
      mt->mt_frame_preview=TRUE;

      if (mt->play_blank->parent!=NULL) {
	gtk_widget_ref(mt->play_blank);
	gtk_container_remove (GTK_CONTAINER(mt->play_box),mt->play_blank);
      }

      if (mainw->plug!=NULL) {
	gtk_container_remove (GTK_CONTAINER(mainw->plug),mainw->image274);
	gtk_widget_destroy (mainw->plug);
	mainw->plug=NULL;
      }

      if (GTK_IS_WIDGET(mainw->playarea)) gtk_widget_destroy (mainw->playarea);
      mainw->playarea = gtk_hbox_new (FALSE,0);
      gtk_widget_show(mainw->playarea);
      gtk_container_add (GTK_CONTAINER (mt->play_box), mainw->playarea);
      //gtk_widget_set_app_paintable(mainw->playarea,TRUE);

      if (mt->is_ready)
	while (g_main_context_iteration(NULL,FALSE));
  
      mainw->sep_win=FALSE;
      add_to_playframe();
      mainw->sep_win=sep_win;
    }
  }

  if (mainw->playing_file>-1) {

    return;
  }
  // start "playback" at current pos, we just "play" one frame
  curr_tc=set_play_position(mt);
  actual_frame=(gint)((gdouble)(curr_tc/U_SECL)*cfile->fps+1.4999);
  mainw->frame_layer=NULL;

  if (mt->is_rendering&&actual_frame<=cfile->frames) {
    // get the actual frame if it has already been rendered
    mainw->frame_layer=weed_plant_new(WEED_PLANT_CHANNEL);
    weed_set_int_value(mainw->frame_layer,"clip",mainw->current_file);
    weed_set_int_value(mainw->frame_layer,"frame",actual_frame);
    pull_frame(mainw->frame_layer,prefs->image_ext,curr_tc);
  }
  else {
    mainw->is_rendering=TRUE;
  
    if (mt->pb_start_event!=NULL) {
      // "play" a single frame
      current_file=mainw->current_file;
      mainw->internal_messaging=TRUE; // stop load_frame from showing image
      cfile->next_event=mt->pb_start_event;
      if (is_rendering) {
	backup_weed_instances();
	backup_host_tags(mt->event_list,curr_tc);
      }

      // pass quickly through events_list, switching on and off effects an interpolating at current time
      get_audio_and_effects_state_at(mt->event_list,mt->pb_start_event,FALSE,mt->exact_preview);

      // if we are previewing a specific effect we also need to init it
      if (mt->current_rfx!=NULL&&mt->init_event!=NULL) {
	if (mt->current_rfx->source_type==LIVES_RFX_SOURCE_WEED&&mt->current_rfx->source!=NULL) {
	  weed_plant_t *inst=(weed_plant_t *)mt->current_rfx->source;
	  weed_call_init_func(inst);
	}
      }

      mainw->last_display_ticks=0;
      process_events(mt->pb_start_event,0);
      mainw->internal_messaging=internal_messaging;
      mainw->current_file=current_file;
      deinit_render_effects();

      // if we are previewing an effect we now need to deinit it
      if (mt->current_rfx!=NULL&&mt->init_event!=NULL) {
	if (mt->current_rfx->source_type==LIVES_RFX_SOURCE_WEED&&mt->current_rfx->source!=NULL) {
	  weed_plant_t *inst=(weed_plant_t *)mt->current_rfx->source;
	  weed_call_deinit_func(inst);
	}
      }

      if (is_rendering) {
	restore_weed_instances();
	restore_host_tags(mt->event_list,curr_tc);
      }
      cfile->next_event=NULL;
      mainw->is_rendering=is_rendering;
    }
  }

  if (return_layer) return;

  mt->outwidth=cfile->hsize;
  mt->outheight=cfile->vsize;
  calc_maxspect(mt->play_width,mt->play_height,&mt->outwidth,&mt->outheight);

  if (mainw->frame_layer!=NULL) {
    int fx_layer_palette;
    GdkPixbuf *pixbuf;
    int weed_error;

    mainw->pwidth=mt->outwidth;
    mainw->pheight=mt->outheight;

    fx_layer_palette=weed_get_int_value(mainw->frame_layer,"current_palette",&weed_error);
    if (fx_layer_palette!=WEED_PALETTE_RGB24) convert_layer_palette(mainw->frame_layer,WEED_PALETTE_RGB24,0);

    if (mainw->play_window==NULL||(mt->poly_state==POLY_PARAMS&&mt->framedraw!=NULL)) {
      if ((mt->outwidth!=(weed_get_int_value(mainw->frame_layer,"width",&weed_error))||
	   mt->outheight!=weed_get_int_value(mainw->frame_layer,"height",&weed_error))) 
	resize_layer(mainw->frame_layer,mt->outwidth,mt->outheight,LIVES_INTERP_BEST);
    }
    else resize_layer(mainw->frame_layer,cfile->hsize,cfile->vsize,LIVES_INTERP_BEST);

    pixbuf=layer_to_pixbuf(mainw->frame_layer);
    weed_plant_free(mainw->frame_layer);
    mainw->frame_layer=NULL;

    if (mainw->play_window!=NULL&&GDK_IS_WINDOW (mainw->play_window->window)) {

      mainw->pwidth=cfile->hsize;
      mainw->pheight=cfile->vsize;

      // force signal unblocked
      g_signal_handlers_block_matched(mainw->play_window,(GSignalMatchType)(G_SIGNAL_MATCH_FUNC|G_SIGNAL_MATCH_UNBLOCKED),
				      0,0,0,(gpointer)expose_play_window,NULL);
      g_signal_handler_unblock(mainw->play_window,mainw->pw_exp_func);
      mainw->pw_exp_is_blocked=FALSE;

      gtk_window_resize (GTK_WINDOW (mainw->play_window), mainw->pwidth, mainw->pheight);

      cr = gdk_cairo_create (mainw->play_window->window);

      gdk_cairo_set_source_pixbuf (cr, pixbuf, 0, 0);
      cairo_paint (cr);
      cairo_destroy (cr);

      gtk_widget_queue_draw (mainw->play_window);
      gtk_widget_queue_resize (mainw->play_window);
      if (mt->framedraw==NULL||!mt_framedraw(mt,pixbuf)) {
	if (mt->poly_state!=POLY_PARAMS) {
	  gtk_image_set_from_pixbuf(GTK_IMAGE(mainw->image274),mainw->imframe);
	  gtk_widget_queue_draw(mt->play_box);
	}
	else {
	  gtk_image_set_from_pixbuf(GTK_IMAGE(mainw->image274),pixbuf);
	  gtk_widget_queue_draw(mt->play_box);
	}
      }
    }
    else {
      if (mt->framedraw==NULL||!mt_framedraw(mt,pixbuf)) {
	gtk_image_set_from_pixbuf(GTK_IMAGE(mainw->image274),pixbuf);
	gtk_widget_queue_draw(mt->play_box);
      }
    }
    if (mt->sepwin_pixbuf!=NULL&&mt->sepwin_pixbuf!=mainw->imframe) {
      gdk_pixbuf_unref(mt->sepwin_pixbuf);
      mt->sepwin_pixbuf=NULL;
    }
    if (mt->framedraw==NULL) mt->sepwin_pixbuf=pixbuf;
    else if (mainw->imframe!=NULL) 
      mt->sepwin_pixbuf=lives_pixbuf_scale_simple(mainw->imframe,cfile->hsize,cfile->vsize,LIVES_INTERP_BEST);
  }
  else {
    // no frame - show blank
    if (mainw->play_window!=NULL&&GDK_IS_WINDOW (mainw->play_window->window)) {

      mainw->pwidth=lives_pixbuf_get_width(mainw->imframe);
      mainw->pheight=lives_pixbuf_get_height(mainw->imframe);

      gtk_window_resize (GTK_WINDOW (mainw->play_window), mainw->pwidth, mainw->pheight);

      cr = gdk_cairo_create (mainw->play_window->window);
      gdk_cairo_set_source_pixbuf (cr, GDK_PIXBUF(mainw->imframe), 0, 0);
      cairo_paint (cr);
      cairo_destroy (cr);

      gtk_widget_queue_draw (mainw->play_window);
    }

    if (mt->poly_state!=POLY_PARAMS) {
      gtk_image_set_from_pixbuf(GTK_IMAGE(mainw->image274),mainw->imframe);
      gtk_widget_queue_draw(mt->play_box);
    }

    if (mt->sepwin_pixbuf!=NULL&&mt->sepwin_pixbuf!=mainw->imframe) gdk_pixbuf_unref(mt->sepwin_pixbuf);
    mt->sepwin_pixbuf=mainw->imframe;
  }

  mainw->frame_layer=frame_layer;

  GTK_RULER(mt->timeline)->position=ptr_time;
  gtk_widget_queue_draw(mt->timeline);

  if (needs_idlefunc) {
    mt->idlefunc=mt_idle_add(mt);
  }

}



void mt_tl_move(lives_mt *mt, gdouble pos_rel) {
  gdouble pos;
  if (mainw->playing_file>-1) return;

  pos=GTK_RULER (mt->timeline)->position+pos_rel;

  pos=q_dbl(pos,mt->fps)/U_SEC;
  if (pos<0.) pos=0.;

  mt->ptr_time=GTK_RULER (mt->timeline)->position=pos;

  if (pos>0.) {
    gtk_widget_set_sensitive (mt->rewind,TRUE);
    gtk_widget_set_sensitive (mainw->m_rewindbutton, TRUE);
  }
  else {
    gtk_widget_set_sensitive (mt->rewind,FALSE);
    gtk_widget_set_sensitive (mainw->m_rewindbutton, FALSE);
  }

  if (mt->is_paused) {
    mt->is_paused=FALSE;
    gtk_widget_set_sensitive (mainw->stop, FALSE);
    gtk_widget_set_sensitive (mainw->m_stopbutton, FALSE);
  }

  gtk_widget_queue_draw(mt->timeline);
  if (mt->init_event!=NULL&&mt->poly_state==POLY_PARAMS&&!mt->block_node_spin) {
    mt->block_tl_move=TRUE;
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(mt->node_spinbutton),pos-get_event_timecode(mt->init_event)/U_SEC);
    mt->block_tl_move=FALSE;
  }
  time_to_string (mt,pos,TIMECODE_LENGTH);

  if (pos>mt->region_end-1./mt->fps) gtk_widget_set_sensitive(mt->tc_to_rs,FALSE);
  else gtk_widget_set_sensitive(mt->tc_to_rs,TRUE);
  if (pos<mt->region_start+1./mt->fps) gtk_widget_set_sensitive(mt->tc_to_re,FALSE);
  else gtk_widget_set_sensitive(mt->tc_to_re,TRUE);

  mt->fx_order=FX_ORD_NONE;

  if (mt->selected_init_event!=NULL) {
    int error;
    weed_timecode_t tc=q_gint64(pos*U_SEC,mt->fps);
    weed_plant_t *deinit_event=(weed_plant_t *)weed_get_voidptr_value(mt->selected_init_event,"deinit_event",&error);
    if (tc<get_event_timecode(mt->selected_init_event)||tc>get_event_timecode(deinit_event)) {
      mt->selected_init_event=NULL;
    }
  }

  if (mt->poly_state==POLY_FX_STACK) polymorph(mt,POLY_FX_STACK);
  if (mt->is_ready) mt_show_current_frame(mt, FALSE);

}


gboolean mt_tlfor (GtkAccelGroup *group, GObject *obj, guint keyval, GdkModifierType mod, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  mt->fm_edit_event=NULL;
  mt_tl_move(mt,1.);
  return TRUE;
}

gboolean mt_tlfor_frame (GtkAccelGroup *group, GObject *obj, guint keyval, GdkModifierType mod, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  mt->fm_edit_event=NULL;
  mt_tl_move(mt,1./mt->fps);
  return TRUE;
}


gboolean mt_tlback (GtkAccelGroup *group, GObject *obj, guint keyval, GdkModifierType mod, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  mt->fm_edit_event=NULL;
  mt_tl_move(mt,-1.);
  return TRUE;
}

gboolean mt_tlback_frame (GtkAccelGroup *group, GObject *obj, guint keyval, GdkModifierType mod, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  mt->fm_edit_event=NULL;
  mt_tl_move(mt,-1./mt->fps);
  return TRUE;
}



static void scroll_track_on_screen(lives_mt *mt, gint track) {
  if (track>mt->top_track) track=get_top_track_for(mt,track);
  scroll_tracks(mt,track);
 
  return;
}




void
scroll_track_by_scrollbar (GtkVScrollbar *sbar, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  scroll_tracks(mt,GTK_ADJUSTMENT(GTK_RANGE(sbar)->adjustment)->value);
  track_select(mt);
}




static void mt_zoom (lives_mt *mt, gdouble scale) {
  gdouble tl_span=(mt->tl_max-mt->tl_min)/2.;
  gdouble tl_cur;


  if (scale>0.) tl_cur=GTK_RULER(mt->timeline)->position;  // center on cursor
  else {
    tl_cur=mt->tl_min+tl_span; // center on middle of screen
    scale=-scale;
  }

  mt->tl_min=tl_cur-tl_span*scale;
  mt->tl_max=tl_cur+tl_span*scale;

  if (mt->tl_min<0.) {
    mt->tl_max-=mt->tl_min;
    mt->tl_min=0.;
  }

  mt->tl_min=q_gint64(mt->tl_min*U_SEC,mt->fps)/U_SEC;
  mt->tl_max=q_gint64(mt->tl_max*U_SEC,mt->fps)/U_SEC;

  if (mt->tl_min==mt->tl_max) mt->tl_max=mt->tl_min+1./mt->fps;

  GTK_RULER(mt->timeline)->upper=mt->tl_max;
  GTK_RULER(mt->timeline)->lower=mt->tl_min;

  set_time_scrollbar(mt);

  gtk_widget_queue_draw (mt->vpaned);

  redraw_all_event_boxes(mt);

}


static void
scroll_time_by_scrollbar (GtkVScrollbar *sbar, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  mt->tl_min=(GTK_ADJUSTMENT(GTK_RANGE(sbar)->adjustment)->value);
  mt->tl_max=(GTK_ADJUSTMENT(GTK_RANGE(sbar)->adjustment)->value+GTK_ADJUSTMENT(GTK_RANGE(sbar)->adjustment)->page_size);
  mt_zoom(mt,-1.);
}


gboolean mt_trdown (GtkAccelGroup *group, GObject *obj, guint keyval, GdkModifierType mod, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;

  if (mt->current_track>=0&&mt->opts.pertrack_audio&&!mt->aud_track_selected) {
    GtkWidget *eventbox=(GtkWidget *)g_list_nth_data(mt->video_draws,mt->current_track);
    mt->aud_track_selected=GPOINTER_TO_INT(g_object_get_data(G_OBJECT(eventbox),"expanded"));
    if (!mt->aud_track_selected&&mt->current_track==mt->num_video_tracks-1) return TRUE;
  }
  else {
    if (mt->current_track==mt->num_video_tracks-1) return TRUE;
    mt->aud_track_selected=FALSE;
  }

  if (!mt->aud_track_selected||mt->current_track==-1) {
    if (mt->current_track>-1) mt->current_track++;
    else {
      int i=0;
      GList *llist=mt->video_draws;
      while (llist!=NULL) {
	if (GPOINTER_TO_INT(g_object_get_data(G_OBJECT(llist->data),"hidden"))==0) {
	  mt->current_track=i;
	  break;
	}
	llist=llist->next;
	i++;
      }
      mt->current_track=i;
    }
  }
  mt->selected_init_event=NULL;
  scroll_track_on_screen(mt,mt->current_track);
  track_select(mt);

  return TRUE;
}


gboolean mt_trup (GtkAccelGroup *group, GObject *obj, guint keyval, GdkModifierType mod, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  if (mt->current_track==-1||(mt->current_track==0&&!mt->aud_track_selected&&!mt->opts.show_audio)) return TRUE;

  if (mt->aud_track_selected) mt->aud_track_selected=FALSE;
  else {
    mt->current_track--;
    if (mt->current_track>=0&&mt->opts.pertrack_audio) {
      GtkWidget *eventbox=(GtkWidget *)g_list_nth_data(mt->video_draws,mt->current_track);
      mt->aud_track_selected=GPOINTER_TO_INT(g_object_get_data(G_OBJECT(eventbox),"expanded"));
    }
  }
  mt->selected_init_event=NULL;
  if (mt->current_track!=-1) scroll_track_on_screen(mt,mt->current_track);
  track_select(mt);

  return TRUE;
}



LIVES_INLINE gint poly_page_to_tab(guint page) {
  return ++page;
}

LIVES_INLINE gint poly_tab_to_page(guint tab) {
  return --tab;
}


LIVES_INLINE lives_mt_poly_state_t get_poly_state_from_page(lives_mt *mt) {
  return (lives_mt_poly_state_t)poly_page_to_tab(gtk_notebook_get_current_page(GTK_NOTEBOOK(mt->nb)));
}


static void notebook_error(GtkNotebook *nb, guint tab, lives_mt_nb_error_t err, lives_mt *mt) {
  guint page=poly_tab_to_page(tab);

  if (mt->nb_label!=NULL) gtk_widget_destroy(mt->nb_label);

  switch(err) {
  case NB_ERROR_SEL:
    mt->nb_label=gtk_label_new(_("\n\nPlease select a block\nin the timeline by\nright or double clicking on it.\n"));
    break;
  case NB_ERROR_NOEFFECT:
    mt->nb_label=gtk_label_new(_("\n\nNo effect selected.\nSelect an effect in FX stack first to view its parameters.\n"));
    break;
  case NB_ERROR_NOCLIP:
    mt->nb_label=gtk_label_new(_("\n\nNo clips loaded.\n"));
    break;
  case NB_ERROR_NOTRANS:
    mt->nb_label=gtk_label_new(_("You must select two video tracks\nand a time region\nto apply transitions.\n\nAlternately, you can enable Autotransitions from the Effects menu\nbefore inserting clips into the timeline."));
    break;
  case NB_ERROR_NOCOMP:
    mt->nb_label=gtk_label_new(_("\n\nYou must select at least one video track\nand a time region\nto apply compositors.\n"));
    break;
  }

  gtk_label_set_justify (GTK_LABEL (mt->nb_label), GTK_JUSTIFY_CENTER);
  gtk_widget_modify_fg(mt->nb_label, GTK_STATE_NORMAL, &palette->normal_fore);
  gtk_container_add(GTK_CONTAINER(gtk_notebook_get_nth_page(GTK_NOTEBOOK(nb),page)),mt->nb_label);
  gtk_widget_show(mt->nb_label);
  gtk_widget_hide(mt->poly_box);

  gtk_widget_queue_resize(mt->nb_label);

}


static void fubar(lives_mt *mt) {
  int npch,i,error;
  int num_in_tracks;
  int *in_tracks;
  void **pchainx;
  gchar *fhash;

  mt->init_event=mt->selected_init_event;

  mt->track_index=-1;

  if ((num_in_tracks=weed_leaf_num_elements(mt->init_event,"in_tracks"))>0) {
    in_tracks=weed_get_int_array(mt->init_event,"in_tracks",&error);
    // set track_index (for special widgets)
    for (i=0;i<num_in_tracks;i++) {
      if (mt->current_track==in_tracks[i]) mt->track_index=i;
    }
    weed_free(in_tracks);
  }

  fhash=weed_get_string_value(mt->init_event,"filter",&error);
  mt->current_fx=weed_get_idx_for_hashname(fhash,TRUE);
  weed_free(fhash);
  
  if (weed_plant_has_leaf(mt->selected_init_event,"in_parameters")&&weed_get_voidptr_value(mt->selected_init_event,"in_parameters",&error)!=NULL) {
    npch=weed_leaf_num_elements(mt->init_event,"in_parameters");
    pchainx=weed_get_voidptr_array(mt->init_event,"in_parameters",&error);
    pchain=(void **)g_malloc(npch*sizeof(void *)); // because we later g_free(), must use g_malloc and not weed_malloc (althought normally weed_malloc==g_malloc)
    for (i=0;i<npch;i++) pchain[i]=pchainx[i];
    weed_free(pchainx);
  }
 }






static gboolean notebook_page(GtkWidget *nb, GtkNotebookPage *nbp, guint tab, gpointer user_data) {
  guint page;
  lives_mt *mt=(lives_mt *)user_data;

  if (nbp!=NULL) {
    page=tab;
    tab=poly_page_to_tab(page);
  }
  else {
    page=poly_tab_to_page(tab);
  }

  if (mt->nb_label!=NULL) gtk_widget_destroy(mt->nb_label);
  mt->nb_label=NULL;

  gtk_widget_show(mt->poly_box);

  switch (tab) {
  case POLY_CLIPS:
    if (mt->clip_labels==NULL) {
      notebook_error(GTK_NOTEBOOK(nb),tab,NB_ERROR_NOCLIP,mt);
      return FALSE;
    }
    if (mt->poly_state!=POLY_CLIPS) polymorph(mt,POLY_CLIPS);
    else gtk_widget_reparent(mt->poly_box,gtk_notebook_get_nth_page(GTK_NOTEBOOK(nb),page));
    break;
  case POLY_IN_OUT:
    if (mt->block_selected==NULL&&mt->poly_state!=POLY_IN_OUT) {
      notebook_error(GTK_NOTEBOOK(nb),tab,NB_ERROR_SEL,mt);
      return FALSE;
    }
    if (mt->poly_state!=POLY_IN_OUT) polymorph(mt,POLY_IN_OUT);
    else gtk_widget_reparent(mt->poly_box,gtk_notebook_get_nth_page(GTK_NOTEBOOK(nb),page));
    break;
  case POLY_FX_STACK:
    if (mt->poly_state!=POLY_FX_STACK) polymorph(mt,POLY_FX_STACK);
    else gtk_widget_reparent(mt->poly_box,gtk_notebook_get_nth_page(GTK_NOTEBOOK(nb),page));
    break;
  case POLY_EFFECTS:
    if (mt->block_selected==NULL&&mt->poly_state!=POLY_EFFECTS) {
      notebook_error(GTK_NOTEBOOK(nb),tab,NB_ERROR_SEL,mt);
      return FALSE;
    }
    if (mt->poly_state!=POLY_EFFECTS) polymorph(mt,POLY_EFFECTS);
    else gtk_widget_reparent(mt->poly_box,gtk_notebook_get_nth_page(GTK_NOTEBOOK(nb),page));
    break;
  case POLY_TRANS:
    if (g_list_length(mt->selected_tracks)!=2||mt->region_start==mt->region_end) {
      notebook_error(GTK_NOTEBOOK(nb),tab,NB_ERROR_NOTRANS,mt);
      return FALSE;
    }
    if (mt->poly_state!=POLY_TRANS) polymorph(mt,POLY_TRANS);
    else gtk_widget_reparent(mt->poly_box,gtk_notebook_get_nth_page(GTK_NOTEBOOK(nb),page));
    break;
  case POLY_COMP:
    if (mt->selected_tracks==NULL||mt->region_start==mt->region_end) {
      notebook_error(GTK_NOTEBOOK(nb),tab,NB_ERROR_NOCOMP,mt);
      return FALSE;
    }
    if (mt->poly_state!=POLY_COMP) polymorph(mt,POLY_COMP);
    else gtk_widget_reparent(mt->poly_box,gtk_notebook_get_nth_page(GTK_NOTEBOOK(nb),page));
    break;
  case POLY_PARAMS:
    if (mt->poly_state!=POLY_PARAMS&&mt->selected_init_event==NULL) {
      notebook_error(GTK_NOTEBOOK(nb),tab,NB_ERROR_NOEFFECT,mt);
      return FALSE;
    }
    gtk_widget_reparent(mt->poly_box,gtk_notebook_get_nth_page(GTK_NOTEBOOK(nb),page));
    if (mt->selected_init_event!=NULL&&mt->poly_state!=POLY_PARAMS) {
      fubar(mt);
      polymorph(mt,POLY_PARAMS);
    }
    break;
  }

  return FALSE;
} 


static void set_poly_tab(lives_mt *mt, guint tab) {
  gint page=poly_tab_to_page(tab);

  if (gtk_notebook_get_current_page(GTK_NOTEBOOK(mt->nb))==page) notebook_page(mt->nb,NULL,tab,mt);
  else gtk_notebook_set_page(GTK_NOTEBOOK(mt->nb),page);
}



static void select_block (lives_mt *mt) {
  track_rect *block=mt->putative_block;
  gint track;
  gint filenum;
  gchar *tmp,*tmp2;

  if (block!=NULL) {
    GtkWidget *eventbox=block->eventbox;

    if (cfile->achans==0||mt->audio_draws==NULL||(mt->opts.back_audio_tracks==0||eventbox!=mt->audio_draws->data)) track=GPOINTER_TO_INT(g_object_get_data(G_OBJECT(eventbox),"layer_number"));
    else track=-1;

    if (cfile->achans==0||mt->audio_draws==NULL||!is_audio_eventbox(mt,eventbox)) filenum=get_frame_event_clip(block->start_event,track);
    else filenum=get_audio_frame_clip(block->start_event,track);
    block->state=BLOCK_SELECTED;
    mt->block_selected=block;

    clear_context(mt);

    if (cfile->achans==0||mt->audio_draws==NULL||(mt->opts.back_audio_tracks==0||eventbox!=mt->audio_draws->data)) add_context_label (mt,(tmp2=g_strdup_printf (_("Current track: %s (layer %d)\n"),g_object_get_data(G_OBJECT(eventbox),"track_name"),track)));
    else add_context_label (mt,(tmp2=g_strdup (_("Current track: Backing audio\n"))));
    g_free(tmp2);
    
    add_context_label (mt,(tmp2=g_strdup_printf (_("%.2f sec. to %.2f sec.\n"),get_event_timecode(block->start_event)/U_SEC,get_event_timecode(block->end_event)/U_SEC+1./mt->fps)));
    g_free(tmp2);
    add_context_label (mt,(tmp2=g_strdup_printf (_("Source: %s"),(tmp=g_path_get_basename (mainw->files[filenum]->name)))));
    g_free(tmp2);
    add_context_label (mt,(_("Right click for context menu.\n")));
    add_context_label (mt,(_("Single click on timeline\nto select a frame.\n")));
    g_free(tmp);
    gtk_widget_set_sensitive(mt->view_in_out,TRUE);
    gtk_widget_set_sensitive (mt->fx_block, TRUE);

    redraw_eventbox(mt,eventbox);

    multitrack_view_in_out(NULL,mt);
  }

  mt->context_time=-1.;
  
}


static gboolean
on_drag_filter_end           (GtkWidget       *widget,
			      GdkEventButton  *event,
			      gpointer         user_data) {
  GdkWindow *window;
  GtkWidget *eventbox;
  GtkWidget *labelbox;
  GtkWidget *ahbox;
  GtkWidget *menuitem;
  lives_mt *mt=(lives_mt *)user_data;
  int win_x,win_y;
  int i;
  gint tchan=0;
  gdouble timesecs;

  if (mt->cursor_style!=LIVES_CURSOR_FX_BLOCK) {
    mt->selected_filter=-1;
    return FALSE;
  }

  set_cursor_style(mt,LIVES_CURSOR_NORMAL,0,0,0,0,0);

  if (mt->is_rendering||mainw->playing_file>-1||mt->selected_filter==-1) {
    mt->selected_filter=-1;
    return FALSE;
  }

  window=gdk_display_get_window_at_pointer (mt->display,&win_x,&win_y);

  if (cfile->achans>0&&mt->opts.back_audio_tracks>0&&GPOINTER_TO_INT(g_object_get_data(G_OBJECT(mt->audio_draws->data),"hidden"))==0) {
    labelbox=(GtkWidget *)g_object_get_data(G_OBJECT(mt->audio_draws->data),"labelbox");
    ahbox=(GtkWidget *)g_object_get_data(G_OBJECT(mt->audio_draws->data),"ahbox");
  
    if (GTK_WIDGET(mt->audio_draws->data)->window==window||labelbox->window==window||ahbox->window==window) {
      if (labelbox->window==window||ahbox->window==window) timesecs=0.;
      else {
	gdk_window_get_pointer(GDK_WINDOW (mt->timeline->window), &mt->sel_x, &mt->sel_y, NULL);
	timesecs=get_time_from_x(mt,mt->sel_x);
      }
      tchan=-1;
      eventbox=(GtkWidget *)mt->audio_draws->data;
    }
  }

  if (tchan==0) {
    tchan=-1;
    for (i=0;i<g_list_length(mt->video_draws);i++) {
      eventbox=(GtkWidget *)g_list_nth_data(mt->video_draws,i);
      if (GPOINTER_TO_INT(g_object_get_data(G_OBJECT(eventbox),"hidden"))!=0) continue;
      labelbox=(GtkWidget *)g_object_get_data(G_OBJECT(eventbox),"labelbox");
      ahbox=(GtkWidget *)g_object_get_data(G_OBJECT(eventbox),"ahbox");
      if (eventbox->window==window||labelbox->window==window||ahbox->window==window) {
	if (labelbox->window==window||ahbox->window==window) timesecs=0.;
	else {
	  gdk_window_get_pointer(GDK_WINDOW (mt->timeline->window), &mt->sel_x, &mt->sel_y, NULL);
	  timesecs=get_time_from_x(mt,mt->sel_x);
	}
	tchan=GPOINTER_TO_INT(g_object_get_data(G_OBJECT(eventbox),"layer_number"));
	break;
      }
    }
    if (tchan==-1) {
      mt->selected_filter=-1;
      return FALSE;
    }
  }

  mt->current_fx=mt->selected_filter;
  mt->selected_filter=-1;

  // create dummy menuitem
  menuitem=gtk_menu_item_new();
  g_object_set_data(G_OBJECT(menuitem),"idx",GINT_TO_POINTER(mt->current_fx));

  switch (enabled_in_channels(get_weed_filter(mt->current_fx),TRUE)) {
  case 1:
    // filter - either we drop on a region or on a block
    if (g_list_length(mt->selected_tracks)==1&&mt->region_start!=mt->region_end) {
      // apply to region
      mt_add_region_effect(GTK_MENU_ITEM(menuitem),mt);
    }
    else {
      track_rect *block;
      if (tchan==-1) {
	gtk_widget_destroy(menuitem);
	return FALSE;
      }
      block=get_block_from_time(eventbox,timesecs,mt);
      unselect_all(mt);
      mt->putative_block=block;
      select_block(mt);
      // apply to block
      mt->putative_block=NULL;
      mt_add_block_effect(GTK_MENU_ITEM(menuitem),mt);
    }
    break;
  case 2:
    // transition
    if (g_list_length(mt->selected_tracks)==2&&mt->region_start!=mt->region_end) {
      // apply to region
      mt_add_region_effect(GTK_MENU_ITEM(menuitem),mt);
    }
    break;
  case 1000000:
    // compositor
    if (mt->selected_tracks!=NULL&&mt->region_start!=mt->region_end) {
      // apply to region
      mt_add_region_effect(NULL,mt);
    }
  }

  gtk_widget_destroy(menuitem);
  return FALSE;
}



static gboolean
filter_ebox_pressed (GtkWidget *eventbox, GdkEventButton *event, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;

  if (mt->is_rendering) return FALSE;

  mt->selected_filter=GPOINTER_TO_INT(g_object_get_data(G_OBJECT(eventbox),"fxid"));

  if (event->type!=GDK_BUTTON_PRESS) {
    // double click
    return FALSE;
  }

  if (mainw->playing_file==-1) {
    // change cursor to mini block
    if (mt->video_draws==NULL&&mt->audio_draws==NULL) {
      return FALSE;
    }
    else {
      set_cursor_style(mt,LIVES_CURSOR_FX_BLOCK,FX_BLOCK_WIDTH,FX_BLOCK_HEIGHT,0,0,FX_BLOCK_HEIGHT/2);
      mt->hotspot_x=mt->hotspot_y=0;
    }
  }

  return FALSE;
}








static void populate_filter_box(GtkWidget *box, gint ninchans, lives_mt *mt) {
  GtkWidget *xeventbox,*vbox,*label;
  gchar *txt;
  gint nfilts=rte_get_numfilters();
  int i,error;
  lives_fx_cat_t cat,subcat;
  gchar *tmp;

  for (i=0;i<nfilts;i++) {
    weed_plant_t *filter=get_weed_filter(i);
    if (filter!=NULL&&!weed_plant_has_leaf(filter,"host_menu_hide")) {

      if (enabled_in_channels(filter,TRUE)==ninchans&&enabled_out_channels(filter,FALSE)==1) {
	if (weed_plant_has_leaf(filter,"plugin_unstable")&&
	    weed_get_boolean_value(filter,"plugin_unstable",&error)==WEED_TRUE) {
	  if (!prefs->unstable_fx) continue;
	  tmp=weed_filter_get_name(i);
	  txt=g_strdup_printf(_("%s [unstable]"),tmp);
	  g_free(tmp);
	}
	else txt=weed_filter_get_name(i);

	cat=weed_filter_categorise(filter,enabled_in_channels(filter,TRUE),enabled_out_channels(filter,FALSE));
	if ((subcat=weed_filter_subcategorise(filter,cat,(cat==5)))!=0) {
	  tmp=g_strdup_printf("%s (%s)",txt,lives_fx_cat_to_text(subcat,FALSE));
	  g_free(txt);
	  txt=tmp;
	}

	xeventbox=gtk_event_box_new();
	g_object_set_data(G_OBJECT(xeventbox),"fxid",GINT_TO_POINTER(i));

	gtk_widget_add_events (xeventbox, GDK_BUTTON_RELEASE_MASK | GDK_BUTTON_PRESS_MASK);
	if (palette->style&STYLE_1) {
	  if (palette->style&STYLE_3) {
	    gtk_widget_modify_bg(xeventbox, GTK_STATE_NORMAL, &palette->normal_back);
	    gtk_widget_modify_bg(xeventbox, GTK_STATE_SELECTED, &palette->menu_and_bars);
	  }
	  else {
	    gtk_widget_modify_bg(xeventbox, GTK_STATE_NORMAL, &palette->menu_and_bars);
	    gtk_widget_modify_bg(xeventbox, GTK_STATE_SELECTED, &palette->normal_back);
	  }
	}

	vbox=gtk_vbox_new(FALSE,0);

	gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);
	gtk_container_add (GTK_CONTAINER (xeventbox), vbox);
	label=gtk_label_new(txt);
	g_free(txt);
	
	if (palette->style&STYLE_1) {
	  gtk_widget_modify_fg (label, GTK_STATE_NORMAL, &palette->info_text);
	  gtk_widget_modify_fg (label, GTK_STATE_SELECTED, &palette->info_text);
	}
	gtk_container_set_border_width (GTK_CONTAINER (xeventbox), 5);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

	// pack a/v transitions first
	if (get_transition_param(filter)==-1||!has_video_chans_in(filter,FALSE)) 
	  gtk_box_pack_end (GTK_BOX (box), xeventbox, FALSE, FALSE, 0);
	else gtk_box_pack_start (GTK_BOX (box), xeventbox, FALSE, FALSE, 0);
	
	g_signal_connect (GTK_OBJECT (xeventbox), "button_press_event",
			  G_CALLBACK (filter_ebox_pressed),
			  (gpointer)mt);
	g_signal_connect (GTK_OBJECT (xeventbox), "button_release_event",
			  G_CALLBACK (on_drag_filter_end),
			  (gpointer)mt);
      }
    }
  }
}




gboolean mt_selblock (GtkAccelGroup *group, GObject *obj, guint keyval, GdkModifierType mod, gpointer user_data) {
  // ctrl-Enter - select block at current time/track
  lives_mt *mt=(lives_mt *)user_data;
  GtkWidget *eventbox;
  gdouble timesecs=GTK_RULER (mt->timeline)->position;

  unselect_all(mt);

  if (mt->current_track==-1) eventbox=(GtkWidget *)mt->audio_draws->data;
  else eventbox=(GtkWidget *)g_list_nth_data(mt->video_draws,mt->current_track);

  mt->putative_block=get_block_from_time(eventbox,timesecs,mt);
  select_block((lives_mt *)user_data);
  return TRUE;
}


void mt_center_on_cursor (GtkMenuItem *menuitem, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  mt_zoom(mt,1.);
}

void mt_zoom_in (GtkMenuItem *menuitem, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  mt_zoom(mt,0.5);
}

void mt_zoom_out (GtkMenuItem *menuitem, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  mt_zoom(mt,2.);
}


static void
paned_pos (GtkWidget *paned, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  gtk_widget_queue_draw (mt->timeline_table);
}

static void
hpaned_pos (GtkWidget *paned, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  gtk_widget_queue_draw (mt->hbox);
}



void
mt_spin_start_value_changed           (GtkSpinButton   *spinbutton,
				       gpointer         user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  gboolean has_region=(mt->region_start!=mt->region_end);

  g_signal_handler_block(mt->spinbutton_start,mt->spin_start_func);
  mt->region_start=q_dbl(gtk_spin_button_get_value(spinbutton),mt->fps)/U_SEC;
  gtk_spin_button_set_value(spinbutton,mt->region_start);
  gtk_spin_button_set_range (GTK_SPIN_BUTTON (mt->spinbutton_end),mt->region_start+1./mt->fps,mt->end_secs);
  gtk_widget_queue_draw(mt->timeline_reg);
  gdk_window_process_updates(mt->timeline_reg->window,FALSE);
  draw_region(mt);
  do_sel_context(mt);

  if ((((mt->region_start!=mt->region_end&&!has_region)||(mt->region_start==mt->region_end&&has_region)))&&mt->event_list!=NULL&&get_first_event(mt->event_list)!=NULL) {
  if (mt->selected_tracks!=NULL) {
      gtk_widget_set_sensitive(mt->split_sel,TRUE);
      if (mt->region_start!=mt->region_end) {
	gtk_widget_set_sensitive(mt->playsel,TRUE);
	gtk_widget_set_sensitive(mt->ins_gap_sel,TRUE);
	gtk_widget_set_sensitive (mt->remove_first_gaps, TRUE);
	gtk_widget_set_sensitive(mt->fx_region,TRUE);
	switch (g_list_length(mt->selected_tracks)) {
	case 1:
	  gtk_widget_set_sensitive(mt->fx_region_1,TRUE);
	  break;
	case 2:
	  gtk_widget_set_sensitive(mt->fx_region_2,TRUE);
	  break;
	default:
	  break;
	}
      }
      // update labels
      if (get_poly_state_from_page(mt)==POLY_TRANS||get_poly_state_from_page(mt)==POLY_COMP) {
	//polymorph(mt,POLY_NONE);
	polymorph(mt,get_poly_state_from_page(mt));
      }
    }
  }

  g_signal_handler_unblock(mt->spinbutton_start,mt->spin_start_func);
}



void
mt_spin_end_value_changed           (GtkSpinButton   *spinbutton,
				     gpointer         user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  gboolean has_region=(mt->region_start!=mt->region_end);

  g_signal_handler_block(mt->spinbutton_end,mt->spin_end_func);

  mt->region_end=q_dbl(gtk_spin_button_get_value(spinbutton),mt->fps)/U_SEC;
  gtk_spin_button_set_value(spinbutton,mt->region_end);
  gtk_spin_button_set_range (GTK_SPIN_BUTTON (mt->spinbutton_start),0.,mt->region_end-1./mt->fps);
  gtk_widget_queue_draw(mt->timeline_reg);
  gdk_window_process_updates(mt->timeline_reg->window,FALSE);
  draw_region(mt);
  do_sel_context(mt);

  if ((((mt->region_start!=mt->region_end&&!has_region)||(mt->region_start==mt->region_end&&has_region)))&&mt->event_list!=NULL&&get_first_event(mt->event_list)!=NULL) {
    if (mt->selected_tracks!=NULL) {
      gtk_widget_set_sensitive(mt->split_sel,TRUE);
      if (mt->region_start!=mt->region_end) {
	gtk_widget_set_sensitive(mt->playsel,TRUE);
	gtk_widget_set_sensitive(mt->ins_gap_sel,TRUE);
	gtk_widget_set_sensitive (mt->remove_gaps, TRUE);
	gtk_widget_set_sensitive (mt->remove_first_gaps, TRUE);
	gtk_widget_set_sensitive(mt->fx_region,TRUE);
	switch (g_list_length(mt->selected_tracks)) {
	case 1:
	  gtk_widget_set_sensitive(mt->fx_region_1,TRUE);
	  break;
	case 2:
	  gtk_widget_set_sensitive(mt->fx_region_2,TRUE);
	  break;
	default:
	  break;
	}
      }
      // update labels
      if (get_poly_state_from_page(mt)==POLY_TRANS||get_poly_state_from_page(mt)==POLY_COMP) {
	polymorph(mt,POLY_NONE);
	polymorph(mt,get_poly_state_from_page(mt));
      }
    }
  }

  g_signal_handler_unblock(mt->spinbutton_end,mt->spin_end_func);
}


static gboolean in_out_ebox_pressed (GtkWidget *eventbox, GdkEventButton *event, gpointer user_data) {

  int height;
  gdouble width;
  gint ebwidth;
  file *sfile;
  gint file;
  lives_mt *mt=(lives_mt *)user_data;

  if (mt->block_selected!=NULL) return FALSE;

  ebwidth=GTK_WIDGET(mt->timeline)->allocation.width;
  file=mt_file_from_clip(mt,mt->clip_selected);
  sfile=mainw->files[file];

  // change cursor to block
  if (mt->video_draws==NULL&&mt->audio_draws==NULL) {
    return FALSE;
  }
  else {
    if (sfile->frames>0) {
      if (!mt->opts.ign_ins_sel) {
	width=(sfile->end-sfile->start+1.)/sfile->fps;
      }
      else {
	width=sfile->frames/sfile->fps;
      }
    }
    else width=sfile->laudio_time;
    if (width==0) return FALSE;
    width=width/(mt->tl_max-mt->tl_min)*(gdouble)ebwidth;
    if (width>ebwidth) width=ebwidth;
    if (width<2) width=2;
    height=get_track_height(mt);
    gdk_window_set_cursor (eventbox->window, NULL);
    set_cursor_style(mt,LIVES_CURSOR_BLOCK,width,height,file,0,height/2);
    mt->hotspot_x=mt->hotspot_y=0;
  }

  
  return FALSE;

}




static void do_clip_context (lives_mt *mt, GdkEventButton *event, file *sfile) {
  // pop up a context menu when clip is right clicked on

  // unfinished...

  GtkWidget *edit_start_end,*edit_clipedit,*close_clip,*show_clipinfo;
  GtkWidget *menu=gtk_menu_new();

  gtk_menu_set_title (GTK_MENU(menu),_("LiVES: Selected clip"));

  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(menu, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }

  if (sfile->frames>0) {
    edit_start_end = gtk_menu_item_new_with_mnemonic (_("_Adjust start and end points"));
    g_signal_connect (GTK_OBJECT (edit_start_end), "activate",
		      G_CALLBACK (edit_start_end_cb),
		      (gpointer)mt);
    
    gtk_container_add (GTK_CONTAINER (menu), edit_start_end);

  }

  edit_clipedit = gtk_menu_item_new_with_mnemonic (_("_Edit/encode in clip editor"));
  g_signal_connect (GTK_OBJECT (edit_clipedit), "activate",
		    G_CALLBACK (multitrack_end_cb),
		    (gpointer)mt);

  gtk_container_add (GTK_CONTAINER (menu), edit_clipedit);

  show_clipinfo = gtk_menu_item_new_with_mnemonic (_("_Show clip information"));
  g_signal_connect (GTK_OBJECT (show_clipinfo), "activate",
		    G_CALLBACK (show_clipinfo_cb),
		    (gpointer)mt);

  gtk_container_add (GTK_CONTAINER (menu), show_clipinfo);

  close_clip = gtk_menu_item_new_with_mnemonic (_("_Close this clip"));
  g_signal_connect (GTK_OBJECT (close_clip), "activate",
		    G_CALLBACK (close_clip_cb),
		    (gpointer)mt);

  gtk_container_add (GTK_CONTAINER (menu), close_clip);

  gtk_widget_show_all (menu);
  gtk_menu_popup (GTK_MENU(menu), NULL, NULL, NULL, NULL, event->button, event->time);


}



static gboolean clip_ebox_pressed (GtkWidget *eventbox, GdkEventButton *event, gpointer user_data) {

  int height;
  gdouble width;
  gint ebwidth;
  file *sfile;
  gint file;
  lives_mt *mt=(lives_mt *)user_data;

  if (!mt->is_ready) return FALSE;

  if (event->type!=GDK_BUTTON_PRESS&&!mt->is_rendering) {
    set_cursor_style(mt,LIVES_CURSOR_NORMAL,0,0,0,0,0);
    // double click, open up in clip editor
    if (mainw->playing_file==-1) multitrack_delete(mt,!(prefs->warning_mask&WARN_MASK_EXIT_MT));
    return FALSE;
  }

  mt->clip_selected=get_box_child_index(GTK_BOX(mt->clip_inner_box),eventbox);
  mt_clip_select(mt,FALSE);
  
  ebwidth=GTK_WIDGET(mt->timeline)->allocation.width;
  file=mt_file_from_clip(mt,mt->clip_selected);
  sfile=mainw->files[file];

  if (event->button==3) {
    do_clip_context(mt,event,sfile);
    return FALSE;
  }

  // change cursor to block
  if (mt->video_draws==NULL&&mt->audio_draws==NULL) {
    return FALSE;
  }
  else {
    if (sfile->frames>0) {
      if (!mt->opts.ign_ins_sel) {
	width=(sfile->end-sfile->start+1.)/sfile->fps;
      }
      else {
	width=sfile->frames/sfile->fps;
      }
    }
    else width=sfile->laudio_time;
    if (width==0) return FALSE;
    width=width/(mt->tl_max-mt->tl_min)*(gdouble)ebwidth;
    if (width>ebwidth) width=ebwidth;
    if (width<2) width=2;
    height=get_track_height(mt);
    gdk_window_set_cursor (eventbox->window, NULL);
    set_cursor_style(mt,LIVES_CURSOR_BLOCK,width,height,file,0,height/2);
    mt->hotspot_x=mt->hotspot_y=0;
  }
  
  return FALSE;
}



static gboolean
on_drag_clip_end           (GtkWidget       *widget,
			    GdkEventButton  *event,
			    gpointer         user_data) {
  GdkWindow *window;
  GtkWidget *eventbox;
  GtkWidget *labelbox;
  GtkWidget *ahbox;
  lives_mt *mt=(lives_mt *)user_data;
  int win_x,win_y;
  int i;
  gdouble timesecs,osecs;

  if (mt->is_rendering) return FALSE;

  if (mt->cursor_style!=LIVES_CURSOR_BLOCK) return FALSE;

  osecs=GTK_RULER (mt->timeline)->position;

  set_cursor_style(mt,LIVES_CURSOR_BUSY,0,0,0,0,0);

  window=gdk_display_get_window_at_pointer (mt->display,&win_x,&win_y);

  if (cfile->achans>0&&mt->opts.back_audio_tracks>0&&GPOINTER_TO_INT(g_object_get_data(G_OBJECT(mt->audio_draws->data),"hidden"))==0) {
    labelbox=(GtkWidget *)g_object_get_data(G_OBJECT(mt->audio_draws->data),"labelbox");
    ahbox=(GtkWidget *)g_object_get_data(G_OBJECT(mt->audio_draws->data),"ahbox");
  
    if (GTK_WIDGET(mt->audio_draws->data)->window==window||labelbox->window==window||ahbox->window==window) {

      // insert in backing audio
      if (labelbox->window==window||ahbox->window==window) timesecs=0.;
      else {
	gdk_window_get_pointer(GDK_WINDOW (mt->timeline->window), &mt->sel_x, &mt->sel_y, NULL);
	timesecs=get_time_from_x(mt,mt->sel_x);
      }
      mt->current_track=-1;
      track_select(mt);
      
      if (mainw->playing_file==-1) {
	GTK_RULER (mt->timeline)->position=timesecs;
	if (!mt->is_paused) {
	  if (mt->poly_state==POLY_FX_STACK) {
	    polymorph(mt,POLY_FX_STACK);
	  }
	  mt_show_current_frame(mt, FALSE);
	  if (timesecs>0.) {
	    gtk_widget_set_sensitive (mt->rewind,TRUE);
	    gtk_widget_set_sensitive (mainw->m_rewindbutton, TRUE);
	  }
	}
	gtk_widget_queue_draw (mt->timeline);
      }

      if (mainw->playing_file==-1&&(mainw->files[mt->file_selected]->laudio_time>((mainw->files[mt->file_selected]->start-1.)/mainw->files[mt->file_selected]->fps)||(mainw->files[mt->file_selected]->laudio_time>0.&&mt->opts.ign_ins_sel))) insert_audio_here_cb(NULL,(gpointer)mt);
      set_cursor_style(mt,LIVES_CURSOR_NORMAL,0,0,0,0,0);
      if (mt->is_paused) GTK_RULER (mt->timeline)->position=osecs;
      return FALSE;
    }
  }

  for (i=0;i<g_list_length(mt->video_draws);i++) {
    eventbox=(GtkWidget *)g_list_nth_data(mt->video_draws,i);
    if (GPOINTER_TO_INT(g_object_get_data(G_OBJECT(eventbox),"hidden"))!=0) continue;
    labelbox=(GtkWidget *)g_object_get_data(G_OBJECT(eventbox),"labelbox");
    ahbox=(GtkWidget *)g_object_get_data(G_OBJECT(eventbox),"ahbox");
    if (eventbox->window==window||labelbox->window==window||ahbox->window==window) {
      if (labelbox->window==window||ahbox->window==window) timesecs=0.;
      else {
	gdk_window_get_pointer(GDK_WINDOW (mt->timeline->window), &mt->sel_x, &mt->sel_y, NULL);
	timesecs=get_time_from_x(mt,mt->sel_x);
      }
      mt->current_track=GPOINTER_TO_INT(g_object_get_data(G_OBJECT(eventbox),"layer_number"));

      track_select(mt);

      if (mainw->playing_file==-1) {
	GTK_RULER (mt->timeline)->position=timesecs;
	if (!mt->is_paused) {
	  mt_show_current_frame(mt, FALSE);
	  if (timesecs>0.) {
	    gtk_widget_set_sensitive (mt->rewind,TRUE);
	    gtk_widget_set_sensitive (mainw->m_rewindbutton, TRUE);
	  }
	}
	gtk_widget_queue_draw (mt->timeline);
      }
      if (mainw->playing_file==-1&&mainw->files[mt->file_selected]->frames>0) insert_here_cb(NULL,mt);
      break;
    }
  }

  if (mt->is_paused) GTK_RULER (mt->timeline)->position=osecs;
  set_cursor_style(mt,LIVES_CURSOR_NORMAL,0,0,0,0,0);

  return FALSE;
}


static gboolean
on_clipbox_enter (GtkWidget *widget, GdkEventCrossing *event, gpointer user_data) {
  GdkCursor *cursor;
  lives_mt *mt=(lives_mt *)user_data;
  if (mt->cursor_style!=0) return FALSE;
  cursor=gdk_cursor_new_for_display (mt->display, GDK_HAND2);
  gdk_window_set_cursor (widget->window, cursor);
  return FALSE;
}


void mt_init_start_end_spins(lives_mt *mt) {
  GtkWidget *hbox,*btoolbar;
  GObject *spinbutton_start_adj;
  GObject *spinbutton_end_adj;
  
  hbox = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox);
  
  gtk_box_pack_start (GTK_BOX (mt->top_vbox), hbox, FALSE, FALSE, 6);

  btoolbar=gtk_toolbar_new();
  gtk_box_pack_start (GTK_BOX (hbox), btoolbar, FALSE, FALSE, 20);

  gtk_toolbar_set_show_arrow(GTK_TOOLBAR(btoolbar),FALSE);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(btoolbar, GTK_STATE_NORMAL, &palette->menu_and_bars);
    gtk_widget_modify_bg(btoolbar, GTK_STATE_PRELIGHT, &palette->menu_and_bars);
  }
  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(btoolbar, GTK_STATE_INSENSITIVE, &palette->normal_back);
  }

  gtk_toolbar_set_style (GTK_TOOLBAR (btoolbar), GTK_TOOLBAR_TEXT);


  mt->amixer_button=GTK_WIDGET(gtk_tool_button_new(NULL,_ ("Audio mixer (ctrl-m)")));

  gtk_toolbar_insert(GTK_TOOLBAR(btoolbar),GTK_TOOL_ITEM(mt->amixer_button),-1);

  gtk_widget_add_accelerator (mt->amixer_button, "clicked", mt->accel_group,
                              GDK_m, GDK_CONTROL_MASK,
                              GTK_ACCEL_VISIBLE);

  if (cfile->achans==0||!mt->opts.pertrack_audio) gtk_widget_set_sensitive(mt->amixer_button,FALSE);

  g_signal_connect (GTK_OBJECT (mt->amixer_button), "clicked",
		    G_CALLBACK (amixer_show),
		    (gpointer)mt);

  spinbutton_start_adj = (GObject *)gtk_adjustment_new (0., 0., 0., 1./mt->fps, 1./mt->fps, 0.);
  mt->spinbutton_start = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_start_adj), 1, 3);

  gtk_box_pack_start (GTK_BOX (hbox), mt->spinbutton_start, TRUE, FALSE, MAIN_SPIN_SPACER);
  GTK_WIDGET_SET_FLAGS (mt->spinbutton_start, GTK_CAN_DEFAULT|GTK_CAN_FOCUS);
  gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (mt->spinbutton_start), TRUE);
  gtk_spin_button_set_update_policy (GTK_SPIN_BUTTON (mt->spinbutton_start),GTK_UPDATE_ALWAYS);

  mt->l_sel_arrow = gtk_arrow_new (GTK_ARROW_LEFT, GTK_SHADOW_OUT);
  gtk_box_pack_start (GTK_BOX (hbox), mt->l_sel_arrow, FALSE, FALSE, 0);
  gtk_widget_modify_fg(mt->l_sel_arrow, GTK_STATE_NORMAL, &palette->normal_fore);

  gtk_entry_set_width_chars (GTK_ENTRY (mt->spinbutton_start),12);
  mt->sel_label = gtk_label_new(NULL);


  set_sel_label(mt->sel_label);

  gtk_box_pack_start (GTK_BOX (hbox), mt->sel_label, FALSE, FALSE, 0);
  gtk_label_set_justify (GTK_LABEL (mt->sel_label), GTK_JUSTIFY_LEFT);
  gtk_widget_modify_fg(mt->sel_label, GTK_STATE_NORMAL, &palette->normal_fore);

  mt->r_sel_arrow = gtk_arrow_new (GTK_ARROW_RIGHT, GTK_SHADOW_OUT);
  gtk_box_pack_start (GTK_BOX (hbox), mt->r_sel_arrow, FALSE, FALSE, 3);
  gtk_widget_modify_fg(mt->r_sel_arrow, GTK_STATE_NORMAL, &palette->normal_fore);

  spinbutton_end_adj = (GObject *)gtk_adjustment_new (0., 0., 0., 1./mt->fps, 1./mt->fps, 0.);
  mt->spinbutton_end = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_end_adj), 1, 3);
  gtk_box_pack_start (GTK_BOX (hbox), mt->spinbutton_end, TRUE, FALSE, MAIN_SPIN_SPACER);
  gtk_entry_set_width_chars (GTK_ENTRY (mt->spinbutton_end),12);

  GTK_WIDGET_SET_FLAGS (mt->spinbutton_end, GTK_CAN_DEFAULT|GTK_CAN_FOCUS);
  gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (mt->spinbutton_end), TRUE);
  gtk_spin_button_set_update_policy (GTK_SPIN_BUTTON (mt->spinbutton_end),GTK_UPDATE_ALWAYS);

  if (palette->style&STYLE_1&&palette->style&STYLE_2) {
    gtk_widget_modify_base(mt->spinbutton_start, GTK_STATE_NORMAL, &palette->normal_back);
    gtk_widget_modify_base(mt->spinbutton_start, GTK_STATE_INSENSITIVE, &palette->normal_back);
    gtk_widget_modify_base(mt->spinbutton_end, GTK_STATE_NORMAL, &palette->normal_back);
    gtk_widget_modify_base(mt->spinbutton_end, GTK_STATE_INSENSITIVE, &palette->normal_back);
    gtk_widget_modify_text(mt->spinbutton_start, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_text(mt->spinbutton_start, GTK_STATE_INSENSITIVE, &palette->normal_fore);
    gtk_widget_modify_text(mt->spinbutton_end, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_text(mt->spinbutton_end, GTK_STATE_INSENSITIVE, &palette->normal_fore);
    gtk_widget_modify_fg(mt->sel_label, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  mt->spin_start_func=g_signal_connect_after (GTK_OBJECT (mt->spinbutton_start), "value_changed",
					      G_CALLBACK (mt_spin_start_value_changed),
					      (gpointer)mt);

  mt->spin_end_func=g_signal_connect_after (GTK_OBJECT (mt->spinbutton_end), "value_changed",
					    G_CALLBACK (mt_spin_end_value_changed),
					    (gpointer)mt);
}



void mouse_mode_context(lives_mt *mt) {
  clear_context(mt);
  
  if (mt->opts.mouse_mode==MOUSE_MODE_MOVE) {
    add_context_label (mt,(_("Single click on timeline")));
    add_context_label (mt,(_("to select a frame.")));
    add_context_label (mt,(_("Double click or right click on timeline")));
    add_context_label (mt,(_("to select a block.")));
    add_context_label (mt,(_("Clips can be dragged")));
    add_context_label (mt,(_("onto the timeline.")));
    
    add_context_label (mt,(_("Mouse mode is: Move")));
    add_context_label (mt,(_("clips can be moved around.")));
  }
  else if (mt->opts.mouse_mode==MOUSE_MODE_SELECT) {
    clear_context(mt);
    
    add_context_label (mt,(_("Mouse mode is: Select.")));
    add_context_label (mt,(_("Drag with mouse on timeline")));
    add_context_label (mt,(_("to select tracks and time.")));
  }
}




static void on_insert_mode_changed (GtkMenuItem *menuitem, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;

  if (menuitem==(GtkMenuItem *)mt->ins_normal) {
    set_menu_text(mt->ins_menuitem,_("_Insert mode: Normal"),TRUE);
    mt->opts.insert_mode=INSERT_MODE_NORMAL;
  }

  g_signal_handler_block(mt->ins_normal,mt->ins_normal_func);
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mt->ins_normal),mt->opts.insert_mode==INSERT_MODE_NORMAL);
  g_signal_handler_unblock(mt->ins_normal,mt->ins_normal_func);


}


static void on_mouse_mode_changed (GtkMenuItem *menuitem, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;

  if (menuitem==(GtkMenuItem *)mt->mm_move) {
    set_menu_text(mt->mm_menuitem,_("_Mouse mode: Move"),TRUE);
    mt->opts.mouse_mode=MOUSE_MODE_MOVE;
  }
  else if (menuitem==(GtkMenuItem *)mt->mm_select) {
    set_menu_text(mt->mm_menuitem,_("_Mouse mode: Select"),TRUE);
    mt->opts.mouse_mode=MOUSE_MODE_SELECT;
  }

  mouse_mode_context(mt);

  g_signal_handler_block(mt->mm_move,mt->mm_move_func);
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mt->mm_move),mt->opts.mouse_mode==MOUSE_MODE_MOVE);
  g_signal_handler_unblock(mt->mm_move,mt->mm_move_func);

  g_signal_handler_block(mt->mm_select,mt->mm_select_func);
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mt->mm_select),mt->opts.mouse_mode==MOUSE_MODE_SELECT);
  g_signal_handler_unblock(mt->mm_select,mt->mm_select_func);


}



static void on_grav_mode_changed (GtkMenuItem *menuitem, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;

  if (menuitem==(GtkMenuItem *)mt->grav_normal) {
    gtk_tool_button_set_label(GTK_TOOL_BUTTON(mt->grav_menuitem),_("_Gravity: Normal"));
    mt->opts.grav_mode=GRAV_MODE_NORMAL;
  }
  else if (menuitem==(GtkMenuItem *)mt->grav_left) {
    gtk_tool_button_set_label(GTK_TOOL_BUTTON(mt->grav_menuitem),_("_Gravity: Left"));
    mt->opts.grav_mode=GRAV_MODE_LEFT;
  }

  if (menuitem==(GtkMenuItem *)mt->grav_right) {
    gtk_tool_button_set_label(GTK_TOOL_BUTTON(mt->grav_menuitem),_("_Gravity: Right"));
    mt->opts.grav_mode=GRAV_MODE_RIGHT;
    set_menu_text(mt->remove_first_gaps,_("Close _last gap(s) in selected tracks/time"),TRUE);
  }
  else {
    set_menu_text(mt->remove_first_gaps,_("Close _first gap(s) in selected tracks/time"),TRUE);
  }

  g_signal_handler_block(mt->grav_normal,mt->grav_normal_func);
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mt->grav_normal),mt->opts.grav_mode==GRAV_MODE_NORMAL);
  g_signal_handler_unblock(mt->grav_normal,mt->grav_normal_func);

  g_signal_handler_block(mt->grav_left,mt->grav_left_func);
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mt->grav_left),mt->opts.grav_mode==GRAV_MODE_LEFT);
  g_signal_handler_unblock(mt->grav_left,mt->grav_left_func);

  g_signal_handler_block(mt->grav_right,mt->grav_right_func);
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mt->grav_right),mt->opts.grav_mode==GRAV_MODE_RIGHT);
  g_signal_handler_unblock(mt->grav_right,mt->grav_right_func);



}


static size_t estimate_space(lives_mt *mt, gint undo_type) {
  size_t needed=sizeof(mt_undo);

  switch (undo_type) {
  case MT_UNDO_NONE:
    break;
  default:
    needed+=event_list_get_byte_size(mt,mt->event_list,NULL);
    break;
  }
  return needed;
}

static gchar *get_undo_text(gint action, void *extra) {
  gchar *filtname,*ret;
  int error;

  switch (action) {
  case MT_UNDO_REMOVE_GAPS:
    return g_strdup(_("Close gaps"));
  case MT_UNDO_MOVE_BLOCK:
    return g_strdup(_("Move block"));
  case MT_UNDO_MOVE_AUDIO_BLOCK:
    return g_strdup(_("Move audio block"));
  case MT_UNDO_DELETE_BLOCK:
    return g_strdup(_("Delete block"));
  case MT_UNDO_DELETE_AUDIO_BLOCK:
    return g_strdup(_("Delete audio block"));
  case MT_UNDO_SPLIT_MULTI:
    return g_strdup(_("Split tracks"));
  case MT_UNDO_SPLIT:
    return g_strdup(_("Split block"));
  case MT_UNDO_APPLY_FILTER:
    filtname=weed_get_string_value((weed_plant_t *)extra,"name",&error);
    ret=g_strdup_printf(_("Apply %s"),filtname);
    g_free(filtname);
    return ret;
  case MT_UNDO_DELETE_FILTER:
    filtname=weed_get_string_value((weed_plant_t *)extra,"name",&error);
    ret=g_strdup_printf(_("Delete %s"),filtname);
    g_free(filtname);
    return ret;
  case MT_UNDO_INSERT_BLOCK:
    return g_strdup(_("Insert block"));
  case MT_UNDO_INSERT_GAP:
    return g_strdup(_("Insert gap"));
  case MT_UNDO_INSERT_AUDIO_BLOCK:
    return g_strdup(_("Insert audio block"));
  case MT_UNDO_FILTER_MAP_CHANGE:
    return g_strdup(_("Effect order change"));
  }
  return g_strdup("");
}





static void mt_set_undoable (lives_mt *mt, gint what, void *extra, gboolean sensitive) {
  mt->undoable=sensitive;
  if (what!=MT_UNDO_NONE) {
    gchar *what_safe;
    gchar *text=get_undo_text(what,extra);
    what_safe=g_strdelimit (g_strdup (text),"_",' ');
    g_snprintf(mt->undo_text,32,_ ("_Undo %s"),what_safe);

    g_free (what_safe);
    g_free(text);
  }
  else {
    mt->undoable=FALSE;
    g_snprintf(mt->undo_text,32,"%s",_ ("_Undo"));
  }
  set_menu_text(mt->undo,mt->undo_text,TRUE);

  gtk_widget_set_sensitive (mt->undo,sensitive);

}


static void mt_set_redoable (lives_mt *mt, gint what, void *extra, gboolean sensitive) {
  mt->redoable=sensitive;
  if (what!=MT_UNDO_NONE) {
    gchar *what_safe;
    gchar *text=get_undo_text(what,extra);
    what_safe=g_strdelimit (g_strdup (text),"_",' ');
    g_snprintf(mt->redo_text,32,_ ("_Redo %s"),what_safe);
    g_free (what_safe);
    g_free(text);
  }
  else {
    mt->redoable=FALSE;
    g_snprintf(mt->redo_text,32,"%s",_ ("_Redo"));
  }
  set_menu_text(mt->redo,mt->redo_text,TRUE);

  gtk_widget_set_sensitive (mt->redo,sensitive);

}



gboolean make_backup_space (lives_mt *mt, size_t space_needed) {
  // read thru mt->undos and eliminate that space until we have space_needed
  size_t space_avail=(size_t)(prefs->mt_undo_buf*1024*1024)-mt->undo_buffer_used;
  size_t space_freed=0;
  GList *xundo=mt->undos,*ulist;
  int count=0;
  mt_undo *undo;

  while (xundo!=NULL) {
    count++;
    undo=(mt_undo *)(xundo->data);
    space_freed+=undo->data_len;
    if ((space_avail+space_freed)>=space_needed) {
      memmove(mt->undo_mem,mt->undo_mem+space_freed,mt->undo_buffer_used-space_freed);
      ulist=g_list_copy(g_list_nth(mt->undos,count));
      if (ulist!=NULL) ulist->prev=NULL;
      g_list_free(mt->undos);
      mt->undos=ulist;
      while (ulist!=NULL) {
	ulist->data=(unsigned char *)(ulist->data)-space_freed;
	ulist=ulist->next;
      }
      mt->undo_buffer_used-=space_freed;
      if (mt->undo_offset>g_list_length(mt->undos)) {
	mt->undo_offset=g_list_length(mt->undos);
	mt_set_undoable(mt,MT_UNDO_NONE,NULL,FALSE);
	mt_set_redoable(mt,((mt_undo *)(mt->undos->data))->action,NULL,TRUE);
      }
      return TRUE;
    }
    xundo=xundo->next;
  }
  mt->undo_buffer_used=0;
  g_list_free(mt->undos);
  mt->undos=NULL;
  mt->undo_offset=0;
  mt_set_undoable(mt,MT_UNDO_NONE,NULL,FALSE);
  mt_set_redoable(mt,MT_UNDO_NONE,NULL,FALSE);
  return FALSE;
}


void mt_backup(lives_mt *mt, gint undo_type, weed_timecode_t tc) {
  // backup an operation in the undo/redo list

  size_t space_needed=0;
  mt_undo *undo;
  unsigned char *memblock;

  mt_undo *last_valid_undo;

  mt->did_backup=TRUE;

  mt->changed=mt->auto_changed=TRUE;

  if (mt->undo_mem==NULL) return;

  if (mt->undos!=NULL&&mt->undo_offset!=0) {
    // invalidate redo's - we are backing up, so we can't redo any more
    // invalidate from g_list_length-undo_offset onwards
    if ((g_list_length(mt->undos))==mt->undo_offset) {
      mt->undos=NULL;
      mt->undo_buffer_used=0;
    }
    else {
      int i=0;
      GList *ulist=mt->undos;
      while (i<((int)g_list_length(mt->undos)-1-mt->undo_offset)) {
	ulist=ulist->next;
	i++;
      }
      if (ulist!=NULL) {
	memblock=(unsigned char *)ulist->data;
	last_valid_undo=(mt_undo *)memblock;
	memblock+=last_valid_undo->data_len;
	mt->undo_buffer_used=memblock-mt->undo_mem;
	if (ulist->next!=NULL) {
	  ulist=ulist->next;
	  ulist->prev->next=NULL;
	  g_list_free(ulist);
	}
      }
    }
    mt_set_redoable(mt,MT_UNDO_NONE,NULL,FALSE);
    mt->undo_offset=0;
  }

  undo=(mt_undo *)g_malloc(sizeof(mt_undo));
  undo->action=(lives_mt_undo_t)undo_type;
  undo->extra=NULL;

  switch (undo_type) {
  case MT_UNDO_NONE:
    break;
  case MT_UNDO_APPLY_FILTER:
  case MT_UNDO_DELETE_FILTER:
    undo->extra=get_weed_filter(mt->current_fx);
    break;
  case MT_UNDO_FILTER_MAP_CHANGE:
    undo->tc=tc;
    break;
  default:
    break;
  }

  add_markers(mt,mt->event_list);
  if ((space_needed=estimate_space(mt,undo_type)+sizeof(mt_undo))>
      ((size_t)(prefs->mt_undo_buf*1024*1024)-mt->undo_buffer_used)) {
    if (!make_backup_space(mt,space_needed)) {
      remove_markers(mt->event_list);
      do_mt_backup_space_error(mt,(gint)((space_needed*3)>>20));
      return;
    }
    memblock=(unsigned char *)(mt->undo_mem+mt->undo_buffer_used+sizeof(mt_undo));
  }

  undo->data_len=space_needed;
  memblock=(unsigned char *)(mt->undo_mem+mt->undo_buffer_used+sizeof(mt_undo));
  save_event_list_inner(NULL,0,mt->event_list,&memblock);
  remove_markers(mt->event_list);

  memcpy(mt->undo_mem+mt->undo_buffer_used,undo,sizeof(mt_undo));
  mt->undos=g_list_append(mt->undos,mt->undo_mem+mt->undo_buffer_used);
  mt->undo_buffer_used+=space_needed;
  mt_set_undoable(mt,undo->action,undo->extra,TRUE);
  g_free(undo);

}

void mt_aparam_view_toggled (GtkMenuItem *menuitem, gpointer user_data) {
  gint which=GPOINTER_TO_INT(g_object_get_data(G_OBJECT(menuitem),"pnum"));
  lives_mt *mt=(lives_mt *)user_data;
  int i;

  if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menuitem))) mt->aparam_view_list=g_list_append(mt->aparam_view_list,GINT_TO_POINTER(which));
  else mt->aparam_view_list=g_list_remove(mt->aparam_view_list,GINT_TO_POINTER(which));
  for (i=0;i<g_list_length(mt->audio_draws);i++) {
    gtk_widget_queue_draw((GtkWidget *)g_list_nth_data(mt->audio_draws,i));
  }
}


static void destroy_widget(GtkWidget *widget, gpointer user_data) {
  gtk_widget_destroy(widget);
}



static void add_aparam_menuitems(lives_mt *mt) {
  // add menuitems for avol_fx to the View/Audio parameters submenu
  weed_plant_t *filter;
  lives_rfx_t *rfx;
  int i;
  GtkWidget *menuitem;
  gtk_container_foreach(GTK_CONTAINER(mt->aparam_submenu),destroy_widget,NULL);

  if (mt->avol_fx==-1||mt->audio_draws==NULL) {
    gtk_widget_hide(mt->insa_eventbox);
    gtk_widget_hide(mt->insa_checkbutton);
    gtk_widget_hide(mt->aparam_separator);
    gtk_widget_hide(mt->aparam_menuitem);
    gtk_widget_hide(mt->aparam_submenu);

    gtk_widget_hide(mt->render_aud);
    gtk_widget_hide(mt->normalise_aud);
    gtk_widget_hide(mt->render_vid);
    gtk_widget_hide(mt->render_sep);

    if (mt->aparam_view_list!=NULL) {
      g_list_free(mt->aparam_view_list);
      mt->aparam_view_list=NULL;
    }
    return;
  }
  if (mt->opts.pertrack_audio) {
    gtk_widget_show(mt->insa_eventbox);
    gtk_widget_show(mt->insa_checkbutton);
  }

  gtk_widget_show(mt->render_aud);
  gtk_widget_show(mt->normalise_aud);
  gtk_widget_show(mt->render_vid);
  gtk_widget_show(mt->render_sep);

  //  gtk_widget_show(mt->aparam_separator);
  gtk_widget_show(mt->aparam_menuitem);
  gtk_widget_show(mt->aparam_submenu);

  filter=get_weed_filter(mt->avol_fx);
  rfx=weed_to_rfx(filter,FALSE);
  for (i=0;i<rfx->num_params;i++) {
    if ((rfx->params[i].hidden|HIDDEN_MULTI)==HIDDEN_MULTI&&rfx->params[i].type==LIVES_PARAM_NUM) {
      menuitem = gtk_check_menu_item_new_with_label (rfx->params[i].name);
      if (mt->aparam_view_list!=NULL&&g_list_find(mt->aparam_view_list,GINT_TO_POINTER(i))) gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem),TRUE);
      else gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem),FALSE);
      gtk_container_add (GTK_CONTAINER (mt->aparam_submenu), menuitem);
      gtk_widget_show(menuitem);
      g_object_set_data(G_OBJECT(menuitem),"pnum",GINT_TO_POINTER(i));
      g_signal_connect (GTK_OBJECT (menuitem), "activate",
			G_CALLBACK (mt_aparam_view_toggled),
			(gpointer)mt);
    }
  }
  rfx_free(rfx);
  g_free(rfx);
}


static void apply_avol_filter(lives_mt *mt) {
  // apply audio volume effect from 0 to last frame event
  // since audio off occurs on a frame event this should cover the whole timeline

  weed_plant_t *init_event=mt->avol_init_event,*new_end_event;
  weed_plant_t *deinit_event;
  weed_timecode_t new_tc;
  int error,i;

  if (mt->opts.back_audio_tracks==0&&!mt->opts.pertrack_audio) return;

  new_end_event=get_last_frame_event(mt->event_list);

  if (new_end_event==NULL&&init_event!=NULL) {
    remove_filter_from_event_list(mt->event_list,init_event);
    if (mt->aparam_view_list!=NULL) {
      for (i=0;i<g_list_length(mt->audio_draws);i++) {
	redraw_eventbox(mt,(GtkWidget *)g_list_nth_data(mt->audio_draws,i));
      }
    }
    return;
  }

  if (mt->opts.pertrack_audio) gtk_widget_set_sensitive(mt->prerender_aud,TRUE);

  if (init_event==NULL) {
    gdouble region_start=mt->region_start;
    gdouble region_end=mt->region_end;
    GList *slist=g_list_copy(mt->selected_tracks);
    int current_fx=mt->current_fx;
    weed_plant_t *old_mt_init=mt->init_event;
    gboolean did_backup=mt->did_backup;

    if (!did_backup&&mt->idlefunc>0) {
      g_source_remove(mt->idlefunc);
      mt->idlefunc=0;
    }

    mt->region_start=0.;
    mt->region_end=(get_event_timecode(new_end_event)+U_SEC/mt->fps)/U_SEC;
    if (mt->selected_tracks!=NULL) {
      g_list_free(mt->selected_tracks);
      mt->selected_tracks=NULL;
    }
    if (mt->opts.back_audio_tracks>0) mt->selected_tracks=g_list_append(mt->selected_tracks,GINT_TO_POINTER(-1));
    if (mt->opts.pertrack_audio) {
      for (i=0;i<mt->num_video_tracks;i++) {
	mt->selected_tracks=g_list_append(mt->selected_tracks,GINT_TO_POINTER(i));
      }
    }
    mt->current_fx=mt->avol_fx;

    mt->did_backup=TRUE;
    mt_add_region_effect(NULL,mt);
    mt->avol_init_event=mt->init_event;

    mt->region_start=region_start;
    mt->region_end=region_end;
    g_list_free(mt->selected_tracks);
    mt->selected_tracks=g_list_copy(slist);
    if (slist!=NULL) g_list_free(slist);
    mt->current_fx=current_fx;
    mt->init_event=old_mt_init;

    mt->did_backup=did_backup;

    if (mt->aparam_view_list!=NULL) {
      for (i=0;i<g_list_length(mt->audio_draws);i++) {
	redraw_eventbox(mt,(GtkWidget *)g_list_nth_data(mt->audio_draws,i));
      }
    }

    if (!did_backup) mt->idlefunc=mt_idle_add(mt);

    return;
  }

  // init event is already there - we will move the deinit event to tc==new_end event
  deinit_event=(weed_plant_t *)weed_get_voidptr_value(init_event,"deinit_event",&error);
  new_tc=get_event_timecode(new_end_event);

  move_filter_deinit_event(mt->event_list,new_tc,deinit_event,mt->fps,FALSE);

  if (mt->aparam_view_list!=NULL) {
    for (i=0;i<g_list_length(mt->audio_draws);i++) {
      redraw_eventbox(mt,(GtkWidget *)g_list_nth_data(mt->audio_draws,i));
    }
  }
}


static void set_audio_filter_channel_values(lives_mt *mt) {
  // audio values may have changed
  // we need to reinit the filters if they are being edited
  // for now we just have avol_fx

  // TODO - in future we may have more audio filters

  weed_plant_t *inst;
  gint num_in,num_out;
  weed_plant_t **in_channels,**out_channels;
  int i,error;
  
  add_aparam_menuitems(mt);

  if (mt->current_rfx==NULL||mt->current_fx==-1||mt->current_fx!=mt->avol_fx) return;

  inst=(weed_plant_t *)mt->current_rfx->source;
  if (weed_plant_has_leaf(inst,"in_channels")&&(num_in=weed_leaf_num_elements(inst,"in_channels"))) {
    in_channels=weed_get_plantptr_array(inst,"in_channels",&error);
    for (i=0;i<num_in;i++) {
      weed_set_int_value(in_channels[i],"audio_channels",cfile->achans);
      weed_set_int_value(in_channels[i],"audio_rate",cfile->arate);
    }
  }
  if (weed_plant_has_leaf(inst,"out_channels")&&(num_out=weed_leaf_num_elements(inst,"out_channels"))) {
    out_channels=weed_get_plantptr_array(inst,"out_channels",&error);
    for (i=0;i<num_out;i++) {
      weed_set_int_value(out_channels[i],"audio_channels",cfile->achans);
      weed_set_int_value(out_channels[i],"audio_rate",cfile->arate);
    }
  }
  
  mt->changed=mt->auto_changed=TRUE;

  weed_reinit_effect(inst,TRUE);
  polymorph(mt,POLY_PARAMS);

}



static gchar *mt_set_vals_string(void) {
  gchar sendian[128];

  if (cfile->signed_endian&AFORM_UNSIGNED) g_snprintf(sendian,128,"%s",_("unsigned "));
  else g_snprintf(sendian,128,"%s",_("signed "));
  
  if (cfile->signed_endian&AFORM_BIG_ENDIAN) g_strappend(sendian,128,_("big endian"));
  else g_strappend(sendian,128,_("little endian"));
  
  return g_strdup_printf(_("Multitrack values set to %.3f fps, frame size %d x %d, audio channels %d, audio rate %d, audio sample size %d, %s.\n"),cfile->fps,cfile->hsize,cfile->vsize,cfile->achans,cfile->arate,cfile->asampsize,sendian);
}


static void set_mt_play_sizes(lives_mt *mt, gint width, gint height) {
  if (!mt->opts.show_ctx) {
    mt->play_width=MIN(width,MT_PLAY_WIDTH_EXP);
    mt->play_height=MIN(height,MT_PLAY_HEIGHT_EXP);
    mt->play_window_width=MT_PLAY_WIDTH_EXP;
    mt->play_window_height=MT_PLAY_HEIGHT_EXP;
  }
  else {
    mt->play_width=MIN(width,MT_PLAY_WIDTH_SMALL);
    mt->play_height=MIN(height,MT_PLAY_HEIGHT_SMALL);
    mt->play_window_width=MT_PLAY_WIDTH_SMALL;
    mt->play_window_height=MT_PLAY_HEIGHT_SMALL;
  }
}

static weed_plant_t *load_event_list_inner (lives_mt *mt, int fd, gboolean show_errors, gint *num_events, unsigned char **mem, unsigned char *mem_end) {
  weed_plant_t *event,*eventprev=NULL;
  weed_plant_t *event_list;
  int error;
  gdouble fps=-1;
  gchar *msg;

  if (fd>0||mem!=NULL) event_list=weed_plant_deserialise(fd,mem);
  else event_list=mainw->stored_event_list;

  if (mt!=NULL) mt->layout_set_properties=FALSE;

  if (event_list==NULL||!WEED_PLANT_IS_EVENT_LIST(event_list)) {
    if (show_errors) d_print(_("invalid event list. Failed.\n"));
    return NULL;
  }

  if (show_errors&&(!weed_plant_has_leaf(event_list,"fps")||(fps=weed_get_double_value(event_list,"fps",&error))<1.||
		    fps>FPS_MAX)) {
    d_print(_("event list has invalid fps. Failed.\n"));
    return NULL;
  }
  
  if (weed_plant_has_leaf(event_list,"needs_set")) {
    if (show_errors) {
      gchar *set_needed=weed_get_string_value(event_list,"needs_set",&error);
      gchar *err;
      if (!mainw->was_set||strcmp(set_needed,mainw->set_name)) {
	err=g_strdup_printf(_("\nThis layout requires the set \"%s\"\nIn order to load it you must return to the Clip Editor, \nclose the current set,\nthen load in the new set from the File menu.\n"),set_needed);
	d_print(err);
	do_error_dialog_with_check_transient(err,TRUE,0,GTK_WINDOW(mt->window));
	g_free(err);
	weed_free(set_needed);
	return NULL;
      }
      weed_free(set_needed);
    }
  }
  else if (!show_errors&&mem==NULL) return NULL; // no change needed

  if (event_list==mainw->stored_event_list||(mt!=NULL&&!mt->ignore_load_vals)) {
    if (fps>-1) {
      cfile->fps=cfile->pb_fps=fps;
      if (mt!=NULL) mt->fps=cfile->fps;
      cfile->ratio_fps=check_for_ratio_fps(cfile->fps);
    }

    // check for optional leaves
    if (weed_plant_has_leaf(event_list,"width")) {
      gint width=weed_get_int_value(event_list,"width",&error);
      if (width>0) {
	cfile->hsize=width;
	if (mt!=NULL) mt->layout_set_properties=TRUE;
      }
    }
    
    if (weed_plant_has_leaf(event_list,"height")) {
      gint height=weed_get_int_value(event_list,"height",&error);
      if (height>0) {
	cfile->vsize=height;
	if (mt!=NULL) mt->layout_set_properties=TRUE;
      }
    }
    
    if (weed_plant_has_leaf(event_list,"audio_channels")) {
      gint achans=weed_get_int_value(event_list,"audio_channels",&error);

      if (achans>=0&&mt!=NULL) {
	if (achans>2) {
	  gchar *err=g_strdup_printf(_("\nThis has an invalid number of audio channels (%d) for LiVES.\nIt cannot be loaded.\n"),achans);
	  d_print(err);
	  do_error_dialog_with_check_transient(err,TRUE,0,GTK_WINDOW(mt->window));
	  g_free(err);
	  return NULL;
	}
	cfile->achans=achans;
	if (mt!=NULL) mt->layout_set_properties=TRUE;
      }
    }
    
    if (weed_plant_has_leaf(event_list,"audio_rate")) {
      gint arate=weed_get_int_value(event_list,"audio_rate",&error);
      if (arate>0) {
	cfile->arate=cfile->arps=arate;
	if (mt!=NULL) mt->layout_set_properties=TRUE;
      }
    }
    
    if (weed_plant_has_leaf(event_list,"audio_sample_size")) {
      gint asamps=weed_get_int_value(event_list,"audio_sample_size",&error);
      if (asamps==8||asamps==16) {
	cfile->asampsize=asamps;
	if (mt!=NULL) mt->layout_set_properties=TRUE;
      }
      else if (cfile->achans>0) g_printerr("Layout has invalid sample size %d\n",asamps);
    }
    
    if (weed_plant_has_leaf(event_list,"audio_signed")) {
      gint asigned=weed_get_boolean_value(event_list,"audio_signed",&error);
      if (asigned==WEED_TRUE) {
	if (cfile->signed_endian&AFORM_UNSIGNED) cfile->signed_endian^=AFORM_UNSIGNED;
      }
      else {
	if (!(cfile->signed_endian&AFORM_UNSIGNED)) cfile->signed_endian|=AFORM_UNSIGNED;
      }
      if (mt!=NULL) mt->layout_set_properties=TRUE;
    }
    
    if (weed_plant_has_leaf(event_list,"audio_endian")) {
      gint aendian=weed_get_int_value(event_list,"audio_endian",&error);
      if (aendian==0) {
	if (cfile->signed_endian&AFORM_BIG_ENDIAN) cfile->signed_endian^=AFORM_BIG_ENDIAN;
      }
      else {
	if (!(cfile->signed_endian&AFORM_BIG_ENDIAN)) cfile->signed_endian|=AFORM_BIG_ENDIAN;
      }
      if (mt!=NULL) mt->layout_set_properties=TRUE;
    }
  }
  else {
    if (mt!=NULL) {
      msg=set_values_from_defs(mt,FALSE);
      if (msg!=NULL) {
	if (mt!=NULL) mt->layout_set_properties=TRUE;
	g_free(msg);
      }
      cfile->fps=cfile->pb_fps;
      if (mt!=NULL) mt->fps=cfile->fps;
      cfile->ratio_fps=check_for_ratio_fps(cfile->fps);
    }
  }

  if (event_list==mainw->stored_event_list) return event_list;

  if (weed_plant_has_leaf(event_list,"first")) weed_leaf_delete(event_list,"first");
  if (weed_plant_has_leaf(event_list,"last")) weed_leaf_delete(event_list,"last");

  weed_set_voidptr_value(event_list,"first",NULL);
  weed_set_voidptr_value(event_list,"last",NULL);

  do {
    if (mem!=NULL&&(*mem)>=mem_end) break;
    event=weed_plant_deserialise(fd,mem);
    if (event!=NULL) {
      if (weed_plant_has_leaf(event,"previous")) weed_leaf_delete(event,"previous");
      if (weed_plant_has_leaf(event,"next")) weed_leaf_delete(event,"next");
      if (eventprev!=NULL) weed_set_voidptr_value(eventprev,"next",event);
      weed_set_voidptr_value(event,"previous",eventprev);
      weed_set_voidptr_value(event,"next",NULL);

      if (get_first_event(event_list)==NULL) {
	weed_set_voidptr_value(event_list,"first",event);
      }
      weed_set_voidptr_value(event_list,"last",event);
      weed_add_plant_flags(event,WEED_LEAF_READONLY_PLUGIN);
      eventprev=event;
      if (num_events!=NULL) (*num_events)++;
    }
  } while (event!=NULL);

  weed_add_plant_flags(event_list,WEED_LEAF_READONLY_PLUGIN);
  return event_list;
}


static void on_insa_toggled (GtkToggleButton *tbutton, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  mt->opts.insert_audio=gtk_toggle_button_get_active(tbutton);
  if (prefs->lamp_buttons) {
    if (mt->opts.insert_audio) gtk_widget_modify_bg(GTK_WIDGET(tbutton), GTK_STATE_PRELIGHT, &palette->light_green);
    else gtk_widget_modify_bg(GTK_WIDGET(tbutton), GTK_STATE_PRELIGHT, &palette->dark_red);
  }
}

static void on_snapo_toggled (GtkToggleButton *tbutton, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  mt->opts.snap_over=gtk_toggle_button_get_active(tbutton);
  if (prefs->lamp_buttons) {
    if (mt->opts.snap_over) gtk_widget_modify_bg(GTK_WIDGET(tbutton), GTK_STATE_PRELIGHT, &palette->light_green);
    else gtk_widget_modify_bg(GTK_WIDGET(tbutton), GTK_STATE_PRELIGHT, &palette->dark_red);
  }
}



gchar *set_values_from_defs(lives_mt *mt, gboolean from_prefs) {
  // set various multitrack state flags from either defaults or user preferences

  gchar *retval=NULL;
  gint hsize=cfile->hsize;
  gint vsize=cfile->vsize;
  gint arate=cfile->arate;
  gint achans=cfile->achans;
  gint asamps=cfile->asampsize;
  gint ase=cfile->signed_endian;

  if (mainw->stored_event_list!=NULL) {
    load_event_list_inner(mt,-1,TRUE,NULL,NULL,NULL);
    mt->user_width=cfile->hsize;
    mt->user_height=cfile->vsize;
    cfile->pb_fps=mt->fps=mt->user_fps=cfile->fps;
    cfile->arps=mt->user_arate=cfile->arate;
    mt->user_achans=cfile->achans;
    mt->user_asamps=cfile->asampsize;
    mt->user_signed_endian=cfile->signed_endian;
  }
  else {
    if (!from_prefs) {
      cfile->hsize=mt->user_width;
      cfile->vsize=mt->user_height;
      cfile->pb_fps=cfile->fps=mt->fps=mt->user_fps;
      cfile->arps=cfile->arate=mt->user_arate;
      cfile->achans=mt->user_achans;
      cfile->asampsize=mt->user_asamps;
      cfile->signed_endian=mt->user_signed_endian;
    }
    else {
      mt->user_width=cfile->hsize=prefs->mt_def_width;
      mt->user_height=cfile->vsize=prefs->mt_def_height;
      mt->user_fps=cfile->pb_fps=cfile->fps=mt->fps=prefs->mt_def_fps;
      mt->user_arate=cfile->arate=cfile->arps=prefs->mt_def_arate;
      mt->user_achans=cfile->achans=prefs->mt_def_achans;
      mt->user_asamps=cfile->asampsize=prefs->mt_def_asamps;
      mt->user_signed_endian=cfile->signed_endian=prefs->mt_def_signed_endian;
    }
  }
  cfile->ratio_fps=check_for_ratio_fps(cfile->fps);

  if (cfile->hsize!=hsize||cfile->vsize!=vsize||cfile->arate!=arate||cfile->achans!=achans||
      cfile->asampsize!=asamps||cfile->signed_endian!=ase) {
    retval=mt_set_vals_string();
  }

  if (mt->is_ready) scroll_tracks(mt,0);

  if (cfile->achans==0) {
    mt->avol_fx=-1;
    mt->avol_init_event=NULL;
  }

  set_mt_play_sizes(mt,cfile->hsize,cfile->vsize);

  set_audio_filter_channel_values(mt);

  return retval;
}



void event_list_free_undos(lives_mt *mt) {
  if (mt==NULL) return;

  if (mt->undos!=NULL) g_list_free(mt->undos);
  mt->undos=NULL;
  mt->undo_buffer_used=0;
  mt->undo_offset=0;

  mt_set_undoable(mt,MT_UNDO_NONE,NULL,FALSE);
  mt_set_redoable(mt,MT_UNDO_NONE,NULL,FALSE);
}






void stored_event_list_free_undos(void) {
  if (mainw->stored_layout_undos!=NULL) g_list_free(mainw->stored_layout_undos);
  mainw->stored_layout_undos=NULL;
  if (mainw->sl_undo_mem!=NULL) g_free(mainw->sl_undo_mem);
  mainw->sl_undo_mem=NULL;
  mainw->sl_undo_buffer_used=0;
  mainw->sl_undo_offset=0;
}




void remove_current_from_affected_layouts(lives_mt *mt) {
  // remove from affected layouts map
  if (mainw->affected_layouts_map!=NULL) {
    GList *found=g_list_find_custom(mainw->affected_layouts_map,mainw->cl_string,(GCompareFunc)strcmp);
    if (found!=NULL) {
      g_free(found->data);
      mainw->affected_layouts_map=g_list_delete_link(mainw->affected_layouts_map,found);
    }
  }
  
  if (mainw->affected_layouts_map==NULL) {
    gtk_widget_set_sensitive (mainw->show_layout_errors, FALSE);
    if (mt!=NULL) gtk_widget_set_sensitive (mt->show_layout_errors, FALSE);
  }

  recover_layout_cancelled(NULL,NULL);

  if (mt!=NULL) {
    if (mt->event_list!=NULL) {
      event_list_free(mt->event_list);
      mt->event_list=NULL;
    }
    
    mt_clear_timeline(mt);
  }

  // remove some text
  
  if (mainw->layout_textbuffer!=NULL) {
    GtkTextIter iter1,iter2;
    GList *markmap=mainw->affected_layout_marks;
    while (markmap!=NULL) {
      gtk_text_buffer_get_iter_at_mark(GTK_TEXT_BUFFER(mainw->layout_textbuffer),&iter1,(GtkTextMark *)markmap->data);
      gtk_text_buffer_get_iter_at_mark(GTK_TEXT_BUFFER(mainw->layout_textbuffer),&iter2,(GtkTextMark *)markmap->next->data);
      gtk_text_buffer_delete(GTK_TEXT_BUFFER(mainw->layout_textbuffer),&iter1,&iter2);
      
      gtk_text_buffer_delete_mark(GTK_TEXT_BUFFER(mainw->layout_textbuffer),(GtkTextMark *)markmap->data);
      gtk_text_buffer_delete_mark(GTK_TEXT_BUFFER(mainw->layout_textbuffer),(GtkTextMark *)markmap->next->data);
      markmap=markmap->next->next;
    }
    mainw->affected_layout_marks=NULL;
  }
}









void stored_event_list_free_all(gboolean wiped) {
  int i;

  for (i=0;i<MAX_FILES;i++) {
    if (mainw->files[i]!=NULL) {
      mainw->files[i]->stored_layout_frame=0;
      mainw->files[i]->stored_layout_audio=0.;
      mainw->files[i]->stored_layout_fps=0.;
      mainw->files[i]->stored_layout_idx=-1;
    }
  }

  stored_event_list_free_undos();

  if (mainw->stored_event_list!=NULL) event_list_free(mainw->stored_event_list);
  mainw->stored_event_list=NULL;

  if (wiped) {
    remove_current_from_affected_layouts(NULL);
    mainw->stored_event_list_changed=FALSE;
  }
}



gboolean check_for_layout_del (lives_mt *mt, gboolean exiting) {
  // save or wipe event_list
  gint resp=2;

  if ((mt==NULL||mt->event_list==NULL||get_first_event(mt->event_list)==NULL)&&
      (mainw->stored_event_list==NULL||get_first_event(mainw->stored_event_list)==NULL)) return TRUE;

  if (((mt!=NULL&&(mt->changed||mainw->scrap_file!=-1||mainw->ascrap_file!=-1))||(mainw->stored_event_list!=NULL&&
										  mainw->stored_event_list_changed))) {
    gint type=((mainw->scrap_file==-1&&mainw->ascrap_file==-1)||mt==NULL)?3*(!exiting):4;
    _entryw *cdsw=create_cds_dialog(type);

    do {
      resp=gtk_dialog_run(GTK_DIALOG(cdsw->dialog));
      if (resp==2) {
	// save
	mainw->cancelled=CANCEL_NONE;
	on_save_event_list_activate(NULL,mt);
	if (mainw->cancelled==CANCEL_NONE) {
	  break;
	}
	else mainw->cancelled=CANCEL_NONE;
      }
    } while (resp==2);

    gtk_widget_destroy(cdsw->dialog);
    g_free(cdsw);

    if (resp==0) {
      // cancel
      return FALSE;
    }

    recover_layout_cancelled(NULL,NULL);

    if (resp==1&&!exiting) {
      // wipe
      prefs->ar_layout=FALSE;
      set_pref("ar_layout","");
      memset(prefs->ar_layout_name,0,1);
    }
 
  }


  if (mainw->stored_event_list!=NULL||mainw->sl_undo_mem!=NULL) {
    stored_event_list_free_all(TRUE);
    d_print(_("Layout was wiped.\n"));
  }
  else if (mt!=NULL&&mt->event_list!=NULL&&(exiting||resp==1)) {
    event_list_free(mt->event_list);
    event_list_free_undos(mt);
    mt->event_list=NULL;
    mt_clear_timeline(mt);
    close_scrap_file();
    close_ascrap_file();
    d_print(_("Layout was wiped.\n"));
  }

  return TRUE;
}


static void
on_comp_exp (GtkButton *button, gpointer user_data)
{
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(user_data),!gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(user_data)));


}


void delete_audio_tracks(lives_mt *mt, GList *list, gboolean full) {
  GList *slist=list;
  while (slist!=NULL) {
    delete_audio_track(mt,(GtkWidget *)slist->data,full);
    slist=slist->next;
  }
  g_list_free(list);
}




void mt_quit_activate (GtkMenuItem *menuitem, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;

  if (!check_for_layout_del(mt,FALSE)) return;

  if (mt->idlefunc>0) g_source_remove(mt->idlefunc);
  mt->idlefunc=0;

  on_quit_activate(menuitem,NULL);
}

static void set_mt_title (lives_mt *mt) {
  gchar *wtxt=g_strdup_printf(_("LiVES-%s: Multitrack %dx%d : %d bpp %.3f fps"),LiVES_VERSION,cfile->hsize,cfile->vsize,cfile->bpp,cfile->fps);
  gtk_window_set_title (GTK_WINDOW (mt->window), wtxt);
  g_free(wtxt);
}


static gboolean timecode_string_validate(GtkEntry *entry, lives_mt *mt) {
  const gchar *etext=gtk_entry_get_text(entry);
  gchar **array;
  gint hrs,mins;
  gdouble secs;
  gdouble tl_range,pos;

  if (get_token_count((gchar *)etext,':')!=3) return FALSE;

  array=g_strsplit(etext,":",3);

  if (get_token_count(array[2],'.')!=2) {
    g_strfreev(array);
    return FALSE;
  }

  hrs=atoi(array[0]);
  mins=atoi(array[1]);
  if (mins>59) mins=59;
  secs=g_strtod(array[2],NULL);

  g_strfreev(array);

  secs=secs+mins*60.+hrs*3600.;

  if (secs>mt->end_secs){
    tl_range=mt->tl_max-mt->tl_min;
    set_timeline_end_secs(mt,secs);

    mt->tl_min=secs-tl_range/2;
    mt->tl_max=secs+tl_range/2;

    if (mt->tl_max>mt->end_secs) {
      mt->tl_min-=(mt->tl_max-mt->end_secs);
      mt->tl_max=mt->end_secs;
    }

  }
  
  pos=GTK_RULER (mt->timeline)->position;

  pos=q_dbl(pos,mt->fps)/U_SEC;
  if (pos<0.) pos=0.;

  mt_tl_move(mt,secs-pos);

  if (mt->idlefunc>0) g_source_remove(mt->idlefunc);
  while (g_main_context_iteration(NULL,FALSE));
  if (mt->idlefunc>0) {
    mt->idlefunc=0;
    mt->idlefunc=mt_idle_add(mt);
  }

  pos=GTK_RULER (mt->timeline)->position;

  pos=q_dbl(pos,mt->fps)/U_SEC;
  if (pos<0.) pos=0.;

  time_to_string(mt,pos,TIMECODE_LENGTH);

  return TRUE;
}

static gboolean on_mt_delete_event (GtkWidget *widget, GdkEvent *event, gpointer user_data) {
  mt_quit_activate(NULL,user_data);
  return FALSE;
}


static void cmi_set_inactive(GtkWidget *widget, gpointer data) {
  if (widget==data) return;
  g_object_freeze_notify(G_OBJECT(widget));
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(widget),FALSE);
  g_object_thaw_notify(G_OBJECT(widget));
}

static void mt_set_atrans_effect (GtkMenuItem *menuitem, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  gchar *atrans_hash;

  if (!gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menuitem))) return;
  gtk_container_foreach(GTK_CONTAINER(mt->submenu_atransfx),cmi_set_inactive,menuitem);
  prefs->atrans_fx=GPOINTER_TO_INT(g_object_get_data(G_OBJECT(menuitem),"idx"));

  // set pref
  atrans_hash=make_weed_hashname(prefs->atrans_fx,FALSE);
  set_pref("current_autotrans",atrans_hash);
  g_free(atrans_hash);
}


static void after_timecode_changed(GtkWidget *entry, GtkDirectionType dir, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  gdouble pos;

  if (!timecode_string_validate(GTK_ENTRY(entry),mt)) {
    pos=GTK_RULER (mt->timeline)->position;
    pos=q_dbl(pos,mt->fps)/U_SEC;
    if (pos<0.) pos=0.;
    time_to_string(mt,pos,TIMECODE_LENGTH);
  }
  
}


 lives_mt *multitrack (weed_plant_t *event_list, gint orig_file, gdouble fps) {
  GtkWidget *hseparator;
  GtkWidget *menubar;
  GtkWidget *btoolbar;
  GtkWidget *menu_hbox;
  GtkWidget *menuitem;
  GtkWidget *menuitem2;
  GtkWidget *menuitemsep;
  GtkWidget *menuitem_menu;
  GtkWidget *menuitem_menu2;
  GtkWidget *selcopy_menu;
  GtkWidget *image;
  GtkWidget *separator;
  GtkWidget *full_screen;
  GtkWidget *sticky;
  GtkWidget *about;
  GtkWidget *show_mt_keys;
  GtkWidget *view_mt_details;
  GtkWidget *zoom_in;
  GtkWidget *zoom_out;
  GtkWidget *show_messages;
  GtkWidget *tl_vbox;
  GtkWidget *scrollbar;
  GtkWidget *hbox;
  GtkWidget *hbox2;
  GtkWidget *vbox;
  GtkWidget *view_ctx;
  GtkWidget *eventbox;
  GtkWidget *label;
  GtkWidget *ign_ins_sel;
  GtkWidget *submenu;
  GtkWidget *recent_submenu;
  GtkWidget *vcd_dvd_submenu;
  GtkWidget *vcd_dvd_menu;
#ifdef HAVE_LDVGRAB
  GtkWidget *device_menu;
  GtkWidget *device_submenu;
#endif

#ifdef HAVE_WEBM
  GtkWidget *open_loc_menu;
  GtkWidget *open_loc_submenu;
#endif

  GtkWidget *submenu_menu;
  GtkWidget *submenu_menu2;
  GtkWidget *submenu_menu3;
  GtkWidget *submenu_menu4;
  GtkWidget *submenu_menu5;
  GtkWidget *submenu_menu10;
  GtkWidget *submenu_menu11;
  GtkWidget *submenu_menu12;
  GtkWidget *show_frame_events;
  GtkWidget *frame;
  GtkWidget *ccursor;
  GtkWidget *sep;
  GtkWidget *show_manual;
  GtkWidget *donate;
  GtkWidget *email_author;
  GtkWidget *report_bug;
  GtkWidget *suggest_feature;
  GtkWidget *help_translate;


  GObject *vadjustment;
  GObject *spinbutton_adj;
  gint num_filters;
  int i,error;
  gchar *cname,*tname,*msg;

  gchar buff[32768];

  gint scr_width;

  lives_mt *mt=(lives_mt *)g_malloc(sizeof(lives_mt));
  mt->is_ready=FALSE;
  mt->tl_marks=NULL;

  mt->idlefunc=0; // idle function for auto backup
  mt->auto_back_time=0;

  mt->playing_sel=FALSE;

  mt->render_file=mainw->current_file;

  if (prefs->gui_monitor==0) scr_width=mainw->scr_width;
  else scr_width=mainw->mgeom[prefs->gui_monitor-1].width;

  audcol.blue=audcol.red=16384;
  audcol.green=65535;

  fxcol.red=65535;
  fxcol.green=fxcol.blue=0;

  if (mainw->sl_undo_mem==NULL) {
    mt->undo_mem=(unsigned char *)g_try_malloc(prefs->mt_undo_buf*1024*1024);
    if (mt->undo_mem==NULL) {
      do_mt_undo_mem_error();
    }
    mt->undo_buffer_used=0;
    mt->undos=NULL;
    mt->undo_offset=0;
  }
  else {
    mt->undo_mem=mainw->sl_undo_mem;
    mt->undo_buffer_used=mainw->sl_undo_buffer_used;
    mt->undos=mainw->stored_layout_undos;
    mt->undo_offset=mainw->sl_undo_offset;
  }

  mt->apply_fx_button=NULL;

  mt->cursor_style=LIVES_CURSOR_NORMAL;
  mt->cursor=NULL;

  mt->file_selected=orig_file;

  if (event_list==NULL) mt->changed=FALSE;
  else mt->changed=TRUE;

  mt->auto_changed=mt->changed;

  mt->was_undo_redo=FALSE;

  mt->tl_mouse=FALSE;

  mt->clip_labels=NULL;

  if (mainw->multi_opts.set) {
    mt->opts.move_effects=mainw->multi_opts.move_effects;
    mt->opts.fx_auto_preview=mainw->multi_opts.fx_auto_preview;
    mt->opts.snap_over=mainw->multi_opts.snap_over;
    mt->opts.mouse_mode=mainw->multi_opts.mouse_mode;
    mt->opts.grav_mode=mainw->multi_opts.grav_mode;
    mt->opts.insert_mode=mainw->multi_opts.insert_mode;
    mt->opts.show_audio=mainw->multi_opts.show_audio;
    mt->opts.show_ctx=mainw->multi_opts.show_ctx;
    mt->opts.ign_ins_sel=mainw->multi_opts.ign_ins_sel;
    mt->opts.follow_playback=mainw->multi_opts.follow_playback;
  }
  else {
    mt->opts.move_effects=TRUE;
    mt->opts.fx_auto_preview=TRUE;
    mt->opts.snap_over=FALSE;
    mt->opts.mouse_mode=MOUSE_MODE_MOVE;
    mt->opts.show_audio=TRUE;
    mt->opts.show_ctx=TRUE;
    mt->opts.ign_ins_sel=FALSE;
    mt->opts.follow_playback=FALSE;
    mt->opts.grav_mode=GRAV_MODE_NORMAL;
    mt->opts.insert_mode=INSERT_MODE_NORMAL;
  }

  mt->opts.insert_audio=TRUE;

  mt->opts.pertrack_audio=prefs->mt_pertrack_audio;
  mt->opts.audio_bleedthru=FALSE;
  mt->opts.gang_audio=TRUE;
  mt->opts.back_audio_tracks=1;

  if (force_pertrack_audio) mt->opts.pertrack_audio=TRUE;
  force_pertrack_audio=FALSE;

  mt->tl_fixed_length=0.;
  mt->ptr_time=0.;
  mt->video_draws=NULL;
  mt->block_selected=NULL;
  mt->event_list=event_list;
  mt->accel_group = GTK_ACCEL_GROUP(gtk_accel_group_new ());
  mt->fps=fps;
  mt->hotspot_x=mt->hotspot_y=0;
  mt->redraw_block=FALSE;

  mt->region_start=mt->region_end=0.;
  mt->region_updating=FALSE;
  mt->is_rendering=FALSE;
  mt->pr_audio=FALSE;
  mt->selected_tracks=NULL;
  mt->max_disp_vtracks=MAX_DISP_VTRACKS;
  mt->mt_frame_preview=FALSE;
  mt->current_rfx=NULL;
  mt->current_fx=-1;
  mt->putative_block=NULL;
  mt->specific_event=NULL;

  mt->block_tl_move=FALSE;
  mt->block_node_spin=FALSE;

  mt->is_atrans=FALSE;

  mt->last_fx_type=MT_LAST_FX_NONE;

  mt->display=gdk_display_get_default(); // TODO

  mt->moving_block=FALSE;

  mt->insert_start=mt->insert_end=-1;
  mt->insert_avel=1.;

  mt->selected_init_event=mt->init_event=NULL;

  mt->auto_reloading=FALSE;
  mt->fm_edit_event=NULL;

  mt->nb_label=NULL;

  mt->moving_fx=NULL;
  mt->fx_order=FX_ORD_NONE;

  memset(mt->layout_name,0,1);

  mt->did_backup=FALSE;
  mt->framedraw=NULL;

  mt->audio_draws=NULL;
  mt->audio_vols=mt->audio_vols_back=NULL;
  mt->amixer=NULL;
  mt->ignore_load_vals=FALSE;

  mt->exact_preview=0;

  mt->render_vidp=TRUE;
  mt->render_audp=prefs->render_audio;
  mt->normalise_audp=prefs->normalise_audio;

  mt->context_time=-1.;
  mt->use_context=FALSE;

  mt->sepwin_pixbuf=NULL;

  mt->no_expose=FALSE;

  mt->is_paused=FALSE;

  mt->pb_start_event=NULL;

  mt->aud_track_selected=FALSE;

  mt->has_audio_file=FALSE;

  mt->fx_params_label=NULL;
  mt->fx_box=NULL;

  mt->selected_filter=-1;

  mt->top_track=0;

  if (mainw->fx_candidates[FX_CANDIDATE_AUDIO_VOL].delegate!=-1) {
    // user (or system) has delegated an audio volume filter from the candidates
    mt->avol_fx=GPOINTER_TO_INT(g_list_nth_data(mainw->fx_candidates[FX_CANDIDATE_AUDIO_VOL].list,mainw->fx_candidates[FX_CANDIDATE_AUDIO_VOL].delegate));
  }
  else mt->avol_fx=-1;
  mt->avol_init_event=NULL;

  mt->aparam_view_list=NULL;

  if (prefs->mt_enter_prompt&&rdet!=NULL) {
    mt->user_width=rdet->width;
    mt->user_height=rdet->height;
    mt->user_fps=rdet->fps;
    mt->user_arate=xarate;
    mt->user_achans=xachans;
    mt->user_asamps=xasamps;
    mt->user_signed_endian=xse;
    mt->opts.pertrack_audio=ptaud;
    mt->opts.back_audio_tracks=btaud;
    g_free(rdet->encoder_name);
    g_free(rdet);
    rdet=NULL;
    if (resaudw!=NULL) g_free(resaudw);
    resaudw=NULL;
  }

  if (force_backing_tracks>mt->opts.back_audio_tracks) mt->opts.back_audio_tracks=force_backing_tracks;
  force_backing_tracks=0;

  mt->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  if (palette->style&STYLE_1) {
    if (palette->style&STYLE_3) {
      gtk_widget_modify_bg(mt->window, GTK_STATE_NORMAL, &palette->normal_back);
    }
    else gtk_widget_modify_bg(mt->window, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }

  gtk_drag_dest_set(mt->window,GTK_DEST_DEFAULT_ALL,mainw->target_table,2,
		    (GdkDragAction)(GDK_ACTION_COPY|GDK_ACTION_MOVE|GDK_ACTION_LINK));

  g_signal_connect (GTK_OBJECT (mt->window), "drag-data-received",
		    G_CALLBACK (drag_from_outside),
		    NULL);

  mt->top_vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (mt->window), mt->top_vbox);

  menu_hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (mt->top_vbox), menu_hbox, FALSE, FALSE, 0);

  menubar = gtk_menu_bar_new ();
  gtk_box_pack_start (GTK_BOX (menu_hbox), menubar, TRUE, TRUE, 0);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(menubar, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }


  // File
  menuitem = gtk_menu_item_new_with_mnemonic (_ ("_File"));
  gtk_container_add (GTK_CONTAINER (menubar), menuitem);
  
  menuitem_menu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menuitem_menu);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(menuitem_menu, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }




  mt->open_menu = gtk_menu_item_new_with_mnemonic (_("_Open..."));
  gtk_container_add (GTK_CONTAINER (menuitem_menu), mt->open_menu);

  menuitem_menu2 = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (mt->open_menu), menuitem_menu2);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(menuitem_menu2, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }

  menuitem = gtk_menu_item_new_with_mnemonic (_("_Open File/Directory"));
  gtk_container_add (GTK_CONTAINER (menuitem_menu2), menuitem);

  g_signal_connect (GTK_OBJECT (menuitem), "activate",
		    G_CALLBACK (on_open_activate),
		    NULL);

  menuitem = gtk_menu_item_new_with_mnemonic (_("O_pen File Selection..."));
  gtk_container_add (GTK_CONTAINER (menuitem_menu2), menuitem);

  g_signal_connect (GTK_OBJECT (menuitem), "activate",
		    G_CALLBACK (on_open_sel_activate),
		    NULL);

  if (capable->has_mplayer) {

#ifdef HAVE_WEBM
    open_loc_menu = gtk_menu_item_new_with_mnemonic (_("Open _Location/Stream..."));
    gtk_container_add (GTK_CONTAINER (menuitem_menu2), open_loc_menu);

    open_loc_submenu=gtk_menu_new();
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (open_loc_menu), open_loc_submenu);
    if (palette->style&STYLE_1) {
      gtk_widget_modify_bg(open_loc_submenu, GTK_STATE_NORMAL, &palette->menu_and_bars);
    }

    menuitem = gtk_menu_item_new_with_mnemonic (_("Open _Youtube Clip..."));
    gtk_container_add (GTK_CONTAINER (open_loc_submenu), menuitem);

    g_signal_connect (GTK_OBJECT (menuitem), "activate",
		      G_CALLBACK (on_open_utube_activate),
		      NULL);

    menuitem = gtk_menu_item_new_with_mnemonic (_("Open _Location/Stream..."));
    gtk_container_add (GTK_CONTAINER (open_loc_submenu), menuitem);

#else

    menuitem = gtk_menu_item_new_with_mnemonic (_("Open _Location/Stream..."));
    gtk_container_add (GTK_CONTAINER (menuitem_menu2), menuitem);
    
#endif

    g_signal_connect (GTK_OBJECT (menuitem), "activate",
		      G_CALLBACK (on_open_loc_activate),
		      NULL);
    
    
    
#ifdef ENABLE_DVD_GRAB
    vcd_dvd_menu = gtk_menu_item_new_with_mnemonic (_("Import Selection from _dvd/vcd..."));
    gtk_container_add (GTK_CONTAINER (menuitem_menu2), vcd_dvd_menu);
    vcd_dvd_submenu=gtk_menu_new();
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (vcd_dvd_menu), vcd_dvd_submenu);
    if (palette->style&STYLE_1) {
      gtk_widget_modify_bg(vcd_dvd_submenu, GTK_STATE_NORMAL, &palette->menu_and_bars);
    }
    
    
    menuitem = gtk_menu_item_new_with_mnemonic (_("Import Selection from _dvd"));
    gtk_container_add (GTK_CONTAINER (vcd_dvd_submenu), menuitem);
    
    g_signal_connect (GTK_OBJECT (menuitem), "activate",
		      G_CALLBACK (on_open_vcd_activate),
		      GINT_TO_POINTER (1));
    
    
# endif
    
    menuitem = gtk_menu_item_new_with_mnemonic (_("Import Selection from _vcd"));
    
#ifdef ENABLE_DVD_GRAB
    gtk_container_add (GTK_CONTAINER (vcd_dvd_submenu), menuitem);
#else
    gtk_container_add (GTK_CONTAINER (menuitem_menu2), menuitem);
#endif
    
    g_signal_connect (GTK_OBJECT (menuitem), "activate",
		      G_CALLBACK (on_open_vcd_activate),
		      GINT_TO_POINTER (2));

  }


#ifdef HAVE_LDVGRAB
  device_menu = gtk_menu_item_new_with_mnemonic (_("_Import from Device"));
  gtk_container_add (GTK_CONTAINER (menuitem_menu2), device_menu);
  device_submenu=gtk_menu_new();

  gtk_menu_item_set_submenu (GTK_MENU_ITEM (device_menu), device_submenu);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(device_submenu, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }

  menuitem = gtk_menu_item_new_with_mnemonic (_("Import from _Firewire Device (dv)"));
  gtk_container_add (GTK_CONTAINER (device_submenu), menuitem);


  g_signal_connect (GTK_OBJECT (menuitem), "activate",
		    G_CALLBACK (on_open_fw_activate),
		    GINT_TO_POINTER(CAM_FORMAT_DV));

  menuitem = gtk_menu_item_new_with_mnemonic (_("Import from _Firewire Device (hdv)"));
  gtk_container_add (GTK_CONTAINER (device_submenu), menuitem);

  g_signal_connect (GTK_OBJECT (menuitem), "activate",
		    G_CALLBACK (on_open_fw_activate),
		    GINT_TO_POINTER(CAM_FORMAT_HDV));
#endif


  mt->close = gtk_menu_item_new_with_mnemonic (_("_Close the selected clip"));
  gtk_container_add (GTK_CONTAINER (menuitem_menu), mt->close);
  
  g_signal_connect (GTK_OBJECT (mt->close), "activate",
		    G_CALLBACK (on_close_activate),
		    NULL);

  gtk_widget_set_sensitive(mt->close,FALSE);


  gtk_widget_add_accelerator (mt->close, "activate", mt->accel_group,
                              GDK_w, GDK_CONTROL_MASK,
                              GTK_ACCEL_VISIBLE);

  mt->recent_menu = gtk_menu_item_new_with_mnemonic (_("_Recent Files..."));
  gtk_container_add (GTK_CONTAINER (menuitem_menu), mt->recent_menu);
  recent_submenu=gtk_menu_new();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (mt->recent_menu), recent_submenu);

  memset(buff,0,1);

  get_pref_utf8("recent1",buff,32768);

  mt->recent1 = gtk_menu_item_new_with_label (buff);
  if (strlen (buff)) gtk_widget_show (mt->recent1);

  get_pref_utf8("recent2",buff,32768);

  mt->recent2 = gtk_menu_item_new_with_label (buff);
  if (strlen (buff)) gtk_widget_show (mt->recent2);


  get_pref_utf8("recent3",buff,32768);

  mt->recent3 = gtk_menu_item_new_with_label (buff);
  if (strlen (buff)) gtk_widget_show (mt->recent3);


  get_pref_utf8("recent4",buff,32768);

  mt->recent4 = gtk_menu_item_new_with_label (buff);
  if (strlen (buff)) gtk_widget_show (mt->recent4);

  gtk_container_add (GTK_CONTAINER (recent_submenu), mt->recent1);
  gtk_container_add (GTK_CONTAINER (recent_submenu), mt->recent2);
  gtk_container_add (GTK_CONTAINER (recent_submenu), mt->recent3);
  gtk_container_add (GTK_CONTAINER (recent_submenu), mt->recent4);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(recent_submenu, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }
  
  gtk_widget_show (recent_submenu);


  g_signal_connect (GTK_OBJECT (mt->recent1), "activate",
                      G_CALLBACK (on_recent_activate),
		    GINT_TO_POINTER(1));
  g_signal_connect (GTK_OBJECT (mt->recent2), "activate",
                      G_CALLBACK (on_recent_activate),
		    GINT_TO_POINTER(2));
  g_signal_connect (GTK_OBJECT (mt->recent3), "activate",
                      G_CALLBACK (on_recent_activate),
		    GINT_TO_POINTER(3));
  g_signal_connect (GTK_OBJECT (mt->recent4), "activate",
                      G_CALLBACK (on_recent_activate),
		    GINT_TO_POINTER(4));


  separator = gtk_menu_item_new ();
  gtk_container_add (GTK_CONTAINER (menuitem_menu), separator);
  gtk_widget_set_sensitive (separator, FALSE);

  mt->load_set = gtk_menu_item_new_with_mnemonic (_("_Reload Clip Set..."));
  gtk_widget_show (mt->load_set);
  gtk_container_add (GTK_CONTAINER (menuitem_menu), mt->load_set);

  g_signal_connect (GTK_OBJECT (mt->load_set), "activate",
		    G_CALLBACK (on_load_set_activate),
		    NULL);

  mt->save_set = gtk_menu_item_new_with_mnemonic (_("Close/Sa_ve All Clips"));
  gtk_widget_show (mt->save_set);
  gtk_widget_set_sensitive (mt->save_set, FALSE);
  gtk_container_add (GTK_CONTAINER (menuitem_menu), mt->save_set);

  g_signal_connect (GTK_OBJECT (mt->save_set), "activate",
                      G_CALLBACK (on_quit_activate),
                      GINT_TO_POINTER(1));

  separator = gtk_menu_item_new ();
  gtk_container_add (GTK_CONTAINER (menuitem_menu), separator);
  gtk_widget_set_sensitive (separator, FALSE);

  mt->save_event_list = gtk_image_menu_item_new_with_mnemonic (_("_Save layout as..."));
  gtk_container_add (GTK_CONTAINER (menuitem_menu), mt->save_event_list);
  gtk_widget_set_sensitive (mt->save_event_list, FALSE);

  gtk_widget_add_accelerator (mt->save_event_list, "activate", mt->accel_group,
                              GDK_s, GDK_CONTROL_MASK,
                              GTK_ACCEL_VISIBLE);

  mt->load_event_list = gtk_image_menu_item_new_with_mnemonic (_("_Load layout..."));
  gtk_container_add (GTK_CONTAINER (menuitem_menu), mt->load_event_list);
  gtk_widget_set_sensitive (mt->load_event_list, strlen(mainw->set_name)>0);

  mt->clear_event_list = gtk_image_menu_item_new_with_mnemonic (_("_Wipe/Delete layout..."));
  gtk_container_add (GTK_CONTAINER (menuitem_menu), mt->clear_event_list);

  gtk_widget_add_accelerator (mt->clear_event_list, "activate", mt->accel_group,
                              GDK_d, GDK_CONTROL_MASK,
                              GTK_ACCEL_VISIBLE);

  gtk_widget_set_sensitive(mt->clear_event_list,mt->event_list!=NULL);


  separator = gtk_menu_item_new ();
  gtk_container_add (GTK_CONTAINER (menuitem_menu), separator);
  gtk_widget_set_sensitive (separator, FALSE);

  mt->clear_ds = gtk_menu_item_new_with_mnemonic (_("Clean _up Diskspace"));
  gtk_widget_show (mt->clear_ds);
  gtk_container_add (GTK_CONTAINER (menuitem_menu), mt->clear_ds);

  g_signal_connect (GTK_OBJECT (mt->clear_ds), "activate",
                      G_CALLBACK (on_cleardisk_activate),
                      NULL);

  separator = gtk_menu_item_new ();
  gtk_container_add (GTK_CONTAINER (menuitem_menu), separator);
  gtk_widget_set_sensitive (separator, FALSE);

  mt->load_vals = gtk_check_menu_item_new_with_mnemonic (_("_Ignore width, height and audio values from loaded layouts"));
  gtk_container_add (GTK_CONTAINER (menuitem_menu), mt->load_vals);
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mt->load_vals),mt->ignore_load_vals);

  mt->aload_subs = gtk_check_menu_item_new_with_mnemonic (_("Auto load _subtitles with clips"));
  gtk_container_add (GTK_CONTAINER (menuitem_menu), mt->aload_subs);
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mt->aload_subs),prefs->autoload_subs);

  separator = gtk_menu_item_new ();
  gtk_container_add (GTK_CONTAINER (menuitem_menu), separator);
  gtk_widget_set_sensitive (separator, FALSE);

  mt->quit = gtk_image_menu_item_new_from_stock ("gtk-quit", mt->accel_group);
  gtk_container_add (GTK_CONTAINER (menuitem_menu), mt->quit);

  gtk_widget_add_accelerator (mt->quit, "activate", mt->accel_group,
                              GDK_q, GDK_CONTROL_MASK,
                              GTK_ACCEL_VISIBLE);

  // Edit

  menuitem = gtk_menu_item_new_with_mnemonic (_("_Edit"));
  gtk_container_add (GTK_CONTAINER (menubar), menuitem);

  menuitem_menu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menuitem_menu);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(menuitem_menu, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }

  mt->undo = gtk_image_menu_item_new_with_mnemonic (_("_Undo"));
  gtk_widget_show (mt->undo);
  gtk_container_add (GTK_CONTAINER (menuitem_menu), mt->undo);
  gtk_widget_set_sensitive (mt->undo, FALSE);

  gtk_widget_add_accelerator (mt->undo, "activate", mt->accel_group,
                              GDK_u, GDK_CONTROL_MASK,
                              GTK_ACCEL_VISIBLE);
  
  image = gtk_image_new_from_stock ("gtk-undo", GTK_ICON_SIZE_MENU);
  gtk_widget_show (image);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (mt->undo), image);

  if (mt->undo_offset==g_list_length(mt->undos)) mt_set_undoable(mt,MT_UNDO_NONE,NULL,FALSE);
  else {
    mt_undo *undo=(mt_undo *)(g_list_nth_data(mt->undos,g_list_length(mt->undos)-mt->undo_offset-1));
    mt_set_undoable(mt,undo->action,undo->extra,TRUE);
  }

  g_signal_connect (GTK_OBJECT (mt->undo), "activate",
		    G_CALLBACK (multitrack_undo),
		    (gpointer)mt);

  mt->redo = gtk_image_menu_item_new_with_mnemonic (_("_Redo"));
  gtk_container_add (GTK_CONTAINER (menuitem_menu), mt->redo);
  gtk_widget_set_sensitive (mt->redo, FALSE);

  gtk_widget_add_accelerator (mt->redo, "activate", mt->accel_group,
                              GDK_z, GDK_CONTROL_MASK,
                              GTK_ACCEL_VISIBLE);

  image = gtk_image_new_from_stock ("gtk-redo", GTK_ICON_SIZE_MENU);
  gtk_widget_show (image);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (mt->redo), image);

  if (mt->undo_offset<=1) mt_set_redoable(mt,MT_UNDO_NONE,NULL,FALSE);
  else {
    mt_undo *redo=(mt_undo *)(g_list_nth_data(mt->undos,g_list_length(mt->undos)-mt->undo_offset));
    mt_set_redoable(mt,redo->action,redo->extra,TRUE);
  }

  g_signal_connect (GTK_OBJECT (mt->redo), "activate",
		    G_CALLBACK (multitrack_redo),
		    (gpointer)mt);

  separator = gtk_menu_item_new ();
  gtk_container_add (GTK_CONTAINER (menuitem_menu), separator);
  gtk_widget_set_sensitive (separator, FALSE);


  mt->clipedit = gtk_image_menu_item_new_with_mnemonic (_("_CLIP EDITOR"));
  gtk_container_add (GTK_CONTAINER (menuitem_menu), mt->clipedit);

  gtk_widget_add_accelerator (mt->clipedit, "activate", mt->accel_group,
                              GDK_e, GDK_CONTROL_MASK,
                              GTK_ACCEL_VISIBLE);


  separator = gtk_menu_item_new ();
  gtk_container_add (GTK_CONTAINER (menuitem_menu), separator);
  gtk_widget_set_sensitive (separator, FALSE);

  mt->adjust_start_end = gtk_image_menu_item_new_with_mnemonic (_("_Adjust selected clip start/end points"));
  gtk_container_add (GTK_CONTAINER (menuitem_menu), mt->adjust_start_end);

  gtk_widget_add_accelerator (mt->adjust_start_end, "activate", mt->accel_group,
                              GDK_x, GDK_CONTROL_MASK,
                              GTK_ACCEL_VISIBLE);
  gtk_widget_set_sensitive (mt->adjust_start_end, FALSE);


  mt->insert = gtk_image_menu_item_new_with_mnemonic (_("_Insert selected clip"));
  gtk_container_add (GTK_CONTAINER (menuitem_menu), mt->insert);

  gtk_widget_add_accelerator (mt->insert, "activate", mt->accel_group,
                              GDK_i, (GdkModifierType)0,
                              GTK_ACCEL_VISIBLE);
  gtk_widget_add_accelerator (mt->insert, "activate", mt->accel_group,
                              GDK_i, GDK_CONTROL_MASK,
                              (GtkAccelFlags)0);
  gtk_widget_set_sensitive (mt->insert, FALSE);


  mt->audio_insert = gtk_image_menu_item_new_with_mnemonic (_("_Insert selected clip audio"));
  gtk_container_add (GTK_CONTAINER (menuitem_menu), mt->audio_insert);

  gtk_widget_add_accelerator (mt->audio_insert, "activate", mt->accel_group,
                              GDK_i, GDK_CONTROL_MASK,
                              GTK_ACCEL_VISIBLE);
  gtk_widget_set_sensitive (mt->audio_insert, FALSE);


  mt->delblock = gtk_image_menu_item_new_with_mnemonic (_("_Delete selected block"));
  gtk_container_add (GTK_CONTAINER (menuitem_menu), mt->delblock);
  gtk_widget_set_sensitive (mt->delblock, FALSE);

  gtk_widget_add_accelerator (mt->delblock, "activate", mt->accel_group,
                              GDK_d, GDK_CONTROL_MASK,
                              GTK_ACCEL_VISIBLE);


  mt->jumpback = gtk_image_menu_item_new_with_mnemonic (_("_Jump to previous block boundary"));
  gtk_container_add (GTK_CONTAINER (menuitem_menu), mt->jumpback);

  gtk_widget_add_accelerator (mt->jumpback, "activate", mt->accel_group,
                              GDK_j, GDK_CONTROL_MASK,
                              GTK_ACCEL_VISIBLE);

  gtk_widget_set_sensitive (mt->jumpback, FALSE);

  mt->jumpnext = gtk_image_menu_item_new_with_mnemonic (_("_Jump to next block boundary"));
  gtk_container_add (GTK_CONTAINER (menuitem_menu), mt->jumpnext);

  gtk_widget_add_accelerator (mt->jumpnext, "activate", mt->accel_group,
                              GDK_l, GDK_CONTROL_MASK,
                              GTK_ACCEL_VISIBLE);

  gtk_widget_set_sensitive (mt->jumpnext, FALSE);

  separator = gtk_menu_item_new ();
  gtk_container_add (GTK_CONTAINER (menuitem_menu), separator);
  gtk_widget_set_sensitive (separator, FALSE);

  mt->clear_marks = gtk_image_menu_item_new_with_mnemonic (_("Clear _marks from timeline"));
  gtk_container_add (GTK_CONTAINER (menuitem_menu), mt->clear_marks);
  gtk_widget_set_sensitive(mt->clear_marks,FALSE);

  separator = gtk_menu_item_new ();
  gtk_container_add (GTK_CONTAINER (menuitem_menu), separator);
  gtk_widget_set_sensitive (separator, FALSE);

  ign_ins_sel = gtk_check_menu_item_new_with_mnemonic (_("Ignore selection limits when inserting"));
  gtk_container_add (GTK_CONTAINER (menuitem_menu), ign_ins_sel);
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(ign_ins_sel),mt->opts.ign_ins_sel);

  // Play

  menuitem = gtk_menu_item_new_with_mnemonic (_ ("_Play"));
  gtk_container_add (GTK_CONTAINER (menubar), menuitem);
  
  menuitem_menu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menuitem_menu);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(menuitem_menu, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }

  mt->playall = gtk_image_menu_item_new_with_mnemonic (_("_Play from Timeline Position"));
  gtk_widget_add_accelerator (mt->playall, "activate", mt->accel_group,
                              GDK_p, (GdkModifierType)0,
                              GTK_ACCEL_VISIBLE);
  gtk_widget_set_sensitive (mt->playall, FALSE);


  gtk_container_add (GTK_CONTAINER (menuitem_menu), mt->playall);

  image = gtk_image_new_from_stock ("gtk-refresh", GTK_ICON_SIZE_MENU);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (mt->playall), image);

  mt->playsel = gtk_image_menu_item_new_with_mnemonic (_("Pla_y selected time only"));
  gtk_widget_add_accelerator (mt->playsel, "activate", mt->accel_group,
                              GDK_y, (GdkModifierType)0,
                              GTK_ACCEL_VISIBLE);
  gtk_container_add (GTK_CONTAINER (menuitem_menu), mt->playsel);
  gtk_widget_set_sensitive (mt->playsel, FALSE);

  mt->stop = gtk_image_menu_item_new_with_mnemonic (_("_Stop"));
  gtk_container_add (GTK_CONTAINER (menuitem_menu), mt->stop);
  gtk_widget_set_sensitive (mt->stop, FALSE);
  gtk_widget_add_accelerator (mt->stop, "activate", mt->accel_group,
                              GDK_q, (GdkModifierType)0,
                              GTK_ACCEL_VISIBLE);

  image = gtk_image_new_from_stock ("gtk-stop", GTK_ICON_SIZE_MENU);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (mt->stop), image);

  mt->rewind = gtk_image_menu_item_new_with_mnemonic (_("Re_wind"));
  gtk_container_add (GTK_CONTAINER (menuitem_menu), mt->rewind);

  image = gtk_image_new_from_stock ("gtk-back", GTK_ICON_SIZE_MENU);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (mt->rewind), image);
  gtk_widget_set_sensitive (mt->rewind, FALSE);

  gtk_widget_add_accelerator (mt->rewind, "activate", mt->accel_group,
                              GDK_w, (GdkModifierType)0,
                              GTK_ACCEL_VISIBLE);

  separator = gtk_menu_item_new ();
  gtk_container_add (GTK_CONTAINER (menuitem_menu), separator);
  gtk_widget_set_sensitive (separator, FALSE);

  full_screen = gtk_check_menu_item_new_with_mnemonic (_("_Full Screen"));
  gtk_container_add (GTK_CONTAINER (menuitem_menu), full_screen);
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(full_screen),mainw->fs);

  gtk_widget_add_accelerator (full_screen, "activate", mt->accel_group,
                              GDK_f, (GdkModifierType)0,
                              GTK_ACCEL_VISIBLE);

  mt->sepwin = gtk_check_menu_item_new_with_mnemonic (_("Play in _Separate Window"));
  gtk_container_add (GTK_CONTAINER (menuitem_menu), mt->sepwin);
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mt->sepwin),mainw->sep_win);

  gtk_widget_add_accelerator (mt->sepwin, "activate", mt->accel_group,
                              GDK_s, (GdkModifierType)0,
                              GTK_ACCEL_VISIBLE);

  mt->loop_continue = gtk_check_menu_item_new_with_mnemonic (_("L_oop Continuously"));
  gtk_container_add (GTK_CONTAINER (menuitem_menu), mt->loop_continue);
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mt->loop_continue),mainw->loop_cont);

  gtk_widget_add_accelerator (mt->loop_continue, "activate", mt->accel_group,
                              GDK_o, (GdkModifierType)0,
                              GTK_ACCEL_VISIBLE);

  mt->mute_audio = gtk_check_menu_item_new_with_mnemonic (_("_Mute"));
  gtk_container_add (GTK_CONTAINER (menuitem_menu), mt->mute_audio);
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mt->mute_audio),mainw->mute);

  gtk_widget_add_accelerator (mt->mute_audio, "activate", mt->accel_group,
                              GDK_z, (GdkModifierType)0,
                              GTK_ACCEL_VISIBLE);


  separator = gtk_menu_item_new ();
  gtk_container_add (GTK_CONTAINER (menuitem_menu), separator);
  gtk_widget_set_sensitive (separator, FALSE);

  sticky = gtk_check_menu_item_new_with_mnemonic (_("Separate Window 'S_ticky' Mode"));

  gtk_widget_add_accelerator (sticky, "activate", mt->accel_group,
                              GDK_t, (GdkModifierType)0,
                              GTK_ACCEL_VISIBLE);

  gtk_container_add (GTK_CONTAINER (menuitem_menu), sticky);

  if (capable->smog_version_correct&&prefs->sepwin_type==1) {
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(sticky),TRUE);
  }


  // Effects

  menuitem = gtk_menu_item_new_with_mnemonic (_ ("Effect_s"));
  gtk_container_add (GTK_CONTAINER (menubar), menuitem);
  
  menuitem_menu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menuitem_menu);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(menuitem_menu, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }

  mt->move_fx = gtk_check_menu_item_new_with_mnemonic (_("_Move effects with blocks"));
  gtk_container_add (GTK_CONTAINER (menuitem_menu), mt->move_fx);
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mt->move_fx),mt->opts.move_effects);

  g_signal_connect_after (GTK_OBJECT (mt->move_fx), "toggled",
			  G_CALLBACK (on_move_fx_changed),
			  (gpointer)mt);

  separator = gtk_menu_item_new ();
  gtk_container_add (GTK_CONTAINER (menuitem_menu), separator);
  gtk_widget_set_sensitive (separator, FALSE);


  mt->atrans_menuitem = gtk_menu_item_new_with_mnemonic (_("Select _autotransition effect..."));
  gtk_container_add (GTK_CONTAINER (menuitem_menu), mt->atrans_menuitem);

  mt->submenu_atransfx=gtk_menu_new();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (mt->atrans_menuitem), mt->submenu_atransfx);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(mt->submenu_atransfx, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }

  separator = gtk_menu_item_new ();
  gtk_container_add (GTK_CONTAINER (menuitem_menu), separator);
  gtk_widget_set_sensitive (separator, FALSE);

  mt->fx_edit = gtk_menu_item_new_with_mnemonic (_("View/_Edit selected effect"));
  gtk_container_add (GTK_CONTAINER (menuitem_menu), mt->fx_edit);
  gtk_widget_set_sensitive(mt->fx_edit,FALSE);

  mt->fx_delete = gtk_menu_item_new_with_mnemonic (_("_Delete selected effect"));
  gtk_container_add (GTK_CONTAINER (menuitem_menu), mt->fx_delete);
  gtk_widget_set_sensitive(mt->fx_delete,FALSE);

  separator = gtk_menu_item_new ();
  gtk_container_add (GTK_CONTAINER (menuitem_menu), separator);
  gtk_widget_set_sensitive (separator, FALSE);

  mt->fx_block = gtk_menu_item_new_with_mnemonic (_("Apply effect to _block..."));
  gtk_container_add (GTK_CONTAINER (menuitem_menu), mt->fx_block);
  gtk_widget_set_sensitive(mt->fx_block,FALSE);

  submenu_menu=gtk_menu_new();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (mt->fx_block), submenu_menu);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(submenu_menu, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }

  mt->fx_region = gtk_menu_item_new_with_mnemonic (_("Apply effect to _region..."));
  gtk_container_add (GTK_CONTAINER (menuitem_menu), mt->fx_region);
  gtk_widget_set_sensitive(mt->fx_region,FALSE);

  submenu_menu2=gtk_menu_new();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (mt->fx_region), submenu_menu2);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(submenu_menu2, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }

  tname=lives_fx_cat_to_text(LIVES_FX_CAT_EFFECT,TRUE); // effects
  cname=g_strdup_printf("_%s...",tname);
  g_free(tname);

  mt->fx_region_1 = gtk_menu_item_new_with_mnemonic (cname);
  g_free(cname);
  gtk_container_add (GTK_CONTAINER (submenu_menu2), mt->fx_region_1);
  gtk_widget_set_sensitive(mt->fx_region_1,FALSE);

  submenu_menu3=gtk_menu_new();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (mt->fx_region_1), submenu_menu3);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(submenu_menu3, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }

  tname=lives_fx_cat_to_text(LIVES_FX_CAT_TRANSITION,TRUE); // transitions
  cname=g_strdup_printf("_%s...",tname);
  g_free(tname);

  mt->fx_region_2 = gtk_menu_item_new_with_mnemonic (cname);
  g_free(cname);
  gtk_container_add (GTK_CONTAINER (submenu_menu2), mt->fx_region_2);

  submenu_menu4=gtk_menu_new();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (mt->fx_region_2), submenu_menu4);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(submenu_menu4, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }


  tname=lives_fx_cat_to_text(LIVES_FX_CAT_AV_TRANSITION,TRUE); //audio/video transitions
  cname=g_strdup_printf("_%s...",tname);
  g_free(tname);

  mt->fx_region_2av = gtk_menu_item_new_with_mnemonic (cname);
  g_free(cname);
  gtk_container_add (GTK_CONTAINER (submenu_menu4), mt->fx_region_2av);

  submenu_menu10=gtk_menu_new();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (mt->fx_region_2av), submenu_menu10);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(submenu_menu10, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }


  tname=lives_fx_cat_to_text(LIVES_FX_CAT_VIDEO_TRANSITION,TRUE); //video only transitions
  cname=g_strdup_printf("_%s...",tname);
  g_free(tname);

  mt->fx_region_2v = gtk_menu_item_new_with_mnemonic (cname);
  g_free(cname);
  gtk_container_add (GTK_CONTAINER (submenu_menu4), mt->fx_region_2v);

  submenu_menu11=gtk_menu_new();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (mt->fx_region_2v), submenu_menu11);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(submenu_menu11, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }


  tname=lives_fx_cat_to_text(LIVES_FX_CAT_AUDIO_TRANSITION,TRUE); //audio only transitions
  cname=g_strdup_printf("_%s...",tname);
  g_free(tname);

  mt->fx_region_2a = gtk_menu_item_new_with_mnemonic (cname);
  g_free(cname);
  gtk_container_add (GTK_CONTAINER (submenu_menu4), mt->fx_region_2a);

  if (!mt->opts.pertrack_audio) gtk_widget_set_sensitive(mt->fx_region_2a,FALSE);

  submenu_menu12=gtk_menu_new();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (mt->fx_region_2a), submenu_menu12);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(submenu_menu12, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }




  tname=lives_fx_cat_to_text(LIVES_FX_CAT_COMPOSITOR,TRUE); // compositors
  cname=g_strdup_printf("_%s...",tname);
  g_free(tname);

  mt->fx_region_3 = gtk_menu_item_new_with_mnemonic (cname);
  g_free(cname);
  gtk_container_add (GTK_CONTAINER (submenu_menu2), mt->fx_region_3);

  submenu_menu5=gtk_menu_new();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (mt->fx_region_3), submenu_menu5);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(submenu_menu5, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }

  num_filters=rte_get_numfilters();
  for (i=0;i<num_filters;i++) {
    weed_plant_t *filter=get_weed_filter(i);
    if (filter!=NULL&&!weed_plant_has_leaf(filter,"host_menu_hide")) {
      GtkWidget *menuitem;
      gchar *fname=weed_filter_get_name(i),*fxname;
      if (weed_plant_has_leaf(filter,"plugin_unstable")&&
	  weed_get_boolean_value(filter,"plugin_unstable",&error)==WEED_TRUE) {
	if (!prefs->unstable_fx) {
	  weed_free(fname);
	  continue;
	}
	fxname=g_strdup_printf(_("%s [unstable]"),fname);
      }
      else fxname=g_strdup(fname);

      if (enabled_in_channels(filter,TRUE)>2&&enabled_out_channels(filter,FALSE)==1) {
	// add all compositor effects to submenus
	menuitem = gtk_image_menu_item_new_with_label (fxname);
	gtk_container_add (GTK_CONTAINER (submenu_menu), menuitem);
	g_object_set_data(G_OBJECT(menuitem),"idx",GINT_TO_POINTER(i));
	g_signal_connect (GTK_OBJECT (menuitem), "activate",
			  G_CALLBACK (mt_add_block_effect),
			  (gpointer)mt);
	menuitem = gtk_image_menu_item_new_with_label (fxname);
	gtk_container_add (GTK_CONTAINER (submenu_menu5), menuitem);
	g_object_set_data(G_OBJECT(menuitem),"idx",GINT_TO_POINTER(i));
	g_signal_connect (GTK_OBJECT (menuitem), "activate",
			  G_CALLBACK (mt_add_region_effect),
			  (gpointer)mt);
      }
      else if (enabled_in_channels(filter,FALSE)==1&&enabled_out_channels(filter,FALSE)==1) {
	// add all filter effects to submenus
	menuitem = gtk_image_menu_item_new_with_label (fxname);
	gtk_container_add (GTK_CONTAINER (submenu_menu), menuitem);
	g_object_set_data(G_OBJECT(menuitem),"idx",GINT_TO_POINTER(i));
	g_signal_connect (GTK_OBJECT (menuitem), "activate",
			  G_CALLBACK (mt_add_block_effect),
			  (gpointer)mt);
	menuitem = gtk_image_menu_item_new_with_label (fxname);
	gtk_container_add (GTK_CONTAINER (submenu_menu3), menuitem);
	g_object_set_data(G_OBJECT(menuitem),"idx",GINT_TO_POINTER(i));
	g_signal_connect (GTK_OBJECT (menuitem), "activate",
			  G_CALLBACK (mt_add_region_effect),
			  (gpointer)mt);
      }
      else if (enabled_in_channels(filter,FALSE)==2&&enabled_out_channels(filter,FALSE)==1) {
	// add all transitions to submenus
	menuitem = gtk_image_menu_item_new_with_label (fxname);
	g_object_set_data(G_OBJECT(menuitem),"idx",GINT_TO_POINTER(i));
	if (get_transition_param(filter)==-1) gtk_container_add (GTK_CONTAINER (submenu_menu11), menuitem);
	else {
	  if (has_video_chans_in(filter,FALSE)) {
	    /// the autotransitions menu
	    menuitem2 = gtk_check_menu_item_new_with_label (fxname);
	    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem2),prefs->atrans_fx==i);
	    g_object_set_data(G_OBJECT(menuitem2),"idx",GINT_TO_POINTER(i));

	    g_signal_connect (GTK_OBJECT (menuitem2), "activate",
			      G_CALLBACK (mt_set_atrans_effect),
			      (gpointer)mt);


	    if (!strcmp(fname,prefs->def_autotrans)) {
	      gtk_menu_shell_prepend(GTK_MENU_SHELL(mt->submenu_atransfx),menuitem2);
	    }
	    else gtk_menu_shell_append(GTK_MENU_SHELL(mt->submenu_atransfx),menuitem2);
	    /// apply block effect menu
	    gtk_container_add (GTK_CONTAINER (submenu_menu10), menuitem);
	  }
	  else gtk_container_add (GTK_CONTAINER (submenu_menu12), menuitem);
	}
	g_signal_connect (GTK_OBJECT (menuitem), "activate",
			  G_CALLBACK (mt_add_region_effect),
			  (gpointer)mt);
      }
      g_free(fname);
      g_free(fxname);
    }
  }

  /// None autotransition
  menuitem2 = gtk_check_menu_item_new_with_label (mainw->none_string);
  g_object_set_data(G_OBJECT(menuitem2),"idx",GINT_TO_POINTER(-1));
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem2),prefs->atrans_fx==-1);
  gtk_menu_shell_prepend(GTK_MENU_SHELL(mt->submenu_atransfx),menuitem2);

  g_signal_connect (GTK_OBJECT (menuitem2), "activate",
		    G_CALLBACK (mt_set_atrans_effect),
		    (gpointer)mt);


  // Tracks

  menuitem = gtk_menu_item_new_with_mnemonic (_ ("_Tracks"));
  gtk_container_add (GTK_CONTAINER (menubar), menuitem);
  
  menuitem_menu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menuitem_menu);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(menuitem_menu, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }

  mt->cback_audio = gtk_image_menu_item_new_with_mnemonic (_("Make _Backing Audio current track"));
  gtk_container_add (GTK_CONTAINER (menuitem_menu), mt->cback_audio);

  gtk_widget_add_accelerator (mt->cback_audio, "activate", mt->accel_group,
                              GDK_b, GDK_CONTROL_MASK,
                              GTK_ACCEL_VISIBLE);

  separator = gtk_menu_item_new ();
  gtk_container_add (GTK_CONTAINER (menuitem_menu), separator);
  gtk_widget_set_sensitive (separator, FALSE);

  mt->add_vid_behind = gtk_image_menu_item_new_with_mnemonic (_("Add Video Track at _Rear"));
  gtk_container_add (GTK_CONTAINER (menuitem_menu), mt->add_vid_behind);

  gtk_widget_add_accelerator (mt->add_vid_behind, "activate", mt->accel_group,
                              GDK_t, GDK_CONTROL_MASK,
                              GTK_ACCEL_VISIBLE);


  mt->add_vid_front = gtk_image_menu_item_new_with_mnemonic (_("Add Video Track at _Front"));
  gtk_container_add (GTK_CONTAINER (menuitem_menu), mt->add_vid_front);

  gtk_widget_add_accelerator (mt->add_vid_front, "activate", mt->accel_group,
                              GDK_t, (GdkModifierType)(GDK_CONTROL_MASK|GDK_SHIFT_MASK),
                              GTK_ACCEL_VISIBLE);


  separator = gtk_menu_item_new ();
  gtk_container_add (GTK_CONTAINER (menuitem_menu), separator);
  gtk_widget_set_sensitive (separator, FALSE);


  menuitem = gtk_menu_item_new_with_mnemonic (_("_Split current track at cursor"));
  gtk_container_add (GTK_CONTAINER (menuitem_menu), menuitem);

  g_signal_connect (GTK_OBJECT (menuitem), "activate",
		    G_CALLBACK (on_split_curr_activate),
		    (gpointer)mt);

  gtk_widget_add_accelerator (menuitem, "activate", mt->accel_group,
                              GDK_s, (GdkModifierType)GDK_CONTROL_MASK,
                              GTK_ACCEL_VISIBLE);


  mt->split_sel = gtk_menu_item_new_with_mnemonic (_("_Split selected video tracks"));
  gtk_container_add (GTK_CONTAINER (menuitem_menu), mt->split_sel);
  gtk_widget_set_sensitive (mt->split_sel, FALSE);

  g_signal_connect (GTK_OBJECT (mt->split_sel), "activate",
		    G_CALLBACK (on_split_sel_activate),
		    (gpointer)mt);


  separator = gtk_menu_item_new ();
  gtk_container_add (GTK_CONTAINER (menuitem_menu), separator);
  gtk_widget_set_sensitive (separator, FALSE);

  mt->ins_gap_sel = gtk_image_menu_item_new_with_mnemonic (_("Insert gap in selected tracks/time"));
  gtk_container_add (GTK_CONTAINER (menuitem_menu), mt->ins_gap_sel);
  gtk_widget_set_sensitive (mt->ins_gap_sel, FALSE);

  g_signal_connect (GTK_OBJECT (mt->ins_gap_sel), "activate",
		    G_CALLBACK (on_insgap_sel_activate),
		    (gpointer)mt);

  mt->ins_gap_cur = gtk_image_menu_item_new_with_mnemonic (_("Insert gap in current track/selected time"));
  gtk_container_add (GTK_CONTAINER (menuitem_menu), mt->ins_gap_cur);
  gtk_widget_set_sensitive (mt->ins_gap_cur, FALSE);

  g_signal_connect (GTK_OBJECT (mt->ins_gap_cur), "activate",
		    G_CALLBACK (on_insgap_cur_activate),
		    (gpointer)mt);

  separator = gtk_menu_item_new ();
  gtk_container_add (GTK_CONTAINER (menuitem_menu), separator);
  gtk_widget_set_sensitive (separator, FALSE);

  mt->remove_gaps = gtk_menu_item_new_with_mnemonic (_("Close all _gaps in selected tracks/time"));
  gtk_container_add (GTK_CONTAINER (menuitem_menu), mt->remove_gaps);

  g_signal_connect (GTK_OBJECT (mt->remove_gaps), "activate",
		    G_CALLBACK (remove_gaps),
		    (gpointer)mt);

  gtk_widget_add_accelerator (mt->remove_gaps, "activate", mt->accel_group,
                              GDK_g, GDK_CONTROL_MASK,
                              GTK_ACCEL_VISIBLE);

  mt->remove_first_gaps = gtk_menu_item_new_with_mnemonic ("");
  gtk_container_add (GTK_CONTAINER (menuitem_menu), mt->remove_first_gaps);

  g_signal_connect (GTK_OBJECT (mt->remove_first_gaps), "activate",
		    G_CALLBACK (remove_first_gaps),
		    (gpointer)mt);

  gtk_widget_add_accelerator (mt->remove_first_gaps, "activate", mt->accel_group,
                              GDK_f, GDK_CONTROL_MASK,
                              GTK_ACCEL_VISIBLE);



  // Selection

  menuitem = gtk_menu_item_new_with_mnemonic (_ ("Se_lection"));
  gtk_container_add (GTK_CONTAINER (menubar), menuitem);

  menuitem_menu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menuitem_menu);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(menuitem_menu, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }

  mt->select_track = gtk_check_menu_item_new_with_mnemonic (_("_Select Current Track"));
  gtk_container_add (GTK_CONTAINER (menuitem_menu), mt->select_track);

  gtk_widget_add_accelerator (mt->select_track, "activate", mt->accel_group,
                              GDK_space, GDK_CONTROL_MASK,
                              GTK_ACCEL_VISIBLE);

  menuitem = gtk_menu_item_new_with_mnemonic (_("Select _all video tracks"));
  gtk_container_add (GTK_CONTAINER (menuitem_menu), menuitem);

  g_signal_connect (GTK_OBJECT (menuitem), "activate",
		    G_CALLBACK (select_all_vid),
		    (gpointer)mt);

  menuitem = gtk_menu_item_new_with_mnemonic (_("Select _no video tracks"));
  gtk_container_add (GTK_CONTAINER (menuitem_menu), menuitem);

  g_signal_connect (GTK_OBJECT (menuitem), "activate",
		    G_CALLBACK (select_no_vid),
		    (gpointer)mt);

  menuitem = gtk_menu_item_new_with_mnemonic (_("Select all _time"));
  gtk_container_add (GTK_CONTAINER (menuitem_menu), menuitem);

  g_signal_connect (GTK_OBJECT (menuitem), "activate",
		    G_CALLBACK (select_all_time),
		    (gpointer)mt);

  gtk_widget_add_accelerator (menuitem, "activate", mt->accel_group,
                              GDK_a, GDK_CONTROL_MASK,
                              GTK_ACCEL_VISIBLE);

  menuitem = gtk_menu_item_new_with_mnemonic (_("Select from _zero time"));
  gtk_container_add (GTK_CONTAINER (menuitem_menu), menuitem);

  g_signal_connect (GTK_OBJECT (menuitem), "activate",
		    G_CALLBACK (select_from_zero_time),
		    (gpointer)mt);

  menuitem = gtk_menu_item_new_with_mnemonic (_("Select to _end time"));
  gtk_container_add (GTK_CONTAINER (menuitem_menu), menuitem);

  g_signal_connect (GTK_OBJECT (menuitem), "activate",
		    G_CALLBACK (select_to_end_time),
		    (gpointer)mt);

  menuitem = gtk_menu_item_new_with_mnemonic (_("_Copy..."));
  gtk_container_add (GTK_CONTAINER (menuitem_menu), menuitem);

  selcopy_menu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), selcopy_menu);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(selcopy_menu, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }

  mt->tc_to_rs = gtk_menu_item_new_with_mnemonic (_("_Timecode to region start"));
  gtk_container_add (GTK_CONTAINER (selcopy_menu), mt->tc_to_rs);

  g_signal_connect (GTK_OBJECT (mt->tc_to_rs), "activate",
		    G_CALLBACK (tc_to_rs),
		    (gpointer)mt);

  mt->tc_to_re = gtk_menu_item_new_with_mnemonic (_("_Timecode to region end"));
  gtk_container_add (GTK_CONTAINER (selcopy_menu), mt->tc_to_re);

  g_signal_connect (GTK_OBJECT (mt->tc_to_re), "activate",
		    G_CALLBACK (tc_to_re),
		    (gpointer)mt);

  mt->rs_to_tc = gtk_menu_item_new_with_mnemonic (_("_Region start to timecode"));
  gtk_container_add (GTK_CONTAINER (selcopy_menu), mt->rs_to_tc);

  g_signal_connect (GTK_OBJECT (mt->rs_to_tc), "activate",
		    G_CALLBACK (rs_to_tc),
		    (gpointer)mt);

  mt->re_to_tc = gtk_menu_item_new_with_mnemonic (_("_Region end to timecode"));
  gtk_container_add (GTK_CONTAINER (selcopy_menu), mt->re_to_tc);

  g_signal_connect (GTK_OBJECT (mt->re_to_tc), "activate",
		    G_CALLBACK (re_to_tc),
		    (gpointer)mt);

  gtk_widget_set_sensitive(mt->rs_to_tc,FALSE);
  gtk_widget_set_sensitive(mt->re_to_tc,FALSE);


  // Tools

  menuitem = gtk_menu_item_new_with_mnemonic (_ ("_Tools"));
  gtk_container_add (GTK_CONTAINER (menubar), menuitem);
  
  menuitem_menu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menuitem_menu);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(menuitem_menu, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }


  mt->change_vals = gtk_image_menu_item_new_with_mnemonic (_("_Change width, height and audio values..."));
  gtk_widget_show (mt->change_vals);
  gtk_container_add (GTK_CONTAINER (menuitem_menu), mt->change_vals);

  g_signal_connect (GTK_OBJECT (mt->change_vals), "activate",
                      G_CALLBACK (mt_change_vals_activate),
                      (gpointer)mt);


  separator = gtk_menu_item_new ();
  gtk_container_add (GTK_CONTAINER (menuitem_menu), separator);
  gtk_widget_set_sensitive (separator, FALSE);


  mt->gens_submenu = gtk_menu_item_new_with_mnemonic (_("_Generate"));
  gtk_container_add (GTK_CONTAINER (menuitem_menu), mt->gens_submenu);

  gtk_widget_ref(mainw->gens_menu);
  gtk_menu_detach(GTK_MENU(mainw->gens_menu));

  gtk_menu_item_set_submenu (GTK_MENU_ITEM (mt->gens_submenu), mainw->gens_menu);


  mt->capture = gtk_menu_item_new_with_mnemonic (_("Capture _External Window... "));
  gtk_widget_show (mt->capture);
  gtk_container_add (GTK_CONTAINER (menuitem_menu), mt->capture);


  g_signal_connect (GTK_OBJECT (mt->capture), "activate",
                      G_CALLBACK (on_capture_activate),
                      NULL);


  separator = gtk_menu_item_new ();
  gtk_container_add (GTK_CONTAINER (menuitem_menu), separator);
  gtk_widget_set_sensitive (separator, FALSE);


  menuitem = gtk_image_menu_item_new_with_mnemonic (_("_Preferences..."));
  gtk_widget_show (menuitem);
  gtk_container_add (GTK_CONTAINER (menuitem_menu), menuitem);
  gtk_widget_add_accelerator (menuitem, "activate", mt->accel_group,
                              GDK_p, GDK_CONTROL_MASK,
                              GTK_ACCEL_VISIBLE);

  image = gtk_image_new_from_stock ("gtk-preferences", GTK_ICON_SIZE_MENU);
  gtk_widget_show (image);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), image);

  g_signal_connect (GTK_OBJECT (menuitem), "activate",
                      G_CALLBACK (on_preferences_activate),
                      NULL);



  // Render

  menuitem = gtk_menu_item_new_with_mnemonic (_ ("_Render"));
  gtk_container_add (GTK_CONTAINER (menubar), menuitem);
  
  menuitem_menu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menuitem_menu);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(menuitem_menu, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }

  mt->render = gtk_image_menu_item_new_with_mnemonic (_("_Render all to new clip"));
  gtk_container_add (GTK_CONTAINER (menuitem_menu), mt->render);
  gtk_widget_set_sensitive (mt->render, FALSE);

  gtk_widget_add_accelerator (mt->render, "activate", mt->accel_group,
                              GDK_r, GDK_CONTROL_MASK,
                              GTK_ACCEL_VISIBLE);

  // TODO - render selected time


  mt->render_sep = gtk_menu_item_new ();
  gtk_container_add (GTK_CONTAINER (menuitem_menu), mt->render_sep);
  gtk_widget_set_sensitive (mt->render_sep, FALSE);

  mt->render_vid = gtk_check_menu_item_new_with_mnemonic (_("Render _video"));
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mt->render_vid), TRUE);

  gtk_container_add (GTK_CONTAINER (menuitem_menu), mt->render_vid);

  mt->render_aud = gtk_check_menu_item_new_with_mnemonic (_("Render _audio"));
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mt->render_aud), mt->render_audp);

  gtk_container_add (GTK_CONTAINER (menuitem_menu), mt->render_aud);

  sep = gtk_menu_item_new ();

  gtk_container_add (GTK_CONTAINER (menuitem_menu), sep);
  gtk_widget_set_sensitive (sep, FALSE);

  mt->normalise_aud = gtk_check_menu_item_new_with_mnemonic (_("_Normalise rendered audio"));
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mt->normalise_aud), mt->normalise_audp);

  gtk_container_add (GTK_CONTAINER (menuitem_menu), mt->normalise_aud);



  mt->prerender_aud = gtk_menu_item_new_with_mnemonic (_("_Pre-render audio"));
  gtk_widget_set_sensitive(mt->prerender_aud, FALSE);

  //gtk_container_add (GTK_CONTAINER (menuitem_menu), mt->prerender_aud);



  // View

  menuitem = gtk_menu_item_new_with_mnemonic (_ ("_View"));
  gtk_container_add (GTK_CONTAINER (menubar), menuitem);
  
  menuitem_menu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menuitem_menu);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(menuitem_menu, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }


  mt->view_clips = gtk_menu_item_new_with_mnemonic (_("_Clips"));
  gtk_container_add (GTK_CONTAINER (menuitem_menu), mt->view_clips);

  gtk_widget_add_accelerator (mt->view_clips, "activate", mt->accel_group,
                              GDK_c, (GdkModifierType)0,
                              GTK_ACCEL_VISIBLE);

  mt->view_in_out = gtk_menu_item_new_with_mnemonic (_("Block _In/out points"));
  gtk_container_add (GTK_CONTAINER (menuitem_menu), mt->view_in_out);

  gtk_widget_add_accelerator (mt->view_in_out, "activate", mt->accel_group,
                              GDK_n, (GdkModifierType)0,
                              GTK_ACCEL_VISIBLE);

  gtk_widget_set_sensitive(mt->view_in_out,FALSE);

  mt->view_effects = gtk_menu_item_new_with_mnemonic (_("_Effects at current"));
  gtk_container_add (GTK_CONTAINER (menuitem_menu), mt->view_effects);

  gtk_widget_add_accelerator (mt->view_effects, "activate", mt->accel_group,
                              GDK_e, (GdkModifierType)0,
                              GTK_ACCEL_VISIBLE);

  show_messages = gtk_image_menu_item_new_with_mnemonic (_("Show _Messages"));
  gtk_container_add (GTK_CONTAINER (menuitem_menu), show_messages);

  separator = gtk_menu_item_new ();
  gtk_container_add (GTK_CONTAINER (menuitem_menu), separator);
  gtk_widget_set_sensitive (separator, FALSE);

  mt->aparam_separator = gtk_menu_item_new ();
  gtk_container_add (GTK_CONTAINER (menuitem_menu), mt->aparam_separator);
  gtk_widget_set_sensitive (mt->aparam_separator, FALSE);

  mt->aparam_menuitem = gtk_menu_item_new_with_mnemonic (_("Audio parameters"));
  gtk_container_add (GTK_CONTAINER (menuitem_menu), mt->aparam_menuitem);

  mt->aparam_submenu=gtk_menu_new();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (mt->aparam_menuitem), mt->aparam_submenu);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(mt->aparam_submenu, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }

  mt->view_audio = gtk_check_menu_item_new_with_mnemonic (_("Show backing _audio track"));
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mt->view_audio), mt->opts.show_audio);
  gtk_container_add (GTK_CONTAINER (menuitem_menu), mt->view_audio);

  view_ctx = gtk_check_menu_item_new_with_mnemonic (_("Compact view"));
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(view_ctx), mt->opts.show_ctx);
  gtk_container_add (GTK_CONTAINER (menuitem_menu), view_ctx);

  gtk_widget_add_accelerator (view_ctx, "activate", mt->accel_group,
                              GDK_d, (GdkModifierType)0,
                              GTK_ACCEL_VISIBLE);

  mt->change_max_disp = gtk_menu_item_new_with_mnemonic (_("Maximum tracks to display..."));
  gtk_container_add (GTK_CONTAINER (menuitem_menu), mt->change_max_disp);

  separator = gtk_menu_item_new ();
  gtk_container_add (GTK_CONTAINER (menuitem_menu), separator);
  gtk_widget_set_sensitive (separator, FALSE);

  mt->follow_play = gtk_check_menu_item_new_with_mnemonic (_("Scroll to follow playback"));
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mt->follow_play), mt->opts.follow_playback);
  gtk_container_add (GTK_CONTAINER (menuitem_menu), mt->follow_play);

  ccursor = gtk_menu_item_new_with_mnemonic (_("_Center on cursor"));
  gtk_container_add (GTK_CONTAINER (menuitem_menu), ccursor);

  gtk_widget_add_accelerator (ccursor, "activate", mt->accel_group,
                              GDK_c, (GdkModifierType)GDK_CONTROL_MASK,
                              GTK_ACCEL_VISIBLE);

  zoom_in = gtk_menu_item_new_with_mnemonic (_("_Zoom in"));
  gtk_container_add (GTK_CONTAINER (menuitem_menu), zoom_in);

  gtk_widget_add_accelerator (zoom_in, "activate", mt->accel_group,
                              GDK_plus, (GdkModifierType)GDK_CONTROL_MASK,
                              GTK_ACCEL_VISIBLE);

  gtk_widget_add_accelerator (zoom_in, "activate", mt->accel_group,
                              GDK_equal, (GdkModifierType)GDK_CONTROL_MASK,
                              (GtkAccelFlags)0);

  zoom_out = gtk_menu_item_new_with_mnemonic (_("_Zoom out"));
  gtk_container_add (GTK_CONTAINER (menuitem_menu), zoom_out);

  gtk_widget_add_accelerator (zoom_out, "activate", mt->accel_group,
                              GDK_minus, (GdkModifierType)GDK_CONTROL_MASK,
                              GTK_ACCEL_VISIBLE);


  separator = gtk_menu_item_new ();
  gtk_container_add (GTK_CONTAINER (menuitem_menu), separator);
  gtk_widget_set_sensitive (separator, FALSE);

  view_mt_details = gtk_menu_item_new_with_mnemonic (_("Multitrack _details"));
  gtk_container_add (GTK_CONTAINER (menuitem_menu), view_mt_details);


  mt->show_layout_errors = gtk_image_menu_item_new_with_mnemonic (_("Show _Layout Errors"));
  gtk_widget_show (mt->show_layout_errors);
  gtk_container_add (GTK_CONTAINER (menuitem_menu), mt->show_layout_errors);
  gtk_widget_set_sensitive (mt->show_layout_errors, mainw->affected_layouts_map!=NULL);

  separator = gtk_menu_item_new ();
  gtk_container_add (GTK_CONTAINER (menuitem_menu), separator);
  gtk_widget_set_sensitive (separator, FALSE);

  mt->view_events = gtk_image_menu_item_new_with_mnemonic (_("_Event Window"));
  gtk_container_add (GTK_CONTAINER (menuitem_menu), mt->view_events);
  gtk_widget_set_sensitive (mt->view_events, FALSE);

  mt->view_sel_events = gtk_image_menu_item_new_with_mnemonic (_("_Event Window (selected time only)"));
  gtk_container_add (GTK_CONTAINER (menuitem_menu), mt->view_sel_events);
  gtk_widget_set_sensitive (mt->view_sel_events, FALSE);

  show_frame_events = gtk_check_menu_item_new_with_mnemonic (_("_Show FRAME events"));
  gtk_container_add (GTK_CONTAINER (menuitem_menu), show_frame_events);
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(show_frame_events),prefs->event_window_show_frame_events);

  // help
  menuitem = gtk_menu_item_new_with_mnemonic (_("_Help"));
  gtk_widget_show (menuitem);
  gtk_container_add (GTK_CONTAINER (menubar), menuitem);

  menuitem_menu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menuitem_menu);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(menuitem_menu, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }

  show_mt_keys = gtk_menu_item_new_with_mnemonic (_("_Show multitrack keys"));
  gtk_widget_show (show_mt_keys);
  gtk_container_add (GTK_CONTAINER (menuitem_menu), show_mt_keys);

  separator = gtk_menu_item_new ();
  gtk_container_add (GTK_CONTAINER (menuitem_menu), separator);
  gtk_widget_set_sensitive (separator, FALSE);

  show_manual = gtk_menu_item_new_with_mnemonic (_("_Manual (opens in browser)"));
  gtk_widget_show (show_manual);
  gtk_container_add (GTK_CONTAINER (menuitem_menu), show_manual);

  separator = gtk_menu_item_new ();
  gtk_widget_show (separator);
  gtk_container_add (GTK_CONTAINER (menuitem_menu), separator);
  gtk_widget_set_sensitive (separator, FALSE);

  donate = gtk_menu_item_new_with_mnemonic (_("_Donate to the project !"));
  gtk_widget_show (donate);
  gtk_container_add (GTK_CONTAINER (menuitem_menu), donate);

  email_author = gtk_menu_item_new_with_mnemonic (_("_Email the author"));
  gtk_widget_show (email_author);
  gtk_container_add (GTK_CONTAINER (menuitem_menu), email_author);

  report_bug = gtk_menu_item_new_with_mnemonic (_("Report a _bug"));
  gtk_widget_show (report_bug);
  gtk_container_add (GTK_CONTAINER (menuitem_menu), report_bug);

  suggest_feature = gtk_menu_item_new_with_mnemonic (_("Suggest a _feature"));
  gtk_widget_show (suggest_feature);
  gtk_container_add (GTK_CONTAINER (menuitem_menu), suggest_feature);

  help_translate = gtk_menu_item_new_with_mnemonic (_("Assist with _translating"));
  gtk_widget_show (help_translate);
  gtk_container_add (GTK_CONTAINER (menuitem_menu), help_translate);

  separator = gtk_separator_menu_item_new ();
  gtk_widget_show (separator);
  gtk_container_add (GTK_CONTAINER (menuitem_menu), separator);
  gtk_widget_set_sensitive (separator, FALSE);

  mt->troubleshoot=gtk_menu_item_new_with_mnemonic (_("_Troubleshoot"));
  gtk_widget_show (mt->troubleshoot);
  gtk_container_add (GTK_CONTAINER (menuitem_menu), mt->troubleshoot);

  about = gtk_menu_item_new_with_mnemonic (_("_About"));
  gtk_widget_show (about);
  gtk_container_add (GTK_CONTAINER (menuitem_menu), about);

  // gtk dont like menu_item_separator in horizontal menus
  menuitemsep = gtk_menu_item_new_with_label("|");
  gtk_widget_set_sensitive(menuitemsep,FALSE);
  gtk_container_add (GTK_CONTAINER(menubar), menuitemsep);



  mt->mm_menuitem = gtk_menu_item_new_with_label ("");

  gtk_container_add (GTK_CONTAINER(menubar), mt->mm_menuitem);

  submenu = gtk_menu_new ();
  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(submenu, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (mt->mm_menuitem), submenu);

  mt->mm_move = gtk_check_menu_item_new_with_mnemonic (_("Mouse mode: _Move"));
  gtk_container_add (GTK_CONTAINER(submenu), mt->mm_move);
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mt->mm_move),mt->opts.mouse_mode==MOUSE_MODE_MOVE);

  mt->mm_move_func=g_signal_connect (GTK_OBJECT (mt->mm_move), "toggled",
				     G_CALLBACK (on_mouse_mode_changed),
				     (gpointer)mt);

  mt->mm_select = gtk_check_menu_item_new_with_mnemonic (_("Mouse mode: _Select"));
  gtk_container_add (GTK_CONTAINER(submenu), mt->mm_select);
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mt->mm_select),mt->opts.mouse_mode==MOUSE_MODE_SELECT);

  mt->mm_select_func=g_signal_connect (GTK_OBJECT (mt->mm_select), "toggled",
				       G_CALLBACK (on_mouse_mode_changed),
				       (gpointer)mt);






  menuitemsep = gtk_menu_item_new_with_label("|");
  gtk_widget_set_sensitive(menuitemsep,FALSE);
  gtk_container_add (GTK_CONTAINER(menubar), menuitemsep);


  mt->ins_menuitem = gtk_menu_item_new_with_label ("");

  gtk_container_add (GTK_CONTAINER(menubar), mt->ins_menuitem);

  submenu = gtk_menu_new ();
  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(submenu, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (mt->ins_menuitem), submenu);

  mt->ins_normal = gtk_check_menu_item_new_with_mnemonic (_("Insert mode: _Normal"));
  gtk_container_add (GTK_CONTAINER(submenu), mt->ins_normal);

  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mt->ins_normal),mt->opts.insert_mode==INSERT_MODE_NORMAL);

  mt->ins_normal_func=g_signal_connect (GTK_OBJECT (mt->ins_normal), "toggled",
				      G_CALLBACK (on_insert_mode_changed),
				      (gpointer)mt);



  g_signal_connect (GTK_OBJECT (mt->quit), "activate",
		    G_CALLBACK (mt_quit_activate),
		    (gpointer)mt);
  g_signal_connect (GTK_OBJECT (mt->load_vals), "activate",
		    G_CALLBACK (mt_load_vals_toggled),
		    (gpointer)mt);
  g_signal_connect (GTK_OBJECT (mt->aload_subs), "activate",
		    G_CALLBACK (on_boolean_toggled),
		    &prefs->autoload_subs);
  g_signal_connect (GTK_OBJECT (mt->clipedit), "activate",
		    G_CALLBACK (multitrack_end_cb),
		    (gpointer)mt);
  g_signal_connect (GTK_OBJECT (mt->playall), "activate",
		    G_CALLBACK (on_playall_activate),
		    NULL);
  g_signal_connect (GTK_OBJECT (mt->playsel), "activate",
		    G_CALLBACK (multitrack_play_sel),
		    (gpointer)mt);
  g_signal_connect (GTK_OBJECT (mt->insert), "activate",
		    G_CALLBACK (multitrack_insert),
		    (gpointer)mt);
  g_signal_connect (GTK_OBJECT (mt->audio_insert), "activate",
		    G_CALLBACK (multitrack_audio_insert),
		    (gpointer)mt);
  g_signal_connect (GTK_OBJECT (mt->adjust_start_end), "activate",
		    G_CALLBACK (multitrack_adj_start_end),
		    (gpointer)mt);
  g_signal_connect (GTK_OBJECT (mt->view_events), "activate",
		    G_CALLBACK (multitrack_view_events),
		    (gpointer)mt);
  g_signal_connect (GTK_OBJECT (mt->view_sel_events), "activate",
		    G_CALLBACK (multitrack_view_sel_events),
		    (gpointer)mt);
  g_signal_connect (GTK_OBJECT (mt->clear_marks), "activate",
		    G_CALLBACK (multitrack_clear_marks),
		    (gpointer)mt);
  g_signal_connect (GTK_OBJECT (view_mt_details), "activate",
		    G_CALLBACK (multitrack_view_details),
		    (gpointer)mt);
  g_signal_connect (GTK_OBJECT (mt->show_layout_errors), "activate",
		    G_CALLBACK (popup_lmap_errors),
		    NULL);
  g_signal_connect (GTK_OBJECT (mt->view_clips), "activate",
		    G_CALLBACK (multitrack_view_clips),
		    (gpointer)mt);
  g_signal_connect (GTK_OBJECT (mt->view_in_out), "activate",
		    G_CALLBACK (multitrack_view_in_out),
		    (gpointer)mt);
  g_signal_connect (GTK_OBJECT (show_messages), "activate",
		    G_CALLBACK (on_show_messages_activate),
		    NULL);
  g_signal_connect (GTK_OBJECT (mt->stop), "activate",
		    G_CALLBACK (on_stop_activate),
		    NULL);
  g_signal_connect (GTK_OBJECT (mt->rewind), "activate",
		    G_CALLBACK (on_rewind_activate),
		    NULL);
  mt->sepwin_func=g_signal_connect (GTK_OBJECT (mt->sepwin), "activate",
				    G_CALLBACK (on_sepwin_activate),
				    NULL);
  g_signal_connect (GTK_OBJECT (full_screen), "activate",
		    G_CALLBACK (on_full_screen_activate),
		    NULL);
  mt->loop_cont_func=g_signal_connect (GTK_OBJECT (mt->loop_continue), "activate",
				       G_CALLBACK (on_loop_cont_activate),
				       NULL);
  mt->mute_audio_func=g_signal_connect (GTK_OBJECT (mt->mute_audio), "activate",
					G_CALLBACK (on_mute_activate),
					NULL);
  g_signal_connect (GTK_OBJECT (sticky), "activate",
		    G_CALLBACK (on_sticky_activate),
		    NULL);
  g_signal_connect (GTK_OBJECT (mt->cback_audio), "activate",
		    G_CALLBACK (on_cback_audio_activate),
		    (gpointer)mt);
  g_signal_connect (GTK_OBJECT (mt->add_vid_behind), "activate",
		    G_CALLBACK (add_video_track_behind),
		    (gpointer)mt);
  g_signal_connect (GTK_OBJECT (mt->add_vid_front), "activate",
		    G_CALLBACK (add_video_track_front),
		    (gpointer)mt);
  g_signal_connect (GTK_OBJECT (mt->render), "activate",
		    G_CALLBACK (on_render_activate),
		    (gpointer)mt);
  g_signal_connect (GTK_OBJECT (mt->prerender_aud), "activate",
		    G_CALLBACK (on_prerender_aud_activate),
		    (gpointer)mt);
  g_signal_connect (GTK_OBJECT (mt->jumpback), "activate",
		    G_CALLBACK (on_jumpback_activate),
		    (gpointer)mt);
  g_signal_connect (GTK_OBJECT (mt->jumpnext), "activate",
		    G_CALLBACK (on_jumpnext_activate),
		    (gpointer)mt);
  g_signal_connect (GTK_OBJECT (mt->delblock), "activate",
		    G_CALLBACK (on_delblock_activate),
		    (gpointer)mt);
  g_signal_connect (GTK_OBJECT (mt->save_event_list), "activate",
		    G_CALLBACK (on_save_event_list_activate),
		    (gpointer)mt);
  g_signal_connect (GTK_OBJECT (mt->load_event_list), "activate",
		    G_CALLBACK (on_load_event_list_activate),
		    (gpointer)mt);
  g_signal_connect (GTK_OBJECT (mt->clear_event_list), "activate",
		    G_CALLBACK (on_clear_event_list_activate),
		    (gpointer)mt);
  g_signal_connect (GTK_OBJECT (mt->view_audio), "activate",
                      G_CALLBACK (mt_view_audio_toggled),
                      (gpointer)mt);
  g_signal_connect (GTK_OBJECT (view_ctx), "activate",
                      G_CALLBACK (mt_view_ctx_toggled),
                      (gpointer)mt);
  g_signal_connect (GTK_OBJECT (mt->change_max_disp), "activate",
                      G_CALLBACK (mt_change_max_disp_tracks),
                      (gpointer)mt);
  g_signal_connect (GTK_OBJECT (mt->render_vid), "activate",
                      G_CALLBACK (mt_render_vid_toggled),
                      (gpointer)mt);
  g_signal_connect (GTK_OBJECT (mt->render_aud), "activate",
                      G_CALLBACK (mt_render_aud_toggled),
                      (gpointer)mt);
  g_signal_connect (GTK_OBJECT (mt->normalise_aud), "activate",
                      G_CALLBACK (mt_norm_aud_toggled),
                      (gpointer)mt);
  g_signal_connect (GTK_OBJECT (ign_ins_sel), "activate",
                      G_CALLBACK (mt_ign_ins_sel_toggled),
                      (gpointer)mt);
  g_signal_connect (GTK_OBJECT (show_frame_events), "activate",
                      G_CALLBACK (show_frame_events_activate),
                      NULL);
  g_signal_connect (GTK_OBJECT (ccursor), "activate",
                      G_CALLBACK (mt_center_on_cursor),
                      (gpointer)mt);
  g_signal_connect (GTK_OBJECT (mt->follow_play), "activate",
                      G_CALLBACK (mt_fplay_toggled),
                      (gpointer)mt);
  g_signal_connect (GTK_OBJECT (zoom_in), "activate",
                      G_CALLBACK (mt_zoom_in),
                      (gpointer)mt);
  g_signal_connect (GTK_OBJECT (zoom_out), "activate",
                      G_CALLBACK (mt_zoom_out),
                      (gpointer)mt);
  mt->seltrack_func=g_signal_connect (GTK_OBJECT (mt->select_track), "activate",
				      G_CALLBACK (on_seltrack_activate),
				      (gpointer)mt);

  g_signal_connect (GTK_OBJECT (show_manual), "activate",
		    G_CALLBACK (show_manual_activate),
		    NULL);

  g_signal_connect (GTK_OBJECT (email_author), "activate",
		    G_CALLBACK (email_author_activate),
		    NULL);

  g_signal_connect (GTK_OBJECT (donate), "activate",
		    G_CALLBACK (donate_activate),
		    NULL);

  g_signal_connect (GTK_OBJECT (report_bug), "activate",
		    G_CALLBACK (report_bug_activate),
		    NULL);

  g_signal_connect (GTK_OBJECT (suggest_feature), "activate",
		    G_CALLBACK (suggest_feature_activate),
		    NULL);

  g_signal_connect (GTK_OBJECT (help_translate), "activate",
		    G_CALLBACK (help_translate_activate),
		    NULL);

  g_signal_connect (GTK_OBJECT (about), "activate",
                      G_CALLBACK (on_about_activate),
                      NULL);

  g_signal_connect (GTK_OBJECT (mt->troubleshoot), "activate",
		    G_CALLBACK (on_troubleshoot_activate),
		    NULL);

  g_signal_connect (GTK_OBJECT (show_mt_keys), "activate",
                      G_CALLBACK (on_mt_showkeys_activate),
                      NULL);
  g_signal_connect (GTK_OBJECT (mt->fx_delete), "activate",
                      G_CALLBACK (on_mt_delfx_activate),
                      (gpointer)mt);
  g_signal_connect (GTK_OBJECT (mt->fx_edit), "activate",
                      G_CALLBACK (on_mt_fx_edit_activate),
                      (gpointer)mt);
  g_signal_connect (GTK_OBJECT (mt->view_effects), "activate",
                      G_CALLBACK (on_mt_list_fx_activate),
                      (gpointer)mt);

  gtk_accel_group_connect (GTK_ACCEL_GROUP (mt->accel_group), GDK_m, (GdkModifierType)0, (GtkAccelFlags)0, 
			   g_cclosure_new (G_CALLBACK (mt_mark_callback),(gpointer)mt,NULL));

  mt->insa_eventbox=gtk_event_box_new();
  mt->insa_checkbutton = gtk_check_button_new ();
  
  // must do this here to set cfile->hsize, cfile->vsize; and we must have created aparam_submenu and insa_eventbox and insa_checkbutton
  msg=set_values_from_defs(mt,!prefs->mt_enter_prompt||(mainw->recoverable_layout&&prefs->startup_interface==STARTUP_CE));
  if (msg!=NULL) g_free(msg);

  eventbox = gtk_event_box_new ();
  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (mt->top_vbox), eventbox, FALSE, FALSE, 0);
  gtk_container_add(GTK_CONTAINER(eventbox),hbox);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }

  // play buttons

  btoolbar=gtk_toolbar_new();
  gtk_box_pack_start (GTK_BOX (hbox), btoolbar, FALSE, FALSE, 0);

  gtk_toolbar_set_show_arrow(GTK_TOOLBAR(btoolbar),FALSE);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(btoolbar, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }

  gtk_toolbar_set_style (GTK_TOOLBAR (btoolbar), GTK_TOOLBAR_ICONS);
  gtk_toolbar_set_icon_size (GTK_TOOLBAR(btoolbar),GTK_ICON_SIZE_SMALL_TOOLBAR);

  gtk_widget_ref(mainw->m_sepwinbutton);
  gtk_container_remove(GTK_CONTAINER(mainw->m_sepwinbutton->parent),mainw->m_sepwinbutton);
  gtk_toolbar_insert(GTK_TOOLBAR(btoolbar),GTK_TOOL_ITEM(mainw->m_sepwinbutton),-1);
  gtk_widget_unref(mainw->m_sepwinbutton);

  gtk_widget_ref(mainw->m_rewindbutton);
  gtk_container_remove(GTK_CONTAINER(mainw->m_rewindbutton->parent),mainw->m_rewindbutton);
  gtk_toolbar_insert(GTK_TOOLBAR(btoolbar),GTK_TOOL_ITEM(mainw->m_rewindbutton),-1);
  gtk_widget_unref(mainw->m_rewindbutton);

  gtk_widget_ref(mainw->m_playbutton);
  gtk_container_remove(GTK_CONTAINER(mainw->m_playbutton->parent),mainw->m_playbutton);
  gtk_toolbar_insert(GTK_TOOLBAR(btoolbar),GTK_TOOL_ITEM(mainw->m_playbutton),-1);
  gtk_widget_unref(mainw->m_playbutton);

  gtk_widget_ref(mainw->m_stopbutton);
  gtk_container_remove(GTK_CONTAINER(mainw->m_stopbutton->parent),mainw->m_stopbutton);
  gtk_toolbar_insert(GTK_TOOLBAR(btoolbar),GTK_TOOL_ITEM(mainw->m_stopbutton),-1);
  gtk_widget_unref(mainw->m_stopbutton);


  /*  gtk_widget_ref(mainw->m_playselbutton);
  gtk_container_remove(GTK_CONTAINER(mainw->m_playselbutton->parent),mainw->m_playselbutton);
  gtk_toolbar_insert(GTK_TOOLBAR(btoolbar),GTK_TOOL_ITEM(mainw->m_playselbutton),-1);
  gtk_widget_unref(mainw->m_playselbutton);*/


  gtk_widget_ref(mainw->m_loopbutton);
  gtk_container_remove(GTK_CONTAINER(mainw->m_loopbutton->parent),mainw->m_loopbutton);
  gtk_toolbar_insert(GTK_TOOLBAR(btoolbar),GTK_TOOL_ITEM(mainw->m_loopbutton),-1);
  gtk_widget_unref(mainw->m_loopbutton);



  mt->timecode=gtk_entry_new();
  time_to_string (mt,0.,TIMECODE_LENGTH);
  gtk_entry_set_max_length(GTK_ENTRY (mt->timecode),TIMECODE_LENGTH);
  gtk_entry_set_width_chars (GTK_ENTRY (mt->timecode),TIMECODE_LENGTH);
  gtk_box_pack_start (GTK_BOX (hbox), mt->timecode, FALSE, FALSE, 10);

  gtk_widget_add_events(mt->timecode,GDK_FOCUS_CHANGE_MASK);

  mt->tc_func=g_signal_connect_after (G_OBJECT (mt->timecode),"focus_out_event", G_CALLBACK (after_timecode_changed), (gpointer) mt);

  gtk_widget_modify_base(mt->timecode, GTK_STATE_NORMAL, &palette->black);
  gtk_widget_modify_text(mt->timecode, GTK_STATE_NORMAL, &palette->light_green);


  gtk_widget_set_tooltip_text( mt->insa_checkbutton, _("Select whether video clips are inserted and moved with their audio or not"));
  hbox2 = gtk_hbox_new (FALSE, 0);

  gtk_tooltips_copy(mt->insa_eventbox,mt->insa_checkbutton);
  label=gtk_label_new_with_mnemonic (_("Insert with _audio"));
  gtk_label_set_mnemonic_widget (GTK_LABEL (label),mt->insa_checkbutton);

  gtk_container_add(GTK_CONTAINER(mt->insa_eventbox),label);
  g_signal_connect (GTK_OBJECT (mt->insa_eventbox), "button_press_event",
		    G_CALLBACK (label_act_toggle),
		    mt->insa_checkbutton);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(mt->insa_eventbox, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }

  gtk_box_pack_start (GTK_BOX (hbox), hbox2, FALSE, FALSE, 2);
  gtk_box_pack_start (GTK_BOX (hbox2), mt->insa_checkbutton, FALSE, FALSE, 2);
  gtk_box_pack_start (GTK_BOX (hbox2), mt->insa_eventbox, FALSE, FALSE, 2);

  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mt->insa_checkbutton),mt->opts.insert_audio);

  g_signal_connect_after (GTK_OBJECT (mt->insa_checkbutton), "toggled",
			  G_CALLBACK (on_insa_toggled),
			  mt);


  if (prefs->lamp_buttons) {
    on_insa_toggled(GTK_TOGGLE_BUTTON(mt->insa_checkbutton),mt);
    gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(mt->insa_checkbutton),FALSE);
    gtk_widget_modify_bg(mt->insa_checkbutton, GTK_STATE_ACTIVE, &palette->light_green);
    gtk_widget_modify_bg(mt->insa_checkbutton, GTK_STATE_NORMAL, &palette->dark_red);
  }

  mt->snapo_checkbutton = gtk_check_button_new ();
  gtk_widget_set_tooltip_text( mt->snapo_checkbutton, _("Select whether timeline selection snaps to overlap between selected tracks or not"));
  hbox2 = gtk_hbox_new (FALSE, 0);

  eventbox=gtk_event_box_new();
  gtk_tooltips_copy(eventbox,mt->snapo_checkbutton);
  label=gtk_label_new_with_mnemonic (_("Select _overlap"));
  gtk_label_set_mnemonic_widget (GTK_LABEL (label),mt->snapo_checkbutton);

  gtk_container_add(GTK_CONTAINER(eventbox),label);
  g_signal_connect (GTK_OBJECT (eventbox), "button_press_event",
		    G_CALLBACK (label_act_toggle),
		    mt->snapo_checkbutton);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }

  gtk_box_pack_start (GTK_BOX (hbox), hbox2, FALSE, FALSE, 2);
  gtk_box_pack_start (GTK_BOX (hbox2), mt->snapo_checkbutton, FALSE, FALSE, 2);
  gtk_box_pack_start (GTK_BOX (hbox2), eventbox, FALSE, FALSE, 2);

  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mt->snapo_checkbutton),mt->opts.snap_over);

  g_signal_connect_after (GTK_OBJECT (mt->snapo_checkbutton), "toggled",
			  G_CALLBACK (on_snapo_toggled),
			  mt);

  if (prefs->lamp_buttons) {
    gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(mt->snapo_checkbutton),FALSE);
    gtk_widget_modify_bg(mt->snapo_checkbutton, GTK_STATE_ACTIVE, &palette->light_green);
    gtk_widget_modify_bg(mt->snapo_checkbutton, GTK_STATE_NORMAL, &palette->dark_red);
    
    on_snapo_toggled(GTK_TOGGLE_BUTTON(mt->snapo_checkbutton),mt);
  }

  // TODO - add a vbox with two hboxes
  // in each hbox we have 16 images
  // light for audio - in animate_multitrack
  // divide by out volume - then we have a volume gauge

  // add toolbar

  /*  volind=GTK_WIDGET(gtk_tool_item_new());
  mainw->volind_hbox=gtk_hbox_new(TRUE,0);
  gtk_container_add(GTK_CONTAINER(volind),mainw->volind_hbox);
  gtk_toolbar_insert(GTK_TOOLBAR(mainw->btoolbar),GTK_TOOL_ITEM(mainw->vol_label),7);
  */



  // compact view and expanded view buttons


  btoolbar=gtk_toolbar_new();
  gtk_box_pack_start (GTK_BOX (hbox), btoolbar, FALSE, FALSE, 20);

  gtk_toolbar_set_show_arrow(GTK_TOOLBAR(btoolbar),FALSE);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(btoolbar, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }

  gtk_toolbar_set_style (GTK_TOOLBAR (btoolbar), GTK_TOOLBAR_TEXT);

  mt->eview_button=GTK_WIDGET(gtk_tool_button_new(NULL,_ ("Expanded View (d)")));

  if (!mt->opts.show_ctx) gtk_tool_button_set_label(GTK_TOOL_BUTTON(mt->eview_button),_ ("Compact View (d)"));

  gtk_toolbar_insert(GTK_TOOLBAR(btoolbar),GTK_TOOL_ITEM(mt->eview_button),-1);

  g_signal_connect (GTK_OBJECT (mt->eview_button), "clicked",
		    G_CALLBACK (on_comp_exp),
		    (gpointer)view_ctx);




  btoolbar=gtk_toolbar_new();
  gtk_box_pack_start (GTK_BOX (hbox), btoolbar, FALSE, FALSE, 0);

  gtk_toolbar_set_show_arrow(GTK_TOOLBAR(btoolbar),FALSE);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(btoolbar, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }

  gtk_toolbar_set_style (GTK_TOOLBAR (btoolbar), GTK_TOOLBAR_TEXT);

  mt->grav_menuitem = gtk_menu_tool_button_new (NULL,_("_Gravity: Normal"));
  gtk_tool_button_set_use_underline (GTK_TOOL_BUTTON(mt->grav_menuitem),TRUE);

  gtk_toolbar_insert(GTK_TOOLBAR(btoolbar),GTK_TOOL_ITEM(mt->grav_menuitem),-1);

  submenu = gtk_menu_new ();
  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(submenu, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }
  gtk_menu_tool_button_set_menu (GTK_MENU_TOOL_BUTTON (mt->grav_menuitem), submenu);

  mt->grav_normal = gtk_check_menu_item_new_with_mnemonic (_("Gravity: _Normal"));
  gtk_container_add (GTK_CONTAINER(submenu), mt->grav_normal);

  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mt->grav_normal),mt->opts.grav_mode==GRAV_MODE_NORMAL);

  mt->grav_normal_func=g_signal_connect (GTK_OBJECT (mt->grav_normal), "toggled",
					 G_CALLBACK (on_grav_mode_changed),
					 (gpointer)mt);


  mt->grav_left = gtk_check_menu_item_new_with_mnemonic (_("Gravity: _Left"));
  gtk_container_add (GTK_CONTAINER(submenu), mt->grav_left);

  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mt->grav_left),mt->opts.grav_mode==GRAV_MODE_LEFT);

  mt->grav_left_func=g_signal_connect (GTK_OBJECT (mt->grav_left), "toggled",
				       G_CALLBACK (on_grav_mode_changed),
				       (gpointer)mt);



  mt->grav_right = gtk_check_menu_item_new_with_mnemonic (_("Gravity: _Right"));
  gtk_container_add (GTK_CONTAINER(submenu), mt->grav_right);

  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mt->grav_right),mt->opts.grav_mode==GRAV_MODE_RIGHT);

  mt->grav_right_func=g_signal_connect (GTK_OBJECT (mt->grav_right), "toggled",
					G_CALLBACK (on_grav_mode_changed),
					(gpointer)mt);


  gtk_widget_show_all(submenu);





  btoolbar=gtk_toolbar_new();
  gtk_box_pack_start (GTK_BOX (hbox), btoolbar, TRUE, TRUE, 0);

  gtk_toolbar_set_show_arrow(GTK_TOOLBAR(btoolbar),FALSE);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(btoolbar, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }

  gtk_toolbar_set_style (GTK_TOOLBAR (btoolbar), GTK_TOOLBAR_ICONS);
  gtk_toolbar_set_icon_size (GTK_TOOLBAR(btoolbar),GTK_ICON_SIZE_SMALL_TOOLBAR);



  gtk_widget_ref(mainw->m_mutebutton);
  gtk_container_remove(GTK_CONTAINER(mainw->m_mutebutton->parent),mainw->m_mutebutton);
  gtk_toolbar_insert(GTK_TOOLBAR(btoolbar),GTK_TOOL_ITEM(mainw->m_mutebutton),-1);
  gtk_widget_unref(mainw->m_mutebutton);


#ifndef HAVE_GTK_NICE_VERSION
  gtk_widget_ref(mainw->vol_label);
  gtk_container_remove(GTK_CONTAINER(mainw->vol_label->parent),mainw->vol_label);
  gtk_toolbar_insert(GTK_TOOLBAR(btoolbar),GTK_TOOL_ITEM(mainw->vol_label),-1);
  gtk_widget_unref(mainw->vol_label);
#else
  gtk_scale_button_set_orientation (GTK_SCALE_BUTTON(mainw->volume_scale),GTK_ORIENTATION_VERTICAL);
#endif

  gtk_widget_ref(mainw->vol_toolitem);
  gtk_container_remove(GTK_CONTAINER(mainw->vol_toolitem->parent),mainw->vol_toolitem);
  gtk_toolbar_insert(GTK_TOOLBAR(btoolbar),GTK_TOOL_ITEM(mainw->vol_toolitem),-1);
  gtk_widget_unref(mainw->vol_toolitem);



  hseparator = gtk_hseparator_new ();
  gtk_box_pack_start (GTK_BOX (mt->top_vbox), hseparator, FALSE, FALSE, 0);

  mt->hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (mt->top_vbox), mt->hbox, FALSE, FALSE, 0);

  mt->play_blank = gtk_image_new_from_pixbuf (mainw->imframe);
  frame = gtk_frame_new (_("Preview"));
  gtk_box_pack_start (GTK_BOX (mt->hbox), frame, FALSE, FALSE, 0);
  mt->fd_frame=frame;

  gtk_widget_modify_bg (frame, GTK_STATE_NORMAL, &palette->normal_back);
  gtk_widget_modify_fg (frame, GTK_STATE_NORMAL, &palette->normal_fore);
  gtk_widget_modify_fg (gtk_frame_get_label_widget(GTK_FRAME(frame)), GTK_STATE_NORMAL, &palette->normal_fore);

  eventbox=gtk_event_box_new();
  gtk_widget_set_size_request (eventbox, mt->play_window_width, mt->play_window_height);
  mt->play_box = gtk_vbox_new (FALSE, BORD_HEIGHT);
  gtk_widget_set_app_paintable(mt->play_box,TRUE);
  gtk_widget_set_size_request (mt->play_box, mt->play_window_width, mt->play_window_height);

  gtk_container_add (GTK_CONTAINER (frame), eventbox);
  gtk_container_add (GTK_CONTAINER (eventbox), mt->play_box);
  gtk_widget_modify_bg (eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  gtk_container_add (GTK_CONTAINER (mt->play_box), mt->play_blank);

  gtk_widget_add_events (eventbox, GDK_BUTTON1_MOTION_MASK | GDK_BUTTON_RELEASE_MASK | GDK_BUTTON_PRESS_MASK | GDK_ENTER_NOTIFY | GDK_LEAVE_NOTIFY);

  g_signal_connect (GTK_OBJECT (eventbox), "motion_notify_event",
		    G_CALLBACK (on_framedraw_mouse_update),
		    NULL);
  g_signal_connect (GTK_OBJECT (eventbox), "button_release_event",
		    G_CALLBACK (on_framedraw_mouse_reset),
		    NULL);
  g_signal_connect (GTK_OBJECT (eventbox), "button_press_event",
		    G_CALLBACK (on_framedraw_mouse_start),
		    NULL);
  g_signal_connect (GTK_OBJECT(eventbox), "enter-notify-event",G_CALLBACK (on_framedraw_enter),NULL);


  mt->hpaned=gtk_hpaned_new();
  gtk_box_pack_start (GTK_BOX (mt->hbox), mt->hpaned, TRUE, TRUE, 0);

  g_signal_connect (GTK_OBJECT (mt->hpaned), "accept_position",
		    G_CALLBACK (hpaned_pos),
		    (gpointer)mt);


  mt->nb = gtk_notebook_new ();
  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg (mt->nb, GTK_STATE_NORMAL, &palette->normal_back);
    gtk_widget_modify_bg (mt->nb, GTK_STATE_ACTIVE, &palette->menu_and_bars);
  }

  hbox = gtk_hbox_new (FALSE, 0);

  gtk_container_add (GTK_CONTAINER (mt->nb), hbox);

  label=gtk_label_new (_("Clips"));



  // prepare polymorph box
  mt->poly_box = gtk_vbox_new (FALSE, 0);

  gtk_container_add (GTK_CONTAINER (hbox), mt->poly_box);

  gtk_notebook_set_tab_label (GTK_NOTEBOOK (mt->nb), gtk_notebook_get_nth_page (GTK_NOTEBOOK (mt->nb), 0), label);

  gtk_widget_modify_fg (gtk_notebook_get_tab_label(GTK_NOTEBOOK(mt->nb),hbox), GTK_STATE_NORMAL, &palette->normal_fore);


  gtk_paned_pack1 (GTK_PANED (mt->hpaned), mt->nb, TRUE, FALSE);

  gtk_paned_set_position(GTK_PANED(mt->hpaned),2*scr_width/5);

  // poly clip scroll
  mt->clip_scroll = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_ref (mt->clip_scroll);
  gtk_widget_set_events (mt->clip_scroll, GDK_SCROLL_MASK);
  g_signal_connect (GTK_OBJECT (mt->clip_scroll), "scroll_event",
                      G_CALLBACK (on_mouse_scroll),
                      mt);

  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (mt->clip_scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);

  mt->clip_inner_box = gtk_hbox_new (FALSE, 10);

  gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (mt->clip_scroll), mt->clip_inner_box);

  if (palette->style&STYLE_4) {
    gtk_widget_modify_bg(GTK_BIN(mt->clip_scroll)->child, GTK_STATE_NORMAL, &palette->normal_back);
  }


  label=gtk_label_new (_("In/out"));
  hbox = gtk_hbox_new (FALSE, 0);

  gtk_container_add (GTK_CONTAINER (mt->nb), hbox);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (mt->nb), gtk_notebook_get_nth_page (GTK_NOTEBOOK (mt->nb), 1), label);
  gtk_widget_modify_fg (gtk_notebook_get_tab_label(GTK_NOTEBOOK(mt->nb),hbox), GTK_STATE_NORMAL, &palette->normal_fore);



  label=gtk_label_new (_("FX stack"));
  hbox = gtk_hbox_new (FALSE, 0);

  gtk_container_add (GTK_CONTAINER (mt->nb), hbox);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (mt->nb), gtk_notebook_get_nth_page (GTK_NOTEBOOK (mt->nb), 2), label);
  gtk_widget_modify_fg (gtk_notebook_get_tab_label(GTK_NOTEBOOK(mt->nb),hbox), GTK_STATE_NORMAL, &palette->normal_fore);


  tname=lives_fx_cat_to_text(LIVES_FX_CAT_EFFECT,TRUE); // effects
  label=gtk_label_new (tname);
  g_free(tname);
  hbox = gtk_hbox_new (FALSE, 0);

  gtk_container_add (GTK_CONTAINER (mt->nb), hbox);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (mt->nb), gtk_notebook_get_nth_page (GTK_NOTEBOOK (mt->nb), 3), label);
  gtk_widget_modify_fg (gtk_notebook_get_tab_label(GTK_NOTEBOOK(mt->nb),hbox), GTK_STATE_NORMAL, &palette->normal_fore);



  tname=lives_fx_cat_to_text(LIVES_FX_CAT_TRANSITION,TRUE); // transitions
  label=gtk_label_new (tname);
  g_free(tname);
  hbox = gtk_hbox_new (FALSE, 0);

  gtk_container_add (GTK_CONTAINER (mt->nb), hbox);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (mt->nb), gtk_notebook_get_nth_page (GTK_NOTEBOOK (mt->nb), 4), label);
  gtk_widget_modify_fg (gtk_notebook_get_tab_label(GTK_NOTEBOOK(mt->nb),hbox), GTK_STATE_NORMAL, &palette->normal_fore);


  tname=lives_fx_cat_to_text(LIVES_FX_CAT_COMPOSITOR,TRUE); // compositors
  label=gtk_label_new (tname);
  g_free(tname);
  hbox = gtk_hbox_new (FALSE, 0);

  gtk_container_add (GTK_CONTAINER (mt->nb), hbox);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (mt->nb), gtk_notebook_get_nth_page (GTK_NOTEBOOK (mt->nb), 5), label);
  gtk_widget_modify_fg (gtk_notebook_get_tab_label(GTK_NOTEBOOK(mt->nb),hbox), GTK_STATE_NORMAL, &palette->normal_fore);


  label=gtk_label_new (_("Params."));
  hbox = gtk_hbox_new (FALSE, 0);


  gtk_container_add (GTK_CONTAINER (mt->nb), hbox);
  gtk_notebook_set_tab_label (GTK_NOTEBOOK (mt->nb), gtk_notebook_get_nth_page (GTK_NOTEBOOK (mt->nb), 6), label);
  gtk_widget_modify_fg (gtk_notebook_get_tab_label(GTK_NOTEBOOK(mt->nb),hbox), 
			GTK_STATE_NORMAL, &palette->normal_fore);




  // params contents

  mt->fx_base_box = gtk_vbox_new (FALSE, 0);
  gtk_widget_ref(mt->fx_base_box);

  mt->fx_contents_box=gtk_vbox_new(FALSE,10);

  gtk_box_pack_end (GTK_BOX (mt->fx_base_box), mt->fx_contents_box, FALSE, FALSE, 0);


  hbox=gtk_hbox_new(FALSE,10);
  gtk_box_pack_end (GTK_BOX (mt->fx_contents_box), hbox, FALSE, FALSE, 0);

  mt->apply_fx_button = gtk_button_new_with_mnemonic (_("_Apply"));
  gtk_box_pack_start (GTK_BOX (hbox), mt->apply_fx_button, FALSE, FALSE, 0);
  
  g_signal_connect (GTK_OBJECT (mt->apply_fx_button), "clicked",
		    G_CALLBACK (on_set_pvals_clicked),
		    (gpointer)mt);
  

  mt->node_adj = (GObject *)gtk_adjustment_new (0., 0., 0., 1./mt->fps, 10./mt->fps, 0.);

  mt->node_scale=gtk_hscale_new(GTK_ADJUSTMENT(mt->node_adj));
  gtk_scale_set_draw_value(GTK_SCALE(mt->node_scale),FALSE);
  mt->node_spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (mt->node_adj), 0, 3);

  mt->node_adj_func=g_signal_connect_after (GTK_OBJECT (mt->node_spinbutton), "value_changed",
					    G_CALLBACK (on_node_spin_value_changed),
					    (gpointer)mt);
  

  gtk_widget_show (mt->node_spinbutton);
  gtk_box_pack_start (GTK_BOX (hbox), mt->node_spinbutton, FALSE, TRUE, 0);

  label=gtk_label_new(_("Time"));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }

    
  gtk_widget_show(mt->node_scale);
  gtk_widget_show(hbox);
  gtk_box_pack_start (GTK_BOX (hbox), mt->node_scale, TRUE, TRUE, 10);
  

  hbox=gtk_hbox_new(FALSE,10);
  gtk_box_pack_end (GTK_BOX (mt->fx_contents_box), hbox, FALSE, FALSE, 0);

  mt->del_node_button = gtk_button_new_with_mnemonic (_("_Del. node"));
  gtk_box_pack_end (GTK_BOX (hbox), mt->del_node_button, FALSE, FALSE, 0);
  gtk_widget_set_sensitive(mt->del_node_button,FALSE);
  
  g_signal_connect (GTK_OBJECT (mt->del_node_button), "clicked",
		    G_CALLBACK (on_del_node_clicked),
		    (gpointer)mt);
  
  mt->next_node_button = gtk_button_new_with_mnemonic (_("_Next node"));
  gtk_box_pack_end (GTK_BOX (hbox), mt->next_node_button, FALSE, FALSE, 0);
  gtk_widget_set_sensitive(mt->next_node_button,FALSE);

  g_signal_connect (GTK_OBJECT (mt->next_node_button), "clicked",
		    G_CALLBACK (on_next_node_clicked),
		    (gpointer)mt);
  
  mt->prev_node_button = gtk_button_new_with_mnemonic (_("_Prev node"));
  gtk_box_pack_end (GTK_BOX (hbox), mt->prev_node_button, FALSE, FALSE, 0);
  gtk_widget_set_sensitive(mt->prev_node_button,FALSE);
  
  g_signal_connect (GTK_OBJECT (mt->prev_node_button), "clicked",
		    G_CALLBACK (on_prev_node_clicked),
		    (gpointer)mt);
  

  mt->fx_label=gtk_label_new("");
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(mt->fx_label, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  gtk_box_pack_end (GTK_BOX (hbox), mt->fx_label, FALSE, FALSE, 20);


  set_mt_title(mt);

  mt_init_clips (mt,orig_file,FALSE);

  // poly audio velocity
  mt->avel_box = gtk_vbox_new (FALSE, 0);
  gtk_widget_ref (mt->avel_box);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start(GTK_BOX(mt->avel_box),hbox,FALSE,FALSE,8);

  mt->checkbutton_avel_reverse = gtk_check_button_new ();
  eventbox=gtk_event_box_new();
  label=gtk_label_new_with_mnemonic (_("_Reverse playback  "));
  gtk_label_set_mnemonic_widget (GTK_LABEL (label),mt->checkbutton_avel_reverse);

  gtk_container_add(GTK_CONTAINER(eventbox),label);
  g_signal_connect (GTK_OBJECT (eventbox), "button_press_event",
		    G_CALLBACK (label_act_toggle),
		    mt->checkbutton_avel_reverse);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    if (palette->style&STYLE_3) gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &palette->normal_back);
    else gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }

  gtk_box_pack_start (GTK_BOX (hbox), mt->checkbutton_avel_reverse, FALSE, FALSE, 10);
  gtk_box_pack_start (GTK_BOX (hbox), eventbox, FALSE, FALSE, 10);
  GTK_WIDGET_SET_FLAGS (mt->checkbutton_avel_reverse, GTK_CAN_DEFAULT|GTK_CAN_FOCUS);

  mt->check_avel_rev_func=g_signal_connect_after (GTK_OBJECT (mt->checkbutton_avel_reverse), "toggled",
						  G_CALLBACK (avel_reverse_toggled),
						  mt);


  hbox = gtk_hbox_new (FALSE, 8);
  gtk_box_pack_start(GTK_BOX(mt->avel_box),hbox,FALSE,FALSE,10);


  label = gtk_label_new_with_mnemonic (_("_Velocity  "));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  gtk_box_pack_start(GTK_BOX(hbox),label,FALSE,TRUE,0);

  spinbutton_adj = (GObject *)gtk_adjustment_new (1.,0.5,2.,.1,1.,0.);
  mt->spinbutton_avel = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_adj), 0.1, 2);
  gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (mt->spinbutton_avel),TRUE);
  gtk_box_pack_start(GTK_BOX(hbox),mt->spinbutton_avel,FALSE,TRUE,0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label),mt->spinbutton_avel);

  mt->spin_avel_func=g_signal_connect_after (GTK_OBJECT (mt->spinbutton_avel), "value_changed",
					     G_CALLBACK (avel_spin_changed),
					     mt);

  mt->avel_scale=gtk_hscale_new(GTK_ADJUSTMENT(spinbutton_adj));
  gtk_box_pack_start (GTK_BOX (hbox), mt->avel_scale, TRUE, TRUE, 10);
  gtk_scale_set_draw_value(GTK_SCALE(mt->avel_scale),FALSE);

  gtk_widget_show_all(mt->avel_box);

  // poly in_out_box
  mt->in_out_box = gtk_hbox_new (FALSE, 0);
  gtk_widget_ref (mt->in_out_box);

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_box_pack_start(GTK_BOX(mt->in_out_box),vbox,FALSE,TRUE,0);

  mt->spinbutton_in_adj = (GObject *)gtk_adjustment_new (0.,0.,0.,1./mt->fps,1.,0.);
  mt->spinbutton_in = gtk_spin_button_new (GTK_ADJUSTMENT (mt->spinbutton_in_adj), 1./mt->fps, 2);
  gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (mt->spinbutton_in),TRUE);

  mt->in_image=gtk_image_new();
  eventbox=gtk_event_box_new();
  gtk_container_add (GTK_CONTAINER (eventbox), mt->in_image);
  gtk_box_pack_start(GTK_BOX(vbox),eventbox,FALSE,FALSE,0);


  g_signal_connect (GTK_OBJECT (eventbox), "button_press_event",
		    G_CALLBACK (in_out_ebox_pressed),
		    (gpointer)mt);
  g_signal_connect (GTK_OBJECT (eventbox), "button_release_event",
		    G_CALLBACK (on_drag_clip_end),
		    (gpointer)mt);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    if (palette->style&STYLE_3) gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &palette->normal_back);
    else gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }

  mt->in_hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox),mt->in_hbox,TRUE,FALSE,0);
  gtk_box_pack_start(GTK_BOX(mt->in_hbox),mt->spinbutton_in,FALSE,FALSE,0);

  mt->checkbutton_start_anchored = gtk_check_button_new ();
  gtk_widget_set_tooltip_text( mt->checkbutton_start_anchored, _("Anchor the start point to the timeline"));
  mt->in_eventbox=gtk_event_box_new();
  gtk_tooltips_copy(mt->in_eventbox,mt->checkbutton_start_anchored);
  label=gtk_label_new_with_mnemonic (_("Anchor _start"));
  gtk_label_set_mnemonic_widget (GTK_LABEL (label),mt->checkbutton_start_anchored);

  g_signal_connect (GTK_OBJECT (mt->in_eventbox), "button_press_event",
		    G_CALLBACK (label_act_toggle),
		    mt->checkbutton_start_anchored);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(mt->in_eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    if (palette->style&STYLE_3) gtk_widget_modify_bg(mt->in_eventbox, GTK_STATE_NORMAL, &palette->normal_back);
    else gtk_widget_modify_bg(mt->in_eventbox, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }

  gtk_box_pack_start (GTK_BOX (mt->in_hbox), mt->in_eventbox, FALSE, FALSE, 10);


  hbox = gtk_hbox_new (FALSE, 0);

  gtk_box_pack_start(GTK_BOX(hbox),label,FALSE,FALSE,10);
  gtk_box_pack_start(GTK_BOX(hbox),mt->checkbutton_start_anchored,TRUE,FALSE,10);

  gtk_container_add (GTK_CONTAINER (mt->in_eventbox), hbox);
  GTK_WIDGET_SET_FLAGS (mt->checkbutton_start_anchored, GTK_CAN_DEFAULT|GTK_CAN_FOCUS);
  
  mt->spin_in_func=g_signal_connect_after (GTK_OBJECT (mt->spinbutton_in), "value_changed",
					   G_CALLBACK (in_out_start_changed),
					   mt);

  mt->check_start_func=g_signal_connect_after (GTK_OBJECT (mt->checkbutton_start_anchored), "toggled",
					    G_CALLBACK (in_anchor_toggled),
					    mt);


  mt->start_in_label=gtk_label_new(_("Start frame"));
  gtk_box_pack_start (GTK_BOX (mt->in_hbox), mt->start_in_label, FALSE, FALSE, 10);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(mt->start_in_label, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_box_pack_end(GTK_BOX(mt->in_out_box),vbox,FALSE,TRUE,0);
  mt->spinbutton_out_adj = (GObject *)gtk_adjustment_new (0.,0.,0.,1./mt->fps,1.,0.);
  mt->spinbutton_out = gtk_spin_button_new (GTK_ADJUSTMENT (mt->spinbutton_out_adj), 1./mt->fps, 2);
  gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (mt->spinbutton_out),TRUE);
  mt->out_image=gtk_image_new();

  eventbox=gtk_event_box_new();
  gtk_container_add (GTK_CONTAINER (eventbox), mt->out_image);
  gtk_box_pack_start(GTK_BOX(vbox),eventbox,FALSE,FALSE,0);

  g_signal_connect (GTK_OBJECT (eventbox), "button_press_event",
		    G_CALLBACK (in_out_ebox_pressed),
		    (gpointer)mt);
  g_signal_connect (GTK_OBJECT (eventbox), "button_release_event",
		    G_CALLBACK (on_drag_clip_end),
		    (gpointer)mt);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    if (palette->style&STYLE_3) gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &palette->normal_back);
    else gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }

  mt->out_hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox),mt->out_hbox,TRUE,FALSE,0);

  gtk_box_pack_start(GTK_BOX(mt->out_hbox),mt->spinbutton_out,FALSE,FALSE,0);

  mt->checkbutton_end_anchored = gtk_check_button_new ();
  gtk_widget_set_tooltip_text( mt->checkbutton_end_anchored, _("Anchor the end point to the timeline"));
  mt->out_eventbox=gtk_event_box_new();
  gtk_tooltips_copy(mt->out_eventbox,mt->checkbutton_end_anchored);
  label=gtk_label_new_with_mnemonic (_("Anchor _end"));
  gtk_label_set_mnemonic_widget (GTK_LABEL (label),mt->checkbutton_end_anchored);

  g_signal_connect (GTK_OBJECT (mt->out_eventbox), "button_press_event",
		    G_CALLBACK (label_act_toggle),
		    mt->checkbutton_end_anchored);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(mt->out_eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    if (palette->style&STYLE_3) gtk_widget_modify_bg(mt->out_eventbox, GTK_STATE_NORMAL, &palette->normal_back);
    else gtk_widget_modify_bg(mt->out_eventbox, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }

  gtk_box_pack_start (GTK_BOX (mt->out_hbox), mt->out_eventbox, FALSE, FALSE, 10);

  hbox = gtk_hbox_new (FALSE, 0);

  gtk_box_pack_start(GTK_BOX(hbox),label,FALSE,FALSE,10);
  gtk_box_pack_start(GTK_BOX(hbox),mt->checkbutton_end_anchored,TRUE,FALSE,10);

  gtk_container_add (GTK_CONTAINER (mt->out_eventbox), hbox);

  GTK_WIDGET_SET_FLAGS (mt->checkbutton_end_anchored, GTK_CAN_DEFAULT|GTK_CAN_FOCUS);

  mt->spin_out_func=g_signal_connect_after (GTK_OBJECT (mt->spinbutton_out), "value_changed",
					    G_CALLBACK (in_out_end_changed),
					    mt);

  mt->check_end_func=g_signal_connect_after (GTK_OBJECT (mt->checkbutton_end_anchored), "toggled",
					     G_CALLBACK (out_anchor_toggled),
					     mt);

  mt->end_out_label=gtk_label_new(_("End frame"));
  gtk_box_pack_start (GTK_BOX (mt->out_hbox), mt->end_out_label, FALSE, FALSE, 10);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(mt->end_out_label, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  g_signal_handler_block (mt->spinbutton_in,mt->spin_in_func);
  g_signal_handler_block (mt->spinbutton_out,mt->spin_out_func);

  gtk_widget_show_all(mt->in_out_box);
  gtk_widget_show_all(mt->nb);

  g_signal_connect (GTK_OBJECT (mt->nb), "switch_page",
		    G_CALLBACK (notebook_page),
		    (gpointer)mt);


  mt->poly_state=POLY_NONE;
  polymorph(mt,POLY_CLIPS);

  mt->context_frame = gtk_frame_new (_("Info"));
  gtk_widget_modify_bg (mt->context_frame, GTK_STATE_NORMAL, &palette->normal_back);
  gtk_widget_modify_fg (mt->context_frame, GTK_STATE_NORMAL, &palette->normal_fore);
  gtk_widget_modify_fg (gtk_frame_get_label_widget(GTK_FRAME(mt->context_frame)), GTK_STATE_NORMAL, &palette->normal_fore);

  gtk_paned_pack2 (GTK_PANED (mt->hpaned), mt->context_frame, TRUE, TRUE);

  mt->context_scroll=NULL;

  clear_context(mt);

  if (mainw->imsep==NULL) {
    mt->sep_image=NULL;
    hseparator = gtk_hseparator_new ();
    gtk_box_pack_start (GTK_BOX (mt->top_vbox), hseparator, FALSE, FALSE, 2);
    if (palette->style&STYLE_5) {
      gtk_widget_modify_fg(hseparator, GTK_STATE_NORMAL, &palette->normal_back);
      gtk_widget_modify_bg(hseparator, GTK_STATE_NORMAL, &palette->normal_back);
    }
  }
  else {
    hseparator = gtk_hseparator_new ();
    gtk_box_pack_start (GTK_BOX (mt->top_vbox), hseparator, FALSE, FALSE, 0);
    if (palette->style&STYLE_5) {
      gtk_widget_modify_fg(hseparator, GTK_STATE_NORMAL, &palette->normal_back);
      gtk_widget_modify_bg(hseparator, GTK_STATE_NORMAL, &palette->normal_back);
    }
    mt->sep_image = gtk_image_new_from_pixbuf (mainw->imsep);
    gtk_box_pack_start (GTK_BOX (mt->top_vbox), mt->sep_image, FALSE, FALSE, 0);
    hseparator = gtk_hseparator_new ();
    gtk_box_pack_start (GTK_BOX (mt->top_vbox), hseparator, FALSE, FALSE, 0);
    if (palette->style&STYLE_5) {
      gtk_widget_modify_fg(hseparator, GTK_STATE_NORMAL, &palette->normal_back);
      gtk_widget_modify_bg(hseparator, GTK_STATE_NORMAL, &palette->normal_back);
    }
  }

  mt_init_start_end_spins(mt);

  mt->vpaned=gtk_vpaned_new();
  gtk_box_pack_start (GTK_BOX (mt->top_vbox), mt->vpaned, TRUE, TRUE, 0);

  g_signal_connect (GTK_OBJECT (mt->vpaned), "accept_position",
		    G_CALLBACK (paned_pos),
		    (gpointer)mt);

  tl_vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (tl_vbox), 0);

  gtk_container_add (GTK_CONTAINER (mt->vpaned), tl_vbox);


  mt->timeline_table_header = gtk_table_new (2, 40, TRUE);
  gtk_table_set_row_spacings(GTK_TABLE(mt->timeline_table_header),0);

  eventbox=gtk_event_box_new();
  gtk_box_pack_start (GTK_BOX (tl_vbox), eventbox, FALSE, FALSE, 0);

  if (palette->style&STYLE_1) {
    if (palette->style&STYLE_3) gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &palette->normal_back);
    else gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }

  g_signal_connect (GTK_OBJECT (eventbox), "button_press_event",
		    G_CALLBACK (on_track_header_click),
		    (gpointer)mt);

  g_signal_connect (GTK_OBJECT (eventbox), "button_release_event",
		    G_CALLBACK (on_track_header_release),
		    (gpointer)mt);

  gtk_widget_add_events (eventbox, GDK_BUTTON1_MOTION_MASK | GDK_BUTTON_RELEASE_MASK | GDK_BUTTON_PRESS_MASK);
  mt->mouse_mot1=g_signal_connect (GTK_OBJECT (eventbox), "motion_notify_event",
				   G_CALLBACK (on_track_header_move),
				   (gpointer)mt);
  g_signal_handler_block (eventbox,mt->mouse_mot1);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (eventbox), hbox);

  vadjustment = (GObject *)gtk_adjustment_new (1.0,1.0,1.0,1.0,1.0,1.0);
  scrollbar=gtk_vscrollbar_new(GTK_ADJUSTMENT(vadjustment));
  gtk_widget_set_sensitive(scrollbar,FALSE);

  gtk_box_pack_start (GTK_BOX (hbox), mt->timeline_table_header, TRUE, TRUE, 0);
  gtk_box_pack_end (GTK_BOX (hbox), scrollbar, FALSE, FALSE, 10);

  mt->tl_hbox = gtk_hbox_new (FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (mt->tl_hbox), 0);
    
  gtk_box_pack_start (GTK_BOX (tl_vbox), mt->tl_hbox, TRUE, TRUE, 0);

  mt->vadjustment = (GObject *)gtk_adjustment_new (0.0,0.0,1.0,1.0,mt->max_disp_vtracks,1.0);
  mt->scrollbar=gtk_vscrollbar_new(GTK_ADJUSTMENT(mt->vadjustment));

  g_signal_connect (GTK_OBJECT (mt->scrollbar), "value_changed",
		    G_CALLBACK (scroll_track_by_scrollbar),
		    (gpointer)mt);

  mt->tl_eventbox=gtk_event_box_new();
  gtk_box_pack_start (GTK_BOX (mt->tl_hbox), mt->tl_eventbox, TRUE, TRUE, 0);

  if (palette->style&STYLE_1) {
    if (palette->style&STYLE_3) gtk_widget_modify_bg(mt->tl_eventbox, GTK_STATE_NORMAL, &palette->normal_back);
    else gtk_widget_modify_bg(mt->tl_eventbox, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }

  g_signal_connect (GTK_OBJECT (mt->tl_eventbox), "button_press_event",
		    G_CALLBACK (on_track_between_click),
		    (gpointer)mt);

  g_signal_connect (GTK_OBJECT (mt->tl_eventbox), "button_release_event",
		    G_CALLBACK (on_track_between_release),
		    (gpointer)mt);


  gtk_widget_add_events (mt->tl_eventbox, GDK_BUTTON1_MOTION_MASK | GDK_BUTTON_RELEASE_MASK | GDK_BUTTON_PRESS_MASK|GDK_SCROLL_MASK);
  mt->mouse_mot2=g_signal_connect (GTK_OBJECT (mt->tl_eventbox), "motion_notify_event",
				  G_CALLBACK (on_track_move),
				  (gpointer)mt);

  g_signal_handler_block (mt->tl_eventbox,mt->mouse_mot2);


  g_signal_connect (GTK_OBJECT (mt->tl_eventbox), "scroll_event",
		    G_CALLBACK (on_mt_timeline_scroll),
		    (gpointer)mt);

  gtk_box_pack_end (GTK_BOX (mt->tl_hbox), mt->scrollbar, FALSE, FALSE, 10);


  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (tl_vbox), hbox, FALSE, FALSE, 4);
  label=gtk_label_new(_("Scroll"));

  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 10);

  mt->hadjustment = (GObject *)gtk_adjustment_new (0.0,0.0,1.,0.25,1.,1.);
  mt->time_scrollbar=gtk_hscrollbar_new(GTK_ADJUSTMENT(mt->hadjustment));
  gtk_box_pack_start (GTK_BOX (hbox), mt->time_scrollbar, TRUE, TRUE, 10);

  g_signal_connect (GTK_OBJECT (mt->time_scrollbar), "value_changed",
		    G_CALLBACK (scroll_time_by_scrollbar),
		    (gpointer)mt);

  mt->num_video_tracks=0;

  mt->timeline_table=NULL;
  mt->timeline_eb=NULL;

  if (prefs->ar_layout&&mt->event_list==NULL&&!mainw->recoverable_layout) {
    gchar *eload_file=g_build_filename(prefs->tmpdir,mainw->set_name,"layouts",prefs->ar_layout_name,NULL);
    mt->auto_reloading=TRUE;
    set_pref("ar_layout",""); // in case we crash...
    mainw->event_list=mt->event_list=load_event_list(mt,eload_file);
    mt->auto_reloading=FALSE;
    g_free(eload_file);
    if (mt->event_list!=NULL) {
      mt_init_tracks(mt,TRUE);
      remove_markers(mt->event_list);
      set_pref("ar_layout",prefs->ar_layout_name);
    }
    else {
      prefs->ar_layout=FALSE;
      memset(prefs->ar_layout_name,0,1);
      mt_init_tracks (mt,TRUE);
      mainw->unordered_blocks=FALSE;
    }
  }
  else if (mainw->recoverable_layout) {
    mt_load_recovery_layout(mt);
  }
  else {
    mt_init_tracks (mt,TRUE);
    mainw->unordered_blocks=FALSE;
  }

  add_message_scroller(mt->vpaned);

  gtk_widget_set_size_request (mt->window, scr_width-MT_BORDER_WIDTH, -1);

  // add info bar

  gtk_window_add_accel_group (GTK_WINDOW (mt->window), mt->accel_group);

  g_signal_connect (GTK_OBJECT (mt->window), "delete_event",
		    G_CALLBACK (on_mt_delete_event),
		    (gpointer)mt);

  mainw->multitrack=mt;

  gtk_accel_group_connect (GTK_ACCEL_GROUP (mt->accel_group), GDK_Page_Up, GDK_CONTROL_MASK, (GtkAccelFlags)0,
			   g_cclosure_new (G_CALLBACK (mt_prevclip),mt,NULL));
  gtk_accel_group_connect (GTK_ACCEL_GROUP (mt->accel_group), GDK_Page_Down, GDK_CONTROL_MASK, (GtkAccelFlags)0, 
			   g_cclosure_new (G_CALLBACK (mt_nextclip),mt,NULL));

  gtk_accel_group_connect (GTK_ACCEL_GROUP (mt->accel_group), GDK_Left, GDK_CONTROL_MASK, (GtkAccelFlags)0, 
			   g_cclosure_new (G_CALLBACK (mt_tlback),mt,NULL));
  gtk_accel_group_connect (GTK_ACCEL_GROUP (mt->accel_group), GDK_Right, GDK_CONTROL_MASK, (GtkAccelFlags)0, 
			   g_cclosure_new (G_CALLBACK (mt_tlfor),mt,NULL));

  gtk_accel_group_connect (GTK_ACCEL_GROUP (mt->accel_group), GDK_Left, GDK_SHIFT_MASK, (GtkAccelFlags)0, 
			   g_cclosure_new (G_CALLBACK (mt_tlback_frame),mt,NULL));
  gtk_accel_group_connect (GTK_ACCEL_GROUP (mt->accel_group), GDK_Right, GDK_SHIFT_MASK, (GtkAccelFlags)0, 
			   g_cclosure_new (G_CALLBACK (mt_tlfor_frame),mt,NULL));

  gtk_accel_group_connect (GTK_ACCEL_GROUP (mt->accel_group), GDK_Up, GDK_CONTROL_MASK, (GtkAccelFlags)0, 
			   g_cclosure_new (G_CALLBACK (mt_trup),mt,NULL));
  gtk_accel_group_connect (GTK_ACCEL_GROUP (mt->accel_group), GDK_Down, GDK_CONTROL_MASK, (GtkAccelFlags)0, 
			   g_cclosure_new (G_CALLBACK (mt_trdown),mt,NULL));

  gtk_accel_group_connect (GTK_ACCEL_GROUP (mt->accel_group), GDK_Return, GDK_CONTROL_MASK, (GtkAccelFlags)0, 
			   g_cclosure_new (G_CALLBACK (mt_selblock),mt,NULL));

  mt->last_direction=DIRECTION_POSITIVE;

  // set check menuitems
  if (mt->opts.mouse_mode==MOUSE_MODE_MOVE) on_mouse_mode_changed(GTK_MENU_ITEM(mt->mm_move),(gpointer)mt);
  else if (mt->opts.mouse_mode==MOUSE_MODE_SELECT) on_mouse_mode_changed(GTK_MENU_ITEM(mt->mm_select),(gpointer)mt);

  if (mt->opts.insert_mode==INSERT_MODE_NORMAL) on_insert_mode_changed(GTK_MENU_ITEM(mt->ins_normal),(gpointer)mt);

  if (mt->opts.grav_mode==GRAV_MODE_NORMAL) on_grav_mode_changed(GTK_MENU_ITEM(mt->grav_normal),(gpointer)mt);
  else if (mt->opts.grav_mode==GRAV_MODE_LEFT) on_grav_mode_changed(GTK_MENU_ITEM(mt->grav_left),(gpointer)mt);
  else if (mt->opts.grav_mode==GRAV_MODE_RIGHT) on_grav_mode_changed(GTK_MENU_ITEM(mt->grav_right),(gpointer)mt);


  mt_sensitise(mt);
  mt->is_ready=TRUE;

  gtk_widget_grab_focus(mainw->textview1);

  return mt;
}


void delete_audio_track(lives_mt *mt, GtkWidget *eventbox, gboolean full) {
  // WARNING - does not yet delete events from event_list
  // only deletes visually

  track_rect *block=(track_rect *)g_object_get_data (G_OBJECT(eventbox), "blocks"),*blocknext;

  GtkWidget *label,*labelbox,*arrow,*ahbox,*xeventbox;
  GdkPixbuf *bgimg,*st_image;

  while (block!=NULL) {
    blocknext=block->next;
    if (mt->block_selected==block) mt->block_selected=NULL;
    g_free(block);
    block=blocknext;
  }

  if ((bgimg=(GdkPixbuf *)g_object_get_data(G_OBJECT(eventbox), "bgimg"))!=NULL) {
    gdk_pixbuf_unref(bgimg);
  }

  if ((st_image=(GdkPixbuf *)g_object_get_data(G_OBJECT(eventbox),"backup_image"))!=NULL) {
    g_object_unref(st_image);
  }

  label=(GtkWidget *)g_object_get_data (G_OBJECT(eventbox), "label");
  arrow=(GtkWidget *)g_object_get_data (G_OBJECT(eventbox), "arrow");
  gtk_widget_destroy(label);
  gtk_widget_destroy(arrow);
  if (GPOINTER_TO_INT(g_object_get_data(G_OBJECT(eventbox),"hidden"))==0) {
    labelbox=(GtkWidget *)g_object_get_data (G_OBJECT(eventbox), "labelbox");
    ahbox=(GtkWidget *)g_object_get_data (G_OBJECT(eventbox), "ahbox");
    if (labelbox!=NULL) gtk_widget_destroy(labelbox);
    if (ahbox!=NULL) gtk_widget_destroy(ahbox);
  }

  xeventbox=(GtkWidget *)g_object_get_data(G_OBJECT(eventbox),"achan0");
  if (xeventbox!=NULL) {
    if ((bgimg=(GdkPixbuf *)g_object_get_data(G_OBJECT(xeventbox), "bgimg"))!=NULL) {
      gdk_pixbuf_unref(bgimg);
    }
    
    if ((st_image=(GdkPixbuf *)g_object_get_data(G_OBJECT(xeventbox),"backup_image"))!=NULL) {
      g_object_unref(st_image);
    }
    gtk_widget_destroy(xeventbox);
  }
  if (cfile->achans>1) {
    xeventbox=(GtkWidget *)g_object_get_data(G_OBJECT(eventbox),"achan1");
    if (xeventbox!=NULL) {
      if ((bgimg=(GdkPixbuf *)g_object_get_data(G_OBJECT(xeventbox), "bgimg"))!=NULL) {
	gdk_pixbuf_unref(bgimg);
      }
      
      if ((st_image=(GdkPixbuf *)g_object_get_data(G_OBJECT(xeventbox),"backup_image"))!=NULL) {
	g_object_unref(st_image);
      }
      gtk_widget_destroy(xeventbox);
    }
  }

  g_free(g_object_get_data(G_OBJECT(eventbox),"track_name"));
  gtk_widget_destroy(eventbox);
}



static int *update_layout_map(weed_plant_t *event_list) {
  // update our current layout map with the current layout
  // returns an int * of maximum frame used for each clip that exists, 0 means unused
  int *used_clips;
  weed_plant_t *event=get_first_event(event_list);
  int i;

  used_clips=(int *)g_malloc((MAX_FILES+1)*sizint);
  for (i=1;i<=MAX_FILES;i++) used_clips[i]=0;

  while (event!=NULL) {
    if (WEED_EVENT_IS_FRAME(event)) {
      int numtracks=weed_leaf_num_elements(event,"clips");
      if (numtracks>0) {
	int i,error;
	int *clip_index=weed_get_int_array(event,"clips",&error);
	int *frame_index=weed_get_int_array(event,"frames",&error);
	for (i=0;i<numtracks;i++) {
	  if (clip_index[i]>0&&(frame_index[i]>used_clips[clip_index[i]])) used_clips[clip_index[i]]=frame_index[i];
	}
	weed_free(clip_index);
	weed_free(frame_index);
      }
    }
    event=get_next_event(event);
  }
  return used_clips;
}



static double *update_layout_map_audio(weed_plant_t *event_list) {
  // update our current layout map with the current layout
  // returns a double * of maximum audio in seconds used for each clip that exists, 0. means unused
  double *used_clips;
  weed_plant_t *event=get_first_event(event_list);
  int i;

  // TODO - use linked lists
  double aseek[65536];
  double avel[65536];
  weed_timecode_t atc[65536];
  int last_aclips[65536];

  double neg_aseek[65536];
  double neg_avel[65536];
  weed_timecode_t neg_atc[65536];
  int neg_last_aclips[65536];

  int atrack;
  double aval;
  weed_timecode_t tc;
  int last_aclip;

  used_clips=(double *)g_malloc((MAX_FILES+1)*sizdbl);
  for (i=1;i<=MAX_FILES;i++) used_clips[i]=0.;

  for (i=0;i<65536;i++) {
    avel[i]=neg_avel[i]=0.;
  }

  while (event!=NULL) {
    if (WEED_EVENT_IS_FRAME(event)) {
      if (weed_plant_has_leaf(event,"audio_clips")) {
	int numatracks=weed_leaf_num_elements(event,"audio_clips");
	int i,error;
	int *aclip_index=weed_get_int_array(event,"audio_clips",&error);
	double *aseek_index=weed_get_double_array(event,"audio_seeks",&error);
	for (i=0;i<numatracks;i+=2) {
	  if (aclip_index[i+1]>0) {
	    atrack=aclip_index[i];
	    tc=get_event_timecode(event);
	    if (atrack>=0) {
	      if (atrack>65535) {
		LIVES_ERROR("invalid atrack");
	      }
	      else {
		if (avel[atrack]!=0.) {
		  aval=aseek[atrack]+(tc-atc[atrack])/U_SEC*avel[atrack];
		  last_aclip=last_aclips[atrack];
		  if (aval>used_clips[last_aclip]) used_clips[last_aclip]=aval;
		}
		aseek[atrack]=aseek_index[i];
		avel[atrack]=aseek_index[i+1];
		atc[atrack]=tc;
		last_aclips[atrack]=aclip_index[i+1];
	      }
	    }
	    else {
	      atrack=-atrack;
	      if (atrack>65535) {
		LIVES_ERROR("invalid back atrack");
	      }
	      else {
		if (neg_avel[atrack]!=0.) {
		  aval=neg_aseek[atrack]+(tc-neg_atc[atrack])/U_SEC*neg_avel[atrack];
		  last_aclip=neg_last_aclips[atrack];
		  if (aval>used_clips[last_aclip]) used_clips[last_aclip]=aval;
		}
		neg_aseek[atrack]=aseek_index[i];
		neg_avel[atrack]=aseek_index[i+1];
		neg_atc[atrack]=tc;
		neg_last_aclips[atrack]=aclip_index[i+1];
	      }
	    }
	  }
	}
	weed_free(aclip_index);
	weed_free(aseek_index);
      }
    }
    event=get_next_event(event);
  }
  return used_clips;
}



gboolean used_in_current_layout(lives_mt *mt, gint file) {
  // see if <file> is used in current layout
  int *layout_map;
  double *layout_map_audio;
  gboolean retval=FALSE;

  if (mainw->stored_event_list!=NULL) {
    return (mainw->files[file]->stored_layout_frame>0||mainw->files[file]->stored_layout_audio>0.);
  }

  if (mt!=NULL&&mt->event_list!=NULL) {
    layout_map=update_layout_map(mt->event_list);
    layout_map_audio=update_layout_map_audio(mt->event_list);
    
    if (layout_map[file]>0||layout_map_audio[file]>0.) retval=TRUE;
    
    if (layout_map!=NULL) g_free(layout_map);
    if (layout_map_audio!=NULL) g_free(layout_map_audio);
  }

  return retval;

}






gboolean multitrack_delete (lives_mt *mt, gboolean save_layout) {
  // free lives_mt struct
  int i;
  gboolean transfer_focus=FALSE;
  int *layout_map;
  double *layout_map_audio;
#ifdef ENABLE_OSC
  gchar *tmp;
#endif
  mainw->cancelled=CANCEL_NONE;

  if (mt->idlefunc>0) g_source_remove(mt->idlefunc);
  mt->idlefunc=0;

  if (save_layout||mainw->scrap_file!=-1||mainw->ascrap_file!=-1) {
    gint file_selected=mt->file_selected;
    if (!check_for_layout_del(mt,TRUE)) {
      mt->idlefunc=mt_idle_add(mt);
      return FALSE;
    }
    mt->file_selected=file_selected; // because init_clips will reset this
  }
  else {
    if (mt->event_list!=NULL) {
      save_event_list_inner(mt,-1,mt->event_list,NULL); // set width, height, fps etc.
      add_markers(mt,mt->event_list);
      mainw->stored_event_list=mt->event_list;
      mt->event_list=NULL;
      mainw->stored_event_list_changed=mt->changed;
      memcpy(mainw->stored_layout_name,mt->layout_name,(strlen(mt->layout_name)+1));
      
      mainw->stored_layout_undos=mt->undos;
      mainw->sl_undo_mem=mt->undo_mem;
      mainw->sl_undo_buffer_used=mt->undo_buffer_used;
      mainw->sl_undo_offset=mt->undo_offset;
      
      mt->undos=NULL;
      mt->undo_mem=NULL;

      // update layout maps (kind of) with the stored_event_list
      
      layout_map=update_layout_map(mainw->stored_event_list);
      layout_map_audio=update_layout_map_audio(mainw->stored_event_list);
      
      for (i=0;i<MAX_FILES;i++) {
	if (mainw->files[i]!=NULL&&(layout_map[i]!=0||layout_map_audio[i]!=0.)) {
	  mainw->files[i]->stored_layout_frame=layout_map[i];
	  mainw->files[i]->stored_layout_audio=layout_map_audio[i];
	  mainw->files[i]->stored_layout_fps=mainw->files[i]->fps;
	  mainw->files[i]->stored_layout_idx=i;
	}
      }

      if (layout_map!=NULL) g_free(layout_map);
      if (layout_map_audio!=NULL) g_free(layout_map_audio);
    }
  }

  if (mt->amixer!=NULL) on_amixer_close_clicked(NULL,mt);

  mt->no_expose=TRUE;
  mt->is_ready=FALSE;

  mainw->multi_opts.set=TRUE;
  mainw->multi_opts.move_effects=mt->opts.move_effects;
  mainw->multi_opts.fx_auto_preview=mt->opts.fx_auto_preview;
  mainw->multi_opts.snap_over=mt->opts.snap_over;
  mainw->multi_opts.grav_mode=mt->opts.grav_mode;
  mainw->multi_opts.mouse_mode=mt->opts.mouse_mode;
  mainw->multi_opts.insert_mode=mt->opts.insert_mode;
  mainw->multi_opts.show_audio=mt->opts.show_audio;
  mainw->multi_opts.show_ctx=mt->opts.show_ctx;
  mainw->multi_opts.ign_ins_sel=mt->opts.ign_ins_sel;
  mainw->multi_opts.follow_playback=mt->opts.follow_playback;

  if (mt->poly_state==POLY_PARAMS) polymorph(mt,POLY_CLIPS);

  if (mt->undo_mem!=NULL) g_free(mt->undo_mem);
  mt->undo_mem=NULL;
  if (mt->undos!=NULL) g_list_free(mt->undos);

  if (mt->selected_tracks!=NULL) g_list_free(mt->selected_tracks);

  if (mt->aparam_view_list!=NULL) g_list_free(mt->aparam_view_list);

  if (mainw->event_list==mt->event_list) mainw->event_list=NULL;
  if (mt->event_list!=NULL) event_list_free(mt->event_list);
  mt->event_list=NULL;

  if (mt->clip_selected>=0&&mainw->files[mt_file_from_clip(mt,mt->clip_selected)]!=NULL) mt_file_from_clip(mt,mt->clip_selected);

  if (mt->cursor!=NULL) gdk_cursor_unref(mt->cursor);

  if (mt->clip_labels!=NULL) g_list_free(mt->clip_labels);

  add_message_scroller(mainw->message_box);

  if (prefs->show_gui) {
    if (gtk_window_has_toplevel_focus(GTK_WINDOW(mt->window))) transfer_focus=TRUE;
    gtk_widget_show (mainw->LiVES);
    mainw->is_ready=mainw_was_ready;
    unblock_expose();
  }

  gtk_window_remove_accel_group (GTK_WINDOW (mt->window), mt->accel_group);

  g_signal_handler_block(mainw->full_screen,mainw->fullscreen_cb_func);
  g_signal_handler_block(mainw->sepwin,mainw->sepwin_cb_func);
  gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (mainw->full_screen),mainw->fs);
  gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (mainw->sepwin),mainw->sep_win);
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mainw->sticky),TRUE);
  g_signal_handler_unblock(mainw->full_screen,mainw->fullscreen_cb_func);
  g_signal_handler_unblock(mainw->sepwin,mainw->sepwin_cb_func);

  if (mainw->sep_win&&prefs->sepwin_type==1&&mainw->play_window==NULL) make_play_window();

  if (mainw->play_window!=NULL) {
    gtk_window_remove_accel_group (GTK_WINDOW (mainw->play_window), mt->accel_group);
    gtk_window_add_accel_group (GTK_WINDOW (mainw->play_window), mainw->accel_group);
  }

  // put buttons back in mainw->menubar
  mt_swap_play_pause(mt,FALSE);

  g_signal_handler_block(mainw->loop_continue,mainw->loop_cont_func);
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mainw->loop_continue),mainw->loop_cont);
  g_signal_handler_unblock(mainw->loop_continue,mainw->loop_cont_func);

  g_signal_handler_block(mainw->mute_audio,mainw->mute_audio_func);
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mainw->mute_audio),mainw->mute);
  g_signal_handler_unblock(mainw->mute_audio,mainw->mute_audio_func);


  gtk_widget_ref(mainw->m_sepwinbutton);
  gtk_container_remove(GTK_CONTAINER(mainw->m_sepwinbutton->parent),mainw->m_sepwinbutton);
  gtk_toolbar_insert(GTK_TOOLBAR(mainw->btoolbar),GTK_TOOL_ITEM(mainw->m_sepwinbutton),0);
  gtk_widget_unref(mainw->m_sepwinbutton);

  gtk_widget_ref(mainw->m_rewindbutton);
  gtk_container_remove(GTK_CONTAINER(mainw->m_rewindbutton->parent),mainw->m_rewindbutton);
  gtk_toolbar_insert(GTK_TOOLBAR(mainw->btoolbar),GTK_TOOL_ITEM(mainw->m_rewindbutton),1);
  gtk_widget_unref(mainw->m_rewindbutton);

  gtk_widget_ref(mainw->m_playbutton);
  gtk_container_remove(GTK_CONTAINER(mainw->m_playbutton->parent),mainw->m_playbutton);
  gtk_toolbar_insert(GTK_TOOLBAR(mainw->btoolbar),GTK_TOOL_ITEM(mainw->m_playbutton),2);
  gtk_widget_unref(mainw->m_playbutton);

  gtk_widget_ref(mainw->m_stopbutton);
  gtk_container_remove(GTK_CONTAINER(mainw->m_stopbutton->parent),mainw->m_stopbutton);
  gtk_toolbar_insert(GTK_TOOLBAR(mainw->btoolbar),GTK_TOOL_ITEM(mainw->m_stopbutton),3);
  gtk_widget_unref(mainw->m_stopbutton);

  /*  gtk_widget_ref(mainw->m_playselbutton);
  gtk_container_remove(GTK_CONTAINER(mainw->m_playselbutton->parent),mainw->m_playselbutton);
  gtk_toolbar_insert(GTK_TOOLBAR(mainw->btoolbar),GTK_TOOL_ITEM(mainw->m_playselbutton),4);
  gtk_widget_unref(mainw->m_playselbutton);*/

  gtk_widget_ref(mainw->m_loopbutton);
  gtk_container_remove(GTK_CONTAINER(mainw->m_loopbutton->parent),mainw->m_loopbutton);
  gtk_toolbar_insert(GTK_TOOLBAR(mainw->btoolbar),GTK_TOOL_ITEM(mainw->m_loopbutton),5);
  gtk_widget_unref(mainw->m_loopbutton);

  gtk_widget_ref(mainw->m_mutebutton);
  gtk_container_remove(GTK_CONTAINER(mainw->m_mutebutton->parent),mainw->m_mutebutton);
  gtk_toolbar_insert(GTK_TOOLBAR(mainw->btoolbar),GTK_TOOL_ITEM(mainw->m_mutebutton),6);
  gtk_widget_unref(mainw->m_mutebutton);

#ifndef HAVE_GTK_NICE_VERSION

  gtk_widget_ref(mainw->vol_label);
  gtk_container_remove(GTK_CONTAINER(mainw->vol_label->parent),mainw->vol_label);
  gtk_toolbar_insert(GTK_TOOLBAR(mainw->btoolbar),GTK_TOOL_ITEM(mainw->vol_label),7);
  gtk_widget_unref(mainw->vol_label);
#else
  gtk_scale_button_set_orientation (GTK_SCALE_BUTTON(mainw->volume_scale),GTK_ORIENTATION_HORIZONTAL);
#endif

  gtk_widget_ref(mainw->vol_toolitem);
  gtk_container_remove(GTK_CONTAINER(mainw->vol_toolitem->parent),mainw->vol_toolitem);
  gtk_toolbar_insert(GTK_TOOLBAR(mainw->btoolbar),GTK_TOOL_ITEM(mainw->vol_toolitem),-1);
  gtk_widget_unref(mainw->vol_toolitem);

  gtk_widget_ref(mainw->gens_menu);
  gtk_menu_detach(GTK_MENU(mainw->gens_menu));

  gtk_menu_item_set_submenu (GTK_MENU_ITEM (mainw->gens_submenu), mainw->gens_menu);

  if (mt->mt_frame_preview) {
    if (mainw->plug!=NULL) {
      gtk_container_remove (GTK_CONTAINER(mainw->plug),mainw->image274);
      gtk_widget_destroy (mainw->plug);
      mainw->plug=NULL;
    }

    mainw->playarea = gtk_hbox_new (FALSE,0);

    gtk_container_add (GTK_CONTAINER (mainw->pl_eventbox), mainw->playarea);
    gtk_widget_modify_bg (mainw->playframe, GTK_STATE_NORMAL, &palette->normal_back);
    gtk_widget_show(mainw->playarea);
    gtk_widget_set_app_paintable(mainw->playarea,TRUE);
  }

  if (mt->sepwin_pixbuf!=NULL&&mt->sepwin_pixbuf!=mainw->imframe) gdk_pixbuf_unref(mt->sepwin_pixbuf);
  mt->sepwin_pixbuf=NULL;

  // free our track_rects
  if (cfile->achans>0) {
    delete_audio_tracks(mt,mt->audio_draws,FALSE);
    if (mt->audio_vols!=NULL) g_list_free(mt->audio_vols);
  }

  if (cfile->total_time==0.) close_current_file(mt->file_selected);

  if (mt->video_draws!=NULL) {
    for (i=0;i<mt->num_video_tracks;i++) {
      delete_video_track(mt,i,FALSE);
    }
    g_list_free (mt->video_draws);
  }

  gtk_widget_destroy(mt->in_out_box);
  gtk_widget_destroy(mt->clip_scroll);
  gtk_widget_destroy(mt->fx_base_box);

  g_list_free(mt->tl_marks);

  gtk_widget_destroy (mt->window);

  mainw->multitrack=NULL;
  mainw->event_list=NULL;

  for (i=1;i<MAX_FILES;i++) {
    if (mainw->files[i]!=NULL) {
      if (mainw->files[i]->event_list!=NULL) {
	event_list_free(mainw->files[i]->event_list);
      }
      mainw->files[i]->event_list=NULL;
    }
  }
  if (mainw->current_file>0) sensitize();
  gtk_widget_hide(mainw->playframe);
  mainw->is_rendering=FALSE;
  if (transfer_focus) gtk_window_present(GTK_WINDOW(mainw->LiVES));

  reset_clip_menu();
  mainw->last_dprint_file=-1;
  
  while (g_main_context_iteration(NULL,FALSE));

  if (prefs->show_gui&&prefs->open_maximised) {
    gtk_window_maximize (GTK_WINDOW(mainw->LiVES));
  }


  while (g_main_context_iteration(NULL,FALSE));
  d_print (_ ("\n==============================\nSwitched to Clip Edit mode\n"));

  if (mt->file_selected!=-1) {
    switch_to_file ((mainw->current_file=0),mt->file_selected);
  }

  g_free (mt);

  if (mainw->play_window!=NULL) {
    gchar *xtrabit,*title;
    resize_play_window();
    if (mainw->sepwin_scale!=100.) xtrabit=g_strdup_printf(_(" (%d %% scale)"),(int)mainw->sepwin_scale);
    else xtrabit=g_strdup("");
    title=g_strdup_printf("%s%s",gtk_window_get_title(GTK_WINDOW(mainw->LiVES)),xtrabit);
    gtk_window_set_title(GTK_WINDOW(mainw->play_window),title);
    g_free(title);
    g_free(xtrabit);
  }

#ifdef ENABLE_OSC
  lives_osc_notify(LIVES_OSC_NOTIFY_MODE_CHANGED,(tmp=g_strdup_printf("%d",STARTUP_CE)));
  g_free(tmp);
#endif

  return TRUE;
}



static void locate_avol_init_event(lives_mt *mt, weed_plant_t *event_list, int avol_fx) {
  // once we have detected or assigned our audio volume effect, we search for a FILTER_INIT event for it
  // this becomes our mt->avol_init_event
  int error;
  gchar *filter_hash;
  weed_plant_t *event=get_first_event(event_list);

  while (event!=NULL) {
    if (WEED_EVENT_IS_FILTER_INIT(event)) {
      filter_hash=weed_get_string_value(event,"filter",&error);
      if (avol_fx==weed_get_idx_for_hashname(filter_hash,TRUE)) {
	weed_free(filter_hash);
	mt->avol_init_event=event;
	return;
      }
      weed_free(filter_hash);
    }
    event=get_next_event(event);
  }
}



static track_rect *add_block_start_point (GtkWidget *eventbox, weed_timecode_t tc, gint filenum, 
					  weed_timecode_t offset_start, weed_plant_t *event, gboolean ordered) {
  // each mt->video_draw (eventbox) has a gulong data which points to a linked list of track_rect
  // here we create a new linked list item and set the start timecode in the timeline, 
  // offset in the source file, and start event
  // then append it to our list

  // "block_last" points to the last block added - not the last block in the track !!

  // note: filenum is unused and may be removed in future

  track_rect *block=(track_rect *)g_object_get_data (G_OBJECT(eventbox), "blocks");
  track_rect *new_block=(track_rect *)g_malloc (sizeof(track_rect));

  new_block->next=new_block->prev=NULL;
  new_block->state=BLOCK_UNSELECTED;
  new_block->start_anchored=new_block->end_anchored=FALSE;
  new_block->start_event=event;
  new_block->ordered=ordered;
  new_block->eventbox=eventbox;
  new_block->offset_start=offset_start;

  g_object_set_data (G_OBJECT(eventbox),"block_last",(gpointer)new_block);

  while (block!=NULL) {
    if (get_event_timecode(block->start_event)>tc) {
      // found a block after insertion point
      if (block->prev!=NULL) {
	block->prev->next=new_block;
	new_block->prev=block->prev;
      }
      // add as first block
      else g_object_set_data (G_OBJECT(eventbox),"blocks",(gpointer)new_block);
      new_block->next=block;
      block->prev=new_block;
      break;
    }
    if (block->next==NULL) {
      // add as last block
      block->next=new_block;
      new_block->prev=block;
      break;
    }
    block=block->next;
  }

  // there were no blocks there
  if (block==NULL) g_object_set_data (G_OBJECT(eventbox),"blocks",(gpointer)new_block);

  return new_block;
}


static track_rect *add_block_end_point (GtkWidget *eventbox, weed_plant_t *event) {
  // here we add the end point to our last track_rect
  track_rect *block=(track_rect *)g_object_get_data (G_OBJECT(eventbox),"block_last");
  if (block!=NULL) block->end_event=event;
  return block;
}

static gboolean
on_tlreg_enter (GtkWidget *widget, GdkEventCrossing *event, gpointer user_data) {
  GdkCursor *cursor;
  lives_mt *mt=(lives_mt *)user_data;
  if (mt->cursor_style!=0) return FALSE;
  cursor=gdk_cursor_new_for_display (mt->display, GDK_SB_H_DOUBLE_ARROW);
  gdk_window_set_cursor (widget->window, cursor);
  return FALSE;
}

static gboolean
on_tleb_enter (GtkWidget *widget, GdkEventCrossing *event, gpointer user_data) {
  GdkCursor *cursor;
  lives_mt *mt=(lives_mt *)user_data;
  if (mt->cursor_style!=0) return FALSE;
  cursor=gdk_cursor_new_for_display (mt->display, GDK_CENTER_PTR);
  gdk_window_set_cursor (widget->window, cursor);
  return FALSE;
}




static void reset_renumbering(lives_mt *mt) {
  int i;

  for (i=1;i<=MAX_FILES;i++) {
    if (mainw->files[i]!=NULL) {
      renumbered_clips[i]=i;
    }
    else renumbered_clips[i]=0;
  }
}







void mt_init_tracks (lives_mt *mt, gboolean set_min_max) {
  GtkWidget *label;
  GList *tlist;

  tlist=mt->audio_draws;

  while (mt->audio_draws!=NULL) {
    if (mt->audio_draws->data!=NULL) gtk_widget_destroy((GtkWidget *)mt->audio_draws->data);
    mt->audio_draws=mt->audio_draws->next;
  }

  g_list_free(tlist);

  tlist=mt->video_draws;

  while (mt->video_draws!=NULL) {
    if (mt->video_draws->data!=NULL) gtk_widget_destroy((GtkWidget *)mt->video_draws->data);
    mt->video_draws=mt->video_draws->next;
  }

  g_list_free(tlist);
  mt->num_video_tracks=0;

  if (mt->timeline_table==NULL) {
    label=gtk_label_new (_("Timeline (seconds)"));
    
    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    }
    gtk_table_attach (GTK_TABLE (mt->timeline_table_header), label, 0, 7, 0, 2, GTK_FILL, (GtkAttachOptions)0, 0, 0);
  }

  mt->current_track=0;

  mt->clip_selected=mt_clip_from_file(mt,mt->file_selected);
  mt_clip_select(mt,TRUE);

  if (cfile->achans>0&&mt->opts.back_audio_tracks>0) {
    // start with 1 audio track
    add_audio_track(mt,-1,FALSE);
  }

  // start with 2 video tracks
  add_video_track_behind (NULL,mt);
  add_video_track_behind (NULL,mt);

  mt->current_track=0;
  mt->block_selected=NULL;

  if (mt->timeline_eb==NULL) {
    mt->timeline = gtk_hruler_new();
    mt->timeline_reg=gtk_event_box_new();
    label=gtk_label_new (""); // dummy label
    gtk_container_add (GTK_CONTAINER (mt->timeline_reg), label);

    gtk_widget_show(mt->timeline_reg);

    mt->timeline_eb=gtk_event_box_new();
    gtk_widget_show(mt->timeline_eb);
    
    if (palette->style&STYLE_1) {
      if (palette->style&STYLE_3) {
	gtk_widget_modify_fg (mt->timeline, GTK_STATE_NORMAL, &palette->normal_fore);
	gtk_widget_modify_bg (mt->timeline, GTK_STATE_NORMAL, &palette->normal_back);
	gtk_widget_modify_bg (mt->timeline_eb, GTK_STATE_NORMAL, &palette->menu_and_bars);
	gtk_widget_modify_bg (mt->timeline_reg, GTK_STATE_NORMAL, &palette->menu_and_bars);
      }
    }

    gtk_widget_add_events (mt->timeline_eb, GDK_POINTER_MOTION_MASK | GDK_BUTTON1_MOTION_MASK | GDK_BUTTON_RELEASE_MASK | GDK_BUTTON_PRESS_MASK | GDK_ENTER_NOTIFY);
    gtk_widget_add_events (mt->timeline_reg, GDK_POINTER_MOTION_MASK | GDK_BUTTON1_MOTION_MASK | GDK_BUTTON_RELEASE_MASK | GDK_BUTTON_PRESS_MASK | GDK_ENTER_NOTIFY);
    g_signal_connect (GTK_OBJECT(mt->timeline_eb), "enter-notify-event",G_CALLBACK (on_tleb_enter),(gpointer)mt);
    g_signal_connect (GTK_OBJECT(mt->timeline_reg), "enter-notify-event",G_CALLBACK (on_tlreg_enter),(gpointer)mt);
    
    g_signal_connect (GTK_OBJECT (mt->timeline), "motion_notify_event",
		      G_CALLBACK (return_true),
		      NULL);
    
    g_signal_connect (GTK_OBJECT (mt->timeline_eb), "motion_notify_event",
		      G_CALLBACK (on_timeline_update),
		      (gpointer)mt);
    
    g_signal_connect (GTK_OBJECT (mt->timeline_eb), "button_release_event",
		      G_CALLBACK (on_timeline_release),
		      (gpointer)mt);
    
    g_signal_connect (GTK_OBJECT (mt->timeline_eb), "button_press_event",
		      G_CALLBACK (on_timeline_press),
		      (gpointer)mt);
    
    g_signal_connect (GTK_OBJECT (mt->timeline_reg), "motion_notify_event",
		      G_CALLBACK (on_timeline_update),
		      (gpointer)mt);
    
    g_signal_connect (GTK_OBJECT (mt->timeline_reg), "button_release_event",
		      G_CALLBACK (on_timeline_release),
		      (gpointer)mt);
    
    g_signal_connect (GTK_OBJECT (mt->timeline_reg), "button_press_event",
		      G_CALLBACK (on_timeline_press),
		      (gpointer)mt);
    
    g_signal_connect_after (GTK_OBJECT (mt->timeline_reg), "expose_event",
			    G_CALLBACK (expose_timeline_reg_event),
			    (gpointer)mt);
    
    gtk_container_add (GTK_CONTAINER (mt->timeline_eb), mt->timeline);
    
    gtk_table_attach (GTK_TABLE (mt->timeline_table_header), mt->timeline_eb, 7, 40, 0, 1,
		      (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
		      (GtkAttachOptions) (GTK_FILL), 0, 0);

    gtk_table_attach (GTK_TABLE (mt->timeline_table_header), mt->timeline_reg, 7, 40, 1, 2,
		      (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
		      (GtkAttachOptions) (GTK_FILL), 0, 0);
  }

  if (mt->event_list!=NULL) {
    gint last_tracks=1; // number of video tracks on timeline
    int *frame_index;
    int *clip_index,*new_clip_index,*new_frame_index;
    weed_plant_t *event,*last_event=NULL,*next_frame_event;
    gint num_tracks;
    int tracks[65536]; // TODO - use linked list
    double avels[65536]; // ditto
    int j,error;
    weed_timecode_t tc,last_tc;
    gint last_valid_frame;
    weed_timecode_t offset_start;
    weed_timecode_t block_marker_tc=-1;
    int block_marker_num_tracks=0;
    int *block_marker_tracks=NULL;
    gboolean forced_end;
    weed_timecode_t block_marker_uo_tc=-1;
    int block_marker_uo_num_tracks=0;
    int *block_marker_uo_tracks=NULL;
    gboolean ordered;
    int num_aclips,i;
    int *aclips;
    double *aseeks;
    gboolean shown_audio_warn=FALSE;

    GtkWidget *audio_draw;
    
    GList *slist;

    track_rect *block;

    if (mt->avol_fx!=-1) locate_avol_init_event(mt,mt->event_list,mt->avol_fx);

    for (j=0;j<65536;j++) {
      tracks[j]=0;
      avels[j]=0.;
    }

    // draw coloured blocks to represent the FRAME events
    event=get_first_event(mt->event_list);
    while (event!=NULL) {
      if (WEED_EVENT_IS_MARKER(event)) {
	if (weed_get_int_value(event,"lives_type",&error)==EVENT_MARKER_BLOCK_START) {
	  block_marker_tc=get_event_timecode(event);
	  block_marker_num_tracks=weed_leaf_num_elements(event,"tracks");
	  if (block_marker_tracks!=NULL) weed_free(block_marker_tracks);
	  block_marker_tracks=weed_get_int_array(event,"tracks",&error);
	}
	else if (weed_get_int_value(event,"lives_type",&error)==EVENT_MARKER_BLOCK_UNORDERED) {
	  block_marker_uo_tc=get_event_timecode(event);
	  block_marker_uo_num_tracks=weed_leaf_num_elements(event,"tracks");
	  if (block_marker_uo_tracks!=NULL) weed_free(block_marker_uo_tracks);
	  block_marker_uo_tracks=weed_get_int_array(event,"tracks",&error);
	}
      }
      else if (WEED_EVENT_IS_FRAME(event)) {
	tc=get_event_timecode (event);
	num_tracks=weed_leaf_num_elements (event,"clips");

	clip_index=weed_get_int_array(event,"clips",&error);
	frame_index=weed_get_int_array(event,"frames",&error);

	if (num_tracks<last_tracks) {
	  for (j=num_tracks;j<last_tracks;j++) {
	    // TODO - tracks should be linked list
	    if (tracks[j]>0) {
	      add_block_end_point (GTK_WIDGET(g_list_nth_data(mt->video_draws,j)),last_event); // end of previous rectangle
	      tracks[j]=0;
	    }
	  }
	}

	if (num_tracks>mt->num_video_tracks) {
	  for (j=mt->num_video_tracks;j<num_tracks;j++) {
	    add_video_track_behind(NULL,mt);
	  }
	}

	last_tracks=num_tracks;
	new_clip_index=(int *)g_malloc(num_tracks*sizint);
	new_frame_index=(int *)g_malloc(num_tracks*sizint);
	last_valid_frame=0;

	for (j=0;j<num_tracks;j++) {
	  // TODO - tracks should be linked list
	  if (clip_index[j]>0&&frame_index[j]>-1&&renumbered_clips[clip_index[j]]>0&&frame_index[j]<=
	      mainw->files[renumbered_clips[clip_index[j]]]->frames) {
	    forced_end=FALSE;
	    if (tc==block_marker_tc&&int_array_contains_value(block_marker_tracks,block_marker_num_tracks,j)) 
	      forced_end=TRUE;
	    if ((tracks[j]!=renumbered_clips[clip_index[j]])||forced_end) {
	      // handling fro block end or split blocks
	      if (tracks[j]>0) {
		add_block_end_point (GTK_WIDGET(g_list_nth_data(mt->video_draws,j)),last_event); // end of previous rectangle
	      }
	      if (clip_index[j]>0) {
		ordered=!mainw->unordered_blocks;
		if (tc==block_marker_uo_tc&&int_array_contains_value(block_marker_uo_tracks,block_marker_uo_num_tracks,j)) 
		  ordered=FALSE;
		// start a new rectangle
		offset_start=calc_time_from_frame(renumbered_clips[clip_index[j]],frame_index[j])*U_SEC;
		add_block_start_point (GTK_WIDGET(g_list_nth_data(mt->video_draws,j)),tc,
				       renumbered_clips[clip_index[j]],offset_start,event,ordered);
	      }
	      tracks[j]=renumbered_clips[clip_index[j]];
	    }
	    new_clip_index[j]=renumbered_clips[clip_index[j]];
	    new_frame_index[j]=frame_index[j];
	    last_valid_frame=j+1;
	  }
	  else {
	    // clip has probably been closed, so we remove its frames

	    // TODO - do similar check for audio
	    new_clip_index[j]=-1;
	    new_frame_index[j]=0;
	    if (tracks[j]>0) {
	      add_block_end_point (GTK_WIDGET(g_list_nth_data(mt->video_draws,j)),last_event); // end of previous rectangle
	      tracks[j]=0;
	    }
	  }
	}

	if (last_valid_frame==0) {
	  g_free(new_clip_index);
	  g_free(new_frame_index);
	  new_clip_index=(int *)g_malloc(sizint);
	  new_frame_index=(int *)g_malloc(sizint);
	  *new_clip_index=-1;
	  *new_frame_index=0;
	  num_tracks=1;
	}
	else {
	  if (last_valid_frame<num_tracks) {
	    g_free(new_clip_index);
	    g_free(new_frame_index);
	    new_clip_index=(int *)g_malloc(last_valid_frame*sizint);
	    new_frame_index=(int *)g_malloc(last_valid_frame*sizint);
	    for (j=0;j<last_valid_frame;j++) {
	      new_clip_index[j]=clip_index[j];
	      new_frame_index[j]=frame_index[j];
	    }
	    num_tracks=last_valid_frame;
	  }
	}

	weed_set_int_array(event,"clips",num_tracks,new_clip_index);
	weed_set_int_array(event,"frames",num_tracks,new_frame_index);

	weed_free(clip_index);
	g_free(new_clip_index);
	weed_free(frame_index);
	g_free(new_frame_index);


	if (weed_plant_has_leaf(event,"audio_clips")) {
	  // audio starts or stops here
	  num_aclips=weed_leaf_num_elements(event,"audio_clips");
	  aclips=weed_get_int_array(event,"audio_clips",&error);
	  aseeks=weed_get_double_array(event,"audio_seeks",&error);
	  for (i=0;i<num_aclips;i+=2) {
	    if (aclips[i+1]>0) {
	      if (cfile->achans==0) {
		if (!shown_audio_warn) {
		  shown_audio_warn=TRUE;
		  do_mt_audchan_error(WARN_MASK_MT_ACHANS);
		}
	      }
	      else {
		if (aclips[i]==-1) audio_draw=(GtkWidget *)mt->audio_draws->data;
		else audio_draw=(GtkWidget *)g_list_nth_data(mt->audio_draws,aclips[i]+mt->opts.back_audio_tracks);
		if (avels[aclips[i]+1]!=0.) {
		  add_block_end_point (audio_draw,event);
		}
		//if (renumbered_clips[clip_index[aclips[i+1]]]>0) {
		  avels[aclips[i]+1]=aseeks[i+1];
		  //}
		if (avels[aclips[i]+1]!=0.) {
		  add_block_start_point (audio_draw,tc,renumbered_clips[aclips[i+1]],aseeks[i]*U_SEC,event,TRUE);
		}
	      }
	    }
	    if (aclips[i+1]>0) aclips[i+1]=renumbered_clips[aclips[i+1]];
	  }
	  weed_set_int_array(event,"audio_clips",num_aclips,aclips);
	  weed_free(aclips);
	  weed_free(aseeks);
	}

	num_aclips=g_list_length(mt->audio_draws);
	for (i=0;i<num_aclips;i++) {
	  // handling for split blocks
	  if (tc==block_marker_tc&&int_array_contains_value(block_marker_tracks,block_marker_num_tracks,-i-1)) {
	    audio_draw=(GtkWidget *)g_list_nth_data(mt->audio_draws,i+mt->opts.back_audio_tracks-1);
	    if (avels[i]!=0.) {
	      // end the current block and add a new one
	      // note we only add markers here, when drawing the block audio events will be added
	      block=add_block_end_point (audio_draw,event);
	      if (block!=NULL) {
		last_tc=get_event_timecode(block->start_event);
		offset_start=block->offset_start+(weed_timecode_t)((double)(tc-last_tc)*avels[i]+.5);
		add_block_start_point (audio_draw,tc,-1,offset_start,event,TRUE);
	      }
	    }
	  }
	}

	next_frame_event=get_next_frame_event(event);
      
	if (next_frame_event==NULL) {
	  // this is the last FRAME event, so close all our rectangles
	  for (j=0;j<mt->num_video_tracks;j++) {
	    if (tracks[j]>0) {
	      add_block_end_point (GTK_WIDGET(g_list_nth_data(mt->video_draws,j)),event);
	    }
	  }
	  slist=mt->audio_draws;
	  for (j=0;j<g_list_length(mt->audio_draws);j++) {
	    if (cfile->achans>0&&avels[j]!=0.) add_block_end_point ((GtkWidget *)slist->data,event);

	    slist=slist->next;
	  }
	}
	last_event=event;
      }
      event=get_next_event(event);
    }
    if (!mt->was_undo_redo) remove_end_blank_frames(mt->event_list);
    if (block_marker_tracks!=NULL) weed_free(block_marker_tracks);
    if (block_marker_uo_tracks!=NULL) weed_free(block_marker_uo_tracks);

    if (cfile->achans>0&&mt->opts.back_audio_tracks>0) gtk_widget_show (mt->view_audio);

    if (!mt->was_undo_redo&&mt->avol_fx!=-1&&mt->audio_draws!=NULL) {
      apply_avol_filter(mt);
    }
  }

  mt->end_secs=0.;
  if (mt->event_list!=NULL) {
    mt->end_secs=event_list_get_end_secs(mt->event_list)*2.;
    if (mt->end_secs==0.) LIVES_WARN("got zero length event_list");
  }
  if (mt->end_secs==0.) mt->end_secs=DEF_TIME;

  if (set_min_max) {
    mt->tl_min=0.;
    mt->tl_max=mt->end_secs;
  }

  set_timeline_end_secs(mt,mt->end_secs);

  if (!mt->was_undo_redo) {
    if (mt->is_ready) {
      if (mt->current_track!=0) {
	mt->current_track=0;
	track_select(mt);
      }
      if (mt->region_start!=mt->region_end||mt->region_start!=0.) {
	mt->region_start=mt->region_end=0.;
	draw_region(mt);
      }
    }
    mt_tl_move(mt,-GTK_RULER (mt->timeline)->position);
  }
  else mt->was_undo_redo=FALSE;

  reset_renumbering(mt);

}


void delete_video_track(lives_mt *mt, gint layer, gboolean full) {
  // WARNING - does not yet delete events from event_list
  // only deletes visually

  GtkWidget *eventbox=(GtkWidget *)g_list_nth_data(mt->video_draws,layer);
  track_rect *block=(track_rect *)g_object_get_data (G_OBJECT(eventbox), "blocks"),*blocknext;

  GtkWidget *checkbutton;
  GtkWidget *label,*labelbox,*ahbox,*arrow;
  GdkPixbuf *bgimg,*st_image;

  while (block!=NULL) {
    blocknext=block->next;
    if (mt->block_selected==block) mt->block_selected=NULL;
    g_free(block);
    block=blocknext;
  }

  if ((bgimg=(GdkPixbuf *)g_object_get_data(G_OBJECT(eventbox), "bgimg"))!=NULL) {
    gdk_pixbuf_unref(bgimg);
  }

  if ((st_image=(GdkPixbuf *)g_object_get_data(G_OBJECT(eventbox),"backup_image"))!=NULL) {
    g_object_unref(st_image);
  }

  checkbutton=(GtkWidget *)g_object_get_data (G_OBJECT(eventbox), "checkbutton");
  label=(GtkWidget *)g_object_get_data (G_OBJECT(eventbox), "label");
  arrow=(GtkWidget *)g_object_get_data (G_OBJECT(eventbox), "arrow");

  gtk_widget_destroy(checkbutton);
  gtk_widget_destroy(label);
  gtk_widget_destroy(arrow);
  if (GPOINTER_TO_INT(g_object_get_data(G_OBJECT(eventbox),"hidden"))==0) {
    labelbox=(GtkWidget *)g_object_get_data (G_OBJECT(eventbox), "labelbox");
    ahbox=(GtkWidget *)g_object_get_data (G_OBJECT(eventbox), "ahbox");
    if (labelbox!=NULL) gtk_widget_destroy(labelbox);
    if (ahbox!=NULL) gtk_widget_destroy(ahbox);
  }
  g_free(g_object_get_data(G_OBJECT(eventbox),"track_name"));


  // corresponding audio track will be deleted in delete_audio_track(s)

  gtk_widget_destroy(eventbox);
}


GtkWidget *add_audio_track (lives_mt *mt, gint track, gboolean behind) {
  // add float or pertrack audio track to our timeline_table
  GObject *adj;
  GtkWidget *label;
  GtkWidget *arrow;
  GtkWidget *eventbox;
  GtkWidget *vbox;
  GtkWidget *audio_draw=gtk_event_box_new();
  gchar *pname,*tname;
  gint max_disp_vtracks=mt->max_disp_vtracks-1;
  gint llen,vol=0;
  gint nachans=0;
  int i;

  gtk_widget_ref(audio_draw);

  g_object_set_data (G_OBJECT(audio_draw), "blocks", (gpointer)NULL);
  g_object_set_data (G_OBJECT(audio_draw), "block_last", (gpointer)NULL);
  g_object_set_data (G_OBJECT(audio_draw), "hidden", GINT_TO_POINTER(0));
  g_object_set_data (G_OBJECT(audio_draw), "expanded",GINT_TO_POINTER(FALSE));
  g_object_set_data (G_OBJECT(audio_draw), "bgimg", NULL);
  g_object_set_data (G_OBJECT(audio_draw), "backup_image", NULL);

  GTK_ADJUSTMENT(mt->vadjustment)->upper=(gdouble)mt->num_video_tracks;
  GTK_ADJUSTMENT(mt->vadjustment)->page_size=max_disp_vtracks>mt->num_video_tracks?(gdouble)mt->num_video_tracks:(gdouble)max_disp_vtracks;

  if (track==-1) {
    label=gtk_label_new (_(" Backing audio"));
  }
  else {
    gchar *tmp=g_strdup_printf(_(" Layer %d audio"),track);
    label=gtk_label_new (tmp);
    g_free(tmp);
  }
  gtk_widget_ref(label);

  arrow = gtk_arrow_new (GTK_ARROW_RIGHT, GTK_SHADOW_OUT);
  gtk_widget_set_tooltip_text( arrow, _("Show/hide audio details"));
  gtk_widget_ref(arrow);

  if (palette->style&STYLE_1) {
    if (!(palette->style&STYLE_3)) {
      gtk_widget_modify_fg (label, GTK_STATE_NORMAL, &palette->normal_back);
      gtk_widget_modify_fg (label, GTK_STATE_SELECTED, &palette->normal_back);
    }
  }

  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);

  g_object_set_data (G_OBJECT(audio_draw),"label",label);
  g_object_set_data (G_OBJECT(audio_draw),"arrow",arrow);

  g_object_set_data (G_OBJECT(arrow),"layer_number",GINT_TO_POINTER(track));
  g_object_set_data (G_OBJECT(audio_draw),"layer_number",GINT_TO_POINTER(track));
  g_object_set_data (G_OBJECT(audio_draw),"track_name",g_strdup_printf(_("Layer %d audio"),track));

  // add channel subtracks
  for (i=0;i<cfile->achans;i++) {
    eventbox=gtk_event_box_new();
    gtk_widget_ref(eventbox);
    pname=g_strdup_printf("achan%d",i);
    g_object_set_data(G_OBJECT(audio_draw),pname,eventbox);
    g_free(pname);
    g_object_set_data(G_OBJECT(eventbox),"owner",audio_draw);
    g_object_set_data (G_OBJECT(eventbox), "hidden", GINT_TO_POINTER(0));
    g_object_set_data (G_OBJECT(eventbox), "bgimg", NULL);
    g_object_set_data (G_OBJECT(eventbox), "backup_image", NULL);
  }

  gtk_widget_queue_draw (mt->vpaned);

  if (!mt->was_undo_redo) {
    // calc layer volume value
    llen=g_list_length(mt->audio_draws);
    if (llen==0) {
      // set vol to 1.0
      vol=LIVES_AVOL_SCALE;
    }
    else if (llen==1) {
      if (mt->opts.back_audio_tracks>0) {
	vol=LIVES_AVOL_SCALE/2.;
	mt->audio_vols->data=GINT_TO_POINTER(vol);
      }
      else {
	if (mt->opts.gang_audio) {
	  vol=GPOINTER_TO_INT(g_list_nth_data(mt->audio_vols,0));
	}
	else vol=LIVES_AVOL_SCALE;
      }
    }
    else {
      if (mt->opts.gang_audio) {
	vol=GPOINTER_TO_INT(g_list_nth_data(mt->audio_vols,mt->opts.back_audio_tracks));
      }
      else {
	if (mt->opts.back_audio_tracks>0) {
	  vol=LIVES_AVOL_SCALE/2.;
	}
	else {
	  vol=LIVES_AVOL_SCALE;
	}
      }
    }
  }

  if (!mt->was_undo_redo&&mt->amixer!=NULL&&track>=0) {
    // if mixer is open add space for another slider
    GtkWidget **ch_sliders;
    gulong *ch_slider_fns;

    int j=0;

    nachans=g_list_length(mt->audio_vols)+1;

    ch_sliders=(GtkWidget **)g_malloc(nachans*sizeof(GtkWidget *));
    ch_slider_fns=(gulong *)g_malloc(nachans*sizeof(gulong));
    
    // make a gap
    for (i=0;i<nachans-1;i++) {
      if (!behind&&i==mt->opts.back_audio_tracks) j++;
      ch_sliders[j]=mt->amixer->ch_sliders[i];
      ch_slider_fns[j]=mt->amixer->ch_slider_fns[i];
      j++;
    }

    g_free(mt->amixer->ch_sliders);
    g_free(mt->amixer->ch_slider_fns);

    mt->amixer->ch_sliders=ch_sliders;
    mt->amixer->ch_slider_fns=ch_slider_fns;
  }

  if (track==-1) {
    mt->audio_draws=g_list_prepend(mt->audio_draws,(gpointer)audio_draw);
    if (!mt->was_undo_redo) mt->audio_vols=g_list_prepend(mt->audio_vols,GINT_TO_POINTER(vol));
  }
  else if (behind) {
    mt->audio_draws=g_list_append(mt->audio_draws,(gpointer)audio_draw);

    if (!mt->was_undo_redo) {
      if (mt->amixer!=NULL) {
	// if mixer is open add a new track at end
	vbox=amixer_add_channel_slider(mt,nachans-1-mt->opts.back_audio_tracks);
	gtk_box_pack_start (GTK_BOX (mt->amixer->main_hbox), vbox, FALSE, FALSE, 10);
	gtk_widget_show_all(vbox);
      }

      mt->audio_vols=g_list_append(mt->audio_vols,GINT_TO_POINTER(vol));
    }
  }
  else {
    mt->audio_draws=g_list_insert(mt->audio_draws,(gpointer)audio_draw,mt->opts.back_audio_tracks);
    if (!mt->was_undo_redo) {
      mt->audio_vols=g_list_insert(mt->audio_vols,GINT_TO_POINTER(vol),mt->opts.back_audio_tracks);

      if (mt->amixer!=NULL) {
	// if mixer is open add a new track at posn 0 and update all labels and layer numbers
	vbox=amixer_add_channel_slider(mt,0);

	// pack at posn 2
	gtk_box_pack_start (GTK_BOX (mt->amixer->main_hbox), vbox, FALSE, FALSE, 10);
	gtk_box_reorder_child(GTK_BOX(mt->amixer->main_hbox), vbox, 2);
	gtk_widget_show_all(vbox);

	// update labels and layer numbers

	for (i=mt->opts.back_audio_tracks+1;i<nachans;i++) {
	  label=(GtkWidget *)g_object_get_data(G_OBJECT(mt->amixer->ch_sliders[i]),"label");
	  tname=get_track_name(mt,i-mt->opts.back_audio_tracks,TRUE);
	  gtk_label_set_text(GTK_LABEL(label),tname);
	  g_free(tname);
	  
	  adj=(GObject *)g_object_get_data(G_OBJECT(mt->amixer->ch_sliders[i]),"adj");
	  g_object_set_data(G_OBJECT(adj),"layer",GINT_TO_POINTER(i));
	}

      }
    }

  }

  return audio_draw;
}



void add_video_track (lives_mt *mt, gboolean behind) {
  // add another video track to our timeline_table
  GtkWidget *label;
  GtkWidget *checkbutton;
  GtkWidget *arrow;
  GtkWidget *eventbox;  // each track has an eventbox, which we store in GList *video_draws
  GtkWidget *aeventbox; // each track has optionally an associated audio track, which we store in GList *audio_draws
  int i;
  GList *liste;
  gint max_disp_vtracks=mt->max_disp_vtracks;
  gchar *tmp;

  if (mt->audio_draws!=NULL&&mt->audio_draws->data!=NULL&&mt->opts.back_audio_tracks>0&&GPOINTER_TO_INT(g_object_get_data(G_OBJECT(mt->audio_draws->data),"hidden"))==0) {
    max_disp_vtracks--;
    if (GPOINTER_TO_INT(g_object_get_data(G_OBJECT(mt->audio_draws->data),"expanded"))) max_disp_vtracks-=cfile->achans;
  }

  mt->num_video_tracks++;

#ifdef ENABLE_GIW
  if (!prefs->lamp_buttons) {
#endif
    checkbutton = gtk_check_button_new ();
#ifdef ENABLE_GIW
  }
  else {
    checkbutton=giw_led_new();
    giw_led_enable_mouse(GIW_LED(checkbutton),TRUE);
  }
#endif
  gtk_widget_ref(checkbutton);
  gtk_widget_set_tooltip_text( checkbutton, _("Select track"));

  arrow = gtk_arrow_new (GTK_ARROW_RIGHT, GTK_SHADOW_OUT);
  gtk_widget_set_tooltip_text( arrow, _("Show/hide audio"));
  gtk_widget_ref(arrow);

  eventbox=gtk_event_box_new();
  gtk_widget_ref(eventbox);

  g_object_set_data (G_OBJECT(eventbox),"track_name",g_strdup_printf(_("Video %d"),mt->num_video_tracks));
  g_object_set_data (G_OBJECT(eventbox),"checkbutton",(gpointer)checkbutton);
  g_object_set_data (G_OBJECT(eventbox),"arrow",(gpointer)arrow);
  g_object_set_data (G_OBJECT(eventbox),"expanded",GINT_TO_POINTER(FALSE));

  g_object_set_data (G_OBJECT(eventbox), "blocks", (gpointer)NULL);
  g_object_set_data (G_OBJECT(eventbox), "block_last", (gpointer)NULL);
  g_object_set_data (G_OBJECT(eventbox), "hidden", GINT_TO_POINTER(0));
  g_object_set_data (G_OBJECT(eventbox), "bgimg", NULL);
  g_object_set_data (G_OBJECT(eventbox), "backup_image", NULL);

  GTK_ADJUSTMENT(mt->vadjustment)->page_size=max_disp_vtracks>mt->num_video_tracks?(gdouble)mt->num_video_tracks:(gdouble)max_disp_vtracks;
  GTK_ADJUSTMENT(mt->vadjustment)->upper=(gdouble)mt->num_video_tracks+GTK_ADJUSTMENT(mt->vadjustment)->page_size;

  if (!behind) {
    // track in front of (above) stack
    // shift all rows down
    // increment "layer_number"s, change labels
    g_object_set_data (G_OBJECT(eventbox),"layer_number",GINT_TO_POINTER(0));
    for (i=0;i<mt->num_video_tracks-1;i++) {
      gchar *newtext;
      GtkWidget *xeventbox=(GtkWidget *)g_list_nth_data(mt->video_draws,i);
      GtkWidget *xcheckbutton=(GtkWidget *)g_object_get_data(G_OBJECT(xeventbox),"checkbutton");
      GtkWidget *xarrow=(GtkWidget *)g_object_get_data(G_OBJECT(xeventbox),"arrow");
      g_object_set_data (G_OBJECT(xeventbox),"layer_number",GINT_TO_POINTER(i+1));
      g_object_set_data (G_OBJECT(xcheckbutton),"layer_number",GINT_TO_POINTER(i+1));
      g_object_set_data (G_OBJECT(xarrow),"layer_number",GINT_TO_POINTER(i+1));
      label=GTK_WIDGET(g_object_get_data(G_OBJECT(xeventbox),"label"));
      newtext=g_strdup_printf(_("%s (layer %d)"),g_object_get_data(G_OBJECT(xeventbox),"track_name"),i+1);
      gtk_label_set_text(GTK_LABEL(label),newtext);
      if (mt->opts.pertrack_audio) {
	GtkWidget *aeventbox=(GtkWidget *)g_object_get_data(G_OBJECT(xeventbox),"atrack");
	g_object_set_data (G_OBJECT(aeventbox),"layer_number",GINT_TO_POINTER(i+1));
	xarrow=(GtkWidget *)g_object_get_data(G_OBJECT(aeventbox),"arrow");
	g_object_set_data (G_OBJECT(xarrow),"layer_number",GINT_TO_POINTER(i+1));
	label=(GtkWidget *)g_object_get_data(G_OBJECT(aeventbox),"label");
	g_object_set_data (G_OBJECT(aeventbox),"track_name",g_strdup_printf(_("Layer %d audio"),i+1));
	newtext=g_strdup_printf(_(" %s"),g_object_get_data(G_OBJECT(aeventbox),"track_name"));
	gtk_label_set_text(GTK_LABEL(label),newtext);
      }
    }
    // add a -1,0 in all frame events
    // renumber "in_tacks", "out_tracks" in effect_init events
    event_list_add_track(mt->event_list,0);

    mt->video_draws=g_list_prepend (mt->video_draws, (gpointer)eventbox);
    mt->current_track=0;

    //renumber all tracks in mt->selected_tracks
    liste=mt->selected_tracks;
    while (liste!=NULL) {
      liste->data=GINT_TO_POINTER(GPOINTER_TO_INT(liste->data)+1);
      liste=liste->next;
    }
  }
  else {
    // add track behind (below) stack
    mt->video_draws=g_list_append (mt->video_draws, (gpointer)eventbox);
    g_object_set_data (G_OBJECT(eventbox),"layer_number",GINT_TO_POINTER(mt->num_video_tracks-1));
    g_object_set_data (G_OBJECT(checkbutton),"layer_number",GINT_TO_POINTER(mt->num_video_tracks-1));
    g_object_set_data (G_OBJECT(arrow),"layer_number",GINT_TO_POINTER(mt->num_video_tracks-1));
    mt->current_track=mt->num_video_tracks-1;
  }

  label=gtk_label_new ((tmp=g_strdup_printf(_("%s (layer %d)"),g_object_get_data (G_OBJECT(eventbox),"track_name"),GPOINTER_TO_INT(g_object_get_data(G_OBJECT(eventbox),"layer_number")))));
  g_free(tmp);
  gtk_widget_ref(label);

  if (palette->style&STYLE_1) {
    if (!(palette->style&STYLE_3)) {
      gtk_widget_modify_fg (label, GTK_STATE_NORMAL, &palette->normal_back);
      gtk_widget_modify_fg (label, GTK_STATE_SELECTED, &palette->normal_back);
    }
  }

  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);

  g_object_set_data (G_OBJECT(eventbox),"label",label);

  if (mt->opts.pertrack_audio) {
    aeventbox=add_audio_track(mt,mt->current_track,behind);
    g_object_set_data (G_OBJECT(aeventbox),"owner",eventbox);
    g_object_set_data (G_OBJECT(eventbox),"atrack",aeventbox);
  }

  if (!behind) scroll_track_on_screen(mt,0);
  else scroll_track_on_screen(mt,mt->num_video_tracks-1);

  gtk_widget_queue_draw (mt->vpaned);

}

void add_video_track_behind (GtkMenuItem *menuitem, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  if (menuitem!=NULL) mt_desensitise(mt);
  add_video_track (mt,TRUE);
  if (menuitem!=NULL) mt_sensitise(mt);
}

void add_video_track_front (GtkMenuItem *menuitem, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  if (menuitem!=NULL) mt_desensitise(mt);
  add_video_track (mt,FALSE);
  if (menuitem!=NULL) mt_sensitise(mt);
}


 
void on_mt_fx_edit_activate (GtkMenuItem *menuitem, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  if (mt->selected_init_event==NULL) return;
  fubar(mt);
  polymorph(mt,POLY_PARAMS);
  gtk_widget_set_sensitive(mt->apply_fx_button,FALSE);
}

void mt_avol_quick(GtkMenuItem *menuitem, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  mt->selected_init_event=mt->avol_init_event;
  on_mt_fx_edit_activate(menuitem,user_data);
}


void do_effect_context (lives_mt *mt, GdkEventButton *event) {
  // pop up a context menu when a selected block is right clicked on

  GtkWidget *edit_effect;
  GtkWidget *delete_effect;
  GtkWidget *menu=gtk_menu_new();

  gtk_menu_set_title (GTK_MENU(menu),_("LiVES: Selected effect"));

  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(menu, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }
  
  if (weed_plant_has_leaf(mt->selected_init_event,"in_parameters")) {
    edit_effect = gtk_menu_item_new_with_mnemonic (_("_View/Edit this effect"));
  }
  else {
    edit_effect = gtk_menu_item_new_with_mnemonic (_("_View this effect"));
  }
  gtk_container_add (GTK_CONTAINER (menu), edit_effect);
  
  g_signal_connect (GTK_OBJECT (edit_effect), "activate",
		    G_CALLBACK (on_mt_fx_edit_activate),
		    (gpointer)mt);

  delete_effect = gtk_menu_item_new_with_mnemonic (_("_Delete this effect"));
  if (mt->selected_init_event!=mt->avol_init_event) {
    gtk_container_add (GTK_CONTAINER (menu), delete_effect);
  
    g_signal_connect (GTK_OBJECT (delete_effect), "activate",
		      G_CALLBACK (on_mt_delfx_activate),
		      (gpointer)mt);
  }

  gtk_widget_show_all (menu);
  
  gtk_menu_popup (GTK_MENU(menu), NULL, NULL, NULL, NULL, event->button, event->time);
}




static gboolean
fx_ebox_pressed (GtkWidget *eventbox, GdkEventButton *event, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  GList *children;
  weed_plant_t *osel=mt->selected_init_event;

  if (mt->is_rendering) return FALSE;

  mt->selected_init_event=(weed_plant_t *)g_object_get_data(G_OBJECT(eventbox),"init_event");

  if (event->type==GDK_2BUTTON_PRESS) {
    // double click
    mt->moving_fx=NULL;
    if (mainw->playing_file==-1) on_mt_fx_edit_activate(NULL,mt);
    return FALSE;
  }

  if (mainw->playing_file==-1) {
    if (mt->fx_order!=FX_ORD_NONE) {
      if (osel!=mt->selected_init_event&&osel!=mt->avol_init_event) {
	switch (mt->fx_order) {
	case FX_ORD_BEFORE:
	  /// backup events and note timecode of filter map
	  mt_backup(mt,MT_UNDO_FILTER_MAP_CHANGE,get_event_timecode(mt->fm_edit_event));

	  /// move the init event in the filter map
	  move_init_in_filter_map(mt,mt->event_list,mt->fm_edit_event,osel,mt->selected_init_event,
				  mt->current_track,FALSE);
	  mt->did_backup=FALSE;
	  break;
	case FX_ORD_AFTER:
	  if (init_event_is_process_last(mt->selected_init_event)) {
	    // cannot insert after a process_last effect
	    clear_context(mt);
	    add_context_label(mt,_("Cannot insert after this effect"));
	    mt->selected_init_event=osel;
	    mt->fx_order=FX_ORD_NONE;
	    return FALSE;
	  }

	  /// backup events and note timecode of filter map
	  mt_backup(mt,MT_UNDO_FILTER_MAP_CHANGE,get_event_timecode(mt->fm_edit_event));

	  /// move the init event in the filter map
	  move_init_in_filter_map(mt,mt->event_list,mt->fm_edit_event,osel,mt->selected_init_event,
				  mt->current_track,TRUE);
	  mt->did_backup=FALSE;
	  break;

	default:
	  break;
	}
      }
      
      mt->selected_init_event=osel;
      mt->fx_order=FX_ORD_NONE;
      polymorph(mt,POLY_FX_STACK);
      mt_show_current_frame(mt, FALSE); ///< show updated preview
      return FALSE;
    }
    if (mt->fx_order==FX_ORD_NONE&&WEED_EVENT_IS_FILTER_MAP(mt->fm_edit_event)) {
      if (init_event_is_process_last(mt->selected_init_event)){
	clear_context(mt);
	add_context_label(mt,_("This effect cannot be moved"));
	if (mt->fx_ibefore_button!=NULL) gtk_widget_set_sensitive(mt->fx_ibefore_button,FALSE);
	if (mt->fx_iafter_button!=NULL) gtk_widget_set_sensitive(mt->fx_iafter_button,FALSE);
      }
      else {
	do_fx_move_context(mt);
	if (mt->fx_ibefore_button!=NULL) gtk_widget_set_sensitive(mt->fx_ibefore_button,TRUE);
	if (mt->fx_iafter_button!=NULL) gtk_widget_set_sensitive(mt->fx_iafter_button,TRUE);
      }
    }
    gtk_widget_set_sensitive(mt->fx_edit,TRUE);
    if (mt->selected_init_event!=mt->avol_init_event) gtk_widget_set_sensitive(mt->fx_delete,TRUE);
  }

  // set clicked-on widget to selected state and reset all others
  children=gtk_container_get_children(GTK_CONTAINER(mt->fx_list_vbox));
  while (children!=NULL) {
    GtkWidget *child=(GtkWidget *)children->data;
    if (child!=eventbox) gtk_widget_set_state(child,GTK_STATE_NORMAL);
    else gtk_widget_set_state(child,GTK_STATE_SELECTED);
    children=children->next;
  }

  if (event->button==3&&mainw->playing_file==-1) {
    do_effect_context(mt,event);
  }

  return FALSE;
}




static void set_clip_labels_variable(lives_mt *mt, gint i) {
  gchar *tmp;
  GtkLabel *label1,*label2;
  file *sfile=mainw->files[i];

  if (mt->clip_labels==NULL) return;

  i=mt_clip_from_file(mt,i);
  i*=2;

  label1=(GtkLabel *)g_list_nth_data(mt->clip_labels,i);
  label2=(GtkLabel *)g_list_nth_data(mt->clip_labels,++i);

  gtk_label_set_text(label1,(tmp=g_strdup_printf (_("  %d to %d selected  "),sfile->start,sfile->end)));
  g_free(tmp);

  gtk_label_set_text(label2,(tmp=g_strdup_printf (_("%.2f sec."),(sfile->end-sfile->start+1.)/sfile->fps)));
  g_free(tmp);


}


void mt_clear_timeline(lives_mt *mt) {
  int i;
  gchar *msg;

  for (i=0;i<mt->num_video_tracks;i++) {
    delete_video_track(mt,i,FALSE);
  }
  g_list_free(mt->video_draws);
  mt->video_draws=NULL;
  mt->num_video_tracks=0;
  mainw->event_list=mt->event_list=NULL;

  if (mt->selected_tracks!=NULL) g_list_free(mt->selected_tracks);
  mt->selected_tracks=NULL;

  if (mt->amixer!=NULL) on_amixer_close_clicked(NULL,mt);

  delete_audio_tracks(mt,mt->audio_draws,FALSE);
  mt->audio_draws=NULL;
  if (mt->audio_vols!=NULL) g_list_free(mt->audio_vols);
  mt->audio_vols=NULL;

  mt_init_tracks(mt,TRUE);

  unselect_all(mt);
  mt->changed=FALSE;

  msg=set_values_from_defs(mt,FALSE);

  if (cfile->achans==0) mt->opts.pertrack_audio=FALSE;

  if (msg!=NULL) {
    d_print(msg);
    g_free(msg);
    set_mt_title(mt);
  }

  // reset avol_fx
  if (cfile->achans>0&&mainw->fx_candidates[FX_CANDIDATE_AUDIO_VOL].delegate!=-1) {
    // user (or system) has delegated an audio volume filter from the candidates
    mt->avol_fx=GPOINTER_TO_INT(g_list_nth_data(mainw->fx_candidates[FX_CANDIDATE_AUDIO_VOL].list,mainw->fx_candidates[FX_CANDIDATE_AUDIO_VOL].delegate));
  }
  else mt->avol_fx=-1;
  mt->avol_init_event=NULL;

  add_aparam_menuitems(mt);

  memset(mt->layout_name,0,1);

  mt_show_current_frame(mt, FALSE);
}



void mt_delete_clips(lives_mt *mt, gint file) {
  // close eventbox(es) for a given file
  GList *list=GTK_BOX (mt->clip_inner_box)->children,*list_next;
  GtkBoxChild *child;
  GtkWidget *label1,*label2;
  gint neg=0,i=0;
  gboolean removed=FALSE;

  while (list!=NULL) {
    list_next=list->next;
    if (clips_to_files[i]==file) {
      removed=TRUE;

      if (list->prev!=NULL) list->prev->next=list->next;
      if (list->next!=NULL) list->next->prev=list->prev;

      child=(GtkBoxChild *)list->data;
      gtk_widget_destroy(child->widget);

      label1=(GtkWidget *)g_list_nth_data(mt->clip_labels,i*2);
      label2=(GtkWidget *)g_list_nth_data(mt->clip_labels,i*2+1);

      mt->clip_labels=g_list_remove(mt->clip_labels,label1);
      mt->clip_labels=g_list_remove(mt->clip_labels,label2);

      if (mt->idlefunc>0) g_source_remove(mt->idlefunc);
      while (g_main_context_iteration(NULL,FALSE));
      if (mt->idlefunc>0) {
	mt->idlefunc=0;
	mt->idlefunc=mt_idle_add(mt);
      }

      neg++;
    }
    clips_to_files[i]=clips_to_files[i+neg];
    list=list_next;
    i++;
  }

  if (mt->event_list!=NULL&&used_in_current_layout(mt,file)&&removed) {
    gint current_file=mainw->current_file;

    if (!event_list_rectify(mt,mt->event_list)||get_first_event(mt->event_list)==NULL) {
      // delete the current layout
      mainw->current_file=mt->render_file;
      remove_current_from_affected_layouts(mt);
      mainw->current_file=current_file;
    }
    else {
      mainw->current_file=mt->render_file;
      mt_init_tracks(mt,FALSE);
      mainw->current_file=current_file;
    }
  }

  if (mainw->current_file==-1) {
    gtk_widget_set_sensitive (mt->adjust_start_end, FALSE);
  }

}








void mt_init_clips (lives_mt *mt, gint orig_file, gboolean add) {
  // setup clip boxes in the poly window. if add is TRUE then we are just adding a new clip
  // orig_file is the file we want to select

  // mt_clip_select() should be called after this


  GtkWidget *thumb_image=NULL;
  GtkWidget *vbox, *label;
  GtkWidget *eventbox;
  gchar clip_name[CLIP_LABEL_LENGTH];
  GdkPixbuf *thumbnail;
  int i=1;
  gint width=CLIP_THUMB_WIDTH,height=CLIP_THUMB_HEIGHT;
  gchar filename[PATH_MAX];
  gchar *tmp;
  int count=g_list_length(mt->clip_labels)/2;
  GList *cliplist=mainw->cliplist;

  mt->clip_selected=-1;

  if (add) i=orig_file;

  while (add||cliplist!=NULL) {
    if (add) i=orig_file;
    else i=GPOINTER_TO_INT(cliplist->data);
    if (mainw->files[i]->clip_type!=CLIP_TYPE_DISK&&mainw->files[i]->clip_type!=CLIP_TYPE_FILE) {
      cliplist=cliplist->next;
      continue;
      
    }
    if (i!=mainw->scrap_file&&i!=mainw->ascrap_file) {
      if (i==orig_file||(mt->clip_selected==-1&&i==mainw->pre_src_file)) {
	if (!add) mt->clip_selected=mt_clip_from_file(mt,i);
	else {
	  mt->file_selected=i;
	  mt->clip_selected=count;
	  renumbered_clips[i]=i;
	}
      }
      // make a small thumbnail, add it to the clips box
      thumbnail=make_thumb(mt,i,width,height,mainw->files[i]->start,TRUE);

      eventbox=gtk_event_box_new();
      if (palette->style&STYLE_1) {
	if (palette->style&STYLE_3) {
	  gtk_widget_modify_bg (eventbox, GTK_STATE_SELECTED, &palette->menu_and_bars);
	}
	else gtk_widget_modify_bg (eventbox, GTK_STATE_SELECTED, &palette->normal_back);
      }
      gtk_widget_add_events (eventbox, GDK_BUTTON_RELEASE_MASK | GDK_BUTTON_PRESS_MASK | GDK_ENTER_NOTIFY);
      g_signal_connect (GTK_OBJECT(eventbox), "enter-notify-event",G_CALLBACK (on_clipbox_enter),(gpointer)mt);

      clips_to_files[count]=i;

      vbox = gtk_vbox_new (FALSE, 6);

      thumb_image=gtk_image_new();
      gtk_image_set_from_pixbuf(GTK_IMAGE(thumb_image),thumbnail);
      if (thumbnail!=NULL) gdk_pixbuf_unref(thumbnail);
      gtk_container_add (GTK_CONTAINER (eventbox), vbox);
      gtk_box_pack_start (GTK_BOX (mt->clip_inner_box), eventbox, FALSE, FALSE, 0);
      if (palette->style&STYLE_4) {
	gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &palette->normal_back);
      }

      g_snprintf (filename,PATH_MAX,"%s",(tmp=g_path_get_basename(mainw->files[i]->name)));
      g_free(tmp);
      get_basename(filename);
      g_snprintf (clip_name,CLIP_LABEL_LENGTH,"  %s  ",filename);
      label=gtk_label_new (clip_name);
      if (palette->style&STYLE_3) gtk_widget_modify_fg (label, GTK_STATE_SELECTED, &palette->info_text);
      if (palette->style&STYLE_4) gtk_widget_modify_fg (label, GTK_STATE_NORMAL, &palette->normal_fore);
      gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
      gtk_box_pack_start (GTK_BOX (vbox), thumb_image, FALSE, FALSE, 0);
      
      if (mainw->files[i]->frames>0) {
	gchar *tmp;
	label=gtk_label_new ((tmp=g_strdup_printf (_("%d frames"),mainw->files[i]->frames)));
	g_free(tmp);
	if (palette->style&STYLE_3) gtk_widget_modify_fg (label, GTK_STATE_SELECTED, &palette->info_text);
	if (palette->style&STYLE_4) gtk_widget_modify_fg (label, GTK_STATE_NORMAL, &palette->normal_fore);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

	label=gtk_label_new ("");
	mt->clip_labels=g_list_append(mt->clip_labels,label);

	if (palette->style&STYLE_3) gtk_widget_modify_fg (label, GTK_STATE_SELECTED, &palette->info_text);
	if (palette->style&STYLE_4) gtk_widget_modify_fg (label, GTK_STATE_NORMAL, &palette->normal_fore);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

	label=gtk_label_new ("");
	mt->clip_labels=g_list_append(mt->clip_labels,label);

	if (palette->style&STYLE_3) gtk_widget_modify_fg (label, GTK_STATE_SELECTED, &palette->info_text);
	if (palette->style&STYLE_4) gtk_widget_modify_fg (label, GTK_STATE_NORMAL, &palette->normal_fore);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

	set_clip_labels_variable(mt,i);
      }
      else {
	label=gtk_label_new (g_strdup (_("audio only")));
	if (palette->style&STYLE_3) gtk_widget_modify_fg (label, GTK_STATE_SELECTED, &palette->info_text);
	if (palette->style&STYLE_4) gtk_widget_modify_fg (label, GTK_STATE_NORMAL, &palette->normal_fore);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
	mt->clip_labels=g_list_append(mt->clip_labels,label);

	label=gtk_label_new (g_strdup_printf (_("%.2f sec."),mainw->files[i]->laudio_time));
	if (palette->style&STYLE_3) gtk_widget_modify_fg (label, GTK_STATE_SELECTED, &palette->info_text);
	if (palette->style&STYLE_4) gtk_widget_modify_fg (label, GTK_STATE_NORMAL, &palette->normal_fore);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
	mt->clip_labels=g_list_append(mt->clip_labels,label);
      }
      
      count++;
      
      g_signal_connect (GTK_OBJECT (eventbox), "button_press_event",
			G_CALLBACK (clip_ebox_pressed),
			(gpointer)mt);
      g_signal_connect (GTK_OBJECT (eventbox), "button_release_event",
			G_CALLBACK (on_drag_clip_end),
			(gpointer)mt);
      
      if (add) {
	gtk_widget_show_all(eventbox);
	break;
      }
    }
    cliplist=cliplist->next;
  }
}


gboolean on_multitrack_activate (GtkMenuItem *menuitem, weed_plant_t *event_list) {
  //returns TRUE if we go into mt mode
  gint orig_file;
  gboolean response;
  gboolean transfer_focus=FALSE;
#ifdef ENABLE_OSC
  gchar *tmp;
#endif
  lives_mt *multi;

  xachans=xarate=xasamps=xse=0;
  ptaud=prefs->mt_pertrack_audio;
  btaud=prefs->mt_backaudio;

  if (mainw->frame_layer!=NULL) weed_layer_free(mainw->frame_layer);
  mainw->frame_layer=NULL;

  if (prefs->mt_enter_prompt&&mainw->stored_event_list==NULL&&prefs->show_gui&&!(mainw->recoverable_layout&&prefs->startup_interface==STARTUP_CE)) {
    rdet=create_render_details(3);  // WARNING !! - rdet is global in events.h
    rdet->enc_changed=FALSE;
    gtk_widget_show_all(rdet->always_hbox);
    do {
      rdet->suggestion_followed=FALSE;
      if ((response=gtk_dialog_run(GTK_DIALOG(rdet->dialog)))==GTK_RESPONSE_OK) {
	if (rdet->enc_changed) {
	  check_encoder_restrictions(FALSE,gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(resaudw->aud_checkbutton)),TRUE);
	}
      }
    } while (rdet->suggestion_followed);

    xarate=(gint)atoi (gtk_entry_get_text(GTK_ENTRY(resaudw->entry_arate)));
    xachans=(gint)atoi (gtk_entry_get_text(GTK_ENTRY(resaudw->entry_achans)));
    xasamps=(gint)atoi (gtk_entry_get_text(GTK_ENTRY(resaudw->entry_asamps)));
    
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(resaudw->rb_unsigned))) {
      xse=AFORM_UNSIGNED;;
    }

    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(resaudw->rb_bigend))) {
      xse|=AFORM_BIG_ENDIAN;
    }

    if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(resaudw->aud_checkbutton))) {
      xachans=0;
    }

    ptaud=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(rdet->pertrack_checkbutton));
    btaud=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(rdet->backaudio_checkbutton));

    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(rdet->always_checkbutton))) {
      prefs->mt_enter_prompt=FALSE;
      set_boolean_pref("mt_enter_prompt",prefs->mt_enter_prompt);
      prefs->mt_def_width=rdet->width;
      set_int_pref("mt_def_width",prefs->mt_def_width);
      prefs->mt_def_height=rdet->height;
      set_int_pref("mt_def_height",prefs->mt_def_height);
      prefs->mt_def_fps=rdet->fps;
      set_double_pref("mt_def_fps",prefs->mt_def_fps);
      prefs->mt_def_arate=xarate;
      set_int_pref("mt_def_arate",prefs->mt_def_arate);
      prefs->mt_def_achans=xachans;
      set_int_pref("mt_def_achans",prefs->mt_def_achans);
      prefs->mt_def_asamps=xasamps;
      set_int_pref("mt_def_asamps",prefs->mt_def_asamps);
      prefs->mt_def_signed_endian=xse;
      set_int_pref("mt_def_signed_endian",prefs->mt_def_signed_endian);
      prefs->mt_pertrack_audio=ptaud;
      set_boolean_pref("mt_pertrack_audio",prefs->mt_pertrack_audio);
      prefs->mt_backaudio=btaud;
      set_int_pref("mt_backaudio",prefs->mt_backaudio);
    }
    else {
      if (!prefs->mt_enter_prompt) {
	prefs->mt_enter_prompt=TRUE;
	set_boolean_pref("mt_enter_prompt",prefs->mt_enter_prompt);
      }
    }

    if (gtk_window_has_toplevel_focus(GTK_WINDOW(rdet->dialog))) transfer_focus=TRUE;
    gtk_widget_destroy (rdet->dialog); 
    
    if (response==GTK_RESPONSE_CANCEL) {
      g_free(rdet->encoder_name);
      g_free(rdet);
      rdet=NULL;
      if (resaudw!=NULL) g_free(resaudw);
      resaudw=NULL;
      return FALSE;
    }
  }

  if (mainw->current_file>-1&&cfile!=NULL&&cfile->clip_type==CLIP_TYPE_GENERATOR) {
    weed_generator_end ((weed_plant_t *)cfile->ext_src);
  }

  if (prefs->show_gui) {
    while (g_main_context_iteration(NULL,FALSE));
  }

  // create new file for rendering to
  renumber_clips();
  orig_file=mainw->current_file;
  mainw->current_file=mainw->first_free_file;

  if (!get_new_handle(mainw->current_file,NULL)) {
    mainw->current_file=orig_file;
    return FALSE; // show dialog again
  }

  cfile->bpp=cfile->img_type==IMG_TYPE_JPEG?24:32;
  cfile->changed=TRUE;
  cfile->is_loaded=TRUE;
  
  cfile->old_frames=cfile->frames;

  force_pertrack_audio=FALSE;
  force_backing_tracks=0;

  if (mainw->stored_event_list!=NULL) {
    event_list=mainw->stored_event_list;
    rerenumber_clips(NULL);
  }
  
  if (prefs->show_gui) {
    // must check this before event_list_rectify, since it can throw error dialogs
    if (gtk_window_has_toplevel_focus(GTK_WINDOW(mainw->LiVES))) transfer_focus=TRUE;
  }

  // if we have an existing event list, we will quantise it to the selected fps
  if (event_list!=NULL) {
    weed_plant_t *qevent_list=quantise_events(event_list,cfile->fps,FALSE);
    if (qevent_list==NULL) return FALSE; // memory error
    event_list_replace_events(event_list,qevent_list);
    weed_set_double_value(event_list,"fps",cfile->fps);
    event_list_rectify(NULL,event_list);
  }

  // in case we are starting up in mt mode
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


  if (prefs->show_gui) block_expose();

  multi=multitrack(event_list,orig_file,cfile->fps);

  if (mainw->stored_event_list!=NULL) {
    mainw->stored_event_list=NULL;
    mainw->stored_layout_undos=NULL;
    mainw->sl_undo_mem=NULL;
    stored_event_list_free_all(FALSE);
    if (multi->event_list==NULL) {
      multi->clip_selected=mt_clip_from_file(multi,orig_file);
      multi->file_selected=orig_file;
      if (prefs->show_gui) unblock_expose();
      return FALSE;
    }
    remove_markers(multi->event_list);
  }

  if (mainw->recoverable_layout&&multi->event_list==NULL&&prefs->startup_interface==STARTUP_CE) {
    // failed to load recovery layout
    multi->clip_selected=mt_clip_from_file(multi,orig_file);
    multi->file_selected=orig_file;
    return FALSE;
  }

  if (prefs->show_gui) {
    gtk_widget_show_all (multi->window);

    if (!prefs->show_recent) {
      gtk_widget_hide (multi->recent_menu);
    }
    if (multi->nb_label!=NULL) {
      gtk_widget_hide (multi->poly_box);
      gtk_widget_queue_resize(multi->nb_label);
    }
    if (!mainw->has_custom_gens) {
      gtk_widget_hide(mainw->custom_gens_menu);
      gtk_widget_hide(mainw->custom_gens_submenu);
    }
    gtk_widget_hide (mainw->LiVES);
  }

  gtk_widget_hide(multi->aparam_separator); // no longer used
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mainw->sticky),FALSE);

  if (cfile->achans==0) {
    gtk_widget_hide(multi->render_sep);
    gtk_widget_hide(multi->render_vid);
    gtk_widget_hide(multi->render_aud);
    gtk_widget_hide(multi->normalise_aud);
    gtk_widget_hide(multi->view_audio);
    gtk_widget_hide(multi->aparam_menuitem);
    gtk_widget_hide(multi->aparam_separator);
    multi->opts.pertrack_audio=FALSE;
  }

  if (prefs->audio_player!=AUD_PLAYER_JACK&&prefs->audio_player!=AUD_PLAYER_PULSE) {
    gtk_widget_hide(mainw->vol_toolitem);
    gtk_widget_hide(mainw->vol_label);
  }

  if (!multi->opts.show_ctx) {
    gtk_widget_hide(multi->context_frame);
    gtk_widget_hide(mainw->scrolledwindow);
    gtk_widget_hide(multi->sep_image);
  }

  if (!multi->opts.pertrack_audio) {
    gtk_widget_hide(multi->insa_eventbox);
    gtk_widget_hide(multi->insa_checkbutton);
  }

  if (multi->opts.back_audio_tracks==0) gtk_widget_hide(multi->view_audio);

  if (cfile->achans>0) add_aparam_menuitems(multi);

  track_select(multi);
  mt_clip_select(multi,TRUE);  // call this again to scroll clip on screen

  if (mainw->preview_box!=NULL&&mainw->preview_box->parent!=NULL) {
    g_object_unref(mainw->preview_box);
    gtk_container_remove (GTK_CONTAINER (mainw->play_window), mainw->preview_box);
    mainw->preview_box=NULL;
  }

  if (mainw->play_window!=NULL) {
    gchar *title,*xtrabit;
    g_signal_handlers_block_matched(mainw->play_window,(GSignalMatchType)(G_SIGNAL_MATCH_FUNC|G_SIGNAL_MATCH_UNBLOCKED),
				    0,0,0,(gpointer)expose_play_window,NULL);
    g_signal_handler_unblock(mainw->play_window,mainw->pw_exp_func);
    mainw->pw_exp_is_blocked=FALSE;
    gtk_window_remove_accel_group (GTK_WINDOW (mainw->play_window), mainw->accel_group);
    gtk_window_add_accel_group (GTK_WINDOW (mainw->play_window), multi->accel_group);

    resize_play_window();
    if (mainw->sepwin_scale!=100.) xtrabit=g_strdup_printf(_(" (%d %% scale)"),(int)mainw->sepwin_scale);
    else xtrabit=g_strdup("");
    title=g_strdup_printf("%s%s",gtk_window_get_title(GTK_WINDOW(multi->window)),xtrabit);
    gtk_window_set_title(GTK_WINDOW(mainw->play_window),title);
    g_free(title);
    g_free(xtrabit);


  }
  d_print (_ ("\n==============================\nSwitched to Multitrack mode\n"));


  if (cfile->achans>0&&prefs->audio_player!=AUD_PLAYER_JACK&&prefs->audio_player!=AUD_PLAYER_PULSE) {
    do_mt_no_jack_error(WARN_MASK_MT_NO_JACK);
  }

  if (prefs->gui_monitor!=0) {
    gint xcen=mainw->mgeom[prefs->gui_monitor-1].x+(mainw->mgeom[prefs->gui_monitor-1].width-multi->window->allocation.width)/2;
    gint ycen=mainw->mgeom[prefs->gui_monitor-1].y+(mainw->mgeom[prefs->gui_monitor-1].height-multi->window->allocation.height)/2;
    gtk_window_set_screen(GTK_WINDOW(multi->window),mainw->mgeom[prefs->gui_monitor-1].screen);
    gtk_window_move(GTK_WINDOW(multi->window),xcen,ycen);
  }


  if ((prefs->gui_monitor!=0||capable->nmonitors<=1)&&prefs->open_maximised) {
    gtk_window_maximize (GTK_WINDOW(multi->window));
  }

  multi->is_ready=FALSE;
  mt_show_current_frame(multi,FALSE);
  multi->is_ready=TRUE;

  if (transfer_focus) gtk_window_present(GTK_WINDOW(multi->window));

  if (multi->idlefunc==0) {
    multi->idlefunc=mt_idle_add(multi);
  }

  mainw_was_ready=mainw->is_ready;
  mainw->is_ready=TRUE;

#ifdef ENABLE_OSC
  lives_osc_notify(LIVES_OSC_NOTIFY_MODE_CHANGED,(tmp=g_strdup_printf("%d",STARTUP_MT)));
  g_free(tmp);
#endif

  return TRUE;
}


gboolean block_overlap (GtkWidget *eventbox, gdouble time_start, gdouble time_end) {
  weed_timecode_t tc_start=time_start*U_SECL;
  weed_timecode_t tc_end=time_end*U_SECL;
  track_rect *block=(track_rect *)g_object_get_data (G_OBJECT(eventbox),"blocks");

  while (block!=NULL) {
    if (get_event_timecode(block->start_event)>tc_end) return FALSE;
    if (get_event_timecode(block->end_event)>=tc_start) return TRUE;
    block=block->next;
  }
  return FALSE;
}  




static track_rect *get_block_before (GtkWidget *eventbox, gdouble time, gboolean allow_cur) {
  // get the last block which ends before or at time
  // if allow_cur is TRUE, we may count blocks whose end is after "time" but whose start is 
  // before or at time

  weed_timecode_t tc=time*U_SECL;
  track_rect *block=(track_rect *)g_object_get_data (G_OBJECT(eventbox),"blocks"),*last_block=NULL;

  while (block!=NULL) {
    if ((allow_cur&&get_event_timecode(block->start_event)>=tc)||(!allow_cur&&get_event_timecode(block->end_event)>=tc)) break;
    last_block=block;
    block=block->next;
  }
  return last_block;
}

static track_rect *get_block_after (GtkWidget *eventbox, gdouble time, gboolean allow_cur) {
  // return the first block which starts at or after time
  // if allow_cur is TRUE, we may count blocks whose end is after "time" but whose start is 
  // before or at time

  weed_timecode_t tc=time*U_SECL;
  track_rect *block=(track_rect *)g_object_get_data (G_OBJECT(eventbox),"blocks");

  while (block!=NULL) {
    if (get_event_timecode(block->start_event)>=tc||(allow_cur&&get_event_timecode(block->end_event)>=tc)) break;
    block=block->next;
  }
  return block;
}


static track_rect *move_block (lives_mt *mt, track_rect *block, gdouble timesecs, gint old_track, gint new_track) {
  weed_timecode_t new_start_tc,end_tc;
  weed_timecode_t start_tc=get_event_timecode(block->start_event);
  GtkWidget *eventbox,*oeventbox;
  gint clip,current_track=-1;
  gboolean did_backup=mt->did_backup;

  if (!did_backup&&mt->idlefunc>0) {
    g_source_remove(mt->idlefunc);
    mt->idlefunc=0;
  }

  if (!did_backup) {
    if (old_track==-1) mt_backup(mt,MT_UNDO_MOVE_AUDIO_BLOCK,0);
    else mt_backup(mt,MT_UNDO_MOVE_BLOCK,0);
  }

  if (is_audio_eventbox(mt,block->eventbox)&&(oeventbox=(GtkWidget *)g_object_get_data(G_OBJECT(block->eventbox),"owner"))!=NULL) {
    // if moving an audio block we move the associated video block first
    block=get_block_from_time(oeventbox,start_tc/U_SEC,mt);
  }

  mt->block_selected=block;
  end_tc=get_event_timecode(block->end_event);
  
  mt->specific_event=get_prev_event(block->start_event);
  while (mt->specific_event!=NULL&&get_event_timecode(mt->specific_event)==start_tc) {
    mt->specific_event=get_prev_event(mt->specific_event);
  }

  if (old_track>-1) {
    clip=get_frame_event_clip(block->start_event,old_track);
    mt->insert_start=block->offset_start;
    mt->insert_end=block->offset_start+end_tc-start_tc+q_gint64(U_SEC/mt->fps,mt->fps);
  }
  else {
    clip=get_audio_frame_clip(block->start_event,old_track);
    mt->insert_avel=get_audio_frame_vel(block->start_event,old_track);
    mt->insert_start=q_gint64(get_audio_frame_seek(block->start_event,old_track)*U_SEC,mt->fps);
    mt->insert_end=q_gint64(mt->insert_start+(end_tc-start_tc),mt->fps);
  }

  mt->moving_block=TRUE;
  mt->current_track=old_track;
  delete_block_cb(NULL,(gpointer)mt);
  mt->block_selected=NULL;
  mt->current_track=new_track;
  track_select(mt);
  mt->clip_selected=mt_clip_from_file(mt,clip);
  mt_clip_select(mt,TRUE);
  mt_tl_move(mt,timesecs-GTK_RULER (mt->timeline)->position);

  if (new_track!=-1) insert_here_cb(NULL,(gpointer)mt);
  else {
    insert_audio_here_cb(NULL,(gpointer)mt);
    mt->insert_avel=1.;
  }

  mt->insert_start=mt->insert_end=-1;

  new_start_tc=q_gint64(timesecs*U_SEC,mt->fps);
  if (mt->opts.move_effects) update_filter_events(mt,mt->specific_event,start_tc,end_tc,old_track,new_start_tc,mt->current_track);
  mt->moving_block=FALSE;
  mt->specific_event=NULL;

  if (new_track!=-1) eventbox=(GtkWidget *)g_list_nth_data(mt->video_draws,mt->current_track);
  else eventbox=(GtkWidget *)mt->audio_draws->data;
  block = (track_rect *)g_object_get_data(G_OBJECT(eventbox),"block_last");
  
  remove_end_blank_frames(mt->event_list);

  if (block!=NULL&&(mt->opts.grav_mode==GRAV_MODE_LEFT||(mt->opts.grav_mode==GRAV_MODE_RIGHT&&block->next!=NULL))&&!did_backup) {
    gdouble oldr_start=mt->region_start;
    gdouble oldr_end=mt->region_end;
    GList *tracks_sel=NULL;
    track_rect *lblock;
    gdouble rtc=get_event_timecode(block->start_event)/U_SEC-1./mt->fps,rstart=0.,rend;

    if (mt->opts.grav_mode==GRAV_MODE_LEFT) {
      // gravity left - move left until we hit another block or time 0
      if (rtc>=0.) {
	lblock=block->prev;
	if (lblock!=NULL) rstart=get_event_timecode(lblock->end_event)/U_SEC;
      }
      rend=get_event_timecode(block->end_event)/U_SEC;
    }
    else {
      // gravity right - move right until we hit the next block
      lblock=block->next;
      rstart=get_event_timecode(block->start_event)/U_SEC;
      rend=get_event_timecode(lblock->start_event)/U_SEC;
    }

    mt->region_start=rstart;
    mt->region_end=rend;

    if (new_track>-1) {
      tracks_sel=g_list_copy(mt->selected_tracks);
      if (mt->selected_tracks!=NULL) g_list_free(mt->selected_tracks);
      mt->selected_tracks=NULL;
      mt->selected_tracks=g_list_append(mt->selected_tracks,GINT_TO_POINTER(new_track));
     }
    else {
      current_track=mt->current_track;
      mt->current_track=old_track;
    }

    remove_first_gaps(NULL,mt);
    if (old_track>-1) {
      g_list_free(mt->selected_tracks);
      mt->selected_tracks=g_list_copy(tracks_sel);
      if (tracks_sel!=NULL) g_list_free(tracks_sel);
    }
    else mt->current_track=current_track;
    mt->region_start=oldr_start;
    mt->region_end=oldr_end;
    mt_sensitise(mt);
  }

  // get this again because it could have moved
  block = (track_rect *)g_object_get_data(G_OBJECT(eventbox),"block_last");

  if (!did_backup) {
    if (mt->avol_fx!=-1&&(block==NULL||block->next==NULL)&&mt->audio_draws!=NULL&&mt->audio_draws->data!=NULL&&get_first_event(mt->event_list)!=NULL) {
      apply_avol_filter(mt);
    }
  }

  mt->did_backup=did_backup;

  if (!did_backup&&mt->framedraw!=NULL&&mt->current_rfx!=NULL&&mt->init_event!=NULL&&mt->poly_state==POLY_PARAMS&&weed_plant_has_leaf(mt->init_event,"in_tracks")) {
    weed_timecode_t tc=q_gint64(gtk_spin_button_get_value(GTK_SPIN_BUTTON(mt->node_spinbutton))*U_SEC+get_event_timecode(mt->init_event),mt->fps);
    get_track_index(mt,tc);
  }

  if (!did_backup) mt->idlefunc=mt_idle_add(mt);

  return block;
}


void unselect_all (lives_mt *mt) {
  // unselect all blocks 
  int i;
  GtkWidget *eventbox;
  track_rect *trec;

  if (mt->block_selected!=NULL) gtk_widget_queue_draw(mt->block_selected->eventbox);

  if (cfile->achans>0) {
    for (i=0;i<g_list_length(mt->audio_draws);i++) {
      eventbox=(GtkWidget *)g_list_nth_data(mt->audio_draws,i);
      if (eventbox!=NULL) {
	trec=(track_rect *)g_object_get_data (G_OBJECT(eventbox),"blocks");
	while (trec!=NULL) {
	  trec->state=BLOCK_UNSELECTED;
	  trec=trec->next;
	}
      }
    }
  }

  for (i=0;i<mt->num_video_tracks;i++) {
    eventbox=(GtkWidget *)g_list_nth_data(mt->video_draws,i);
    if (eventbox!=NULL) {
      trec=(track_rect *)g_object_get_data (G_OBJECT(eventbox),"blocks");
      while (trec!=NULL) {
	trec->state=BLOCK_UNSELECTED;
	trec=trec->next;
      }
    }
  }
  mt->block_selected=NULL;
  gtk_widget_set_sensitive(mt->view_in_out,FALSE);
  gtk_widget_set_sensitive (mt->delblock, FALSE);
  gtk_widget_set_sensitive (mt->fx_block, FALSE);
  if (mt->poly_state!=POLY_FX_STACK) polymorph (mt,POLY_CLIPS);
}


void clear_context (lives_mt *mt) {
  if (mt->context_scroll!=NULL) {
    gtk_widget_destroy (mt->context_scroll);
  }

  mt->context_scroll=gtk_scrolled_window_new (NULL, NULL);

  gtk_container_add (GTK_CONTAINER (mt->context_frame), mt->context_scroll);

  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (mt->context_scroll), GTK_POLICY_AUTOMATIC, 
				  GTK_POLICY_AUTOMATIC);
  
  mt->context_box = gtk_vbox_new (FALSE, 4);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(mt->context_box, GTK_STATE_NORMAL, &palette->info_base);
  }

  gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (mt->context_scroll), mt->context_box);

  // Apply theme background to scrolled window
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(GTK_BIN(mt->context_scroll)->child, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg(GTK_BIN(mt->context_scroll)->child, GTK_STATE_NORMAL, &palette->normal_back);
  }

  if (mt->opts.show_ctx) gtk_widget_show_all (mt->context_frame);
}


void add_context_label (lives_mt *mt, const gchar *text) {
  // WARNING - do not add > 8 lines of text (including newlines) - otherwise the window can get resized

  GtkWidget *label=gtk_label_new (text);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_CENTER);
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (mt->context_box), label, FALSE, FALSE, 0);
}



gboolean resize_timeline (lives_mt *mt) {
  gdouble end_secs;

  if (mt->event_list==NULL||get_first_event(mt->event_list)==NULL||mt->tl_fixed_length>0.) return FALSE;

  end_secs=event_list_get_end_secs(mt->event_list);

  if (end_secs>mt->end_secs) {
    set_timeline_end_secs(mt,end_secs);
    return TRUE;
  }

  redraw_all_event_boxes(mt);

  return FALSE;
}



static void set_in_out_spin_ranges(lives_mt *mt, weed_timecode_t start_tc, weed_timecode_t end_tc) {
  track_rect *block=mt->block_selected;
  weed_timecode_t min_tc=0,max_tc=-1;
  weed_timecode_t offset_start=get_event_timecode(block->start_event);
  gint filenum;
  gdouble in_val=start_tc/U_SEC,out_val=end_tc/U_SEC,in_start_range=0.,out_start_range=in_val+1./mt->fps;
  gdouble out_end_range,real_out_end_range;
  gdouble in_end_range=out_val-1./mt->fps,real_in_start_range=in_start_range;
  gdouble avel=1.;

  gint track=GPOINTER_TO_INT(g_object_get_data(G_OBJECT(block->eventbox),"layer_number"));

  g_signal_handler_block (mt->spinbutton_out,mt->spin_out_func);
  g_signal_handler_block (mt->spinbutton_in,mt->spin_in_func);

  if (block->prev!=NULL) min_tc=get_event_timecode(block->prev->end_event)+(gdouble)(track>=0)*U_SEC/mt->fps;
  if (block->next!=NULL) max_tc=get_event_timecode(block->next->start_event)-(gdouble)(track>=0)*U_SEC/mt->fps;

  if (track>=0) {
    filenum=get_frame_event_clip(block->start_event,track);

    // actually we should quantise this to the mt->fps, but we leave it in case clip has only 
    // one frame -> otherwise we could quantise to zero frames
    out_end_range=count_resampled_frames(mainw->files[filenum]->frames,mainw->files[filenum]->fps,mt->fps)/mt->fps;
  }
  else {
    filenum=get_audio_frame_clip(block->start_event,track);
    out_end_range=q_gint64(mainw->files[filenum]->laudio_time*U_SEC,mt->fps)/U_SEC;
    avel=get_audio_frame_vel(block->start_event,track);
  }
  real_out_end_range=out_end_range;

  if (mt->opts.insert_mode!=INSERT_MODE_OVERWRITE) {
    if (!block->end_anchored&&max_tc>-1&&(((max_tc-offset_start)/U_SEC*ABS(avel)+in_val)<out_end_range)) real_out_end_range=q_gint64((max_tc-offset_start)*ABS(avel)+in_val*U_SEC,mt->fps)/U_SEC;
    if (!block->start_anchored&&min_tc>-1&&(((min_tc-offset_start)/U_SEC*ABS(avel)+in_val)>in_start_range)) real_in_start_range=q_gint64((min_tc-offset_start)*ABS(avel)+in_val*U_SEC,mt->fps)/U_SEC;
    if (!block->start_anchored) out_end_range=real_out_end_range; 
    if (!block->end_anchored) in_start_range=real_in_start_range; 
  }
  
  if (block->end_anchored&&(out_val-in_val>out_start_range)) out_start_range=in_start_range+out_val-in_val;
  if (block->start_anchored&&(out_end_range-out_val+in_val)<in_end_range) in_end_range=out_end_range-out_val+in_val;
  
  if (avel>0.) {
    gtk_spin_button_set_range(GTK_SPIN_BUTTON(mt->spinbutton_out), out_start_range, real_out_end_range);
    gtk_spin_button_set_range(GTK_SPIN_BUTTON(mt->spinbutton_in), real_in_start_range, in_end_range);
  }
  else {
    gtk_spin_button_set_range(GTK_SPIN_BUTTON(mt->spinbutton_in), out_start_range, real_out_end_range);
    gtk_spin_button_set_range(GTK_SPIN_BUTTON(mt->spinbutton_out), real_in_start_range, in_end_range);
  }

  g_signal_handler_unblock (mt->spinbutton_out,mt->spin_out_func);
  g_signal_handler_unblock (mt->spinbutton_in,mt->spin_in_func);

}

static void update_in_image(lives_mt *mt) {
  GdkPixbuf *thumb;
  track_rect *block=mt->block_selected;
  gint track;
  gint filenum;
  gint frame_start;
  gint width=cfile->hsize;
  gint height=cfile->vsize;

  if (block!=NULL) {
    track=GPOINTER_TO_INT(g_object_get_data(G_OBJECT(block->eventbox),"layer_number"));
    filenum=get_frame_event_clip(block->start_event,track);
    frame_start=calc_frame_from_time(filenum,block->offset_start/U_SEC);
  }
  else {
    filenum=mt->file_selected;
    frame_start=mainw->files[filenum]->start;
  }

  calc_maxspect(mt->poly_box->allocation.width/2-IN_OUT_SEP,mt->poly_box->allocation.height-
		((block==NULL||block->ordered)?mainw->spinbutton_start->allocation.height:0),&width,&height);

  thumb=make_thumb(mt,filenum,width,height,frame_start,FALSE);
  gtk_image_set_from_pixbuf (GTK_IMAGE(mt->in_image),thumb);
  if (thumb!=NULL) gdk_pixbuf_unref(thumb);
}


static void update_out_image(lives_mt *mt, weed_timecode_t end_tc) {
  GdkPixbuf *thumb;
  track_rect *block=mt->block_selected;
  gint track;
  gint filenum;
  gint frame_end;
  gint width=cfile->hsize;
  gint height=cfile->vsize;
  
  if (block!=NULL) {
    track=GPOINTER_TO_INT(g_object_get_data(G_OBJECT(block->eventbox),"layer_number"));
    filenum=get_frame_event_clip(block->start_event,track);
    frame_end=calc_frame_from_time(filenum,end_tc/U_SEC-1./mt->fps);
  }
  else {
    filenum=mt->file_selected;
    frame_end=mainw->files[filenum]->end;
  }

  calc_maxspect(mt->poly_box->allocation.width/2-IN_OUT_SEP,mt->poly_box->allocation.height-
		((block==NULL||block->ordered)?mainw->spinbutton_start->allocation.height:0),&width,&height);

  thumb=make_thumb(mt,filenum,width,height,frame_end,FALSE);
  gtk_image_set_from_pixbuf (GTK_IMAGE(mt->out_image),thumb);
  if (thumb!=NULL) gdk_pixbuf_unref(thumb);
}




void
in_out_start_changed (GtkWidget *widget, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  track_rect *block=mt->block_selected,*ablock=NULL;
  gdouble new_start;
  weed_plant_t *event;
  weed_timecode_t new_start_tc,orig_start_tc,offset_end,tl_start;
  gint track;
  gint filenum;
  weed_plant_t *start_event=NULL,*event_next;
  gboolean was_moved;
  weed_timecode_t new_tl_tc;

  int aclip=0;
  double avel=1.,aseek=0.;

  gboolean start_anchored;

  if (block==NULL) {
    file *sfile=mainw->files[mt->file_selected];
    sfile->start=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
    set_clip_labels_variable(mt,mt->file_selected);
    update_in_image(mt);

    if (sfile->end<sfile->start) gtk_spin_button_set_value(GTK_SPIN_BUTTON(mt->spinbutton_out),(gdouble)sfile->start);
    return;
  }

  new_start=gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget));

  event=block->start_event;
  orig_start_tc=block->offset_start;
  track=GPOINTER_TO_INT(g_object_get_data(G_OBJECT(block->eventbox),"layer_number"));
  new_start_tc=q_dbl(new_start,mt->fps);

  if (new_start_tc==orig_start_tc||!block->ordered) {
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(mt->spinbutton_in),new_start_tc/U_SEC);
    return;
  }

  tl_start=get_event_timecode(event);
  if (track>=0) {
    if (!mt->aud_track_selected) {
      if (mt->opts.pertrack_audio) {
	GtkWidget *aeventbox=GTK_WIDGET(g_object_get_data(G_OBJECT(block->eventbox),"atrack"));
	ablock=get_block_from_time(aeventbox,tl_start/U_SEC,mt);
      }
      start_anchored=block->start_anchored;
    }
    else {
      GtkWidget *eventbox=GTK_WIDGET(g_object_get_data(G_OBJECT(block->eventbox),"owner"));
      ablock=block;
      block=get_block_from_time(eventbox,tl_start/U_SEC,mt);
      start_anchored=ablock->start_anchored;
    }
    filenum=get_frame_event_clip(block->start_event,track);
  }
  else {
    ablock=block;
    start_anchored=block->start_anchored;
    avel=get_audio_frame_vel(ablock->start_event,track);
    filenum=get_audio_frame_clip(ablock->start_event,track);
  }

  if (!start_anchored) {
    if (new_start_tc>block->offset_start) {
      start_event=get_prev_frame_event(block->start_event);

      // start increased, not anchored
      while (event!=NULL) {
	if ((get_event_timecode(event)-tl_start)>=(new_start_tc-block->offset_start)/avel) {
	  if (event==block->end_event) return;
	  // done
	  if (ablock!=NULL) {
	    aclip=get_audio_frame_clip(ablock->start_event,track);
	    aseek=get_audio_frame_seek(ablock->start_event,track);
	    
	    if (ablock->prev!=NULL&&ablock->prev->end_event==ablock->start_event) {
	      int last_aclip=get_audio_frame_clip(ablock->prev->start_event,track);
	      insert_audio_event_at(mt->event_list,ablock->start_event,track,last_aclip,0.,0.);
	    }
	    else {
	      remove_audio_for_track(ablock->start_event,track);
	    }
	    aseek+=avel*(get_event_timecode(event)-get_event_timecode(ablock->start_event))/U_SEC;
	    ablock->start_event=event;
	    ablock->offset_start=new_start_tc;
	  }
	  if (block!=ablock) {
	    block->start_event=event;
	    block->offset_start=new_start_tc;
	  }
	  break;
	}
	if (track>=0) remove_frame_from_event(mt->event_list,event,track);
	event=get_next_frame_event(event);
      }

      if (ablock!=NULL) {
	insert_audio_event_at(mt->event_list,ablock->start_event,track,aclip,aseek,avel);
	ablock->offset_start=aseek*U_SEC;
      }

      // move filter_inits right, and deinits left
      new_tl_tc=get_event_timecode(block->start_event);
      if (start_event==NULL) event=get_first_event(mt->event_list);
      else event=get_next_event(start_event);
      while (event!=NULL&&get_event_timecode(event)<tl_start) event=get_next_event(event);

      while (event!=NULL&&get_event_timecode(event)<new_tl_tc) {
	event_next=get_next_event(event);
	was_moved=FALSE;
	if (WEED_EVENT_IS_FILTER_INIT(event)&&event!=mt->avol_init_event) {
	  if (!move_event_right(mt->event_list,event,TRUE,mt->fps)) {
	    was_moved=TRUE;
	    if (event==start_event) start_event=NULL;
	  }
	}
	else {
	  if (WEED_EVENT_IS_FILTER_DEINIT(event)) {
	    if (!move_event_left(mt->event_list,event,TRUE,mt->fps)) {
	      was_moved=TRUE;
	    }
	  }
	}
	if (was_moved) {
	  if (start_event==NULL) event=get_first_event(mt->event_list);
	  else event=get_next_event(start_event);
	  while (event!=NULL&&get_event_timecode(event)<tl_start) event=get_next_event(event);
	}
	else {
	  event=event_next;
	  if (WEED_EVENT_IS_FRAME(event)) start_event=event;
	}
      }
    }
    else {
      // move start left, not anchored
      if (ablock!=NULL) {
	aclip=get_audio_frame_clip(ablock->start_event,track);
	aseek=get_audio_frame_seek(ablock->start_event,track);
	
	remove_audio_for_track(ablock->start_event,track);

	aseek+=(new_start_tc-ablock->offset_start)/U_SEC;
	ablock->start_event=get_frame_event_at_or_before(mt->event_list,q_gint64(tl_start+(new_start_tc-ablock->offset_start)/avel,mt->fps),get_prev_frame_event(ablock->start_event));
	insert_audio_event_at(mt->event_list,ablock->start_event,track,aclip,aseek,avel);
	ablock->offset_start=aseek*U_SEC;
      }
      if (block!=ablock) {
	// do an insert from offset_start down
	insert_frames (filenum,block->offset_start,new_start_tc,tl_start,DIRECTION_NEGATIVE,block->eventbox,mt,block);
	block->start_event=get_frame_event_at_or_before(mt->event_list,q_gint64(tl_start+new_start_tc-block->offset_start,mt->fps),get_prev_frame_event(block->start_event));
	block->offset_start=new_start_tc;
      }

      // any filter_inits with this track as owner get moved as far left as possible
      new_tl_tc=get_event_timecode(block->start_event);
      event=block->start_event;

      while (event!=NULL&&get_event_timecode(event)<tl_start) {
	start_event=event;
	event=get_next_event(event);
      }
      while (event!=NULL&&get_event_timecode(event)==tl_start) {
	if (WEED_EVENT_IS_FILTER_INIT(event)&&event!=mt->avol_init_event) {
	  if (filter_init_has_owner(event,track)) {
	    // candidate for moving
	    move_filter_init_event(mt->event_list,new_tl_tc,event,mt->fps);
	    // account for overlaps
	    move_event_right(mt->event_list,event,TRUE,mt->fps);
	    event=start_event;
	    while (event!=NULL&&get_event_timecode(event)<tl_start) event=get_next_event(event);
	    continue;
	  }
	}
	event=get_next_event(event);
      }
    }
  }
  else {
    // start is anchored, do a re-insert from start to end
    lives_mt_insert_mode_t insert_mode=mt->opts.insert_mode;
    offset_end=q_gint64((block->offset_start=new_start_tc)+(weed_timecode_t)(U_SEC/mt->fps)+(get_event_timecode(block->end_event)-get_event_timecode(block->start_event)),mt->fps);
    mt->opts.insert_mode=INSERT_MODE_OVERWRITE;
    if (track>=0) insert_frames (filenum,new_start_tc,offset_end,tl_start,DIRECTION_POSITIVE,block->eventbox,mt,block);
    if (ablock!=NULL) {
      aclip=get_audio_frame_clip(ablock->start_event,track);
      aseek=get_audio_frame_seek(ablock->start_event,track);
      aseek+=(new_start_tc-orig_start_tc)/U_SEC;
      insert_audio_event_at(mt->event_list,ablock->start_event,track,aclip,aseek,avel);
      ablock->offset_start=aseek*U_SEC;
    }
    mt->opts.insert_mode=insert_mode;
  }

  new_start_tc=block->offset_start;
  offset_end=(new_start_tc=block->offset_start)+(weed_timecode_t)((gdouble)(track>=0)*U_SEC/mt->fps)+avel*(get_event_timecode(block->end_event)-get_event_timecode(block->start_event));

  if (mt->poly_state==POLY_IN_OUT) {
    g_signal_handler_block (mt->spinbutton_out,mt->spin_out_func);
    g_signal_handler_block (mt->spinbutton_in,mt->spin_in_func);
    set_in_out_spin_ranges(mt,new_start_tc,offset_end);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(mt->spinbutton_out),offset_end/U_SEC);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(mt->spinbutton_in),new_start_tc/U_SEC);
    g_signal_handler_unblock (mt->spinbutton_out,mt->spin_out_func);
    g_signal_handler_unblock (mt->spinbutton_in,mt->spin_in_func);

    if (track>=0) {
      // update images
      update_in_image(mt);
      if (start_anchored) update_out_image(mt,offset_end);
    }
  }

  if (!resize_timeline(mt)) {
    redraw_eventbox(mt,block->eventbox);
  }
}




void in_out_end_changed (GtkWidget *widget, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  track_rect *block=mt->block_selected,*ablock=NULL;
  gdouble new_end=gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget));
  gdouble start_val;
  weed_timecode_t offset_end,orig_end_tc;
  weed_plant_t *event,*prevevent;
  gint track;
  gint filenum;
  weed_timecode_t new_end_tc,tl_end;
  weed_plant_t *start_event,*event_next,*init_event,*new_end_event;
  gboolean was_moved;
  weed_timecode_t new_tl_tc;
  int error;
  int aclip=0;
  double aseek,avel=1.;

  gboolean end_anchored;

  if (block==NULL) {
    file *sfile=mainw->files[mt->file_selected];
    sfile->end=(int)new_end;
    set_clip_labels_variable(mt,mt->file_selected);
    update_out_image(mt,0);

    if (sfile->end<sfile->start) gtk_spin_button_set_value(GTK_SPIN_BUTTON(mt->spinbutton_in),(gdouble)sfile->end);
    return;
  }

  start_val=block->offset_start/U_SEC;
  event=block->end_event;
  track=GPOINTER_TO_INT(g_object_get_data(G_OBJECT(block->eventbox),"layer_number"));

  tl_end=get_event_timecode(event);

  if (track>=0) {
    if (!mt->aud_track_selected) {
      if (mt->opts.pertrack_audio) {
	GtkWidget *aeventbox=GTK_WIDGET(g_object_get_data(G_OBJECT(block->eventbox),"atrack"));
	ablock=get_block_from_time(aeventbox,tl_end/U_SEC-1./mt->fps,mt);
      }
      end_anchored=block->end_anchored;
    }
    else {
      GtkWidget *eventbox=GTK_WIDGET(g_object_get_data(G_OBJECT(block->eventbox),"owner"));
      ablock=block;
      block=get_block_from_time(eventbox,tl_end/U_SEC-1./mt->fps,mt);
      end_anchored=ablock->end_anchored;
    }
    filenum=get_frame_event_clip(block->start_event,track);
  }
  else {
    ablock=block;
    end_anchored=block->end_anchored;
    avel=get_audio_frame_vel(ablock->start_event,track);
    filenum=get_audio_frame_clip(ablock->start_event,track);
  }

  // offset_end is timecode of end event within source (scaled for velocity)
  offset_end=q_gint64(block->offset_start+(weed_timecode_t)((gdouble)(track>=0)*U_SEC/mt->fps)+
		      (weed_timecode_t)((gdouble)(get_event_timecode(block->end_event)-
						  get_event_timecode(block->start_event))*avel),mt->fps);

  if (track>=0&&new_end>mainw->files[filenum]->frames/mainw->files[filenum]->fps) new_end=mainw->files[filenum]->frames/
										    mainw->files[filenum]->fps;

  new_end_tc=q_gint64(block->offset_start+(new_end-start_val)*U_SEC,mt->fps);
  orig_end_tc=offset_end;

#ifdef DEBUG_BL_MOVE
  g_print("pt a %lld %lld %lld %.4f %lld %lld\n",block->offset_start,get_event_timecode(block->end_event),
	  get_event_timecode(block->start_event),new_end,orig_end_tc,new_end_tc);
#endif

  if (ABS(new_end_tc-orig_end_tc)<(.5*U_SEC)/mt->fps||!block->ordered) {
    g_signal_handler_block (mt->spinbutton_out,mt->spin_out_func);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(mt->spinbutton_out),new_end_tc/U_SEC);
    g_signal_handler_unblock (mt->spinbutton_out,mt->spin_out_func);
    return;
  }

  start_event=get_prev_frame_event(event);

  if (!end_anchored) {
    new_tl_tc=q_gint64(get_event_timecode(block->start_event)+(new_end-start_val)*U_SEC/avel-
		       (gdouble)(track>=0)*U_SEC/mt->fps,mt->fps);
    if (track<0) new_tl_tc-=(U_SEC/mt->fps);


#ifdef DEBUG_BL_MOVE
    g_print("new tl tc is %lld %lld %.4f %.4f\n",new_tl_tc,tl_end,new_end,start_val);
#endif
    if (tl_end>new_tl_tc) {
      // end decreased, not anchored

      while (event!=NULL) {
	if (get_event_timecode(event)<=new_tl_tc) {
	  if (event==block->start_event) return;
	  // done
	  if (ablock!=NULL) {
	    if (ablock->next==NULL||ablock->next->start_event!=ablock->end_event) 
	      remove_audio_for_track(ablock->end_event,track);
	    aclip=get_audio_frame_clip(ablock->start_event,track);
	  }
	  block->end_event=event;
	  break;
	}
	prevevent=get_prev_frame_event(event);
	if (track>=0) remove_frame_from_event(mt->event_list,event,track);
	event=prevevent;
      }

      if (ablock!=NULL) {
	new_end_event=get_next_frame_event(event);
	if (new_end_event==NULL) {
	  weed_plant_t *shortcut=ablock->end_event;
	  mt->event_list=insert_blank_frame_event_at(mt->event_list,
						     q_gint64(new_tl_tc+(weed_timecode_t)((gdouble)(track>=0)*
											  U_SEC/mt->fps),mt->fps),
						     &shortcut);
	  ablock->end_event=shortcut;
	}
	else ablock->end_event=new_end_event;
	insert_audio_event_at(mt->event_list,ablock->end_event,track,aclip,0.,0.);
      }

      // move filter_inits right, and deinits left
      new_tl_tc=get_event_timecode(block->end_event);
      start_event=block->end_event;
      while (event!=NULL&&get_event_timecode(event)<=new_tl_tc) event=get_next_event(event);

      while (event!=NULL&&get_event_timecode(event)<=tl_end) {
	event_next=get_next_event(event);
	was_moved=FALSE;
	if (WEED_EVENT_IS_FILTER_INIT(event)) {
	  if (!move_event_right(mt->event_list,event,TRUE,mt->fps)) {
	    was_moved=TRUE;
	    if (event==start_event) start_event=NULL;
	  }
	}
	else {
	  if (WEED_EVENT_IS_FILTER_DEINIT(event)) {
	    init_event=(weed_plant_t *)weed_get_voidptr_value(event,"init_event",&error);
	    if (init_event!=mt->avol_init_event) {
	      if (!move_event_left(mt->event_list,event,TRUE,mt->fps)) {
		was_moved=TRUE;
	      }
	    }
	  }
	}
	if (was_moved) {
	  if (start_event==NULL) event=get_first_event(mt->event_list);
	  else event=get_next_event(start_event);
	  while (event!=NULL&&get_event_timecode(event)<=new_tl_tc) event=get_next_event(event);
	}
	else {
	  event=event_next;
	  if (WEED_EVENT_IS_FRAME(event)) start_event=event;
	}
      }
      remove_end_blank_frames(mt->event_list);
    }
    else {
      // end increased, not anchored
      if (track>=0) {
	// do an insert from end_tc up, starting with end_frame and finishing at new_end
	insert_frames (filenum,offset_end,new_end_tc,tl_end+(weed_timecode_t)(U_SEC/mt->fps),DIRECTION_POSITIVE,
		       block->eventbox,mt,block);
	block->end_event=get_frame_event_at(mt->event_list,q_gint64(new_end_tc+tl_end-offset_end,mt->fps),
					    block->end_event,TRUE);
      }
      if (ablock!=NULL) {
	new_end_event=get_frame_event_at(mt->event_list,q_gint64(new_tl_tc+U_SEC/mt->fps,mt->fps),ablock->end_event,TRUE);
	if (new_end_event==ablock->end_event) {
	  g_signal_handler_block (mt->spinbutton_out,mt->spin_out_func);
	  gtk_spin_button_set_value(GTK_SPIN_BUTTON(mt->spinbutton_out),orig_end_tc/U_SEC);
	  g_signal_handler_unblock (mt->spinbutton_out,mt->spin_out_func);
	  return;
	}
	remove_audio_for_track(ablock->end_event,track);
	if (new_end_event==NULL) {
	  weed_plant_t *shortcut=ablock->end_event;
	  mt->event_list=insert_blank_frame_event_at(mt->event_list,q_gint64(new_tl_tc+U_SEC/mt->fps,mt->fps),&shortcut);
	  ablock->end_event=shortcut;
	}
	else ablock->end_event=new_end_event;

	if (ablock->next==NULL||ablock->next->start_event!=ablock->end_event) {
	  aclip=get_audio_frame_clip(ablock->start_event,track);
	  insert_audio_event_at(mt->event_list,ablock->end_event,track,aclip,0.,0.);
	}
      }

      new_tl_tc=get_event_timecode(block->end_event);

      start_event=event;

      while (event!=NULL&&get_event_timecode(event)==tl_end) {
	if (WEED_EVENT_IS_FILTER_DEINIT(event)) {
	  init_event=(weed_plant_t *)weed_get_voidptr_value(event,"init_event",&error);
	  if (init_event!=mt->avol_init_event) {
	    if (filter_init_has_owner(init_event,track)) {
	      // candidate for moving
	      move_filter_deinit_event(mt->event_list,new_tl_tc,event,mt->fps,TRUE);
	      // account for overlaps
	      //move_event_left(mt->event_list,event,TRUE,mt->fps);
	      event=start_event;
	      continue;
	    }
	  }
	}
	event=get_next_event(event);
      }
    }
  }
  else {
    // end is anchored, do a re-insert from end to start
    weed_timecode_t offset_start;
    lives_mt_insert_mode_t insert_mode=mt->opts.insert_mode;

    offset_end=q_gint64((offset_start=block->offset_start+new_end_tc-orig_end_tc)+
			(weed_timecode_t)((gdouble)(track>=0)*U_SEC/mt->fps)+
			(get_event_timecode(block->end_event)-get_event_timecode(block->start_event)),mt->fps);

    mt->opts.insert_mode=INSERT_MODE_OVERWRITE;
    if (track>=0) insert_frames (filenum,offset_end,offset_start,tl_end+
				 (weed_timecode_t)((gdouble)(track>=0&&!mt->aud_track_selected)*U_SEC/mt->fps),
				 DIRECTION_NEGATIVE,block->eventbox,mt,block);

    block->offset_start=q_gint64(offset_start,mt->fps);

    if (ablock!=NULL) {
      aclip=get_audio_frame_clip(ablock->start_event,track);
      aseek=get_audio_frame_seek(ablock->start_event,track);
      avel=get_audio_frame_vel(ablock->start_event,track);
      aseek+=((new_end_tc-orig_end_tc)/U_SEC);
      insert_audio_event_at(mt->event_list,ablock->start_event,track,aclip,aseek,avel);
      ablock->offset_start=aseek*U_SEC;
    }

    mt->opts.insert_mode=insert_mode;
  }

  // get new offset_end
  new_end_tc=(block->offset_start+(weed_timecode_t)((gdouble)(track>=0)*U_SEC/mt->fps)+
	      (get_event_timecode(block->end_event)-get_event_timecode(block->start_event))*avel);

#ifdef DEBUG_BL_MOVE
  g_print("new end tc is %ld %ld %ld %.4f\n",get_event_timecode(block->end_event),
	  get_event_timecode(block->start_event),block->offset_start,avel);
#endif

  if (mt->avol_fx!=-1&&mt->avol_init_event!=NULL&&mt->audio_draws!=NULL&&
      mt->audio_draws->data!=NULL&&block->next==NULL) {
    apply_avol_filter(mt);
  }


  if (mt->poly_state==POLY_IN_OUT) {
    g_signal_handler_block (mt->spinbutton_out,mt->spin_out_func);
    g_signal_handler_block (mt->spinbutton_in,mt->spin_in_func);

    set_in_out_spin_ranges(mt,block->offset_start,new_end_tc);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(mt->spinbutton_in), block->offset_start/U_SEC);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(mt->spinbutton_out), new_end_tc/U_SEC);
    g_signal_handler_unblock (mt->spinbutton_out,mt->spin_out_func);
    g_signal_handler_unblock (mt->spinbutton_in,mt->spin_in_func);

    if (track>=0) {
      // update image
      update_out_image(mt,new_end_tc);
      if (end_anchored) update_in_image(mt);
    }
  }
#ifdef DEBUG_BL_MOVE
  g_print("pt b %ld\n",q_gint64(new_end_tc/avel,mt->fps));
#endif
  if (!resize_timeline(mt)) {
    redraw_eventbox(mt,block->eventbox);
    if (ablock!=NULL&&ablock!=block) redraw_eventbox(mt,block->eventbox);
    // TODO - redraw chans ??
  }
}


void avel_reverse_toggled (GtkToggleButton *togglebutton, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  track_rect *block=mt->block_selected;
  gint track=GPOINTER_TO_INT(g_object_get_data(G_OBJECT(block->eventbox),"layer_number"));
  double avel=-get_audio_frame_vel(block->start_event,track);
  double aseek=get_audio_frame_seek(block->start_event,track),aseek_end;
  int aclip=get_audio_frame_clip(block->start_event,track);
  
  gdouble old_in_val=gtk_spin_button_get_value(GTK_SPIN_BUTTON(mt->spinbutton_in));
  gdouble old_out_val=gtk_spin_button_get_value(GTK_SPIN_BUTTON(mt->spinbutton_out));

  // update avel and aseek
  aseek_end=aseek+(get_event_timecode(block->end_event)-get_event_timecode(block->start_event))/U_SEC*(-avel);
  insert_audio_event_at(mt->event_list,block->start_event,track,aclip,aseek_end,avel);


  g_signal_handler_block (mt->spinbutton_out,mt->spin_out_func);
  g_signal_handler_block (mt->spinbutton_in,mt->spin_in_func);

  if (avel<0.) set_in_out_spin_ranges(mt,old_in_val*U_SEC,old_out_val*U_SEC);
  else set_in_out_spin_ranges(mt,old_out_val*U_SEC,old_in_val*U_SEC);

  gtk_spin_button_set_value(GTK_SPIN_BUTTON(mt->spinbutton_in),old_out_val);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(mt->spinbutton_out),old_in_val);

  g_signal_handler_unblock (mt->spinbutton_in,mt->spin_in_func);
  g_signal_handler_unblock (mt->spinbutton_out,mt->spin_out_func);

  if (avel<0.) {
    gtk_widget_set_sensitive(mt->spinbutton_in,FALSE);
    gtk_widget_set_sensitive(mt->spinbutton_out,FALSE);
    gtk_widget_set_sensitive(mt->spinbutton_avel,FALSE);
    gtk_widget_set_sensitive(mt->avel_scale,FALSE);
    gtk_widget_set_sensitive(mt->checkbutton_start_anchored,FALSE);
    gtk_widget_set_sensitive(mt->checkbutton_end_anchored,FALSE);
  }
  else {
    gtk_widget_set_sensitive(mt->spinbutton_in,TRUE);
    gtk_widget_set_sensitive(mt->spinbutton_out,TRUE);
    if (!block->start_anchored||!block->end_anchored) {
      gtk_widget_set_sensitive(mt->spinbutton_avel,TRUE);
      gtk_widget_set_sensitive(mt->avel_scale,TRUE);
    }
    gtk_widget_set_sensitive(mt->checkbutton_start_anchored,TRUE);
    gtk_widget_set_sensitive(mt->checkbutton_end_anchored,TRUE);
  }

}


void avel_spin_changed (GtkSpinButton *spinbutton, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  track_rect *block=mt->block_selected;
  gint track=GPOINTER_TO_INT(g_object_get_data(G_OBJECT(block->eventbox),"layer_number"));
  double new_avel=gtk_spin_button_get_value(spinbutton);
  double aseek=get_audio_frame_seek(block->start_event,track);
  int aclip=get_audio_frame_clip(block->start_event,track);
  weed_timecode_t new_end_tc,old_tl_tc,start_tc,new_tl_tc,min_tc;
  weed_plant_t *new_end_event,*new_start_event;
  gdouble orig_end_val,orig_start_val;
  gboolean was_adjusted=FALSE;

  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mt->checkbutton_avel_reverse))) new_avel=-new_avel;

  start_tc=block->offset_start;

  orig_end_val=gtk_spin_button_get_value(GTK_SPIN_BUTTON(mt->spinbutton_out));
  old_tl_tc=get_event_timecode(block->end_event);

  if (!block->end_anchored) {
    new_end_tc=q_gint64(start_tc+((orig_end_val=gtk_spin_button_get_value(GTK_SPIN_BUTTON(mt->spinbutton_out)))*U_SEC-start_tc)/new_avel,mt->fps);

    insert_audio_event_at(mt->event_list,block->start_event,track,aclip,aseek,new_avel);

    new_tl_tc=q_gint64(get_event_timecode(block->start_event)+(orig_end_val*U_SEC-start_tc)/new_avel,mt->fps);

    // move end point (if we can)
    if (block->next!=NULL&&new_tl_tc>=get_event_timecode(block->next->start_event)) {
      new_end_tc=q_gint64((get_event_timecode(block->next->start_event)-get_event_timecode(block->start_event))*new_avel+block->offset_start,mt->fps);
      g_signal_handler_block (mt->spinbutton_out,mt->spin_out_func);
      g_signal_handler_block (mt->spinbutton_in,mt->spin_in_func);
      set_in_out_spin_ranges(mt,block->offset_start,new_end_tc);
      gtk_spin_button_set_value(GTK_SPIN_BUTTON(mt->spinbutton_out),new_end_tc/U_SEC);
      g_signal_handler_unblock (mt->spinbutton_in,mt->spin_in_func);
      g_signal_handler_unblock (mt->spinbutton_out,mt->spin_out_func);
      return;
    }
    
    if (new_tl_tc!=old_tl_tc) {
      weed_plant_t *shortcut;
      if (new_tl_tc>old_tl_tc) shortcut=block->end_event;
      else shortcut=block->start_event;
      if (block->next==NULL||block->next->start_event!=block->end_event) remove_audio_for_track(block->end_event,-1);
      new_end_event=get_frame_event_at(mt->event_list,new_tl_tc,shortcut,TRUE);
      if (new_end_event==block->start_event) return;
      block->end_event=new_end_event;
      
      if (block->end_event==NULL) {
	weed_plant_t *last_frame_event=get_last_frame_event(mt->event_list);
	add_blank_frames_up_to(mt->event_list,last_frame_event,new_tl_tc,mt->fps);
	block->end_event=get_last_frame_event(mt->event_list);
      }
      if (block->next==NULL||block->next->start_event!=block->end_event) insert_audio_event_at(mt->event_list,block->end_event,-1,aclip,0.,0.);
      
      gtk_widget_queue_draw((GtkWidget *)mt->audio_draws->data);
      new_end_tc=start_tc+(get_event_timecode(block->end_event)-get_event_timecode(block->start_event))*new_avel;
      g_signal_handler_block (mt->spinbutton_out,mt->spin_out_func);
      gtk_spin_button_set_value(GTK_SPIN_BUTTON(mt->spinbutton_out),new_end_tc/U_SEC);
      g_signal_handler_unblock (mt->spinbutton_out,mt->spin_out_func);
      gtk_spin_button_set_value(GTK_SPIN_BUTTON(mt->spinbutton_out),orig_end_val);

      remove_end_blank_frames(mt->event_list);

      if (mt->avol_fx!=-1&&block->next==NULL) {
	apply_avol_filter(mt);
      }
    }
    if (!resize_timeline(mt)) {
      redraw_eventbox(mt,block->eventbox);
    }
    return;
  }

  // move start point (if we can)
  min_tc=0;
  if (block->prev!=NULL) min_tc=get_event_timecode(block->prev->end_event);

  new_tl_tc=q_gint64(get_event_timecode(block->end_event)-(orig_end_val*U_SEC-start_tc)/new_avel,mt->fps);
  new_end_tc=orig_end_val*U_SEC;

  if (new_tl_tc<min_tc) {
    aseek-=(new_tl_tc-min_tc)/U_SEC;
    start_tc=block->offset_start=aseek*U_SEC;
    new_tl_tc=min_tc;
    was_adjusted=TRUE;
  }

  orig_start_val=gtk_spin_button_get_value(GTK_SPIN_BUTTON(mt->spinbutton_in));
  
  if (was_adjusted||new_tl_tc!=old_tl_tc) {
    weed_plant_t *shortcut;
    if (new_tl_tc>old_tl_tc) shortcut=block->start_event;
    else {
      if (block->prev!=NULL) shortcut=block->prev->end_event;
      else shortcut=NULL;
    }
    
    new_start_event=get_frame_event_at(mt->event_list,new_tl_tc,shortcut,TRUE);
    if (new_start_event==block->end_event) return;

    if (block->prev==NULL||block->start_event!=block->prev->end_event) remove_audio_for_track(block->start_event,-1);
    else insert_audio_event_at(mt->event_list,block->start_event,-1,aclip,0.,0.);
    block->start_event=new_start_event;
    
    insert_audio_event_at(mt->event_list,block->start_event,-1,aclip,aseek,new_avel);
    
    gtk_widget_queue_draw((GtkWidget *)mt->audio_draws->data);
    
    g_signal_handler_block (mt->spinbutton_out,mt->spin_out_func);
    g_signal_handler_block (mt->spinbutton_in,mt->spin_in_func);

    set_in_out_spin_ranges(mt,start_tc,new_end_tc);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(mt->spinbutton_in),start_tc/U_SEC);

    if (!was_adjusted) gtk_spin_button_set_value(GTK_SPIN_BUTTON(mt->spinbutton_in),orig_start_val);

    g_signal_handler_unblock (mt->spinbutton_in,mt->spin_in_func);
    g_signal_handler_unblock (mt->spinbutton_out,mt->spin_out_func);

    if (mt->avol_fx!=-1&&block->next==NULL) {
      apply_avol_filter(mt);
    }
  }

}


void
in_anchor_toggled (GtkToggleButton *togglebutton, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  track_rect *block=mt->block_selected;
  weed_timecode_t offset_end;
  gdouble avel=1.;

  if (mt->current_track<0) {
    avel=get_audio_frame_vel(block->start_event,mt->current_track);
  }

  offset_end=block->offset_start+(gdouble)(mt->current_track>=0&&!mt->aud_track_selected)*(weed_timecode_t)(U_SEC/mt->fps)+((get_event_timecode(block->end_event)-get_event_timecode(block->start_event)))*avel;

  block->start_anchored=!block->start_anchored;


  g_signal_handler_block (mt->spinbutton_out,mt->spin_out_func);
  g_signal_handler_block (mt->spinbutton_in,mt->spin_in_func);
  set_in_out_spin_ranges(mt,block->offset_start,offset_end);
  g_signal_handler_unblock (mt->spinbutton_in,mt->spin_in_func);
  g_signal_handler_unblock (mt->spinbutton_out,mt->spin_out_func);

  if ((block->start_anchored&&block->end_anchored)||mainw->playing_file>-1) {
    gtk_widget_set_sensitive(mt->spinbutton_avel,FALSE);
    gtk_widget_set_sensitive(mt->avel_scale,FALSE);
  }
  else {
    gtk_widget_set_sensitive(mt->spinbutton_avel,TRUE);
    gtk_widget_set_sensitive(mt->avel_scale,TRUE);
  }

  if (mt->current_track>=0&&mt->opts.pertrack_audio) {
    GtkWidget *xeventbox;
    track_rect *xblock;

    if (mt->aud_track_selected) {
      xeventbox=(GtkWidget *)g_object_get_data(G_OBJECT(block->eventbox),"owner");
    }
    else {
      xeventbox=(GtkWidget *)g_object_get_data(G_OBJECT(block->eventbox),"atrack");
    }
    if (xeventbox!=NULL) {
      xblock=get_block_from_time(xeventbox,get_event_timecode(block->start_event)/U_SEC,mt);
      if (xblock!=NULL) xblock->start_anchored=block->start_anchored;
    }
  }
}

void
out_anchor_toggled (GtkToggleButton *togglebutton, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  track_rect *block=mt->block_selected;
  weed_timecode_t offset_end;
  gdouble avel=1.;

  if (mt->current_track<0) {
    avel=get_audio_frame_vel(block->start_event,mt->current_track);
  }

  offset_end=block->offset_start+(gdouble)(mt->current_track>=0&&!mt->aud_track_selected)*(weed_timecode_t)(U_SEC/mt->fps)+((get_event_timecode(block->end_event)-get_event_timecode(block->start_event)))*avel;

  block->end_anchored=!block->end_anchored;

  g_signal_handler_block (mt->spinbutton_out,mt->spin_out_func);
  g_signal_handler_block (mt->spinbutton_in,mt->spin_in_func);
  set_in_out_spin_ranges(mt,block->offset_start,offset_end);
  g_signal_handler_unblock (mt->spinbutton_in,mt->spin_in_func);
  g_signal_handler_unblock (mt->spinbutton_out,mt->spin_out_func);

  if ((block->start_anchored&&block->end_anchored)||mainw->playing_file>-1) {
    gtk_widget_set_sensitive(mt->spinbutton_avel,FALSE);
    gtk_widget_set_sensitive(mt->avel_scale,FALSE);
  }
  else {
    gtk_widget_set_sensitive(mt->spinbutton_avel,TRUE);
    gtk_widget_set_sensitive(mt->avel_scale,TRUE);
  }

  if (mt->current_track>=0&&mt->opts.pertrack_audio) {
    GtkWidget *xeventbox;
    track_rect *xblock;

    if (mt->aud_track_selected) {
      xeventbox=(GtkWidget *)g_object_get_data(G_OBJECT(block->eventbox),"owner");
    }
    else {
      xeventbox=(GtkWidget *)g_object_get_data(G_OBJECT(block->eventbox),"atrack");
    }
    if (xeventbox!=NULL) {
      xblock=get_block_from_time(xeventbox,get_event_timecode(block->start_event)/U_SEC,mt);
      if (xblock!=NULL) xblock->end_anchored=block->end_anchored;
    }
  }

}


void polymorph (lives_mt *mt, lives_mt_poly_state_t poly) {
  GdkPixbuf *thumb;
  gint track;
  gint frame_start,frame_end=0;
  int filenum;
  track_rect *block=mt->block_selected;
  weed_timecode_t offset_end=0;
  weed_plant_t *filter;
  weed_timecode_t tc;
  int error;
  GtkWidget *eventbox,*xeventbox,*yeventbox,*label,*vbox;
  gdouble secs;
  weed_plant_t *frame_event,*filter_map;
  gint num_fx=0;
  void **init_events;
  int i,j,fidx;
  weed_plant_t *init_event;
  gchar *fhash;
  gint num_in_tracks,num_out_tracks;
  gboolean is_input,is_output;
  int *in_tracks,*out_tracks,def_out_track=0;
  gchar *fname,*otrackname,*txt;
  gint olayer;
  gboolean has_effect=FALSE;
  gboolean has_params;
  gboolean needs_idlefunc=FALSE;
  gboolean tab_set=FALSE;
  GtkWidget *bbox;
  weed_plant_t *prev_fm_event,*next_fm_event,*shortcut;
  gint fxcount=0;
  gint nins=1;

  gint width=cfile->hsize;
  gint height=cfile->vsize;
  
  static gint xxwidth,xxheight;

  gboolean start_anchored,end_anchored;

  gdouble out_end_range;
  gdouble avel=1.;

  if (poly==mt->poly_state&&poly!=POLY_PARAMS&&poly!=POLY_FX_STACK) return;

  if (mt->poly_box->allocation.width>1&&mt->poly_box->allocation.height>1) {
    calc_maxspect(mt->poly_box->allocation.width/2-IN_OUT_SEP,mt->poly_box->allocation.height-((block==NULL||block->ordered)?mainw->spinbutton_start->allocation.height:0),&width,&height);
    
    xxwidth=width;
    xxheight=height;
  }
  else {
    width=xxwidth;
    height=xxheight;
  }

  switch (mt->poly_state) {
  case (POLY_CLIPS) :
    gtk_container_remove (GTK_CONTAINER(mt->poly_box),mt->clip_scroll);
    break;
  case (POLY_IN_OUT) :
    g_signal_handler_block (mt->spinbutton_in,mt->spin_in_func);
    g_signal_handler_block (mt->spinbutton_out,mt->spin_out_func);
    if (mt->in_out_box->parent!=NULL) gtk_container_remove (GTK_CONTAINER(mt->poly_box),mt->in_out_box);
    if (mt->avel_box->parent!=NULL) gtk_container_remove (GTK_CONTAINER(mt->poly_box),mt->avel_box);

    break;
  case (POLY_PARAMS) :
    mt->framedraw=NULL;
    mt->fx_params_label=NULL;
    if (mt->current_rfx!=NULL) {
      rfx_free(mt->current_rfx);
      g_free(mt->current_rfx);
    }
    mt->current_rfx=NULL;

    if (mt->fx_box!=NULL) {
      gtk_widget_destroy(mt->fx_box);
      mt->fx_box=NULL;
      
      gtk_container_remove (GTK_CONTAINER(mt->poly_box),mt->fx_base_box);
    }

    if (mt->mt_frame_preview) {
      // put blank back in preview window
      gtk_widget_ref (mainw->playarea);
      gtk_widget_modify_bg (mt->fd_frame, GTK_STATE_NORMAL, &palette->normal_back);
    }
    if (pchain!=NULL&&poly!=POLY_PARAMS) {
      g_free(pchain);
      pchain=NULL;
    }
    mouse_mode_context(mt); // reset context box text
    mt->last_fx_type=MT_LAST_FX_NONE;
    mt->fx_box=NULL;
    gtk_widget_set_sensitive(mt->fx_edit,FALSE);
    gtk_widget_set_sensitive(mt->fx_delete,FALSE);
    if (mt->idlefunc>0) {
      g_source_remove(mt->idlefunc);
      mt->idlefunc=0;
      needs_idlefunc=TRUE;
    }
    if (poly==POLY_PARAMS) { 
      while (g_main_context_iteration(NULL,FALSE));
    }
    else {
      mt->init_event=NULL;
      mt_show_current_frame(mt, FALSE);
    }
    if (needs_idlefunc) {
      mt->idlefunc=mt_idle_add(mt);
    }

    break;
  case POLY_FX_STACK:
    if (poly!=POLY_FX_STACK) {
      mt->selected_init_event=NULL;
      mt->fm_edit_event=NULL;
      mt->context_time=-1.;
    }
  case POLY_EFFECTS:
  case POLY_TRANS:
  case POLY_COMP:
    gtk_widget_destroy(mt->fx_list_box);
    if (mt->nb_label!=NULL) gtk_widget_destroy(mt->nb_label);
    mt->nb_label=NULL;
    break;
  default:
    break;
  }

  mt->poly_state=poly;

  if (mt->poly_state==POLY_NONE) return; // transitional state

  switch (poly) {
  case (POLY_IN_OUT) :
    set_poly_tab(mt,POLY_IN_OUT);

    mt->init_event=NULL;
    if (block==NULL||block->ordered) {
      gtk_widget_show(mt->in_hbox);
      gtk_widget_show(mt->out_hbox);
    }
    else {
      gtk_widget_hide(mt->in_hbox);
      gtk_widget_hide(mt->out_hbox);
    }

    if (block!=NULL) {
      track=GPOINTER_TO_INT(g_object_get_data(G_OBJECT(block->eventbox),"layer_number"));

      offset_end=block->offset_start+(weed_timecode_t)((gdouble)(track>=0)*U_SEC/mt->fps)+
	((get_event_timecode(block->end_event)-get_event_timecode(block->start_event))*ABS(avel));

      start_anchored=block->start_anchored;
      end_anchored=block->end_anchored;
    }
    else {
      track=0;

      start_anchored=end_anchored=FALSE;

      filenum=mt->file_selected;

      frame_start=mainw->files[filenum]->start;
      frame_end=mainw->files[filenum]->end;

    }

    if (track>-1) {
      GtkWidget *oeventbox;

      if (block!=NULL) {
	secs=GTK_RULER(mt->timeline)->position;
	if (mt->context_time!=-1.&&mt->use_context) secs=mt->context_time;
	if (is_audio_eventbox(mt,block->eventbox)&&(oeventbox=(GtkWidget *)g_object_get_data(G_OBJECT(block->eventbox),
										"owner"))!=NULL) {
	  // if moving an audio block we move the associated video block first
	  block=get_block_from_time(oeventbox,secs,mt);
	}
	filenum=get_frame_event_clip(block->start_event,track);
	
	frame_start=calc_frame_from_time(filenum,block->offset_start/U_SEC);
	frame_end=calc_frame_from_time(filenum,offset_end/U_SEC-1./mt->fps);
      }

      gtk_container_set_border_width (GTK_CONTAINER (mt->poly_box), 0);
      gtk_widget_hide(mt->avel_box);
      gtk_widget_show(mt->in_image);
      gtk_widget_show(mt->out_image);

      if (mainw->playing_file==filenum) {
	mainw->files[filenum]->event_list=mt->event_list;
      }
      
      // start image
      thumb=make_thumb(mt,filenum,width,height,frame_start,FALSE);
      gtk_image_set_from_pixbuf (GTK_IMAGE(mt->in_image),thumb);
      if (thumb!=NULL) gdk_pixbuf_unref(thumb);
    }
    else {
      gtk_container_set_border_width (GTK_CONTAINER (mt->poly_box), 10);
      filenum=get_audio_frame_clip(block->start_event,track);
      gtk_widget_hide(mt->in_image);
      gtk_widget_hide(mt->out_image);
      gtk_box_pack_start(GTK_BOX(mt->poly_box),mt->avel_box,TRUE,TRUE,0);
      gtk_widget_show(mt->avel_box);
      avel=get_audio_frame_vel(block->start_event,track);
      offset_end=block->offset_start+q_gint64((weed_timecode_t)((gdouble)(track>=0)*U_SEC/mt->fps)+
					      ((get_event_timecode(block->end_event)-
						get_event_timecode(block->start_event))*ABS(avel)),mt->fps);
    }

    if (block==NULL) {
      gtk_widget_hide(mt->checkbutton_start_anchored);
      gtk_widget_hide(mt->checkbutton_end_anchored);
      gtk_spin_button_set_digits(GTK_SPIN_BUTTON(mt->spinbutton_in),0);
      gtk_spin_button_set_digits(GTK_SPIN_BUTTON(mt->spinbutton_out),0);
      adjustment_configure(GTK_ADJUSTMENT(mt->spinbutton_in_adj),mainw->files[filenum]->start,1.,
			   mainw->files[filenum]->frames,1.,100.,0.);
      adjustment_configure(GTK_ADJUSTMENT(mt->spinbutton_out_adj),mainw->files[filenum]->end,1.,
			   mainw->files[filenum]->frames,1.,100.,0.);
      gtk_widget_hide(mt->in_eventbox);
      gtk_widget_hide(mt->out_eventbox);
      gtk_widget_show(mt->start_in_label);
      gtk_widget_show(mt->end_out_label);
    }
    else {
      gtk_widget_show(mt->checkbutton_start_anchored);
      gtk_widget_show(mt->checkbutton_end_anchored);
      gtk_spin_button_set_digits(GTK_SPIN_BUTTON(mt->spinbutton_in),2);
      gtk_spin_button_set_digits(GTK_SPIN_BUTTON(mt->spinbutton_out),2);
      adjustment_configure(GTK_ADJUSTMENT(mt->spinbutton_in_adj),0.,0.,0.,1./mt->fps,1.,0.);
      adjustment_configure(GTK_ADJUSTMENT(mt->spinbutton_out_adj),0.,0.,0.,1./mt->fps,1.,0.);
      gtk_widget_show(mt->in_eventbox);
      gtk_widget_show(mt->out_eventbox);
      gtk_widget_hide(mt->start_in_label);
      gtk_widget_hide(mt->end_out_label);
    }

    if (avel>0.) {
      if (block!=NULL) {
	gtk_spin_button_set_range(GTK_SPIN_BUTTON(mt->spinbutton_in),0., offset_end/U_SEC-1./mt->fps);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(mt->spinbutton_in),block->offset_start/U_SEC);

      }
      else {
	gtk_spin_button_set_range(GTK_SPIN_BUTTON(mt->spinbutton_in),1., mainw->files[filenum]->frames);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(mt->spinbutton_in),mainw->files[filenum]->start);
      }
      g_signal_handler_block (mt->checkbutton_start_anchored,mt->check_start_func);
      g_signal_handler_block (mt->checkbutton_avel_reverse,mt->check_avel_rev_func);
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mt->checkbutton_start_anchored),start_anchored);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(mt->checkbutton_avel_reverse),FALSE);
      g_signal_handler_unblock (mt->checkbutton_avel_reverse,mt->check_avel_rev_func);
      g_signal_handler_unblock (mt->checkbutton_start_anchored,mt->check_start_func);
    }
    else {
      gtk_spin_button_set_range(GTK_SPIN_BUTTON(mt->spinbutton_out),0., offset_end/U_SEC-1./mt->fps);
      gtk_spin_button_set_value(GTK_SPIN_BUTTON(mt->spinbutton_out),block->offset_start/U_SEC);
      g_signal_handler_block (mt->checkbutton_start_anchored,mt->check_start_func);
      g_signal_handler_block (mt->checkbutton_avel_reverse,mt->check_avel_rev_func);
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mt->checkbutton_start_anchored),start_anchored);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(mt->checkbutton_avel_reverse),TRUE);
      g_signal_handler_unblock (mt->checkbutton_avel_reverse,mt->check_avel_rev_func);
      g_signal_handler_unblock (mt->checkbutton_start_anchored,mt->check_start_func);
    }

    g_signal_handler_block (mt->spinbutton_avel,mt->spin_avel_func);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(mt->spinbutton_avel),ABS(avel));
    g_signal_handler_unblock (mt->spinbutton_avel,mt->spin_avel_func);

    if (track>-1) {
      // end image
      thumb=make_thumb(mt,filenum,width,height,frame_end,FALSE);
      gtk_image_set_from_pixbuf (GTK_IMAGE(mt->out_image),thumb);
      if (thumb!=NULL) gdk_pixbuf_unref(thumb);
      out_end_range=count_resampled_frames(mainw->files[filenum]->frames,mainw->files[filenum]->fps,mt->fps)/mt->fps;
    }
    else out_end_range=q_gint64(mainw->files[filenum]->laudio_time*U_SEC,mt->fps)/U_SEC;

    if (avel>0.) {
      if (block!=NULL) {
	gtk_spin_button_set_range(GTK_SPIN_BUTTON(mt->spinbutton_out), block->offset_start/U_SEC+1./mt->fps, out_end_range);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(mt->spinbutton_out),offset_end/U_SEC);
	if (!block->start_anchored||!block->end_anchored) {
	  gtk_widget_set_sensitive(mt->spinbutton_avel,TRUE);
	  gtk_widget_set_sensitive(mt->avel_scale,TRUE);
	}
      }
      gtk_widget_set_sensitive(mt->spinbutton_in,TRUE);
      gtk_widget_set_sensitive(mt->spinbutton_out,TRUE);

      gtk_widget_grab_focus(mt->spinbutton_in);
    }
    else {
      gtk_spin_button_set_range(GTK_SPIN_BUTTON(mt->spinbutton_in), block->offset_start/U_SEC+1./mt->fps, out_end_range);
      gtk_spin_button_set_value(GTK_SPIN_BUTTON(mt->spinbutton_in),offset_end/U_SEC);
      gtk_widget_set_sensitive(mt->spinbutton_in,FALSE);
      gtk_widget_set_sensitive(mt->spinbutton_out,FALSE);
      gtk_widget_set_sensitive(mt->spinbutton_avel,FALSE);
      gtk_widget_set_sensitive(mt->avel_scale,FALSE);
    }

    g_signal_handler_block (mt->checkbutton_end_anchored,mt->check_end_func);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mt->checkbutton_end_anchored),end_anchored);
    g_signal_handler_unblock (mt->checkbutton_end_anchored,mt->check_end_func);
    gtk_box_pack_start(GTK_BOX(mt->poly_box),mt->in_out_box,TRUE,TRUE,0);

    g_signal_handler_unblock (mt->spinbutton_in,mt->spin_in_func);
    g_signal_handler_unblock (mt->spinbutton_out,mt->spin_out_func);

    if (mainw->playing_file>-1) mt_desensitise(mt);
    else mt_sensitise (mt);

    break;
  case (POLY_CLIPS) :
    set_poly_tab(mt,POLY_CLIPS);
    mt->init_event=NULL;
    gtk_box_pack_start(GTK_BOX(mt->poly_box),mt->clip_scroll,TRUE,TRUE,0);
    if (mt->is_ready) mouse_mode_context(mt);
    break;
  case (POLY_PARAMS):
    set_poly_tab(mt,POLY_PARAMS);

    gtk_box_pack_start(GTK_BOX(mt->poly_box),mt->fx_base_box,TRUE,TRUE,0);

    filter=get_weed_filter(mt->current_fx);

    if (mt->current_rfx!=NULL) {
      rfx_free(mt->current_rfx);
      g_free(mt->current_rfx);
    }

    mt->current_rfx=weed_to_rfx(filter,FALSE);

    tc=get_event_timecode(mt->init_event);

    if (fx_dialog[1]!=NULL) {
      lives_rfx_t *rfx=(lives_rfx_t *)g_object_get_data (G_OBJECT (fx_dialog[1]),"rfx");
      gtk_widget_destroy(fx_dialog[1]);
      on_paramwindow_cancel_clicked2(NULL,rfx);
    }

    if (mt->fx_box!=NULL) gtk_widget_destroy(mt->fx_box);

    mt->fx_box=NULL;
    get_track_index(mt,tc);

    mt->prev_fx_time=0; // force redraw in node_spin_val_changed
    has_params=add_mt_param_box(mt);

    if (mainw->playing_file<0) {
      if (mt->current_track>=0) {
	if (mt->idlefunc>0) {
	  g_source_remove(mt->idlefunc);
	  mt->idlefunc=0;
	  needs_idlefunc=TRUE;
	}
	mt->block_tl_move=TRUE;
	on_node_spin_value_changed(GTK_SPIN_BUTTON(mt->node_spinbutton),mt); // force parameter interpolation
	mt->block_tl_move=FALSE;
	if (needs_idlefunc) {
	  mt->idlefunc=mt_idle_add(mt);
	}
      }
    }
    clear_context(mt);
    if (has_params) {
      add_context_label(mt,_("Drag the time slider to where you"));
      add_context_label(mt,_("want to set effect parameters"));
      add_context_label(mt,_("Set parameters, then click \"Apply\"\n"));
      add_context_label(mt,_("NODES are points where parameters\nhave been set.\nNodes can be deleted."));
    }
    else {
      add_context_label(mt,_("Effect has no parameters.\n"));
    }
    break;
  case POLY_FX_STACK:
    set_poly_tab(mt,POLY_FX_STACK);
    mt->init_event=NULL;
    if (mt->current_track>=0) eventbox=(GtkWidget *)g_list_nth_data(mt->video_draws,mt->current_track);
    else eventbox=(GtkWidget *)mt->audio_draws->data;
    secs=GTK_RULER(mt->timeline)->position;
    if (mt->context_time!=-1.&&mt->use_context) secs=mt->context_time;

    block=get_block_from_time(eventbox,secs,mt);
    if (block==NULL) {
      block=get_block_before(eventbox,secs,FALSE);
      if (block!=NULL) shortcut=block->end_event;
      else shortcut=NULL;
    }
    else shortcut=block->start_event;

    tc=q_gint64(secs*U_SEC,mt->fps);

    frame_event=get_frame_event_at(mt->event_list,tc,shortcut,TRUE);
    filter_map=mt->fm_edit_event=get_filter_map_before(frame_event,-1000000,NULL);

    mt->fx_list_box=gtk_vbox_new(FALSE,0);
    mt->fx_list_scroll = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (mt->fx_list_scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start (GTK_BOX (mt->fx_list_box), mt->fx_list_scroll, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(mt->poly_box),mt->fx_list_box,TRUE,TRUE,0);

    mt->fx_list_vbox=gtk_vbox_new(FALSE,10);
    gtk_container_set_border_width (GTK_CONTAINER (mt->fx_list_vbox), 10);
    gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (mt->fx_list_scroll), mt->fx_list_vbox);

    if (filter_map!=NULL) {
      if (weed_plant_has_leaf(filter_map,"init_events")) num_fx=weed_leaf_num_elements(filter_map,"init_events");
      if (num_fx>0) {
	init_events=weed_get_voidptr_array(filter_map,"init_events",&error);
	for (i=0;i<num_fx;i++) {
	  init_event=(weed_plant_t *)init_events[i];
	  if (init_event!=NULL) {

	    num_in_tracks=0;
	    is_input=FALSE;
	    if (weed_plant_has_leaf(init_event,"in_tracks")) {
	      num_in_tracks=weed_leaf_num_elements(init_event,"in_tracks");
	      if (num_in_tracks>0) {
		in_tracks=weed_get_int_array(init_event,"in_tracks",&error);
		for (j=0;j<num_in_tracks;j++) {
		  if (in_tracks[j]==mt->current_track) {
		    is_input=TRUE;
		    break;
		  }
		}
		weed_free(in_tracks);
	      }
	    }
	    num_out_tracks=0;
	    is_output=FALSE;
	    if (weed_plant_has_leaf(init_event,"out_tracks")) {
	      num_out_tracks=weed_leaf_num_elements(init_event,"out_tracks");
	      if (num_out_tracks>0) {
		out_tracks=weed_get_int_array(init_event,"out_tracks",&error);
		def_out_track=out_tracks[0];
		for (j=0;j<num_out_tracks;j++) {
		  if (out_tracks[j]==mt->current_track) {
		    is_output=TRUE;
		    break;
		  }
		}
		weed_free(out_tracks);
	      }
	    }

	    has_effect=TRUE;

	    fxcount++;

	    fhash=weed_get_string_value(init_event,"filter",&error);
	    fidx=weed_get_idx_for_hashname(fhash,TRUE);
	    weed_free(fhash);
	    fname=weed_filter_get_name(fidx);

	    if (!is_input) {
	      txt=g_strdup_printf(_("%s output"),fname);
	    }
	    else if (!is_output&&num_out_tracks>0) {
	      if (def_out_track>-1) {
		yeventbox=(GtkWidget *)g_list_nth_data(mt->video_draws,def_out_track);
		olayer=GPOINTER_TO_INT(g_object_get_data(G_OBJECT(yeventbox),"layer_number"));
		otrackname=g_strdup_printf(_("layer %d"),olayer);
	      }
	      else otrackname=g_strdup(_("audio track"));
	      txt=g_strdup_printf(_("%s to %s"),fname,otrackname);
	      g_free(otrackname);
	    }
	    else {
	      txt=g_strdup(fname);
	    }
	    xeventbox=gtk_event_box_new();
	    g_object_set_data(G_OBJECT(xeventbox),"init_event",(gpointer)init_event);

	    gtk_widget_add_events (xeventbox, GDK_BUTTON_RELEASE_MASK | GDK_BUTTON_PRESS_MASK);
	    if (palette->style&STYLE_1) {
	      if (palette->style&STYLE_3) {
		gtk_widget_modify_bg(xeventbox, GTK_STATE_NORMAL, &palette->normal_back);
		gtk_widget_modify_bg(xeventbox, GTK_STATE_SELECTED, &palette->menu_and_bars);
	      }
	      else {
		gtk_widget_modify_bg(xeventbox, GTK_STATE_NORMAL, &palette->menu_and_bars);
		gtk_widget_modify_bg(xeventbox, GTK_STATE_SELECTED, &palette->normal_back);
	      }
	    }

	    if (init_event==mt->selected_init_event) gtk_widget_set_state(xeventbox,GTK_STATE_SELECTED);
	    vbox=gtk_vbox_new(FALSE,0);

	    gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);
	    gtk_container_add (GTK_CONTAINER (xeventbox), vbox);
	    label=gtk_label_new(txt);
	    g_free(txt);
	    g_free(fname);

	    if (palette->style&STYLE_1) {
	      gtk_widget_modify_fg (label, GTK_STATE_NORMAL, &palette->info_text);
	      gtk_widget_modify_fg (label, GTK_STATE_SELECTED, &palette->info_text);
	    }
	    gtk_container_set_border_width (GTK_CONTAINER (xeventbox), 5);
	    gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
	    gtk_box_pack_start (GTK_BOX (mt->fx_list_vbox), xeventbox, FALSE, FALSE, 0);

	    g_signal_connect (GTK_OBJECT (xeventbox), "button_press_event",
			      G_CALLBACK (fx_ebox_pressed),
			      (gpointer)mt);
	  }
	}
	weed_free(init_events);
      }
    }

    bbox=gtk_hbutton_box_new();
    gtk_button_box_set_layout (GTK_BUTTON_BOX (bbox), GTK_BUTTONBOX_SPREAD);
    gtk_box_pack_end (GTK_BOX (mt->fx_list_box), bbox, FALSE, FALSE, 0);

    mt->prev_fm_button = gtk_button_new_with_mnemonic (_("_Prev filter map")); // Note to translators: previous filter map
    gtk_box_pack_start (GTK_BOX (bbox), mt->prev_fm_button, FALSE, FALSE, 0);

    gtk_widget_set_sensitive(mt->prev_fm_button,(prev_fm_event=get_prev_fm(mt,mt->current_track,frame_event))!=NULL&&
			     (get_event_timecode(prev_fm_event)!=(get_event_timecode(frame_event))));
    
    g_signal_connect (GTK_OBJECT (mt->prev_fm_button), "clicked",
		      G_CALLBACK (on_prev_fm_clicked),
		      (gpointer)mt);

    if (fxcount>1) {
      mt->fx_ibefore_button = gtk_button_new_with_mnemonic (_("Insert _before"));
      gtk_box_pack_start (GTK_BOX (bbox), mt->fx_ibefore_button, FALSE, FALSE, 0);
      gtk_widget_set_sensitive(mt->fx_ibefore_button,mt->fx_order==FX_ORD_NONE&&
			       get_event_timecode(mt->fm_edit_event)==get_event_timecode(frame_event)&&
			       mt->selected_init_event!=NULL);
      
      g_signal_connect (GTK_OBJECT (mt->fx_ibefore_button), "clicked",
			G_CALLBACK (on_fx_insb_clicked),
			(gpointer)mt);
      
      mt->fx_iafter_button = gtk_button_new_with_mnemonic (_("Insert _after"));
      gtk_box_pack_start (GTK_BOX (bbox), mt->fx_iafter_button, FALSE, FALSE, 0);
      gtk_widget_set_sensitive(mt->fx_iafter_button,mt->fx_order==FX_ORD_NONE&&
			       get_event_timecode(mt->fm_edit_event)==get_event_timecode(frame_event)&&
			       mt->selected_init_event!=NULL);
      
      g_signal_connect (GTK_OBJECT (mt->fx_iafter_button), "clicked",
			G_CALLBACK (on_fx_insa_clicked),
			(gpointer)mt);
      
    }
    else {
      mt->fx_ibefore_button=mt->fx_iafter_button=NULL;
    }

    mt->next_fm_button = gtk_button_new_with_mnemonic (_("_Next filter map"));
    gtk_box_pack_end (GTK_BOX (bbox), mt->next_fm_button, FALSE, FALSE, 0);
    
    gtk_widget_set_sensitive(mt->next_fm_button,(next_fm_event=get_next_fm(mt,mt->current_track,frame_event))!=NULL&&
			     (get_event_timecode(next_fm_event)>get_event_timecode(frame_event)));

    g_signal_connect (GTK_OBJECT (mt->next_fm_button), "clicked",
		      G_CALLBACK (on_next_fm_clicked),
		      (gpointer)mt);

    if (has_effect) {
      gtk_widget_show_all(mt->fx_list_box);
      do_fx_list_context(mt,fxcount);
    }
    else {
      label=gtk_label_new(_("\n\nNo effects at current track,\ncurrent time.\n"));
      gtk_box_pack_start (GTK_BOX (mt->fx_list_box), label, TRUE, TRUE, 0);
      gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_CENTER);
      gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
      gtk_widget_show(mt->fx_list_box);
      gtk_widget_show(label);
    }
    break;

  case POLY_COMP:
    set_poly_tab(mt,POLY_COMP);
    clear_context(mt);
    add_context_label (mt,(_("Drag a compositor anywhere\non the timeline\nto apply it to the selected region.")));
    tab_set=TRUE;
    ++nins;
  case POLY_TRANS:
    if (!tab_set){
      set_poly_tab(mt,POLY_TRANS);
      clear_context(mt);
      add_context_label (mt,(_("Drag a transition anywhere\non the timeline\nto apply it to the selected region.")));
    }
    tab_set=TRUE; 
    ++nins;
  case POLY_EFFECTS:
    if (!tab_set){
      set_poly_tab(mt,POLY_EFFECTS);
      clear_context(mt);
      add_context_label (mt,(_("Effects can be dragged\nonto blocks on the timeline.")));
    }
    mt->fx_list_box=gtk_vbox_new(FALSE,0);
    mt->fx_list_scroll = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (mt->fx_list_scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start (GTK_BOX (mt->fx_list_box), mt->fx_list_scroll, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(mt->poly_box),mt->fx_list_box,TRUE,TRUE,0);

    mt->fx_list_vbox=gtk_vbox_new(FALSE,10);
    gtk_container_set_border_width (GTK_CONTAINER (mt->fx_list_vbox), 10);
    gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (mt->fx_list_scroll), mt->fx_list_vbox);

    if (mt->poly_state==POLY_COMP) nins=1000000;
    populate_filter_box(mt->fx_list_vbox,nins,mt);

    gtk_widget_show_all(mt->fx_list_box);
    break;

  default:
    break;
  }
  gtk_widget_queue_draw(mt->poly_box);

  if (prefs->open_maximised) {
    gtk_window_maximize (GTK_WINDOW(mt->window));
  }

}


static void mouse_select_start(GtkWidget *widget, lives_mt *mt) {
  gdouble timesecs;
  gint min_x;

  gtk_widget_set_sensitive(mt->mm_menuitem,FALSE);
  gtk_widget_set_sensitive(mt->view_sel_events,FALSE);
  
  gdk_window_get_pointer(GDK_WINDOW (mt->timeline->window), &mt->sel_x, &mt->sel_y, NULL);
  timesecs=get_time_from_x(mt,mt->sel_x);
  mt->region_start=mt->region_end=mt->region_init=timesecs;

  mt->region_updating=TRUE;
  on_timeline_update(mt->timeline_eb,NULL,mt);
  mt->region_updating=FALSE;

  gdk_window_get_pointer(GDK_WINDOW (mt->tl_eventbox->window), &mt->sel_x, &mt->sel_y, NULL);
  gdk_window_get_position(GDK_WINDOW (mt->timeline_eb->window), &min_x, NULL);

  if (mt->sel_x<min_x) mt->sel_x=min_x;
  if (mt->sel_y<0.) mt->sel_y=0.;

  gtk_widget_queue_draw(mt->tl_hbox);
  gtk_widget_queue_draw(mt->timeline);

  mt->tl_selecting=TRUE;

}

void mouse_select_end(GtkWidget *widget, lives_mt *mt) {
  mt->tl_selecting=FALSE;
  gdk_window_get_pointer(GDK_WINDOW (mt->timeline->window), &mt->sel_x, &mt->sel_y, NULL);
  gtk_widget_set_sensitive(mt->mm_menuitem,TRUE);
  gtk_widget_queue_draw(mt->tl_eventbox);
  on_timeline_release(mt->timeline_reg,NULL,mt);
}


static void mouse_select_move(GtkWidget *widget, lives_mt *mt) {
  gint x,y;
  gint start_x,start_y,width,height;
  gint current_track=mt->current_track;
  int i;
  GtkWidget *xeventbox;
  gint rel_x,rel_y,min_x;
  gint offs_y_start,offs_y_end,xheight;
  GtkWidget *checkbutton;

  if (mt->block_selected!=NULL) unselect_all(mt);

  gdk_window_get_pointer(GDK_WINDOW (mt->tl_eventbox->window), &x, &y, NULL);
  gdk_window_get_position(GDK_WINDOW (mt->timeline_eb->window), &min_x, NULL);

  if (x<min_x) x=min_x;
  if (y<0.) y=0.;

  gtk_widget_queue_draw(mt->tl_hbox);
  gdk_window_process_updates(mt->tl_eventbox->window,FALSE);

  if (x>=mt->sel_x) {
    start_x=mt->sel_x;
    width=x-mt->sel_x;
  }
  else {
    start_x=x;
    width=mt->sel_x-x;
  }
  if (y>=mt->sel_y) {
    start_y=mt->sel_y;
    height=y-mt->sel_y;
  }
  else {
    start_y=y;
    height=mt->sel_y-y;
  }

  if (start_x<0) start_x=0;
  if (start_y<0) start_y=0;

  gdk_draw_rectangle (GDK_DRAWABLE(mt->tl_eventbox->window), mt->window->style->black_gc, TRUE, 
		      start_x, start_y, width, height);

  for (i=0;i<mt->num_video_tracks;i++) {
    xeventbox=(GtkWidget *)g_list_nth_data(mt->video_draws,i);
    if (GPOINTER_TO_INT(g_object_get_data(G_OBJECT(xeventbox),"hidden"))==0) {
      xheight=xeventbox->allocation.height;
      checkbutton=(GtkWidget *)g_object_get_data(G_OBJECT(xeventbox),"checkbutton");
      gdk_window_get_position(xeventbox->window,&rel_x,&rel_y);
      if (start_y>(rel_y+xheight/2)||(start_y+height)<(rel_y+xheight/2)) {
#ifdef ENABLE_GIW
	if (!prefs->lamp_buttons) {
#endif
	  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkbutton))) {
	    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbutton),FALSE);
	    mt->current_track=current_track;
	    track_select(mt);
	  }
#ifdef ENABLE_GIW
	}
	else {
	  if (giw_led_get_mode(GIW_LED(checkbutton))) {
	    giw_led_set_mode(GIW_LED(checkbutton),FALSE);
	    mt->current_track=current_track;
	    track_select(mt);
	  }
	}
#endif
	continue;
      }
      offs_y_start=0;
      offs_y_end=xheight;
      if (start_y<rel_y+xheight) {
	offs_y_start=start_y-rel_y;
	gdk_draw_line (xeventbox->window, mt->window->style->black_gc, start_x-rel_x, offs_y_start, 
		       start_x+width-rel_x-1, offs_y_start);
      }
      if (start_y+height<rel_y+xheight) {
	offs_y_end=start_y-rel_y+height;
	gdk_draw_line (xeventbox->window, mt->window->style->black_gc, start_x-rel_x, offs_y_end, 
		       start_x+width-rel_x-1, offs_y_end);
      }
      gdk_draw_line (xeventbox->window, mt->window->style->black_gc, start_x-rel_x, offs_y_start, start_x-rel_x, 
		     offs_y_end);
      gdk_draw_line (xeventbox->window, mt->window->style->black_gc, start_x+width-rel_x-1, offs_y_start, 
		     start_x+width-rel_x-1, offs_y_end);

#ifdef ENABLE_GIW
      if (!prefs->lamp_buttons) {
#endif
	if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkbutton))) {
	  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbutton),TRUE);
	  mt->current_track=current_track;
	  track_select(mt);
	}
#ifdef ENABLE_GIW
      }
      else {
	if (!giw_led_get_mode(GIW_LED(checkbutton))) {
	  giw_led_set_mode(GIW_LED(checkbutton),TRUE);
	  mt->current_track=current_track;
	  track_select(mt);
	}
      }
#endif
    }
  }

  if (widget!=mt->timeline_eb) {
    mt->region_updating=TRUE;
    on_timeline_update(mt->timeline_eb,NULL,mt);
    mt->region_updating=FALSE;
  }
  
}




void do_block_context (lives_mt *mt, GdkEventButton *event, track_rect *block) {
  // pop up a context menu when a selected block is right clicked on

  // unfinished...

  GtkWidget *delete_block;
  GtkWidget *split_here;
  GtkWidget *list_fx_here;
  GtkWidget *selblock;
  GtkWidget *avol;
  GtkWidget *menu=gtk_menu_new();
  int error;

  //mouse_select_end(NULL,mt);

  gtk_menu_set_title (GTK_MENU(menu),_("LiVES: Selected block/frame"));

  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(menu, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }
  
  selblock = gtk_menu_item_new_with_mnemonic (_("_Select this block"));
  gtk_container_add (GTK_CONTAINER (menu), selblock);

  g_signal_connect (GTK_OBJECT (selblock), "activate",
		    G_CALLBACK (selblock_cb),
		    (gpointer)mt);


  
  if (block->ordered) { // TODO
    split_here = gtk_menu_item_new_with_mnemonic (_("_Split block here"));
    gtk_container_add (GTK_CONTAINER (menu), split_here);
    
    g_signal_connect (GTK_OBJECT (split_here), "activate",
		      G_CALLBACK (on_split_activate),
		      (gpointer)mt);
  }

  list_fx_here = gtk_menu_item_new_with_mnemonic (_("List _effects here"));
  gtk_container_add (GTK_CONTAINER (menu), list_fx_here);

  g_signal_connect (GTK_OBJECT (list_fx_here), "activate",
		    G_CALLBACK (list_fx_here_cb),
		    (gpointer)mt);

  if (is_audio_eventbox(mt,block->eventbox)&&mt->avol_init_event!=NULL) {
    gchar *avol_fxname=weed_get_string_value(get_weed_filter(mt->avol_fx),"name",&error);
    gchar *text=g_strdup_printf(_("_Adjust %s"),avol_fxname);
    avol = gtk_menu_item_new_with_mnemonic (text);
    g_free(avol_fxname);
    g_free(text);
    gtk_container_add (GTK_CONTAINER (menu), avol);

    g_signal_connect (GTK_OBJECT (avol), "activate",
		      G_CALLBACK (mt_avol_quick),
		      (gpointer)mt);
    
    if (mt->event_list==NULL) gtk_widget_set_sensitive(avol,FALSE);

  }


  delete_block = gtk_menu_item_new_with_mnemonic (_("_Delete this block"));
  gtk_container_add (GTK_CONTAINER (menu), delete_block);
  if (mt->is_rendering) gtk_widget_set_sensitive(delete_block,FALSE);

  g_signal_connect (GTK_OBJECT (delete_block), "activate",
		    G_CALLBACK (delete_block_cb),
		    (gpointer)mt);

  gtk_widget_show_all (menu);
  
  gtk_menu_popup (GTK_MENU(menu), NULL, NULL, NULL, NULL, event->button, event->time);


}


void do_track_context (lives_mt *mt, GdkEventButton *event, gdouble timesecs, gint track) {
  // pop up a context menu when track is right clicked on

  // unfinished...

  GtkWidget *insert_here,*avol;
  GtkWidget *menu=gtk_menu_new();
  gboolean has_something=FALSE;
  gboolean needs_idlefunc=FALSE;

  if (mt->idlefunc>0) {
    g_source_remove(mt->idlefunc);
    mt->idlefunc=0;
    needs_idlefunc=TRUE;
  }

  mouse_select_end(NULL,mt);

  gtk_menu_set_title (GTK_MENU(menu),_("LiVES: Selected frame"));

  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(menu, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }

  if (mt->file_selected>0&&((track<0&&mainw->files[mt->file_selected]->achans>0&&
			     mainw->files[mt->file_selected]->laudio_time>0.)||
			    (track>=0&&mainw->files[mt->file_selected]->frames>0))) {
    if (track>=0) {
      insert_here = gtk_menu_item_new_with_mnemonic (_("_Insert here"));
      g_signal_connect (GTK_OBJECT (insert_here), "activate",
			G_CALLBACK (insert_at_ctx_cb),
			(gpointer)mt);
    }
    else {
      insert_here = gtk_menu_item_new_with_mnemonic (_("_Insert audio here"));
      g_signal_connect (GTK_OBJECT (insert_here), "activate",
			G_CALLBACK (insert_audio_at_ctx_cb),
			(gpointer)mt);
    }
    gtk_container_add (GTK_CONTAINER (menu), insert_here);
    has_something=TRUE;
  }


  if (mt->audio_draws!=NULL&&(track<0||mt->opts.pertrack_audio)&&mt->event_list!=NULL) {
    int error;
    gchar *avol_fxname=weed_get_string_value(get_weed_filter(mt->avol_fx),"name",&error);
    gchar *text=g_strdup_printf(_("_Adjust %s"),avol_fxname);
    avol = gtk_menu_item_new_with_mnemonic (text);
    g_free(avol_fxname);
    g_free(text);
    gtk_container_add (GTK_CONTAINER (menu), avol);

    g_signal_connect (GTK_OBJECT (avol), "activate",
		      G_CALLBACK (mt_avol_quick),
		      (gpointer)mt);

    if (mt->event_list==NULL) gtk_widget_set_sensitive(avol,FALSE);


    has_something=TRUE;
  }


  if (has_something) {
    gtk_widget_show_all (menu);
    gtk_menu_popup (GTK_MENU(menu), NULL, NULL, NULL, NULL, event->button, event->time);
  }
  else gtk_widget_destroy(menu);

  if (needs_idlefunc) {
    mt->idlefunc=mt_idle_add(mt);
  }


}


gboolean on_track_release (GtkWidget *eventbox, GdkEventButton *event, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  gint x,y;
  gdouble timesecs;
  gint track=0;
  GtkWidget *xeventbox;
  GtkWidget *oeventbox;
  GtkWidget *xlabelbox;
  GtkWidget *xahbox;
  GdkWindow *window;
  gint win_x,win_y;
  int i;
  gint old_track=mt->current_track;
  gboolean got_track=FALSE;
  gboolean needs_idlefunc=FALSE;
  weed_timecode_t tc,tcpp;

  if (mt->idlefunc>0) {
    g_source_remove(mt->idlefunc);
    mt->idlefunc=0;
    needs_idlefunc=TRUE;
  }

  set_cursor_style(mt,LIVES_CURSOR_BUSY,0,0,0,0,0);

  gdk_window_get_pointer(GDK_WINDOW (eventbox->window), &x, &y, NULL);
  timesecs=get_time_from_x(mt,x);
  tc=timesecs*U_SECL;

  window=gdk_display_get_window_at_pointer (mt->display,&win_x,&win_y);
  
  if (cfile->achans>0) {
    for (i=0;i<g_list_length(mt->audio_draws);i++) {
      xeventbox=(GtkWidget *)g_list_nth_data(mt->audio_draws,i);
      oeventbox=(GtkWidget *)g_object_get_data(G_OBJECT(xeventbox),"owner");
      if (i>=mt->opts.back_audio_tracks&&!GPOINTER_TO_INT(g_object_get_data(G_OBJECT(oeventbox),"expanded"))) continue;
      xlabelbox=(GtkWidget *)g_object_get_data(G_OBJECT(xeventbox),"labelbox");
      xahbox=(GtkWidget *)g_object_get_data(G_OBJECT(xeventbox),"ahbox");
      if (xeventbox->window==window||xlabelbox->window==window||xahbox->window==window) {
	track=i-1;
	got_track=TRUE;
	mt->aud_track_selected=TRUE;
	break;
      }
    }
  }
  if (track!=-1) {
    for (i=0;i<g_list_length(mt->video_draws);i++) {
      xeventbox=(GtkWidget *)g_list_nth_data(mt->video_draws,i);
      xlabelbox=(GtkWidget *)g_object_get_data(G_OBJECT(xeventbox),"labelbox");
      xahbox=(GtkWidget *)g_object_get_data(G_OBJECT(xeventbox),"ahbox");
      if (xeventbox->window==window||xlabelbox->window==window||xahbox->window==window) {
	track=GPOINTER_TO_INT(g_object_get_data(G_OBJECT(xeventbox),"layer_number"));
	mt->aud_track_selected=FALSE;
	got_track=TRUE;
	break;
      }
    }
  }

  if (mt->opts.mouse_mode==MOUSE_MODE_SELECT) {
    mouse_select_end(eventbox,mt);
    g_signal_handler_block (mt->tl_eventbox,mt->mouse_mot2);
    mt->sel_y-=y+2;
  }
  else {
    if (mt->hotspot_x!=0||mt->hotspot_y!=0) {
      GdkScreen *screen;
      gint abs_x,abs_y;

      gint height=GTK_WIDGET(g_list_nth_data(mt->video_draws,0))->allocation.height;
      gdk_display_get_pointer(mt->display,&screen,&abs_x,&abs_y,NULL);
#if GLIB_CHECK_VERSION(2,8,0)
      gdk_display_warp_pointer(mt->display,screen,abs_x+mt->hotspot_x,abs_y+mt->hotspot_y-height/2);

      // we need to call this to warp the pointer, but gtk+ behaves like a PITA
      if (mt->idlefunc>0) g_source_remove(mt->idlefunc);
      threaded_dialog_spin();
      g_main_context_iteration(NULL,FALSE);
      threaded_dialog_spin();
#endif
    }

    if (got_track&&!mt->is_rendering&&mt->putative_block!=NULL&&mainw->playing_file==-1&&
	event->button==1&&event->time!=dclick_time) {
      weed_timecode_t start_tc;
      mt_desensitise(mt);

      start_tc=get_event_timecode(mt->putative_block->start_event);

      // timecodes per pixel
      tcpp=U_SEC*((mt->tl_max-mt->tl_min)/(gdouble)GTK_WIDGET(g_list_nth_data(mt->video_draws,0))->allocation.width);

      // need to move at least 1.5 pixels, or to another track
      if ((track!=mt->current_track||(tc-start_tc>(tcpp*3/2))||(start_tc-tc>(tcpp*3/2)))&&
	  ((old_track<0&&track<0)||(old_track>=0&&track>=0))) {
	move_block(mt,mt->putative_block,timesecs,old_track,track);
	mt->putative_block=NULL;

	gdk_window_get_pointer(GDK_WINDOW (eventbox->window), &x, &y, NULL);
	timesecs=get_time_from_x(mt,x);

	mt_tl_move(mt,timesecs-GTK_RULER(mt->timeline)->position);
      }
    }

  }
  
  if (mainw->playing_file==-1) mt_sensitise(mt);
  mt->hotspot_x=mt->hotspot_y=0;
  mt->putative_block=NULL;

  set_cursor_style(mt,LIVES_CURSOR_NORMAL,0,0,0,0,0);
  gtk_widget_set_sensitive(mt->mm_menuitem,TRUE);

  if (needs_idlefunc) {
    mt->idlefunc=mt_idle_add(mt);
  }

  return TRUE;
}




gboolean on_track_header_click (GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  if (mt->opts.mouse_mode==MOUSE_MODE_SELECT) {
    mouse_select_start(widget,mt);
    g_signal_handler_unblock (widget,mt->mouse_mot1);
  }
  return TRUE;
}

gboolean on_track_header_release (GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  if (mt->opts.mouse_mode==MOUSE_MODE_SELECT) {
    mouse_select_end(widget,mt);
    g_signal_handler_block (widget,mt->mouse_mot1);
  }
  return TRUE;
}

gboolean on_track_between_click (GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  if (mt->opts.mouse_mode==MOUSE_MODE_SELECT) {
    mouse_select_start(widget,mt);
    g_signal_handler_unblock (mt->tl_eventbox,mt->mouse_mot2);
  }
  return TRUE;
}

gboolean on_track_between_release (GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  if (mt->opts.mouse_mode==MOUSE_MODE_SELECT) {
    mouse_select_end(widget,mt);
    g_signal_handler_block (mt->tl_eventbox,mt->mouse_mot2);
  }
  return TRUE;
}



gboolean on_track_click (GtkWidget *eventbox, GdkEventButton *event, gpointer user_data) {
  gint x,y;
  track_rect *block;
  lives_mt *mt=(lives_mt *)user_data;
  gdouble timesecs;
  gint track;
  int filenum=-1;

  mt->aud_track_selected=is_audio_eventbox(mt,eventbox);

  gtk_widget_set_sensitive(mt->mm_menuitem,FALSE);

  gdk_window_get_pointer(GDK_WINDOW (eventbox->window), &x, &y, NULL);
  timesecs=get_time_from_x(mt,x+mt->hotspot_x);

  if (cfile->achans==0||mt->audio_draws==NULL||(mt->opts.back_audio_tracks==0||eventbox!=mt->audio_draws->data)) 
    track=GPOINTER_TO_INT(g_object_get_data(G_OBJECT(eventbox),"layer_number"));
  else track=-1;
  block=mt->putative_block=get_block_from_time(eventbox,timesecs,mt);

  unselect_all(mt); // set all blocks unselected

  if (mt->opts.mouse_mode==MOUSE_MODE_SELECT&&event->type==GDK_BUTTON_PRESS) {
    mouse_select_start(eventbox,mt);
    g_signal_handler_unblock (mt->tl_eventbox,mt->mouse_mot2);
  }
  else {
    if (event->type!=GDK_BUTTON_PRESS) {
      // this is a double-click
      dclick_time=gdk_event_get_time((GdkEvent *)event);
      select_block(mt);
      mt->putative_block=NULL;
    }
    else {
      // single click, TODO - locate the frame for the track in event_list

      if (mainw->playing_file==-1) {
	mt->fm_edit_event=NULL;
	mt_tl_move(mt,timesecs-GTK_RULER (mt->timeline)->position);
      }

      // for a double click, gdk normally sends 2 single click events, 
      // followed by a double click

      // calling mt_tl_move causes any double click to be triggered during
      // the second single click and then we return here 
      // however, this is quite useful as we can skip the next bit

      if (event->time!=dclick_time) {
	show_track_info(mt,eventbox,track,timesecs);
	if (block!=NULL) {
	  if (cfile->achans==0||mt->audio_draws==NULL||!is_audio_eventbox(mt,eventbox)) 
	    filenum=get_frame_event_clip(block->start_event,track);
	  else filenum=get_audio_frame_clip(block->start_event,-1);
	  if (filenum!=mainw->scrap_file&&filenum!=mainw->ascrap_file) {
	    mt->clip_selected=mt_clip_from_file(mt,filenum);
	    mt_clip_select(mt,TRUE);
	  }

	  if (event->button!=3&&!mt->is_rendering) {
	    gdouble start_secs,end_secs;
	    
	    GdkScreen *screen;
	    gint abs_x,abs_y;
	    
	    gint ebwidth=GTK_WIDGET(mt->timeline)->allocation.width;
	    
	    gdouble width=((end_secs=(get_event_timecode(block->end_event)/U_SEC))-
			   (start_secs=(get_event_timecode(block->start_event)/U_SEC))+1./mt->fps);
	    gint height;
	    
	    // start point must be on timeline to move a block
	    if (block!=NULL&&(mt->tl_min*U_SEC>get_event_timecode(block->start_event))) {
	      mt->putative_block=NULL;
	      return TRUE;
	    }

	    if (cfile->achans==0||!is_audio_eventbox(mt,eventbox)) 
	      height=GTK_WIDGET(g_list_nth_data(mt->video_draws,0))->allocation.height;
	    else height=GTK_WIDGET(mt->audio_draws->data)->allocation.height;
	    
	    width=(width/(mt->tl_max-mt->tl_min)*(gdouble)ebwidth);
	    if (width>ebwidth) width=ebwidth;
	    if (width<2) width=2;

	    mt->hotspot_x=x-(int)((ebwidth*((gdouble)start_secs-mt->tl_min)/(mt->tl_max-mt->tl_min))+.5);
	    mt->hotspot_y=y;
	    //if (mt->hotspot_x>x) mt->hotspot_x=x;
	    gdk_display_get_pointer(mt->display,&screen,&abs_x,&abs_y,NULL);
#if GLIB_CHECK_VERSION(2,8,0)
	    gdk_display_warp_pointer(mt->display,screen,abs_x-mt->hotspot_x,abs_y-y+height/2);
#endif
	    if (track>=0&&!mt->aud_track_selected) set_cursor_style(mt,LIVES_CURSOR_BLOCK,width,height,filenum,0,height/2);
	    else set_cursor_style(mt,LIVES_CURSOR_AUDIO_BLOCK,width,height,filenum,0,height/2);
	  }
	}
      }
      else {
	mt->putative_block=NULL; // please don't move the block
      }
    }
  }

  mt->current_track=track;
  track_select(mt);
  
  if (event->button==3&&mainw->playing_file==-1) {
    gtk_widget_set_sensitive(mt->mm_menuitem,TRUE);
    mt->context_time=timesecs;
    if (block!=NULL) {
      // context menu for a selected block
      mt->putative_block=block;
      do_block_context(mt,event,block);
      return TRUE;
    }
    else {
      do_track_context(mt,event,timesecs,track);
      return TRUE;
    }
  }

  return TRUE;
}

gboolean on_track_move (GtkWidget *widget, GdkEventMotion *event, gpointer user_data) {
  // used for mouse mode SELECT
  lives_mt *mt=(lives_mt *)user_data;
  if (mt->opts.mouse_mode==MOUSE_MODE_SELECT) mouse_select_move(widget,mt);
  return TRUE;
}

gboolean on_track_header_move (GtkWidget *widget, GdkEventMotion *event, gpointer user_data) {
  // used for mouse mode SELECT
  lives_mt *mt=(lives_mt *)user_data;
  if (mt->opts.mouse_mode==MOUSE_MODE_SELECT) mouse_select_move(widget,mt);
  return TRUE;
}

void unpaint_line(lives_mt *mt, GtkWidget *eventbox) {
  GdkImage *st_image;
  guint64 bth,btl;
  gdouble ocurrtime;
  gint xoffset;
  gint ebwidth;

  if (mt->redraw_block) return; // don't update during expose event, otherwise we might leave lines
  if (!GTK_WIDGET_VISIBLE(eventbox)) return;

  ebwidth=GTK_WIDGET(mt->timeline)->allocation.width;
  if ((st_image=(GdkImage *)g_object_get_data(G_OBJECT(eventbox),"backup_image"))!=NULL) {
    // draw over old pointer value
    bth=((guint64)((guint)(GPOINTER_TO_INT(g_object_get_data(G_OBJECT(eventbox),"backup_timepos_h")))))<<32;
    btl=(guint64)((guint)(GPOINTER_TO_INT(g_object_get_data(G_OBJECT(eventbox),"backup_timepos_l"))));
    ocurrtime=(bth+btl)/U_SEC;
    xoffset=(ocurrtime-mt->tl_min)/(mt->tl_max-mt->tl_min)*(gdouble)ebwidth;
    if (xoffset>=0&&xoffset<ebwidth) {
      gdk_draw_image(GDK_DRAWABLE(eventbox->window),mt->window->style->black_gc, st_image, 0, 0, xoffset, 1, xoffset, 
		     eventbox->allocation.height-2);
    }
    g_object_unref(st_image);
    g_object_set_data(G_OBJECT(eventbox),"backup_image",NULL);
  }
}

void unpaint_lines(lives_mt *mt) {
  gint len=g_list_length(mt->video_draws);
  int i;
  GtkWidget *eventbox,*xeventbox;
  gboolean is_video=FALSE;

  for (i=-1;i<len;i++) {
    if (i==-1) {
      if (mt->audio_draws==NULL||(eventbox=(GtkWidget *)mt->audio_draws->data)==NULL) continue;
    }
    else {
      eventbox=(GtkWidget *)g_list_nth_data(mt->video_draws,i);
      is_video=TRUE;
    }
    if (GPOINTER_TO_INT(g_object_get_data(G_OBJECT(eventbox),"hidden"))==0) {
      unpaint_line(mt,eventbox);
    }
    if (is_video) {
      if (GPOINTER_TO_INT(g_object_get_data(G_OBJECT(eventbox),"expanded"))) {
	eventbox=GTK_WIDGET(g_object_get_data(G_OBJECT(eventbox),"atrack"));
	unpaint_line(mt,eventbox);
      }
      else continue;
    }
    if (GPOINTER_TO_INT(g_object_get_data(G_OBJECT(eventbox),"expanded"))) {
      xeventbox=(GtkWidget *)g_object_get_data(G_OBJECT(eventbox),"achan0");
      unpaint_line(mt,xeventbox);
      if (cfile->achans>1) {
	xeventbox=(GtkWidget *)g_object_get_data(G_OBJECT(eventbox),"achan1");
	unpaint_line(mt,xeventbox);
      }
    }
  }
}


void animate_multitrack (lives_mt *mt) {
  // update timeline pointer(s)
  GdkImage *st_image;
  GtkWidget *eventbox=NULL,*aeventbox=NULL;
  gint offset=-1;
  int i;
  gint len=g_list_length (mt->video_draws);
  gdouble currtime=mainw->currticks/U_SEC;
  gint ebwidth=GTK_WIDGET(mt->timeline)->allocation.width;
  gboolean expanded=FALSE;
  gdouble tl_page;
  gboolean is_video=FALSE;

  if (mt->opts.follow_playback) {
    if (currtime>(mt->tl_min+((tl_page=mt->tl_max-mt->tl_min))*.85)&&event_list_get_end_secs(mt->event_list)>mt->tl_max) {
      // scroll right one page
      mt->tl_min+=tl_page*.85;
      mt->tl_max+=tl_page*.85;
      mt_zoom(mt,-1.);
    }
  }

  time_to_string(mt,currtime,TIMECODE_LENGTH);

  GTK_RULER (mt->timeline)->position=currtime;
  gtk_widget_queue_draw (mt->timeline);

  if (mt->redraw_block) return; // don't update during expose event, otherwise we might leave lines

  for (i=-mt->opts.back_audio_tracks;i<len;i++) {
    if (i==-1) {
      if (mt->audio_draws==NULL||((aeventbox=eventbox=(GtkWidget *)mt->audio_draws->data)==NULL)) continue;
    }
    else {
      eventbox=(GtkWidget *)g_list_nth_data(mt->video_draws,i);
      is_video=TRUE;
    }
    expanded=GPOINTER_TO_INT(g_object_get_data(G_OBJECT(eventbox),"expanded"));
    if (GTK_WIDGET_VISIBLE(eventbox)) {
      unpaint_line(mt,eventbox);
      if (offset==-1) {
	offset=(currtime-mt->tl_min)/(mt->tl_max-mt->tl_min)*(gdouble)ebwidth;
      }
      if (offset>0&&offset<ebwidth) {
	st_image=gdk_drawable_copy_to_image(GDK_DRAWABLE(eventbox->window),NULL,offset,1,0,0,1,
					    eventbox->allocation.height-2);
	g_object_set_data(G_OBJECT(eventbox),"backup_image",st_image);
	g_object_set_data(G_OBJECT(eventbox),"backup_timepos_h",
			  GINT_TO_POINTER((gint)(((guint64)(currtime*U_SEC))>>32))); // upper 4 bytes
	g_object_set_data(G_OBJECT(eventbox),"backup_timepos_l",
			  GINT_TO_POINTER((guint)(((guint64)(currtime*U_SEC))&0XFFFFFFFF))); // lower 4 bytes
	gdk_draw_line (GDK_DRAWABLE(eventbox->window), mt->window->style->black_gc, offset, 1, offset, 
		       eventbox->allocation.height-2);
      }
    }
    if (expanded) {
      if (is_video) {
	aeventbox=GTK_WIDGET(g_object_get_data(G_OBJECT(eventbox),"atrack"));
	expanded=GPOINTER_TO_INT(g_object_get_data(G_OBJECT(aeventbox),"expanded"));
	if (GTK_WIDGET_VISIBLE(aeventbox)) {
	  unpaint_line(mt,aeventbox);
	  if (offset==-1) {
	    offset=(currtime-mt->tl_min)/(mt->tl_max-mt->tl_min)*(gdouble)ebwidth;
	  }
	  if (offset>0&&offset<ebwidth) {
	    st_image=gdk_drawable_copy_to_image(GDK_DRAWABLE(aeventbox->window),NULL,offset,1,0,0,1,
						aeventbox->allocation.height-2);
	    g_object_set_data(G_OBJECT(aeventbox),"backup_image",st_image);
	    g_object_set_data(G_OBJECT(aeventbox),"backup_timepos_h",
			      GINT_TO_POINTER((gint)(((guint64)(currtime*U_SEC))>>32))); // upper 4 bytes
	    g_object_set_data(G_OBJECT(aeventbox),"backup_timepos_l",
			      GINT_TO_POINTER((guint)(((guint64)(currtime*U_SEC))&0XFFFFFFFF))); // lower 4 bytes
	    gdk_draw_line (GDK_DRAWABLE(aeventbox->window), mt->window->style->black_gc, offset, 1, offset, 
			   aeventbox->allocation.height-2);
	  }
	}
      }
      if (expanded) eventbox=(GtkWidget *)g_object_get_data(G_OBJECT(aeventbox),"achan0");
      else continue;
      if (GTK_WIDGET_VISIBLE(eventbox)) {
	unpaint_line(mt,eventbox);
	if (offset==-1) {
	  offset=(currtime-mt->tl_min)/(mt->tl_max-mt->tl_min)*(gdouble)ebwidth;
	}
	if (offset>0&&offset<ebwidth) {
	  st_image=gdk_drawable_copy_to_image(GDK_DRAWABLE(eventbox->window),NULL,offset,1,0,0,1,
					      eventbox->allocation.height-2);
	  g_object_set_data(G_OBJECT(eventbox),"backup_image",st_image);
	  g_object_set_data(G_OBJECT(eventbox),"backup_timepos_h",
			    GINT_TO_POINTER((gint)(((guint64)(currtime*U_SEC))>>32))); // upper 4 bytes
	  g_object_set_data(G_OBJECT(eventbox),"backup_timepos_l",
			    GINT_TO_POINTER((guint)(((guint64)(currtime*U_SEC))&0XFFFFFFFF))); // lower 4 bytes
	  gdk_draw_line (GDK_DRAWABLE(eventbox->window), mt->window->style->black_gc, offset, 1, offset, 
			 eventbox->allocation.height-2);
	}
      }
      // expanded right audio
      if (cfile->achans>1) eventbox=(GtkWidget *)g_object_get_data(G_OBJECT(aeventbox),"achan1");
      else continue;
      if (GTK_WIDGET_VISIBLE(eventbox)) {
	unpaint_line(mt,eventbox);
	if (offset==-1) {
	  offset=(currtime-mt->tl_min)/(mt->tl_max-mt->tl_min)*(gdouble)ebwidth;
	}
	if (offset>0&&offset<ebwidth) {
	  st_image=gdk_drawable_copy_to_image(GDK_DRAWABLE(eventbox->window),NULL,offset,1,0,0,1,
					      eventbox->allocation.height-2);
	  g_object_set_data(G_OBJECT(eventbox),"backup_image",st_image);
	  g_object_set_data(G_OBJECT(eventbox),"backup_timepos_h",
			    GINT_TO_POINTER((gint)(((guint64)(currtime*U_SEC))>>32))); // upper 4 bytes
	  g_object_set_data(G_OBJECT(eventbox),"backup_timepos_l",
			    GINT_TO_POINTER((guint)(((guint64)(currtime*U_SEC))&0XFFFFFFFF))); // lower 4 bytes
	  gdk_draw_line (GDK_DRAWABLE(eventbox->window), mt->window->style->black_gc, offset, 1, offset,
			 eventbox->allocation.height-2);
	}
      }
    }
  }
} 






////////////////////////////////////////////////////
// menuitem callbacks


static gboolean multitrack_end (GtkMenuItem *menuitem, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  return multitrack_delete (mt,!(prefs->warning_mask&WARN_MASK_EXIT_MT)||menuitem==NULL);
}



// callbacks for future adding to osc.c
void multitrack_end_cb (GtkMenuItem *menuitem, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  if (mt->is_rendering) return;
  multitrack_end(menuitem,user_data);
}

void insert_here_cb (GtkMenuItem *menuitem, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  if (mt->is_rendering) return;
  multitrack_insert(NULL,user_data);
}

void insert_at_ctx_cb (GtkMenuItem *menuitem, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  if (mt->is_rendering) return;
  mt->use_context=TRUE;
  multitrack_insert(NULL,user_data);
}

void edit_start_end_cb (GtkMenuItem *menuitem, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  if (mt->is_rendering) return;
  multitrack_adj_start_end(NULL,user_data);
}

void close_clip_cb (GtkMenuItem *menuitem, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  if (mt->is_rendering) return;
  on_close_activate(NULL,NULL);
}

void show_clipinfo_cb (GtkMenuItem *menuitem, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  gint current_file=mainw->current_file;
  if (mt->file_selected!=-1) {
    mainw->current_file=mt->file_selected;
    on_show_file_info_activate(NULL,NULL);
    mainw->current_file=current_file;
  }
}

void insert_audio_here_cb (GtkMenuItem *menuitem, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  if (mt->is_rendering) return;
  multitrack_audio_insert(NULL,user_data);
}

void insert_audio_at_ctx_cb (GtkMenuItem *menuitem, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  if (mt->is_rendering) return;
  mt->use_context=TRUE;
  multitrack_audio_insert(NULL,user_data);
}

void delete_block_cb (GtkMenuItem *menuitem, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  if (mt->is_rendering) return;
  on_delblock_activate(NULL,user_data);
}

void selblock_cb (GtkMenuItem *menuitem, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  select_block(mt);
  mt->putative_block=NULL;
}

void list_fx_here_cb (GtkMenuItem *menuitem, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  mt->context_time=-1.;
  on_mt_list_fx_activate(NULL,user_data);
}


///////////////////////////////////////////////////////////

void tc_to_rs (GtkMenuItem *menuitem, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  mt->region_start=GTK_RULER(mt->timeline)->position;
  on_timeline_release(mt->timeline_reg,NULL,mt);
}

void tc_to_re (GtkMenuItem *menuitem, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  mt->region_end=GTK_RULER(mt->timeline)->position;
  on_timeline_release(mt->timeline_reg,NULL,mt);
}


void rs_to_tc (GtkMenuItem *menuitem, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  mt_tl_move(mt,mt->region_start-GTK_RULER(mt->timeline)->position);
}

void re_to_tc (GtkMenuItem *menuitem, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  mt_tl_move(mt,mt->region_end-GTK_RULER(mt->timeline)->position);
}



//////////////////////////////////////////////////



void
on_move_fx_changed (GtkMenuItem *menuitem, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  mt->opts.move_effects=!mt->opts.move_effects;
}


void multitrack_clear_marks (GtkMenuItem *menuitem, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  g_list_free(mt->tl_marks);
  mt->tl_marks=NULL;
  gtk_widget_set_sensitive(mt->clear_marks,FALSE);
  gtk_widget_queue_draw(mt->timeline_reg);
}


void select_all_time (GtkMenuItem *menuitem, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  mt->region_start=0.;
  mt->region_end=mt->end_secs;
  on_timeline_release(mt->timeline_reg,NULL,mt);
}


void select_from_zero_time (GtkMenuItem *menuitem, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  if (mt->region_start==0.&&mt->region_end==0.) mt->region_end=GTK_RULER (mt->timeline)->position;
  mt->region_start=0.;
  on_timeline_release(mt->timeline_reg,NULL,mt);
}

void select_to_end_time (GtkMenuItem *menuitem, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  if (mt->region_start==0.&&mt->region_end==0.) mt->region_start=GTK_RULER (mt->timeline)->position;
  mt->region_end=mt->end_secs;
  on_timeline_release(mt->timeline_reg,NULL,mt);
}



void select_all_vid (GtkMenuItem *menuitem, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  GtkWidget *eventbox,*checkbutton;
  gint current_track=mt->current_track;
  GList *vdr=mt->video_draws;
  int i=0;

  g_signal_handler_block(mt->select_track,mt->seltrack_func);
  if (!gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(mt->select_track))) 
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mt->select_track),TRUE);
  g_signal_handler_unblock(mt->select_track,mt->seltrack_func);

  while (vdr!=NULL) {
    eventbox=(GtkWidget *)vdr->data;
    checkbutton=(GtkWidget *)g_object_get_data(G_OBJECT(eventbox),"checkbutton");

#ifdef ENABLE_GIW
    if (!prefs->lamp_buttons) {
#endif
      if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkbutton))) 
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbutton),TRUE);
#ifdef ENABLE_GIW
    }
    else {
      if (!giw_led_get_mode(GIW_LED(checkbutton))) giw_led_set_mode(GIW_LED(checkbutton),TRUE);
    }
#endif
    mt->current_track=i++;
    // we need to call this since it appears that checkbuttons on hidden tracks don't get updated until shown
    on_seltrack_activate(GTK_MENU_ITEM(mt->select_track),mt);
    vdr=vdr->next;
  }
  mt->current_track=current_track;

}

void select_no_vid (GtkMenuItem *menuitem, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  GtkWidget *eventbox,*checkbutton;
  gint current_track=mt->current_track;
  GList *vdr=mt->video_draws;
  int i=0;

  g_signal_handler_block(mt->select_track,mt->seltrack_func);
  if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(mt->select_track))) 
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mt->select_track),FALSE);
  g_signal_handler_unblock(mt->select_track,mt->seltrack_func);

  while (vdr!=NULL) {
    eventbox=(GtkWidget *)vdr->data;
    checkbutton=(GtkWidget *)g_object_get_data(G_OBJECT(eventbox),"checkbutton");


#ifdef ENABLE_GIW
    if (!prefs->lamp_buttons) {
#endif
      if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkbutton))) 
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbutton),FALSE);
#ifdef ENABLE_GIW
    }
    else {
      if (giw_led_get_mode(GIW_LED(checkbutton))) giw_led_set_mode(GIW_LED(checkbutton),FALSE);
    }
#endif
    mt->current_track=i++;
    // we need to call this since it appears that checkbuttons on hidden tracks don't get updated until shown
    on_seltrack_activate(GTK_MENU_ITEM(mt->select_track),mt);
    vdr=vdr->next;
  }
  mt->current_track=current_track;

}


void
mt_fplay_toggled                (GtkMenuItem     *menuitem,
				 gpointer         user_data)
{
  lives_mt *mt=(lives_mt *)user_data;
  mt->opts.follow_playback=!mt->opts.follow_playback;
  gtk_widget_set_sensitive(mt->follow_play,mt->opts.follow_playback);
}

void
mt_render_vid_toggled                (GtkMenuItem     *menuitem,
				      gpointer         user_data)
{
  lives_mt *mt=(lives_mt *)user_data;
  mt->render_vidp=!mt->render_vidp;
  gtk_widget_set_sensitive(mt->render_aud,mt->render_vidp);
}


void
mt_render_aud_toggled                (GtkMenuItem     *menuitem,
				      gpointer         user_data)
{
  lives_mt *mt=(lives_mt *)user_data;
  mt->render_audp=!mt->render_audp;
  gtk_widget_set_sensitive(mt->render_vid,mt->render_audp);
  gtk_widget_set_sensitive(mt->normalise_aud,mt->render_audp);
}


void
mt_norm_aud_toggled                (GtkMenuItem     *menuitem,
				    gpointer         user_data)
{
  lives_mt *mt=(lives_mt *)user_data;
  mt->normalise_audp=!mt->normalise_audp;
}



void
mt_view_audio_toggled                (GtkMenuItem     *menuitem,
				      gpointer         user_data)
{
  lives_mt *mt=(lives_mt *)user_data;
  mt->opts.show_audio=gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menuitem));

  if (!mt->opts.show_audio) g_object_set_data(G_OBJECT(mt->audio_draws->data),"hidden",
					      GINT_TO_POINTER(TRACK_I_HIDDEN_USER));
  else g_object_set_data(G_OBJECT(mt->audio_draws->data),"hidden",GINT_TO_POINTER(0));

  scroll_tracks(mt,mt->top_track);
  track_select(mt);
}

void
mt_view_ctx_toggled                (GtkMenuItem     *menuitem,
				    gpointer         user_data) {
  // toggle between compact view and expanded view

  lives_mt *mt=(lives_mt *)user_data;
  lives_mt_poly_state_t poly_state=mt->poly_state;
  gboolean needs_idlefunc=FALSE;

  mt->opts.show_ctx=gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menuitem));

  set_mt_play_sizes(mt,cfile->hsize,cfile->vsize);
  mt_show_current_frame(mt, FALSE);

  gtk_widget_set_size_request (mt->fd_frame, mt->play_window_width, mt->play_window_height+2*BORD_HEIGHT);
  gtk_widget_set_size_request (mt->play_box, mt->play_window_width, mt->play_window_height);
  gtk_widget_set_size_request (mt->hbox, -1, mt->play_window_height+2*BORD_HEIGHT);

  if (mt->opts.show_ctx) {
    // set text to expanded
    gtk_tool_button_set_label(GTK_TOOL_BUTTON(mt->eview_button),_ ("Expanded View (d)"));

    gtk_widget_show(mainw->scrolledwindow);
    gtk_widget_show(mt->sep_image);
    gtk_widget_show(mt->context_frame);
  }
  else {
    gtk_tool_button_set_label(GTK_TOOL_BUTTON(mt->eview_button),_ ("Compact View (d)"));

    gtk_widget_hide(mainw->scrolledwindow);
    gtk_widget_hide(mt->sep_image);
    gtk_widget_hide(mt->context_frame);
  }

  gtk_widget_queue_draw(mt->eview_button);

  // disable auto-backup while we redraw the screen
  if (mt->idlefunc>0) {
    g_source_remove(mt->idlefunc);
    mt->idlefunc=0;
    needs_idlefunc=TRUE;
  }
  if (poly_state!=POLY_PARAMS) {
    polymorph(mt,POLY_NONE);
    if (poly_state==POLY_IN_OUT) while (g_main_context_iteration(NULL,FALSE));
    polymorph(mt,poly_state);
  }
  else polymorph(mt,POLY_PARAMS);

  if (poly_state!=POLY_IN_OUT) while (g_main_context_iteration(NULL,FALSE));

  //re-enable auto backup
  if (needs_idlefunc) {
    mt->idlefunc=mt_idle_add(mt);
  }

  if (prefs->open_maximised) {
    gtk_window_maximize (GTK_WINDOW(mt->window));
    gtk_widget_queue_resize(mt->window);
  }

  mt->play_window_width=mt->play_box->allocation.width;
  mt->play_window_height=mt->play_box->allocation.height;
}



void
mt_ign_ins_sel_toggled                (GtkMenuItem     *menuitem,
				       gpointer         user_data)
{
  lives_mt *mt=(lives_mt *)user_data;
  mt->opts.ign_ins_sel=gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menuitem));
}



static void remove_gaps_inner (GtkMenuItem *menuitem, gpointer user_data, gboolean only_first) {
  lives_mt *mt=(lives_mt *)user_data;
  GList *vsel=mt->selected_tracks;
  track_rect *block=NULL;
  gint track;
  GtkWidget *eventbox;
  weed_timecode_t tc,new_tc,tc_last,new_tc_last,tc_first,block_tc;
  gint filenum;
  gboolean did_backup=mt->did_backup;
  GList *track_sel;
  gboolean audio_done=FALSE;
  weed_timecode_t offset=0;

  if (!did_backup&&mt->idlefunc>0) {
    g_source_remove(mt->idlefunc);
    mt->idlefunc=0;
  }

  if (!did_backup) mt_backup(mt,MT_UNDO_REMOVE_GAPS,0);

  //go through selected tracks, move each block as far left as possible

  tc_last=q_gint64(mt->region_end*U_SEC,mt->fps);

  while (vsel!=NULL||(mt->current_track==-1&&!audio_done)) {
    offset=0;
    if (mt->current_track>-1) {
      track=GPOINTER_TO_INT(vsel->data);
      eventbox=(GtkWidget *)g_list_nth_data(mt->video_draws,track);
    }
    else {
      track=-1;
      eventbox=(GtkWidget *)mt->audio_draws->data;
    }
    tc=mt->region_start*U_SEC;
    tc=q_gint64(tc,mt->fps);

    if (mt->opts.grav_mode!=GRAV_MODE_RIGHT) {
      // adjust the region so it begins after any first partially contained block
      block=get_block_before(eventbox,tc/U_SEC,TRUE);
      if (block!=NULL) {
	new_tc=q_gint64(get_event_timecode(block->end_event)+(gdouble)(track>-1)*U_SEC/mt->fps,mt->fps);
	if (new_tc>tc) tc=new_tc;
      }
    }
    else {
      // adjust the region so it ends before any last partially contained block
      block=get_block_after(eventbox,tc_last/U_SEC,TRUE);
      if (block!=NULL) {
	new_tc_last=q_gint64(get_event_timecode(block->start_event)-(gdouble)(track>-1)*U_SEC/mt->fps,mt->fps);
	if (new_tc_last<tc_last) tc_last=new_tc_last;
      }
    }


    if (mt->opts.grav_mode!=GRAV_MODE_RIGHT) {
      // moving left
      // what we do here:
      // find first block in range. move it left to tc
      // then we adjust tc to the end of the block (+ 1 frame for video tracks)
      // and continue until we reach the end of the region

      // note video and audio are treated slightly differently
      // a video block must end before the next one starts
      // audio blocks can start and end at the same frame

      // if we remove only first gap, we move the first block, store how far it moved in offset
      // and then move all other blocks by offset


      while (tc<=tc_last) {
	
	block=get_block_after(eventbox,tc/U_SEC,FALSE);
	if (block==NULL) break;
	
	new_tc=get_event_timecode(block->start_event);
	if (new_tc>tc_last) break;
	
	if (tc<new_tc) {
	  // move this block to tc
	  if (offset>0) tc=q_gint64(new_tc-offset,mt->fps);
	  filenum=get_frame_event_clip(block->start_event,track);
	  mt->clip_selected=mt_clip_from_file(mt,filenum);
	  mt_clip_select(mt,FALSE);
	  if (!mt->did_backup) mt_backup(mt,MT_UNDO_REMOVE_GAPS,0);
	  
	  // save current selected_tracks, move_block may change this
	  track_sel=mt->selected_tracks;
	  mt->selected_tracks=NULL;
	  block=move_block(mt,block,tc/U_SEC,track,track);
	  if (mt->selected_tracks!=NULL) g_list_free(mt->selected_tracks);
	  mt->selected_tracks=track_sel;
	  if (only_first&&offset==0) offset=new_tc-tc;
	}
	tc=q_gint64(get_event_timecode(block->end_event)+(gdouble)(track>-1)*U_SEC/mt->fps,mt->fps);
      }
      if (mt->current_track>-1) vsel=vsel->next;
      else audio_done=TRUE;
    }
    else {
      // moving right
      // here we do the reverse:
      // find last block in range. move it right so it ends at tc
      // then we adjust tc to the start of the block + 1 frame
      // and continue until we reach the end of the region

      tc_first=tc;
      tc=tc_last;
      while (tc>=tc_first) {

	block=get_block_before(eventbox,tc/U_SEC,FALSE);
	if (block==NULL) break;
	
	new_tc=get_event_timecode(block->end_event);
	if (new_tc<tc_first) break;

	// subtract the length of the block to get the start point
	block_tc=new_tc-get_event_timecode(block->start_event)+(gdouble)(track>-1)*U_SEC/mt->fps;

	if (tc>new_tc) {
	  // move this block to tc
	  if (offset>0) tc=q_gint64(new_tc-block_tc+offset,mt->fps);
	  else tc=q_gint64(tc-block_tc,mt->fps);
	  filenum=get_frame_event_clip(block->start_event,track);
	  mt->clip_selected=mt_clip_from_file(mt,filenum);
	  mt_clip_select(mt,FALSE);
	  if (!mt->did_backup) mt_backup(mt,MT_UNDO_REMOVE_GAPS,0);
	  
	  // save current selected_tracks, move_block may change this
	  track_sel=mt->selected_tracks;
	  mt->selected_tracks=NULL;
	  block=move_block(mt,block,tc/U_SEC,track,track);
	  if (mt->selected_tracks!=NULL) g_list_free(mt->selected_tracks);
	  mt->selected_tracks=track_sel;
	  if (only_first&&offset==0) offset=tc-new_tc+block_tc;
	}
	tc=q_gint64(get_event_timecode(block->start_event)-(gdouble)(track>-1)*U_SEC/mt->fps,mt->fps);
      }
      if (mt->current_track>-1) vsel=vsel->next;
      else audio_done=TRUE;
    }
  }
  
  if (!did_backup) {
    if (mt->avol_fx!=-1&&(block==NULL||block->next==NULL)&&mt->audio_draws!=NULL&&mt->audio_draws->data!=NULL&&get_first_event(mt->event_list)!=NULL) {
      apply_avol_filter(mt);
    }
  }

  mt->did_backup=did_backup;
  if (!did_backup&&mt->framedraw!=NULL&&mt->current_rfx!=NULL&&mt->init_event!=NULL&&mt->poly_state==POLY_PARAMS&&weed_plant_has_leaf(mt->init_event,"in_tracks")) {
    tc=q_gint64(gtk_spin_button_get_value(GTK_SPIN_BUTTON(mt->node_spinbutton))*U_SEC+get_event_timecode(mt->init_event),mt->fps);
    get_track_index(mt,tc);
  }

  if (!did_backup) mt->idlefunc=mt_idle_add(mt);

}


void remove_first_gaps (GtkMenuItem *menuitem, gpointer user_data) {
  // remove first gaps in selected time/tracks
  // if gravity is Right then we remove last gaps instead

  remove_gaps_inner(menuitem,user_data,TRUE);
}

void remove_gaps (GtkMenuItem *menuitem, gpointer user_data) {
  remove_gaps_inner(menuitem,user_data,FALSE);
}



static void split_block(lives_mt *mt, track_rect *block, weed_timecode_t tc, gint track, gboolean no_recurse) {
  weed_plant_t *event=block->start_event;
  weed_plant_t *start_event=event;
  weed_plant_t *old_end_event=block->end_event;
  gint frame=0,clip;
  GtkWidget *eventbox;
  track_rect *new_block;
  weed_timecode_t offset_start;
  double seek,new_seek,vel;

  tc=q_gint64(tc,mt->fps);

  if (block==NULL) return;

  mt->no_expose=TRUE;

  while (get_event_timecode(event)<tc) event=get_next_event(event);
  block->end_event=track>=0?get_prev_event(event):event;
  if (!WEED_EVENT_IS_FRAME(block->end_event)) block->end_event=get_prev_frame_event(event);

  if (!WEED_EVENT_IS_FRAME(event)) event=get_next_frame_event(event);
  eventbox=block->eventbox;

  if (cfile->achans==0||mt->audio_draws==NULL||!is_audio_eventbox(mt,eventbox)) {
    if (!no_recurse) {
      // if we have an audio block, split it too
      GtkWidget *aeventbox=GTK_WIDGET(g_object_get_data(G_OBJECT(eventbox),"atrack"));
      if (aeventbox!=NULL) {
	track_rect *ablock=get_block_from_time(aeventbox,tc/U_SEC+1./mt->fps,mt);
	if (ablock!=NULL) split_block(mt,ablock,tc+U_SEC/mt->fps,track,TRUE);
      }
    }
    frame=get_frame_event_frame(event,track);
    clip=get_frame_event_clip(event,track);
  }
  else {
    if (!no_recurse) {
      // if we have a video block, split it too
      GtkWidget *oeventbox=GTK_WIDGET(g_object_get_data(G_OBJECT(eventbox),"owner"));
      if (oeventbox!=NULL) split_block(mt,get_block_from_time(oeventbox,tc/U_SEC-1./mt->fps,mt),tc-U_SEC/mt->fps,track,TRUE);
    }
    clip=get_audio_frame_clip(start_event,track);
    seek=get_audio_frame_seek(start_event,track);
    vel=get_audio_frame_vel(start_event,track);
    event=block->end_event;
    new_seek=seek+(get_event_timecode(event)/U_SEC-get_event_timecode(start_event)/U_SEC)*vel;
    insert_audio_event_at(mt->event_list,event,track,clip,new_seek,vel);
  }

  if (block->ordered||(cfile->achans>0&&is_audio_eventbox(mt,eventbox))) offset_start=block->offset_start-get_event_timecode(start_event)+get_event_timecode(event);
  else offset_start=calc_time_from_frame(clip,frame)*U_SEC;

  new_block=add_block_start_point (GTK_WIDGET(eventbox),tc,clip,offset_start,event,block->ordered);
  new_block->end_event=old_end_event;

  mt->no_expose=FALSE;

  redraw_eventbox(mt,eventbox);

}




static void insgap_inner (lives_mt *mt, gint tnum, gboolean is_sel, gint passnm) {
  // insert a gap in track tnum
  
  // we will process in 2 passes

  // pass 1

  // if there is a block at start time, we split it
  // then we move the frame events for this track, inserting blanks if necessary, and we update all our blocks


  // pass 2

  // FILTER_INITs and FILTER_DEINITS - we move the filter init/deinit if "move effects with blocks" is selected and all in_tracks are in the tracks to be moved
  // (transitions may have one non-moving track)


  track_rect *sblock,*block,*ablock=NULL;
  GtkWidget *eventbox;
  weed_timecode_t tc,new_tc;
  weed_plant_t *event,*new_event=NULL,*last_frame_event;
  gint xnumclips,numclips,naclips;
  int aclip=0,*audio_clips;
  double aseek=0.,avel=0.,*audio_seeks;
  int clip,frame,*clips,*frames,*new_clips,*new_frames;
  int i,error;

  weed_timecode_t start_tc,new_init_tc,init_tc;
  int nintracks,*in_tracks;
  weed_plant_t *init_event;
  GList *slist;
  gboolean found;
  gint notmatched;

  gdouble end_secs;

  switch (passnm) {
  case 1:
    // frames and blocks
    if (tnum>=0) eventbox=(GtkWidget *)g_list_nth_data(mt->video_draws,tnum);
    else eventbox=(GtkWidget *)mt->audio_draws->data;
    tc=q_dbl(mt->region_start,mt->fps);
    sblock=get_block_from_time(eventbox,mt->region_start,mt);

    if (sblock!=NULL) {
      split_block(mt,sblock,tc,tnum,FALSE);
      sblock=sblock->next;
    }
    else {
      sblock=get_block_after(eventbox,mt->region_start,FALSE);
    }

    if (sblock==NULL) return;

    block=sblock;
    while (block->next!=NULL) block=block->next; 
    event=block->end_event;

    if (tnum>=0&&mt->opts.pertrack_audio) {
      GtkWidget *aeventbox=GTK_WIDGET(g_object_get_data(G_OBJECT(eventbox),"atrack"));
      if (aeventbox!=NULL) {
	ablock=get_block_after(aeventbox,mt->region_start,FALSE);
	if (ablock!=NULL) {
	  while (ablock->next!=NULL) ablock=ablock->next; 
	  event=ablock->end_event;
	}
      }
    }

    while (event!=NULL) {
      if (WEED_EVENT_IS_FRAME(event)) {
	tc=get_event_timecode(event);
	new_tc=q_gint64(tc+(mt->region_end-mt->region_start)*U_SEC,mt->fps);
	new_event=event;

	if (tnum>=0&&tc<=get_event_timecode(block->end_event)) {
	  frame=get_frame_event_frame(event,tnum);
	  clip=get_frame_event_clip(event,tnum);
	    
	  if ((new_event=get_frame_event_at(mt->event_list,new_tc,event,TRUE))==NULL) {
	    last_frame_event=get_last_frame_event(mt->event_list);
	    mt->event_list=add_blank_frames_up_to(mt->event_list,last_frame_event,new_tc,mt->fps);
	    new_event=get_last_frame_event(mt->event_list);
	  }
	  
	  remove_frame_from_event(mt->event_list,event,tnum);

	  xnumclips=numclips=weed_leaf_num_elements(new_event,"clips");
	  if (numclips<tnum+1) xnumclips=tnum+1;
	  
	  new_clips=(int *)g_malloc(xnumclips*sizint);
	  new_frames=(int *)g_malloc(xnumclips*sizint);
	  
	  clips=weed_get_int_array(new_event,"clips",&error);
	  frames=weed_get_int_array(new_event,"frames",&error);
	  
	  for (i=0;i<xnumclips;i++) {
	    if (i==tnum) {
	      new_clips[i]=clip;
	      new_frames[i]=frame;
	    }
	    else {
	      if (i<numclips) {
		new_clips[i]=clips[i];
		new_frames[i]=frames[i];
	      }
	      else {
		new_clips[i]=-1;
		new_frames[i]=0;
	      }
	    }
	  }
	  
	  weed_set_int_array(new_event,"clips",xnumclips,new_clips);
	  weed_set_int_array(new_event,"frames",xnumclips,new_frames);
	  
	  weed_free(clips);
	  weed_free(frames);
	  g_free(new_clips);
	  g_free(new_frames);

	}

	if (weed_plant_has_leaf(event,"audio_clips")) {
	  if ((new_event=get_frame_event_at(mt->event_list,new_tc,event,TRUE))==NULL) {
	    last_frame_event=get_last_frame_event(mt->event_list);
	    mt->event_list=add_blank_frames_up_to(mt->event_list,last_frame_event,q_gint64(new_tc,mt->fps),mt->fps);
	    new_event=get_last_frame_event(mt->event_list);
	  }
	  
	  naclips=weed_leaf_num_elements(event,"audio_clips");
	  audio_clips=weed_get_int_array(event,"audio_clips",&error);
	  audio_seeks=weed_get_double_array(event,"audio_seeks",&error);
	  
	  for (i=0;i<naclips;i+=2) {
	    if (audio_clips[i]==tnum) {
	      aclip=audio_clips[i+1];
	      aseek=audio_seeks[i];
	      avel=audio_seeks[i+1];
	    }
	  }
	  
	  weed_free(audio_clips);
	  weed_free(audio_seeks);

	  remove_audio_for_track(event,tnum);
	  insert_audio_event_at(mt->event_list,new_event,tnum,aclip,aseek,avel);

	  if (mt->avol_fx!=-1) {
	    apply_avol_filter(mt);
	  }

	}
	
	if (new_event!=event) {
	  
	  if (ablock!=NULL) {
	    if (event==ablock->end_event) ablock->end_event=new_event;
	    else if (event==ablock->start_event) {
	      ablock->start_event=new_event;
	    }
	  }

	  if (event==block->end_event) block->end_event=new_event;
	  else if (event==block->start_event) {
	    block->start_event=new_event;
	    if (block==sblock) {
	      if (tnum<0||ablock!=NULL) {
		if (ablock!=NULL) block=ablock;
		if (block->prev!=NULL&&block->prev->end_event==event) {
		  // audio block was split, need to add a new "audio off" event
		  insert_audio_event_at(mt->event_list,event,tnum,aclip,0.,0.);
		}
		if (mt->avol_fx!=-1) {
		  apply_avol_filter(mt);
		}
	      }
	      mt_fixup_events(mt,event,new_event);
	      redraw_eventbox(mt,eventbox);
	      return;
	    }
	    block=block->prev;
	  }
	  if (ablock!=NULL&&ablock->start_event==new_event) {
	    ablock=ablock->prev;
	    if (ablock!=NULL&&event==ablock->end_event) ablock->end_event=new_event;
	  }
	  mt_fixup_events(mt,event,new_event);
	}
      }
      if (tnum>=0) event=get_prev_event(event);
      else {
	if (new_event==block->end_event) event=block->start_event;
	else event=block->end_event; // we will have moved to the previous block
      }
    }

    break;


  case 2:
    // FILTER_INITs
    start_tc=q_gint64(mt->region_start*U_SEC,mt->fps);
    event=get_last_event(mt->event_list);

    while (event!=NULL&&(tc=get_event_timecode(event))>=start_tc) {
      if (WEED_EVENT_IS_FILTER_DEINIT(event)) {
	init_event=(weed_plant_t *)weed_get_voidptr_value(event,"init_event",&error);

	if (init_event==mt->avol_init_event) {
	  event=get_prev_event(event);
	  continue;
	}

	// see if all of this filter`s in_tracks were moved
	nintracks=weed_leaf_num_elements(init_event,"in_tracks");
	in_tracks=weed_get_int_array(init_event,"in_tracks",&error);

	if (!is_sel) {
	  if ((nintracks==1&&in_tracks[0]!=mt->current_track)||(nintracks==2&&in_tracks[0]!=mt->current_track&&in_tracks[1]!=mt->current_track)) {
	    event=get_prev_event(event);
	    continue;
	  }
	}
	else {
	  for (i=0;i<nintracks;i++) {
	    slist=mt->selected_tracks;
	    found=FALSE;
	    notmatched=0;
	    while (slist!=NULL&&!found) {
	      if (GPOINTER_TO_INT(slist->data)==in_tracks[i]) found=TRUE;
	      slist=slist->next;
	    }
	    if (!found) {
	      if (nintracks!=2||notmatched>0) return;
	      notmatched=1;
	    }
	  }
	}

	weed_free(in_tracks);

	// move filter_deinit
	new_tc=q_gint64(tc+(mt->region_end-mt->region_start)*U_SEC,mt->fps);
	move_filter_deinit_event(mt->event_list,new_tc,event,mt->fps,TRUE);

	init_tc=get_event_timecode(init_event);

	if (init_tc>=start_tc) {
	  // move filter init
	  new_init_tc=q_gint64(init_tc+(mt->region_end-mt->region_start)*U_SEC,mt->fps);
	  move_filter_init_event(mt->event_list,new_init_tc,init_event,mt->fps);
	}

	// for a transition where only one track moved, pack around the overlap

	if (nintracks==2) {
	  move_event_left(mt->event_list,event,TRUE,mt->fps);
	  if (init_tc>=start_tc&&init_event!=mt->avol_init_event) move_event_right(mt->event_list,init_event,TRUE,mt->fps);
	}
      }
      event=get_prev_event(event);
    }
    break;

  }
  end_secs=event_list_get_end_secs(mt->event_list);
  if (end_secs>mt->end_secs) {
    set_timeline_end_secs(mt,end_secs);
  }
}









void on_insgap_sel_activate (GtkMenuItem     *menuitem,
			     gpointer         user_data) {


  lives_mt *mt=(lives_mt *)user_data;
  GList *slist=mt->selected_tracks;
  gint track;
  gboolean did_backup=mt->did_backup;
  gchar *msg;

  if (!did_backup&&mt->idlefunc>0) {
    g_source_remove(mt->idlefunc);
    mt->idlefunc=0;
  }

  if (!did_backup) mt_backup(mt,MT_UNDO_INSERT_GAP,0);

  while (slist!=NULL) {
    track=GPOINTER_TO_INT(slist->data);
    insgap_inner(mt,track,TRUE,1);
    slist=slist->next;
  }

  if (mt->opts.move_effects) {
    insgap_inner(mt,0,TRUE,2);
  }

  mt->did_backup=did_backup;
  mt_show_current_frame(mt, FALSE);
  msg=g_strdup_printf(_("Inserted gap in selected tracks from time %.4f to %.4f\n"),mt->region_start,mt->region_end);

  d_print(msg);
  g_free(msg);

  if (!did_backup) mt->idlefunc=mt_idle_add(mt);

}



void on_insgap_cur_activate (GtkMenuItem     *menuitem,
			     gpointer         user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  gboolean did_backup=mt->did_backup;
  gchar *msg,*tname;

  if (!did_backup&&mt->idlefunc>0) {
    g_source_remove(mt->idlefunc);
    mt->idlefunc=0;
  }

  if (!did_backup) mt_backup(mt,MT_UNDO_INSERT_GAP,0);

  insgap_inner(mt,mt->current_track,FALSE,1);

  if (mt->opts.move_effects) {
    insgap_inner(mt,mt->current_track,FALSE,2);
  }

  mt->did_backup=did_backup;
  mt_show_current_frame(mt, FALSE);

  tname=get_track_name(mt,mt->current_track,FALSE);
  msg=g_strdup_printf(_("Inserted gap in track %s from time %.4f to %.4f\n"),tname,mt->region_start,mt->region_end);
  g_free(tname);

  d_print(msg);
  g_free(msg);

  if (!did_backup) mt->idlefunc=mt_idle_add(mt);

}



void
multitrack_undo            (GtkMenuItem     *menuitem,
			    gpointer         user_data) {

  lives_mt *mt=(lives_mt *)user_data;
  size_t space_avail=(size_t)(prefs->mt_undo_buf*1024*1024)-mt->undo_buffer_used;
  size_t space_needed;
  mt_undo *last_undo=(mt_undo *)g_list_nth_data(mt->undos,g_list_length(mt->undos)-1-mt->undo_offset);
  unsigned char *memblock,*mem_end;
  mt_undo *new_redo=NULL;
  int i;
  gint current_track;
  gint clip_sel;
  gint avol_fx;
  gint num_tracks;
  gboolean block_is_selected=FALSE;
  gboolean avoid_fx_list=FALSE;
  gchar *msg,*utxt,*tmp;
  gchar *txt;

  gdouble end_secs;
  GList *slist;
  GList *label_list=NULL;
  GList *vlist,*llist;
  GList *seltracks=NULL;
  GList *aparam_view_list;
  GtkWidget *checkbutton,*eventbox,*label;

  if (mt->undo_mem==NULL) return;

  if (mt->idlefunc>0) {
    g_source_remove(mt->idlefunc);
    mt->idlefunc=0;
  }

  mt_desensitise(mt);

  mt->was_undo_redo=TRUE;
  mt->ptr_time=GTK_RULER(mt->timeline)->position;

  if (mt->block_selected!=NULL) block_is_selected=TRUE;
  
  if (last_undo->action!=MT_UNDO_NONE) {
    if (mt->undo_offset==0) {
      add_markers(mt,mt->event_list);
      if ((space_needed=estimate_space(mt,last_undo->action)+sizeof(mt_undo))>space_avail) {
	if (!make_backup_space(mt,space_needed)||mt->undos==NULL) {
	  remove_markers(mt->event_list);
	  mt->idlefunc=mt_idle_add(mt);
	  do_mt_undo_buf_error();
	  mt_sensitise(mt);
	  return;
	}
      }

      new_redo=(mt_undo *)(mt->undo_mem+mt->undo_buffer_used);
      new_redo->action=last_undo->action;
      
      memblock=(unsigned char *)(new_redo)+sizeof(mt_undo);
      new_redo->data_len=space_needed;
      save_event_list_inner(NULL,0,mt->event_list,&memblock);
      mt->undo_buffer_used+=space_needed;
      mt->undos=g_list_append(mt->undos,new_redo);
      mt->undo_offset++;
    }

    current_track=mt->current_track;
    end_secs=mt->end_secs;
    num_tracks=mt->num_video_tracks;
    clip_sel=mt->clip_selected;

    seltracks=g_list_copy(mt->selected_tracks);

    vlist=mt->video_draws;
    while (vlist!=NULL) {
      eventbox=GTK_WIDGET(vlist->data);
      label=GTK_WIDGET(g_object_get_data(G_OBJECT(eventbox),"label"));
      txt=g_strdup(gtk_label_get_text(GTK_LABEL(label)));
      label_list=g_list_append(label_list,txt);
      vlist=vlist->next;
    }

    aparam_view_list=g_list_copy(mt->aparam_view_list);
    avol_fx=mt->avol_fx;
    mt->avol_fx=-1;

    event_list_free(mt->event_list);
    last_undo=(mt_undo *)g_list_nth_data(mt->undos,g_list_length(mt->undos)-1-mt->undo_offset);
    memblock=(unsigned char *)(last_undo)+sizeof(mt_undo);
    mem_end=memblock+last_undo->data_len-sizeof(mt_undo);
    mt->event_list=load_event_list_inner(mt,-1,FALSE,NULL,&memblock,mem_end);

    if (!event_list_rectify(mt,mt->event_list)) {
      event_list_free(mt->event_list);
      mt->event_list=NULL;
    }
    
    if (get_first_event(mt->event_list)==NULL) {
      event_list_free(mt->event_list);
      mt->event_list=NULL;
    }
    
    for (i=0;i<mt->num_video_tracks;i++) {
      delete_video_track(mt,i,FALSE);
    }
    g_list_free(mt->video_draws);
    mt->video_draws=NULL;
    mt->num_video_tracks=0;

    delete_audio_tracks(mt,mt->audio_draws,FALSE);
    mt->audio_draws=NULL;

    mt->fm_edit_event=NULL; // this might have been deleted; etc., c.f. fixup_events
    mt->init_event=NULL;
    mt->selected_init_event=NULL;
    mt->specific_event=NULL;
    mt->avol_init_event=NULL; // we will try to relocate this in mt_init_tracks()

    mt_init_tracks(mt,FALSE);

    if (mt->avol_fx==-1) mt->avol_fx=avol_fx;
    if (mt->avol_fx!=-1) mt->aparam_view_list=g_list_copy(aparam_view_list);
    if (aparam_view_list!=NULL) g_list_free(aparam_view_list);

    add_aparam_menuitems(mt);

    unselect_all(mt);
    for (i=mt->num_video_tracks;i<num_tracks;i++) {
      add_video_track_behind (NULL,mt);
    }

    mt->clip_selected=clip_sel;
    mt_clip_select(mt,FALSE);

    vlist=mt->video_draws;
    llist=label_list;
    while (vlist!=NULL) {
      eventbox=GTK_WIDGET(vlist->data);
      label=GTK_WIDGET(g_object_get_data(G_OBJECT(eventbox),"label"));
      g_free(GTK_LABEL(label)->text);
      GTK_LABEL(label)->text=(gchar *)llist->data;
      vlist=vlist->next;
      llist=llist->next;
    }
    g_list_free(label_list);

    if (mt->event_list!=NULL) remove_markers(mt->event_list);

    mt->selected_tracks=g_list_copy(seltracks);
    slist=mt->selected_tracks;
    while (slist!=NULL) {
      eventbox=(GtkWidget *)g_list_nth_data(mt->video_draws,GPOINTER_TO_INT(slist->data));
      checkbutton=(GtkWidget *)g_object_get_data(G_OBJECT(eventbox),"checkbutton");
#ifdef ENABLE_GIW
      if (!prefs->lamp_buttons) {
#endif
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbutton),TRUE);
#ifdef ENABLE_GIW
      }
      else {
	giw_led_set_mode(GIW_LED(checkbutton),TRUE);
      }
#endif
      slist=slist->next;
    }
    if (seltracks!=NULL) g_list_free(seltracks);

    mt->current_track=current_track;
    track_select(mt);
    if (mt->end_secs!=end_secs&&event_list_get_end_secs(mt->event_list)<=end_secs) set_timeline_end_secs (mt, end_secs);
  }

  mt->undo_offset++;

  if (mt->undo_offset==g_list_length(mt->undos)) mt_set_undoable(mt,MT_UNDO_NONE,NULL,FALSE);
  else {
    mt_undo *undo=(mt_undo *)(g_list_nth_data(mt->undos,g_list_length(mt->undos)-mt->undo_offset-1));
    mt_set_undoable(mt,undo->action,undo->extra,TRUE);
  }
  mt_set_redoable(mt,last_undo->action,last_undo->extra,TRUE);
  GTK_RULER(mt->timeline)->position=mt->ptr_time;
  gtk_widget_queue_draw(mt->timeline);

  utxt=g_utf8_strdown((tmp=get_undo_text(last_undo->action,last_undo->extra)),-1);
  g_free(tmp);

  msg=g_strdup_printf(_("Undid %s\n"),utxt);
  d_print(msg);
  g_free(utxt);
  g_free(msg);

  if (last_undo->action<=1024&&block_is_selected) mt_selblock(NULL, NULL, 0, (GdkModifierType)0, (gpointer)mt);

  // TODO - make sure this is the effect which is now deleted/added...
  if (mt->poly_state==POLY_PARAMS) {
    if (mt->last_fx_type==MT_LAST_FX_BLOCK&&mt->block_selected!=NULL) polymorph(mt,POLY_FX_STACK);
    else polymorph(mt,POLY_CLIPS);
    avoid_fx_list=TRUE;
  }
  if ((last_undo->action==MT_UNDO_FILTER_MAP_CHANGE||mt->poly_state==POLY_FX_STACK)&&!avoid_fx_list) {
    if (last_undo->action==MT_UNDO_FILTER_MAP_CHANGE) mt_tl_move(mt,last_undo->tc-mt->ptr_time);
    polymorph(mt,POLY_FX_STACK);
  }
  if (mt->poly_state!=POLY_PARAMS) mt_show_current_frame(mt, FALSE);
  mt_desensitise(mt);
  mt_sensitise(mt);


  mt->idlefunc=mt_idle_add(mt);
}


void
multitrack_redo            (GtkMenuItem     *menuitem,
			    gpointer         user_data) {

  lives_mt *mt=(lives_mt *)user_data;
  mt_undo *last_redo=(mt_undo *)g_list_nth_data(mt->undos,g_list_length(mt->undos)+1-mt->undo_offset);
  unsigned char *memblock,*mem_end;
  int i;
  gint current_track;
  gdouble end_secs;
  GList *slist;
  gint num_tracks;
  GtkWidget *checkbutton,*eventbox,*label;
  GList *label_list=NULL;
  GList *vlist,*llist;
  gchar *txt;
  GList *seltracks=NULL;
  gint clip_sel;
  gboolean block_is_selected;
  gchar *msg,*utxt,*tmp;
  GList *aparam_view_list;
  gint avol_fx;

  if (mt->undo_mem==NULL) return;

  if (mt->idlefunc>0) {
    g_source_remove(mt->idlefunc);
    mt->idlefunc=0;
  }

  mt_desensitise(mt);

  if (mt->block_selected!=NULL) block_is_selected=TRUE; // TODO *** - need to set track and time 

  mt->was_undo_redo=TRUE;
  mt->ptr_time=GTK_RULER(mt->timeline)->position;

  if (last_redo->action!=MT_UNDO_NONE) {
    current_track=mt->current_track;
    end_secs=mt->end_secs;
    num_tracks=mt->num_video_tracks;
    clip_sel=mt->clip_selected;

    seltracks=g_list_copy(mt->selected_tracks);

    vlist=mt->video_draws;
    while (vlist!=NULL) {
      eventbox=GTK_WIDGET(vlist->data);
      label=GTK_WIDGET(g_object_get_data(G_OBJECT(eventbox),"label"));
      txt=g_strdup(gtk_label_get_text(GTK_LABEL(label)));
      label_list=g_list_append(label_list,txt);
      vlist=vlist->next;
    }

    aparam_view_list=g_list_copy(mt->aparam_view_list);
    avol_fx=mt->avol_fx;
    mt->avol_fx=-1;

    event_list_free(mt->event_list);

    memblock=(unsigned char *)(last_redo)+sizeof(mt_undo);
    mem_end=memblock+last_redo->data_len-sizeof(mt_undo);
    mt->event_list=load_event_list_inner(mt,-1,FALSE,NULL,&memblock,mem_end);
    if (!event_list_rectify(mt,mt->event_list)) {
      event_list_free(mt->event_list);
      mt->event_list=NULL;
    }
    
    if (get_first_event(mt->event_list)==NULL) {
      event_list_free(mt->event_list);
      mt->event_list=NULL;
    }
    
    for (i=0;i<mt->num_video_tracks;i++) {
      delete_video_track(mt,i,FALSE);
    }
    g_list_free(mt->video_draws);
    mt->video_draws=NULL;
    mt->num_video_tracks=0;

    delete_audio_tracks(mt,mt->audio_draws,FALSE);
    mt->audio_draws=NULL;

    mt->fm_edit_event=NULL; // this might have been deleted; etc., c.f. fixup_events
    mt->init_event=NULL;
    mt->selected_init_event=NULL;
    mt->specific_event=NULL;
    mt->avol_init_event=NULL; // we will try to relocate this in mt_init_tracks()

    mt_init_tracks(mt,FALSE);

    if (mt->avol_fx==avol_fx) {
      mt->aparam_view_list=g_list_copy(aparam_view_list);
    }
    if (aparam_view_list!=NULL) g_list_free(aparam_view_list);

    add_aparam_menuitems(mt);

    unselect_all(mt);
    for (i=mt->num_video_tracks;i<num_tracks;i++) {
      add_video_track_behind (NULL,mt);
    }

    mt->clip_selected=clip_sel;
    mt_clip_select(mt,FALSE);

    vlist=mt->video_draws;
    llist=label_list;
    while (vlist!=NULL) {
      eventbox=GTK_WIDGET(vlist->data);
      label=GTK_WIDGET(g_object_get_data(G_OBJECT(eventbox),"label"));
      g_free(GTK_LABEL(label)->text);
      GTK_LABEL(label)->text=(gchar *)llist->data;
      vlist=vlist->next;
      llist=llist->next;
    }
    g_list_free(label_list);

    if (mt->event_list!=NULL) remove_markers(mt->event_list);

    mt->selected_tracks=g_list_copy(seltracks);
    slist=mt->selected_tracks;
    while (slist!=NULL) {
      eventbox=(GtkWidget *)g_list_nth_data(mt->video_draws,GPOINTER_TO_INT(slist->data));
      checkbutton=(GtkWidget *)g_object_get_data(G_OBJECT(eventbox),"checkbutton");
#ifdef ENABLE_GIW
      if (!prefs->lamp_buttons) {
#endif
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbutton),TRUE);
#ifdef ENABLE_GIW
      }
      else {
	giw_led_set_mode(GIW_LED(checkbutton),TRUE);
      }
#endif
      slist=slist->next;
    }
    if (seltracks!=NULL) g_list_free(seltracks);

    mt->current_track=current_track;
    track_select(mt);
    if (mt->end_secs!=end_secs&&event_list_get_end_secs(mt->event_list)<=end_secs) set_timeline_end_secs (mt, end_secs);
  }

  mt->undo_offset--;

  if (mt->undo_offset<=1) mt_set_redoable(mt,MT_UNDO_NONE,NULL,FALSE);
  else {
    mt_undo *redo=(mt_undo *)(g_list_nth_data(mt->undos,g_list_length(mt->undos)-mt->undo_offset));
    mt_set_redoable(mt,redo->action,redo->extra,TRUE);
  }
  last_redo=(mt_undo *)g_list_nth_data(mt->undos,g_list_length(mt->undos)-1-mt->undo_offset);
  mt_set_undoable(mt,last_redo->action,last_redo->extra,TRUE);

  GTK_RULER(mt->timeline)->position=mt->ptr_time;
  gtk_widget_queue_draw(mt->timeline);


  // TODO *****
  //if (last_redo->action<1024&&block_is_selected) mt_selblock(NULL, NULL, 0, 0, (gpointer)mt);

  if (last_redo->action==MT_UNDO_FILTER_MAP_CHANGE||mt->poly_state==POLY_FX_STACK) {
    if (last_redo->action==MT_UNDO_FILTER_MAP_CHANGE) mt_tl_move(mt,last_redo->tc-mt->ptr_time);
    polymorph(mt,POLY_FX_STACK);
  }
  if (mt->poly_state!=POLY_PARAMS) mt_show_current_frame(mt, FALSE);

  utxt=g_utf8_strdown((tmp=get_undo_text(last_redo->action,last_redo->extra)),-1);
  g_free(tmp);

  msg=g_strdup_printf(_("Redid %s\n"),utxt);
  d_print(msg);
  g_free(utxt);
  g_free(msg);

  mt_desensitise(mt);
  mt_sensitise(mt);

  mt->idlefunc=mt_idle_add(mt);
}



void
multitrack_view_details            (GtkMenuItem     *menuitem,
				    gpointer         user_data) {
  char buff[512];
  fileinfo *filew;
  lives_mt *mt=(lives_mt *)user_data;
  file *rfile=mainw->files[mt->render_file];
  guint bsize=0;
  gdouble time=0.;
  gint num_events=0;

  filew = create_info_window (cfile->achans,TRUE);

  g_snprintf(buff,512,"LiVES - %s",_("Multitrack details"));
  gtk_window_set_title (GTK_WINDOW (filew->info_window), buff);
  
  // type
  g_snprintf(buff,512,"\n  Event List");
  gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (filew->textview24)),buff, -1);

  // fps
  if (mt->fps>0) {
      g_snprintf(buff,512,"\n  %.3f%s",mt->fps,rfile->ratio_fps?"...":"");
  }
  else {
    g_snprintf(buff,512,"%s",_ ("\n (variable)"));
  }

  gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (filew->textview25)),buff, -1);

  // image size
  g_snprintf(buff,512,"\n  %dx%d",rfile->hsize,rfile->vsize);
  gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (filew->textview26)),buff, -1);

  // elist time
  if (mt->event_list!=NULL) {
    bsize=event_list_get_byte_size(mt,mt->event_list,&num_events);
    time=event_list_get_end_secs(mt->event_list);
  }

  // events
  g_snprintf(buff,512,"\n  %d",num_events);
  gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (filew->textview27)),buff, -1);

  g_snprintf(buff,512,"\n  %.3f sec",time);
  gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (filew->textview28)),buff, -1);

  // byte size
  g_snprintf(buff,512,"\n  %d bytes",bsize);
  gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (filew->textview29)),buff, -1);

  if (cfile->achans>0) {
    g_snprintf(buff,512,"\n  %d Hz %d bit",cfile->arate,cfile->asampsize);
    gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (filew->textview_lrate)),buff, -1);
  }
  
  if (cfile->achans>1) {
    g_snprintf(buff,512,"\n  %d Hz %d bit",cfile->arate,cfile->asampsize);
    gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (filew->textview_rrate)),buff, -1);
  }
  gtk_widget_show (filew->info_window);
}



static void add_effect_inner(lives_mt *mt, int num_in_tracks, int *in_tracks, int num_out_tracks, int *out_tracks, weed_plant_t *start_event, weed_plant_t *end_event) {
  int i;
  weed_plant_t *event;
  void **init_events;
  weed_plant_t *filter=get_weed_filter(mt->current_fx);
  weed_timecode_t start_tc=get_event_timecode(start_event);
  weed_timecode_t end_tc=get_event_timecode(end_event);
  gboolean did_backup=mt->did_backup;
  gboolean has_params;
  gdouble timesecs=GTK_RULER (mt->timeline)->position;
  weed_timecode_t tc=q_gint64(timesecs*U_SEC,mt->fps);
  lives_rfx_t *rfx;


  if (!did_backup&&mt->idlefunc>0) {
    g_source_remove(mt->idlefunc);
    mt->idlefunc=0;
  }

  if (!did_backup&&mt->current_fx!=mt->avol_fx) mt_backup(mt,MT_UNDO_APPLY_FILTER,0);

  // set track_index (for special widgets)
  mt->track_index=-1;
  for (i=0;i<num_in_tracks;i++) {
    if (mt->current_track==in_tracks[i]) mt->track_index=i;
  }

  // add effect_init event
  mt->event_list=append_filter_init_event(mt->event_list,start_tc,mt->current_fx,num_in_tracks);
  mt->init_event=get_last_event(mt->event_list);
  unlink_event(mt->event_list,mt->init_event);
  weed_set_int_array(mt->init_event,"in_tracks",num_in_tracks,in_tracks);
  weed_set_int_array(mt->init_event,"out_tracks",num_out_tracks,out_tracks);
  insert_filter_init_event_at(mt->event_list,start_event,mt->init_event);

  if (pchain!=NULL) {
    g_free(pchain);
    pchain=NULL;
  }

  if (weed_plant_has_leaf(filter,"in_parameter_templates")) pchain=filter_init_add_pchanges(mt->event_list,filter,mt->init_event,num_in_tracks);

  // add effect map event
  init_events=get_init_events_before(start_event,mt->init_event,TRUE);
  mt->event_list=append_filter_map_event(mt->event_list,start_tc,init_events);
  g_free(init_events);
  event=get_last_event(mt->event_list);
  unlink_event(mt->event_list,event);
  insert_filter_map_event_at(mt->event_list,start_event,event,TRUE);

  // update all effect maps in block, appending init_event
  update_filter_maps(start_event,end_event,mt->init_event);

  // add effect deinit event
  mt->event_list=append_filter_deinit_event(mt->event_list,end_tc,(void *)mt->init_event,pchain);
  event=get_last_event(mt->event_list);
  unlink_event(mt->event_list,event);
  insert_filter_deinit_event_at(mt->event_list,end_event,event);

  // zip forward a bit, in case there is a FILTER_MAP at end_tc after our FILTER_DEINIT (e.g. if adding multiple filters)
  while (event!=NULL&&get_event_timecode(event)==end_tc) event=get_next_event(event);
  if (event==NULL) event=get_last_event(mt->event_list);
  else event=get_prev_event(event);

  // add effect map event 
  init_events=get_init_events_before(event,mt->init_event,FALSE); // also deletes the effect
  mt->event_list=append_filter_map_event(mt->event_list,end_tc,init_events);
  g_free(init_events);

  event=get_last_event(mt->event_list);
  unlink_event(mt->event_list,event);
  insert_filter_map_event_at(mt->event_list,end_event,event,FALSE);

  mt->did_backup=did_backup;
  if (mt->event_list!=NULL) gtk_widget_set_sensitive (mt->clear_event_list, TRUE);

  if (mt->current_fx==mt->avol_fx) return;

  if (mt->avol_fx!=-1) {
    apply_avol_filter(mt);
  }

  if (mt->is_atrans) return;

  rfx=weed_to_rfx(filter,FALSE);
  get_track_index(mt,tc);

  // here we just check if we have any params to display
  has_params=make_param_box(NULL,rfx);
  rfx_free(rfx);
  g_free(rfx);

  mt_tl_move(mt,start_tc/U_SEC-GTK_RULER (mt->timeline)->position);

  if (has_params) {
    polymorph(mt,POLY_PARAMS);
    gtk_widget_set_sensitive(mt->apply_fx_button,FALSE);
  }
  else polymorph(mt,POLY_FX_STACK);

  if (!did_backup) mt->idlefunc=mt_idle_add(mt);

  mt_show_current_frame(mt, FALSE);
}




weed_plant_t *add_blank_frames_up_to (weed_plant_t *event_list, weed_plant_t *start_event, weed_timecode_t end_tc, gdouble fps) {
  // add blank frames from FRAME event (or NULL) start_event up to and including (quantised) end_tc
  // returns updated event_list
  weed_timecode_t tc;
  weed_plant_t *shortcut=NULL;
  weed_timecode_t tl=q_dbl(1./fps,fps);
  int blank_clip=-1,blank_frame=0;

  if (start_event!=NULL) tc=get_event_timecode(start_event)+tl;
  else tc=0;

  for (;tc<=end_tc;tc=q_gint64(tc+tl,fps)) {
    event_list=insert_frame_event_at (event_list,tc,1,&blank_clip,&blank_frame,&shortcut);
  }
  weed_set_double_value(event_list,"fps",fps);
  return event_list;
}


void mt_add_region_effect (GtkMenuItem *menuitem, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  weed_plant_t *start_event;
  weed_plant_t *end_event;

  weed_timecode_t start_tc=q_gint64(mt->region_start*U_SEC,mt->fps);
  weed_timecode_t end_tc=q_gint64(mt->region_end*U_SEC-U_SEC/mt->fps,mt->fps);

  gchar *filter_name;
  gchar *text,*tname,*track_desc;
  gint numtracks=g_list_length(mt->selected_tracks);
  int *tracks=(int *)g_malloc(numtracks*sizint);
  gint tcount=0,tlast=-1000000,tsmall=-1,ctrack;
  GList *llist;

  weed_plant_t *last_frame_event=NULL;
  weed_timecode_t last_frame_tc=0;

  gchar *tmp,*tmp1;

  // sort selected tracks into ascending order
  while (tcount<numtracks) {
    tsmall=-1000000;
    llist=mt->selected_tracks;
    while (llist!=NULL) {
      ctrack=GPOINTER_TO_INT(llist->data);
      if ((tsmall==-1000000||ctrack<tsmall)&&ctrack>tlast) tsmall=ctrack;
      llist=llist->next;
    }
    tracks[tcount++]=tlast=tsmall;
  }

  // add blank frames up to region end (if necessary)
  if (mt->event_list!=NULL&&((last_frame_event=get_last_frame_event(mt->event_list))!=NULL)) last_frame_tc=get_event_timecode(last_frame_event);
  if (end_tc>last_frame_tc) mt->event_list=add_blank_frames_up_to(mt->event_list,last_frame_event,end_tc-(gdouble)(tracks[0]<0)*U_SEC/mt->fps,mt->fps);

  if (menuitem!=NULL) mt->current_fx=GPOINTER_TO_INT(g_object_get_data(G_OBJECT(menuitem),"idx"));

  start_event=get_frame_event_at(mt->event_list,start_tc,NULL,TRUE);
  end_event=get_frame_event_at(mt->event_list,end_tc,start_event,TRUE);

  add_effect_inner(mt,numtracks,tracks,1,&tracks[0],start_event,end_event);
  if (menuitem==NULL&&!mt->is_atrans) {
    g_free(tracks);
    return;
  }

  mt->last_fx_type=MT_LAST_FX_REGION;

  // create user message
  filter_name=weed_filter_get_name(mt->current_fx);
  numtracks=enabled_in_channels(get_weed_filter(mt->current_fx),TRUE);  // count repeated channels
  switch (numtracks) {
  case 1:
    tname=lives_fx_cat_to_text(LIVES_FX_CAT_EFFECT,FALSE); // effect
    track_desc=g_strdup_printf(_("track %s"),(tmp=get_track_name(mt,tracks[0],FALSE)));
    g_free(tmp);
    break;
  case 2:
    tname=lives_fx_cat_to_text(LIVES_FX_CAT_TRANSITION,FALSE); // transition
    track_desc=g_strdup_printf(_("tracks %s and %s"),(tmp1=get_track_name(mt,tracks[0],FALSE)),(tmp=get_track_name(mt,tracks[1],FALSE)));
    g_free(tmp);
    g_free(tmp1);
    break;
  default:
    tname=lives_fx_cat_to_text(LIVES_FX_CAT_COMPOSITOR,FALSE); // compositor
    track_desc=g_strdup(_("selected tracks"));
    break;
  }
  g_free(tracks);
  text=g_strdup_printf(_("Added %s %s to %s from %.4f to %.4f\n"),tname,filter_name,track_desc,start_tc/U_SEC,q_gint64(end_tc+U_SEC/mt->fps,mt->fps)/U_SEC);
  d_print(text);
  g_free(text);
  g_free(filter_name);
  g_free(tname);
  g_free(track_desc);
}



void mt_add_block_effect (GtkMenuItem *menuitem, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  weed_plant_t *start_event=mt->block_selected->start_event;
  weed_plant_t *end_event=mt->block_selected->end_event;
  weed_timecode_t start_tc=get_event_timecode(start_event);
  weed_timecode_t end_tc=get_event_timecode(end_event);
  gchar *filter_name;
  gchar *text;
  int selected_track;
  gchar *tmp;

  selected_track=GPOINTER_TO_INT(g_object_get_data(G_OBJECT(mt->block_selected->eventbox),"layer_number"));

  if (menuitem!=NULL) mt->current_fx=GPOINTER_TO_INT(g_object_get_data(G_OBJECT(menuitem),"idx"));

  mt->last_fx_type=MT_LAST_FX_BLOCK;
  add_effect_inner(mt,1,&selected_track,1,&selected_track,start_event,end_event);

  filter_name=weed_filter_get_name(mt->current_fx);
  text=g_strdup_printf(_("Added effect %s to track %s from %.4f to %.4f\n"),filter_name,(tmp=get_track_name(mt,selected_track,mt->aud_track_selected)),start_tc/U_SEC,q_gint64(end_tc+U_SEC/mt->fps,mt->fps)/U_SEC);
  g_free(tmp);
  d_print(text);
  g_free(text);
  g_free(filter_name);
}
  


void on_mt_list_fx_activate (GtkMenuItem *menuitem, gpointer user_data) {
  // list effects at current frame/track
  lives_mt *mt=(lives_mt *)user_data;
  polymorph(mt,POLY_FX_STACK);
}

void on_mt_delfx_activate (GtkMenuItem *menuitem, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  gboolean did_backup=mt->did_backup;
  int error;

  gchar *fhash;

  if (mt->selected_init_event==NULL) return;

  if (mt->is_rendering) return;

  if (!did_backup&&mt->idlefunc>0) {
    g_source_remove(mt->idlefunc);
    mt->idlefunc=0;
  }

  fhash=weed_get_string_value(mt->selected_init_event,"filter",&error);
  mt->current_fx=weed_get_idx_for_hashname(fhash,TRUE);
  weed_free(fhash);

  if (!did_backup) mt_backup(mt,MT_UNDO_DELETE_FILTER,0);

  remove_filter_from_event_list(mt->event_list,mt->selected_init_event);
  remove_end_blank_frames(mt->event_list);
  
  mt->selected_init_event=NULL;
  mt->current_fx=-1;
  if (mt->poly_state==POLY_PARAMS) polymorph(mt,POLY_CLIPS);
  else if (mt->poly_state==POLY_FX_STACK) polymorph(mt,POLY_FX_STACK);
  mt_show_current_frame(mt, FALSE);
  mt->did_backup=did_backup;

  if (!did_backup) mt->idlefunc=mt_idle_add(mt);

}


static void mt_jumpto (lives_mt *mt, lives_direction_t dir) {
  GtkWidget *eventbox;
  weed_timecode_t tc=q_gint64(GTK_RULER (mt->timeline)->position*U_SEC,mt->fps);
  gdouble secs=tc/U_SEC;
  track_rect *block;
  weed_timecode_t start_tc,end_tc;
  gdouble offs=1.;

  if (mt->current_track>-1&&!mt->aud_track_selected) eventbox=(GtkWidget *)g_list_nth_data(mt->video_draws,mt->current_track);
  else {
    eventbox=(GtkWidget *)g_list_nth_data(mt->audio_draws,mt->current_track+mt->opts.back_audio_tracks);
    offs=0.;
  }
  block=get_block_from_time(eventbox,secs,mt);

  if (block!=NULL) {
    if (dir==DIRECTION_NEGATIVE) {
      if (tc==(start_tc=get_event_timecode(block->start_event))) {
	secs-=1./mt->fps;
	block=NULL;
      }
      else secs=start_tc/U_SEC;
    }
    else {
      if (tc==q_gint64((end_tc=get_event_timecode(block->end_event))+(offs*U_SEC)/mt->fps,mt->fps)) {
	secs+=1./mt->fps;
	block=NULL;
      }
      else secs=end_tc/U_SEC+offs/mt->fps;
    }
  }
  if (block==NULL) {
    if (dir==DIRECTION_NEGATIVE) {
      block=get_block_before(eventbox,secs,TRUE);
      if (block==NULL) secs=0.;
      else {
	if (tc==q_gint64((end_tc=get_event_timecode(block->end_event))+(offs*U_SEC)/mt->fps,mt->fps)) {
	  secs=get_event_timecode(block->start_event)/U_SEC;
	}
	else secs=end_tc/U_SEC+offs/mt->fps;
      }
    }
    else {
      block=get_block_after(eventbox,secs,FALSE);
      if (block==NULL) return;
      secs=get_event_timecode(block->start_event)/U_SEC;
    }
  }

  if (secs<0.) secs=0.;
  if (secs>mt->end_secs) set_timeline_end_secs (mt, secs);
  mt->fm_edit_event=NULL;
  mt_tl_move(mt,secs-GTK_RULER(mt->timeline)->position);
}


void on_jumpback_activate (GtkMenuItem *menuitem, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  mt_jumpto(mt,DIRECTION_NEGATIVE);
}

void on_jumpnext_activate (GtkMenuItem *menuitem, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  mt_jumpto(mt,DIRECTION_POSITIVE);
}


void on_cback_audio_activate (GtkMenuItem *menuitem, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  mt->current_track=-1;
  track_select(mt);
}  



void on_render_activate (GtkMenuItem *menuitem, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;

  // save these values, because reget_afilesize() can reset them
  gint arate=cfile->arate;
  gint arps=cfile->arps;
  gint asampsize=cfile->asampsize;
  gint achans=cfile->achans;
  gint signed_endian=cfile->signed_endian;
  gint orig_file;
  int i;
  gchar *com,*tmp;
  gboolean had_audio=FALSE;
  gboolean post_reset_ba=FALSE;
  gboolean post_reset_ca=FALSE;

  if (mt->idlefunc>0) {
    g_source_remove(mt->idlefunc);
    mt->idlefunc=0;
  }

  if (menuitem==NULL) {
    mt->pr_audio=TRUE;
    had_audio=mt->has_audio_file;
    if (had_audio) {
      unlink(cfile->info_file);
      mainw->error=FALSE;
      mainw->cancelled=CANCEL_NONE;
      com=g_strdup_printf("\"%s\" backup_audio \"%s\"",prefs->backend,cfile->handle);
      lives_system(com,FALSE);
      g_free(com);
      check_backend_return(cfile);
      if (mainw->error) return;
    }
    mt->has_audio_file=TRUE;
  }
  else {
    mt->pr_audio=FALSE;
    prefs->render_audio=!mt->render_audp; // mt->render_audp sense is reversed
    prefs->normalise_audio=mt->normalise_audp; // mt->normalised_audp sense is reversed
  }

  mt_desensitise(mt);

  mainw->event_list=mt->event_list;

  mt->is_rendering=TRUE; // use this to test for rendering from mt (not mainw->is_rendering)
  gtk_widget_set_sensitive(mt->render_vid,FALSE);
  gtk_widget_set_sensitive(mt->render_aud,FALSE);
  gtk_widget_set_sensitive(mt->normalise_aud,FALSE);

  if (mt->poly_state==POLY_PARAMS) polymorph(mt,POLY_FX_STACK);

  mt->pb_start_event=get_first_event(mainw->event_list);

  if (prefs->normalise_audio) {
    // Normalise audio (preference)

    // TODO - in future we could also check the pb volume levels and adjust to prevent clipping
    // - although this would be time consuming when clicking or activating "render" in mt

    // auto-adjust mixer levels:
    gboolean has_backing_audio=FALSE;
    gboolean has_channel_audio=FALSE;

    // -> if we have either but not both: backing audio or channel audio

    if (mt->opts.back_audio_tracks>=1) {
      // check backing track(s) for audio blocks
      for (i=0;i<mt->opts.back_audio_tracks;i++) {
	if (!is_empty_track(G_OBJECT(g_list_nth_data(mt->audio_draws,i)))) {
	  if (get_mixer_track_vol(mt,i)==0.5) {
	    has_backing_audio=TRUE;
	  }
	}
      }
    }

    for (i=mt->opts.back_audio_tracks;i<g_list_length(mt->audio_draws);i++) {
      // check channel track(s) for audio blocks
      if (!is_empty_track(G_OBJECT(g_list_nth_data(mt->audio_draws,i)))) {
	if (get_mixer_track_vol(mt,i)==0.5) {
	  has_channel_audio=TRUE;
	}
      }
    }


    // first checks done ^

    if (has_backing_audio&&!has_channel_audio) {
      // backing but no channel audio

      // ->
      // if ALL backing levels are at 0.5, set them to 1.0

      for (i=0;i<mt->opts.back_audio_tracks;i++) {
	if (!is_empty_track(G_OBJECT(g_list_nth_data(mt->audio_draws,i)))) {
	  if (get_mixer_track_vol(mt,i)!=0.5) {
	    has_backing_audio=FALSE;
	    break;
	  }
	}
      }

      if (has_backing_audio) {
	post_reset_ba=TRUE; // reset levels after rendering
	for (i=0;i<mt->opts.back_audio_tracks;i++) {
	  if (!is_empty_track(G_OBJECT(g_list_nth_data(mt->audio_draws,i)))) {
	    set_mixer_track_vol(mt,i,1.0);
	  }
	}
      }
    }


    if (!has_backing_audio&&has_channel_audio) {
      // channel but no backing audio

      // ->
      // if ALL channel levels are at 0.5, set them all to 1.0
      
      for (i=mt->opts.back_audio_tracks;i<g_list_length(mt->audio_draws);i++) {
	// check channel track(s) for audio blocks
	if (!is_empty_track(G_OBJECT(g_list_nth_data(mt->audio_draws,i)))) {
	  if (get_mixer_track_vol(mt,i)!=0.5) {
	    has_channel_audio=FALSE;
	  }
	}
      }
      
      if (has_channel_audio) {
	post_reset_ca=TRUE;  // reset levels after rendering
	for (i=mt->opts.back_audio_tracks;i<g_list_length(mt->audio_draws);i++) {
	  if (!is_empty_track(G_OBJECT(g_list_nth_data(mt->audio_draws,i)))) {
	    // set to 1.0
	    set_mixer_track_vol(mt,i,1.0);
	  }
	}
      }
    }
  }


  if (render_to_clip(FALSE)) {
    // rendering was successful

#if 0
    if (mt->pr_audio) {
      mt->pr_audio=FALSE;
      d_print_done();
      gtk_widget_set_sensitive(mt->render_vid,TRUE);
      gtk_widget_set_sensitive(mt->render_aud,TRUE);
      gtk_widget_set_sensitive(mt->normalise_aud,TRUE);
      mt->idlefunc=mt_idle_add(mt);
      return;
    }
#endif

    cfile->start=cfile->frames>0?1:0;
    cfile->end=cfile->frames;
    if (cfile->frames==0) {
      cfile->hsize=cfile->vsize=0;
    }
    set_undoable (NULL,FALSE);
    cfile->changed=TRUE;
    add_to_winmenu();
    mt->file_selected=orig_file=mainw->current_file;
    d_print ((tmp=g_strdup_printf (_ ("rendered %d frames to new clip.\n"),cfile->frames)));
    g_free(tmp);
    if (mainw->scrap_file!=-1||mainw->ascrap_file!=-1) mt->changed=FALSE;
    mt->is_rendering=FALSE;
    prefs->render_audio=TRUE;
    prefs->normalise_audio=TRUE;
    save_clip_values(orig_file);

    if (prefs->crash_recovery) add_to_recovery_file(cfile->handle);
    reset_clip_menu();

    if (post_reset_ba) {
      // reset after normalising backing audio
      for (i=0;i<mt->opts.back_audio_tracks;i++) {
	if (!is_empty_track(G_OBJECT(g_list_nth_data(mt->audio_draws,i)))) {
	  if (get_mixer_track_vol(mt,i)==1.0) {
	    set_mixer_track_vol(mt,i,0.5);
	  }
	}
      }
    }

    if (post_reset_ca) {
      // reset after normalising channel audio
      for (i=mt->opts.back_audio_tracks;i<g_list_length(mt->audio_draws);i++) {
	if (!is_empty_track(G_OBJECT(g_list_nth_data(mt->audio_draws,i)))) {
	  if (get_mixer_track_vol(mt,i)==1.0) {
	    set_mixer_track_vol(mt,i,0.5);
	  }
	}
      }
    }

    mainw->current_file=mainw->first_free_file;
    
    if (!get_new_handle(mainw->current_file,NULL)) {
      mainw->current_file=orig_file;
      if (!multitrack_end(NULL,user_data)) switch_to_file ((mainw->current_file=0),orig_file);
      mt->idlefunc=mt_idle_add(mt);
      return;
    }

    cfile->hsize=mainw->files[orig_file]->hsize;
    cfile->vsize=mainw->files[orig_file]->vsize;
    
    cfile->pb_fps=cfile->fps=mainw->files[orig_file]->fps;
    cfile->ratio_fps=mainw->files[orig_file]->ratio_fps;
    
    cfile->arate=arate;
    cfile->arps=arps;
    cfile->asampsize=asampsize;

    cfile->achans=achans;
    cfile->signed_endian=signed_endian;
    
    cfile->bpp=cfile->img_type==IMG_TYPE_JPEG?24:32;
    cfile->changed=TRUE;
    cfile->is_loaded=TRUE;
    
    cfile->old_frames=cfile->frames;
    
    mt->render_file=mainw->current_file;

    if (prefs->mt_exit_render) {
      if (multitrack_end(menuitem,user_data)) return;
    }

    mt_init_clips(mt,orig_file,TRUE);
    if (mt->idlefunc>0) g_source_remove(mt->idlefunc);
    mt->idlefunc=0;
    while (g_main_context_iteration(NULL,FALSE));
    mt_clip_select(mt,TRUE);
  }
  else {
    gboolean render_audio=prefs->render_audio;
    gchar *curtmpdir;
    // rendering failed - clean up

    cfile->frames=cfile->start=cfile->end=0;
    mt->is_rendering=FALSE;
    prefs->render_audio=TRUE;
    mainw->event_list=NULL;
    if (mt->pr_audio) {
      com=g_strdup_printf("\"%s\" undo_audio \"%s\"",prefs->backend,cfile->handle);
      lives_system(com,FALSE);
      g_free(com);
      mt->has_audio_file=had_audio;
    }
    else {
      // remove subdir
      do_threaded_dialog(_("Cleaning up..."),FALSE);
      curtmpdir=g_build_filename(prefs->tmpdir,cfile->handle,NULL);
      com=g_strdup_printf("/bin/rm -rf \"%s/\"*",curtmpdir);
      lives_system(com,TRUE);
      g_free(com);
      end_threaded_dialog();
    }

    prefs->render_audio=render_audio;
  }

  // enable GUI for next rendering
  gtk_widget_set_sensitive(mt->render_vid,TRUE);
  gtk_widget_set_sensitive(mt->render_aud,TRUE);
  gtk_widget_set_sensitive(mt->normalise_aud,TRUE);
  mt_sensitise(mt);

  mt->idlefunc=mt_idle_add(mt);

}


void on_prerender_aud_activate (GtkMenuItem *menuitem, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  on_render_activate(menuitem,user_data);
  mainw->is_rendering=mainw->internal_messaging=mt->is_rendering=FALSE;
  mt_sensitise(mt);
  gtk_widget_set_sensitive (mt->prerender_aud, FALSE);
}


void update_filter_events(lives_mt *mt, weed_plant_t *first_event, weed_timecode_t start_tc, weed_timecode_t end_tc, 
			  int track, weed_timecode_t new_start_tc, int new_track) {
  // move/remove filter_inits param_change and filter_deinits after deleting/moving a block
  weed_plant_t *event,*event_next;
  gboolean was_moved;
  int error;
  weed_plant_t *init_event,*deinit_event;
  GList *moved_events=NULL;
  gboolean leave_event;
  int i;

  if (first_event==NULL) event=get_first_event(mt->event_list);
  else event=get_next_event(first_event);
  while (event!=NULL&&get_event_timecode(event)<start_tc) event=get_next_event(event);

  while (event!=NULL&&get_event_timecode(event)<=end_tc) {
    event_next=get_next_event(event);
    was_moved=FALSE;
    if (WEED_EVENT_IS_FILTER_INIT(event)) {
      if (event==mt->avol_init_event) {
	event=event_next;
	continue;  // we move our audio volume effect using a separate mechanism
      }
      if (mt->opts.move_effects&&mt->moving_block) {
	if (weed_plant_has_leaf(event,"deinit_event")&&weed_plant_has_leaf(event,"in_tracks")&&
	    weed_leaf_num_elements(event,"in_tracks")==1&&weed_get_int_value(event,"in_tracks",&error)==track) {
	  deinit_event=(weed_plant_t *)weed_get_voidptr_value(event,"deinit_event",&error);
	  if (get_event_timecode(deinit_event)<=end_tc) {
	    if (g_list_index(moved_events,event)==-1) {
	      // update owners,in_tracks and out_tracks
	      weed_set_int_value(event,"in_tracks",new_track);

	      if (weed_plant_has_leaf(event,"out_tracks")) {
		int *out_tracks=weed_get_int_array(event,"out_tracks",&error);
		int num_tracks=weed_leaf_num_elements(event,"out_tracks");
		for (i=0;i<num_tracks;i++) {
		  if (out_tracks[i]==track) out_tracks[i]=new_track;
		}
		weed_set_int_array(event,"out_tracks",num_tracks,out_tracks);
		weed_free(out_tracks);
	      }

	      // move to new position
	      if (new_start_tc<start_tc) {
		move_filter_init_event(mt->event_list,get_event_timecode(event)+new_start_tc-start_tc,event,mt->fps);
		move_filter_deinit_event(mt->event_list,get_event_timecode(deinit_event)+new_start_tc-start_tc,
					 deinit_event,mt->fps,TRUE);
		if (event==first_event) first_event=NULL;
		was_moved=TRUE;
	      }
	      else if (new_start_tc>start_tc) {
		move_filter_deinit_event(mt->event_list,get_event_timecode(deinit_event)+new_start_tc-start_tc,
					 deinit_event,mt->fps,TRUE);
		move_filter_init_event(mt->event_list,get_event_timecode(event)+new_start_tc-start_tc,event,mt->fps);
		if (event==first_event) first_event=NULL;
		was_moved=TRUE;
	      }
	      moved_events=g_list_prepend(moved_events,event);
	    }
	  }
	}
      }
      if (g_list_index(moved_events,event)==-1&&event!=mt->avol_init_event&&!was_moved&&
	  !move_event_right(mt->event_list,event,TRUE,mt->fps)) {
	was_moved=TRUE;
	if (event==first_event) first_event=NULL;
      }
    }
    else {
      leave_event=TRUE;
      if (WEED_EVENT_IS_FILTER_DEINIT(event)) {
	if (mt->opts.move_effects&&mt->moving_block) {
	  if (weed_plant_has_leaf(event,"init_event")) {
	    init_event=(weed_plant_t *)weed_get_voidptr_value(event,"init_event",&error);
	    if (g_list_index(moved_events,init_event)==-1&&!(weed_plant_has_leaf(event,"in_tracks")&&
							     weed_leaf_num_elements(event,"in_tracks")==1&&
							     weed_get_int_value(event,"in_tracks",&error)==track)&&
		init_event!=mt->avol_init_event) {
	      leave_event=FALSE;
	    }
	  }
	}
	if (!leave_event&&!move_event_left(mt->event_list,event,TRUE,mt->fps)) {
	  was_moved=TRUE;
	}
      }
    }
    if (was_moved) {
      if (first_event==NULL) event=get_first_event(mt->event_list);
      else event=get_next_event(first_event);
      while (event!=NULL&&get_event_timecode(event)<start_tc) event=get_next_event(event);
    }
    else {
      event=event_next;
      if (WEED_EVENT_IS_FRAME(event)) first_event=event;
    }
  }
  if (moved_events!=NULL) g_list_free(moved_events);
}



void on_split_activate (GtkMenuItem *menuitem, gpointer user_data) {
  // split current block at current time
  lives_mt *mt=(lives_mt *)user_data;
  gdouble timesecs=GTK_RULER (mt->timeline)->position;
  gboolean did_backup=mt->did_backup;
  weed_timecode_t tc;

  if (mt->putative_block==NULL) return;

  if (!did_backup&&mt->idlefunc>0) {
    g_source_remove(mt->idlefunc);
    mt->idlefunc=0;
  }

  if (mt->context_time!=-1.&&mt->use_context) {
    timesecs=mt->context_time;
    mt->context_time=-1.;
    mt->use_context=FALSE;
  }

  if (!did_backup) mt_backup(mt,MT_UNDO_SPLIT,0);

  tc=q_gint64(timesecs*U_SEC,mt->fps);

  split_block(mt,mt->putative_block,tc,mt->current_track,FALSE);
  mt->did_backup=did_backup;

  if (!did_backup) mt->idlefunc=mt_idle_add(mt);

}





void on_split_curr_activate (GtkMenuItem *menuitem, gpointer user_data) {
  // split current track at current time
  lives_mt *mt=(lives_mt *)user_data;
  gdouble timesecs=GTK_RULER (mt->timeline)->position;
  gboolean did_backup=mt->did_backup;
  weed_timecode_t tc;
  GtkWidget *eventbox;
  track_rect *block;

  if (mt->idlefunc>0) {
    g_source_remove(mt->idlefunc);
    mt->idlefunc=0;
  }

  if (!did_backup) mt_backup(mt,MT_UNDO_SPLIT,0);

  tc=q_gint64(timesecs*U_SEC,mt->fps);


  if (mt->current_track==-1) eventbox=(GtkWidget *)mt->audio_draws->data;
  else eventbox=(GtkWidget *)g_list_nth_data(mt->video_draws,mt->current_track);

  block=get_block_from_time(eventbox,timesecs,mt);

  if (block==NULL) return;

  split_block(mt,block,tc,mt->current_track,FALSE);
  mt->did_backup=did_backup;

  if (!did_backup) mt->idlefunc=mt_idle_add(mt);

}



void on_split_sel_activate (GtkMenuItem *menuitem, gpointer user_data) {
  // split selected tracks at current time
  lives_mt *mt=(lives_mt *)user_data;
  GList *selt=mt->selected_tracks;
  GtkWidget *eventbox;
  gint track;
  track_rect *block;
  gdouble timesecs=GTK_RULER (mt->timeline)->position;
  gboolean did_backup=mt->did_backup;

  if (mt->selected_tracks==NULL) return;

  if (!did_backup&&mt->idlefunc>0) {
    g_source_remove(mt->idlefunc);
    mt->idlefunc=0;
  }

  if (!did_backup) mt_backup(mt,MT_UNDO_SPLIT_MULTI,0);

  while (selt!=NULL) {
    track=GPOINTER_TO_INT(selt->data);
    eventbox=(GtkWidget *)g_list_nth_data(mt->video_draws,track);
    block=get_block_from_time(eventbox,timesecs,mt);
    if (block!=NULL) split_block(mt,block,timesecs*U_SEC,track,FALSE);
    selt=selt->next;
  }
  mt->did_backup=did_backup;

  if (!did_backup) mt->idlefunc=mt_idle_add(mt);

}



void on_delblock_activate (GtkMenuItem *menuitem, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  track_rect *block,*blockprev,*blocknext;
  weed_plant_t *event,*prevevent;
  GtkWidget *eventbox,*aeventbox;
  gint track;
  gboolean done=FALSE;
  weed_timecode_t start_tc,end_tc;
  weed_plant_t *first_event;
  gboolean did_backup=mt->did_backup;

  if (mt->is_rendering) return;

  if (!did_backup&&mt->idlefunc>0) {
    g_source_remove(mt->idlefunc);
    mt->idlefunc=0;
  }

  mt->context_time=-1.;

  if (!did_backup) {
    if (mt->current_track!=-1) mt_backup(mt,MT_UNDO_DELETE_BLOCK,0);
    else mt_backup(mt,MT_UNDO_DELETE_AUDIO_BLOCK,0);
  }

  if (mt->block_selected==NULL) mt->block_selected=mt->putative_block;
  block=mt->block_selected;
  eventbox=block->eventbox;

  if (mt->current_track!=-1) track=GPOINTER_TO_INT(g_object_get_data(G_OBJECT(eventbox),"layer_number"));
  else track=-1;

  if ((aeventbox=GTK_WIDGET(g_object_get_data(G_OBJECT(eventbox),"atrack")))!=NULL) {
    gint current_track=mt->current_track;
    mt->current_track=track;
    mt->block_selected=get_block_from_time(aeventbox,get_event_timecode(block->start_event)/U_SEC,mt);
    if (mt->block_selected!=NULL) on_delblock_activate(NULL,user_data);
    mt->block_selected=block;
    mt->current_track=current_track;
  }

  mt_desensitise(mt);

  start_tc=get_event_timecode(block->start_event);
  end_tc=get_event_timecode(block->end_event);

  first_event=get_prev_event(block->start_event);
  while (first_event!=NULL&&get_event_timecode(first_event)==start_tc) {
    first_event=get_prev_event(first_event);
  }

  event=block->end_event;

  if (mt->current_track!=-1&&!is_audio_eventbox(mt,eventbox)) {
    // delete frames
    while (event!=NULL&&!done) {
      prevevent=get_prev_frame_event(event);
      if (event==block->start_event) done=TRUE;
      remove_frame_from_event(mt->event_list,event,track);
      if (!done) event=prevevent;
    }
  }
  else {
    // update audio events
    // if last event in block turns audio off, delete it
    if (get_audio_frame_vel(block->end_event,track)==0.) {
      remove_audio_for_track(block->end_event,track);
    }

    // if first event in block is the end of another block, turn audio off (velocity==0)
    if (block->prev!=NULL&&block->start_event==block->prev->end_event) {
      insert_audio_event_at(mt->event_list,block->start_event,track,1,0.,0.);
    }						
    // else we'll delete it
    else {
      remove_audio_for_track(block->start_event,track);
    }
  }

  if ((blockprev=block->prev)!=NULL) blockprev->next=block->next;
  else g_object_set_data (G_OBJECT(eventbox),"blocks",(gpointer)block->next);
  if ((blocknext=block->next)!=NULL) blocknext->prev=blockprev;

  g_free(block);

  gtk_widget_queue_draw (eventbox);
  if (cfile->achans>0&&mt->audio_draws!=NULL&&mt->opts.back_audio_tracks>0&&eventbox==mt->audio_draws->data&&GPOINTER_TO_INT(g_object_get_data(G_OBJECT(eventbox),"expanded"))) {
    GtkWidget *xeventbox=(GtkWidget *)g_object_get_data(G_OBJECT(eventbox),"achan0");
    if (xeventbox!=NULL) gtk_widget_queue_draw (xeventbox);
    xeventbox=(GtkWidget *)g_object_get_data(G_OBJECT(eventbox),"achan1");
    if (xeventbox!=NULL) gtk_widget_queue_draw (xeventbox);
  }

  if (!mt->opts.move_effects||!mt->moving_block) {
    update_filter_events(mt,first_event,start_tc,end_tc,track,start_tc,track);
    remove_end_blank_frames(mt->event_list);
    if (mt->block_selected==block) {
      mt->block_selected=NULL;
      unselect_all(mt);
    }
    mt_sensitise(mt);
  }

  if ((mt->opts.grav_mode==GRAV_MODE_LEFT||mt->opts.grav_mode==GRAV_MODE_RIGHT)&&!mt->moving_block&&!did_backup) {
    // gravity left - remove first gap from old block start to end time
    // gravity right - remove last gap from 0 to old block end time


    gdouble oldr_start=mt->region_start;
    gdouble oldr_end=mt->region_end;
    GList *tracks_sel=NULL;
    if (mt->current_track!=-1) {
      tracks_sel=g_list_copy(mt->selected_tracks);
      if (mt->selected_tracks!=NULL) g_list_free(mt->selected_tracks);
      mt->selected_tracks=NULL;
      mt->selected_tracks=g_list_append(mt->selected_tracks,GINT_TO_POINTER(mt->current_track));
    }

    if (mt->opts.grav_mode==GRAV_MODE_LEFT) {
      mt->region_start=start_tc/U_SEC;
      mt->region_end=mt->end_secs;
    }
    else {
      mt->region_start=0.;
      mt->region_end=end_tc/U_SEC;
    }

    remove_first_gaps(NULL,mt);
    if (mt->current_track>-1) {
      g_list_free(mt->selected_tracks);
      mt->selected_tracks=g_list_copy(tracks_sel);
      if (tracks_sel!=NULL) g_list_free(tracks_sel);
    }
    mt->region_start=oldr_start;
    mt->region_end=oldr_end;
    mt_sensitise(mt);
  }

  remove_end_blank_frames(mt->event_list);

  if ((!mt->moving_block||get_first_frame_event(mt->event_list)==NULL)&&mt->avol_fx!=-1&&blocknext==NULL&&mt->audio_draws!=NULL&&get_first_event(mt->event_list)!=NULL) {
    apply_avol_filter(mt);
  }

  mt->did_backup=did_backup;
  if (!did_backup&&mt->framedraw!=NULL&&mt->current_rfx!=NULL&&mt->init_event!=NULL&&mt->poly_state==POLY_PARAMS&&weed_plant_has_leaf(mt->init_event,"in_tracks")) {
    weed_timecode_t tc=q_gint64(gtk_spin_button_get_value(GTK_SPIN_BUTTON(mt->node_spinbutton))*U_SEC+get_event_timecode(mt->init_event),mt->fps);
    get_track_index(mt,tc);
  }

  redraw_eventbox(mt,eventbox);

  if (!mt->moving_block) mt_show_current_frame(mt, FALSE);

  if (!did_backup) mt->idlefunc=mt_idle_add(mt);

}



void on_seltrack_activate (GtkMenuItem *menuitem, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  GtkWidget *eventbox;
  GtkWidget *checkbutton;
  gulong seltrack_func;
  gboolean mi_state;

  if (mt->current_track==-1) return;

  eventbox=(GtkWidget *)g_list_nth_data(mt->video_draws,mt->current_track);
  checkbutton=(GtkWidget *)g_object_get_data(G_OBJECT(eventbox),"checkbutton");
  seltrack_func=(gulong)g_object_get_data(G_OBJECT(checkbutton),"tfunc");
  mi_state=gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menuitem));

  if (mi_state) {
    // selected
    if (g_list_index(mt->selected_tracks,GINT_TO_POINTER(mt->current_track))==-1) 
      mt->selected_tracks=g_list_append(mt->selected_tracks,GINT_TO_POINTER(mt->current_track));
  }
  else {
    // unselected
    if (g_list_index(mt->selected_tracks,GINT_TO_POINTER(mt->current_track))!=-1) 
      mt->selected_tracks=g_list_remove(mt->selected_tracks,GINT_TO_POINTER(mt->current_track));
  }

#ifdef ENABLE_GIW
  if (!prefs->lamp_buttons) {
#endif
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkbutton))!=mi_state) {
      g_signal_handler_block(checkbutton,seltrack_func);
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbutton),mi_state);
      g_signal_handler_unblock(checkbutton,seltrack_func);
    }
#ifdef ENABLE_GIW
  }
  else {
    if (giw_led_get_mode(GIW_LED(checkbutton))!=mi_state) {
      g_signal_handler_block(checkbutton,seltrack_func);
      giw_led_set_mode(GIW_LED(checkbutton),mi_state);
      g_signal_handler_unblock(checkbutton,seltrack_func);
    }
  }
#endif
  do_sel_context(mt);

  gtk_widget_set_sensitive(mt->fx_region,FALSE);
  gtk_widget_set_sensitive(mt->ins_gap_sel,FALSE);
  gtk_widget_set_sensitive (mt->remove_gaps, FALSE);
  gtk_widget_set_sensitive (mt->remove_first_gaps, FALSE);
  gtk_widget_set_sensitive(mt->split_sel,FALSE);
  gtk_widget_set_sensitive(mt->fx_region_1,FALSE);
  gtk_widget_set_sensitive(mt->fx_region_2,FALSE);
  
  if (mt->selected_tracks!=NULL&&mt->event_list!=NULL&&get_first_event(mt->event_list)!=NULL) {
    gtk_widget_set_sensitive(mt->split_sel,TRUE);
    if (mt->region_start!=mt->region_end) {
      gtk_widget_set_sensitive(mt->ins_gap_sel,TRUE);
      gtk_widget_set_sensitive (mt->remove_gaps, TRUE);
      gtk_widget_set_sensitive (mt->remove_first_gaps, TRUE);
      gtk_widget_set_sensitive(mt->fx_region,TRUE);
      switch (g_list_length(mt->selected_tracks)) {
      case 1:
	gtk_widget_set_sensitive(mt->fx_region_1,TRUE);
	break;
      case 2:
	gtk_widget_set_sensitive(mt->fx_region_2,TRUE);
	break;
      default:
	break;
      }
    }
  }

  // update labels
  if (get_poly_state_from_page(mt)==POLY_TRANS||get_poly_state_from_page(mt)==POLY_COMP) {
    polymorph(mt,POLY_NONE);
    polymorph(mt,get_poly_state_from_page(mt));
  }

}


void on_seltrack_toggled (GtkWidget *checkbutton, gpointer user_data) {
  gint track=GPOINTER_TO_INT(g_object_get_data(G_OBJECT(checkbutton),"layer_number"));
  lives_mt *mt=(lives_mt *)user_data;

  mt->current_track=track;
  if (track>-1) mt->aud_track_selected=FALSE;
  else mt->aud_track_selected=TRUE;

  // track_select will call on_seltrack_activate, which will set our new state
  track_select(mt);

}



///////////////////////////////////////////////////////////


void mt_desensitise (lives_mt *mt) {
  gdouble val;
  gtk_widget_set_sensitive (mt->clipedit,FALSE);
  gtk_widget_set_sensitive (mt->insert,FALSE);
  gtk_widget_set_sensitive (mt->audio_insert,FALSE);
  gtk_widget_set_sensitive (mt->playall,FALSE);
  gtk_widget_set_sensitive (mt->playsel,FALSE);
  gtk_widget_set_sensitive (mt->view_events,FALSE);
  gtk_widget_set_sensitive (mt->view_sel_events,FALSE);
  gtk_widget_set_sensitive (mt->render, FALSE);
  gtk_widget_set_sensitive (mt->prerender_aud, FALSE);
  gtk_widget_set_sensitive (mt->delblock, FALSE);
  gtk_widget_set_sensitive (mt->save_event_list, FALSE);
  gtk_widget_set_sensitive (mt->load_event_list, FALSE);
  gtk_widget_set_sensitive (mt->clear_event_list, FALSE);
  gtk_widget_set_sensitive (mt->remove_gaps, FALSE);
  gtk_widget_set_sensitive (mt->remove_first_gaps, FALSE);
  gtk_widget_set_sensitive (mt->undo, FALSE);
  gtk_widget_set_sensitive (mt->redo, FALSE);
  gtk_widget_set_sensitive (mt->fx_edit,FALSE);
  gtk_widget_set_sensitive (mt->fx_delete,FALSE);
  gtk_widget_set_sensitive (mt->checkbutton_avel_reverse,FALSE);
  gtk_widget_set_sensitive (mt->spinbutton_avel,FALSE);
  gtk_widget_set_sensitive (mt->avel_scale,FALSE);
  gtk_widget_set_sensitive (mt->change_vals,FALSE);
  gtk_widget_set_sensitive (mt->add_vid_behind,FALSE);
  gtk_widget_set_sensitive (mt->add_vid_front,FALSE);
  gtk_widget_set_sensitive (mt->quit,FALSE);
  gtk_widget_set_sensitive (mt->open_menu,FALSE);
  gtk_widget_set_sensitive (mt->recent_menu,FALSE);
  gtk_widget_set_sensitive (mt->load_set,FALSE);
  gtk_widget_set_sensitive (mt->save_set,FALSE);
  gtk_widget_set_sensitive (mt->close,FALSE);
  gtk_widget_set_sensitive (mt->capture,FALSE);
  gtk_widget_set_sensitive (mt->gens_submenu,FALSE);
  gtk_widget_set_sensitive (mainw->troubleshoot, FALSE);

  gtk_widget_set_sensitive (mt->fx_region, FALSE);
  gtk_widget_set_sensitive (mt->ins_gap_sel, FALSE);
  gtk_widget_set_sensitive (mt->ins_gap_cur, FALSE);

  if (mt->poly_state==POLY_IN_OUT) {
    if (mt->block_selected!=NULL) {
      val=gtk_spin_button_get_value(GTK_SPIN_BUTTON(mt->spinbutton_in));
      gtk_spin_button_set_range(GTK_SPIN_BUTTON(mt->spinbutton_in),val,val);
      
      val=gtk_spin_button_get_value(GTK_SPIN_BUTTON(mt->spinbutton_out));
      gtk_spin_button_set_range(GTK_SPIN_BUTTON(mt->spinbutton_out),val,val);
    }
  }
}


void mt_sensitise (lives_mt *mt) {
  GtkWidget *eventbox=NULL;

  if (mt->event_list!=NULL&&get_first_event(mt->event_list)!=NULL) {
    gtk_widget_set_sensitive (mt->playall,TRUE);
    gtk_widget_set_sensitive (mainw->m_playbutton, TRUE);
    gtk_widget_set_sensitive (mt->view_events,TRUE);
    gtk_widget_set_sensitive (mt->view_sel_events,mt->region_start!=mt->region_end);
    gtk_widget_set_sensitive (mt->render, TRUE);
    if (mt->avol_init_event!=NULL&&mt->opts.pertrack_audio&&mainw->files[mt->render_file]->achans>0) 
      gtk_widget_set_sensitive (mt->prerender_aud, TRUE);
    gtk_widget_set_sensitive (mt->save_event_list, TRUE);
  }
  else {
    gtk_widget_set_sensitive (mt->playall,FALSE);
    gtk_widget_set_sensitive (mt->playsel,FALSE);
    gtk_widget_set_sensitive (mainw->m_playbutton, FALSE);
    gtk_widget_set_sensitive (mt->view_events,FALSE);
    gtk_widget_set_sensitive (mt->view_sel_events,FALSE);
    gtk_widget_set_sensitive (mt->render,FALSE);
    gtk_widget_set_sensitive (mt->save_event_list,FALSE);
  }

  if (mt->event_list!=NULL) gtk_widget_set_sensitive (mt->clear_event_list, TRUE);

  gtk_widget_set_sensitive (mt->add_vid_behind,TRUE);
  gtk_widget_set_sensitive (mt->add_vid_front,TRUE);
  gtk_widget_set_sensitive (mt->quit,TRUE);
  gtk_widget_set_sensitive (mt->open_menu,TRUE);
  gtk_widget_set_sensitive (mt->recent_menu,TRUE);
  gtk_widget_set_sensitive (mt->capture,TRUE);
  gtk_widget_set_sensitive (mt->gens_submenu,TRUE);
  gtk_widget_set_sensitive (mainw->troubleshoot, TRUE);

  gtk_widget_set_sensitive (mainw->m_mutebutton, TRUE);

  gtk_widget_set_sensitive (mt->load_set,!mainw->was_set);

  if (mt->undoable) gtk_widget_set_sensitive (mt->undo, TRUE);
  if (mt->redoable) gtk_widget_set_sensitive (mt->redo, TRUE);
  if (mt->selected_init_event!=NULL) gtk_widget_set_sensitive(mt->fx_edit,TRUE);
  if (mt->selected_init_event!=NULL) gtk_widget_set_sensitive(mt->fx_delete,TRUE);
  gtk_widget_set_sensitive (mt->checkbutton_avel_reverse,TRUE);

  if (mt->block_selected!=NULL&&(!mt->block_selected->start_anchored||
				 !mt->block_selected->end_anchored)&&!gtk_toggle_button_get_active
      (GTK_TOGGLE_BUTTON(mt->checkbutton_avel_reverse))) {
    gtk_widget_set_sensitive (mt->spinbutton_avel,TRUE);
    gtk_widget_set_sensitive(mt->avel_scale,TRUE);
  }

  gtk_widget_set_sensitive (mt->load_event_list, strlen(mainw->set_name)>0);
  gtk_widget_set_sensitive (mt->clipedit,TRUE);
  if (mt->file_selected>-1) {
    if (mainw->files[mt->file_selected]->frames>0) gtk_widget_set_sensitive (mt->insert,TRUE);
    if (mainw->files[mt->file_selected]->achans>0&&mainw->files[mt->file_selected]->laudio_time>0.) 
      gtk_widget_set_sensitive (mt->audio_insert,TRUE);
    gtk_widget_set_sensitive (mt->save_set,TRUE);
    gtk_widget_set_sensitive (mt->close,TRUE);
    gtk_widget_set_sensitive (mt->adjust_start_end, TRUE);
  }

  if (mt->video_draws!=NULL&&mt->current_track>-1) eventbox=(GtkWidget *)g_list_nth_data (mt->video_draws,mt->current_track);
  else if (mt->audio_draws!=NULL) eventbox=(GtkWidget *)mt->audio_draws->data;

  if (eventbox!=NULL) {
    gtk_widget_set_sensitive (mt->jumpback, g_object_get_data(G_OBJECT(eventbox),"blocks")!=NULL);
    gtk_widget_set_sensitive (mt->jumpnext, g_object_get_data(G_OBJECT(eventbox),"blocks")!=NULL);
  }

  gtk_widget_set_sensitive (mt->change_vals,TRUE);

  if (mt->block_selected) {
    gtk_widget_set_sensitive (mt->delblock, TRUE);
    if (mt->poly_state==POLY_IN_OUT&&mt->block_selected->ordered) {
      weed_timecode_t offset_end=mt->block_selected->offset_start+(weed_timecode_t)(U_SEC/mt->fps)+
	(get_event_timecode(mt->block_selected->end_event)-get_event_timecode(mt->block_selected->start_event));
      
      g_signal_handler_block (mt->spinbutton_out,mt->spin_out_func);
      g_signal_handler_block (mt->spinbutton_in,mt->spin_in_func);
      set_in_out_spin_ranges(mt,mt->block_selected->offset_start,offset_end);
      g_signal_handler_unblock (mt->spinbutton_in,mt->spin_in_func);
      g_signal_handler_unblock (mt->spinbutton_out,mt->spin_out_func);
    }
  }
  else if (mt->poly_state==POLY_IN_OUT) {
    gint filenum=mt_file_from_clip(mt,mt->clip_selected);
    g_signal_handler_block (mt->spinbutton_in,mt->spin_in_func);
    g_signal_handler_block (mt->spinbutton_out,mt->spin_out_func);
    gtk_spin_button_set_range(GTK_SPIN_BUTTON(mt->spinbutton_in),1., mainw->files[filenum]->frames);
    gtk_spin_button_set_range(GTK_SPIN_BUTTON(mt->spinbutton_out),1., mainw->files[filenum]->frames);

    g_signal_handler_unblock (mt->spinbutton_in,mt->spin_in_func);
    g_signal_handler_unblock (mt->spinbutton_out,mt->spin_out_func);
  }




  if (mt->region_end>mt->region_start&&mt->event_list!=NULL&&get_first_event(mt->event_list)!=NULL) {
    if (mt->selected_tracks!=NULL) {
      gtk_widget_set_sensitive (mt->fx_region, TRUE);
      gtk_widget_set_sensitive (mt->ins_gap_sel, TRUE);
      gtk_widget_set_sensitive (mt->remove_gaps, TRUE);
      gtk_widget_set_sensitive (mt->remove_first_gaps, TRUE);
    }
    gtk_widget_set_sensitive(mt->playsel,TRUE);
    gtk_widget_set_sensitive (mt->ins_gap_cur, TRUE);
    gtk_widget_set_sensitive(mt->view_sel_events,TRUE);
  }

  track_select(mt);

}


void mt_swap_play_pause (lives_mt *mt, gboolean put_pause) {
  GtkWidget *tmp_img;

  if (put_pause) {
    tmp_img = gtk_image_new_from_stock ("gtk-media-pause", gtk_toolbar_get_icon_size (GTK_TOOLBAR (mainw->btoolbar)));
    set_menu_text(mt->playall,_("_Pause"),TRUE);
    gtk_tool_item_set_tooltip_text(GTK_TOOL_ITEM(mainw->m_playbutton),_("Pause (p)"));
    gtk_widget_set_sensitive(mt->playall,TRUE);
    gtk_widget_set_sensitive(mainw->m_playbutton,TRUE);
  }
  else {
    tmp_img = gtk_image_new_from_stock ("gtk-media-play", gtk_toolbar_get_icon_size (GTK_TOOLBAR (mainw->btoolbar)));
    set_menu_text(mt->playall,_("_Play from Timeline Position"),TRUE);
    gtk_tool_item_set_tooltip_text(GTK_TOOL_ITEM(mainw->m_playbutton),_("Play all (p)"));
  }
  gtk_widget_show(tmp_img);
  gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(mainw->m_playbutton),tmp_img);
}




//////////////////////////////////////////////////////////////////
void multitrack_preview_clicked  (GtkWidget *button, gpointer user_data) {
  //preview during rendering
  lives_mt *mt=(lives_mt *)user_data;

  if (mainw->playing_file==-1) multitrack_playall(mt);
  else mainw->cancelled=CANCEL_NO_PROPOGATE;
}




void mt_prepare_for_playback(lives_mt *mt) {
  // called from on_preview_clicked

  pb_loop_event=mt->pb_loop_event;
  pb_filter_map=mainw->filter_map; // keep a copy of this, in case we are rendering
  pb_audio_needs_prerender=GTK_WIDGET_SENSITIVE(mt->prerender_aud);

  mt_desensitise(mt);

  if (mt->mt_frame_preview) {
    // put blank back in preview window
    if (mt->framedraw!=NULL) gtk_widget_modify_bg (mt->fd_frame, GTK_STATE_NORMAL, &palette->normal_back);
    
  }
  else {
    gtk_widget_ref(mt->play_blank);
    gtk_container_remove (GTK_CONTAINER(mt->play_box),mt->play_blank);
  }

  gtk_widget_set_sensitive (mt->stop,TRUE);
  gtk_widget_set_sensitive (mt->rewind,FALSE);
  gtk_widget_set_sensitive (mainw->m_rewindbutton, FALSE);

  if (!mt->is_paused&&!mt->playing_sel) mt->ptr_time=GTK_RULER(mt->timeline)->position;

  mainw->must_resize=TRUE;

  if (mainw->play_window==NULL) {
    mainw->pwidth=cfile->hsize;
    mainw->pheight=cfile->vsize;
    calc_maxspect(mt->play_width,mt->play_height,&mainw->pwidth,&mainw->pheight);
  }
  else {
    mainw->pwidth=cfile->hsize;
    mainw->pheight=cfile->vsize;
  }

}



void mt_post_playback(lives_mt *mt) {
  // called from on_preview_clicked

  unhide_cursor(mainw->playarea->window);
    
  mainw->must_resize=FALSE;

  if (mainw->cancelled!=CANCEL_USER_PAUSED&&!((mainw->cancelled==CANCEL_NONE||mainw->cancelled==CANCEL_NO_MORE_PREVIEW)&&mt->is_paused)) {
    gtk_widget_set_sensitive (mt->stop,FALSE);
    mt_tl_move(mt,mt->ptr_time-GTK_RULER (mt->timeline)->position);
  }
  else {
    mt->is_paused=TRUE;
    if (GTK_RULER(mt->timeline)->position>0.) {
      gtk_widget_set_sensitive (mt->rewind,TRUE);
      gtk_widget_set_sensitive (mainw->m_rewindbutton, TRUE);
    }
  }

  mainw->cancelled=CANCEL_NONE;
  gtk_widget_show(mainw->playarea);
  unpaint_lines(mt);

  if (!mt->is_rendering) {
    if (mt->poly_state==POLY_PARAMS) {
      if (mt->init_event!=NULL) {
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(mt->node_spinbutton),(GTK_RULER(mt->timeline)->position-get_event_timecode(mt->init_event)/U_SEC));
	gtk_widget_set_sensitive(mt->apply_fx_button,FALSE);
      }
    }
    if (mt->poly_state==POLY_FX_STACK) {
      polymorph(mt,POLY_FX_STACK);
    }
  }

  if (mainw->play_window!=NULL) {
    g_signal_handlers_block_matched(mainw->play_window,(GSignalMatchType)(G_SIGNAL_MATCH_FUNC|G_SIGNAL_MATCH_UNBLOCKED),
				    0,0,0,(gpointer)expose_play_window,NULL);
    g_signal_handler_unblock(mainw->play_window,mainw->pw_exp_func);
    mainw->pw_exp_is_blocked=FALSE;
  }
  if (mt->ptr_time>0.) {
    gtk_widget_set_sensitive (mt->rewind,TRUE);
    gtk_widget_set_sensitive (mainw->m_rewindbutton, TRUE);
  }

  mainw->filter_map=pb_filter_map;

  if (mt->is_paused) mt->pb_loop_event=pb_loop_event;
  gtk_widget_set_sensitive (mainw->m_playbutton, TRUE);
  mt_show_current_frame(mt, FALSE);


}




void multitrack_playall (lives_mt *mt) {
  GtkWidget *old_context_scroll=mt->context_scroll;

  if (mainw->current_file<1) return;

  if (mt->idlefunc>0) {
    g_source_remove(mt->idlefunc);
    mt->idlefunc=0;
  }

  gtk_widget_ref(mt->context_scroll); // this allows us to get our old messages back
  gtk_container_remove (GTK_CONTAINER (mt->context_frame), mt->context_scroll);
  mt->context_scroll=NULL;
  clear_context(mt);

  add_context_label(mt,_("Press 'm' during playback"));
  add_context_label(mt,_("to make a mark on the timeline"));

  if (mt->opts.follow_playback) {
    gdouble currtime=GTK_RULER(mt->timeline)->position;
    if (currtime>mt->tl_max||currtime<mt->tl_min) {
      gdouble page=mt->tl_max-mt->tl_min;
      mt->tl_min=currtime-page*.25;
      mt->tl_max=currtime+page*.75;
      mt_zoom(mt,-1.);
    }
  }

  if (mt->is_rendering) {
    // preview during rendering
    gboolean had_audio=mt->has_audio_file;
    mt->pb_start_event=NULL;
    mt->has_audio_file=TRUE;
    on_preview_clicked(GTK_BUTTON(cfile->proc_ptr->preview_button),NULL);
    mt->has_audio_file=had_audio;
  }
  else {
    if (mt->event_list!=NULL) {
      mainw->is_rendering=TRUE;  // NOTE : mainw->is_rendering is not the same as mt->is_rendering !
      set_play_position(mt);
      if (mainw->cancelled!=CANCEL_VID_END) {
	// otherwise jack transport set us out of range

	if (mt->playing_sel) mt->pb_loop_event=get_frame_event_at(mt->event_list,q_gint64(mt->region_start*U_SEC,mt->fps),NULL,TRUE);
	else if (mt->is_paused) mt->pb_loop_event=pb_loop_event;
	
	on_preview_clicked (NULL,GINT_TO_POINTER(1));
      }

      mainw->is_rendering=mainw->is_processing=FALSE;
    }
  }

  mt_swap_play_pause(mt,FALSE);

  gtk_container_remove (GTK_CONTAINER (mt->context_frame), mt->context_scroll);

  mt->context_scroll=old_context_scroll;
  gtk_container_add (GTK_CONTAINER (mt->context_frame), mt->context_scroll);

  if (mt->opts.show_ctx) gtk_widget_show_all(mt->context_frame);

  gtk_widget_unref(mt->context_scroll);

  if (!mt->is_rendering) mt_sensitise(mt);
  if (!pb_audio_needs_prerender) gtk_widget_set_sensitive(mt->prerender_aud,FALSE);

  mt->idlefunc=mt_idle_add(mt);

}



void multitrack_play_sel (GtkMenuItem *menuitem, gpointer user_data) {
  // get current pointer time; if it is outside the time region jump to start
  gdouble ptr_time;
  lives_mt *mt=(lives_mt *)user_data;

  ptr_time=GTK_RULER(mt->timeline)->position;
  if (!mt->is_paused) mt->ptr_time=ptr_time;

  if (ptr_time<mt->region_start||ptr_time>=mt->region_end) {
    GTK_RULER(mt->timeline)->position=mt->region_start;
  }

  // set loop start point to region start, and pb_start to current position
  // set loop end point to region end or tl end, whichever is soonest
  mt->playing_sel=TRUE;

  multitrack_playall(mt);

  // on return here, return the pointer to its original position, unless paused
  if (!mt->is_paused) {
    mt->playing_sel=FALSE;
  }
}



void multitrack_adj_start_end (GtkMenuItem *menuitem, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  unselect_all(mt);
  polymorph (mt,POLY_IN_OUT);
}





void multitrack_insert (GtkMenuItem *menuitem, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  file *sfile=mainw->files[mt->file_selected];
  gdouble secs=GTK_RULER (mt->timeline)->position;
  GtkWidget *eventbox;
  weed_timecode_t ins_start=(sfile->start-1.)/sfile->fps*U_SEC;
  weed_timecode_t ins_end=(gdouble)(sfile->end)/sfile->fps*U_SEC;
  gboolean did_backup=mt->did_backup;
  track_rect *block;

  if (mt->current_track==-1) {
    multitrack_audio_insert(menuitem,user_data);
    return;
  }

  if (sfile->frames==0) return;

  if (!did_backup&&mt->idlefunc>0) {
    g_source_remove(mt->idlefunc);
    mt->idlefunc=0;
  }

  if (mt->context_time!=-1.&&mt->use_context) {
    secs=mt->context_time;
    mt->context_time=-1.;
    mt->use_context=FALSE;
  }

  eventbox=(GtkWidget *)g_list_nth_data(mt->video_draws,mt->current_track);
  if (!did_backup) mt_backup(mt,MT_UNDO_INSERT_BLOCK,0);

  if (mt->opts.ign_ins_sel) {
    // ignore selection limits
    ins_start=0;
    ins_end=(gdouble)(sfile->frames)/sfile->fps*U_SEC;
  }

  if (mt->insert_start!=-1) {
    // used if we move a block
    ins_start=mt->insert_start;
    ins_end=mt->insert_end;
  }

  insert_frames (mt->file_selected,ins_start,ins_end,secs*U_SECL,DIRECTION_POSITIVE,eventbox,mt,NULL);

  block=(track_rect *)g_object_get_data(G_OBJECT(eventbox),"block_last");

  if (block!=NULL&&(mt->opts.grav_mode==GRAV_MODE_LEFT||(block->next!=NULL&&mt->opts.grav_mode==GRAV_MODE_RIGHT))&&!(did_backup||mt->moving_block)) {
    gdouble oldr_start=mt->region_start;
    gdouble oldr_end=mt->region_end;
    GList *tracks_sel;
    track_rect *selblock=NULL;
    if (mt->block_selected!=block) selblock=mt->block_selected;
    tracks_sel=g_list_copy(mt->selected_tracks);
    if (mt->selected_tracks!=NULL) g_list_free(mt->selected_tracks);
    mt->selected_tracks=NULL;
    mt->selected_tracks=g_list_append(mt->selected_tracks,GINT_TO_POINTER(mt->current_track));
    
    if (mt->opts.grav_mode==GRAV_MODE_LEFT) {
      if (block->prev!=NULL) mt->region_start=get_event_timecode(block->prev->end_event)/U_SEC;
      else mt->region_start=0.;
      mt->region_end=get_event_timecode(block->start_event)/U_SEC;
    }
    else {
      mt->region_start=get_event_timecode(block->end_event)/U_SEC;
      mt->region_end=get_event_timecode(block->next->start_event)/U_SEC;
    }
    
    remove_first_gaps(NULL,mt);
    g_list_free(mt->selected_tracks);
    mt->selected_tracks=g_list_copy(tracks_sel);
    if (tracks_sel!=NULL) g_list_free(tracks_sel);
    mt->region_start=oldr_start;
    mt->region_end=oldr_end;
    mt_sensitise(mt);
    if (selblock!=NULL) mt->block_selected=selblock;
  }

  // get this again because it could have moved
  block=(track_rect *)g_object_get_data(G_OBJECT(eventbox),"block_last");

  if (!did_backup) {
    if (mt->avol_fx!=-1&&block!=NULL&&block->next==NULL&&get_first_event(mt->event_list)!=NULL) {
      apply_avol_filter(mt);
    }
  }

  if (prefs->atrans_fx!=-1) mt_do_autotransition(mt, block);

  mt->did_backup=did_backup;

  if (!resize_timeline(mt)) {
    redraw_eventbox(mt,eventbox);
  }

  if (!did_backup&&mt->framedraw!=NULL&&mt->current_rfx!=NULL&&mt->init_event!=NULL&&mt->poly_state==POLY_PARAMS&&weed_plant_has_leaf(mt->init_event,"in_tracks")) {
    weed_timecode_t tc=q_gint64(gtk_spin_button_get_value(GTK_SPIN_BUTTON(mt->node_spinbutton))*U_SEC+get_event_timecode(mt->init_event),mt->fps);
    get_track_index(mt,tc);
  }

  mt_tl_move(mt,0.);
  mt_show_current_frame(mt, FALSE);

  if (!did_backup) mt->idlefunc=mt_idle_add(mt);

}


void multitrack_audio_insert (GtkMenuItem *menuitem, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  file *sfile=mainw->files[mt->file_selected];
  gdouble secs=GTK_RULER (mt->timeline)->position;
  GtkWidget *eventbox=(GtkWidget *)mt->audio_draws->data;
  weed_timecode_t ins_start=q_gint64((sfile->start-1.)/sfile->fps*U_SEC,mt->fps);
  weed_timecode_t ins_end=q_gint64((gdouble)sfile->end/sfile->fps*U_SEC,mt->fps);
  gboolean did_backup=mt->did_backup;
  track_rect *block;
  gchar *text,*tmp;
  lives_direction_t dir;

  if (mt->current_track!=-1||sfile->achans==0) return;

  if (!did_backup&&mt->idlefunc>0) {
    g_source_remove(mt->idlefunc);
    mt->idlefunc=0;
  }

  if (mt->context_time!=-1.&&mt->use_context) {
    secs=mt->context_time;
    mt->context_time=-1.;
    mt->use_context=FALSE;
  }

  if (sfile->frames==0||mt->opts.ign_ins_sel) {
    ins_start=0;
    ins_end=q_gint64(sfile->laudio_time*U_SEC,mt->fps);
  }
  else {
    if (ins_end>q_gint64((gdouble)(sfile->frames-.5)/sfile->fps*U_SEC,mt->fps)) {
      q_gint64(((gdouble)sfile->end-.5)/sfile->fps*U_SEC,mt->fps);
    }
  }

  if (ins_start>q_gint64(sfile->laudio_time*U_SEC,mt->fps)) {
    return;
  }

  if (ins_end>q_gint64(sfile->laudio_time*U_SEC,mt->fps)) {
    ins_end=q_gint64(sfile->laudio_time*U_SEC,mt->fps);
  }
  
  if (!did_backup) mt_backup(mt,MT_UNDO_INSERT_AUDIO_BLOCK,0);

  if (mt->insert_start!=-1) {
    ins_start=mt->insert_start;
    ins_end=mt->insert_end;
  }

  if (mt->insert_avel>0.) dir=DIRECTION_POSITIVE;
  else dir=DIRECTION_NEGATIVE;

  insert_audio (mt->file_selected,ins_start,ins_end,secs*U_SECL,mt->insert_avel,dir,eventbox,mt,NULL);

  block=(track_rect *)g_object_get_data(G_OBJECT(eventbox),"block_last");

  if (block!=NULL&&(mt->opts.grav_mode==GRAV_MODE_LEFT||(mt->opts.grav_mode==GRAV_MODE_RIGHT&&block->next!=NULL))&&!(did_backup||mt->moving_block)) {
    gdouble oldr_start=mt->region_start;
    gdouble oldr_end=mt->region_end;
    GList *tracks_sel;
    track_rect *selblock=NULL;
    if (mt->block_selected!=block) selblock=mt->block_selected;
    tracks_sel=g_list_copy(mt->selected_tracks);
    if (mt->selected_tracks!=NULL) g_list_free(mt->selected_tracks);
    mt->selected_tracks=NULL;
    mt->current_track=-1;
    
    if (mt->opts.grav_mode==GRAV_MODE_LEFT) {
      if (block->prev!=NULL) mt->region_start=get_event_timecode(block->prev->end_event)/U_SEC;
      else mt->region_start=0.;
      mt->region_end=get_event_timecode(block->start_event)/U_SEC;
    }
    else {
      mt->region_start=get_event_timecode(block->end_event)/U_SEC;
      mt->region_end=get_event_timecode(block->next->start_event)/U_SEC;
    }
    
    remove_first_gaps(NULL,mt);
    g_list_free(mt->selected_tracks);
    mt->selected_tracks=g_list_copy(tracks_sel);
    if (tracks_sel!=NULL) g_list_free(tracks_sel);
    mt->region_start=oldr_start;
    mt->region_end=oldr_end;
    if (selblock!=NULL) mt->block_selected=selblock;
  }

  mt->did_backup=did_backup;
  
  text=g_strdup_printf(_("Inserted audio %.4f to %.4f from clip %s into backing audio from time %.4f to %.4f\n"),ins_start/U_SEC,ins_end/U_SEC,(tmp=g_path_get_basename(sfile->name)),secs,secs+(ins_end-ins_start)/U_SEC);
  d_print(text);
  g_free(tmp);
  g_free(text);

  if (!resize_timeline(mt)) {
    redraw_eventbox(mt,eventbox);
    if (GPOINTER_TO_INT(g_object_get_data(G_OBJECT(eventbox),"expanded"))) {
      GtkWidget *xeventbox=(GtkWidget *)g_object_get_data(G_OBJECT(eventbox),"achan0");
      if (xeventbox!=NULL) gtk_widget_queue_draw (xeventbox);
      xeventbox=(GtkWidget *)g_object_get_data(G_OBJECT(eventbox),"achan1");
      if (xeventbox!=NULL) gtk_widget_queue_draw (xeventbox);
    }
  }

  // get this again because it could have moved
  block=(track_rect *)g_object_get_data(G_OBJECT(eventbox),"block_last");
  
  if (!did_backup) {
    if (mt->avol_fx!=-1&&block!=NULL&&block->next==NULL&&get_first_event(mt->event_list)!=NULL) {
      apply_avol_filter(mt);
    }
  }

  mt_tl_move(mt,0.);

  if (!did_backup&&mt->framedraw!=NULL&&mt->current_rfx!=NULL&&mt->init_event!=NULL&&mt->poly_state==POLY_PARAMS&&weed_plant_has_leaf(mt->init_event,"in_tracks")) {
    weed_timecode_t tc=q_gint64(gtk_spin_button_get_value(GTK_SPIN_BUTTON(mt->node_spinbutton))*U_SEC+get_event_timecode(mt->init_event),mt->fps);
    get_track_index(mt,tc);
  }

  if (!did_backup) mt->idlefunc=mt_idle_add(mt);

}

 
void insert_frames (gint filenum, weed_timecode_t offset_start, weed_timecode_t offset_end, weed_timecode_t tc, lives_direction_t direction, GtkWidget *eventbox, lives_mt *mt, track_rect *in_block) {
  // insert the selected frames from mainw->files[filenum] from source file filenum into mt->event_list starting at timeline timecode tc
  // if in_block is non-NULL, then we extend (existing) in_block with the new frames; otherwise we create a new block and insert it into eventbox

  // this is quite complex as the framerates of the sourcefile and the timeline might not match. Therefore we resample (in memory) our source file
  // After resampling, we insert resampled frames from offset_start (inclusive) to offset_end (non-inclusive) [forwards]
  // or from offset_start (non-inclusive) to offset_end (inclusive) if going backwards 

  // if we are inserting in an existing block, we can only use this to extend the end (not shrink it)

  // we also optionally insert with audio

  // TODO - handle extend with audio

  // TODO - handle insert before, insert after

  // TODO - handle case where frames are overwritten

  int numframes,i;
  gint render_file=mainw->current_file;
  gboolean isfirst=TRUE;
  file *sfile=mainw->files[filenum];
  gint track=GPOINTER_TO_INT(g_object_get_data(G_OBJECT(eventbox),"layer_number"));
  weed_timecode_t last_tc=0,offset_start_tc,start_tc,last_offset;
  int *clips=NULL,*frames=NULL,*rep_clips,*rep_frames,error;
  weed_plant_t *last_frame_event=NULL;
  weed_plant_t *event,*shortcut1=NULL,*shortcut2=NULL;
  int frame=((gdouble)(offset_start/U_SEC)*mt->fps+1.4999);
  track_rect *new_block=NULL;
  gchar *text;
  weed_timecode_t orig_st=offset_start,orig_end=offset_end;
  GtkWidget *aeventbox=NULL;
  gdouble aseek;
  gdouble end_secs;

  mt_desensitise(mt);

  g_object_set_data (G_OBJECT(eventbox),"block_last",(gpointer)NULL);
  if ((aeventbox=(GtkWidget *)g_object_get_data(G_OBJECT(eventbox),"atrack"))!=NULL) g_object_set_data (G_OBJECT(aeventbox),"block_last",(gpointer)NULL);

  last_offset=offset_start_tc=q_gint64(offset_start,mt->fps);
  offset_end=q_gint64(offset_end,mt->fps);
  start_tc=q_gint64(tc,mt->fps);
  if (direction==DIRECTION_NEGATIVE) tc-=U_SEC/mt->fps;
  last_tc=q_gint64(tc,mt->fps);

  if (direction==DIRECTION_POSITIVE) {
    // fill in blank frames in a gap
    if (mt->event_list!=NULL) last_frame_event=get_last_frame_event(mt->event_list);
    mt->event_list=add_blank_frames_up_to(mt->event_list,last_frame_event,q_gint64(start_tc-1./mt->fps,mt->fps),mt->fps);
  }

  mainw->current_file=filenum;

  if (cfile->fps!=mt->fps&&cfile->event_list==NULL) {
    // resample clip to render fps
    cfile->undo1_dbl=mt->fps;
    on_resample_vid_ok (NULL,NULL);
  }

  mainw->current_file=render_file;

  while (((direction==DIRECTION_POSITIVE&&(offset_start=q_gint64(last_tc-start_tc+offset_start_tc,mt->fps))<offset_end)||(direction==DIRECTION_NEGATIVE&&(offset_start=q_gint64(last_tc+offset_start_tc-start_tc,mt->fps))>=offset_end))) {
    numframes=0;
    clips=rep_clips=NULL;
    frames=rep_frames=NULL;
    
    if ((event=get_frame_event_at (mt->event_list,last_tc,shortcut1,TRUE))!=NULL) {
      // TODO - memcheck
      numframes=weed_leaf_num_elements(event,"clips");
      clips=weed_get_int_array(event,"clips",&error);
      frames=weed_get_int_array(event,"frames",&error);
      shortcut1=event;
    }
    else if (direction==DIRECTION_POSITIVE&&mt->event_list!=NULL) {
      shortcut1=get_last_event(mt->event_list);
    }

    if (numframes<=track) {
      // TODO - memcheck
      rep_clips=(int *)g_malloc(track*sizint+sizint);
      rep_frames=(int *)g_malloc(track*sizint+sizint);

      for (i=0;i<track;i++) {
	if (i<numframes) {
	  rep_clips[i]=clips[i];
	  rep_frames[i]=frames[i];
	}
	else {
	  rep_clips[i]=-1;
	  rep_frames[i]=0;
	}
      }
      numframes=track+1;
    }
    else {
      if (mt->opts.insert_mode==INSERT_MODE_NORMAL&&frames[track]>0) {
	if (in_block==NULL&&new_block!=NULL) {
	  if (direction==DIRECTION_POSITIVE) {
	    shortcut1=get_prev_frame_event(shortcut1);
	  }
	}
	if (clips!=NULL) weed_free(clips);
	if (frames!=NULL) weed_free(frames);
	break; // do not allow overwriting in this mode
      }
      rep_clips=clips;
      rep_frames=frames;
    }

    if (sfile->event_list!=NULL) event=get_frame_event_at(sfile->event_list,offset_start,shortcut2,TRUE);
    if (sfile->event_list!=NULL&&event==NULL) {
      if (rep_clips!=clips&&rep_clips!=NULL) g_free(rep_clips);
      if (rep_frames!=frames&&rep_frames!=NULL) g_free(rep_frames);
      if (clips!=NULL) weed_free(clips);
      if (frames!=NULL) weed_free(frames);
      break; // insert finished: ran out of frames in resampled clip
    }
    last_offset=offset_start;
    if (sfile->event_list!=NULL) {
      // frames were resampled, get new frame at the source file timecode
      frame=weed_get_int_value(event,"frames",&error);
      if (direction==DIRECTION_POSITIVE) shortcut2=event;
      else shortcut2=get_prev_frame_event(event); // TODO : this is not optimal for the first frame
    }
    rep_clips[track]=filenum;
    rep_frames[track]=frame;

    // TODO - memcheck
    mt->event_list=insert_frame_event_at (mt->event_list,last_tc,numframes,rep_clips,rep_frames,&shortcut1);

    if (rep_clips!=clips&&rep_clips!=NULL) g_free(rep_clips);
    if (rep_frames!=frames&&rep_frames!=NULL) g_free(rep_frames);

    if (isfirst) {
      // TODO - memcheck
      if (in_block==NULL) {
	new_block=add_block_start_point (eventbox,last_tc,filenum,offset_start,shortcut1,TRUE);
	if (aeventbox!=NULL) {
	  if (cfile->achans>0&&sfile->achans>0&&mt->opts.insert_audio) {
	    // insert audio start or end
	    if (direction==DIRECTION_POSITIVE) {
	      aseek=(gdouble)(frame-1.)/sfile->fps;

	      insert_audio_event_at(mt->event_list,shortcut1,track,filenum,aseek,1.);
	      add_block_start_point (aeventbox,last_tc,filenum,offset_start,shortcut1,TRUE);
	    }
	    else {
	      weed_plant_t *nframe;
	      if ((nframe=get_next_frame_event(shortcut1))==NULL) {
		mt->event_list=insert_blank_frame_event_at(mt->event_list,q_gint64(last_tc+U_SEC/mt->fps,mt->fps),&shortcut1);
		nframe=shortcut1;
	      }
	      insert_audio_event_at(mt->event_list,nframe,track,filenum,0.,0.);
	    }
	  }
	}
	isfirst=FALSE;
      }
    }

    if (clips!=NULL) weed_free(clips);
    if (frames!=NULL) weed_free(frames);
    
    if (direction==DIRECTION_POSITIVE) last_tc+=U_SEC/mt->fps;
    else {
      if (last_tc<U_SEC/mt->fps) break;
      last_tc-=U_SEC/mt->fps;
    }
    last_tc=q_gint64(last_tc,mt->fps);
    if (sfile->event_list==NULL) if ((direction==DIRECTION_POSITIVE&&(++frame>sfile->frames))||(direction==DIRECTION_NEGATIVE&&(--frame<1))) {
      break;
    }
  }

  if (!isfirst) {
    if (direction==DIRECTION_POSITIVE) {
      if (in_block!=NULL) g_object_set_data (G_OBJECT(eventbox),"block_last",(gpointer)in_block);
      add_block_end_point(eventbox,shortcut1);

      if (cfile->achans>0&&sfile->achans>0&&mt->opts.insert_audio&&mt->opts.pertrack_audio) {
	weed_plant_t *shortcut2=get_next_frame_event(shortcut1);
	if (shortcut2==NULL) {
	  mt->event_list=insert_blank_frame_event_at(mt->event_list,last_tc,&shortcut1);
	}
	else shortcut1=shortcut2;
	insert_audio_event_at(mt->event_list,shortcut1,track,filenum,0.,0.);
	add_block_end_point(aeventbox,shortcut1);
      }
    }
    else if (in_block!=NULL) {
      in_block->offset_start=last_offset;
      in_block->start_event=shortcut1;
      if (cfile->achans>0&&sfile->achans>0&&mt->opts.insert_audio&&mt->opts.pertrack_audio) {
	weed_plant_t *shortcut2=get_next_frame_event(shortcut1);
	if (shortcut2==NULL) {
	  mt->event_list=insert_blank_frame_event_at(mt->event_list,last_tc,&shortcut1);
	}
	else shortcut1=shortcut2;
      }
    }
  }

  mt->last_direction=direction;

  if (mt->event_list!=NULL) {
    weed_set_double_value(mt->event_list,"fps",mainw->files[render_file]->fps);
  }

  if (in_block==NULL) {
    gchar *tmp,*tmp1;
    text=g_strdup_printf(_("Inserted frames %d to %d from clip %s into track %s from time %.4f to %.4f\n"),sfile->start,sfile->end,(tmp1=g_path_get_basename(sfile->name)),(tmp=get_track_name(mt,mt->current_track,mt->aud_track_selected)),(orig_st+start_tc)/U_SEC,(orig_end+start_tc)/U_SEC);
    g_free(tmp);
    g_free(tmp1);
    d_print(text);
    g_free(text);
  }

  end_secs=event_list_get_end_secs(mt->event_list);
  if (end_secs>mt->end_secs) {
    set_timeline_end_secs(mt,end_secs);
  }
  mt_sensitise(mt);
}


void insert_audio (gint filenum, weed_timecode_t offset_start, weed_timecode_t offset_end, weed_timecode_t tc, gdouble avel, lives_direction_t direction, GtkWidget *eventbox, lives_mt *mt, track_rect *in_block) {
  // insert the selected audio from mainw->files[filenum] from source file filenum into mt->event_list starting at timeline timecode tc
  // if in_block is non-NULL, then we extend (existing) in_block with the new frames; otherwise we create a new block and insert it into eventbox
  weed_timecode_t start_tc=q_gint64(tc,mt->fps);
  weed_timecode_t end_tc=q_gint64(start_tc+offset_end-offset_start,mt->fps);
  weed_plant_t *last_frame_event;
  track_rect *block;
  weed_plant_t *shortcut=NULL;
  weed_plant_t *frame_event;

  gdouble end_secs;

  g_object_set_data (G_OBJECT(eventbox),"block_last",(gpointer)NULL);

  if (direction==DIRECTION_NEGATIVE) {
    weed_timecode_t tmp_tc=offset_end;
    offset_end=offset_start;
    offset_start=tmp_tc;
  }

  // if already block at tc, return
  if ((block=get_block_from_time((GtkWidget *)mt->audio_draws->data,start_tc/U_SEC,mt))!=NULL&&
      get_event_timecode(block->end_event)>start_tc) return;


  // insert blank frames up to end_tc
  last_frame_event=get_last_frame_event(mt->event_list);
  mt->event_list=add_blank_frames_up_to(mt->event_list,last_frame_event,end_tc,mt->fps);

  block=get_block_before((GtkWidget *)mt->audio_draws->data,start_tc/U_SEC,TRUE);
  if (block!=NULL) shortcut=block->end_event;

  block=get_block_after((GtkWidget *)mt->audio_draws->data,start_tc/U_SEC,FALSE);

  // insert audio seek at tc
  frame_event=get_frame_event_at(mt->event_list,start_tc,shortcut,TRUE);

  if (direction==DIRECTION_POSITIVE) {
    insert_audio_event_at(mt->event_list,frame_event,-1,filenum,offset_start/U_SEC,avel);
  }
  else {
    insert_audio_event_at(mt->event_list,frame_event,-1,filenum,offset_end/U_SEC,avel);
    offset_start=offset_start-offset_end+offset_end*mt->insert_avel;
  }

  add_block_start_point ((GtkWidget *)mt->audio_draws->data, start_tc, filenum, offset_start, frame_event, TRUE);

  if (block==NULL||get_event_timecode(block->start_event)>end_tc) {
    // if no blocks after end point, insert audio off at end point
    frame_event=get_frame_event_at(mt->event_list,end_tc,frame_event,TRUE);
    insert_audio_event_at(mt->event_list,frame_event,-1,filenum,0.,0.);
    add_block_end_point ((GtkWidget *)mt->audio_draws->data, frame_event);
  }
  else add_block_end_point ((GtkWidget *)mt->audio_draws->data, block->start_event);

  end_secs=event_list_get_end_secs(mt->event_list);
  if (end_secs>mt->end_secs) {
    set_timeline_end_secs(mt,end_secs);
  }

  mt_sensitise(mt);
}



void multitrack_view_events (GtkMenuItem *menuitem, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  GtkWidget *elist_dialog;
  if ((prefs->event_window_show_frame_events&&count_events(mt->event_list,TRUE,0,0)>1000)||(!prefs->event_window_show_frame_events&&((count_events(mt->event_list,TRUE,0,0)-count_events(mt->event_list,FALSE,0,0))>1000))) if (!do_event_list_warning()) return;
  elist_dialog=create_event_list_dialog(mt->event_list,0,0);
  gtk_dialog_run(GTK_DIALOG(elist_dialog));
  gtk_widget_destroy(elist_dialog);
}


void multitrack_view_sel_events (GtkMenuItem *menuitem, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  GtkWidget *elist_dialog;

  weed_timecode_t tc_start=q_gint64(mt->region_start*U_SECL,mt->fps);
  weed_timecode_t tc_end=q_gint64(mt->region_end*U_SECL,mt->fps);

  if ((prefs->event_window_show_frame_events&&count_events(mt->event_list,TRUE,tc_start,tc_end)>1000)||(!prefs->event_window_show_frame_events&&((count_events(mt->event_list,TRUE,tc_start,tc_end)-count_events(mt->event_list,FALSE,tc_start,tc_end))>1000))) if (!do_event_list_warning()) return;
  elist_dialog=create_event_list_dialog(mt->event_list,tc_start,tc_end);
  gtk_dialog_run(GTK_DIALOG(elist_dialog));
  gtk_widget_destroy(elist_dialog);
}


////////////////////////////////////////////////////////
// region functions

void draw_region (lives_mt *mt) {
  gdouble start,end;
  if (mt->region_start==mt->region_end) return;


  if (mt->region_start<mt->region_end) {
    start=mt->region_start;
    end=mt->region_end;
  }
  else {
    start=mt->region_end;
    end=mt->region_start;
  }

  if (mt->region_start==mt->region_end) {
    gtk_widget_set_sensitive(mt->rs_to_tc,FALSE);
    gtk_widget_set_sensitive(mt->re_to_tc,FALSE);
  }
  else {
    gtk_widget_set_sensitive(mt->rs_to_tc,TRUE);
    gtk_widget_set_sensitive(mt->re_to_tc,TRUE);
  }

  gdk_draw_rectangle (GDK_DRAWABLE(mt->timeline_reg->window), mt->window->style->black_gc, TRUE, (start-mt->tl_min)*mt->timeline->allocation.width/(mt->tl_max-mt->tl_min), 0, (end-start)*mt->timeline->allocation.width/(mt->tl_max-mt->tl_min),mt->timeline_reg->allocation.height-2);
}

gint expose_timeline_reg_event (GtkWidget *timeline, GdkEventExpose *event, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  GList *tl_marks=mt->tl_marks;
  gint ebwidth;
  gdouble time;
  gint offset;

  if (event->count>0) return FALSE;
  if (mainw->playing_file>-1||mt->is_rendering) return FALSE;
  draw_region (mt);

  set_fg_colour(0,0,255);

  while (tl_marks!=NULL) {
    time=strtod((char *)tl_marks->data,NULL);
    ebwidth=GTK_WIDGET(mt->timeline)->allocation.width;
    offset=(time-mt->tl_min)/(mt->tl_max-mt->tl_min)*(gdouble)ebwidth;
    gdk_draw_line (GDK_DRAWABLE(mt->timeline_reg->window), mainw->general_gc, offset, 1, offset, 
		   mt->timeline_reg->allocation.height-2);
    tl_marks=tl_marks->next;
  }


  return FALSE;
}


static float get_float_audio_val_at_time(gint fnum, gdouble secs, gint chnum, gint chans) {
  file *afile=mainw->files[fnum];
  int64_t bytes=secs*afile->arate*afile->achans*afile->asampsize/8;
  int64_t apos=((int64_t)(bytes/afile->achans/(afile->asampsize/8)))*afile->achans*(afile->asampsize/8); // quantise
  gchar *filename;
  uint8_t val8;
  uint8_t val8b;
  uint16_t val16;
  float val;

  if (fnum!=aofile) {
    if (afd!=-1) close(afd);
    filename=g_build_filename(prefs->tmpdir,afile->handle,"audio",NULL);
    afd=open(filename,O_RDONLY);
    aofile=fnum;
  }

  if (afd==-1) {
    // deal with read errors after drawing a whole block
    mainw->read_failed=TRUE;
    return 0.;
  }

  apos+=afile->asampsize/8*chnum;

  lseek(afd,apos,SEEK_SET);

  if (afile->asampsize==8) {
    // 8 bit sample size
    lives_read(afd,&val8,1,FALSE);
    if (!(afile->signed_endian&AFORM_UNSIGNED)) val=val8>=128?val8-256:val8;
    else val=val8-127;
    val/=127.;
  }
  else {
    // 16 bit sample size
    lives_read(afd,&val8,1,FALSE);
    lives_read(afd,&val8b,1,FALSE);
    if (afile->signed_endian&AFORM_BIG_ENDIAN) val16=(uint16_t)(val8<<8)+val8b;
    else val16=(uint16_t)(val8b<<8)+val8;
    if (!(afile->signed_endian&AFORM_UNSIGNED)) val=val16>=32768?val16-65536:val16;
    else val=val16-32767;
    val/=32767.;
  }
  return val;
}



static void draw_soundwave(GtkWidget *ebox, gint start, gint width, gint chnum, lives_mt *mt) {
  weed_plant_t *event;
  weed_timecode_t tc;
  gdouble offset_startd,offset_endd; // time values
  gint offset_start,offset_end;  // pixel values
  gdouble tl_span=mt->tl_max-mt->tl_min;

  gdouble start_secs=(gdouble)start/ebox->allocation.width*tl_span+mt->tl_min;
  gdouble end_secs=(gdouble)(start+width)/ebox->allocation.width*tl_span+mt->tl_min;

  gdouble secs;
  int i;
  gdouble ypos;

  gint fnum;
  gdouble seek,vel;

  GtkWidget *eventbox=(GtkWidget *)g_object_get_data(G_OBJECT(ebox),"owner");
  gint track=GPOINTER_TO_INT(g_object_get_data(G_OBJECT(eventbox),"layer_number"));
  track_rect *block=(track_rect *)g_object_get_data (G_OBJECT(eventbox), "blocks");


  aofile=-1;
  afd=-1;

  mainw->read_failed=FALSE;

  while (block!=NULL) {
    event=block->start_event;
    tc=get_event_timecode(event);

    offset_startd=tc/U_SEC;
    if (offset_startd>end_secs) {
      if (afd!=-1) close(afd);
      return;
    }

    offset_start=(int)((offset_startd-mt->tl_min)/tl_span*ebox->allocation.width+.5);
    if ((start>0||width>0)&&offset_start>(start+width)) {
      if (afd!=-1) close(afd);
      return;
    }

    offset_endd=get_event_timecode(block->end_event)/U_SEC;
    offset_end=(offset_endd-mt->tl_min)/tl_span*ebox->allocation.width;

    if (offset_end<start_secs) {
      block=block->next;
      continue;
    }
  
    fnum=get_audio_frame_clip(block->start_event,track);
    seek=get_audio_frame_seek(block->start_event,track);
    vel=get_audio_frame_vel(block->start_event,track);

    gdk_draw_line(GDK_DRAWABLE(ebox->window),mt->window->style->fg_gc[0],offset_start,0,offset_start,
		  ebox->allocation.height);
    gdk_draw_line(GDK_DRAWABLE(ebox->window),mt->window->style->fg_gc[0],offset_start,0,offset_end,0);
    gdk_draw_line(GDK_DRAWABLE(ebox->window),mt->window->style->fg_gc[0],offset_end,0,offset_end,
		  ebox->allocation.height);
    gdk_draw_line(GDK_DRAWABLE(ebox->window),mt->window->style->fg_gc[0],offset_end,ebox->allocation.height-1,
		  offset_start,ebox->allocation.height-1);

    set_fg_colour(128,128,128); // mid grey
    for (i=offset_start;i<=offset_end;i++) {
      secs=((gdouble)i/ebox->allocation.width*tl_span+mt->tl_min-offset_startd)*vel;
      secs+=seek;
      ypos=ABS(get_float_audio_val_at_time(fnum,secs,chnum,cfile->achans));
      if (chnum==0)
	gdk_draw_line(GDK_DRAWABLE(ebox->window),mainw->general_gc,i,
		      ebox->allocation.height,i,(1.-ypos)*ebox->allocation.height);
      else if (chnum==1)
	gdk_draw_line(GDK_DRAWABLE(ebox->window),mainw->general_gc,i,
		      0,i,ypos*ebox->allocation.height);
      else
	gdk_draw_line(GDK_DRAWABLE(ebox->window),mainw->general_gc,i,
		      (1.-ypos)/2.*ebox->allocation.height,i,(1.+ypos)/2.*ebox->allocation.height);


    }
    block=block->next;

    if (mainw->read_failed) {
      gchar *filename=g_build_filename(prefs->tmpdir,mainw->files[fnum]->handle,"audio",NULL);
      do_read_failed_error_s(filename,NULL);
      g_free(filename);
    }
  }

  if (afd!=-1) close(afd);


}





gint mt_expose_laudtrack_event (GtkWidget *ebox, GdkEventExpose *event, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  GdkRegion *reg=event->region;
  GdkRectangle rect;
  GdkPixbuf *bgimage;

  gint startx,width;
  gint hidden;

  if (mt->no_expose) return TRUE;

  gdk_region_get_clipbox(reg,&rect);
  startx=rect.x;
  width=rect.width;

  if (event!=NULL&&event->count>0) {
    return TRUE;
  }

  hidden=(gint)GPOINTER_TO_INT(g_object_get_data (G_OBJECT(ebox), "hidden"));
  if (hidden!=0) {
    return FALSE;
  }

  if (width>ebox->allocation.width-startx) width=ebox->allocation.width-startx;

  if (GPOINTER_TO_INT(g_object_get_data (G_OBJECT(ebox), "drawn"))) {
    bgimage=(GdkPixbuf *)g_object_get_data (G_OBJECT(ebox), "bgimg");
    if (bgimage!=NULL&&lives_pixbuf_get_width(bgimage)>0) {
      cairo_t *cr = gdk_cairo_create (ebox->window);
      gdk_cairo_set_source_pixbuf (cr, bgimage, startx, 0);
      cairo_paint (cr);
      cairo_destroy (cr);
      return FALSE;
    }
  }

  gdk_window_clear_area (ebox->window, startx, 0, width, ebox->allocation.height);
  draw_soundwave(ebox,startx,width,0,mt);


#if GTK_CHECK_VERSION(3,0,0)
  {
    int xwidth,xheight;
    gdk_window_get_size(ebox->window,&xwidth,&xheight);
    if ((bgimage=gdk_pixbuf_get_from_window (ebox->window,
					     0,0,
					     xwidth,
					     xheight
					     ))!=NULL) {
#else
  bgimage=gdk_pixbuf_get_from_drawable(NULL,GDK_DRAWABLE(ebox->window),NULL,0,0,0,0,ebox->allocation.width,
				       ebox->allocation.height);
#endif
#if GTK_CHECK_VERSION(3,0,0)
    }
#endif

  if (lives_pixbuf_get_width(bgimage)>0) {
    g_object_set_data (G_OBJECT(ebox), "drawn",GINT_TO_POINTER(TRUE));
    g_object_set_data (G_OBJECT(ebox), "bgimg",bgimage);
  }

  return FALSE;
}


gint mt_expose_raudtrack_event (GtkWidget *ebox, GdkEventExpose *event, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  GdkRegion *reg=event->region;
  GdkRectangle rect;
  GdkPixbuf *bgimage;

  gint startx,width;
  gint hidden;

  if (mt->no_expose) return TRUE;

  gdk_region_get_clipbox(reg,&rect);
  startx=rect.x;
  width=rect.width;

  if (event!=NULL&&event->count>0) {
    return TRUE;
  }

  hidden=(gint)GPOINTER_TO_INT(g_object_get_data (G_OBJECT(ebox), "hidden"));
  if (hidden!=0) {
    return FALSE;
  }

  if (width>ebox->allocation.width-startx) width=ebox->allocation.width-startx;

  if (GPOINTER_TO_INT(g_object_get_data (G_OBJECT(ebox), "drawn"))) {
    bgimage=(GdkPixbuf *)g_object_get_data (G_OBJECT(ebox), "bgimg");
    if (bgimage!=NULL&&lives_pixbuf_get_width(bgimage)>0) {
      cairo_t *cr = gdk_cairo_create (ebox->window);
      gdk_cairo_set_source_pixbuf (cr, bgimage, startx, 0);
      cairo_paint (cr);
      cairo_destroy (cr);
      return FALSE;
    }
  }

  gdk_window_clear_area (ebox->window, startx, 0, width, ebox->allocation.height);
  draw_soundwave(ebox,startx,width,1,mt);

#if GTK_CHECK_VERSION(3,0,0)
  {
    int xwidth,xheight;
    gdk_window_get_size(ebox->window,&xwidth,&xheight);
    if ((bgimage=gdk_pixbuf_get_from_window (ebox->window,
					     0,0,
					     xwidth,
					     xheight
					     ))!=NULL) {
#else
  bgimage=gdk_pixbuf_get_from_drawable(NULL,GDK_DRAWABLE(ebox->window),NULL,0,0,0,0,ebox->allocation.width,
				       ebox->allocation.height);

#endif
#if GTK_CHECK_VERSION(3,0,0)
    }
#endif

  if (lives_pixbuf_get_width(bgimage)>0) {
    g_object_set_data (G_OBJECT(ebox), "drawn",GINT_TO_POINTER(TRUE));
    g_object_set_data (G_OBJECT(ebox), "bgimg",bgimage);
  }

  return FALSE;
}




////////////////////////////////////////////////////

// functions for moving and clicking on the timeline


gboolean
on_timeline_update (GtkWidget *widget, GdkEventMotion *event, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  gint x;
  gdouble pos;

  if (mainw->playing_file>-1) return TRUE;

  gdk_window_get_pointer(GDK_WINDOW (widget->window), &x, NULL, NULL);
  pos=get_time_from_x(mt,x);

  if (!mt->region_updating) {
    if (mt->tl_mouse) {
      mt->fm_edit_event=NULL;
      mt_tl_move(mt,pos-GTK_RULER (mt->timeline)->position);
    }
    return TRUE;
  }

  if (pos>mt->region_init) {
    mt->region_start=mt->region_init;
    mt->region_end=pos;
  }
  else {
    mt->region_start=pos;
    mt->region_end=mt->region_init;
  }

  gtk_spin_button_set_value(GTK_SPIN_BUTTON(mt->spinbutton_start),mt->region_start);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(mt->spinbutton_end),mt->region_end);

  if (mt->opts.mouse_mode==MOUSE_MODE_SELECT&&mt->tl_selecting&&event!=NULL) mouse_select_move(widget,mt);

  return TRUE;
}



gboolean all_present (weed_plant_t *event, GList *sel) {
  int *clips;
  int *frames;
  int error,numclips,layer;
  // see if we have an actual clip/frame for each layer in sel
  if (event==NULL||sel==NULL) return FALSE;

  numclips=weed_leaf_num_elements(event,"clips");

  if (!numclips) return FALSE;

  clips=weed_get_int_array(event,"clips",&error);
  frames=weed_get_int_array(event,"frames",&error);

  while (sel!=NULL) {
    layer=GPOINTER_TO_INT(sel->data);
    if (layer>=numclips||clips[layer]<1||frames[layer]<1) {
      weed_free(clips);
      weed_free(frames);
      return FALSE;
    }
    sel=sel->next;
  }
  weed_free(clips);
  weed_free(frames);
  return TRUE;
}


void get_region_overlap(lives_mt *mt) {
  // get region which overlaps all selected tracks
  weed_plant_t *event;
  weed_timecode_t tc;

  if (mt->selected_tracks==NULL||mt->event_list==NULL) {
    mt->region_start=mt->region_end=0.;
    return;
  }

  tc=q_gint64(mt->region_start*U_SECL,mt->fps);
  event=get_frame_event_at(mt->event_list,tc,NULL,TRUE);

  while (all_present(event,mt->selected_tracks)) {
    // move start to left
    event=get_prev_frame_event(event);
  }

  if (event==NULL) {
    event=get_first_event(mt->event_list);
    if (!WEED_EVENT_IS_FRAME(event)) event=get_next_frame_event(event);
  }

  while (event!=NULL&&!all_present(event,mt->selected_tracks)) {
    event=get_next_frame_event(event);
  }

  if (event==NULL) mt->region_start=0.;
  else mt->region_start=get_event_timecode(event)/U_SEC;

  tc=q_gint64(mt->region_end*U_SECL,mt->fps);
  event=get_frame_event_at(mt->event_list,tc,NULL,TRUE);

  while (all_present(event,mt->selected_tracks)) {
    // move end to right
    event=get_next_frame_event(event);
  }

  if (event==NULL) {
     event=get_last_event(mt->event_list);
    if (!WEED_EVENT_IS_FRAME(event)) event=get_prev_frame_event(event);
  }
 
  while (event!=NULL&&!all_present(event,mt->selected_tracks)) {
    event=get_prev_frame_event(event);
  }

  if (event==NULL) mt->region_end=0.;
  mt->region_end=get_event_timecode(event)/U_SEC+1./mt->fps;

  if (mt->event_list!=NULL&&get_first_event(mt->event_list)!=NULL) {
    gtk_widget_set_sensitive(mt->view_sel_events,mt->region_start!=mt->region_end);
  }

}


void do_sel_context (lives_mt *mt) {
  gchar *msg;
  if (mt->region_start==mt->region_end||mt->did_backup) return;
  clear_context(mt);
  msg=g_strdup_printf(_("Time region %.3f to %.3f\nselected.\n"),mt->region_start,mt->region_end);
  add_context_label(mt,msg);
  g_free(msg);
  if (mt->selected_tracks==NULL) {
    msg=g_strdup_printf(_("select one or more tracks\nto create a region.\n"),g_list_length(mt->selected_tracks));
  }
  else msg=g_strdup_printf(_("%d video tracks selected.\n"),g_list_length(mt->selected_tracks));
  add_context_label (mt,msg);
  add_context_label (mt,_("Double click on timeline\nto deselect time region."));
  g_free(msg);
}


void do_fx_list_context (lives_mt *mt, gint fxcount) {
  clear_context(mt);
  add_context_label (mt,(_("Single click on an effect\nto select it.")));
  add_context_label (mt,(_("Double click on an effect\nto edit it.")));
  add_context_label (mt,(_("Right click on an effect\nfor context menu.\n")));
  if (fxcount>1) {
    add_context_label (mt,(_("Effect order can be changed at\nFILTER MAPS")));
  }
}


void do_fx_move_context(lives_mt *mt) {
  clear_context(mt);
  add_context_label (mt,(_("You can select an effect,\nthen use the INSERT BEFORE")));
  add_context_label (mt,(_("or INSERT AFTER buttons to move it.")));
}



gboolean
on_timeline_release (GtkWidget *eventbox, GdkEventButton *event, gpointer user_data) {
  //button release
  lives_mt *mt=(lives_mt *)user_data;
  gdouble pos=mt->region_end;

  if (mainw->playing_file>-1) return FALSE;

  mt->tl_mouse=FALSE;

  if (eventbox!=mt->timeline_reg) {
    return FALSE;
  }

  if (event!=NULL) mt->region_updating=FALSE;

  if (mt->region_start==mt->region_end&&eventbox==mt->timeline_reg) {
    mt->region_start=mt->region_end=0;
    gtk_widget_set_sensitive(mt->view_sel_events,FALSE);
    g_signal_handler_block(mt->spinbutton_start,mt->spin_start_func);
    g_signal_handler_block(mt->spinbutton_end,mt->spin_end_func);
    gtk_spin_button_set_range (GTK_SPIN_BUTTON (mt->spinbutton_start),0.,mt->end_secs);
    gtk_spin_button_set_range (GTK_SPIN_BUTTON (mt->spinbutton_end),0.,mt->end_secs+1./mt->fps);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (mt->spinbutton_start),0.);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (mt->spinbutton_end),0.);
    g_signal_handler_unblock(mt->spinbutton_start,mt->spin_start_func);
    g_signal_handler_unblock(mt->spinbutton_end,mt->spin_end_func);
    gtk_widget_queue_draw(mt->timeline_reg);
    gdk_window_process_updates(mt->timeline_reg->window,FALSE);
    draw_region(mt);
    clear_context(mt);
    add_context_label(mt,_("You can click and drag\nbelow the timeline"));
    add_context_label(mt,_("to select a time region.\n"));
  }

  if ((mt->region_end!=mt->region_start)&&eventbox==mt->timeline_reg) {
    if (mt->opts.snap_over) get_region_overlap(mt);
    if (mt->region_end<mt->region_start) {
      mt->region_start-=mt->region_end;
      mt->region_end+=mt->region_start;
      mt->region_start=mt->region_end-mt->region_start;
    }
    if (mt->region_end>mt->region_start&&mt->event_list!=NULL&&get_first_event(mt->event_list)!=NULL) {
      if (mt->selected_tracks!=NULL) {
	gtk_widget_set_sensitive (mt->fx_region, TRUE);
	gtk_widget_set_sensitive (mt->ins_gap_sel, TRUE);
	gtk_widget_set_sensitive (mt->remove_gaps, TRUE);
	gtk_widget_set_sensitive (mt->remove_first_gaps, TRUE);
      }
      else {
	gtk_widget_set_sensitive (mt->fx_region, FALSE);
      }
      gtk_widget_set_sensitive(mt->playsel,TRUE);
      gtk_widget_set_sensitive (mt->ins_gap_cur, TRUE);
      gtk_widget_set_sensitive(mt->view_sel_events,TRUE);
    }
    else {
      gtk_widget_set_sensitive (mt->playsel,FALSE);
      gtk_widget_set_sensitive (mt->fx_region, FALSE);
      gtk_widget_set_sensitive (mt->ins_gap_cur, FALSE);
      gtk_widget_set_sensitive (mt->ins_gap_sel, FALSE);
      gtk_widget_set_sensitive (mt->remove_gaps, FALSE);
      gtk_widget_set_sensitive (mt->remove_first_gaps, FALSE);
    }
    if (mt->region_start==mt->region_end) gtk_widget_queue_draw (mt->timeline);
  }
  else {
    if (eventbox!=mt->timeline_reg) mt_tl_move(mt,pos-GTK_RULER (mt->timeline)->position);
    gtk_widget_set_sensitive (mt->fx_region, FALSE);
    gtk_widget_set_sensitive (mt->ins_gap_cur, FALSE);
    gtk_widget_set_sensitive (mt->ins_gap_sel, FALSE);
    gtk_widget_set_sensitive (mt->playsel,FALSE);
    gtk_widget_set_sensitive (mt->remove_gaps, FALSE);
    gtk_widget_set_sensitive (mt->remove_first_gaps, FALSE);
    if (mt->init_event!=NULL&&mt->poly_state==POLY_PARAMS) gtk_spin_button_set_value(GTK_SPIN_BUTTON(mt->node_spinbutton),pos-get_event_timecode(mt->init_event)/U_SEC);
  }

  gtk_spin_button_set_value(GTK_SPIN_BUTTON(mt->spinbutton_start),mt->region_start);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(mt->spinbutton_end),mt->region_end);

  pos=GTK_RULER(mt->timeline)->position;
  if (pos>mt->region_end-1./mt->fps) gtk_widget_set_sensitive(mt->tc_to_rs,FALSE);
  else gtk_widget_set_sensitive(mt->tc_to_rs,TRUE);
  if (pos<mt->region_start+1./mt->fps) gtk_widget_set_sensitive(mt->tc_to_re,FALSE);
  else gtk_widget_set_sensitive(mt->tc_to_re,TRUE);

  if (mt->opts.mouse_mode==MOUSE_MODE_SELECT&&event!=NULL) mouse_select_end(eventbox,mt);

  gtk_widget_set_sensitive(mt->fx_region_1,FALSE);
  gtk_widget_set_sensitive(mt->fx_region_2,FALSE);
  
  if (mt->selected_tracks!=NULL&&mt->region_end!=mt->region_start) {
    switch (g_list_length(mt->selected_tracks)) {
    case 1:
      gtk_widget_set_sensitive(mt->fx_region_1,TRUE);
      break;
    case 2:
      gtk_widget_set_sensitive(mt->fx_region_2,TRUE);
      break;
    default:
      break;
    }
  }

  // update labels
  if (get_poly_state_from_page(mt)==POLY_TRANS||get_poly_state_from_page(mt)==POLY_COMP) {
    polymorph(mt,POLY_NONE);
    polymorph(mt,get_poly_state_from_page(mt));
  }

  return TRUE;
}


gboolean
on_timeline_press (GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  gint x;
  gdouble pos;

  if (mainw->playing_file>-1) return FALSE;

  gdk_window_get_pointer(GDK_WINDOW (widget->window), &x, NULL, NULL);
  pos=get_time_from_x(mt,x);
  if (widget==mt->timeline_reg) {
    mt->region_start=mt->region_end=mt->region_init=pos;
    gtk_widget_set_sensitive(mt->view_sel_events,FALSE);
    mt->region_updating=TRUE;
  }

  if (widget==mt->timeline_eb) {
    mt->fm_edit_event=NULL;
    mt_tl_move(mt,pos-GTK_RULER(mt->timeline)->position);
    mt->tl_mouse=TRUE;
  }

  if (mt->opts.mouse_mode==MOUSE_MODE_SELECT) mouse_select_start(widget,mt);

  return TRUE;
}


weed_plant_t *get_prev_fm (lives_mt *mt, gint current_track, weed_plant_t *event) {
  weed_plant_t *event2,*event3,*eventx;

  if (event==NULL) return NULL;

  eventx=get_filter_map_before(event,current_track,NULL);

  if (eventx==NULL) return NULL;

  if (get_event_timecode(eventx)==get_event_timecode(event)) {
    // start with a map different from current

    while (1) {
      event2=get_prev_event(eventx);

      if (event2==NULL) return NULL;

      event3=get_filter_map_before(event2,current_track,NULL);

      if (!compare_filter_maps(event3,eventx,current_track)) {
	event=event2=event3;
	break; // continue with event 3
      }
      eventx=event3;
    }
  }
  else {
    if ((event2=get_prev_frame_event(event))==NULL) return NULL;

    event2=get_filter_map_before(event2,current_track,NULL);
    
    if (event2==NULL) return NULL;
  }

  // now find the earliest which is the same
  while (1) {
    event=event2;

    event3=get_prev_event(event2);

    if (event3==NULL) break;

    event2=get_filter_map_before(event3,current_track,NULL);

    if (event2==NULL) break;

    if (!compare_filter_maps(event2,event,current_track)) break;

  }

  if (filter_map_after_frame(event)) return get_next_frame_event(event);

  return event;
}


weed_plant_t *get_next_fm (lives_mt *mt, gint current_track, weed_plant_t *event) {
  weed_plant_t *event2,*event3;

  if (event==NULL) return NULL;

  if ((event2=get_filter_map_after(event,current_track))==NULL) return event;

  event3=get_filter_map_before(event,-1000000,NULL);

  if (event3==NULL) return NULL;

  // find the first filter_map which differs from the current
  while (1) {

    if (!compare_filter_maps(event2,event3,current_track)) break;

    event=get_next_event(event2);

    if (event==NULL) return NULL;

    event3=event2;

    if ((event2=get_filter_map_after(event,current_track))==NULL) return NULL;

  }


  if (filter_map_after_frame(event2)) return get_next_frame_event(event2);

  return event2;
}



static void add_mark_at(lives_mt *mt, gdouble time) {
  gchar *tstring=g_strdup_printf("%.6f",time);
  gint offset;

  set_fg_colour(0,0,255);

  gtk_widget_set_sensitive(mt->clear_marks,TRUE);
  mt->tl_marks=g_list_append(mt->tl_marks,tstring);
  offset=(time-mt->tl_min)/(mt->tl_max-mt->tl_min)*(gdouble)mt->timeline->allocation.width;
  gdk_draw_line (GDK_DRAWABLE(mt->timeline_reg->window), mainw->general_gc, offset, 1, offset, mt->timeline_reg->allocation.height-2);
}



gboolean mt_mark_callback (GtkAccelGroup *group, GObject *obj, guint keyval, GdkModifierType mod, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  gdouble cur_time;

  if (mainw->playing_file==-1) return TRUE;

  cur_time=GTK_RULER (mt->timeline)->position;

  add_mark_at(mt, cur_time);
  return TRUE;
}


void on_fx_insa_clicked  (GtkWidget *button, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  mt->fx_order=FX_ORD_AFTER;
  gtk_widget_set_sensitive(mt->fx_ibefore_button,FALSE);
  gtk_widget_set_sensitive(mt->fx_iafter_button,FALSE);

  clear_context(mt);
  add_context_label (mt,(_("Click on another effect,")));
  add_context_label (mt,(_("and the selected one\nwill be inserted")));
  add_context_label (mt,(_("after it.\n")));

}

void on_fx_insb_clicked  (GtkWidget *button, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  mt->fx_order=FX_ORD_BEFORE;
  gtk_widget_set_sensitive(mt->fx_ibefore_button,FALSE);
  gtk_widget_set_sensitive(mt->fx_iafter_button,FALSE);

  clear_context(mt);
  add_context_label (mt,(_("Click on another effect,")));
  add_context_label (mt,(_("and the selected one\nwill be inserted")));
  add_context_label (mt,(_("before it.\n")));
}



void on_prev_fm_clicked  (GtkWidget *button, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  weed_timecode_t tc;
  gdouble secs=GTK_RULER(mt->timeline)->position;
  tc=q_gint64(secs*U_SEC,mt->fps);
  weed_plant_t *event;

  event=get_frame_event_at(mt->event_list,tc,mt->fm_edit_event,TRUE);

  event=get_prev_fm(mt,mt->current_track,event);

  if (event!=NULL) tc=get_event_timecode(event);

  mt_tl_move(mt,tc/U_SEC-GTK_RULER(mt->timeline)->position);
}


void on_next_fm_clicked  (GtkWidget *button, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  weed_timecode_t tc;
  weed_plant_t *event;
  gdouble secs=GTK_RULER(mt->timeline)->position;
  tc=q_gint64(secs*U_SEC,mt->fps);

  event=get_frame_event_at(mt->event_list,tc,mt->fm_edit_event,TRUE);

  event=get_next_fm(mt,mt->current_track,event);

  if (event!=NULL) tc=get_event_timecode(event);

  mt_tl_move(mt,tc/U_SEC-GTK_RULER(mt->timeline)->position);

}



static weed_timecode_t get_prev_node_tc(lives_mt *mt, weed_timecode_t tc) {
  int num_params=weed_leaf_num_elements(get_weed_filter(mt->current_fx),"in_parameter_templates");
  int i,error;
  weed_timecode_t prev_tc=-1;
  weed_plant_t *event;
  weed_timecode_t ev_tc;

  if (pchain==NULL) return tc;

  for (i=0;i<num_params;i++) {
    event=(weed_plant_t *)pchain[i];
    while (event!=NULL&&(ev_tc=get_event_timecode(event))<tc) {
      if (ev_tc>prev_tc) prev_tc=ev_tc;
      event=(weed_plant_t *)weed_get_voidptr_value(event,"next_change",&error);
    }
  }
  return prev_tc;
}


static weed_timecode_t get_next_node_tc(lives_mt *mt, weed_timecode_t tc) {
  int num_params=weed_leaf_num_elements(get_weed_filter(mt->current_fx),"in_parameter_templates");
  int i,error;
  weed_timecode_t next_tc=-1;
  weed_plant_t *event;
  weed_timecode_t ev_tc;

  if (pchain==NULL) return tc;

  for (i=0;i<num_params;i++) {
    event=(weed_plant_t *)pchain[i];
    while (event!=NULL&&(ev_tc=get_event_timecode(event))<=tc) 
      event=(weed_plant_t *)weed_get_voidptr_value(event,"next_change",&error);
    if (event!=NULL) {
      if (next_tc==-1||ev_tc<next_tc) next_tc=ev_tc;
    }
  }
  return next_tc;
}


static gboolean is_node_tc(lives_mt *mt, weed_timecode_t tc) {
  int num_params=weed_leaf_num_elements(get_weed_filter(mt->current_fx),"in_parameter_templates");
  int i,error;
  weed_plant_t *event;
  weed_timecode_t ev_tc;

  for (i=0;i<num_params;i++) {
    event=(weed_plant_t *)pchain[i];
    ev_tc=-1;
    while (event!=NULL&&(ev_tc=get_event_timecode(event))<tc) event=(weed_plant_t *)weed_get_voidptr_value(event,"next_change",&error);
    if (ev_tc==tc) return TRUE;
  }
  return FALSE;
}



// apply the param changes and update widgets
void
on_node_spin_value_changed           (GtkSpinButton   *spinbutton,
				      gpointer         user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  weed_timecode_t init_tc=get_event_timecode(mt->init_event);
  weed_timecode_t otc=gtk_spin_button_get_value(spinbutton)*U_SEC+init_tc;
  weed_timecode_t tc=q_gint64(otc,mt->fps);
  gdouble timesecs;
  gboolean auto_prev=mt->opts.fx_auto_preview;

  g_object_freeze_notify(G_OBJECT(spinbutton));

  if (!mt->block_tl_move) {
    timesecs=otc/U_SEC;
    mt->block_node_spin=TRUE;
    mt_tl_move(mt,timesecs-GTK_RULER (mt->timeline)->position);
    mt->block_node_spin=FALSE;
  }

  if (mt->prev_fx_time==0.||tc==init_tc) {
    add_mt_param_box(mt); // sensitise/desensitise reinit params
  }
  else mt->prev_fx_time=mt_get_effect_time(mt);

  interpolate_params((weed_plant_t *)mt->current_rfx->source,pchain,tc);
  
  set_params_unchanged(mt->current_rfx);

  get_track_index(mt,tc);

  mt->opts.fx_auto_preview=FALSE; // we will preview anyway later, so don't do it twice

  mainw->block_param_updates=TRUE;
  update_visual_params(mt->current_rfx,TRUE);
  mainw->block_param_updates=FALSE;
  
  mt->opts.fx_auto_preview=auto_prev;

  if (get_prev_node_tc(mt,tc)>-1) gtk_widget_set_sensitive(mt->prev_node_button,TRUE);
  else gtk_widget_set_sensitive(mt->prev_node_button,FALSE);

  if (get_next_node_tc(mt,tc)>-1) gtk_widget_set_sensitive(mt->next_node_button,TRUE);
  else gtk_widget_set_sensitive(mt->next_node_button,FALSE);

  if (is_node_tc(mt,tc)) {
    gtk_widget_set_sensitive(mt->del_node_button,TRUE);
    gtk_widget_set_sensitive(mt->apply_fx_button,FALSE);
  }
  else gtk_widget_set_sensitive(mt->del_node_button,FALSE);

  if (mt->current_track>=0) {
    if (mt->opts.fx_auto_preview||mainw->play_window!=NULL) mt_show_current_frame(mt, FALSE);
  }

  g_object_thaw_notify(G_OBJECT(spinbutton));
}

// node buttons
void on_next_node_clicked  (GtkWidget *button, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  weed_timecode_t init_tc=get_event_timecode(mt->init_event);
  weed_timecode_t tc=q_gint64(gtk_spin_button_get_value(GTK_SPIN_BUTTON(mt->node_spinbutton))*U_SEC+init_tc,mt->fps);
  weed_timecode_t next_tc=get_next_node_tc(mt,tc);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(mt->node_spinbutton),(next_tc-init_tc)/U_SEC);
  if (mt->current_track>=0) mt_show_current_frame(mt, FALSE);
  gtk_widget_set_sensitive(mt->apply_fx_button,FALSE);
}


void on_prev_node_clicked  (GtkWidget *button, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  weed_timecode_t init_tc=get_event_timecode(mt->init_event);
  weed_timecode_t tc=q_gint64(gtk_spin_button_get_value(GTK_SPIN_BUTTON(mt->node_spinbutton))*U_SEC+init_tc,mt->fps);
  weed_timecode_t prev_tc=get_prev_node_tc(mt,tc);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(mt->node_spinbutton),(prev_tc-init_tc)/U_SEC);
  if (mt->current_track>=0) mt_show_current_frame(mt, FALSE);
  gtk_widget_set_sensitive(mt->apply_fx_button,FALSE);
}


void on_del_node_clicked  (GtkWidget *button, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  int i,error;
  weed_plant_t *event;
  weed_timecode_t ev_tc;
  weed_timecode_t tc=q_gint64(gtk_spin_button_get_value(GTK_SPIN_BUTTON(mt->node_spinbutton))*U_SEC+get_event_timecode(mt->init_event),mt->fps);
  weed_plant_t *prev_pchange,*next_pchange;
  gchar *filter_name,*text;
  int num_params=weed_leaf_num_elements((weed_plant_t *)mt->current_rfx->source,"in_parameters");
  weed_plant_t **in_params=weed_get_plantptr_array((weed_plant_t *)mt->current_rfx->source,"in_parameters",&error);

  for (i=0;i<num_params;i++) {
    event=(weed_plant_t *)pchain[i];
    ev_tc=-1;
    while (event!=NULL&&(ev_tc=get_event_timecode(event))<tc) 
      event=(weed_plant_t *)weed_get_voidptr_value(event,"next_change",&error);
    if (ev_tc==tc) {
      prev_pchange=(weed_plant_t *)weed_get_voidptr_value(event,"prev_change",&error);
      next_pchange=(weed_plant_t *)weed_get_voidptr_value(event,"next_change",&error);
      if (event!=pchain[i]) {
	delete_event(mt->event_list,event);
	if (prev_pchange!=NULL) weed_set_voidptr_value(prev_pchange,"next_change",next_pchange);
	if (next_pchange!=NULL) weed_set_voidptr_value(next_pchange,"prev_change",prev_pchange);
      }
      else {
	// is initial pchange, reset to defaults, c.f. paramspecial.c
	weed_plant_t *param=in_params[i];
	weed_plant_t *paramtmpl=weed_get_plantptr_value(param,"template",&error);
	if (weed_plant_has_leaf(paramtmpl,"host_default")) {
	  weed_leaf_copy(event,"value",paramtmpl,"host_default");
	}
	else weed_leaf_copy(event,"value",paramtmpl,"default");
	if (is_perchannel_multiw(param)) {
	  int num_in_tracks=weed_leaf_num_elements(mt->init_event,"in_tracks");
	  int param_hint=weed_get_int_value(paramtmpl,"hint",&error);
	  fill_param_vals_to(paramtmpl,event,i,param_hint,num_in_tracks-1);
	}
      }
    }
  }
  weed_free(in_params);

  if (mt->current_fx==mt->avol_fx&&mt->avol_init_event!=NULL&&mt->aparam_view_list!=NULL) {
    GList *slist=mt->audio_draws;
    while (slist!=NULL) {
      gtk_widget_queue_draw((GtkWidget *)slist->data);
      slist=slist->next;
    }
  }

  filter_name=weed_filter_get_name(mt->current_fx);

  text=g_strdup_printf(_("Removed parameter values for effect %s at time %.4f\n"),filter_name,tc);
  d_print(text);
  g_free(text);
  g_free(filter_name);
  mt->block_tl_move=TRUE;
  on_node_spin_value_changed(GTK_SPIN_BUTTON(mt->node_spinbutton),(gpointer)mt);
  mt->block_tl_move=FALSE;
  gtk_widget_set_sensitive(mt->del_node_button,FALSE);
  if (mt->current_track>=0) {
    mt_show_current_frame(mt, FALSE);
  }
  gtk_widget_set_sensitive(mt->apply_fx_button,FALSE);

  mt->changed=mt->auto_changed=TRUE;

}


void mt_fixup_events(lives_mt * mt, weed_plant_t *old_event, weed_plant_t *new_event) {
  // if any "notable" events have changed, we should repoint them here

  if (mt==NULL) return;

  if (mt->fm_edit_event==old_event) mt->fm_edit_event=new_event;
  if (mt->init_event==old_event) mt->init_event=new_event;
  if (mt->selected_init_event==old_event) {
    mt->selected_init_event=new_event;
  }
  if (mt->avol_init_event==old_event) mt->avol_init_event=new_event;
  if (mt->specific_event==old_event) mt->specific_event=new_event;
}



static void combine_ign (weed_plant_t *xnew, weed_plant_t *xold) {
  int num,numo,*nign,*oign,i,error;

  // combine "ignore" values using NAND
  if (!weed_plant_has_leaf(xold,"ignore")) return;
  num=weed_leaf_num_elements(xnew,"ignore");
  numo=weed_leaf_num_elements(xnew,"ignore");
  oign=weed_get_boolean_array(xold,"ignore",&error);
  nign=weed_get_boolean_array(xnew,"ignore",&error);
  for (i=0;i<num;i++) if (i>=numo||oign[i]==WEED_FALSE) nign[i]=WEED_FALSE;
  weed_set_boolean_array(xnew,"ignore",num,nign);
  weed_free(oign);
  weed_free(nign);
}



static void add_to_pchain(weed_plant_t *event_list, weed_plant_t *init_event, weed_plant_t *pchange, int index, weed_timecode_t tc) {
  weed_plant_t *event=(weed_plant_t *)pchain[index];
  weed_plant_t *last_event=NULL;
  int error;

  while (event!=NULL&&get_event_timecode(event)<tc) {
    last_event=event;
    event=(weed_plant_t *)weed_get_voidptr_value(event,"next_change",&error);
  }

  if (event!=NULL&&get_event_timecode(event)==tc) {
    // replace an existing change
    weed_plant_t *next_event=(weed_plant_t *)weed_get_voidptr_value(event,"next_change",&error);
    if (next_event!=NULL) weed_set_voidptr_value(next_event,"prev_change",pchange);
    weed_set_voidptr_value(pchange,"next_change",next_event);
    if (event==pchain[index]) weed_leaf_delete(pchange,"ignore"); // never ignore our init pchanges
    if (weed_plant_has_leaf(pchange,"ignore")) combine_ign(pchange,event);
    delete_event(event_list,event);
  }
  else {
    weed_set_voidptr_value(pchange,"next_change",event);
    if (event!=NULL) weed_set_voidptr_value(event,"prev_change",pchange);
  }

  if (last_event!=NULL) weed_set_voidptr_value(last_event,"next_change",pchange);
  else {
    // update "in_params" for init_event
    int numin=weed_leaf_num_elements(init_event,"in_parameters");
    void **in_params=weed_get_voidptr_array(init_event,"in_parameters",&error);
    in_params[index]=pchain[index]=(void *)pchange;
    weed_set_voidptr_array(init_event,"in_parameters",numin,in_params);
    weed_free(in_params);
  }
  weed_set_voidptr_value(pchange,"prev_change",last_event);
}


void activate_mt_preview(lives_mt *mt) {
  // called from paramwindow.c when a parameter changes - show effect with currently unapplied values
  if (mt->poly_state==POLY_PARAMS) {
    if (mt->opts.fx_auto_preview) {
      mainw->no_interp=TRUE; // no interpolation - parameter is in an uncommited state
      mt_show_current_frame(mt, FALSE);
      mainw->no_interp=FALSE;
    }
    if (mt->apply_fx_button!=NULL) gtk_widget_set_sensitive(mt->apply_fx_button,TRUE);
  }
  else mt_show_current_frame(mt, FALSE);
}


void on_set_pvals_clicked  (GtkWidget *button, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  weed_plant_t *inst=(weed_plant_t *)mt->current_rfx->source;
  weed_plant_t *param,*pchange,*at_event;
  gchar *filter_name,*text;
  gchar *tname,*track_desc;
  int i,error;
  weed_timecode_t tc=q_gint64(gtk_spin_button_get_value(GTK_SPIN_BUTTON(mt->node_spinbutton))*U_SEC+get_event_timecode(mt->init_event),mt->fps);
  int numtracks;
  int *tracks;
  int *ign;
  gboolean has_multi=FALSE;
  gboolean was_changed=FALSE;
  gchar *tmp,*tmp2;

  if (mt->idlefunc>0) {
    g_source_remove(mt->idlefunc);
    mt->idlefunc=0;
  }

  gtk_widget_set_sensitive(mt->apply_fx_button,FALSE);

  for (i=0;((param=weed_inst_in_param(inst,i,FALSE))!=NULL);i++) {
    if (!mt->current_rfx->params[i].changed) continue; // set only user changed parameters
    pchange=weed_plant_new(WEED_PLANT_EVENT);
    weed_set_int_value(pchange,"hint",WEED_EVENT_HINT_PARAM_CHANGE);
    weed_set_int64_value(pchange,"timecode",tc);
    weed_set_voidptr_value(pchange,"init_event",mt->init_event);
    weed_set_int_value(pchange,"index",i);
    if (is_perchannel_multiw(param)) {
      int num_vals,j;
      if (mt->track_index==-1) {
	weed_plant_free(pchange);
	continue;
      }
      has_multi=TRUE;
      num_vals=weed_leaf_num_elements(param,"value");
      ign=(int *)g_malloc(num_vals*sizint);
      for (j=0;j<num_vals;j++) {
	if (j==mt->track_index) {
	  ign[j]=WEED_FALSE;
	  was_changed=TRUE;
	}
	else ign[j]=WEED_TRUE;
      }
      weed_set_boolean_array(pchange,"ignore",num_vals,ign);
      g_free(ign);
    }
    else was_changed=TRUE;

    weed_leaf_copy(pchange,"value",param,"value");
    weed_add_plant_flags(pchange,WEED_LEAF_READONLY_PLUGIN);

    // set next_change, prev_change
    add_to_pchain(mt->event_list,mt->init_event,pchange,i,tc);

    at_event=get_frame_event_at(mt->event_list,tc,mt->init_event,TRUE);
    insert_param_change_event_at(mt->event_list,at_event,pchange);
  }
  if (!was_changed) {
    mt->idlefunc=mt_idle_add(mt);
    return;
  }

  filter_name=weed_filter_get_name(mt->current_fx);
  tracks=weed_get_int_array(mt->init_event,"in_tracks",&error);
  numtracks=enabled_in_channels(get_weed_filter(mt->current_fx),TRUE); // count repeated channels

  switch (numtracks) {
  case 1:
    tname=lives_fx_cat_to_text(LIVES_FX_CAT_EFFECT,FALSE); // effect
    track_desc=g_strdup_printf(_("track %s"),(tmp=get_track_name(mt,tracks[0],FALSE)));
    g_free(tmp);
    break;
  case 2:
    tname=lives_fx_cat_to_text(LIVES_FX_CAT_TRANSITION,FALSE); // transition
    track_desc=g_strdup_printf(_("tracks %s and %s"),(tmp=get_track_name(mt,tracks[0],FALSE)),(tmp2=get_track_name(mt,tracks[1],FALSE)));
    g_free(tmp);
    g_free(tmp2);
    break;
  default:
    tname=lives_fx_cat_to_text(LIVES_FX_CAT_COMPOSITOR,FALSE); // compositor
    if (has_multi) {
      track_desc=g_strdup_printf(_("track %s"),(tmp=get_track_name(mt,mt->current_track,mt->aud_track_selected)));
      g_free(tmp);
    }
    else track_desc=g_strdup(_("selected tracks"));
    break;
  }
  weed_free(tracks);
  if (mt->current_fx==mt->avol_fx) {
    g_free(tname);
    tname=g_strdup(_("audio"));
  }
  text=g_strdup_printf(_("Set parameter values for %s %s on %s at time %.4f\n"),tname,filter_name,track_desc,tc/U_SEC);
  d_print(text);
  g_free(text);
  g_free(filter_name);
  g_free(tname);
  g_free(track_desc);

  gtk_widget_set_sensitive(mt->del_node_button,TRUE);

  if (mt->current_fx==mt->avol_fx&&mt->avol_init_event!=NULL&&mt->aparam_view_list!=NULL) {
    GList *slist=mt->audio_draws;
    while (slist!=NULL) {
      gtk_widget_queue_draw((GtkWidget *)slist->data);
      slist=slist->next;
    }
  }

  if (mt->current_track>=0) {
    mt_show_current_frame(mt, FALSE); // show full preview in play window
  }

  mt->changed=mt->auto_changed=TRUE;
  mt->idlefunc=mt_idle_add(mt);

}


//////////////////////////////////////////////////////////////////////////
static int free_tt_key;
static int elist_errors;



GList *load_layout_map(void) {
  // load in a layout "map" for the set, [create mainw->current_layouts_map]

  // the layout.map file maps clip "unique_id" and "handle" stored in the header.lives file and matches it with
  // the track numbers in the any layout file (.lay) file for that set

  // [thus a layout could be transferred to another set and the unique_id's/handles altered, 
  // one could use a layout.map and a layout file as a template for
  // rendering many different sets]


  // this is called from recover_layout_map() in saveplay.c, where the map entries are assigned
  // to files (clips)

  gchar **array;
  GList *lmap=NULL;
  layout_map *lmap_entry;
  guint64 unique_id;
  ssize_t bytes;

  gchar *lmap_name=g_build_filename(prefs->tmpdir,mainw->set_name,"layouts","layout.map",NULL);
  gchar *handle;
  gchar *entry;
  gchar *string;
  gchar *name;

  int len,nm,i;
  int fd;
  int retval;

  gboolean err=FALSE;

  if (!g_file_test(lmap_name,G_FILE_TEST_EXISTS)) {
    g_free(lmap_name);
    return NULL;
  }

  do { 
    retval=0;
    fd=open(lmap_name,O_RDONLY);
    if (fd<0) {
      retval=do_read_failed_error_s_with_retry(lmap_name,NULL,NULL);
    }
    else {
      while (1) {
	bytes=lives_read_le(fd,&len,4,TRUE);
	if (bytes<4) {
	  break;
	}
	handle=(gchar *)g_malloc(len+1);
	bytes=read(fd,handle,len);
	if (bytes<len) {
	  break;
	}
	memset(handle+len,0,1);
	bytes=lives_read_le(fd,&unique_id,8,TRUE);
	if (bytes<8) {
	  break;
	}
	bytes=lives_read_le(fd,&len,4,TRUE);
	if (bytes<4) {
	  break;
	}
	name=(gchar *)g_malloc(len+1);
	bytes=read(fd,name,len);
	if (bytes<len) {
	  break;
	}
	memset(name+len,0,1);
	bytes=lives_read_le(fd,&nm,4,TRUE);
	if (bytes<4) {
	  break;
	}
	
	lmap_entry=(layout_map *)g_malloc(sizeof(layout_map));
	lmap_entry->handle=handle;
	lmap_entry->unique_id=unique_id;
	lmap_entry->name=name;
	lmap_entry->list=NULL;
	
	for (i=0;i<nm;i++) {
	  bytes=lives_read_le(fd,&len,4,TRUE);
	  if (bytes<sizint) {
	    err=TRUE;
	    break;
	  }
	  entry=(gchar *)g_malloc(len+1);
	  bytes=read(fd,entry,len);
	  if (bytes<len) {
	    err=TRUE;
	    break;
	  }
	  memset(entry+len,0,1);
	  string=repl_tmpdir(entry,FALSE); // allow relocation of tmpdir
	  lmap_entry->list=g_list_append(lmap_entry->list,g_strdup(string));
	  array=g_strsplit(string,"|",-1);
	  g_free(string);
	  mainw->current_layouts_map=g_list_append_unique(mainw->current_layouts_map,array[0]);
	  g_strfreev(array);
	  g_free(entry);
	}
	if (err) break;
	lmap=g_list_append(lmap,lmap_entry);
      }
    }

    if (fd>=0) close(fd);

    if (err) {
      retval=do_read_failed_error_s_with_retry(lmap_name,NULL,NULL);
    }
  } while (retval==LIVES_RETRY);

  g_free(lmap_name);
  return lmap;
}




void save_layout_map (int *lmap, double *lmap_audio, const gchar *file, const gchar *dir) {
  // in the file "layout.map", we map each clip used in the set to which layouts (if any) it is used in
  // we also record the highest frame number used and the max audio time; and the current fps of the clip
  // and audio rate;

  // one entry per layout file, per clip

  // map format in memory is:

  // this was knocked together very hastily, so it could probably be improved upon


  // layout_file_file_name|clip_number|max_frame_used|clip fps|max audio time|audio rate


  // when we save this to a file, we use the (int32)data_length data
  // convention
  // and the format is:
  // 4 bytes handle len
  // n bytes handle
  // 8 bytes unique_id
  // 4 bytes file name len
  // n bytes file name
  // 4 bytes data len
  // n bytes data

  // where data is simply a text dump of the above memory format

  GList *map,*map_next;

  gchar *new_entry;
  gchar *map_name,*ldir;
  gchar *string;
  gchar *com;

  int i;
  int fd;
  int len;
  int retval;
  gint max_frame;
  gboolean written=FALSE;

  guint size=0;

  gdouble max_atime;

  if (dir==NULL&&strlen(mainw->set_name)==0) return;

  // TODO - dirsep

  if (file!=NULL&&(mainw->current_layouts_map==NULL||
		   !g_list_find(mainw->current_layouts_map,file))) 
    mainw->current_layouts_map=g_list_append(mainw->current_layouts_map,g_strdup(file));
  if (dir==NULL) ldir=g_build_filename(prefs->tmpdir,mainw->set_name,"layouts/",NULL);
  else ldir=g_strdup(dir);

  map_name=g_build_filename(ldir,"layout.map",NULL);

  com=g_strdup_printf("/bin/mkdir -p \"%s\" 2>/dev/null",ldir);
  lives_system(com,FALSE);
  g_free(com);

  do {
    retval=0;
    fd=creat(map_name,DEF_FILE_PERMS);

    if (fd==-1) {
      retval=do_write_failed_error_s_with_retry(map_name,g_strerror(errno),NULL);
    }
    else {
      mainw->write_failed=FALSE;

      for (i=1;i<=MAX_FILES;i++) {
	// add or update
	if (mainw->files[i]!=NULL) {
	  
	  if (mainw->files[i]->layout_map!=NULL) {
	    map=mainw->files[i]->layout_map;
	    while (map!=NULL) {
	      map_next=map->next;
	      if (map->data!=NULL) {
		gchar **array=g_strsplit((gchar *)map->data,"|",-1);
		if ((file!=NULL&&!strcmp(array[0],file))||(file==NULL&&dir==NULL&&
							   !g_file_test(array[0],G_FILE_TEST_EXISTS))) {
		  // remove prior entry
		  g_free(map->data);
		  if (map->prev!=NULL) map->prev->next=map_next;
		  else mainw->files[i]->layout_map=map_next;
		  if (map_next!=NULL) map_next->prev=map->prev;
		  map->next=map->prev=NULL;
		  g_list_free(map);
		  break;
		}
		g_strfreev(array);
	      }
	      map=map_next;
	    }
	  }
	  
	  if (file!=NULL&&((lmap!=NULL&&lmap[i]!=0)||(lmap_audio!=NULL&&lmap_audio[i]!=0.))) {
	    if (lmap!=NULL) max_frame=lmap[i];
	    else max_frame=0;
	    if (lmap_audio!=NULL) max_atime=lmap_audio[i];
	    else max_atime=0.;
	    
	    new_entry=g_strdup_printf("%s|%d|%d|%.8f|%.8f|%.8f",file,i,max_frame,mainw->files[i]->fps,
				      max_atime,(gdouble)((gint)((gdouble)(mainw->files[i]->arps)/
								 (gdouble)mainw->files[i]->arate*10000.+.5))/10000.);
	    mainw->files[i]->layout_map=g_list_prepend(mainw->files[i]->layout_map,new_entry);
	  }

	  if ((map=mainw->files[i]->layout_map)!=NULL) {
	    written=TRUE;
	    len=strlen(mainw->files[i]->handle);
	    lives_write_le(fd,&len,4,TRUE);
	    lives_write(fd,mainw->files[i]->handle,len,TRUE);
	    lives_write_le(fd,&mainw->files[i]->unique_id,8,TRUE);
	    len=strlen(mainw->files[i]->name);
	    lives_write_le(fd,&len,4,TRUE);
	    lives_write(fd,mainw->files[i]->name,len,TRUE);
	    len=g_list_length(map);
	    lives_write_le(fd,&len,4,TRUE);
	    while (map!=NULL) {
	      string=repl_tmpdir((char *)map->data,TRUE); // allow relocation of tmpdir
	      len=strlen(string);
	      lives_write_le(fd,&len,4,TRUE);
	      lives_write(fd,string,len,TRUE);
	      g_free(string);
	      map=map->next;
	    }
	  }
	}
	if (mainw->write_failed) break;
      }
      if (mainw->write_failed) {
	retval=do_write_failed_error_s_with_retry(map_name,NULL,NULL);
	mainw->write_failed=FALSE;
      }

    }
    if (retval==LIVES_RETRY && fd>=0) close(fd);
  } while (retval==LIVES_RETRY);

  if (retval!=LIVES_CANCEL) {
    size=get_file_size(fd);
    close(fd);

    if (size==0||!written) {
      LIVES_DEBUG("Removing layout map file: ");
      LIVES_DEBUG(map_name);
      unlink(map_name);
    }

    LIVES_DEBUG("Removing layout dir: ");
    LIVES_DEBUG(ldir);
    com=g_strdup_printf("/bin/rmdir \"%s\" 2>/dev/null",ldir);
    lives_system(com,TRUE);
    g_free(com);
  }

  g_free(ldir);
  g_free(map_name);
}





void add_markers(lives_mt *mt, weed_plant_t *event_list) {
  // add "block_start" and "block_unordered" markers to a timeline
  // this is done when we save an event_list (layout file).
  // these markers are removed when the event_list is loaded and displayed

  // other hosts are not bound to take notice of "marker" events, so these could be absent or misplaced
  // when the layout is reloaded

  GList *track_blocks=NULL;
  GList *tlist=mt->video_draws;
  GList *blist;
  track_rect *block;
  weed_timecode_t tc;
  GtkWidget *eventbox;
  weed_plant_t *event;
  gint track;

  while (tlist!=NULL) {
    eventbox=(GtkWidget *)tlist->data;
    block=(track_rect *)g_object_get_data (G_OBJECT(eventbox), "blocks");
    track_blocks=g_list_append(track_blocks,(gpointer)block);
    tlist=tlist->next;
  }

  event=get_first_event(event_list);

  while (event!=NULL) {
    if (WEED_EVENT_IS_FRAME(event)) {
      tc=get_event_timecode(event);
      blist=track_blocks;
      track=0;
      while (blist!=NULL) {
	block=(track_rect *)blist->data;
	if (block!=NULL) {
	  if (block->prev!=NULL&&(get_event_timecode(block->prev->end_event)==
				  q_gint64(tc-U_SEC/mt->fps,mt->fps))&&(tc==get_event_timecode(block->start_event))&&
	      (get_frame_event_clip(block->prev->end_event,track)==get_frame_event_clip(block->start_event,track))) {
	    insert_marker_event_at(mt->event_list,event,EVENT_MARKER_BLOCK_START,GINT_TO_POINTER(track));

	    if (mt->audio_draws!=NULL&&g_list_length(mt->audio_draws)>=track+mt->opts.back_audio_tracks) {
	      // insert in audio too
	      insert_marker_event_at(mt->event_list,event,EVENT_MARKER_BLOCK_START,
				     GINT_TO_POINTER(-track-mt->opts.back_audio_tracks-1));
	    }
	  }
	  if (!block->ordered)  {
	    insert_marker_event_at(mt->event_list,event,EVENT_MARKER_BLOCK_UNORDERED,GINT_TO_POINTER(track));
	  }
	  if (event==block->end_event) blist->data=block->next;
	}
	track++;
	blist=blist->next;
      }
    }
    event=get_next_event(event);
  }
}






void on_save_event_list_activate (GtkMenuItem *menuitem, gpointer user_data) {
  //  here we save a layout list (*.lay) file

  // we dump (serialise) the event_list plant, followed by all of its events
  // serialisation method is described in the weed-docs/weedevents spec.
  // (serialising of event_lists)

  // loading an event list is simply the reverse of this process

  int fd;
  lives_mt *mt=(lives_mt *)user_data;
  gchar *esave_dir;
  gchar *esave_file;
  gchar *msg;
  gboolean response;
  gboolean retval=TRUE;
  gchar *filt[]={"*.lay",NULL};
  gboolean was_set=mainw->was_set;
  gchar *com;
  int *layout_map;
  int retval2;
  double *layout_map_audio;
  GtkWidget *ar_checkbutton;
  GtkWidget *eventbox;
  GtkWidget *label;
  GtkWidget *hbox;
  gboolean orig_ar_layout=prefs->ar_layout,ar_layout;
  weed_plant_t *event_list;
  gchar *layout_name;
  gchar xlayout_name[PATH_MAX];


  if (mt==NULL) {
    event_list=mainw->stored_event_list;
    layout_name=mainw->stored_layout_name;
  }
  else {
    mt_desensitise(mt);
    event_list=mt->event_list;
    layout_name=mt->layout_name;
  }

  // update layout map
  layout_map=update_layout_map(event_list);
  layout_map_audio=update_layout_map_audio(event_list);

  if (mainw->scrap_file!=-1&&layout_map[mainw->scrap_file]!=0) {
    // can't save if we have generated frames
    do_layout_scrap_file_error();
    g_free(layout_map);
    g_free(layout_map_audio);
    mainw->cancelled=CANCEL_USER;
    if (mt!=NULL) mt_sensitise(mt);
    return;
  }

  if (mainw->ascrap_file!=-1&&layout_map[mainw->ascrap_file]!=0) {
    // can't save if we have recorded audio
    do_layout_ascrap_file_error();
    g_free(layout_map);
    g_free(layout_map_audio);
    mainw->cancelled=CANCEL_USER;
    if (mt!=NULL) mt_sensitise(mt);
    return;
  }

  if (mt!=NULL&&mt->idlefunc>0) {
    g_source_remove(mt->idlefunc);
    mt->idlefunc=0;
  }

  if (strlen(mainw->set_name)>0) {
    weed_set_string_value(event_list,"needs_set",mainw->set_name);
  }
  else {
    gchar new_set_name[128];
    do {
      // prompt for a set name, advise user to save set
      renamew=create_rename_dialog(4);
      gtk_widget_show(renamew->dialog);
      response=gtk_dialog_run(GTK_DIALOG(renamew->dialog));
      if (response==GTK_RESPONSE_CANCEL) {
	gtk_widget_destroy(renamew->dialog);
	g_free(renamew);
	mainw->cancelled=CANCEL_USER;
	if (mt!=NULL) {
	  mt->idlefunc=0;
	  mt->idlefunc=mt_idle_add(mt);
	  mt_sensitise(mt);
	}
	return;
      }
      g_snprintf(new_set_name,128,"%s",gtk_entry_get_text (GTK_ENTRY (renamew->entry)));
      gtk_widget_destroy(renamew->dialog);
      g_free(renamew);
      while (g_main_context_iteration(NULL,FALSE));
    } while (!is_legal_set_name(new_set_name,FALSE));
    g_snprintf(mainw->set_name,128,"%s",new_set_name);
  }
  
  esave_dir=g_build_filename(prefs->tmpdir,mainw->set_name,"layouts/",NULL);
  com=g_strdup_printf ("/bin/mkdir -p \"%s\"",esave_dir);
  lives_system (com,FALSE);
  g_free (com);

  ar_checkbutton = gtk_check_button_new ();
  eventbox=gtk_event_box_new();
  label=gtk_label_new_with_mnemonic (_("_Autoreload each time"));
  gtk_label_set_mnemonic_widget (GTK_LABEL (label),ar_checkbutton);

  gtk_container_add(GTK_CONTAINER(eventbox),label);
  g_signal_connect (GTK_OBJECT (eventbox), "button_press_event",
		    G_CALLBACK (label_act_toggle),
		    ar_checkbutton);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg (eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }

  hbox = gtk_hbox_new (FALSE, 0);

  gtk_box_pack_start (GTK_BOX (hbox), ar_checkbutton, FALSE, FALSE, 10);
  gtk_box_pack_start (GTK_BOX (hbox), eventbox, FALSE, FALSE, 10);
  GTK_WIDGET_SET_FLAGS (ar_checkbutton, GTK_CAN_DEFAULT|GTK_CAN_FOCUS);

  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ar_checkbutton),prefs->ar_layout);
  g_signal_connect (GTK_OBJECT (ar_checkbutton), "toggled",
		    G_CALLBACK (on_autoreload_toggled),
		    GINT_TO_POINTER(2));

  gtk_widget_show_all(hbox);

  if (!strlen(layout_name)) esave_file=choose_file(esave_dir,NULL,filt,GTK_FILE_CHOOSER_ACTION_SAVE,NULL,hbox);
  else esave_file=choose_file(esave_dir,layout_name,filt,GTK_FILE_CHOOSER_ACTION_SAVE,NULL,hbox);

  ar_layout=prefs->ar_layout;
  prefs->ar_layout=orig_ar_layout;

  if (esave_file!=NULL) {
    g_free(esave_dir);
    esave_dir=get_dir(esave_file);
  }

  if (esave_file==NULL||!check_storage_space(NULL,FALSE)) {
    gchar *cdir;
    com=g_strdup_printf("/bin/rmdir \"%s\" 2>/dev/null",esave_dir);
    lives_system(com,TRUE);
    g_free(com);

    cdir=g_build_filename(prefs->tmpdir,mainw->set_name,NULL);
    com=g_strdup_printf("/bin/rmdir \"%s\" 2>/dev/null",cdir);
    lives_system(com,TRUE);
    g_free(com);

    g_free(esave_file);
    g_free(esave_dir);
    g_free(layout_map);
    g_free(layout_map_audio);
    mainw->was_set=was_set;
    if (!was_set) memset(mainw->set_name,0,1);
    mainw->cancelled=CANCEL_USER;

    if (mt!=NULL) {
      mt->idlefunc=0;
      mt->idlefunc=mt_idle_add(mt);
      mt_sensitise(mt);
    }
    return;
  }

  esave_file=ensure_extension(esave_file,".lay");

  g_snprintf(xlayout_name,PATH_MAX,"%s",esave_file);
  get_basename(xlayout_name);

  if (mt!=NULL) add_markers(mt,mt->event_list);

  do {
    retval2=0;
    retval=TRUE;

    fd=creat(esave_file,DEF_FILE_PERMS);

    if (fd>=0) {
      retval=save_event_list_inner(mt,fd,event_list,NULL);
      close(fd);
    }

    if (!retval||fd<0) {
      retval2=do_write_failed_error_s_with_retry(esave_file,(fd<0)?g_strerror(errno):NULL,NULL);
      if (retval2==LIVES_CANCEL) {
	if (mt!=NULL) {
	  mt->idlefunc=0;
	  mt->idlefunc=mt_idle_add(mt);
	  mt_sensitise(mt);
	}
	return;
      }
    }
  } while (retval2==LIVES_RETRY);

  if (retval2!=LIVES_CANCEL) {
    msg=g_strdup_printf(_("Saved layout to %s\n"),esave_file);
    d_print(msg);
    g_free(msg);
  }

  // save layout map
  save_layout_map(layout_map,layout_map_audio,esave_file,esave_dir);

  if (mt!=NULL) mt->changed=FALSE;

  if (!ar_layout) {
    prefs->ar_layout=FALSE;
    set_pref("ar_layout","");
    memset(prefs->ar_layout_name,0,1);
  }
  else {
    prefs->ar_layout=TRUE;
    set_pref("ar_layout",layout_name);
    g_snprintf(prefs->ar_layout_name,PATH_MAX,"%s",xlayout_name);
  }

  g_free(esave_file);
  g_free(esave_dir);
  if (layout_map!=NULL) g_free(layout_map);
  if (layout_map_audio!=NULL) g_free(layout_map_audio);

  recover_layout_cancelled(NULL,NULL);

  if (mt!=NULL) {
    mt->auto_changed=FALSE;
    mt->idlefunc=0;
    mt->idlefunc=mt_idle_add(mt);
    mt_sensitise(mt);
  }
}

// next functions are mainly to do with event_list manipulation


static gchar *rec_error_add(gchar *ebuf, gchar *msg, int num, weed_timecode_t tc) {
  // log an error generated during event_list rectification

  gchar *tmp;
  gchar *xnew;

  elist_errors++;

  threaded_dialog_spin();
  if (tc==-1) xnew=g_strdup(msg); // missing timecode
  else {
    if (num==-1) xnew=g_strdup_printf("%s at timecode %"PRId64"\n",msg,tc);
    else xnew=g_strdup_printf("%s %d at timecode %"PRId64"\n",msg,num,tc);
  }
  tmp=g_strconcat (ebuf,xnew,NULL);
#define SILENT_EVENT_LIST_LOAD
#ifndef SILENT_EVENT_LIST_LOAD
  g_printerr("Rec error: %s",xnew);
#endif
  g_free(ebuf);
  g_free(xnew);
  threaded_dialog_spin();
  return tmp;
} 



static int get_next_tt_key(ttable *trans_table) {
  int i;
  for (i=free_tt_key;i<FX_KEYS_MAX-FX_KEYS_MAX_VIRTUAL;i++) {
    if (trans_table[i].in==NULL) return i;
  }
  return -1;
}

static void *find_init_event_in_ttable(ttable *trans_table, weed_plant_t *in, gboolean normal) {
  int i;
  for (i=0;i<FX_KEYS_MAX-FX_KEYS_MAX_VIRTUAL;i++) {
    if (normal&&trans_table[i].in==in) return trans_table[i].out;
    if (!normal&&trans_table[i].out==in) return trans_table[i].in; // reverse lookup for past filter_map check
    if (trans_table[i].out==NULL) return NULL;
  }
  return NULL;
}


static void **remove_nulls_from_filter_map(void **init_events, int *num_events) {
  // remove NULLs from filter_map init_events

  // old array may be free()d, return value should be freed() unless NULL
  int num_nulls=0,i,j=0;
  void **new_init_events;

  if (*num_events==1) return init_events;

  for (i=0;i<*num_events;i++) if (init_events[i]==NULL) num_nulls++;
  if (num_nulls==0) return init_events;

  *num_events-=num_nulls;

  if (*num_events==0) new_init_events=NULL;

  else new_init_events=(void **)g_malloc((*num_events)*sizeof(void *));
  
  for (i=0;i<*num_events+num_nulls;i++) if (init_events[i]!=NULL) new_init_events[j++]=init_events[i];

  g_free(init_events);

  if (*num_events==0) *num_events=1;

  return new_init_events;
}




void move_init_in_filter_map(lives_mt *mt, weed_plant_t *event_list, weed_plant_t *event, weed_plant_t *ifrom, 
			     weed_plant_t *ito, gint track, gboolean after) {
  int error,i,j;
  weed_plant_t *deinit_event=(weed_plant_t *)weed_get_voidptr_value(ifrom,"deinit_event",&error);
  void **events_before=NULL;
  void **events_after=NULL;
  gint num_before=0,j1;
  gint num_after=0,j2;
  gboolean got_after;
  void **init_events,**new_init_events;
  int num_inits;

  while (event!=deinit_event) {
    if (!WEED_EVENT_IS_FILTER_MAP(event)) {
      event=get_next_event(event);
      continue;
    }
    init_events=weed_get_voidptr_array(event,"init_events",&error);
    num_inits=weed_leaf_num_elements(event,"init_events");
    if (events_before==NULL&&events_after==NULL) {
      j=0;
      for (i=0;i<num_inits;i++) {
	if (init_events[i]==ifrom) continue;
	if (init_events[i]!=ito&&!init_event_is_relevant((weed_plant_t *)init_events[i],track)) continue;
	j++;
	if (init_events[i]==ito) {
	  num_before=j-1+after;
	  j=1;
	}
      }
      num_after=j-after;
      if (num_before>0) events_before=(void **)g_malloc(num_before*sizeof(void *));
      if (num_after>0) events_after=(void **)g_malloc(num_after*sizeof(void *));
      j1=j2=0;
      for (i=0;i<num_inits;i++) {
	if (!init_event_is_relevant((weed_plant_t *)init_events[i],track)) continue;
	if (init_events[i]==ifrom) continue;
	if (j1<num_before) {
	  events_before[j1]=init_events[i];
	  j1++;
	}
	else {
	  events_after[j2]=init_events[i];
	  j2++;
	}
      }
    }
    // check to see if we can move event without problem
    got_after=FALSE;
    for (i=0;i<num_inits;i++) {
      if (init_events[i]==ifrom) continue;
      if (!init_event_is_relevant((weed_plant_t *)init_events[i],track)) continue;
      if (!got_after&&init_event_in_list(events_after,num_after,(weed_plant_t *)init_events[i])) got_after=TRUE;
      if (got_after&&init_event_in_list(events_before,num_before,(weed_plant_t *)init_events[i])) {
	weed_free(init_events);
	if (events_before!=NULL) g_free(events_before);
	if (events_after!=NULL) g_free(events_after);
	return; // order has changed, give up
      }
    }
    new_init_events=(void **)g_malloc(num_inits*sizeof(void *));
    got_after=FALSE;
    j=0;
    for (i=0;i<num_inits;i++) {
      if (init_events[i]==ifrom) continue;
      if ((init_event_in_list(events_before,num_before,(weed_plant_t *)init_events[i])||
	   !init_event_is_relevant((weed_plant_t *)init_events[i],track))&&
	  !init_event_is_process_last((weed_plant_t *)init_events[i])&&!init_event_is_process_last(ifrom))
	new_init_events[j]=init_events[i];
      else {
	if (!got_after) {
	  got_after=TRUE;
	  new_init_events[j]=ifrom;
	  i--;
	  j++;
	  continue;
	}
	new_init_events[j]=init_events[i];
      }
      j++;
    }
    if (j<num_inits) new_init_events[j]=ifrom;
    weed_set_voidptr_array(event,"init_events",num_inits,new_init_events);
    g_free(new_init_events);
    weed_free(init_events);
    event=get_next_event(event);
  }

  if (events_before!=NULL) g_free(events_before);
  if (events_after!=NULL) g_free(events_after);

}



gboolean compare_filter_maps(weed_plant_t *fm1, weed_plant_t *fm2, gint ctrack) {
  // return TRUE if the maps match exactly; if ctrack is !=-1000000, 
  // then we only compare filter maps where ctrack is an in_track or out_track
  int i1,i2,error,num_events1,num_events2;
  void **inits1,**inits2;

  if (!weed_plant_has_leaf(fm1,"init_events")&&!weed_plant_has_leaf(fm2,"init_events")) return TRUE;
  if (ctrack==-1000000&&((!weed_plant_has_leaf(fm1,"init_events")&&
			  weed_get_voidptr_value(fm2,"init_events",&error)!=NULL)||
			 (!weed_plant_has_leaf(fm2,"init_events")&&
			  weed_get_voidptr_value(fm1,"init_events",&error)!=NULL))) return FALSE;

  if (ctrack==-1000000&&(weed_plant_has_leaf(fm1,"init_events"))&&weed_plant_has_leaf(fm2,"init_events")&&
      ((weed_get_voidptr_value(fm1,"init_events",&error)==NULL&&
	weed_get_voidptr_value(fm2,"init_events",&error)!=NULL)||
       (weed_get_voidptr_value(fm1,"init_events",&error)!=NULL&&
	weed_get_voidptr_value(fm2,"init_events",&error)==NULL))) return FALSE;

  num_events1=weed_leaf_num_elements(fm1,"init_events");
  num_events2=weed_leaf_num_elements(fm2,"init_events");
  if (ctrack==-1000000&&num_events1!=num_events2) return FALSE;

  inits1=weed_get_voidptr_array(fm1,"init_events",&error);
  inits2=weed_get_voidptr_array(fm2,"init_events",&error);

  if (inits1==NULL&&inits2==NULL) return TRUE;

  i2=0;

  for (i1=0;i1<num_events1;i1++) {

    if (i2<num_events2&&init_event_is_process_last((weed_plant_t *)inits2[i2])) {
      // for process_last we don't care about the exact order
      if (init_event_in_list(inits1,num_events1,(weed_plant_t *)inits2[i2])) {
	i2++;
	continue;
      }
    }

    if (init_event_is_process_last((weed_plant_t *)inits1[i1])) {
      // for process_last we don't care about the exact order
      if (init_event_in_list(inits2,num_events2,(weed_plant_t *)inits1[i1])) {
	continue;
      }
    }


    if (ctrack!=-1000000) {

      if (inits1[i1]!=NULL) {

	if (init_event_is_relevant((weed_plant_t *)inits1[i1],ctrack)) {
	  if (i2>=num_events2) {
	    weed_free(inits1);
	    weed_free(inits2);
	    return FALSE;
	  }
	}
	else continue;  // skip this one, it doesn't involve ctrack

      }
      else continue; // skip NULLS
    }

    if (i2<num_events2) {
      if (ctrack!=-1000000) {
	if (inits2[i2]==NULL||!init_event_is_relevant((weed_plant_t *)inits2[i2],ctrack)) {
	  i2++;
	  i1--;
	  continue; // skip this one, it doesn't involve ctrack
	}
      }
      
      if (inits1[i1]!=inits2[i2]) {
	weed_free(inits1);
	weed_free(inits2);
	return FALSE;
      }
      i2++;
    }
  }

  if (i2<num_events2) {
    if (ctrack==-1000000) {
      weed_free(inits1);
      return FALSE;
    }
    for (;i2<num_events2;i2++) {
      if (inits2[i2]!=NULL) {

	if (init_event_is_process_last((weed_plant_t *)inits2[i2])) {
	  // for process_last we don't care about the exact order
	  if (init_event_in_list(inits1,num_events1,(weed_plant_t *)inits2[i2])) continue;
	}

	if (init_event_is_relevant((weed_plant_t *)inits2[i2],ctrack)) {
	  weed_free(inits1);
	  weed_free(inits2);
	  return FALSE;
	}

      }
    }
  }
  if (inits1!=NULL) weed_free(inits1);
  if (inits2!=NULL) weed_free(inits2);
  return TRUE;
}



static gchar *filter_map_check(ttable *trans_table,weed_plant_t *filter_map, weed_timecode_t deinit_tc, 
			       weed_timecode_t fm_tc, gchar *ebuf) {
  int num_init_events;
  void **init_events;
  void **copy_events;
  int error,i;

  if (!weed_plant_has_leaf(filter_map,"init_events")) return ebuf;
  // check no deinited events are active
  num_init_events=weed_leaf_num_elements(filter_map,"init_events");
  if (num_init_events==1&&weed_get_voidptr_value(filter_map,"init_events",&error)==NULL) return ebuf;
  init_events=weed_get_voidptr_array(filter_map,"init_events",&error);
  copy_events=(void **)g_malloc(num_init_events*sizeof(weed_plant_t *));
  for (i=0;i<num_init_events;i++) {
    if (find_init_event_in_ttable(trans_table,(weed_plant_t *)init_events[i],FALSE)!=NULL) copy_events[i]=init_events[i];
    else {
      copy_events[i]=NULL;
      ebuf=rec_error_add(ebuf,"Filter_map points to invalid filter_init",-1,fm_tc);
    }
  }
  if (num_init_events>1) copy_events=remove_nulls_from_filter_map(copy_events,&num_init_events);

  if (copy_events!=NULL) g_free(copy_events);
  weed_free(init_events);
  return ebuf;
}


static gchar *add_filter_deinits(weed_plant_t *event_list, ttable *trans_table, void ***pchains, 
				 weed_timecode_t tc, gchar *ebuf) {
  // add filter deinit events for any remaining active filters
  int i,j,error,num_params;
  gchar *filter_hash;
  gint idx;
  weed_plant_t *filter,*init_event,*event;
  void **in_pchanges;

  for (i=0;i<FX_KEYS_MAX-FX_KEYS_MAX_VIRTUAL;i++) {
    if (trans_table[i].out==NULL) continue;
    if (trans_table[i].in!=NULL) {
      event_list=append_filter_deinit_event(event_list,tc,(init_event=(weed_plant_t *)trans_table[i].out),pchains[i]);
      event=get_last_event(event_list);

      filter_hash=weed_get_string_value(init_event,"filter",&error);
      if ((idx=weed_get_idx_for_hashname(filter_hash,TRUE))!=-1) {
	filter=get_weed_filter(idx);
	if ((num_params=num_in_params(filter,TRUE,TRUE))>0) {
	  in_pchanges=(void **)g_malloc(num_params*sizeof(void *));
	  for (j=0;j<num_params;j++) {
	    if (!WEED_EVENT_IS_FILTER_INIT((weed_plant_t *)pchains[i][j])) 
	      in_pchanges[j]=(weed_plant_t *)pchains[i][j];
	    else in_pchanges[j]=NULL;
	  }
	  weed_set_voidptr_array(event,"in_parameters",num_params,in_pchanges); // set array to last param_changes
	  g_free(in_pchanges);
	  g_free(pchains[i]);
	}
      }
      weed_free(filter_hash);
      ebuf=rec_error_add(ebuf,"Added missing filter_deinit",-1,tc);
    }
  }
  return ebuf;
}


static gchar *add_null_filter_map(weed_plant_t *event_list, weed_plant_t *last_fm, weed_timecode_t tc, gchar *ebuf) {
  int num_events;
  int error;

  if (!weed_plant_has_leaf(last_fm,"init_events")) return ebuf;

  num_events=weed_leaf_num_elements(last_fm,"init_events");
  if (num_events==1&&weed_get_voidptr_value(last_fm,"init_events",&error)==NULL) return ebuf;

  event_list=append_filter_map_event(event_list,tc,NULL);

  ebuf=rec_error_add(ebuf,"Added missing empty filter_map",-1,tc);
  return ebuf;
}

static weed_plant_t *duplicate_frame_at(weed_plant_t *event_list, weed_plant_t *src_frame, weed_timecode_t tc) {
  // tc should be > src_frame tc : i.e. copy is forward in time because insert_frame_event_at searches forward
  int error;
  int *clips,*frames;
  int numframes=weed_leaf_num_elements(src_frame,"clips");

  if (!numframes) return src_frame;

  clips=weed_get_int_array(src_frame,"clips",&error);
  frames=weed_get_int_array(src_frame,"frames",&error);

  event_list=insert_frame_event_at (event_list, tc, numframes, clips, frames, &src_frame);

  weed_free(clips);
  weed_free(frames);
  return get_frame_event_at(event_list,tc,src_frame,TRUE);
}




static GList *atrack_list;

static void add_atrack_to_list(int track, int clip) {
  // keep record of audio tracks so we can add closures if missing
  GList *alist=atrack_list;
  gchar *entry;
  gchar **array;

  while (alist!=NULL) {
    entry=(gchar *)alist->data;
    array=g_strsplit(entry,"|",-1);
    if (atoi(array[0])==track) {
      g_free(alist->data);
      alist->data=g_strdup_printf("%d|%d",track,clip);
      g_strfreev(array);
      return;
    }
    g_strfreev(array);
    alist=alist->next;
  }
  atrack_list=g_list_append(atrack_list,g_strdup_printf("%d|%d",track,clip));
}


static void remove_atrack_from_list(int track) {
  // keep record of audio tracks so we can add closures if missing
  GList *alist=atrack_list,*alist_next;
  gchar *entry;
  gchar **array;

  while (alist!=NULL) {
    alist_next=alist->next;
    entry=(gchar *)alist->data;
    array=g_strsplit(entry,"|",-1);
    if (atoi(array[0])==track) {
      atrack_list=g_list_remove(atrack_list,entry);
      g_strfreev(array);
      g_free(entry);
      return;
    }
    g_strfreev(array);
    alist=alist_next;
  }
}


static void add_missing_atrack_closers(weed_plant_t *event_list, gdouble fps, gchar *ebuf) {
  GList *alist=atrack_list;
  gchar *entry;
  gchar **array;
  int i=0;

  int *aclips;
  double *aseeks;

  weed_plant_t *last_frame;
  int num_atracks;
  weed_timecode_t tc;

  if (atrack_list==NULL) return;

  num_atracks=g_list_length(atrack_list)*2;

  aclips=(int *)g_malloc(num_atracks*sizint);
  aseeks=(double *)g_malloc(num_atracks*sizdbl);

  last_frame=get_last_frame_event(event_list);
  tc=get_event_timecode(last_frame);

  if (!is_blank_frame(last_frame,TRUE)) {
    weed_plant_t *shortcut=last_frame;
    event_list=insert_blank_frame_event_at(event_list,q_gint64(tc+1./U_SEC,fps),&shortcut);
  }

  while (alist!=NULL) {
    entry=(gchar *)alist->data;
    array=g_strsplit(entry,"|",-1);
    aclips[i]=atoi(array[0]);
    aclips[i+1]=atoi(array[1]);
    aseeks[i]=0.;
    aseeks[i+1]=0.;
    g_strfreev(array);
    if (aclips[i]>=0) ebuf=rec_error_add(ebuf,"Added missing audio closure",aclips[i],tc);
    else ebuf=rec_error_add(ebuf,"Added missing audio closure to backing track",-aclips[i],tc);
    i+=2;
    alist=alist->next;
  }

  weed_set_int_array(last_frame,"audio_clips",num_atracks,aclips);
  weed_set_double_array(last_frame,"audio_seeks",num_atracks,aseeks);

  g_free(aclips);
  g_free(aseeks);

  g_list_free_strings(atrack_list);
  g_list_free(atrack_list);
  atrack_list=NULL;
}




gboolean event_list_rectify(lives_mt *mt, weed_plant_t *event_list) {
  // check and reassemble a newly loaded event_list
  // reassemply consists of matching init_event(s) to event_id's
  // we also rebuild our param_change chains ("in_parameters" in filter_init and filter_deinit, 
  // and "next_change" and "prev_change" 
  // in other param_change events)

  // The checking done is quite sophisticated, and can correct many errors in badly-formed event_lists

  weed_plant_t **ptmpls;
  weed_plant_t **ctmpls;

  weed_plant_t *event=get_first_event(event_list),*event_next;
  weed_plant_t *shortcut=NULL;
  weed_plant_t *last_frame_event;
  weed_plant_t *event_id;
  weed_plant_t *last_filter_map=NULL;
  weed_plant_t *filter=NULL;
  weed_plant_t *last_event;

  weed_timecode_t tc=0,last_tc=0;
  weed_timecode_t last_frame_tc=-1;
  weed_timecode_t last_deinit_tc=-1;
  weed_timecode_t last_filter_map_tc=-1;
  weed_timecode_t cur_tc=0;

  gchar *ebuf=g_strdup("");
  gchar *host_tag_s;
  gchar *filter_hash;
  gchar *bit1=g_strdup(""),*bit2=NULL,*bit3=g_strdup("."),*msg;

  int *inct,*outct;
  int *clip_index,*frame_index,*aclip_index;
  int *new_clip_index,*new_frame_index,*new_aclip_index;

  int hint;
  int i,error,idx,filter_idx,j;
  int host_tag;
  int num_ctmpls,num_inct,num_outct;
  int pnum,thisct;
  int num_init_events;
  int num_params;
  int pflags;
  int num_tracks,num_atracks;
  int last_valid_frame;
  int marker_type;
  int ev_count=0;

  gboolean check_filter_map=FALSE;
  gboolean was_deleted=FALSE;
  gboolean was_moved;
  gboolean missing_clips=FALSE,missing_frames=FALSE;

  void *init_event;
  void **init_events,**new_init_events;
  void **in_pchanges,**orig_pchanges;
  void **pchains[FX_KEYS_MAX-FX_KEYS_MAX_VIRTUAL]; // parameter chains

  gdouble fps=weed_get_double_value(event_list,"fps",&error);
  double *aseek_index,*new_aseek_index;

  GtkWidget *transient;

  if (mt!=NULL) mt->layout_prompt=FALSE;

  ttable trans_table[FX_KEYS_MAX-FX_KEYS_MAX_VIRTUAL]; // translation table for init_events

  for (i=0;i<FX_KEYS_MAX-FX_KEYS_MAX_VIRTUAL;i++) trans_table[i].in=trans_table[i].out=NULL;

  free_tt_key=0;

  atrack_list=NULL;

  while (event!=NULL) {
    was_deleted=FALSE;
    event_next=get_next_event(event);
    if (!weed_plant_has_leaf(event,"timecode")) {
      ebuf=rec_error_add(ebuf,"Event has no timecode",weed_get_plant_type(event),-1);
      delete_event(event_list,event);
      event=event_next;
      continue;
    }
    tc=get_event_timecode(event);
    tc=q_gint64(tc+U_SEC/(2.*fps)-1,fps);
    weed_set_int64_value(event,"timecode",tc);


    ev_count++;
    g_snprintf(mainw->msg,256,"%d|",ev_count);
    threaded_dialog_spin();

    if (!weed_get_plant_type(event)==WEED_PLANT_EVENT) {
      ebuf=rec_error_add(ebuf,"Invalid plant type",weed_get_plant_type(event),tc);
      delete_event(event_list,event);
      event=event_next;
      continue;
    }
    if (tc<last_tc) {
      ebuf=rec_error_add(ebuf,"Out of order event",-1,tc);
      delete_event(event_list,event);
      event=event_next;
      continue;
    }
    if (!weed_plant_has_leaf(event,"hint")) {
      ebuf=rec_error_add(ebuf,"Event has no hint",weed_get_plant_type(event),tc);
      delete_event(event_list,event);
      event=event_next;
      continue;
    }

    hint=get_event_hint(event);

    switch (hint) {
    case WEED_EVENT_HINT_FILTER_INIT:
      // set in table
      if (!weed_plant_has_leaf(event,"event_id")) {
	ebuf=rec_error_add(ebuf,"Filter_init missing event_id",-1,tc);
	delete_event(event_list,event);
	was_deleted=TRUE;
      }
      else {
	if (!weed_plant_has_leaf(event,"filter")) {
	  ebuf=rec_error_add(ebuf,"Filter_init missing filter",-1,tc);
	  delete_event(event_list,event);
	  was_deleted=TRUE;
	}
	else {
	  filter_hash=weed_get_string_value(event,"filter",&error);
	  if ((filter_idx=weed_get_idx_for_hashname(filter_hash,TRUE))!=-1) {
	    filter=get_weed_filter(filter_idx);
	    if (weed_plant_has_leaf(filter,"in_channel_templates")) {
	      if (!weed_plant_has_leaf(event,"in_count")) {
		ebuf=rec_error_add(ebuf,"Filter_init missing filter",-1,tc);
		delete_event(event_list,event);
		was_deleted=TRUE;
	      }
	      else {
		num_ctmpls=weed_leaf_num_elements(filter,"in_channel_templates");
		num_inct=weed_leaf_num_elements(event,"in_count");
		if (num_ctmpls!=num_inct) {
		  ebuf=rec_error_add(ebuf,"Filter_init has invalid in_count",-1,tc);
		  delete_event(event_list,event);
		  was_deleted=TRUE;
		}
		else {
		  inct=weed_get_int_array(event,"in_count",&error);
		  ctmpls=weed_get_plantptr_array(filter,"in_channel_templates",&error);
		  for (i=0;i<num_ctmpls;i++) {
		    thisct=inct[i];
		    if (thisct==0&&!weed_plant_has_leaf(ctmpls[i],"optional")) {
		      ebuf=rec_error_add(ebuf,"Filter_init disables a non-optional in channel",i,tc);
		      delete_event(event_list,event);
		      was_deleted=TRUE;
		    }
		    else {
		      if (thisct>1&&(!weed_plant_has_leaf(ctmpls[i],"max_repeats")||
				     (weed_get_int_value(ctmpls[i],"max_repeats",&error)>0&&
				      weed_get_int_value(ctmpls[i],"max_repeats",&error)<thisct))) {
			ebuf=rec_error_add(ebuf,"Filter_init has too many repeats of in channel",i,tc);
			delete_event(event_list,event);
			was_deleted=TRUE;
		      }
		    }
		  }

		  weed_free(inct);
		  weed_free(ctmpls);

		  if (!was_deleted) {
		    num_ctmpls=weed_leaf_num_elements(filter,"out_channel_templates");
		    num_outct=weed_leaf_num_elements(event,"out_count");
		    if (num_ctmpls!=num_outct) {
		      ebuf=rec_error_add(ebuf,"Filter_init has invalid out_count",-1,tc);
		      delete_event(event_list,event);
		      was_deleted=TRUE;
		    }
		    else {
		      outct=weed_get_int_array(event,"out_count",&error);
		      ctmpls=weed_get_plantptr_array(filter,"out_channel_templates",&error);
		      for (i=0;i<num_ctmpls;i++) {
			thisct=outct[i];
			if (thisct==0&&!weed_plant_has_leaf(ctmpls[i],"optional")) {
			  ebuf=rec_error_add(ebuf,"Filter_init disables a non-optional out channel",i,tc);
			  delete_event(event_list,event);
			  was_deleted=TRUE;
			}
			else {
			  if (thisct>1&&(!weed_plant_has_leaf(ctmpls[i],"max_repeats")||
					 (weed_get_int_value(ctmpls[i],"max_repeats",&error)>0&&
					  weed_get_int_value(ctmpls[i],"max_repeats",&error)<thisct))) {
			    ebuf=rec_error_add(ebuf,"Filter_init has too many repeats of out channel",i,tc);
			    delete_event(event_list,event);
			    was_deleted=TRUE;
			  }
			  else {
			    if (weed_plant_has_leaf(event,"in_tracks")) {
			      gint ntracks=weed_leaf_num_elements(event,"in_tracks");
			      int *trax=weed_get_int_array(event,"in_tracks",&error);
			      for (i=0;i<ntracks;i++) {
				if (trax>=0&&!has_video_chans_in(filter,FALSE)) {
				  // TODO ** inform user
				  if (mt!=NULL&&!mt->opts.pertrack_audio) {
				    gtk_widget_set_sensitive(mt->fx_region_2a,TRUE);
				    mt->opts.pertrack_audio=TRUE;
				  }
				  else force_pertrack_audio=TRUE;
				}

				if (trax[i]==-1) {
				  // TODO ** inform user
				  if (mt!=NULL&&mt->opts.back_audio_tracks==0) {
				    mt->opts.back_audio_tracks=1;
				    ebuf=rec_error_add(ebuf,"Adding backing audio",-1,tc);
				  }
				  else force_backing_tracks=1;
				}
			      }

			      weed_free(trax);
			      ntracks=weed_leaf_num_elements(event,"out_tracks");
			      trax=weed_get_int_array(event,"out_tracks",&error);
			      for (i=0;i<ntracks;i++) {
				if (trax>=0&&!has_video_chans_out(filter,FALSE)) {
				  // TODO ** inform user
				  if (mt!=NULL&&!mt->opts.pertrack_audio) {
				    gtk_widget_set_sensitive(mt->fx_region_2a,TRUE);
				    mt->opts.pertrack_audio=TRUE;
				  }
				  else force_pertrack_audio=TRUE;
				}
				if (trax[i]==-1) {
				  // TODO ** inform user
				  if (mt!=NULL&&mt->opts.back_audio_tracks==0) {
				    mt->opts.back_audio_tracks=1;
				  }
				  else force_backing_tracks=1;
				}
			      }
			      weed_free(trax);
			    }
			  }
			  // all tests passed
			  if (tc==0) {
			    if (mt!=NULL&&mt->avol_fx==-1) {
			    // check if it can be a filter delegate
			      GList *clist=mainw->fx_candidates[FX_CANDIDATE_AUDIO_VOL].list;
			      while (clist!=NULL) {
				if (GPOINTER_TO_INT(clist->data)==filter_idx) {
				  mt->avol_fx=filter_idx;
				  mt->avol_init_event=event;
				  break;
				}
				clist=clist->next;
			      }
			    }
			  }
			}
		      }
		      weed_free(outct);
		      weed_free(ctmpls);
		    }}}}}}
	  else {
	    g_printerr("Layout contains unknown filter %s\n",filter_hash);
	    ebuf=rec_error_add(ebuf,"Layout contains unknown filter",-1,tc);
	    delete_event(event_list,event);
	    was_deleted=TRUE;
	    if (mt!=NULL) mt->layout_prompt=TRUE;
	  }
	  weed_free(filter_hash);
	  if (!was_deleted) {
	    host_tag=get_next_tt_key(trans_table)+FX_KEYS_MAX_VIRTUAL+1;
	    if (host_tag==-1) {
	      ebuf=rec_error_add(ebuf,"Fatal: too many active effects",FX_KEYS_MAX-FX_KEYS_MAX_VIRTUAL,tc);
	      end_threaded_dialog();
	      return FALSE;
	    }
	    host_tag_s=g_strdup_printf("%d",host_tag);
	    weed_set_string_value(event,"host_tag",host_tag_s);
	    g_free(host_tag_s);
	    event_id=(weed_plant_t *)weed_get_voidptr_value(event,"event_id",&error);
	    trans_table[(idx=host_tag-FX_KEYS_MAX_VIRTUAL-1)].in=event_id;
	    trans_table[idx].out=event;
	    
	    // use pchain array
	    if ((num_params=num_in_params(filter,TRUE,TRUE))>0) {
	      pchains[idx]=(void **)g_malloc(num_params*sizeof(void *));
	      in_pchanges=(void **)g_malloc(num_params*sizeof(void *));
	      for (i=0;i<num_params;i++) {
		pchains[idx][i]=event;
		in_pchanges[i]=NULL;
	      }
	      // set all to NULL, we will re-fill as we go along
	      weed_set_voidptr_array(event,"in_parameters",num_params,in_pchanges);
	      g_free(in_pchanges);
	    }
	  }
	}
      }
      break;
    case WEED_EVENT_HINT_FILTER_DEINIT:

      // update "init_event" from table, remove entry; we will check filter_map at end of tc
      if (!weed_plant_has_leaf(event,"init_event")) {
	ebuf=rec_error_add(ebuf,"Filter_deinit missing init_event",-1,tc);
	delete_event(event_list,event);
	was_deleted=TRUE;
      }
      else {
	event_id=(weed_plant_t *)weed_get_voidptr_value(event,"init_event",&error);
	init_event=find_init_event_in_ttable(trans_table,event_id,TRUE);
	if (init_event==NULL) {
	  ebuf=rec_error_add(ebuf,"Filter_deinit has invalid init_event",-1,tc);
	  delete_event(event_list,event);
	  was_deleted=TRUE;
	}
	else {
	  weed_set_voidptr_value((weed_plant_t *)init_event,"deinit_event",event);
	  host_tag_s=weed_get_string_value((weed_plant_t *)init_event,"host_tag",&error);
	  host_tag=atoi(host_tag_s);
	  weed_free(host_tag_s);
	  trans_table[(idx=host_tag-FX_KEYS_MAX_VIRTUAL-1)].in=NULL;
	  if (idx<free_tt_key) free_tt_key=idx;
	  weed_set_voidptr_value(event,"init_event",init_event);
	  check_filter_map=TRUE;
	  last_deinit_tc=tc;

	  filter_hash=weed_get_string_value((weed_plant_t *)init_event,"filter",&error);
	  if ((filter_idx=weed_get_idx_for_hashname(filter_hash,TRUE))!=-1) {
	    filter=get_weed_filter(filter_idx);
	    if ((num_params=num_in_params(filter,TRUE,TRUE))>0) {
	      in_pchanges=(void **)g_malloc(num_params*sizeof(void *));
	      for (i=0;i<num_params;i++) {
		if (!WEED_EVENT_IS_FILTER_INIT((weed_plant_t *)pchains[idx][i])) 
		  in_pchanges[i]=(weed_plant_t *)pchains[idx][i];
		else in_pchanges[i]=NULL;
	      }
	      weed_set_voidptr_array(event,"in_parameters",num_params,in_pchanges); // set array to last param_changes
	      g_free(in_pchanges);
	      g_free(pchains[idx]);
	    }
	  }
	  weed_free(filter_hash);
	}
      }
      break;
    case WEED_EVENT_HINT_FILTER_MAP:
	// update "init_events" from table
      if (weed_plant_has_leaf(event,"init_events")) {
	num_init_events=weed_leaf_num_elements(event,"init_events");
	init_events=weed_get_voidptr_array(event,"init_events",&error);
	new_init_events=(void **)g_malloc(num_init_events*sizeof(void *));
	for (i=0;i<num_init_events;i++) {
	  event_id=(weed_plant_t *)init_events[i];
	  if (event_id!=NULL) {
	    init_event=find_init_event_in_ttable(trans_table,event_id,TRUE);
	    if (init_event==NULL) {
	      ebuf=rec_error_add(ebuf,"Filter_map has invalid init_event",-1,tc);
	      new_init_events[i]=NULL;
	    }
	    else new_init_events[i]=init_event;
	  }
	  else new_init_events[i]=NULL;
	}
	new_init_events=remove_nulls_from_filter_map(new_init_events,&num_init_events);

	if (new_init_events==NULL) weed_set_voidptr_value(event,"init_events",NULL);
	else {
	  weed_set_voidptr_array(event,"init_events",num_init_events,new_init_events);

	  for (i=0;i<num_init_events;i++) {
	    if (init_event_is_process_last((weed_plant_t *)new_init_events[i])) {
	      // reposition process_last events to the end
	      add_init_event_to_filter_map(event,(weed_plant_t *)new_init_events[i],NULL);
	    }
	  }
	  g_free(new_init_events);
	}
	weed_free(init_events);
      }
      else {
	weed_set_voidptr_value(event,"init_events",NULL);
      }
      if (last_filter_map!=NULL) {
	if (compare_filter_maps(last_filter_map,event,-1000000)) {
	  // filter map is identical to prior one, we can remove this one
	  delete_event(event_list,event);
	  was_deleted=TRUE;
	}
      }
      else if (weed_leaf_num_elements(event,"init_events")==1&&weed_get_voidptr_value(event,"init_events",&error)==NULL) {
	delete_event(event_list,event);
	was_deleted=TRUE;
      }
      if (!was_deleted) last_filter_map=event;

      break;
    case WEED_EVENT_HINT_PARAM_CHANGE:
      if (!weed_plant_has_leaf(event,"index")) {
	ebuf=rec_error_add(ebuf,"Param_change has no index",-1,tc);
	delete_event(event_list,event);
	was_deleted=TRUE;
      }
      else {
	if (!weed_plant_has_leaf(event,"value")) {
	  ebuf=rec_error_add(ebuf,"Param_change has no value",-1,tc);
	  delete_event(event_list,event);
	  was_deleted=TRUE;
	}
	else {
	  if (!weed_plant_has_leaf(event,"init_event")) {
	    ebuf=rec_error_add(ebuf,"Param_change has no init_event",-1,tc);
	    delete_event(event_list,event);
	    was_deleted=TRUE;
	  }
	  else {
	    event_id=(weed_plant_t *)weed_get_voidptr_value(event,"init_event",&error);
	    if ((init_event=find_init_event_in_ttable(trans_table,event_id,TRUE))==NULL) {
	      ebuf=rec_error_add(ebuf,"Param_change has invalid init_event",-1,tc);
	      delete_event(event_list,event);
	      was_deleted=TRUE;
	    }
	    else {
	      filter_hash=weed_get_string_value((weed_plant_t *)init_event,"filter",&error);
	      if ((filter_idx=weed_get_idx_for_hashname(filter_hash,TRUE))!=-1) {
		filter=get_weed_filter(filter_idx);
		pnum=weed_get_int_value(event,"index",&error);
		if (pnum<0||pnum>=(num_params=num_in_params(filter,TRUE,TRUE))) {
		  ebuf=rec_error_add(ebuf,"Param_change has invalid index",pnum,tc);
		  delete_event(event_list,event);
		  was_deleted=TRUE;
		}
		else {
		  ptmpls=weed_get_plantptr_array(filter,"in_parameter_templates",&error);
		  if (!weed_plant_has_leaf(event,"value")) {
		    ebuf=rec_error_add(ebuf,"Param_change has no value with index",pnum,tc);
		    delete_event(event_list,event);
		    was_deleted=TRUE;
		  }
		  else {
		    if (weed_leaf_seed_type(event,"value")!=weed_leaf_seed_type(ptmpls[pnum],"default")) {
		      ebuf=rec_error_add(ebuf,"Param_change has invalid seed type with index",pnum,tc);
		      delete_event(event_list,event);
		      was_deleted=TRUE;
		    }
		    else {
		      pflags=weed_get_int_value(ptmpls[pnum],"flags",&error);
		      if (pflags&WEED_PARAMETER_REINIT_ON_VALUE_CHANGE&&!
			  is_init_pchange((weed_plant_t *)init_event,event)) {
			// check we are not changing a reinit param, unless we immediately follow the filter_init event
			ebuf=rec_error_add(ebuf,"Param_change sets a reinit parameter",pnum,tc);
			delete_event(event_list,event);
			was_deleted=TRUE;
		      }
		      else {
			// all checks passed
			host_tag_s=weed_get_string_value((weed_plant_t *)init_event,"host_tag",&error);
			host_tag=atoi(host_tag_s);
			weed_free(host_tag_s);
			idx=host_tag-FX_KEYS_MAX_VIRTUAL-1;
			if (pchains[idx][pnum]==init_event) {
			  orig_pchanges=weed_get_voidptr_array((weed_plant_t *)init_event,"in_parameters",&error);
			  in_pchanges=(void **)g_malloc(num_params*sizeof(void *));
			  for (i=0;i<num_params;i++) {
			    if (orig_pchanges[i]==NULL&&i==pnum) in_pchanges[i]=(void *)event;
			    else in_pchanges[i]=orig_pchanges[i];
			  }
			  weed_set_voidptr_array((weed_plant_t *)init_event,"in_parameters",num_params,in_pchanges);
			  g_free(in_pchanges);
			  g_free(orig_pchanges);
			  weed_set_voidptr_value(event,"prev_change",NULL);
			}
			else {
			  weed_set_voidptr_value((weed_plant_t *)pchains[idx][pnum],"next_change",event);
			  weed_set_voidptr_value(event,"prev_change",pchains[idx][pnum]);
			}
			weed_set_voidptr_value(event,"next_change",NULL);
			weed_set_voidptr_value(event,"init_event",init_event);
			pchains[idx][pnum]=event;
		      }}}
		  weed_free(ptmpls);
		}
		weed_free(filter_hash);
	      }}}}}
      break;
    case WEED_EVENT_HINT_FRAME:
      if (tc==last_frame_tc) {
	ebuf=rec_error_add(ebuf,"Duplicate frame event",-1,tc);
	delete_event(event_list,event);
	was_deleted=TRUE;
      }
      else {
	if (!weed_plant_has_leaf(event,"clips")) {
	  weed_set_int_value(event,"clips",-1);
	  weed_set_int_value(event,"frames",0);
	  ebuf=rec_error_add(ebuf,"Frame event missing clips at",-1,tc);
	}

	last_frame_tc=tc;

	num_tracks=weed_leaf_num_elements (event,"clips");
	clip_index=weed_get_int_array(event,"clips",&error);
	frame_index=weed_get_int_array(event,"frames",&error);

	new_clip_index=(int *)g_malloc(num_tracks*sizint);
	new_frame_index=(int *)g_malloc(num_tracks*sizint);
	last_valid_frame=0;
	//	#define DEBUG_MISSING_CLIPS
#ifdef DEBUG_MISSING_CLIPS
	//g_print("pt zzz %d\n",num_tracks);
#endif
	for (i=0;i<num_tracks;i++) {
	  if (clip_index[i]>0&&(clip_index[i]>MAX_FILES||renumbered_clips[clip_index[i]]<1||
				mainw->files[renumbered_clips[clip_index[i]]]==NULL)) {
	    // clip has probably been closed, so we remove its frames
	    new_clip_index[i]=-1;
	    new_frame_index[i]=0;
	    ebuf=rec_error_add(ebuf,"Invalid clip number",clip_index[i],tc);

#ifdef DEBUG_MISSING_CLIPS
	    g_print("found invalid clip number %d on track %d, renumbered_clips=%d\n",clip_index[i],i,
		    renumbered_clips[clip_index[i]]);
#endif
	    missing_clips=TRUE;
	  }
	  else {
	    // take into account the fact that clip could have been resampled since layout was saved
	    if (clip_index[i]>0&&frame_index[i]>0) {
	      new_frame_index[i]=count_resampled_frames(frame_index[i],lfps[renumbered_clips[clip_index[i]]],
							mainw->files[renumbered_clips[clip_index[i]]]->fps);
	      if (new_frame_index[i]>mainw->files[renumbered_clips[clip_index[i]]]->frames) {
		ebuf=rec_error_add(ebuf,"Invalid frame number",new_frame_index[i],tc);
		new_clip_index[i]=-1;
		new_frame_index[i]=0;
		missing_frames=TRUE;
	      }
	      else {
		new_clip_index[i]=clip_index[i];
		new_frame_index[i]=frame_index[i];
		last_valid_frame=i+1;
	      }
	    }
	    else {
	      new_clip_index[i]=clip_index[i];
	      new_frame_index[i]=frame_index[i];
	      last_valid_frame=i+1;
	    }
	  }
	}

	if (last_valid_frame==0) {
	  g_free(new_clip_index);
	  g_free(new_frame_index);
	  new_clip_index=(int *)g_malloc(sizint);
	  new_frame_index=(int *)g_malloc(sizint);
	  *new_clip_index=-1;
	  *new_frame_index=0;
	  num_tracks=1;
	  weed_set_int_array(event,"clips",num_tracks,new_clip_index);
	  weed_set_int_array(event,"frames",num_tracks,new_frame_index);
	}
	else {
	  if (last_valid_frame<num_tracks) {
	    weed_free(clip_index);
	    weed_free(frame_index);
	    clip_index=(int *)weed_malloc(last_valid_frame*sizint);
	    frame_index=(int *)weed_malloc(last_valid_frame*sizint);
	    for (i=0;i<last_valid_frame;i++) {
	      clip_index[i]=new_clip_index[i];
	      frame_index[i]=new_frame_index[i];
	    }
	    num_tracks=last_valid_frame;
	    weed_set_int_array(event,"clips",num_tracks,clip_index);
	    weed_set_int_array(event,"frames",num_tracks,frame_index);
	  }
	  else {
	    weed_set_int_array(event,"clips",num_tracks,new_clip_index);
	    weed_set_int_array(event,"frames",num_tracks,new_frame_index);
	  }
	}


	g_free(new_clip_index);
	weed_free(clip_index);
	g_free(new_frame_index);
	weed_free(frame_index);

	if (weed_plant_has_leaf(event,"audio_clips")) {
	  // check audio clips
	  num_atracks=weed_leaf_num_elements (event,"audio_clips");
	  if (num_atracks%2!=0) {
	    ebuf=rec_error_add(ebuf,"Invalid number of audio_clips",-1,tc);
	    weed_leaf_delete(event,"audio_clips");
	    weed_leaf_delete(event,"audio_seeks");
	  }
	  else {
	    if (!weed_plant_has_leaf(event,"audio_seeks")||weed_leaf_num_elements(event,"audio_seeks")!=num_atracks) {
	      ebuf=rec_error_add(ebuf,"Invalid number of audio_seeks",-1,tc);
	      weed_leaf_delete(event,"audio_clips");
	      weed_leaf_delete(event,"audio_seeks");
	    }
	    else {
	      aclip_index=weed_get_int_array(event,"audio_clips",&error);
	      aseek_index=weed_get_double_array(event,"audio_seeks",&error);
	      new_aclip_index=(int *)g_malloc(num_atracks*sizint);
	      new_aseek_index=(double *)g_malloc(num_atracks*sizdbl);
	      j=0;
	      for (i=0;i<num_atracks;i+=2) {
		if (aclip_index[i+1]>0) {
		  if ((aclip_index[i+1]>MAX_FILES||renumbered_clips[aclip_index[i+1]]<1||
		       mainw->files[renumbered_clips[aclip_index[i+1]]]==NULL)&&aseek_index[i+1]!=0.) {
		    // clip has probably been closed, so we remove its frames
		    ebuf=rec_error_add(ebuf,"Invalid audio clip number",aclip_index[i+1],tc);
		    missing_clips=TRUE;
		  }
		  else {
		    new_aclip_index[j]=aclip_index[i];
		    new_aclip_index[j+1]=aclip_index[i+1];
		    new_aseek_index[j]=aseek_index[i];
		    new_aseek_index[j+1]=aseek_index[i+1];
		    if (aseek_index[j+1]!=0.) add_atrack_to_list(aclip_index[i],aclip_index[i+1]);
		    else remove_atrack_from_list(aclip_index[i]);
		    j+=2;
		  }
		}
		if (aclip_index[i]>-1) {
		  if (mt!=NULL&&!mt->opts.pertrack_audio) {
		    mt->opts.pertrack_audio=TRUE;
		    // enable audio transitions
		    gtk_widget_set_sensitive(mt->fx_region_2a,TRUE);
		    ebuf=rec_error_add(ebuf,"Adding pertrack audio",-1,tc);
		  }
		  else force_pertrack_audio=TRUE;
		  // TODO ** inform user
		}
		if (aclip_index[i]==-1) {
		  if (mt!=NULL&&mt->opts.back_audio_tracks==0) {
		    mt->opts.back_audio_tracks=1;
		    ebuf=rec_error_add(ebuf,"Adding backing audio",-1,tc);
		  }
		  else force_backing_tracks=1;
		  // TODO ** inform user
		}
	      }
	      if (j==0) {
		weed_leaf_delete(event,"audio_clips");
		weed_leaf_delete(event,"audio_seeks");
	      }
	      else {
		weed_set_int_array(event,"audio_clips",j,new_aclip_index);
		weed_set_double_array(event,"audio_seeks",j,new_aseek_index);
	      }
	      weed_free(aclip_index);
	      weed_free(aseek_index);
	      g_free(new_aclip_index);
	      g_free(new_aseek_index);
	    }
	  }
	}
      }
      break;
    case WEED_EVENT_HINT_MARKER:
      // check marker values
      if (!weed_plant_has_leaf(event,"lives_type")) {
	ebuf=rec_error_add(ebuf,"Unknown marker type",-1,tc);
	delete_event(event_list,event);
	was_deleted=TRUE;
      }
      else {
	marker_type=weed_get_int_value(event,"lives_type",&error);
	if (marker_type!=EVENT_MARKER_BLOCK_START&&marker_type!=EVENT_MARKER_BLOCK_UNORDERED&&
	    marker_type!=EVENT_MARKER_RECORD_END&&marker_type!=EVENT_MARKER_RECORD_START) {
	  ebuf=rec_error_add(ebuf,"Unknown marker type",marker_type,tc);
	  delete_event(event_list,event);
	  was_deleted=TRUE;
	}
	if (marker_type==EVENT_MARKER_BLOCK_START&&!weed_plant_has_leaf(event,"tracks")) {
	  ebuf=rec_error_add(ebuf,"Block start marker has no tracks",-1,tc);
	  delete_event(event_list,event);
	  was_deleted=TRUE;
	}
      }
      break;
    default:
      ebuf=rec_error_add(ebuf,"Invalid hint",hint,tc);
      delete_event(event_list,event);
      was_deleted=TRUE;
    }
    if (!was_deleted&&check_filter_map&&last_filter_map!=NULL&&\
	(event_next==NULL||get_event_timecode(event_next)>last_deinit_tc)) {
      // if our last filter_map refers to filter instances which were deinited, we must add another filter_map here
      ebuf=filter_map_check(trans_table,last_filter_map,last_deinit_tc,tc,ebuf);
      check_filter_map=FALSE;
    }
    event=event_next;

    if (!was_deleted) {
      while (cur_tc<last_frame_tc) {
	// add blank frames
	if (!has_frame_event_at(event_list,cur_tc,&shortcut)) {
	  if (shortcut!=NULL) {
	    shortcut=duplicate_frame_at(event_list,shortcut,cur_tc);
	    ebuf=rec_error_add(ebuf,"Duplicated frame at",-1,cur_tc);
	  }
	  else {
	    event_list=insert_blank_frame_event_at(event_list,cur_tc,&shortcut);
	    ebuf=rec_error_add(ebuf,"Inserted missing blank frame",-1,cur_tc);
	  }
	}
	cur_tc+=U_SEC/fps;
	cur_tc=q_gint64(cur_tc,fps);
      }
      last_tc=tc;
    }
  }

  // add any missing filter_deinit events
  ebuf=add_filter_deinits(event_list,trans_table,pchains,last_tc,ebuf);

  // check the last filter map
  if (last_filter_map!=NULL) ebuf=add_null_filter_map(event_list,last_filter_map,last_tc,ebuf);

  last_event=get_last_event(event_list);
  remove_end_blank_frames(event_list);

  if (get_last_event(event_list)!=last_event) {
    last_event=get_last_event(event_list);
    last_tc=get_event_timecode(last_event);
    ebuf=rec_error_add(ebuf,"Removed final blank frames",-1,last_tc);
  }


  // pass 2 - move left any FILTER_DEINITS before the FRAME, move right any FILTER_INITS or PARAM_CHANGES after the FRAME
  // ensure we have at most 1 FILTER_MAP before each FRAME, and 1 FILTER_MAP after a FRAME

  // we do this as a second pass since we may have inserted blank frames
  last_frame_tc=last_filter_map_tc=-1;
  last_frame_event=NULL;

  event=get_first_event(event_list);
  while (event!=NULL) {
    was_moved=FALSE;
    event_next=get_next_event(event);
    tc=get_event_timecode(event);
    hint=get_event_hint(event);
    switch (hint) {
    case WEED_EVENT_HINT_FILTER_INIT:
      if (mt!=NULL&&event!=mt->avol_init_event) {
	if (!move_event_right(event_list,event,last_frame_tc!=tc,fps)) was_moved=TRUE;
      }
      break;
    case WEED_EVENT_HINT_PARAM_CHANGE:
      if (last_frame_tc==tc) if (!move_event_right(event_list,event,FALSE,fps)) was_moved=TRUE;
      break;
    case WEED_EVENT_HINT_FILTER_DEINIT:
      if (mt!=NULL&&weed_get_voidptr_value(event,"init_event",&error)!=mt->avol_init_event) {
	if (!move_event_left(event_list,event,last_frame_tc==tc,fps)) was_moved=TRUE;
      }
      break;
    case WEED_EVENT_HINT_FILTER_MAP:
      if (last_filter_map_tc==tc) {
	// remove last filter_map
	ebuf=rec_error_add(ebuf,"Duplicate filter maps",-1,tc);
	delete_event(event_list,last_filter_map);
      }
      last_filter_map_tc=tc;
      last_filter_map=event;
      break;
    case WEED_EVENT_HINT_FRAME:
      last_frame_tc=tc;
      last_filter_map_tc=-1;
      last_frame_event=event;
      break;
    }
    if (was_moved) {
      if (last_frame_event!=NULL) event=last_frame_event;
      else event=get_first_event(event_list);
    }
    else event=event_next;
  }

  add_missing_atrack_closers(event_list,fps,ebuf);

  if (mt==NULL) transient=mainw->LiVES;
  else transient=mt->window;

  if (missing_clips&&missing_frames) {
    bit2=g_strdup(_("clips and frames"));
  }
  else {
    if (missing_clips) {
      bit2=g_strdup(_("clips"));
    }
    else if (missing_frames) {
      bit2=g_strdup(_("frames"));
    }
  }

  end_threaded_dialog();

  if (bit2!=NULL) {
    if (mt!=NULL&&mt->auto_reloading) {
      g_free(bit1);
      g_free(bit3);
      bit1=g_strdup(_("\nAuto reload layout.\n"));
      bit3=g_strdup_printf("\n%s",prefs->ar_layout_name);
    }
    msg=g_strdup_printf(_("%s\nSome %s are missing from the layout%s\nTherefore it could not be loaded properly.\n"),
			bit1,bit2,bit3);
    do_error_dialog_with_check_transient(msg,TRUE,0,GTK_WINDOW(transient));
    g_free(msg);
    g_free(bit2);
    if (mt!=NULL) mt->layout_prompt=TRUE;
  }
  g_free(bit1);
  g_free(bit3);


  g_free(ebuf); // TODO - allow option of viewing/saving this

  return TRUE;
}



weed_plant_t *load_event_list(lives_mt *mt, gchar *eload_file) {
  // load (deserialise) a serialised event_list
  // after loading we perform sophisticated checks on it to detect
  // and try to repair any errors in it

  int fd;

  gchar *filt[]={"*.lay",NULL};
  gchar *startdir=NULL;
  gchar *eload_dir;
  gchar *msg;
  gchar *com;
  gchar *eload_name;
  gboolean free_eload_file=TRUE;
  gint old_avol_fx=mt->avol_fx;

  weed_plant_t *event_list=NULL;

  int num_events=0;
  int retval2;
  gboolean orig_ar_layout=prefs->ar_layout,ar_layout;
  gboolean retval=TRUE;

  GtkWidget *ar_checkbutton;
  GtkWidget *eventbox;
  GtkWidget *label;
  GtkWidget *hbox;

  if (eload_file==NULL) {
    if (!strlen(mainw->set_name)) {
      LIVES_ERROR("Loading event list for unknown set");
      return NULL;
    }

    eload_dir=g_build_filename(prefs->tmpdir,mainw->set_name,"layouts/",NULL);

    mainw->com_failed=FALSE;
    com=g_strdup_printf ("/bin/mkdir -p \"%s\"",eload_dir);
    lives_system (com,FALSE);
    g_free (com);
    
    if (mainw->com_failed) {
      g_free(eload_dir);
      return NULL;
    }
    
    if (mt->idlefunc>0) {
      g_source_remove(mt->idlefunc);
      mt->idlefunc=0;
    }
    
    if (!mainw->recoverable_layout&&!g_file_test(eload_dir,G_FILE_TEST_IS_DIR)) {
      g_free(eload_dir);
      mt->idlefunc=mt_idle_add(mt);
      return NULL;
    }

    startdir=g_strdup(eload_dir);

    ar_checkbutton = gtk_check_button_new ();
    gtk_widget_show(ar_checkbutton);
    eventbox=gtk_event_box_new();
    gtk_widget_show(eventbox);
    label=gtk_label_new_with_mnemonic (_("_Autoreload each time"));
    gtk_widget_show(label);
    gtk_label_set_mnemonic_widget (GTK_LABEL (label),ar_checkbutton);
    
    gtk_container_add(GTK_CONTAINER(eventbox),label);
    g_signal_connect (GTK_OBJECT (eventbox), "button_press_event",
		      G_CALLBACK (label_act_toggle),
		      ar_checkbutton);
    
    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
      gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
      gtk_widget_modify_bg (eventbox, GTK_STATE_NORMAL, &palette->normal_back);
    }
    
    hbox = gtk_hbox_new (FALSE, 0);
    gtk_widget_show(hbox);
    
    gtk_box_pack_start (GTK_BOX (hbox), ar_checkbutton, FALSE, FALSE, 10);
    gtk_box_pack_start (GTK_BOX (hbox), eventbox, FALSE, FALSE, 10);
    GTK_WIDGET_SET_FLAGS (ar_checkbutton, GTK_CAN_DEFAULT|GTK_CAN_FOCUS);
    
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ar_checkbutton),prefs->ar_layout);
    g_signal_connect (GTK_OBJECT (ar_checkbutton), "toggled",
		      G_CALLBACK (on_autoreload_toggled),
		      GINT_TO_POINTER(2));
    eload_file=choose_file(startdir,NULL,filt,GTK_FILE_CHOOSER_ACTION_OPEN,NULL,hbox);

    g_free(startdir);
  }
  else free_eload_file=FALSE;

  ar_layout=prefs->ar_layout;
  prefs->ar_layout=orig_ar_layout;

  if (eload_file==NULL) {
    // if the user cancelled see if we can clear the directories
    // this will fail if there are any files in the directories

    gchar *cdir;
    com=g_strdup_printf("/bin/rmdir \"%s\" 2>/dev/null",eload_dir);
    lives_system(com,TRUE);
    g_free(com);

    cdir=g_build_filename(prefs->tmpdir,mainw->set_name,NULL);
    com=g_strdup_printf("/bin/rmdir \"%s\" 2>/dev/null",cdir);
    lives_system(com,TRUE);
    g_free(com);

    g_free(eload_dir);
    mt->idlefunc=mt_idle_add(mt);
    return NULL;
  }

  if (free_eload_file) g_free(eload_dir);

  if (!mainw->recoverable_layout) eload_name=g_strdup(eload_file);
  else eload_name=g_strdup(_("auto backup"));

  if ((fd=open(eload_file,O_RDONLY))<0) {
    msg=g_strdup_printf(_("\nUnable to load layout file %s\n"),eload_name);
    do_error_dialog_with_check_transient(msg,TRUE,0,GTK_WINDOW(mt->window));
    g_free(msg);
    g_free(eload_name);
    return NULL;
  }

  event_list_free_undos(mt);

  if (mainw->event_list!=NULL) {
    event_list_free(mt->event_list);
    mt->event_list=NULL;
    mt_clear_timeline(mt);
  }

  msg=g_strdup_printf(_("Loading layout from %s..."),eload_name);
  mainw->no_switch_dprint=TRUE;
  d_print(msg);
  mainw->no_switch_dprint=FALSE;
  g_free(msg);

  mt_desensitise(mt);

  mainw->read_failed=FALSE;

  do {
    retval=0;
    if ((event_list=load_event_list_inner(mt,fd,TRUE,&num_events,NULL,NULL))==NULL) {
      close(fd);
      
      if (mainw->read_failed) {
	retval=do_read_failed_error_s_with_retry(eload_name,NULL,NULL);
	mainw->read_failed=FALSE;
      }
      
      if (retval!=LIVES_RETRY) {
	if (mt->is_ready) mt_sensitise(mt);
	g_free(eload_name);
	return NULL;
      }
    }
    else close(fd);
  } while (retval==LIVES_RETRY);

  g_free(eload_name);

  d_print_done();

  msg=g_strdup_printf(_("Got %d events...processing..."),num_events);
  d_print(msg);
  g_free(msg);

  mt->auto_changed=mt->changed=mainw->recoverable_layout;

  if (mt->idlefunc>0) g_source_remove(mt->idlefunc);
  while (g_main_context_iteration(NULL,FALSE));
  if (!mainw->recoverable_layout) mt->idlefunc=mt_idle_add(mt);

  cfile->progress_start=1;
  cfile->progress_end=num_events;

  // event list loaded, now we set the pointers for filter_map (init_events), param_change (init_events and param chains), 
  // filter_deinit (init_events)
  do_threaded_dialog(_("Checking and rebuilding event list"),FALSE);

  elist_errors=0;

  if (!mainw->recoverable_layout) {
    // re-map clips so our loaded event_list refers to the correct clips and frames
    rerenumber_clips(eload_file);
  }
  else {
    renumber_from_backup_layout_numbering(mt);
  }

  mt->avol_init_event=NULL;
  mt->avol_fx=-1;

  if (!event_list_rectify(mt,event_list)) {
    event_list_free(event_list);
    event_list=NULL;
  }

  if (get_first_event(event_list)==NULL) {
    event_list_free(event_list);
    event_list=NULL;
  }

  if (event_list!=NULL) {
    msg=g_strdup_printf(_("%d errors detected.\n"),elist_errors);
    d_print(msg);
    g_free(msg);
    if (!mt->auto_reloading) {
      if (!mt->layout_prompt||do_mt_rect_prompt()) {

	do {
	  retval2=0;
	  retval=TRUE;

	  // resave with corrections/updates
	  fd=creat(eload_file,DEF_FILE_PERMS);
	  if (fd>=0) {
	    retval=save_event_list_inner(NULL,fd,event_list,NULL);
	    close(fd);
	  }

	  if (fd<0||!retval) {
	    retval2=do_write_failed_error_s_with_retry(eload_file,(fd<0)?g_strerror(errno):NULL,NULL);
	    if (retval2==LIVES_CANCEL) d_print_file_error_failed();
	  }
	} while (retval2==LIVES_RETRY);
      }
    }
  }
  else d_print_failed();

  mt->layout_prompt=FALSE;

  if (mt->avol_fx==-1&&mainw->fx_candidates[FX_CANDIDATE_AUDIO_VOL].delegate!=-1) {
    // user (or system) has delegated an audio volume filter from the candidates
    mt->avol_fx=GPOINTER_TO_INT(g_list_nth_data(mainw->fx_candidates[FX_CANDIDATE_AUDIO_VOL].list,
						mainw->fx_candidates[FX_CANDIDATE_AUDIO_VOL].delegate));
  }

  if (mt->avol_fx!=old_avol_fx&&mt->aparam_view_list!=NULL) {
    // audio volume effect changed, so we reset which parameters are viewed
    g_list_free(mt->aparam_view_list);
    mt->aparam_view_list=NULL;
  }

  if (event_list!=NULL) {
    if (!mainw->recoverable_layout) {
      g_snprintf(mt->layout_name,256,"%s",eload_file);
      get_basename(mt->layout_name);
    }

    if (mt->layout_set_properties) msg=mt_set_vals_string();
    else msg=g_strdup_printf(_("Multitrack fps set to %.3f\n"),cfile->fps);
    d_print(msg);
    g_free(msg);

    set_mt_title(mt);

    if (!ar_layout) {
      prefs->ar_layout=FALSE;
      set_pref("ar_layout","");
      memset(prefs->ar_layout_name,0,1);
    }
    else {
      prefs->ar_layout=TRUE;
      set_pref("ar_layout",mt->layout_name);
      g_snprintf(prefs->ar_layout_name,128,"%s",mt->layout_name);
    }

  }

  set_audio_filter_channel_values(mt);

  if (mt->opts.back_audio_tracks>0) {
    gtk_widget_show(mt->view_audio);
  }

  if (free_eload_file) g_free(eload_file);

  mt->idlefunc=0;
  if (!mainw->recoverable_layout) mt->idlefunc=mt_idle_add(mt);

  return (event_list);

}


void remove_markers(weed_plant_t *event_list) {
  weed_plant_t *event=get_first_event(event_list);
  weed_plant_t *event_next;
  int marker_type;
  int error;

  while (event!=NULL) {
    event_next=get_next_event(event);
    if (WEED_EVENT_IS_MARKER(event)) {
      marker_type=weed_get_int_value(event,"lives_type",&error);
      if (marker_type==EVENT_MARKER_BLOCK_START||marker_type==EVENT_MARKER_BLOCK_UNORDERED) {
	delete_event(event_list,event);
      }
    }
    event=event_next;
  }
}



void wipe_layout(lives_mt *mt) {

  mt_desensitise(mt);

 if (mt->idlefunc>0) {
    g_source_remove(mt->idlefunc);
    mt->idlefunc=0;
  }

  d_print(_("Layout was wiped.\n"));

  close_scrap_file();
  close_ascrap_file();

  recover_layout_cancelled(NULL,NULL);

  if (strlen(mt->layout_name)>0&&!strcmp(mt->layout_name,prefs->ar_layout_name)) {
    set_pref("ar_layout","");
    memset(prefs->ar_layout_name,0,1);
    prefs->ar_layout=FALSE;
  }
  
  event_list_free(mt->event_list);
  mt->event_list=NULL;

  event_list_free_undos(mt);

  mt_clear_timeline(mt);

  mt_sensitise(mt);

  mt->idlefunc=mt_idle_add(mt);
}




void on_clear_event_list_activate (GtkMenuItem *menuitem, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  _entryw *cdsw;
  gint resp=2;
  gboolean rev_resp=FALSE;

  if (mt->idlefunc>0) {
    g_source_remove(mt->idlefunc);
    mt->idlefunc=0;
  }

  if (strlen(mt->layout_name)>0) {
    cdsw=create_cds_dialog(2);
    rev_resp=TRUE;
  }
  else {
    cdsw=create_cds_dialog(3);
    rev_resp=TRUE;
  }


  do {
    mainw->cancelled=CANCEL_NONE;
    resp=gtk_dialog_run(GTK_DIALOG(cdsw->dialog));

    if (((resp==1&&!rev_resp)||(resp==2&&rev_resp))&&strlen(mt->layout_name)==0) {
      // save
      on_save_event_list_activate(NULL,mt);
      if (mainw->cancelled==CANCEL_NONE) break;
    }

  } while (((resp==1&&!rev_resp)||(resp==2&&rev_resp))&&strlen(mt->layout_name)==0);

  gtk_widget_destroy(cdsw->dialog);
  g_free(cdsw);

  if (resp==0) {
    mt->idlefunc=mt_idle_add(mt);
    return; // cancel
  }

  if (((resp==1&&!rev_resp)||(resp==2&&rev_resp))&&strlen(mt->layout_name)>0) {
    // delete from disk
    GList *layout_map=NULL;
    gchar *lmap_file;
    if (!do_warning_dialog_with_check_transient("\nLayout will be deleted from the disk.\nAre you sure ?\n",0,
						GTK_WINDOW(mt->window))) {
      mt->idlefunc=mt_idle_add(mt);
      return;
    }

    lmap_file=g_build_filename("tmpdir",mainw->set_name,"layouts",mt->layout_name,NULL);
    layout_map=g_list_append(layout_map,lmap_file);
    remove_layout_files(layout_map);
    g_free(lmap_file);
  }

  // wipe
  wipe_layout(mt);

}



static void set_audio_mixer_vols(lives_mt *mt, weed_plant_t *elist) {
  gint natracks,navols;
  int *atracks;
  double *avols;
  gint catracks=g_list_length(mt->audio_vols);
  int error,i,xtrack,xavol;

  if (!weed_plant_has_leaf(elist,"audio_volume_tracks")||!weed_plant_has_leaf(elist,"audio_volume_values")) return;

  natracks=weed_leaf_num_elements(elist,"audio_volume_tracks");
  navols=weed_leaf_num_elements(elist,"audio_volume_values");

  atracks=weed_get_int_array(elist,"audio_volume_tracks",&error);
  if (error!=WEED_NO_ERROR) return;

  avols=weed_get_double_array(elist,"audio_volume_values",&error);
  if (error!=WEED_NO_ERROR) {
    weed_free(atracks);
    return;
  }

  for (i=0;i<natracks;i++) {
    xtrack=atracks[i];
    if (xtrack<-mt->opts.back_audio_tracks) continue;
    if (xtrack>=catracks-mt->opts.back_audio_tracks) continue;

    xavol=i;
    if (xavol>=navols) {
      mt->opts.gang_audio=TRUE;
      xavol=navols-1;
    }
    set_mixer_track_vol(mt,xtrack+mt->opts.back_audio_tracks,avols[xavol]);
  }

  weed_free(atracks);
  weed_free(avols);
}




void on_load_event_list_activate (GtkMenuItem *menuitem, gpointer user_data) {
  int i;
  lives_mt *mt=(lives_mt *)user_data;
  weed_plant_t *new_event_list;
  gchar *eload_file=NULL;

  if (!check_for_layout_del(mt,FALSE)) return;

  if (mt->idlefunc>0) {
    g_source_remove(mt->idlefunc);
    mt->idlefunc=0;
  }

  new_event_list=load_event_list(mt,eload_file);
  if (eload_file!=NULL) g_free(eload_file);

  recover_layout_cancelled(NULL,NULL);

  if (new_event_list==NULL) {
    mt_sensitise(mt);
    mt->idlefunc=mt_idle_add(mt);
    return;
  }

  if (mt->event_list!=NULL) event_list_free(mt->event_list);
  mt->event_list=NULL;

  mt->undo_buffer_used=0;
  mt->undo_offset=0;
  g_list_free(mt->undos);
  mt->undos=NULL;
  mt_set_undoable(mt,MT_UNDO_NONE,NULL,FALSE);
  mt_set_redoable(mt,MT_UNDO_NONE,NULL,FALSE);

  for (i=0;i<mt->num_video_tracks;i++) {
    delete_video_track(mt,i,FALSE);
  }
  g_list_free(mt->video_draws);
  mt->video_draws=NULL;
  mt->num_video_tracks=0;

  if (mt->amixer!=NULL) on_amixer_close_clicked(NULL,mt);

  delete_audio_tracks(mt,mt->audio_draws,FALSE);
  mt->audio_draws=NULL;

  if (mt->audio_vols!=NULL) g_list_free(mt->audio_vols);
  mt->audio_vols=NULL;

  mt->event_list=new_event_list;

  if (mt->selected_tracks!=NULL) g_list_free(mt->selected_tracks);
  mt->selected_tracks=NULL;

  mt_init_tracks(mt,TRUE);

  if (!mt->ignore_load_vals) set_audio_mixer_vols(mt,mt->event_list);

  add_aparam_menuitems(mt);

  unselect_all(mt);
  remove_markers(mt->event_list);
  mt_sensitise(mt);
  mt_show_current_frame(mt, FALSE);

  mt->idlefunc=mt_idle_add(mt);
}



void migrate_layouts (const gchar *old_set_name, const gchar *new_set_name) {
  // if we change the name of a set, we must also update the layouts - at the very least 2 things need to happen
  // 1) the "needs_set" leaf in each layout must be updated
  // 2) the layouts will be physically moved, so if appending we check for name collisions
  // 3) the names of layouts in mainw->affected_layouts_map must be altered

  // here we also update mainw->current_layouts_map and the layout_maps for each clip

  // this last may not be necessary as we are probably closing the set


  // on return from here we physically move the layouts, and we append the layout_map to the new one



  // load each event_list in mainw->layout_map_list
  GList *map=mainw->current_layouts_map;
  int fd;
  int i;
  int retval2=0;
  weed_plant_t *event_list;
  gchar *tmp;
  gboolean retval=TRUE;

  gchar *changefrom=NULL;
  size_t chlen;

  // TODO - dirsep

  if (old_set_name!=NULL) {
    changefrom=g_build_filename(prefs->tmpdir,old_set_name,"layouts/",NULL);
    chlen=strlen(changefrom);
  }
  else chlen=0;

  while (map!=NULL) {
    if (old_set_name!=NULL) {
      // load and save each layout, updating the "needs_set" leaf
      do {
	retval2=0;
	if ((fd=open((gchar *)map->data,O_RDONLY))>-1) {
	  if ((event_list=load_event_list_inner(NULL,fd,FALSE,NULL,NULL,NULL))!=NULL) {
	    close (fd);
	    // adjust the value of "needs_set" to new_set_name
	    weed_set_string_value(event_list,"needs_set",new_set_name);
	    // save the event_list with the same name
	    unlink((char *)map->data);
	    
	    do {
	      retval2=0;
	      fd=creat((char *)map->data,DEF_FILE_PERMS);
	      if (fd>=0) retval=save_event_list_inner(NULL,fd,event_list,NULL);
	      if (fd<0||!retval) {
		if (fd>0) close(fd);
		retval2=do_write_failed_error_s_with_retry((char *)map->data,(fd<0)?g_strerror(errno):NULL,NULL);
	      }
	    } while (retval2==LIVES_RETRY);
	    
	    event_list_free(event_list);
	  }
	  close(fd);
	}
	else {
	  retval2=do_read_failed_error_s_with_retry((char *)map->data,NULL,NULL);
	}
	
      } while (retval2==LIVES_RETRY);
    }

    if (old_set_name!=NULL&&!strncmp((char *)map->data,changefrom,chlen)) {
      // update entries in mainw->current_layouts_map
      tmp=g_build_filename(prefs->tmpdir,new_set_name,"layouts",(char *)map->data+chlen,NULL);
      if (g_file_test(tmp,G_FILE_TEST_EXISTS)) {
	gchar *com;
	// prevent duplication of layouts
	g_free(tmp);
	tmp=g_strdup_printf("%s/%s/layouts/%s-%s",prefs->tmpdir,new_set_name,old_set_name,(char *)map->data+chlen);
	com=g_strdup_printf("/bin/mv \"%s\" \"%s\"",(gchar *)map->data,tmp);
	lives_system(com,FALSE);
	g_free(com);
      }
      g_free(map->data);
      map->data=tmp;
    }
    map=map->next;
  }

  // update layout_map's in mainw->files
  for (i=1;i<=MAX_FILES;i++) {
    if (mainw->files[i]!=NULL) {
      if (mainw->files[i]->layout_map!=NULL) {
	map=mainw->files[i]->layout_map;
	while (map!=NULL) {
	  if (map->data!=NULL) {
	    if ((old_set_name!=NULL&&!strncmp((char *)map->data,changefrom,chlen))||
		(old_set_name==NULL&&(strstr((char *)map->data,new_set_name)==NULL))) {

	      gchar **array=g_strsplit((gchar *)map->data,"|",-1);
	      size_t origlen=strlen(array[0]);
	      gchar *tmp2=g_build_filename(prefs->tmpdir,new_set_name,"layouts",array[0]+chlen,NULL);
	      if (g_file_test(tmp2,G_FILE_TEST_EXISTS)) {
		tmp2=g_strdup_printf("%s/%s/layouts/%s-%s",prefs->tmpdir,new_set_name,old_set_name,array[0]+chlen);
	      }
	      tmp=g_strdup_printf("%s%s",tmp2,(gchar *)map->data+origlen);
	      g_free(tmp2);
	      g_strfreev(array);


	      g_free(map->data);
	      map->data=tmp;
	    }
	    map=map->next;
	  }
	}
      }
    }
  }

  // update mainw->affected_layouts_map
  map=mainw->affected_layouts_map; 
  while (map!=NULL) {
    if ((old_set_name!=NULL&&!strncmp((char *)map->data,changefrom,chlen))||
	(old_set_name==NULL&&(strstr((char *)map->data,new_set_name)==NULL))) {
      if (strcmp(mainw->cl_string,(char *)map->data+chlen)) {
	tmp=g_build_filename(prefs->tmpdir,new_set_name,"layouts",(char *)map->data+chlen,NULL);
	if (g_file_test(tmp,G_FILE_TEST_EXISTS)) {
	  g_free(tmp);
	  tmp=g_strdup_printf("%s/%s/layouts/%s-%s",prefs->tmpdir,new_set_name,old_set_name,(char *)map->data+chlen);
	}
	g_free(map->data);
	map->data=tmp;
      }
    }
    map=map->next;
  }
  if (changefrom!=NULL) g_free(changefrom);
}



GList *layout_frame_is_affected(gint clipno, gint frame) {
  // return list of names of layouts which are affected, or NULL
  // list and list->data should be freed after use

  gchar **array;
  GList *lmap=mainw->files[clipno]->layout_map;
  gdouble orig_fps;
  gint resampled_frame;

  if (mainw->stored_event_list!=NULL&&mainw->files[clipno]->stored_layout_frame!=0) {
    // see if it affects the current layout
    resampled_frame=count_resampled_frames(mainw->files[clipno]->stored_layout_frame,mainw->files[clipno]->stored_layout_fps,mainw->files[clipno]->fps);
    if (frame<=resampled_frame) mainw->xlays=g_list_append_unique(mainw->xlays,mainw->cl_string);
  }

  while (lmap!=NULL) {
    array=g_strsplit((char *)lmap->data,"|",-1);
    if (atoi(array[2])!=0) {
      orig_fps=strtod(array[3],NULL);
      resampled_frame=count_resampled_frames(atoi(array[2]),orig_fps,mainw->files[clipno]->fps);
      if (array[2]==0) resampled_frame=0;
      if (frame<=resampled_frame) {
	mainw->xlays=g_list_append_unique(mainw->xlays,array[0]);
      }
    }
    g_strfreev(array);
    lmap=lmap->next;
  }

  return mainw->xlays;
}



GList *layout_audio_is_affected(gint clipno, gdouble time) {
  gchar **array;
  GList *lmap=mainw->files[clipno]->layout_map;
  gdouble max_time;

  if (mainw->files[clipno]->arate==0) return mainw->xlays;

  // adjust time depending on if we have stretched audio
  time*=mainw->files[clipno]->arps/mainw->files[clipno]->arate;

  if (mainw->stored_event_list!=NULL) {
    // see if it affects the current layout
    if (mainw->files[clipno]->stored_layout_audio>0.&&time<=mainw->files[clipno]->stored_layout_audio) mainw->xlays=g_list_append_unique(mainw->xlays,mainw->cl_string);
  }

  while (lmap!=NULL) {
    if (get_token_count((char *)lmap->data,'|')<5) continue;
    array=g_strsplit((char *)lmap->data,"|",-1);
    max_time=strtod(array[4],NULL);
    if (max_time>0.&&time<=max_time) {
      mainw->xlays=g_list_append_unique(mainw->xlays,array[0]);
    }
    g_strfreev(array);
    lmap=lmap->next;
  }

  return mainw->xlays;
}


void mt_change_disp_tracks_ok                (GtkButton     *button,
					      gpointer         user_data) {
  
  lives_mt *mt=(lives_mt *)user_data;
  gtk_widget_destroy(gtk_widget_get_toplevel(GTK_WIDGET(button)));
  mt->max_disp_vtracks=mainw->fx1_val;
  scroll_tracks(mt,mt->top_track);
}



///////////////////////////////////////////////

void show_frame_events_activate (GtkMenuItem *menuitem, gpointer user_data) {
  prefs->event_window_show_frame_events=!prefs->event_window_show_frame_events;
}

void mt_change_max_disp_tracks (GtkMenuItem *menuitem, gpointer user_data) {
  GtkWidget *dialog;
  lives_mt *mt=(lives_mt *)user_data;

  mainw->fx1_val=mt->max_disp_vtracks;
  dialog=create_cdtrack_dialog(3,mt);
  gtk_widget_show(dialog);

}

void mt_load_vals_toggled (GtkMenuItem *menuitem, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  mt->ignore_load_vals=!mt->ignore_load_vals;
}

void mt_change_vals_activate (GtkMenuItem *menuitem, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  gboolean response;
  gchar *msg;

  rdet=create_render_details(4);  // WARNING !! - rdet is global in events.h
  gtk_widget_show_all(rdet->always_hbox);
  rdet->enc_changed=FALSE;
  do {
    rdet->suggestion_followed=FALSE;
    if ((response=gtk_dialog_run(GTK_DIALOG(rdet->dialog)))==GTK_RESPONSE_OK) {
      if (rdet->enc_changed) {
	check_encoder_restrictions(FALSE,FALSE,TRUE);
      }
    }
  } while (rdet->suggestion_followed);
  
  xarate=(gint)atoi (gtk_entry_get_text(GTK_ENTRY(resaudw->entry_arate)));
  xachans=(gint)atoi (gtk_entry_get_text(GTK_ENTRY(resaudw->entry_achans)));
  xasamps=(gint)atoi (gtk_entry_get_text(GTK_ENTRY(resaudw->entry_asamps)));
  
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(resaudw->rb_unsigned))) {
    xse=AFORM_UNSIGNED;;
  }
  else cfile->signed_endian=0;
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(resaudw->rb_bigend))) {
    xse|=AFORM_BIG_ENDIAN;
  }
  
  if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(resaudw->aud_checkbutton))) {
    xachans=0;
  }

  
  if (response==GTK_RESPONSE_CANCEL) {
    gtk_widget_destroy (rdet->dialog); 
    g_free(rdet->encoder_name);
    g_free(rdet);
    rdet=NULL;
    if (resaudw!=NULL) g_free(resaudw);
    resaudw=NULL;
    return;
  }

  if (xachans==0&&mt->audio_draws!=NULL) {
    GList *slist=mt->audio_draws;
    while (slist!=NULL) {
      if (g_object_get_data(G_OBJECT(slist->data),"blocks")!=NULL) {
	do_mt_no_audchan_error();
	gtk_widget_destroy (rdet->dialog); 
	g_free(rdet->encoder_name);
	g_free(rdet);
	rdet=NULL;
	if (resaudw!=NULL) g_free(resaudw);
	resaudw=NULL;
	return;
      }
      slist=slist->next;
    }
  }

  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(rdet->always_checkbutton))) {
    prefs->mt_enter_prompt=FALSE;
    set_boolean_pref("mt_enter_prompt",prefs->mt_enter_prompt);
    prefs->mt_def_width=rdet->width;
    set_int_pref("mt_def_width",prefs->mt_def_width);
    prefs->mt_def_height=rdet->height;
    set_int_pref("mt_def_height",prefs->mt_def_height);
    prefs->mt_def_fps=rdet->fps;
    set_double_pref("mt_def_fps",prefs->mt_def_fps);
    prefs->mt_def_arate=xarate;
    set_int_pref("mt_def_arate",prefs->mt_def_arate);
    prefs->mt_def_achans=xachans;
    set_int_pref("mt_def_achans",prefs->mt_def_achans);
    prefs->mt_def_asamps=xasamps;
    set_int_pref("mt_def_asamps",prefs->mt_def_asamps);
    prefs->mt_def_signed_endian=xse;
    set_int_pref("mt_def_signed_endian",prefs->mt_def_signed_endian);
    prefs->mt_pertrack_audio=ptaud;
    set_boolean_pref("mt_pertrack_audio",prefs->mt_pertrack_audio);
    prefs->mt_backaudio=btaud;
    set_int_pref("mt_backaudio",prefs->mt_backaudio);
  }
  else {
    if (!prefs->mt_enter_prompt) {
      prefs->mt_enter_prompt=TRUE;
      set_boolean_pref("mt_enter_prompt",prefs->mt_enter_prompt);
    }
  }

  gtk_widget_destroy (rdet->dialog); 

  mt->user_width=rdet->width;
  mt->user_height=rdet->height;
  mt->user_fps=rdet->fps;
  mt->user_arate=xarate;
  mt->user_achans=xachans;
  mt->user_asamps=xasamps;
  mt->user_signed_endian=xse;
  

  g_free(rdet->encoder_name);
  g_free(rdet);
  rdet=NULL;
  if (resaudw!=NULL) g_free(resaudw);
  resaudw=NULL;

  msg=set_values_from_defs(mt,FALSE);
  if (msg!=NULL) {
    d_print(msg);
    g_free(msg);

    set_mt_title(mt);
  }

  if (cfile->achans==0) {
    gtk_widget_hide(mt->render_sep);
    gtk_widget_hide(mt->render_vid);
    gtk_widget_hide(mt->render_aud);
    gtk_widget_hide(mt->normalise_aud);
    gtk_widget_hide(mt->view_audio);
    delete_audio_tracks(mt,mt->audio_draws,FALSE);
    mt->audio_draws=NULL;

    if (mt->amixer!=NULL) on_amixer_close_clicked(NULL,mt);

    if (mt->audio_vols!=NULL) g_list_free(mt->audio_vols);
    mt->audio_vols=NULL;
  }
  else {
    gtk_widget_show(mt->render_sep);
    gtk_widget_show(mt->render_vid);
    gtk_widget_show(mt->render_aud);
    gtk_widget_show(mt->normalise_aud);
    gtk_widget_show(mt->view_audio);
  }
  scroll_tracks(mt,mt->top_track);
}


guint event_list_get_byte_size(lives_mt *mt, weed_plant_t *event_list,int *num_events) {
  // return serialisation size
  int i,j;
  guint tot=0;
  weed_plant_t *event=get_first_event(event_list);
  gchar **leaves;
  int ne;
  int tot_events=0;

  // write extra bits in event_list
  save_event_list_inner(mt,-1,event_list,NULL);

  while (event!=NULL) {
    if (WEED_EVENT_IS_FILTER_INIT(event)) weed_set_voidptr_value(event,"event_id",(void *)event);
    tot_events++;
    leaves=weed_plant_list_leaves(event);
    tot+=sizint; //number of leaves
    for (i=0;leaves[i]!=NULL;i++) {
      tot+=sizint*3+strlen(leaves[i]); // key_length, seed_type, num_elements
      ne=weed_leaf_num_elements(event,leaves[i]);
      // sum data_len + data
      for (j=0;j<ne;j++) tot+=sizint+weed_leaf_element_size(event,leaves[i],j);
      weed_free(leaves[i]);
    }
    weed_free(leaves);
    event=get_next_event(event);
  }

  event=event_list;
  leaves=weed_plant_list_leaves(event);
  tot+=sizint;
  for (i=0;leaves[i]!=NULL;i++) {
    tot+=sizint*3+strlen(leaves[i]);
    ne=weed_leaf_num_elements(event,leaves[i]);
    // sum data_len + data
    for (j=0;j<ne;j++) tot+=sizint+weed_leaf_element_size(event,leaves[i],j);
    weed_free(leaves[i]);
  }
  weed_free(leaves);

  if (num_events!=NULL) *num_events=tot_events;
  return tot;
}


void on_amixer_close_clicked (GtkButton *button, lives_mt *mt) {
  lives_amixer_t *amixer=mt->amixer;
  int i;
  gdouble val;

  mt->opts.gang_audio=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(amixer->gang_checkbutton));

  // set vols from slider vals

  for (i=0;i<amixer->nchans;i++) {
#if ENABLE_GIW
    if (prefs->lamp_buttons) {
      val=giw_vslider_get_value(GIW_VSLIDER(amixer->ch_sliders[i]));
    }
    else {
#endif
      val=gtk_range_get_value(GTK_RANGE(amixer->ch_sliders[i]));
#if ENABLE_GIW
    }
#endif
    set_mixer_track_vol(mt,i,val);
  }

  gtk_widget_destroy(gtk_widget_get_toplevel(GTK_WIDGET(button)));
  g_free(amixer->ch_sliders);
  g_free(amixer->ch_slider_fns);
  g_free(amixer);
  mt->amixer=NULL;
  if (mt->audio_vols_back!=NULL) g_list_free(mt->audio_vols_back);
  //gtk_widget_set_sensitive(mt->prerender_aud,TRUE);

}


static void on_amixer_reset_clicked (GtkButton *button, lives_mt *mt) {
  lives_amixer_t *amixer=mt->amixer;
  int i;

  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(amixer->inv_checkbutton),FALSE);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(amixer->gang_checkbutton),mt->opts.gang_audio);

  // copy vols to slider vals

  for (i=0;i<amixer->nchans;i++) {

#if ENABLE_GIW
    if (prefs->lamp_buttons) {
      g_signal_handler_block(giw_vslider_get_adjustment(GIW_VSLIDER(amixer->ch_sliders[i])),amixer->ch_slider_fns[i]);
      giw_vslider_set_value(GIW_VSLIDER(amixer->ch_sliders[i]),(gdouble)GPOINTER_TO_INT
			    (g_list_nth_data(mt->audio_vols_back,i))/LIVES_AVOL_SCALE);
      g_signal_handler_unblock(giw_vslider_get_adjustment(GIW_VSLIDER(amixer->ch_sliders[i])),amixer->ch_slider_fns[i]);
    }
    else {
#endif
      g_signal_handler_block(gtk_range_get_adjustment(GTK_RANGE(amixer->ch_sliders[i])),amixer->ch_slider_fns[i]);
      gtk_range_set_value(GTK_RANGE(amixer->ch_sliders[i]),(gdouble)GPOINTER_TO_INT
			  (g_list_nth_data(mt->audio_vols_back,i))/LIVES_AVOL_SCALE);
      g_signal_handler_unblock(gtk_range_get_adjustment(GTK_RANGE(amixer->ch_sliders[i])),amixer->ch_slider_fns[i]);
#if ENABLE_GIW
    }
#endif
  }

}


static void after_amixer_gang_toggled (GtkToggleButton *toggle, lives_amixer_t *amixer) {
  gtk_widget_set_sensitive(amixer->inv_checkbutton,(gtk_toggle_button_get_active(toggle)));
  if (prefs->lamp_buttons) {
    if (gtk_toggle_button_get_active(toggle)) gtk_widget_modify_bg(GTK_WIDGET(toggle), GTK_STATE_PRELIGHT, &palette->light_green);
    else gtk_widget_modify_bg(GTK_WIDGET(toggle), GTK_STATE_PRELIGHT, &palette->dark_red);
  }
}


static void after_amixer_inv_toggled (GtkToggleButton *toggle, lives_amixer_t *amixer) {
  if (prefs->lamp_buttons) {
    if (gtk_toggle_button_get_active(toggle)) gtk_widget_modify_bg(GTK_WIDGET(toggle), GTK_STATE_PRELIGHT, &palette->light_green);
    else gtk_widget_modify_bg(GTK_WIDGET(toggle), GTK_STATE_PRELIGHT, &palette->dark_red);
  }
}


void on_amixer_slider_changed (GtkAdjustment *adj, lives_mt *mt) {
  lives_amixer_t *amixer=mt->amixer;
  gint layer=GPOINTER_TO_INT(g_object_get_data(G_OBJECT(adj),"layer"));
  gboolean gang=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(amixer->gang_checkbutton));
  gboolean inv=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(amixer->inv_checkbutton));
  gdouble val;
  int i;

#if ENABLE_GIW
  if (prefs->lamp_buttons) {
    GiwVSlider *slider=GIW_VSLIDER(amixer->ch_sliders[layer]);
    val=giw_vslider_get_value(slider);
  }
  else {
#endif
    if (TRUE) {
      GtkRange *range=GTK_RANGE(amixer->ch_sliders[layer]);
      val=gtk_range_get_value(range);
    }
#if ENABLE_GIW
  }
#endif

  if (gang) {
    if (layer>0) {
      for (i=mt->opts.back_audio_tracks;i<amixer->nchans;i++) {
#if ENABLE_GIW
	if (prefs->lamp_buttons) {
	  g_signal_handler_block(giw_vslider_get_adjustment(GIW_VSLIDER(amixer->ch_sliders[i])),amixer->ch_slider_fns[i]);
	  giw_vslider_set_value(GIW_VSLIDER(amixer->ch_sliders[i]),val);
	  g_signal_handler_unblock(giw_vslider_get_adjustment(GIW_VSLIDER(amixer->ch_sliders[i])),amixer->ch_slider_fns[i]);
	}
	else {
#endif
	  g_signal_handler_block(gtk_range_get_adjustment(GTK_RANGE(amixer->ch_sliders[i])),amixer->ch_slider_fns[i]);
	  gtk_range_set_value(GTK_RANGE(amixer->ch_sliders[i]),val);
	  g_signal_handler_unblock(gtk_range_get_adjustment(GTK_RANGE(amixer->ch_sliders[i])),amixer->ch_slider_fns[i]);
#if ENABLE_GIW
	}
#endif
      }
      if (inv&&mt->opts.back_audio_tracks>0) {
#if ENABLE_GIW
	if (prefs->lamp_buttons) {
	  g_signal_handler_block(giw_vslider_get_adjustment(GIW_VSLIDER(amixer->ch_sliders[0])),amixer->ch_slider_fns[0]);
	  giw_vslider_set_value(GIW_VSLIDER(amixer->ch_sliders[0]),1.-val<0.?0.:1.-val);
	  g_signal_handler_unblock(giw_vslider_get_adjustment(GIW_VSLIDER(amixer->ch_sliders[0])),amixer->ch_slider_fns[0]);
	}
	else {
#endif
	  g_signal_handler_block(gtk_range_get_adjustment(GTK_RANGE(amixer->ch_sliders[0])),amixer->ch_slider_fns[0]);
	  gtk_range_set_value(GTK_RANGE(amixer->ch_sliders[0]),1.-val);
	  g_signal_handler_unblock(gtk_range_get_adjustment(GTK_RANGE(amixer->ch_sliders[0])),amixer->ch_slider_fns[0]);
#if ENABLE_GIW
	}
#endif
      }
    }
    else {
      if (inv) {
	for (i=1;i<amixer->nchans;i++) {
#if ENABLE_GIW
	  if (prefs->lamp_buttons) {
	    g_signal_handler_block(giw_vslider_get_adjustment(GIW_VSLIDER(amixer->ch_sliders[i])),amixer->ch_slider_fns[i]);
	    giw_vslider_set_value(GIW_VSLIDER(amixer->ch_sliders[i]),1.-val<0.?0.:1.-val);
	    g_signal_handler_unblock(giw_vslider_get_adjustment(GIW_VSLIDER(amixer->ch_sliders[i])),amixer->ch_slider_fns[i]);
	  }
	  else {
#endif
	    g_signal_handler_block(gtk_range_get_adjustment(GTK_RANGE(amixer->ch_sliders[i])),amixer->ch_slider_fns[i]);
	    gtk_range_set_value(GTK_RANGE(amixer->ch_sliders[i]),1.-val);
	    g_signal_handler_unblock(gtk_range_get_adjustment(GTK_RANGE(amixer->ch_sliders[i])),amixer->ch_slider_fns[i]);
#if ENABLE_GIW
	  }
#endif
	  
	}
      }
    }
  }

  if (!mt->is_rendering) {
    set_mixer_track_vol(mt,layer,val);
  }

}






GtkWidget * amixer_add_channel_slider (lives_mt *mt, gint i) {
  // add a slider to audio mixer for layer i; i<0 are backing audio tracks 
  // automatically sets the track name and layer number

  GObject *adj;
  GtkWidget *spinbutton;
  GtkWidget *label;
  GtkWidget *vbox;
  lives_amixer_t *amixer=mt->amixer;
  gchar *tname;

  i+=mt->opts.back_audio_tracks;

  adj = (GObject *)gtk_adjustment_new (0.5, 0., 4., 0.01, 0.1, 0.);
    
  spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (adj), 0.1, 3);

#if ENABLE_GIW
  if (prefs->lamp_buttons) {
    amixer->ch_sliders[i]=giw_vslider_new(GTK_ADJUSTMENT(adj));
    giw_vslider_set_legends_digits(GIW_VSLIDER(amixer->ch_sliders[i]),1);
    giw_vslider_set_major_ticks_number(GIW_VSLIDER(amixer->ch_sliders[i]),5);
    giw_vslider_set_minor_ticks_number(GIW_VSLIDER(amixer->ch_sliders[i]),4);
    if (palette->style&STYLE_1) {
      gtk_widget_modify_bg(amixer->ch_sliders[i], GTK_STATE_NORMAL, &palette->normal_back);
    }
  }
  else {
#endif
    amixer->ch_sliders[i]=gtk_vscale_new(GTK_ADJUSTMENT(adj));
    gtk_range_set_inverted(GTK_RANGE(amixer->ch_sliders[i]),TRUE);
    gtk_scale_set_digits(GTK_SCALE(amixer->ch_sliders[i]),2);
    gtk_scale_set_value_pos(GTK_SCALE(amixer->ch_sliders[i]),GTK_POS_BOTTOM);
#if ENABLE_GIW
  }
#endif
  
  g_object_set_data(G_OBJECT(amixer->ch_sliders[i]),"adj",adj);
  g_object_set_data(G_OBJECT(adj),"layer",GINT_TO_POINTER(i));
    
  amixer->ch_slider_fns[i]=g_signal_connect_after (GTK_OBJECT (adj), "value_changed",
						   G_CALLBACK (on_amixer_slider_changed),
						   (gpointer)mt);
  
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(amixer->ch_sliders[i], GTK_STATE_NORMAL, &palette->normal_fore);
  }
  
  tname=get_track_name(mt,i-mt->opts.back_audio_tracks,TRUE);
  label=gtk_label_new(tname);
  g_free(tname);

  g_object_set_data(G_OBJECT(amixer->ch_sliders[i]),"label",label);
  
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  
  vbox = gtk_vbox_new (FALSE, 15);
  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 10);
  gtk_box_pack_start (GTK_BOX (vbox), amixer->ch_sliders[i], TRUE, TRUE, 50);
  gtk_box_pack_start (GTK_BOX (vbox), spinbutton, FALSE, FALSE, 10);

  amixer->nchans++;

  return vbox;
}















void amixer_show (GtkButton *button, gpointer user_data) {
  lives_mt *mt=(lives_mt *)user_data;
  GtkWidget *amixerw;
  GtkWidget *top_vbox;
  GtkWidget *vbox;
  GtkWidget *vbox2;
  GtkWidget *hbox;
  GtkWidget *hbuttonbox;
  GtkWidget *scrolledwindow;
  GtkWidget *label;
  GtkWidget *eventbox;
  GtkWidget *close_button;
  GtkWidget *reset_button;
  GtkAccelGroup *accel_group=GTK_ACCEL_GROUP(gtk_accel_group_new ());

  int nachans=g_list_length(mt->audio_draws);

  int winsize_h,scr_width=mainw->scr_width;
  int winsize_v,scr_height=mainw->scr_height;

  int i;

  lives_amixer_t *amixer;

  if (nachans==0) return;

  mt->audio_vols_back=g_list_copy(mt->audio_vols);

  amixer=mt->amixer=(lives_amixer_t *)g_malloc(sizeof(lives_amixer_t));
  amixer->nchans=0;

  amixer->ch_sliders=(GtkWidget **)g_malloc(nachans*sizeof(GtkWidget *));
  amixer->ch_slider_fns=(gulong *)g_malloc(nachans*sizeof(gulong));

  if (prefs->gui_monitor!=0) {
    scr_width=mainw->mgeom[prefs->gui_monitor-1].width;
    scr_height=mainw->mgeom[prefs->gui_monitor-1].height;
  }

  winsize_h=scr_width*2/3;
  winsize_v=scr_height*2/3;

  amixerw = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_modify_bg(amixerw, GTK_STATE_NORMAL, &palette->menu_and_bars);
  gtk_window_set_title (GTK_WINDOW (amixerw), _("LiVES: Multitrack audio mixer"));

  top_vbox = gtk_vbox_new (FALSE, 15);

  amixer->main_hbox = gtk_hbox_new (FALSE, 20);

  scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  
  gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scrolledwindow), amixer->main_hbox);
  
  if (prefs->gui_monitor!=0) {
    gint xcen=mainw->mgeom[prefs->gui_monitor-1].x+(mainw->mgeom[prefs->gui_monitor-1].width-amixerw->allocation.width)/2;
    gint ycen=mainw->mgeom[prefs->gui_monitor-1].y+(mainw->mgeom[prefs->gui_monitor-1].height-amixerw->allocation.height)/2;
    gtk_window_set_screen(GTK_WINDOW(amixerw),mainw->mgeom[prefs->gui_monitor-1].screen);
    gtk_window_move(GTK_WINDOW(amixerw),xcen,ycen);
  }

  if (prefs->open_maximised) {
    gtk_window_maximize (GTK_WINDOW(amixerw));
  }
  else gtk_window_set_default_size (GTK_WINDOW (amixerw), winsize_h, winsize_v);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(gtk_bin_get_child (GTK_BIN (scrolledwindow)), GTK_STATE_NORMAL, &palette->normal_back);
  }
  
  gtk_viewport_set_shadow_type (GTK_VIEWPORT (gtk_bin_get_child (GTK_BIN (scrolledwindow))),GTK_SHADOW_IN);

  gtk_box_pack_start (GTK_BOX (top_vbox), scrolledwindow, TRUE, TRUE, 0);
  gtk_container_add (GTK_CONTAINER (amixerw), top_vbox);

  hbuttonbox = gtk_hbutton_box_new ();
  gtk_box_pack_start (GTK_BOX (top_vbox), hbuttonbox, FALSE, TRUE, 20);
  gtk_button_box_set_child_size (GTK_BUTTON_BOX (hbuttonbox), DEF_BUTTON_WIDTH, -1);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (hbuttonbox), GTK_BUTTONBOX_SPREAD);

  reset_button = gtk_button_new_with_mnemonic (_("_Reset values"));
  gtk_container_add (GTK_CONTAINER (hbuttonbox), reset_button);
  GTK_WIDGET_SET_FLAGS (reset_button, GTK_CAN_DEFAULT);

  close_button = gtk_button_new_with_mnemonic (_("_Close mixer"));
  gtk_container_add (GTK_CONTAINER (hbuttonbox), close_button);
  GTK_WIDGET_SET_FLAGS (close_button, GTK_CAN_DEFAULT);


  gtk_widget_add_accelerator (close_button, "clicked", accel_group,
                              GDK_m, GDK_CONTROL_MASK,
                              (GtkAccelFlags)0);

  gtk_window_add_accel_group (GTK_WINDOW (amixerw), accel_group);

  if (mt->opts.back_audio_tracks>0) {
    vbox=amixer_add_channel_slider(mt,-1);
    gtk_box_pack_start (GTK_BOX (amixer->main_hbox), vbox, FALSE, FALSE, 10);
  }

  vbox2 = gtk_vbox_new (FALSE, 15);
  gtk_box_pack_start (GTK_BOX (amixer->main_hbox), vbox2, FALSE, FALSE, 10);

  add_fill_to_box(GTK_BOX(vbox2));

  vbox = gtk_vbox_new (FALSE, 15);
  gtk_box_pack_start (GTK_BOX (vbox2), vbox, TRUE, TRUE, 10);
  
    
  if (prefs->lamp_buttons) {
    amixer->inv_checkbutton = gtk_check_button_new_with_label (" ");
    gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(amixer->inv_checkbutton),FALSE);
    gtk_widget_modify_bg(amixer->inv_checkbutton, GTK_STATE_ACTIVE, &palette->light_green);
    gtk_widget_modify_bg(amixer->inv_checkbutton, GTK_STATE_NORMAL, &palette->dark_red);

    g_signal_connect_after (GTK_OBJECT (amixer->inv_checkbutton), "toggled",
			    G_CALLBACK (after_amixer_inv_toggled),
			    (gpointer)amixer);

    after_amixer_inv_toggled(GTK_TOGGLE_BUTTON(amixer->inv_checkbutton),amixer);

  }
  else amixer->inv_checkbutton = gtk_check_button_new ();


  if (mt->opts.back_audio_tracks>0&&mt->opts.pertrack_audio) {
    label=gtk_label_new_with_mnemonic(_("_Invert backing audio\nand layer volumes"));
    
    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    }
    
    gtk_widget_set_tooltip_text( amixer->inv_checkbutton, _("Adjust backing and layer audio values so that they sum to 1.0"));
    eventbox=gtk_event_box_new();
    gtk_tooltips_copy(eventbox,amixer->inv_checkbutton);
    gtk_label_set_mnemonic_widget (GTK_LABEL (label),amixer->inv_checkbutton);
    
    gtk_container_add(GTK_CONTAINER(eventbox),label);
    g_signal_connect (GTK_OBJECT (eventbox), "button_press_event",
		      G_CALLBACK (label_act_toggle),
		      amixer->inv_checkbutton);
    
    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
      gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
      gtk_widget_modify_bg (eventbox, GTK_STATE_NORMAL, &palette->normal_back);
    }
    
    gtk_box_pack_start (GTK_BOX (vbox), eventbox, FALSE, FALSE, 10);
    
    hbox = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 10);
    add_fill_to_box(GTK_BOX(hbox));
    
    gtk_box_pack_start (GTK_BOX (hbox), amixer->inv_checkbutton, FALSE, FALSE, 0);
    GTK_WIDGET_SET_FLAGS (amixer->inv_checkbutton, GTK_CAN_DEFAULT|GTK_CAN_FOCUS);

    add_fill_to_box(GTK_BOX(hbox));
  }


  if (prefs->lamp_buttons) {
    amixer->gang_checkbutton = gtk_check_button_new_with_label (" ");
    gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(amixer->gang_checkbutton),FALSE);
    gtk_widget_modify_bg(amixer->gang_checkbutton, GTK_STATE_ACTIVE, &palette->light_green);
    gtk_widget_modify_bg(amixer->gang_checkbutton, GTK_STATE_NORMAL, &palette->dark_red);
  }
  else amixer->gang_checkbutton = gtk_check_button_new ();


  if (mt->opts.pertrack_audio) {
    label=gtk_label_new_with_mnemonic(_("_Gang layer audio"));
    
    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    }

    gtk_widget_set_tooltip_text( amixer->gang_checkbutton, _("Adjust all layer audio values to the same value"));
    eventbox=gtk_event_box_new();
    gtk_tooltips_copy(eventbox,amixer->gang_checkbutton);
    gtk_label_set_mnemonic_widget (GTK_LABEL (label),amixer->gang_checkbutton);
    
    gtk_container_add(GTK_CONTAINER(eventbox),label);
    g_signal_connect (GTK_OBJECT (eventbox), "button_press_event",
		      G_CALLBACK (label_act_toggle),
		      amixer->gang_checkbutton);
    
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(amixer->gang_checkbutton),mt->opts.gang_audio);
    
    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
      gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
      gtk_widget_modify_bg (eventbox, GTK_STATE_NORMAL, &palette->normal_back);
    }
    
    hbox = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_end (GTK_BOX (vbox), hbox, FALSE, FALSE, 10);
    add_fill_to_box(GTK_BOX(hbox));
    
    gtk_box_pack_start (GTK_BOX (hbox), amixer->gang_checkbutton, FALSE, FALSE, 10);
    add_fill_to_box(GTK_BOX(hbox));

    gtk_box_pack_end (GTK_BOX (vbox), eventbox, FALSE, FALSE, 10);
    GTK_WIDGET_SET_FLAGS (amixer->gang_checkbutton, GTK_CAN_DEFAULT|GTK_CAN_FOCUS);
  }

  add_fill_to_box(GTK_BOX(vbox2));

  for (i=0;i<nachans-mt->opts.back_audio_tracks;i++) {
    vbox=amixer_add_channel_slider(mt,i);
    gtk_box_pack_start (GTK_BOX (amixer->main_hbox), vbox, FALSE, FALSE, 10);
  }

  g_signal_connect (GTK_OBJECT (close_button), "clicked",
		    G_CALLBACK (on_amixer_close_clicked),
		    (gpointer)mt);

  g_signal_connect (GTK_OBJECT (reset_button), "clicked",
		    G_CALLBACK (on_amixer_reset_clicked),
		    (gpointer)mt);

  gtk_widget_add_accelerator (close_button, "activate", accel_group,
                              GDK_Escape, (GdkModifierType)0, (GtkAccelFlags)0);



  g_signal_connect_after (GTK_OBJECT (amixer->gang_checkbutton), "toggled",
			  G_CALLBACK (after_amixer_gang_toggled),
			  (gpointer)amixer);

  after_amixer_gang_toggled(GTK_TOGGLE_BUTTON(amixer->gang_checkbutton),amixer);

  gtk_widget_grab_focus (close_button);

  on_amixer_reset_clicked(NULL,mt);

  gtk_widget_show_all(amixerw);

}


void on_mt_showkeys_activate (GtkMenuItem *menuitem, gpointer user_data) {
  do_mt_keys_window();
}

static GtkWidget *get_eventbox_for_track(lives_mt *mt, int ntrack) {
  GtkWidget *eventbox;
  if (mt_track_is_video(mt,ntrack)) {
    eventbox=(GtkWidget *)g_list_nth_data(mt->video_draws, ntrack);
  }
  else if (mt_track_is_audio(mt,ntrack)) {
    eventbox=(GtkWidget *)g_list_nth_data(mt->audio_draws, 1-ntrack);
  }
  else return NULL;
  return eventbox;
}


static track_rect *get_nth_block_for_track(lives_mt *mt, int itrack, int iblock) {
  int count=0;
  track_rect *block;
  GtkWidget *eventbox=get_eventbox_for_track(mt,itrack);
  if (eventbox==NULL) return NULL; //<invalid track
  block=(track_rect *)g_object_get_data (G_OBJECT(eventbox), "blocks");
  while(block!=NULL) {
    if (count==iblock) return block;
    block=block->next;
    count++;
  }

  return NULL; ///<invalid block
}



// remote API helpers

gboolean mt_track_is_video(lives_mt *mt, int ntrack) {
  if (ntrack>=0&&mt->video_draws!=NULL&&ntrack<g_list_length(mt->video_draws)) return TRUE;
  return FALSE;
}


gboolean mt_track_is_audio(lives_mt *mt, int ntrack) {
  if (ntrack<=0&&mt->audio_draws!=NULL&&ntrack>=-(g_list_length(mt->audio_draws))) return TRUE;
  return FALSE;
}

/*
gboolean mt_track_is_valid(lives_mt *mt, int itrack) {
  if (!mt_track_is_video(itrack)&&!mt_track_is_audio(itrack)) return FALSE;
  return TRUE;
  }*/

gint mt_get_last_block_number(lives_mt *mt, int ntrack) {
  int count=0;
  track_rect *block,*lastblock;
  GtkWidget *eventbox=get_eventbox_for_track(mt,ntrack);
  if (eventbox==NULL) return -1; //<invalid track
  lastblock=(track_rect *)g_object_get_data (G_OBJECT(eventbox),"block_last");
  if (lastblock==NULL) return -1; ///< no blocks in track
  block=(track_rect *)g_object_get_data (G_OBJECT(eventbox), "blocks");
  while(block!=NULL) {
    if (block==lastblock) break;
    block=block->next;
    count++;
  }

  return count;
}



gint mt_get_block_count(lives_mt *mt, int ntrack) {
  int count=0;
  track_rect *block,*lastblock;
  GtkWidget *eventbox=get_eventbox_for_track(mt,ntrack);
  if (eventbox==NULL) return -1; //<invalid track
  lastblock=(track_rect *)g_object_get_data (G_OBJECT(eventbox),"block_last");
  if (lastblock==NULL) return -1; ///< no blocks in track
  block=(track_rect *)g_object_get_data (G_OBJECT(eventbox), "blocks");
  while(block!=NULL) {
    if (block==lastblock) break;
    block=block->next;
    count++;
  }

  return count;
}


/// return time in seconds of first frame event in block
gdouble mt_get_block_sttime(lives_mt *mt, int ntrack, int iblock) {
  track_rect *block=get_nth_block_for_track(mt,ntrack,iblock);
  if (block==NULL) return -1; ///< invalid track or block number
  return (gdouble)get_event_timecode(block->start_event)/U_SEC;
}


/// return time in seconds of last frame event in block, + event duration
gdouble mt_get_block_entime(lives_mt *mt, int ntrack, int iblock) {
  track_rect *block=get_nth_block_for_track(mt,ntrack,iblock);
  if (block==NULL) return -1; ///< invalid track or block number
  return (gdouble)get_event_timecode(block->end_event)/U_SEC+1./mt->fps;
}



////////////////////////////////////
// autotransitions
//



void mt_do_autotransition(lives_mt *mt, track_rect *block) {
  // prefs->atrans_track0 should be the output track (usually the lower of the two)

  track_rect *oblock=NULL;
  weed_timecode_t sttc,endtc=0;
  int nvids=g_list_length(mt->video_draws);

  gdouble region_start=mt->region_start;
  gdouble region_end=mt->region_end;
  GList *slist;
  int current_fx=mt->current_fx;
  weed_plant_t *old_mt_init=mt->init_event;
  gboolean did_backup=mt->did_backup;

  //weed_plant_t *deinit_event;
  weed_plant_t *stevent,*enevent;
  weed_plant_t *filter;
  weed_plant_t **ptmpls;
  weed_plant_t *ptm;
  weed_plant_t **oparams;

  int error;
  int tparam;
  int nparams;
  int param_hint;
  int track;
  int i;

  if (block==NULL) return;  ///<invalid block

  filter=get_weed_filter(prefs->atrans_fx);
  if (!weed_plant_has_leaf(filter,"in_parameter_templates")) return; ///<filter has no in parameters

  tparam=get_transition_param(filter);
  if (tparam==-1) return; ///< filter has no transition parameter

  ptmpls=weed_get_plantptr_array(filter,"in_parameter_templates",&error);
  ptm=ptmpls[tparam];
  param_hint=weed_get_int_value(ptm,"hint",&error);

  mt->current_fx=prefs->atrans_fx;

  sttc=get_event_timecode(block->start_event);

  track=get_track_for_block(block);

  // part 1 - transition in

  slist=g_list_copy(mt->selected_tracks);
  
  if (mt->selected_tracks!=NULL) {
    g_list_free(mt->selected_tracks);
    mt->selected_tracks=NULL;
  }

  for (i=0;i<nvids;i++) {
    if (i==track) continue; ///< cannot transition with self !
    oblock=get_block_from_time((GtkWidget *)g_list_nth_data(mt->video_draws,i),
			       (gdouble)sttc/U_SEC+0.5/mt->fps,mt);
    
    if (oblock!=NULL) {
      if (get_event_timecode(oblock->end_event)<=get_event_timecode(block->end_event)) {
	endtc=q_gint64(get_event_timecode(oblock->end_event)+U_SEC/mt->fps,mt->fps);
	break;
      }
      else oblock=NULL;
    }
  }

  if (!did_backup&&mt->idlefunc>0) {
    // freeze auto backups
    g_source_remove(mt->idlefunc);
    mt->idlefunc=0;
  }

  mt->is_atrans=TRUE; ///< force some visual changes

  if (oblock!=NULL) {
    mt->selected_tracks=g_list_append(mt->selected_tracks,GINT_TO_POINTER(track));
    mt->selected_tracks=g_list_append(mt->selected_tracks,GINT_TO_POINTER(i));

    mt->did_backup=TRUE;
    mt->region_start=sttc/U_SEC;
    mt->region_end=endtc/U_SEC;
    mt_add_region_effect(NULL, mt);
    
    nparams=weed_leaf_num_elements(mt->init_event,"in_parameters");
    oparams=(weed_plant_t **)weed_get_voidptr_array(mt->init_event,"in_parameters",&error);

    for (i=0;i<nparams;i++) {
      if (weed_get_int_value(oparams[i],"index",&error)==tparam) break;
    }

    stevent=oparams[i];

    enevent=weed_plant_new(WEED_PLANT_EVENT);
    weed_set_int_value(enevent,"hint",WEED_EVENT_HINT_PARAM_CHANGE);
    weed_set_int64_value(enevent,"timecode",endtc);
    weed_set_int_value(enevent,"index",tparam);
  
    weed_set_voidptr_value(enevent,"init_event",mt->init_event);
    weed_set_voidptr_value(enevent,"next_change",NULL);
    weed_set_voidptr_value(enevent,"prev_change",stevent);
    weed_add_plant_flags(enevent,WEED_LEAF_READONLY_PLUGIN);

    weed_set_voidptr_value(stevent,"next_change",enevent);

    if (param_hint==WEED_HINT_INTEGER) {
      int min=weed_get_int_value(ptm,"min",&error);
      int max=weed_get_int_value(ptm,"max",&error);
      weed_set_int_value(stevent,"value",i<track?min:max);
      weed_set_int_value(enevent,"value",i<track?max:min);
    }
    else {
      double min=weed_get_double_value(ptm,"min",&error);
      double max=weed_get_double_value(ptm,"max",&error);
      weed_set_double_value(stevent,"value",i<track?min:max);
      weed_set_double_value(enevent,"value",i<track?max:min);
    }

    insert_param_change_event_at(mt->event_list,oblock->end_event,enevent);
    weed_free(oparams);

  }

  // part 2, check if there is a transition out

  oblock=NULL;
  endtc=q_gint64(get_event_timecode(block->end_event)+U_SEC/mt->fps,mt->fps);

  if (mt->selected_tracks!=NULL) {
    g_list_free(mt->selected_tracks);
    mt->selected_tracks=NULL;
  }

  for (i=0;i<nvids;i++) {
    if (i==track) continue; ///< cannot transition with self !
    oblock=get_block_from_time((GtkWidget *)g_list_nth_data(mt->video_draws,i),
			       (gdouble)endtc/U_SEC+0.5/mt->fps,mt);

    if (oblock!=NULL) {
      sttc=get_event_timecode(oblock->start_event);
      if (sttc<get_event_timecode(block->start_event)) oblock=NULL;
      else break;
    }
  }


  if (oblock!=NULL) {
    mt->selected_tracks=g_list_append(mt->selected_tracks,GINT_TO_POINTER(track));
    mt->selected_tracks=g_list_append(mt->selected_tracks,GINT_TO_POINTER(i));

    mt->did_backup=TRUE;
    mt->region_start=sttc/U_SEC;
    mt->region_end=endtc/U_SEC;
    mt_add_region_effect(NULL, mt);

    nparams=weed_leaf_num_elements(mt->init_event,"in_parameters");
    oparams=(weed_plant_t **)weed_get_voidptr_array(mt->init_event,"in_parameters",&error);

    for (i=0;i<nparams;i++) {
      if (weed_get_int_value(oparams[i],"index",&error)==tparam) break;
    }

    stevent=oparams[i];

    enevent=weed_plant_new(WEED_PLANT_EVENT);
    weed_set_int_value(enevent,"hint",WEED_EVENT_HINT_PARAM_CHANGE);
    weed_set_int64_value(enevent,"timecode",get_event_timecode(block->end_event));
    weed_set_int_value(enevent,"index",tparam);
  
    weed_set_voidptr_value(enevent,"init_event",mt->init_event);
    weed_set_voidptr_value(enevent,"next_change",NULL);
    weed_set_voidptr_value(enevent,"prev_change",stevent);
    weed_add_plant_flags(enevent,WEED_LEAF_READONLY_PLUGIN);

    weed_set_voidptr_value(stevent,"next_change",enevent);

    if (param_hint==WEED_HINT_INTEGER) {
      int min=weed_get_int_value(ptm,"min",&error);
      int max=weed_get_int_value(ptm,"max",&error);
      weed_set_int_value(stevent,"value",i<track?max:min);
      weed_set_int_value(enevent,"value",i<track?min:max);
    }
    else {
      double min=weed_get_double_value(ptm,"min",&error);
      double max=weed_get_double_value(ptm,"max",&error);
      weed_set_double_value(stevent,"value",i<track?max:min);
      weed_set_double_value(enevent,"value",i<track?min:max);
    }

    insert_param_change_event_at(mt->event_list,block->end_event,enevent);
    weed_free(oparams);
  }

  mt->is_atrans=FALSE;
  mt->region_start=region_start;
  mt->region_end=region_end;
  g_list_free(mt->selected_tracks);
  mt->selected_tracks=g_list_copy(slist);
  if (slist!=NULL) g_list_free(slist);
  mt->current_fx=current_fx;
  mt->init_event=old_mt_init;

  weed_free(ptmpls);

  mt->did_backup=did_backup;
  if (!did_backup) mt->idlefunc=mt_idle_add(mt);

}

