// framedraw.c
// LiVES
// (c) G. Finch (salsaman@xs4all.nl,salsaman@gmail.com) 2002 - 2011
// see file COPYING for licensing details : released under the GNU GPL 3 or later

// functions for the 'framedraw' widget - lets users draw on frames :-)
#include "../libweed/weed-palettes.h"

#include "main.h"
#include "callbacks.h"
#include "support.h"
#include "interface.h"
#include "effects.h"
#include "cvirtual.h"

// set by mouse button press
static gint xstart,ystart;
static gboolean b1_held;

static gint fdwidth,fdheight;

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
  gchar *com=g_strdup_printf("smogrify stopsubsub \"%s\" 2>/dev/null",cfile->handle);

  gtk_widget_set_sensitive(mainw->framedraw_preview,FALSE);
  while (g_main_context_iteration(NULL,FALSE));

  if (mainw->did_rfx_preview) {
    lives_system(com,TRUE); // try to stop any in-progress preview
    do_rfx_cleanup(rfx);
  }

  g_free(com);

  for (i=0;i<rfx->num_params;i++) {
    rfx->params[i].changed=FALSE;
  }

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
  mainw->framedraw_bitmap = gdk_pixmap_new (NULL, width, height, 1);
  if (mainw->framedraw_bitmap!=NULL) mainw->framedraw_bitmapgc=gdk_gc_new (mainw->framedraw_bitmap);

  gdk_draw_rectangle (GDK_DRAWABLE (mainw->framedraw_bitmap),
		      mainw->framedraw_bitmapgc,
		      TRUE,
		      0, 0,
		      width, height
		      );

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




void
widget_add_framedraw (GtkVBox *box, gint start, gint end, gboolean add_preview_button, gint width, gint height) {
  // adds the frame draw widget to box
  // the redraw button should be connected to an appropriate redraw function
  // after calling this function

  // an example of this is in 'trim frames'

  GtkWidget *vseparator;
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *label;
  GtkObject *spinbutton_adj;
  GtkWidget *label2;
  GtkWidget *frame;
 
  lives_rfx_t *rfx;

  b1_held=FALSE;

  mainw->framedraw_reset=NULL;

  vseparator = gtk_vseparator_new ();
  gtk_box_pack_start (GTK_BOX (GTK_WIDGET (box)->parent), vseparator, FALSE, FALSE, 0);
  gtk_widget_show (vseparator);

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (GTK_WIDGET (box)->parent), vbox, FALSE, FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 10);

  mainw->fd_scale=calc_fd_scale(width,height);
  width/=mainw->fd_scale;
  height/=mainw->fd_scale;

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
  add_fill_to_box(GTK_BOX(hbox));
  frame = gtk_frame_new (NULL);
  gtk_widget_set_size_request (frame, width, height);
  add_fill_to_box(GTK_BOX(hbox));
  gtk_box_pack_start (GTK_BOX (hbox), frame, FALSE, FALSE, 0);
  gtk_widget_modify_bg (frame, GTK_STATE_NORMAL, &palette->normal_back);
  gtk_widget_modify_fg (frame, GTK_STATE_NORMAL, &palette->normal_fore);
  mainw->fd_frame=frame;
  fdwidth=width;
  fdheight=height;

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

  // create a blank bitmap mask
  if (mainw->framedraw_bitmap!=NULL) {
    gdk_pixmap_unref (mainw->framedraw_bitmap);
  }

  mainw->framedraw_bitmap=NULL;
  if (mainw->framedraw_bitmapgc!=NULL) {
    g_object_unref (mainw->framedraw_bitmapgc);
  }
  mainw->framedraw_bitmapgc=NULL;

  mainw->framedraw_image = gtk_image_new_from_pixmap (NULL, NULL);

  gtk_container_add (GTK_CONTAINER (frame), mainw->framedraw);
  gtk_container_add (GTK_CONTAINER (mainw->framedraw), mainw->framedraw_image);

  // load the start frame and show it
  load_framedraw_image(NULL);

  hbox = gtk_hbox_new (FALSE, 2);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (hbox), 10);

  spinbutton_adj = gtk_adjustment_new (start, start, end, 1., 10., 0.);
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

  rfx=g_object_get_data(G_OBJECT(gtk_widget_get_toplevel(GTK_WIDGET(box))),"rfx");
  mainw->framedraw_preview = gtk_button_new_from_stock ("gtk-refresh");
  gtk_button_set_label (GTK_BUTTON (mainw->framedraw_preview),_ ("_Preview"));
  gtk_button_set_use_underline (GTK_BUTTON (mainw->framedraw_preview), TRUE);
  gtk_box_pack_start (GTK_BOX (hbox), mainw->framedraw_preview, TRUE, FALSE, 0);
  gtk_widget_set_sensitive(mainw->framedraw_spinbutton,FALSE);
  gtk_widget_set_sensitive(mainw->framedraw_scale,FALSE);
  g_signal_connect (mainw->framedraw_preview, "clicked",G_CALLBACK (start_preview),rfx);
  
  if (mainw->framedraw_colourgc!=NULL) {
    g_object_unref (mainw->framedraw_colourgc);
    mainw->framedraw_colourgc=NULL;
  }

  gtk_widget_show_all (vbox);

  if (!add_preview_button) {
    gtk_widget_hide(mainw->framedraw_preview);
  }
}


void
framedraw_redraw (lives_special_framedraw_rect_t * framedraw, gboolean reload, GdkPixbuf *pixbuf)
{
  // this will draw the mask (framedraw_bitmap) and optionally reload the image
  // and then combine them

  gint xstart;
  gint ystart;
  gint xend;
  gint yend;

  gint inwidth,inheight,outwidth,outheight;

  if (mainw->framedraw_bitmap==NULL||!GDK_IS_DRAWABLE (mainw->framedraw_bitmap)) return;

  // update framedraw_bitmap
  gdk_gc_set_foreground (mainw->framedraw_bitmapgc, &palette->bm_trans);
  gdk_draw_rectangle (GDK_DRAWABLE (mainw->framedraw_bitmap),
		      mainw->framedraw_bitmapgc,
		      TRUE,
		      0, 0,
		      cfile->hsize, cfile->vsize
		      );

  gdk_gc_set_foreground (mainw->framedraw_bitmapgc, &palette->bm_opaque);

  switch (framedraw->type) {
  case FD_RECT_DEMASK:
    xstart=(gint)(gtk_spin_button_get_value (GTK_SPIN_BUTTON (framedraw->xstart_widget))*
		  fdwidth/(gdouble)cfile->hsize+.4999);
    ystart=(gint)(gtk_spin_button_get_value (GTK_SPIN_BUTTON (framedraw->ystart_widget))*
		  fdheight/(gdouble)cfile->vsize+.4999);
    xend=(gint)(gtk_spin_button_get_value (GTK_SPIN_BUTTON (framedraw->xend_widget))*fdwidth/(gdouble)cfile->hsize+.4999);
    yend=(gint)(gtk_spin_button_get_value (GTK_SPIN_BUTTON (framedraw->yend_widget))*fdheight/(gdouble)cfile->vsize+.4999);

    // above
    gdk_draw_rectangle (GDK_DRAWABLE (mainw->framedraw_bitmap),
			mainw->framedraw_bitmapgc,
			TRUE,
			0, 0,
			(gdouble)fdwidth, (gdouble)ystart
			);

    // right
    gdk_draw_rectangle (GDK_DRAWABLE (mainw->framedraw_bitmap),
			mainw->framedraw_bitmapgc,
			TRUE,
			(gdouble)xend+1, (gdouble)ystart,
			(gdouble)(fdwidth-xend)-1, (gdouble)(fdheight-ystart)
			);

    // below
    gdk_draw_rectangle (GDK_DRAWABLE (mainw->framedraw_bitmap),
			mainw->framedraw_bitmapgc,
			TRUE,
			0, (gdouble)yend+1,
			(gdouble)fdwidth, (gdouble)(fdheight-yend)-1
			);

    // left
    gdk_draw_rectangle (GDK_DRAWABLE (mainw->framedraw_bitmap),
			mainw->framedraw_bitmapgc,
			TRUE,
			0, (gdouble)ystart,
			(gdouble)xstart, (gdouble)(yend-ystart)+1
			);
    break;
  case FD_RECT_MULTRECT:
    if (mainw->multitrack!=NULL) {
      inwidth=mainw->multitrack->inwidth;
      inheight=mainw->multitrack->inheight;
      outwidth=mainw->multitrack->outwidth;
      outheight=mainw->multitrack->outheight;
    }
    else {
      inwidth=cfile->hsize;
      inheight=cfile->vsize;
      outwidth=cfile->hsize;
      outheight=cfile->vsize;
    }

    xstart=(gint)(gtk_spin_button_get_value (GTK_SPIN_BUTTON (framedraw->xstart_widget))*outwidth+.4999);
    ystart=(gint)(gtk_spin_button_get_value (GTK_SPIN_BUTTON (framedraw->ystart_widget))*outheight+.4999);
    xend=xstart+(gint)(gtk_spin_button_get_value (GTK_SPIN_BUTTON (framedraw->xend_widget))*outwidth+.4999);
    yend=ystart+(gint)(gtk_spin_button_get_value (GTK_SPIN_BUTTON (framedraw->yend_widget))*outheight+.4999);

    gdk_draw_line (GDK_DRAWABLE (mainw->framedraw_bitmap),
		   mainw->framedraw_bitmapgc,
		   xstart-1, ystart-1,
		   xend+1, ystart-1
		   );

    gdk_draw_line (GDK_DRAWABLE (mainw->framedraw_bitmap),
		   mainw->framedraw_bitmapgc,
		   xend+1, ystart-1,
		   xend+1, yend+1
		   );

    gdk_draw_line (GDK_DRAWABLE (mainw->framedraw_bitmap),
		   mainw->framedraw_bitmapgc,
		   xend+1, yend+1,
		   xstart-1, yend+1
		   );

    gdk_draw_line (GDK_DRAWABLE (mainw->framedraw_bitmap),
		   mainw->framedraw_bitmapgc,
		   xstart-1, yend+1,
		   xstart-1, ystart-1
		   );
    break;
  case FD_SINGLEPOINT:

    if (mainw->multitrack!=NULL) {
      outwidth=mainw->multitrack->outwidth;
      outheight=mainw->multitrack->outheight;
      xstart=(gint)(gtk_spin_button_get_value (GTK_SPIN_BUTTON (framedraw->xstart_widget))*outwidth+.4999);
      ystart=(gint)(gtk_spin_button_get_value (GTK_SPIN_BUTTON (framedraw->ystart_widget))*outheight+.4999);
    }
    else {
      inwidth=cfile->hsize;
      inheight=cfile->vsize;
      xstart=(gint)(gtk_spin_button_get_value (GTK_SPIN_BUTTON (framedraw->xstart_widget))*inwidth/mainw->fd_scale+.4999);
      ystart=(gint)(gtk_spin_button_get_value (GTK_SPIN_BUTTON (framedraw->ystart_widget))*inheight/mainw->fd_scale+.4999);
    }

    // draw a small cross
    gdk_draw_line (GDK_DRAWABLE (mainw->framedraw_bitmap),
		   mainw->framedraw_bitmapgc,
		   xstart, ystart-3,
		   xstart, ystart+3
		   );

    gdk_draw_line (GDK_DRAWABLE (mainw->framedraw_bitmap),
		   mainw->framedraw_bitmapgc,
		   xstart-3, ystart,
		   xstart+3, ystart
		   );

    break;
  }

  // and call (re)load_framedraw_image to composite it onto
  if (reload) {
    load_framedraw_image(pixbuf);
  }
  else {
    redraw_framedraw_image ();
  }
}



void load_rfx_preview(lives_rfx_t *rfx) {
  // load a preview of an rfx (rendered effect) in clip editor

  GdkPixbuf *pixbuf;
  FILE *infofile=NULL;

  int max_frame=0,tot_frames=0;
  int vend=cfile->start;
  int retval;
  gint current_file=mainw->current_file;
  gboolean retb;

  weed_timecode_t tc;
  const gchar *img_ext;

  if (mainw->framedraw_frame==0) mainw->framedraw_frame=1;

  lives_set_cursor_style(LIVES_CURSOR_BUSY,NULL);

  clear_mainw_msg();
  mainw->write_failed=FALSE;

  // get message from back end processor
  while (!(infofile=fopen(cfile->info_file,"r"))&&!mainw->cancelled) {
    // wait until we get at least 1 frame
    while (g_main_context_iteration(NULL,FALSE));
    if (cfile->clip_type==CLIP_TYPE_FILE&&vend<=cfile->end) {
      // if we have a virtual clip (frames inside a video file)
      // pull some frames to images to get us started
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
	// TODO - ** small generated frames may not work with framedrawing - may need to calc offset between frame and image
	mainw->fd_scale=calc_fd_scale(cfile->hsize,cfile->vsize);
	cfile->fps=cfile->pb_fps=strtod(array[3],NULL);
	if (cfile->fps==0) cfile->fps=cfile->pb_fps=prefs->default_fps;
	tot_frames=atoi(array[4]);
	g_strfreev(array);
      }
    }
  }

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
  pixbuf=pull_gdk_pixbuf_at_size(mainw->current_file,mainw->framedraw_frame,(gchar *)img_ext,
				 tc,(gdouble)cfile->hsize/mainw->fd_scale,
				 (gdouble)cfile->vsize/mainw->fd_scale,GDK_INTERP_HYPER);

  gtk_image_set_from_pixbuf(GTK_IMAGE(mainw->framedraw_image), pixbuf);
  gtk_widget_queue_draw (mainw->framedraw_image);
  mainw->current_file=current_file;
  if (pixbuf!=NULL) gdk_pixbuf_unref(pixbuf);

  lives_set_cursor_style(LIVES_CURSOR_NORMAL,NULL);
}



void
after_framedraw_frame_spinbutton_changed (GtkSpinButton *spinbutton, lives_special_framedraw_rect_t *framedraw)
{
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




void load_framedraw_image(GdkPixbuf *pixbuf) {
  // this is for the single frame framedraw widget
  // it should be called whenever mainw->framedraw_bitmap changes

  // it will load mainw->framedraw_frame into mainw->framedraw_pixmap
  // and combine this with mainw->framedraw_bitmap to create
  // mainw->framedraw_image

  // see for example framedraw_redraw for how to combine with a mask

  GdkBitmap *dummy_bitmap;
  weed_timecode_t tc;
  gboolean needs_free=FALSE;

  if (mainw->framedraw_frame>cfile->frames) mainw->framedraw_frame=cfile->frames;

  // can happen if we preview for rendered generators
  if ((mainw->multitrack==NULL||mainw->current_file!=mainw->multitrack->render_file)&&mainw->framedraw_frame==0) return;

  if (pixbuf==NULL) {
    const gchar *img_ext=cfile->img_type==IMG_TYPE_JPEG?"jpg":"png";
    tc=((mainw->framedraw_frame-1.))/cfile->fps*U_SECL;
    pixbuf=pull_gdk_pixbuf_at_size(mainw->current_file,mainw->framedraw_frame,img_ext,tc,
				   (gdouble)cfile->hsize/mainw->fd_scale,(gdouble)cfile->vsize/mainw->fd_scale,
				   GDK_INTERP_HYPER);
    needs_free=TRUE;
  }

  if (mainw->framedraw_orig_pixmap!=NULL) {
    gdk_pixmap_unref (mainw->framedraw_orig_pixmap);
  }

  // we just want the pixmap

  gdk_pixbuf_render_pixmap_and_mask (pixbuf, &mainw->framedraw_orig_pixmap, &dummy_bitmap, 128);
  if (needs_free&&pixbuf!=NULL) gdk_pixbuf_unref(pixbuf);

  if (dummy_bitmap!=NULL) {
    gdk_pixmap_unref (dummy_bitmap);
  }

  if (mainw->framedraw_colourgc==NULL) {
    mainw->framedraw_colourgc=gdk_gc_new (mainw->framedraw_orig_pixmap);
  }

  redraw_framedraw_image();
}





void 
redraw_framedraw_image(void) {
  // redraw existing image and mask

  if (!(mainw->framedraw_copy_pixmap==NULL)) {
    gdk_pixmap_unref (mainw->framedraw_copy_pixmap);
  }

  mainw->framedraw_copy_pixmap=gdk_pixmap_copy (mainw->framedraw_orig_pixmap);
  // do any drawing on the copy_pixmap here
  gtk_image_set_from_pixmap(GTK_IMAGE(mainw->framedraw_image), mainw->framedraw_copy_pixmap, mainw->framedraw_bitmap);
  gtk_widget_queue_draw (mainw->framedraw_image);
}


// change cursor maybe when we enter or leave the framedraw window

gboolean
on_framedraw_enter (GtkWidget *widget, GdkEventCrossing *event, lives_special_framedraw_rect_t *framedraw) {
  GdkCursor *cursor;

  if (framedraw==NULL&&mainw->multitrack!=NULL) {
    framedraw=mainw->multitrack->framedraw;
    if (framedraw==NULL&&mainw->multitrack->cursor_style==0) gdk_window_set_cursor 
							       (mainw->multitrack->play_box->window, NULL);
  }

  if (framedraw==NULL) return FALSE;
  if (mainw->multitrack!=NULL&&(mainw->multitrack->track_index==-1||mainw->multitrack->cursor_style!=0)) return FALSE;

  switch (framedraw->type) {
  case FD_RECT_DEMASK:
  case FD_RECT_MULTRECT:
    if (mainw->multitrack==NULL) {
      cursor=gdk_cursor_new_for_display (gdk_display_get_default(), GDK_TOP_LEFT_CORNER);
      gdk_window_set_cursor (mainw->framedraw->window, cursor);
    }
    else {
      cursor=gdk_cursor_new_for_display (mainw->multitrack->display, GDK_TOP_LEFT_CORNER);
      gdk_window_set_cursor (mainw->multitrack->play_box->window, cursor);
    }
    break;
  case FD_SINGLEPOINT:
    if (mainw->multitrack==NULL) {
      cursor=gdk_cursor_new_for_display (gdk_display_get_default(), GDK_CROSSHAIR);
      gdk_window_set_cursor (mainw->framedraw->window, cursor);
    }
    else {
      cursor=gdk_cursor_new_for_display (mainw->multitrack->display, GDK_CROSSHAIR);
      gdk_window_set_cursor (mainw->multitrack->play_box->window, cursor);
    }
    break;
  }
  return FALSE;
}

gboolean
on_framedraw_leave (GtkWidget *widget, GdkEventCrossing *event, lives_special_framedraw_rect_t *framedraw) {
  if (framedraw==NULL) return FALSE;
  gdk_window_set_cursor (mainw->framedraw->window, NULL);
  return FALSE;
}


// using these 3 functions, the user can draw on frames

gboolean
on_framedraw_mouse_start (GtkWidget *widget, GdkEventButton *event, lives_special_framedraw_rect_t *framedraw)
{
  // user clicked in the framedraw widget (or multitrack playback widget)

  if (framedraw==NULL&&mainw->multitrack!=NULL) framedraw=mainw->multitrack->framedraw;

  if (framedraw==NULL&&mainw->multitrack!=NULL&&event->button==3) {
    // right click brings up context menu
    frame_context(widget,event,GINT_TO_POINTER(0));
  }

  if (framedraw==NULL) return FALSE;
  if (mainw->multitrack!=NULL&&mainw->multitrack->track_index==-1) return FALSE;

  if (event->button==1) {
    gdk_window_get_pointer(GDK_WINDOW (widget->window), &xstart, &ystart, NULL);

    if (framedraw->type==FD_RECT_DEMASK) {
      xstart*=(gdouble)cfile->hsize/(gdouble)widget->allocation.width;
      ystart*=(gdouble)cfile->vsize/(gdouble)widget->allocation.height;
    }

    b1_held=TRUE;

    if ((framedraw->type==FD_RECT_MULTRECT||framedraw->type==FD_RECT_DEMASK)&&
	(mainw->multitrack==NULL||mainw->multitrack->cursor_style==0)) {
      GdkCursor *cursor;
      if (mainw->multitrack==NULL) {
	cursor=gdk_cursor_new_for_display (gdk_display_get_default(), GDK_BOTTOM_RIGHT_CORNER);
	gdk_window_set_cursor (mainw->framedraw->window, cursor);
      }
      else {
	cursor=gdk_cursor_new_for_display (mainw->multitrack->display, GDK_BOTTOM_RIGHT_CORNER);
	gdk_window_set_cursor (mainw->multitrack->play_box->window, cursor);
      }
    }

    if (framedraw->type==FD_RECT_MULTRECT) {
      gdouble offsx=(gdouble)xstart/(gdouble)widget->allocation.width;
      gdouble offsy=(gdouble)ystart/(gdouble)widget->allocation.height;

      if (mainw->multitrack!=NULL) {
	if (mainw->multitrack->play_width<mainw->multitrack->play_window_width) 
	  offsx=((gdouble)xstart-(gdouble)(mainw->multitrack->play_window_width-
					   mainw->multitrack->play_width)/2.)/(gdouble)mainw->multitrack->play_width;
	if (mainw->multitrack->play_height<mainw->multitrack->play_window_height) 
	  offsy=((gdouble)ystart-(gdouble)(mainw->multitrack->play_window_height-
					   mainw->multitrack->play_height)/2.)/(gdouble)mainw->multitrack->play_height;
      }
      gtk_spin_button_set_value(GTK_SPIN_BUTTON(framedraw->xstart_widget),offsx);
      gtk_spin_button_set_value(GTK_SPIN_BUTTON(framedraw->ystart_widget),offsy);
      gtk_spin_button_set_value(GTK_SPIN_BUTTON(framedraw->xend_widget),0.);
      gtk_spin_button_set_value(GTK_SPIN_BUTTON(framedraw->yend_widget),0.);
    }
  }
  return FALSE;
}

gboolean
on_framedraw_mouse_update (GtkWidget *widget, GdkEventButton *event, lives_special_framedraw_rect_t *framedraw)
{
  // pointer moved in the framedraw widget
  gint xcurrent,ycurrent;

  if (framedraw==NULL&&mainw->multitrack!=NULL) framedraw=mainw->multitrack->framedraw;
  if (framedraw==NULL) return FALSE;
  if (mainw->multitrack!=NULL&&mainw->multitrack->track_index==-1) return FALSE;

  gdk_window_get_pointer(GDK_WINDOW (widget->window), &xcurrent, &ycurrent, NULL);

  switch (framedraw->type) {
  case FD_RECT_DEMASK:
    if (b1_held) {
      // TODO - update all widgets, re-read values
      redraw_framedraw_image();

      // TODO - allow user selectable line colour
      draw_rect_demask (&palette->light_yellow,xstart*fdwidth/cfile->hsize,
			ystart*fdheight/cfile->vsize,xcurrent,ycurrent,FALSE);
      gtk_image_set_from_pixmap(GTK_IMAGE(mainw->framedraw_image), mainw->framedraw_copy_pixmap, 
				mainw->framedraw_bitmap);
    }
    break;
  case FD_RECT_MULTRECT:
    if (b1_held) {
      gdouble scalex;
      gdouble scaley;
      if (mainw->multitrack!=NULL) {
	// scale is rel. to out channel size (outwidth, outheight)
	scalex=(gdouble)(xcurrent-xstart)/(gdouble)mainw->multitrack->outwidth;
	scaley=(gdouble)(ycurrent-ystart)/(gdouble)mainw->multitrack->outheight;
	if (mainw->multitrack->outwidth<mainw->multitrack->play_window_width) {
	  scalex=(gdouble)(xcurrent-xstart)/(gdouble)mainw->multitrack->outwidth;
	}
	if (mainw->multitrack->outheight<mainw->multitrack->play_window_height) {
	  scaley=(gdouble)(ycurrent-ystart)/(gdouble)mainw->multitrack->outheight;
	}
      }
      else {
	scalex=(gdouble)(xcurrent-xstart)/(gdouble)cfile->hsize;
	scaley=(gdouble)(xcurrent-xstart)/(gdouble)cfile->vsize;
      }
      gtk_spin_button_set_value(GTK_SPIN_BUTTON(framedraw->xend_widget),scalex);
      gtk_spin_button_set_value(GTK_SPIN_BUTTON(framedraw->yend_widget),scaley);
    }
    break;
  }

  return FALSE;
}


gboolean
on_framedraw_mouse_reset (GtkWidget *widget, GdkEventButton *event, lives_special_framedraw_rect_t *framedraw)
{
  gint xend,yend;
  gdouble offsx,offsy;

  if (framedraw==NULL&&mainw->multitrack!=NULL) framedraw=mainw->multitrack->framedraw;
  if (framedraw==NULL) return FALSE;
  if (mainw->multitrack!=NULL&&mainw->multitrack->track_index==-1) return FALSE;

  gdk_window_get_pointer(GDK_WINDOW (widget->window), &xend, &yend, NULL);
  // user released the mouse button in framedraw widget
  if (event->button==1) {
    b1_held=FALSE;
  }

  switch (framedraw->type) {
  case FD_RECT_DEMASK:

    xend*=(gdouble)cfile->hsize/(gdouble)fdwidth;
    yend*=(gdouble)cfile->vsize/(gdouble)fdheight;

    if (xend>=cfile->hsize) xend=cfile->hsize-1;
    if (yend>=cfile->vsize) yend=cfile->vsize-1;
    
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(framedraw->xstart_widget),MIN (xstart,xend));
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(framedraw->ystart_widget),MIN (ystart,yend));
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(framedraw->xend_widget),MAX (xstart,xend));
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(framedraw->yend_widget),MAX (ystart,yend));
    break;
  case FD_RECT_MULTRECT:
    //if (mainw->multitrack!=NULL) on_frame_preview_clicked(NULL,mainw->multitrack);
    break;
  case FD_SINGLEPOINT:
    offsx=(gdouble)xend/(gdouble)widget->allocation.width;
    offsy=(gdouble)yend/(gdouble)widget->allocation.height;

    if (mainw->multitrack!=NULL) {
      if (mainw->multitrack->play_width<mainw->multitrack->play_window_width) 
	offsx=(gint)(((gdouble)xend-(gdouble)(mainw->multitrack->play_window_width-
					      mainw->multitrack->play_width)/2.)/
		     (gdouble)mainw->multitrack->play_width+.4999);
      if (mainw->multitrack->play_height<mainw->multitrack->play_window_height) 
	offsy=(gint)(((gdouble)yend-(gdouble)(mainw->multitrack->play_window_height-
					      mainw->multitrack->play_height)/2.)/
		     (gdouble)mainw->multitrack->play_height+.4999);
    }
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(framedraw->xstart_widget),offsx);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(framedraw->ystart_widget),offsy);
    break;
  }

  return FALSE;
}


void
after_framedraw_widget_changed (GtkWidget *widget, lives_special_framedraw_rect_t *framedraw)
{
  if (mainw->block_param_updates) return;

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

void draw_rect_demask (GdkColor *col, gint x1, gint y1, gint x2, gint y2, gboolean filled)
{
  gdk_gc_set_rgb_fg_color (GDK_GC (mainw->framedraw_colourgc),col);
  gdk_draw_rectangle (GDK_DRAWABLE (mainw->framedraw_copy_pixmap),GDK_GC (mainw->framedraw_colourgc), filled, 
		      MIN (x1,x2), MIN (y1,y2), ABS (x2-x1), ABS (y2-y1));
}



void on_framedraw_reset_clicked (GtkButton *button, lives_special_framedraw_rect_t *framedraw)
{
  gdouble x_min,x_max,y_min,y_max;
  // TODO ** - set to defaults

  gtk_spin_button_get_range (GTK_SPIN_BUTTON (framedraw->xstart_widget),&x_min,NULL);
  gtk_spin_button_get_range (GTK_SPIN_BUTTON (framedraw->ystart_widget),&y_min,NULL);
  gtk_spin_button_get_range (GTK_SPIN_BUTTON (framedraw->xend_widget),NULL,&x_max);
  gtk_spin_button_get_range (GTK_SPIN_BUTTON (framedraw->yend_widget),NULL,&y_max);

  gtk_spin_button_set_value(GTK_SPIN_BUTTON(framedraw->xend_widget),x_max);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(framedraw->yend_widget),y_max);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(framedraw->xstart_widget),x_min);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(framedraw->ystart_widget),y_min);

  if (mainw->framedraw_reset!=NULL) {
    gtk_widget_set_sensitive (mainw->framedraw_reset,FALSE);
  }
}
