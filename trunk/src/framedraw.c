// framedraw.c
// LiVES
// (c) G. Finch (salsaman@xs4all.nl,salsaman@gmail.com) 2002 - 2012
// see file COPYING for licensing details : released under the GNU GPL 3 or later

// functions for the 'framedraw' widget - lets users draw on frames :-)

#ifdef HAVE_SYSTEM_WEED
#include <weed/weed-palettes.h>
#else
#include "../libweed/weed-palettes.h"
#endif

#include "main.h"
#include "callbacks.h"
#include "support.h"
#include "interface.h"
#include "effects.h"
#include "cvirtual.h"

// set by mouse button press
static gint xstart,ystart;
static gboolean b1_held;

static boolean noupdate=FALSE;

static gdouble calc_fd_scale(gint width, gint height) {
  gdouble scale=1.;

 if (width<MIN_PRE_X) {
    width=MIN_PRE_X;
  }
  if (height<MIN_PRE_Y) {
    height=MIN_PRE_Y;
  }

  if (width>MAX_PRE_X) scale=(gdouble)width/(gdouble)MAX_PRE_X;
  if (height>MAX_PRE_Y&&(height/MAX_PRE_Y>scale)) scale=(gdouble)height/(gdouble)MAX_PRE_Y;
  return scale;

}

static void start_preview (GtkButton *button, lives_rfx_t *rfx) {
  int i;
  gchar *com;

  gtk_widget_set_sensitive(mainw->framedraw_preview,FALSE);
  while (g_main_context_iteration(NULL,FALSE));

  if (mainw->did_rfx_preview) {
#ifndef IS_MINGW
    com=g_strdup_printf("%s stopsubsub \"%s\" 2>/dev/null",prefs->backend_sync,cfile->handle);
    lives_system(com,TRUE); // try to stop any in-progress preview
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

    if (cfile->start==0) {
      cfile->start=1;
      cfile->end=cfile->frames;
    }

    do_rfx_cleanup(rfx);
  }

#ifndef IS_MINGW
  com=g_strdup_printf("%s clear_pre_files \"%s\" 2>/dev/null",prefs->backend_sync,cfile->handle);
#else
  com=g_strdup_printf("%s clear_pre_files \"%s\" 2>NUL",prefs->backend_sync,cfile->handle);
#endif
  lives_system(com,TRUE); // clear any .pre files from before

  for (i=0;i<rfx->num_params;i++) {
    rfx->params[i].changed=FALSE;
  }

  mainw->cancelled=CANCEL_NONE;
  mainw->error=FALSE;

  // within do_effect() we check and if  
  do_effect(rfx,TRUE); // actually start effect processing in the background

  gtk_widget_set_sensitive(mainw->framedraw_spinbutton,TRUE);
  gtk_widget_set_sensitive(mainw->framedraw_scale,TRUE);

  if (mainw->framedraw_frame>cfile->start&&!(cfile->start==0&&mainw->framedraw_frame==1)) 
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(mainw->framedraw_spinbutton),cfile->start);
  else {
    load_rfx_preview(rfx);
  }

  mainw->did_rfx_preview=TRUE;
}




void framedraw_connect_spinbutton(lives_special_framedraw_rect_t *framedraw, lives_rfx_t *rfx) {
  framedraw->rfx=rfx;

  g_signal_connect_after (GTK_OBJECT (mainw->framedraw_spinbutton), "value_changed",
			  G_CALLBACK (after_framedraw_frame_spinbutton_changed),
			  framedraw);

}



void framedraw_connect(lives_special_framedraw_rect_t *framedraw, gint width, gint height, lives_rfx_t *rfx) {


  // add mouse fn's so we can draw on frames
  g_signal_connect (GTK_OBJECT (mainw->framedraw), "motion_notify_event",
		    G_CALLBACK (on_framedraw_mouse_update),
		    framedraw);
  g_signal_connect (GTK_OBJECT (mainw->framedraw), "button_release_event",
		    G_CALLBACK (on_framedraw_mouse_reset),
		    framedraw);
  g_signal_connect (GTK_OBJECT (mainw->framedraw), "button_press_event",
		    G_CALLBACK (on_framedraw_mouse_start),
		    framedraw);
  g_signal_connect (GTK_OBJECT(mainw->framedraw), "enter-notify-event",G_CALLBACK (on_framedraw_enter),framedraw);
  g_signal_connect (GTK_OBJECT(mainw->framedraw), "leave-notify-event",G_CALLBACK (on_framedraw_leave),framedraw);

  framedraw_connect_spinbutton(framedraw,rfx);

  gtk_widget_modify_bg (mainw->fd_frame, GTK_STATE_NORMAL, &palette->light_red);
  framedraw_redraw(framedraw, TRUE, NULL);
}


void framedraw_add_label(GtkVBox *box) {
  GtkWidget *label;

  // TRANSLATORS - Preview refers to preview window; keep this phrase short
  label=gtk_label_new(_("You can click in Preview to change these values"));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);
}


void framedraw_add_reset(GtkVBox *box, lives_special_framedraw_rect_t *framedraw) {
  GtkWidget *hbox_rst;
 
  framedraw_add_label(box);

  mainw->framedraw_reset = gtk_button_new_from_stock ("gtk-refresh");
  hbox_rst = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (box), hbox_rst, FALSE, FALSE, 0);
  
  gtk_button_set_label (GTK_BUTTON (mainw->framedraw_reset),_ ("_Reset Values"));
  gtk_button_set_use_underline (GTK_BUTTON (mainw->framedraw_reset), TRUE);
  gtk_box_pack_start (GTK_BOX (hbox_rst), mainw->framedraw_reset, TRUE, FALSE, 0);
  gtk_widget_set_sensitive (mainw->framedraw_reset,FALSE);
  
  g_signal_connect (mainw->framedraw_reset, "clicked",G_CALLBACK (on_framedraw_reset_clicked),framedraw);
}


static gboolean expose_fd_event (GtkWidget *widget, GdkEventExpose ev) {
  load_framedraw_image(NULL);
  redraw_framedraw_image();
  return TRUE;
}

void widget_add_framedraw (GtkVBox *box, gint start, gint end, gboolean add_preview_button, gint width, gint height) {
  // adds the frame draw widget to box
  // the redraw button should be connected to an appropriate redraw function
  // after calling this function

  // an example of this is in 'trim frames'

  GtkWidget *vseparator;
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *label;
  GObject *spinbutton_adj;
  GtkWidget *label2;
  GtkWidget *frame;
 
  lives_rfx_t *rfx;

  double fd_scale;

  b1_held=FALSE;

  mainw->framedraw_reset=NULL;

  vseparator = gtk_vseparator_new ();
  gtk_box_pack_start (GTK_BOX (lives_widget_get_parent(LIVES_WIDGET (box))), vseparator, FALSE, FALSE, 0);
  gtk_widget_show (vseparator);

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (lives_widget_get_parent(LIVES_WIDGET (box))), vbox, FALSE, FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 10);

  fd_scale=calc_fd_scale(width,height);
  width/=fd_scale;
  height/=fd_scale;
 
  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
  add_fill_to_box(GTK_BOX(hbox));
  frame = gtk_frame_new (NULL);
  gtk_widget_set_size_request (frame, width, height);
  add_fill_to_box(GTK_BOX(hbox));
  gtk_box_pack_start (GTK_BOX (hbox), frame, FALSE, FALSE, 0);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg (frame, GTK_STATE_NORMAL, &palette->normal_back);
    gtk_widget_modify_fg (frame, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  mainw->fd_frame=frame;

  label = gtk_label_new (_("Preview"));
  gtk_frame_set_label_widget (GTK_FRAME (frame), label);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  gtk_frame_set_shadow_type (GTK_FRAME(frame), GTK_SHADOW_IN);
  mainw->framedraw=gtk_event_box_new();
  gtk_widget_set_size_request (mainw->framedraw, width, height);
  gtk_widget_modify_bg (mainw->framedraw, GTK_STATE_NORMAL, &palette->normal_back);
  gtk_widget_set_events (mainw->framedraw, GDK_BUTTON1_MOTION_MASK | GDK_BUTTON_RELEASE_MASK | 
			 GDK_BUTTON_PRESS_MASK| GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK);

  mainw->framedraw_frame=start;

  gtk_container_add (GTK_CONTAINER (frame), mainw->framedraw);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg (mainw->framedraw, GTK_STATE_NORMAL, &palette->normal_back);
    gtk_widget_modify_fg (mainw->framedraw, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  g_signal_connect_after (GTK_OBJECT (mainw->framedraw), "expose_event",
			  G_CALLBACK (expose_fd_event), NULL);


  hbox = gtk_hbox_new (FALSE, 2);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (hbox), 10);

  spinbutton_adj = (GObject *)gtk_adjustment_new (start, start, end, 1., 10., 0.);
  mainw->framedraw_spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_adj), 1, 0);
  gtk_box_pack_start (GTK_BOX (hbox), mainw->framedraw_spinbutton, FALSE, FALSE, 0);

  label2 = gtk_label_new_with_mnemonic (_("    _Frame"));
  gtk_box_pack_start (GTK_BOX (hbox), label2, FALSE, FALSE, 0);
  gtk_label_set_justify (GTK_LABEL (label2), GTK_JUSTIFY_LEFT);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label2, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  mainw->framedraw_scale=gtk_hscale_new_with_range(0,1,1);
  gtk_box_pack_start (GTK_BOX (hbox), mainw->framedraw_scale, TRUE, TRUE, 0);
  gtk_range_set_adjustment(GTK_RANGE(mainw->framedraw_scale),GTK_ADJUSTMENT(spinbutton_adj));
  gtk_scale_set_draw_value(GTK_SCALE(mainw->framedraw_scale),FALSE);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label2),mainw->framedraw_scale);

  rfx=(lives_rfx_t *)g_object_get_data(G_OBJECT(gtk_widget_get_toplevel(GTK_WIDGET(box))),"rfx");
  mainw->framedraw_preview = gtk_button_new_from_stock ("gtk-refresh");
  gtk_button_set_label (GTK_BUTTON (mainw->framedraw_preview),_ ("_Preview"));
  gtk_button_set_use_underline (GTK_BUTTON (mainw->framedraw_preview), TRUE);
  gtk_box_pack_start (GTK_BOX (hbox), mainw->framedraw_preview, TRUE, FALSE, 0);
  gtk_widget_set_sensitive(mainw->framedraw_spinbutton,FALSE);
  gtk_widget_set_sensitive(mainw->framedraw_scale,FALSE);
  g_signal_connect (mainw->framedraw_preview, "clicked",G_CALLBACK (start_preview),rfx);
  
  gtk_widget_show_all (vbox);

  if (!add_preview_button) {
    gtk_widget_hide(mainw->framedraw_preview);
  }

}



void framedraw_redraw (lives_special_framedraw_rect_t * framedraw, gboolean reload, GdkPixbuf *pixbuf) {
  // this will draw the mask (framedraw_bitmap) and optionally reload the image
  // and then combine them

  gint xstart;
  gint ystart;
  gint xend;
  gint yend;

  int fd_height;
  int fd_width;
  int width,height;

  double xstartf,ystartf,xendf,yendf;

  cairo_t *cr;

  if (!GDK_IS_DRAWABLE(lives_widget_get_xwindow(mainw->framedraw))) return;

  if (mainw->current_file<1||cfile==NULL) return;
  
  fd_width=mainw->framedraw->allocation.width;
  fd_height=mainw->framedraw->allocation.height;
    
  width=cfile->hsize;
  height=cfile->vsize;
  
  calc_maxspect(fd_width,fd_height,&width,&height);
  // copy from orig, resize
  // copy orig layer to layer
  if (mainw->fd_layer!=NULL) {
    weed_layer_free(mainw->fd_layer);
    mainw->fd_layer=NULL;
  }
  
  if (reload||mainw->fd_layer_orig==NULL) load_framedraw_image(pixbuf);
  
  mainw->fd_layer=weed_layer_copy(NULL,mainw->fd_layer_orig);
  // resize to correct size
  resize_layer(mainw->fd_layer, width, height, LIVES_INTERP_BEST);
  
  cr=layer_to_cairo(mainw->fd_layer);


  // draw on the cairo

  switch (framedraw->type) {
  case FD_RECT_DEMASK:
    xstart=(gint)(gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (framedraw->xstart_widget)));
    ystart=(gint)(gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (framedraw->ystart_widget)));
    xend=(gint)(gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (framedraw->xend_widget)));
    yend=(gint)(gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (framedraw->yend_widget)));

    //trans
    xstart -=  (int)((2.*((double)xstart/(double)width)) * (double)((fd_width - width)>>1)+.5);
    xend -=  (int)((2.*((double)xend/(double)width)) * (double)((fd_width - width)>>1)+.5);
    
    ystart -=  (int)((2.*((double)ystart/(double)height)) * (double)((fd_height - height)>>1)+.5);
    yend -=  (int)((2.*((double)yend/(double)height)) * (double)((fd_height - height)>>1)+.5);

    ystart=ystart*fd_height/(fd_height+FD_HT_ADJ*2);
    yend=yend*fd_height/(fd_height+FD_HT_ADJ*2);


    // create a mask which is only opaque within the clipping area

    cairo_rectangle(cr,0,0,width,height);
    cairo_rectangle(cr,xstart,ystart,xend-xstart+1,yend-ystart+1);
    cairo_set_operator(cr,CAIRO_OPERATOR_DEST_OUT);
    cairo_set_source_rgba(cr, .0, .0, .0, .5);
    cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD);
    cairo_fill (cr);

    break;
  case FD_RECT_MULTRECT:
    xstartf=gtk_spin_button_get_value (GTK_SPIN_BUTTON (framedraw->xstart_widget));
    ystartf=gtk_spin_button_get_value (GTK_SPIN_BUTTON (framedraw->ystart_widget));

    xendf=xstartf+gtk_spin_button_get_value (GTK_SPIN_BUTTON (framedraw->xend_widget));
    yendf=ystartf+gtk_spin_button_get_value (GTK_SPIN_BUTTON (framedraw->yend_widget));

    xstart=(int)(xstartf*(double)fd_width+.5);
    ystart=(int)(ystartf*(double)fd_height+.5);

    xend=(int)(xendf*(double)fd_width+.5);
    yend=(int)(yendf*(double)fd_height+.5);

    //trans
    xstart -=  (int)((2.*((double)xstart/(double)width)) * (double)((fd_width - width)>>1)+.5);
    xend -=  (int)((2.*((double)xend/(double)width)) * (double)((fd_width - width)>>1)+.5);
    
    ystart -=  (int)((2.*((double)ystart/(double)height)) * (double)((fd_height - height)>>1)+.5);
    yend -=  (int)((2.*((double)yend/(double)height)) * (double)((fd_height - height)>>1)+.5);

    cairo_set_source_rgba(cr, 1., 0., 0., 1.);
    cairo_rectangle(cr,xstart-1,ystart-1,xend-xstart+2,yend-ystart+2);
    cairo_stroke (cr);

    break;
  case FD_SINGLEPOINT:

    xstart=(gint)(gtk_spin_button_get_value (GTK_SPIN_BUTTON (framedraw->xstart_widget))*fd_width);
    ystart=(gint)(gtk_spin_button_get_value (GTK_SPIN_BUTTON (framedraw->ystart_widget))*fd_height);

    xstart -=  (int)((2.*((double)xstart/(double)width)) * (double)((fd_width - width)>>1)+.5);
    ystart -=  (int)((2.*((double)ystart/(double)height)) * (double)((fd_height - height)>>1)+.5);

    cairo_set_source_rgba(cr, 1., 0., 0., 1.);

    cairo_move_to(cr,xstart,ystart-3);
    cairo_line_to(cr,xstart,ystart+3);

    cairo_move_to(cr,xstart-3,ystart);
    cairo_line_to(cr,xstart+3,ystart);

    cairo_stroke (cr);

    break;
  }

  cairo_to_layer(cr, mainw->fd_layer);

  cairo_destroy(cr);

  redraw_framedraw_image ();
}



void load_rfx_preview(lives_rfx_t *rfx) {
  // load a preview of an rfx (rendered effect) in clip editor

  LiVESPixbuf *pixbuf;
  FILE *infofile=NULL;

  int max_frame=0,tot_frames=0;
  int vend=cfile->start;
  int retval;
  int alarm_handle;
  int current_file=mainw->current_file;

  boolean retb;
  boolean timeout;

  weed_timecode_t tc;
  const char *img_ext;

  if (mainw->framedraw_frame==0) mainw->framedraw_frame=1;

  lives_set_cursor_style(LIVES_CURSOR_BUSY,NULL);

  clear_mainw_msg();
  mainw->write_failed=FALSE;


#define LIVES_RFX_TIMER 10*U_SEC

  if (cfile->clip_type==CLIP_TYPE_FILE&&vend<=cfile->end) {
    // pull some frames for 10 seconds
    alarm_handle=lives_alarm_set(LIVES_RFX_TIMER);
    do {
      while (g_main_context_iteration(NULL,FALSE));
      if (is_virtual_frame(mainw->current_file,vend)) {
	retb=virtual_to_images(mainw->current_file,vend,vend,FALSE);
	if (!retb) return;
      }
      vend++;
      timeout=lives_alarm_get(alarm_handle);
    } while (vend<=cfile->end&&!timeout&&!mainw->cancelled);
    lives_alarm_clear(alarm_handle);
  }

  if (mainw->cancelled) {
    lives_set_cursor_style(LIVES_CURSOR_NORMAL,NULL);
    return;
  }

  // get message from back end processor
  while (!(infofile=fopen(cfile->info_file,"r"))&&!mainw->cancelled) {
    // wait until we get at least 1 frame
    while (g_main_context_iteration(NULL,FALSE));
    if (cfile->clip_type==CLIP_TYPE_FILE&&vend<=cfile->end) {
      // if we have a virtual clip (frames inside a video file)
      // pull some more frames to images to get us started
      do {
	retb=FALSE;
	if (is_virtual_frame(mainw->current_file,vend)) {
	  retb=virtual_to_images(mainw->current_file,vend,vend,FALSE);
	  if (!retb) {
	    fclose(infofile);
	    return;
	  }
	}
	vend++;
      } while (vend<=cfile->end&&!retb);
    }
    else {
      // otherwise wait
      g_usleep(prefs->sleep_time);
    }
  }

  if (mainw->cancelled) {
    if (infofile) fclose(infofile);
    lives_set_cursor_style(LIVES_CURSOR_NORMAL,NULL);
    return;
  }

  do {
    retval=0;
    mainw->read_failed=FALSE;
    lives_fgets(mainw->msg,512,infofile);
    if (mainw->read_failed) retval=do_read_failed_error_s_with_retry(cfile->info_file,NULL,NULL);
  } while (retval==LIVES_RETRY);

  fclose(infofile);


  if (strncmp(mainw->msg,"completed",9)) {
    if (rfx->num_in_channels>0) {
      max_frame=atoi(mainw->msg);
    }
    else {
      gint numtok=get_token_count (mainw->msg,'|');
      if (numtok>4) {
	gchar **array=g_strsplit(mainw->msg,"|",numtok);
	max_frame=atoi(array[0]);
	cfile->hsize=atoi(array[1]);
	cfile->vsize=atoi(array[2]);
	cfile->fps=cfile->pb_fps=strtod(array[3],NULL);
	if (cfile->fps==0) cfile->fps=cfile->pb_fps=prefs->default_fps;
	tot_frames=atoi(array[4]);
	g_strfreev(array);
      }
    }
  }
  else {
    max_frame=cfile->end;
  }

  lives_set_cursor_style(LIVES_CURSOR_NORMAL,NULL);

  if (max_frame>0) {
    if (rfx->num_in_channels==0) {
      gtk_spin_button_set_range (GTK_SPIN_BUTTON (mainw->framedraw_spinbutton),1,tot_frames);
    }
    
    if (mainw->framedraw_frame>max_frame) {
      gtk_spin_button_set_value(GTK_SPIN_BUTTON(mainw->framedraw_spinbutton),max_frame);
      mainw->current_file=current_file;
      return;
    }
  }

  if (rfx->num_in_channels>0) {
    img_ext="pre";
  }
  else {
    img_ext=cfile->img_type==IMG_TYPE_JPEG?"jpg":"png";
  }

  tc=((mainw->framedraw_frame-1.))/cfile->fps*U_SECL;
  pixbuf=pull_lives_pixbuf_at_size(mainw->current_file,mainw->framedraw_frame,(gchar *)img_ext,
				   tc,(gdouble)cfile->hsize,
				   (gdouble)cfile->vsize,LIVES_INTERP_BEST);


  load_framedraw_image(pixbuf);
  redraw_framedraw_image ();

  mainw->current_file=current_file;
}



void after_framedraw_frame_spinbutton_changed (GtkSpinButton *spinbutton, lives_special_framedraw_rect_t *framedraw) {
  // update the single frame/framedraw preview
  // after the "frame number" spinbutton has changed
  mainw->framedraw_frame=gtk_spin_button_get_value_as_int(spinbutton);
  if (GTK_WIDGET_VISIBLE(mainw->framedraw_preview)) {
    if (mainw->framedraw_preview!=NULL) gtk_widget_set_sensitive(mainw->framedraw_preview,FALSE);
    while (g_main_context_iteration(NULL,FALSE));
    load_rfx_preview(framedraw->rfx);
  }
  else framedraw_redraw(framedraw, TRUE, NULL);
}




void load_framedraw_image(LiVESPixbuf *pixbuf) {
  // this is for the single frame framedraw widget
  // it should be called whenever mainw->framedraw_bitmap changes

  weed_timecode_t tc;

  if (mainw->framedraw_frame>cfile->frames) mainw->framedraw_frame=cfile->frames;

  if (pixbuf==NULL) {
    const gchar *img_ext=cfile->img_type==IMG_TYPE_JPEG?"jpg":"png";

    // can happen if we preview for rendered generators
    if ((mainw->multitrack==NULL||mainw->current_file!=mainw->multitrack->render_file)&&mainw->framedraw_frame==0) return;

    tc=((mainw->framedraw_frame-1.))/cfile->fps*U_SECL;
    pixbuf=pull_lives_pixbuf_at_size(mainw->current_file,mainw->framedraw_frame,img_ext,tc,
				     (gdouble)cfile->hsize,(gdouble)cfile->vsize,
				     LIVES_INTERP_BEST);
  }

  if (pixbuf!=NULL) {
    if (mainw->fd_layer_orig!=NULL) {
      weed_layer_free(mainw->fd_layer_orig);
    }

    mainw->fd_layer_orig=weed_layer_new(0,0,NULL,WEED_PALETTE_END);

    if (pixbuf_to_layer(mainw->fd_layer_orig,pixbuf)) {
      mainw->do_not_free=gdk_pixbuf_get_pixels(pixbuf);
      mainw->free_fn=lives_free_with_check;
    }
    g_object_unref(pixbuf);
    mainw->do_not_free=NULL;
    mainw->free_fn=lives_free_normal;

  }

  if (mainw->fd_layer!=NULL) weed_layer_free(mainw->fd_layer);
  mainw->fd_layer=NULL;

}





void redraw_framedraw_image(void) {
  cairo_t *cr;
  LiVESPixbuf *pixbuf;

  int fd_width=mainw->framedraw->allocation.width;
  int fd_height=mainw->framedraw->allocation.height;

  int width,height;

  if (!GDK_IS_DRAWABLE(lives_widget_get_xwindow(mainw->framedraw))) return;

  if (mainw->fd_layer_orig==NULL) return;

  if (mainw->current_file<1||cfile==NULL) return;

  width=cfile->hsize;
  height=cfile->vsize;

  calc_maxspect(fd_width,fd_height,&width,&height);

  // copy orig layer to layer
  if (mainw->fd_layer==NULL) mainw->fd_layer=weed_layer_copy(NULL,mainw->fd_layer_orig);

  // force to RGB24
  convert_layer_palette(mainw->fd_layer,WEED_PALETTE_RGBA32,0);

  // resize to correct size
  resize_layer(mainw->fd_layer, width, height, LIVES_INTERP_BEST);

  // layer to pixbuf
  pixbuf=layer_to_pixbuf(mainw->fd_layer);

  // get cairo for window
  cr = gdk_cairo_create (lives_widget_get_xwindow(mainw->framedraw));

  // set source pixbuf for cairo
  gdk_cairo_set_source_pixbuf (cr, pixbuf, (fd_width-width)>>1, (fd_height-height)>>1);
  cairo_paint (cr);
  cairo_destroy(cr);

  // convert pixbuf back to layer
  if (pixbuf_to_layer(mainw->fd_layer,pixbuf)) {
    mainw->do_not_free=(gpointer)lives_pixbuf_get_pixels_readonly(pixbuf);
    mainw->free_fn=lives_free_with_check;
  }

  g_object_unref(pixbuf);
  mainw->do_not_free=NULL;
  mainw->free_fn=lives_free_normal;


}


// change cursor maybe when we enter or leave the framedraw window

gboolean on_framedraw_enter (GtkWidget *widget, GdkEventCrossing *event, lives_special_framedraw_rect_t *framedraw) {
  GdkCursor *cursor;

  if (framedraw==NULL&&mainw->multitrack!=NULL) {
    framedraw=mainw->multitrack->framedraw;
    if (framedraw==NULL&&mainw->multitrack->cursor_style==0) gdk_window_set_cursor 
							       (lives_widget_get_xwindow(mainw->multitrack->play_box), NULL);
  }

  if (framedraw==NULL) return FALSE;
  if (mainw->multitrack!=NULL&&(mainw->multitrack->track_index==-1||mainw->multitrack->cursor_style!=0)) return FALSE;

  switch (framedraw->type) {
  case FD_RECT_DEMASK:
  case FD_RECT_MULTRECT:
    if (mainw->multitrack==NULL) {
      cursor=gdk_cursor_new_for_display (gdk_display_get_default(), GDK_TOP_LEFT_CORNER);
      gdk_window_set_cursor (lives_widget_get_xwindow(mainw->framedraw), cursor);
    }
    else {
      cursor=gdk_cursor_new_for_display (mainw->multitrack->display, GDK_TOP_LEFT_CORNER);
      gdk_window_set_cursor (lives_widget_get_xwindow(mainw->multitrack->play_box), cursor);
    }
    break;
  case FD_SINGLEPOINT:
    if (mainw->multitrack==NULL) {
      cursor=gdk_cursor_new_for_display (gdk_display_get_default(), GDK_CROSSHAIR);
      gdk_window_set_cursor (lives_widget_get_xwindow(mainw->framedraw), cursor);
    }
    else {
      cursor=gdk_cursor_new_for_display (mainw->multitrack->display, GDK_CROSSHAIR);
      gdk_window_set_cursor (lives_widget_get_xwindow(mainw->multitrack->play_box), cursor);
    }
    break;
  }
  return FALSE;
}

gboolean on_framedraw_leave (GtkWidget *widget, GdkEventCrossing *event, lives_special_framedraw_rect_t *framedraw) {
  if (framedraw==NULL) return FALSE;
  gdk_window_set_cursor (lives_widget_get_xwindow(mainw->framedraw), NULL);
  return FALSE;
}


// using these 3 functions, the user can draw on frames

gboolean on_framedraw_mouse_start (GtkWidget *widget, GdkEventButton *event, lives_special_framedraw_rect_t *framedraw) {
  // user clicked in the framedraw widget (or multitrack playback widget)

  if (framedraw==NULL&&mainw->multitrack!=NULL) framedraw=mainw->multitrack->framedraw;

  if (framedraw==NULL&&mainw->multitrack!=NULL&&event->button==3) {
    // right click brings up context menu
    frame_context(widget,event,GINT_TO_POINTER(0));
  }

  if (framedraw==NULL) return FALSE;
  if (mainw->multitrack!=NULL&&mainw->multitrack->track_index==-1) return FALSE;

  if (event->button==1) {
    gdk_window_get_pointer(lives_widget_get_xwindow(widget), &xstart, &ystart, NULL);

    b1_held=TRUE;

    if ((framedraw->type==FD_RECT_MULTRECT||framedraw->type==FD_RECT_DEMASK)&&
	(mainw->multitrack==NULL||mainw->multitrack->cursor_style==0)) {
      GdkCursor *cursor;
      if (mainw->multitrack==NULL) {
	cursor=gdk_cursor_new_for_display (gdk_display_get_default(), GDK_BOTTOM_RIGHT_CORNER);
	gdk_window_set_cursor (lives_widget_get_xwindow(mainw->framedraw), cursor);
      }
      else {
	cursor=gdk_cursor_new_for_display (mainw->multitrack->display, GDK_BOTTOM_RIGHT_CORNER);
	gdk_window_set_cursor (lives_widget_get_xwindow(mainw->multitrack->play_box), cursor);
      }
    }


    switch (framedraw->type) {
    case FD_RECT_DEMASK:
      {
	// TODO - allow user selectable line colour
	lives_colRGBA32_t col;
	col.red=180;
	col.green=0;
	col.blue=0;
	col.alpha=255;
	
	draw_rect_demask (&col,xstart,ystart,xstart,ystart,FALSE);
	
	redraw_framedraw_image();
      }
      break;
    case (FD_RECT_MULTRECT): 
      {
	int fd_height;
	int fd_width;

	gdouble offsx;
	gdouble offsy;

	fd_width=mainw->framedraw->allocation.width;
	fd_height=mainw->framedraw->allocation.height;
  
	offsx=(gdouble)xstart/(gdouble)fd_width;
	offsy=(gdouble)ystart/(gdouble)fd_height;
	
	noupdate=TRUE;
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(framedraw->xstart_widget),offsx);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(framedraw->ystart_widget),offsy);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(framedraw->xend_widget),0.);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(framedraw->yend_widget),0.);
	noupdate=FALSE;

	framedraw_redraw (framedraw, FALSE, NULL);
	if (mainw->framedraw_reset!=NULL) {
	  gtk_widget_set_sensitive (mainw->framedraw_reset,TRUE);
	}
	if (mainw->framedraw_preview!=NULL) {
	  gtk_widget_set_sensitive (mainw->framedraw_preview,TRUE);
	}
      }
      break;
    default:
      break;
    }
  }

  return FALSE;
}

gboolean on_framedraw_mouse_update (GtkWidget *widget, GdkEventButton *event, lives_special_framedraw_rect_t *framedraw) {
  // pointer moved in the framedraw widget
  gint xcurrent,ycurrent;

  lives_colRGBA32_t col;

  if (framedraw==NULL&&mainw->multitrack!=NULL) framedraw=mainw->multitrack->framedraw;
  if (framedraw==NULL) return FALSE;
  if (mainw->multitrack!=NULL&&mainw->multitrack->track_index==-1) return FALSE;

  gdk_window_get_pointer(lives_widget_get_xwindow(widget), &xcurrent, &ycurrent, NULL);

  switch (framedraw->type) {
  case FD_RECT_DEMASK:
    if (b1_held) {

      // TODO - allow user selectable line colour
      col.red=180;
      col.green=0;
      col.blue=0;
      col.alpha=255;

      draw_rect_demask (&col,xstart,ystart,xcurrent,ycurrent,FALSE);

      redraw_framedraw_image();

    }
    break;
  case FD_RECT_MULTRECT:
    if (b1_held) {
      int xend,yend,width,height,fd_width,fd_height;
      double xscale,yscale;

      xend=xcurrent;
      yend=ycurrent;

      fd_width=mainw->framedraw->allocation.width;
      fd_height=mainw->framedraw->allocation.height;
      
      width=cfile->hsize;
      height=cfile->vsize;
      
      calc_maxspect(fd_width,fd_height,&width,&height);
      
      // translate to picture-in-frame coords.
      
      xend -= (fd_width - width)/4. - (int)((1.*((double)xend/(double)width)) * (double)((fd_width - width)>>1)+.5);
      yend -= (fd_height - height)/4. - (int)((1.*((double)yend/(double)height)) * (double)((fd_height - height)>>1)+.5);

      yend=yend*(fd_height+FD_HT_ADJ*2)/fd_height;

      xscale=((double)(xend-xstart)+1.)/(double)fd_width;
      yscale=((double)(yend-ystart)+1.)/(double)(fd_height+2*FD_HT_ADJ);


      noupdate=TRUE;
      gtk_spin_button_set_value(GTK_SPIN_BUTTON(framedraw->xend_widget),xscale);
      gtk_spin_button_set_value(GTK_SPIN_BUTTON(framedraw->yend_widget),yscale);
      noupdate=FALSE;

      framedraw_redraw (framedraw, FALSE, NULL);
      if (mainw->framedraw_reset!=NULL) {
	gtk_widget_set_sensitive (mainw->framedraw_reset,TRUE);
      }
      if (mainw->framedraw_preview!=NULL) {
	gtk_widget_set_sensitive (mainw->framedraw_preview,TRUE);
      }

    }
    break;
  }

  return FALSE;
}


gboolean on_framedraw_mouse_reset (GtkWidget *widget, GdkEventButton *event, lives_special_framedraw_rect_t *framedraw) {
  gint xend,yend;
  gdouble offsx,offsy;
  int fd_width,fd_height,width,height;

  if (framedraw==NULL&&mainw->multitrack!=NULL) framedraw=mainw->multitrack->framedraw;
  if (framedraw==NULL) return FALSE;
  if (mainw->multitrack!=NULL&&mainw->multitrack->track_index==-1) return FALSE;

  gdk_window_get_pointer(lives_widget_get_xwindow(widget), &xend, &yend, NULL);
  // user released the mouse button in framedraw widget
  if (event->button==1) {
    b1_held=FALSE;
  }

  fd_width=mainw->framedraw->allocation.width;
  fd_height=mainw->framedraw->allocation.height;
    
  width=cfile->hsize;
  height=cfile->vsize;
  
  calc_maxspect(fd_width,fd_height,&width,&height);



  // translate to picture-in-frame coords.

  xstart -= (fd_width - width)/4. - (int)((1.*((double)xstart/(double)width)) * (double)((fd_width - width)>>1)+.5);
  xend -= (fd_width - width)/4. - (int)((1.*((double)xend/(double)width)) * (double)((fd_width - width)>>1)+.5);
  
  ystart -= (fd_height - height)/4. - (int)((1.*((double)ystart/(double)height)) * (double)((fd_height - height)>>1)+.5);
  yend -= (fd_height - height)/4. - (int)((1.*((double)yend/(double)height)) * (double)((fd_height - height)>>1)+.5);

  ystart=ystart*(fd_height+FD_HT_ADJ*2)/fd_height;
  yend=yend*(fd_height+FD_HT_ADJ*2)/fd_height;


  switch (framedraw->type) {
  case FD_RECT_DEMASK:
    if (xstart<0) xstart=0;
    if (ystart<0) ystart=0;

    if (xend>=cfile->hsize) xend=cfile->hsize-1;
    if (yend>=cfile->vsize) yend=cfile->vsize-1;
    
    noupdate=TRUE;
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(framedraw->xstart_widget),MIN (xstart,xend));
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(framedraw->ystart_widget),MIN (ystart,yend));
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(framedraw->xend_widget),MAX (xstart,xend));
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(framedraw->yend_widget),MAX (ystart,yend));
    noupdate=FALSE;

    framedraw_redraw (framedraw, FALSE, NULL);

    if (mainw->framedraw_reset!=NULL) {
      gtk_widget_set_sensitive (mainw->framedraw_reset,TRUE);
    }
    if (mainw->framedraw_preview!=NULL) {
      gtk_widget_set_sensitive (mainw->framedraw_preview,TRUE);
    }

    break;


  case FD_RECT_MULTRECT:
    //if (mainw->multitrack!=NULL) on_frame_preview_clicked(NULL,mainw->multitrack);
    break;

  case FD_SINGLEPOINT:
    offsx=(gdouble)xend/(gdouble)fd_width;
    offsy=(gdouble)yend/(gdouble)(fd_height+FD_HT_ADJ*2);

    noupdate=TRUE;
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(framedraw->xstart_widget),offsx);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(framedraw->ystart_widget),offsy);
    noupdate=FALSE;
    break;
  }

  return FALSE;
}


void after_framedraw_widget_changed (GtkWidget *widget, lives_special_framedraw_rect_t *framedraw) {
  if (mainw->block_param_updates||noupdate) return;

  // redraw mask when spin values change
  framedraw_redraw (framedraw, FALSE, NULL);
  if (mainw->framedraw_reset!=NULL) {
    gtk_widget_set_sensitive (mainw->framedraw_reset,TRUE);
  }
  if (mainw->framedraw_preview!=NULL) {
    gtk_widget_set_sensitive (mainw->framedraw_preview,TRUE);
  }
}




// various drawing functions

void draw_rect_demask (lives_colRGBA32_t *col, int x1, int y1, int x2, int y2, boolean filled) {
  cairo_t *cr;
  int width,height;
  int fd_width;
  int fd_height;

  if (mainw->fd_layer_orig==NULL) return;

  if (!GDK_IS_DRAWABLE(lives_widget_get_xwindow(mainw->framedraw))) return;

  if (mainw->current_file<1||cfile==NULL) return;

  fd_width=mainw->framedraw->allocation.width;
  fd_height=mainw->framedraw->allocation.height;
  
  width=cfile->hsize;
  height=cfile->vsize;
  
  calc_maxspect(fd_width,fd_height,&width,&height);
  // copy from orig, resize
  // copy orig layer to layer

  if (mainw->fd_layer!=NULL) {
    weed_layer_free(mainw->fd_layer);
  }
  
  mainw->fd_layer=weed_layer_copy(NULL,mainw->fd_layer_orig);
  // resize to correct size
  resize_layer(mainw->fd_layer, width, height, LIVES_INTERP_BEST);
  
  // translate to frame-in-frame coords
  x1 -= (fd_width-width)>>1;
  x2 -= (fd_width-width)>>1;
  y1 -= (fd_height-height)>>1;
  y2 -= (fd_height-height)>>1;
  

  cr=layer_to_cairo(mainw->fd_layer);

  // draw on the cairo

  cairo_set_source_rgba(cr, (double)col->red/255., (double)col->green/255.,(double)col->blue/255.,(double)col->alpha/255.);
  
  cairo_rectangle(cr,  MIN (x1,x2), MIN (y1,y2), ABS (x2-x1), ABS (y2-y1));
  
  cairo_stroke (cr);

  if (filled) {
    cairo_clip(cr);
    cairo_paint(cr);
  }

  cairo_to_layer(cr, mainw->fd_layer);

  cairo_destroy(cr);

}



void on_framedraw_reset_clicked (GtkButton *button, lives_special_framedraw_rect_t *framedraw) {
  gdouble x_min,x_max,y_min,y_max;
  // TODO ** - set to defaults

  gtk_spin_button_get_range (GTK_SPIN_BUTTON (framedraw->xstart_widget),&x_min,NULL);
  gtk_spin_button_get_range (GTK_SPIN_BUTTON (framedraw->ystart_widget),&y_min,NULL);
  gtk_spin_button_get_range (GTK_SPIN_BUTTON (framedraw->xend_widget),NULL,&x_max);
  gtk_spin_button_get_range (GTK_SPIN_BUTTON (framedraw->yend_widget),NULL,&y_max);

  noupdate=TRUE;
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(framedraw->xend_widget),x_max);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(framedraw->yend_widget),y_max);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(framedraw->xstart_widget),x_min);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(framedraw->ystart_widget),y_min);
  noupdate=FALSE;

  framedraw_redraw (framedraw, FALSE, NULL);
  if (mainw->framedraw_reset!=NULL) {
    gtk_widget_set_sensitive (mainw->framedraw_reset,TRUE);
  }
  if (mainw->framedraw_preview!=NULL) {
    gtk_widget_set_sensitive (mainw->framedraw_preview,TRUE);
  }

}
