// framedraw.c
// LiVES
// (c) G. Finch (salsaman@gmail.com) 2002 - 2013
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
#include "framedraw.h"

// set by mouse button press
static double xstart,ystart;
static double xcurrent,ycurrent;
static volatile boolean b1_held;

static volatile boolean noupdate=FALSE;

static LiVESWidget *fbord_eventbox;


static double calc_fd_scale(int width, int height) {
  double scale=1.;

  if (width<MIN_PRE_X) {
    width=MIN_PRE_X;
  }
  if (height<MIN_PRE_Y) {
    height=MIN_PRE_Y;
  }

  if (width>MAX_PRE_X) scale=(double)width/(double)MAX_PRE_X;
  if (height>MAX_PRE_Y&&(height/MAX_PRE_Y>scale)) scale=(double)height/(double)MAX_PRE_Y;
  return scale;

}

static void start_preview(LiVESButton *button, lives_rfx_t *rfx) {
  int i;
  char *com;

  lives_widget_set_sensitive(mainw->framedraw_preview,FALSE);
  lives_widget_context_update();

  if (mainw->did_rfx_preview) {
#ifndef IS_MINGW
    com=lives_strdup_printf("%s stopsubsub \"%s\" 2>/dev/null",prefs->backend_sync,cfile->handle);
    lives_system(com,TRUE); // try to stop any in-progress preview
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

    if (cfile->start==0) {
      cfile->start=1;
      cfile->end=cfile->frames;
    }

    do_rfx_cleanup(rfx);
  }

#ifndef IS_MINGW
  com=lives_strdup_printf("%s clear_pre_files \"%s\" 2>/dev/null",prefs->backend_sync,cfile->handle);
#else
  com=lives_strdup_printf("%s clear_pre_files \"%s\" 2>NUL",prefs->backend_sync,cfile->handle);
#endif
  lives_system(com,TRUE); // clear any .pre files from before

  for (i=0; i<rfx->num_params; i++) {
    rfx->params[i].changed=FALSE;
  }

  mainw->cancelled=CANCEL_NONE;
  mainw->error=FALSE;

  // within do_effect() we check and if
  do_effect(rfx,TRUE); // actually start effect processing in the background

  lives_widget_set_sensitive(mainw->framedraw_spinbutton,TRUE);
  lives_widget_set_sensitive(mainw->framedraw_scale,TRUE);

  if (mainw->framedraw_frame>cfile->start&&!(cfile->start==0&&mainw->framedraw_frame==1))
    lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->framedraw_spinbutton),cfile->start);
  else {
    load_rfx_preview(rfx);
  }

  mainw->did_rfx_preview=TRUE;
}




void framedraw_connect_spinbutton(lives_special_framedraw_rect_t *framedraw, lives_rfx_t *rfx) {
  framedraw->rfx=rfx;

  lives_signal_connect_after(LIVES_GUI_OBJECT(mainw->framedraw_spinbutton), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                             LIVES_GUI_CALLBACK(after_framedraw_frame_spinbutton_changed),
                             framedraw);

}



void framedraw_connect(lives_special_framedraw_rect_t *framedraw, int width, int height, lives_rfx_t *rfx) {


  // add mouse fn's so we can draw on frames
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->framedraw), LIVES_WIDGET_MOTION_NOTIFY_EVENT,
                       LIVES_GUI_CALLBACK(on_framedraw_mouse_update),
                       framedraw);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->framedraw), LIVES_WIDGET_BUTTON_RELEASE_EVENT,
                       LIVES_GUI_CALLBACK(on_framedraw_mouse_reset),
                       framedraw);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->framedraw), LIVES_WIDGET_BUTTON_PRESS_EVENT,
                       LIVES_GUI_CALLBACK(on_framedraw_mouse_start),
                       framedraw);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->framedraw), LIVES_WIDGET_ENTER_EVENT,LIVES_GUI_CALLBACK(on_framedraw_enter),framedraw);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->framedraw), LIVES_WIDGET_LEAVE_NOTIFY_EVENT,LIVES_GUI_CALLBACK(on_framedraw_leave),framedraw);

  framedraw_connect_spinbutton(framedraw,rfx);

  lives_widget_set_bg_color(mainw->fd_frame, LIVES_WIDGET_STATE_NORMAL, &palette->light_red);
  lives_widget_set_bg_color(fbord_eventbox, LIVES_WIDGET_STATE_NORMAL, &palette->light_red);

  framedraw_redraw(framedraw, TRUE, NULL);
}


void framedraw_add_label(LiVESVBox *box) {
  LiVESWidget *label;

  // TRANSLATORS - Preview refers to preview window; keep this phrase short
  label=lives_standard_label_new(_("You can click in Preview to change these values"));
  lives_box_pack_start(LIVES_BOX(box), label, FALSE, FALSE, 0);
}


void framedraw_add_reset(LiVESVBox *box, lives_special_framedraw_rect_t *framedraw) {
  LiVESWidget *hbox_rst;

  framedraw_add_label(box);

  mainw->framedraw_reset = lives_button_new_from_stock(LIVES_STOCK_REFRESH);
  hbox_rst = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(box), hbox_rst, FALSE, FALSE, 0);

  lives_button_set_label(LIVES_BUTTON(mainw->framedraw_reset),_("_Reset Values"));
  lives_button_set_use_underline(LIVES_BUTTON(mainw->framedraw_reset), TRUE);
  lives_box_pack_start(LIVES_BOX(hbox_rst), mainw->framedraw_reset, TRUE, FALSE, 0);
  lives_widget_set_sensitive(mainw->framedraw_reset,FALSE);

  lives_signal_connect(mainw->framedraw_reset, LIVES_WIDGET_CLICKED_SIGNAL,LIVES_GUI_CALLBACK(on_framedraw_reset_clicked),framedraw);
}


static boolean expose_fd_event(LiVESWidget *widget, LiVESXEventExpose ev) {
  redraw_framedraw_image();
  return TRUE;
}

void widget_add_framedraw(LiVESVBox *box, int start, int end, boolean add_preview_button, int width, int height) {
  // adds the frame draw widget to box
  // the redraw button should be connected to an appropriate redraw function
  // after calling this function

  // an example of this is in 'trim frames'

  LiVESWidget *vseparator;
  LiVESWidget *vbox;
  LiVESWidget *hbox;
  LiVESWidget *label;
  LiVESAdjustment *spinbutton_adj;
  LiVESWidget *frame;

  lives_rfx_t *rfx;

  double fd_scale;

  b1_held=FALSE;

  mainw->framedraw_reset=NULL;

  vseparator = lives_vseparator_new();
  lives_box_pack_start(LIVES_BOX(lives_widget_get_parent(LIVES_WIDGET(box))), vseparator, FALSE, FALSE, 0);
  lives_widget_show(vseparator);

  vbox = lives_vbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(lives_widget_get_parent(LIVES_WIDGET(box))), vbox, FALSE, FALSE, 0);
  lives_container_set_border_width(LIVES_CONTAINER(vbox), widget_opts.border_width);

  fd_scale=calc_fd_scale(width,height);
  width/=fd_scale;
  height/=fd_scale;

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, 0);

  fbord_eventbox=lives_event_box_new();
  lives_container_set_border_width(LIVES_CONTAINER(fbord_eventbox),widget_opts.border_width);

  frame = lives_frame_new(NULL);

  lives_box_pack_start(LIVES_BOX(hbox), frame, FALSE, FALSE, 0);

  if (palette->style&STYLE_1) {
    lives_widget_set_bg_color(fbord_eventbox, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
    lives_widget_set_bg_color(frame, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
    lives_widget_set_fg_color(frame, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
  }

  mainw->fd_frame=frame;

  label = lives_standard_label_new(_("Preview"));
  lives_frame_set_label_widget(LIVES_FRAME(frame), label);

  lives_frame_set_shadow_type(LIVES_FRAME(frame), LIVES_SHADOW_NONE);

  mainw->framedraw=lives_event_box_new();
  lives_widget_set_size_request(mainw->framedraw, width, height);
  lives_container_set_border_width(LIVES_CONTAINER(mainw->framedraw),1);

  lives_widget_set_events(mainw->framedraw, LIVES_BUTTON1_MOTION_MASK | LIVES_BUTTON_RELEASE_MASK |
                          LIVES_BUTTON_PRESS_MASK| LIVES_ENTER_NOTIFY_MASK | LIVES_LEAVE_NOTIFY_MASK);

  mainw->framedraw_frame=start;

  lives_container_add(LIVES_CONTAINER(frame), fbord_eventbox);
  lives_container_add(LIVES_CONTAINER(fbord_eventbox), mainw->framedraw);

  if (palette->style&STYLE_1) {
    lives_widget_set_bg_color(mainw->framedraw, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
    lives_widget_set_fg_color(mainw->framedraw, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
  }

  lives_signal_connect_after(LIVES_GUI_OBJECT(mainw->framedraw), LIVES_WIDGET_EXPOSE_EVENT,
                             LIVES_GUI_CALLBACK(expose_fd_event), NULL);


  hbox = lives_hbox_new(FALSE, 2);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, 0);

  mainw->framedraw_spinbutton = lives_standard_spin_button_new(_("_Frame"),
                                TRUE,start,start,end,1.,10.,0,LIVES_BOX(hbox),NULL);

  spinbutton_adj=lives_spin_button_get_adjustment(LIVES_SPIN_BUTTON(mainw->framedraw_spinbutton));

  mainw->framedraw_scale=lives_hscale_new(LIVES_ADJUSTMENT(spinbutton_adj));
  lives_box_pack_start(LIVES_BOX(hbox), mainw->framedraw_scale, TRUE, TRUE, 0);
  lives_scale_set_draw_value(LIVES_SCALE(mainw->framedraw_scale),FALSE);

  rfx=(lives_rfx_t *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(lives_widget_get_toplevel(LIVES_WIDGET(box))),"rfx");
  mainw->framedraw_preview = lives_button_new_from_stock(LIVES_STOCK_REFRESH);
  lives_button_set_label(LIVES_BUTTON(mainw->framedraw_preview),_("_Preview"));
  lives_button_set_use_underline(LIVES_BUTTON(mainw->framedraw_preview), TRUE);
  lives_box_pack_start(LIVES_BOX(hbox), mainw->framedraw_preview, TRUE, FALSE, 0);
  lives_widget_set_sensitive(mainw->framedraw_spinbutton,FALSE);
  lives_widget_set_sensitive(mainw->framedraw_scale,FALSE);
  lives_signal_connect(mainw->framedraw_preview, LIVES_WIDGET_CLICKED_SIGNAL,LIVES_GUI_CALLBACK(start_preview),rfx);

  lives_widget_show_all(vbox);

  if (!add_preview_button) {
    lives_widget_hide(mainw->framedraw_preview);
  }

}



void framedraw_redraw(lives_special_framedraw_rect_t *framedraw, boolean reload, LiVESPixbuf *pixbuf) {
  // this will draw the mask (framedraw_bitmap) and optionally reload the image
  // and then combine them

  int fd_height;
  int fd_width;
  int width,height;

  double xstartf,ystartf,xendf,yendf;

  lives_painter_t *cr;

  if (mainw->current_file<1||cfile==NULL) return;

  if (framedraw->rfx->source_type==LIVES_RFX_SOURCE_RFX)
    if (noupdate) return;

  fd_width=lives_widget_get_allocation_width(mainw->framedraw);
  fd_height=lives_widget_get_allocation_height(mainw->framedraw);

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
  resize_layer(mainw->fd_layer, width, height, LIVES_INTERP_BEST, WEED_PALETTE_END, 0);

  cr=layer_to_lives_painter(mainw->fd_layer);


  // draw on the lives_painter

  // her we dont offset because we are drawing in the pixbuf, not the widget

  switch (framedraw->type) {
  case LIVES_PARAM_SPECIAL_TYPE_RECT_MULTRECT: // deprecated
    // scale values
    if (framedraw->xstart_param->dp==0) {
      xstartf=(double)lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(framedraw->xstart_param->widgets[0]));
      xstartf=xstartf/(double)cfile->hsize*(double)width;
    } else {
      xstartf=lives_spin_button_get_value(LIVES_SPIN_BUTTON(framedraw->xstart_param->widgets[0]));
      xstartf=xstartf*(double)width;
    }

    if (framedraw->xend_param->dp==0) {
      xendf=(double)lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(framedraw->xend_param->widgets[0]));
      xendf=xendf/(double)cfile->hsize*(double)width;
    } else {
      xendf=lives_spin_button_get_value(LIVES_SPIN_BUTTON(framedraw->xend_param->widgets[0]));
      xendf=xendf*(double)width;
    }

    if (framedraw->ystart_param->dp==0) {
      ystartf=(double)lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(framedraw->ystart_param->widgets[0]));
      ystartf=ystartf/(double)cfile->vsize*(double)height;
    } else {
      ystartf=lives_spin_button_get_value(LIVES_SPIN_BUTTON(framedraw->ystart_param->widgets[0]));
      ystartf=ystartf*(double)height;
    }

    if (framedraw->yend_param->dp==0) {
      yendf=lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(framedraw->yend_param->widgets[0]));
      yendf=yendf/(double)cfile->vsize*(double)height;
    } else {
      yendf=lives_spin_button_get_value(LIVES_SPIN_BUTTON(framedraw->yend_param->widgets[0]));
      yendf=yendf*(double)height;
    }

    lives_painter_set_source_rgb(cr, 1., 0., 0.);
    lives_painter_rectangle(cr,xstartf-1.,ystartf-1.,xendf+2.,yendf+2.);
    lives_painter_stroke(cr);

    break;
  case LIVES_PARAM_SPECIAL_TYPE_RECT_MULTIRECT:
  case LIVES_PARAM_SPECIAL_TYPE_RECT_DEMASK:

    if (framedraw->xstart_param->dp==0) {
      xstartf=(double)lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(framedraw->xstart_param->widgets[0]));
      xstartf=xstartf/(double)cfile->hsize*(double)width;
    } else {
      xstartf=lives_spin_button_get_value(LIVES_SPIN_BUTTON(framedraw->xstart_param->widgets[0]));
      xstartf=xstartf*(double)width;
    }

    if (framedraw->xend_param->dp==0) {
      xendf=(double)lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(framedraw->xend_param->widgets[0]));
      xendf=xendf/(double)cfile->hsize*(double)width;
    } else {
      xendf=lives_spin_button_get_value(LIVES_SPIN_BUTTON(framedraw->xend_param->widgets[0]));
      xendf=xendf*(double)width;
    }

    if (framedraw->ystart_param->dp==0) {
      ystartf=(double)lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(framedraw->ystart_param->widgets[0]));
      ystartf=ystartf/(double)cfile->vsize*(double)height;
    } else {
      ystartf=lives_spin_button_get_value(LIVES_SPIN_BUTTON(framedraw->ystart_param->widgets[0]));
      ystartf=ystartf*(double)height;
    }

    if (framedraw->yend_param->dp==0) {
      yendf=(double)lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(framedraw->yend_param->widgets[0]));
      yendf=yendf/(double)cfile->vsize*(double)height;
    } else {
      yendf=lives_spin_button_get_value(LIVES_SPIN_BUTTON(framedraw->yend_param->widgets[0]));
      yendf=yendf*(double)height;
    }

    if (b1_held||framedraw->type==LIVES_PARAM_SPECIAL_TYPE_RECT_MULTIRECT) {
      lives_painter_set_source_rgb(cr, 1., 0., 0.);
      lives_painter_rectangle(cr,xstartf-1.,ystartf-1.,xendf-xstartf+2.,yendf-ystartf+2.);
      lives_painter_stroke(cr);
    } else {
      if (!b1_held) {
        // create a mask which is only opaque within the clipping area

        lives_painter_rectangle(cr,0,0,width,height);
        lives_painter_rectangle(cr,xstartf,ystartf,xendf-xstartf+1.,yendf-ystartf+1.);
        lives_painter_set_operator(cr, LIVES_PAINTER_OPERATOR_DEST_OUT);
        lives_painter_set_source_rgba(cr, .0, .0, .0, .5);
        lives_painter_set_fill_rule(cr, LIVES_PAINTER_FILL_RULE_EVEN_ODD);
        lives_painter_fill(cr);
      }
    }

    break;
  case LIVES_PARAM_SPECIAL_TYPE_SINGLEPOINT:

    if (framedraw->xstart_param->dp==0) {
      xstartf=(double)lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(framedraw->xstart_param->widgets[0]));
      xstartf=xstartf/(double)cfile->hsize*(double)width;
    } else {
      xstartf=lives_spin_button_get_value(LIVES_SPIN_BUTTON(framedraw->xstart_param->widgets[0]));
      xstartf=xstartf*(double)width;
    }

    if (framedraw->ystart_param->dp==0) {
      ystartf=(double)lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(framedraw->ystart_param->widgets[0]));
      ystartf=ystartf/(double)cfile->vsize*(double)height;
    } else {
      ystartf=lives_spin_button_get_value(LIVES_SPIN_BUTTON(framedraw->ystart_param->widgets[0]));
      ystartf=ystartf*(double)height;
    }

    lives_painter_set_source_rgb(cr, 1., 0., 0.);

    lives_painter_move_to(cr,xstartf,ystartf-3);
    lives_painter_line_to(cr,xstartf,ystartf+3);

    lives_painter_stroke(cr);

    lives_painter_move_to(cr,xstartf-3,ystartf);
    lives_painter_line_to(cr,xstartf+3,ystartf);

    lives_painter_stroke(cr);

    break;

  default:

    break;

  }

  lives_painter_to_layer(cr, mainw->fd_layer);

  lives_painter_destroy(cr);

  redraw_framedraw_image();
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
      lives_widget_context_update();
      if (is_virtual_frame(mainw->current_file,vend)) {
        retb=virtual_to_images(mainw->current_file,vend,vend,FALSE,NULL);
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
    lives_widget_context_update();
    if (cfile->clip_type==CLIP_TYPE_FILE&&vend<=cfile->end) {
      // if we have a virtual clip (frames inside a video file)
      // pull some more frames to images to get us started
      do {
        retb=FALSE;
        if (is_virtual_frame(mainw->current_file,vend)) {
          retb=virtual_to_images(mainw->current_file,vend,vend,FALSE,NULL);
          if (!retb) {
            fclose(infofile);
            return;
          }
        }
        vend++;
      } while (vend<=cfile->end&&!retb);
    } else {
      // otherwise wait
      lives_usleep(prefs->sleep_time);
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
  } while (retval==LIVES_RESPONSE_RETRY);

  fclose(infofile);


  if (strncmp(mainw->msg,"completed",9)) {
    if (rfx->num_in_channels>0) {
      max_frame=atoi(mainw->msg);
    } else {
      int numtok=get_token_count(mainw->msg,'|');
      if (numtok>4) {
        char **array=lives_strsplit(mainw->msg,"|",numtok);
        max_frame=atoi(array[0]);
        cfile->hsize=atoi(array[1]);
        cfile->vsize=atoi(array[2]);
        cfile->fps=cfile->pb_fps=strtod(array[3],NULL);
        if (cfile->fps==0) cfile->fps=cfile->pb_fps=prefs->default_fps;
        tot_frames=atoi(array[4]);
        lives_strfreev(array);
      }
    }
  } else {
    max_frame=cfile->end;
  }

  lives_set_cursor_style(LIVES_CURSOR_NORMAL,NULL);

  if (max_frame>0) {
    if (rfx->num_in_channels==0) {
      int maxlen=calc_spin_button_width(1.,(double)tot_frames,0);
      lives_spin_button_set_range(LIVES_SPIN_BUTTON(mainw->framedraw_spinbutton),1,tot_frames);
      lives_entry_set_width_chars(LIVES_ENTRY(mainw->framedraw_spinbutton),maxlen);
      lives_widget_queue_draw(mainw->framedraw_spinbutton);
      lives_widget_queue_draw(mainw->framedraw_scale);
    }

    if (mainw->framedraw_frame>max_frame) {
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->framedraw_spinbutton),max_frame);
      mainw->current_file=current_file;
      return;
    }
  }

  if (rfx->num_in_channels>0) {
    img_ext=LIVES_FILE_EXT_PRE;
  } else {
    img_ext=get_image_ext_for_type(cfile->img_type);
  }

  tc=((mainw->framedraw_frame-1.))/cfile->fps*U_SECL;
  pixbuf=pull_lives_pixbuf_at_size(mainw->current_file,mainw->framedraw_frame,(char *)img_ext,
                                   tc,(double)cfile->hsize,
                                   (double)cfile->vsize,LIVES_INTERP_BEST);


  load_framedraw_image(pixbuf);
  redraw_framedraw_image();

  mainw->current_file=current_file;
}



void after_framedraw_frame_spinbutton_changed(LiVESSpinButton *spinbutton, lives_special_framedraw_rect_t *framedraw) {
  // update the single frame/framedraw preview
  // after the "frame number" spinbutton has changed
  mainw->framedraw_frame=lives_spin_button_get_value_as_int(spinbutton);
  if (lives_widget_is_visible(mainw->framedraw_preview)) {
    if (mainw->framedraw_preview!=NULL) lives_widget_set_sensitive(mainw->framedraw_preview,FALSE);
    lives_widget_context_update();
    load_rfx_preview(framedraw->rfx);
  } else framedraw_redraw(framedraw, TRUE, NULL);
}




void load_framedraw_image(LiVESPixbuf *pixbuf) {
  // this is for the single frame framedraw widget
  // it should be called whenever mainw->framedraw_bitmap changes

  weed_timecode_t tc;

  boolean needs_unlock=FALSE;

  if (mainw->framedraw_frame>cfile->frames) mainw->framedraw_frame=cfile->frames;

  if (pixbuf==NULL) {
    const char *img_ext=get_image_ext_for_type(cfile->img_type);

    // can happen if we preview for rendered generators
    if ((mainw->multitrack==NULL||mainw->current_file!=mainw->multitrack->render_file)&&mainw->framedraw_frame==0) return;

    tc=((mainw->framedraw_frame-1.))/cfile->fps*U_SECL;
    pixbuf=pull_lives_pixbuf_at_size(mainw->current_file,mainw->framedraw_frame,img_ext,tc,
                                     (double)cfile->hsize,(double)cfile->vsize,
                                     LIVES_INTERP_BEST);
  }

  if (pixbuf!=NULL) {
    if (mainw->fd_layer_orig!=NULL) {
      weed_layer_free(mainw->fd_layer_orig);
    }

    mainw->fd_layer_orig=weed_layer_new(0,0,NULL,WEED_PALETTE_END);

    if (pixbuf_to_layer(mainw->fd_layer_orig,pixbuf)) {
      mainw->do_not_free=(livespointer)lives_pixbuf_get_pixels_readonly(pixbuf);
      // might be threaded here so we need a mutex to stop other thread resetting free_fn
      pthread_mutex_lock(&mainw->free_fn_mutex);
      needs_unlock=TRUE;
      mainw->free_fn=_lives_free_with_check;
    }
    lives_object_unref(pixbuf);
    mainw->do_not_free=NULL;
    mainw->free_fn=_lives_free_normal;
    if (needs_unlock) pthread_mutex_unlock(&mainw->free_fn_mutex);

  }

  if (mainw->fd_layer!=NULL) weed_layer_free(mainw->fd_layer);
  mainw->fd_layer=NULL;

}





void redraw_framedraw_image(void) {
  lives_painter_t *cr;
  LiVESPixbuf *pixbuf;

  boolean needs_unlock=FALSE;

  int fd_width=lives_widget_get_allocation_width(mainw->framedraw);
  int fd_height=lives_widget_get_allocation_height(mainw->framedraw);

  int width,height;


  if (mainw->fd_layer_orig==NULL) return;

  if (mainw->current_file<1||cfile==NULL) return;

  width=cfile->hsize;
  height=cfile->vsize;

  calc_maxspect(fd_width,fd_height,&width,&height);

  // copy orig layer to layer
  if (mainw->fd_layer==NULL) mainw->fd_layer=weed_layer_copy(NULL,mainw->fd_layer_orig);

  // resize to correct size
  resize_layer(mainw->fd_layer, width, height, LIVES_INTERP_BEST, WEED_PALETTE_RGBA32, 0);

  // force to RGBA32
  convert_layer_palette(mainw->fd_layer,WEED_PALETTE_RGBA32,0);

  // layer to pixbuf
  pixbuf=layer_to_pixbuf(mainw->fd_layer);

  // get lives_painter for window
  cr = lives_painter_create_from_widget(mainw->framedraw);

  if (cr!=NULL) {
    // set source pixbuf for lives_painter
    lives_painter_set_source_pixbuf(cr, pixbuf, (fd_width-width)>>1, (fd_height-height)>>1);
    lives_painter_paint(cr);
    lives_painter_destroy(cr);
  }

  // convert pixbuf back to layer (layer_to_pixbuf destroys it)
  if (pixbuf_to_layer(mainw->fd_layer,pixbuf)) {
    mainw->do_not_free=(livespointer)lives_pixbuf_get_pixels_readonly(pixbuf);
    pthread_mutex_lock(&mainw->free_fn_mutex);
    needs_unlock=TRUE;
    mainw->free_fn=_lives_free_with_check;
  }

  lives_object_unref(pixbuf);
  mainw->do_not_free=NULL;
  mainw->free_fn=_lives_free_normal;
  if (needs_unlock) pthread_mutex_unlock(&mainw->free_fn_mutex);


}


// change cursor maybe when we enter or leave the framedraw window

boolean on_framedraw_enter(LiVESWidget *widget, LiVESXEventCrossing *event, lives_special_framedraw_rect_t *framedraw) {

  if (framedraw==NULL&&mainw->multitrack!=NULL) {
    framedraw=mainw->multitrack->framedraw;
    if (framedraw==NULL&&mainw->multitrack->cursor_style==LIVES_CURSOR_NORMAL)
      lives_set_cursor_style(LIVES_CURSOR_NORMAL,mainw->multitrack->play_box);
  }

  if (framedraw==NULL) return FALSE;
  if (mainw->multitrack!=NULL&&(mainw->multitrack->track_index==-1||mainw->multitrack->cursor_style!=LIVES_CURSOR_NORMAL)) return FALSE;

  switch (framedraw->type) {
  case LIVES_PARAM_SPECIAL_TYPE_RECT_DEMASK:
  case LIVES_PARAM_SPECIAL_TYPE_RECT_MULTRECT:
  case LIVES_PARAM_SPECIAL_TYPE_RECT_MULTIRECT:
    if (mainw->multitrack==NULL) {
      lives_set_cursor_style(LIVES_CURSOR_TOP_LEFT_CORNER,mainw->framedraw);
    } else {
      lives_set_cursor_style(LIVES_CURSOR_TOP_LEFT_CORNER,mainw->multitrack->play_box);
    }
    break;
  case LIVES_PARAM_SPECIAL_TYPE_SINGLEPOINT:
    if (mainw->multitrack==NULL) {
      lives_set_cursor_style(LIVES_CURSOR_CROSSHAIR,mainw->framedraw);
    } else {
      lives_set_cursor_style(LIVES_CURSOR_CROSSHAIR,mainw->multitrack->play_box);
    }
    break;

  default:
    break;

  }
  return FALSE;
}


boolean on_framedraw_leave(LiVESWidget *widget, LiVESXEventCrossing *event, lives_special_framedraw_rect_t *framedraw) {
  if (framedraw==NULL) return FALSE;
  lives_set_cursor_style(LIVES_CURSOR_NORMAL,mainw->framedraw);
  return FALSE;
}


// using these 3 functions, the user can draw on frames

boolean on_framedraw_mouse_start(LiVESWidget *widget, LiVESXEventButton *event, lives_special_framedraw_rect_t *framedraw) {
  // user clicked in the framedraw widget (or multitrack playback widget)

  int fd_height;
  int fd_width;

  int width=cfile->hsize;
  int height=cfile->vsize;

  int xstarti,ystarti;

  if (framedraw==NULL&&mainw->multitrack!=NULL) framedraw=mainw->multitrack->framedraw;

  if (framedraw==NULL) return FALSE;
  if (mainw->multitrack!=NULL&&mainw->multitrack->track_index==-1) return FALSE;

  if (framedraw==NULL&&mainw->multitrack!=NULL&&event->button==3) {
    // right click brings up context menu
    frame_context(widget,event,LIVES_INT_TO_POINTER(0));
  }

  if (event->button!=1) return FALSE;

  b1_held=TRUE;

  lives_widget_get_pointer((LiVESXDevice *)mainw->mgeom[prefs->gui_monitor>0?prefs->gui_monitor-1:0].mouse_device,
                           widget, &xstarti, &ystarti);

  if ((framedraw->type==LIVES_PARAM_SPECIAL_TYPE_RECT_MULTRECT||
       framedraw->type==LIVES_PARAM_SPECIAL_TYPE_RECT_MULTIRECT||
       framedraw->type==LIVES_PARAM_SPECIAL_TYPE_RECT_DEMASK)&&
      (mainw->multitrack==NULL||mainw->multitrack->cursor_style==0)) {
    if (mainw->multitrack==NULL) {
      lives_set_cursor_style(LIVES_CURSOR_BOTTOM_RIGHT_CORNER,widget);
    } else {
      lives_set_cursor_style(LIVES_CURSOR_BOTTOM_RIGHT_CORNER,mainw->multitrack->play_box);
    }
  }


  fd_width=lives_widget_get_allocation_width(widget);
  fd_height=lives_widget_get_allocation_height(widget);

  calc_maxspect(fd_width,fd_height,&width,&height);

  xstart=(double)xstarti-(double)(fd_width-width)/2.;
  ystart=(double)ystarti-(double)(fd_height-height)/2.;

  xstart/=(double)width;
  ystart/=(double)height;

  xcurrent=xstart;
  ycurrent=ystart;

  noupdate=TRUE;

  switch (framedraw->type) {
  case LIVES_PARAM_SPECIAL_TYPE_SINGLEPOINT:

    if (framedraw->xstart_param->dp>0)
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->xstart_param->widgets[0]),xstart);
    else
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->xstart_param->widgets[0]),(int)(xstart*(double)cfile->hsize+.5));
    if (framedraw->xstart_param->dp>0)
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->ystart_param->widgets[0]),ystart);
    else
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->ystart_param->widgets[0]),(int)(ystart*(double)cfile->vsize+.5));

    break;

  case LIVES_PARAM_SPECIAL_TYPE_RECT_MULTRECT:
  case LIVES_PARAM_SPECIAL_TYPE_RECT_MULTIRECT:
  case LIVES_PARAM_SPECIAL_TYPE_RECT_DEMASK:

    if (framedraw->xstart_param->dp>0)
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->xstart_param->widgets[0]),xstart);
    else
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->xstart_param->widgets[0]),(int)(xstart*(double)cfile->hsize+.5));
    if (framedraw->xstart_param->dp>0)
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->ystart_param->widgets[0]),ystart);
    else
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->ystart_param->widgets[0]),(int)(ystart*(double)cfile->vsize+.5));

    if (framedraw->type==LIVES_PARAM_SPECIAL_TYPE_RECT_MULTRECT) {
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->xend_param->widgets[0]),0.);
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->yend_param->widgets[0]),0.);
    } else {
      if (framedraw->xend_param->dp>0)
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->xend_param->widgets[0]),xstart);
      else
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->xend_param->widgets[0]),(int)(xstart*(double)cfile->hsize+.5));
      if (framedraw->xend_param->dp>0)
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->yend_param->widgets[0]),ystart);
      else
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->yend_param->widgets[0]),(int)(ystart*(double)cfile->vsize+.5));
    }


    break;
  default:
    break;
  }


  if (mainw->framedraw_reset!=NULL) {
    lives_widget_set_sensitive(mainw->framedraw_reset,TRUE);
  }
  if (mainw->framedraw_preview!=NULL) {
    lives_widget_set_sensitive(mainw->framedraw_preview,TRUE);
  }

  noupdate=FALSE;

  framedraw_redraw(framedraw, FALSE, NULL);

  return FALSE;
}

boolean on_framedraw_mouse_update(LiVESWidget *widget, LiVESXEventMotion *event, lives_special_framedraw_rect_t *framedraw) {
  // pointer moved in the framedraw widget
  int xcurrenti,ycurrenti;

  int fd_width,fd_height,width,height;

  if (noupdate) return FALSE;

  if (!b1_held) return FALSE;

  if (framedraw==NULL&&mainw->multitrack!=NULL) framedraw=mainw->multitrack->framedraw;
  if (framedraw==NULL) return FALSE;
  if (mainw->multitrack!=NULL&&mainw->multitrack->track_index==-1) return FALSE;

  lives_widget_get_pointer((LiVESXDevice *)mainw->mgeom[prefs->gui_monitor>0?prefs->gui_monitor-1:0].mouse_device,
                           widget, &xcurrenti, &ycurrenti);


  width=cfile->hsize;
  height=cfile->vsize;

  fd_width=lives_widget_get_allocation_width(widget);
  fd_height=lives_widget_get_allocation_height(widget);

  calc_maxspect(fd_width,fd_height,&width,&height);

  xcurrent=(double)xcurrenti-(fd_width-width)/2.;
  ycurrent=(double)ycurrenti-(fd_height-height)/2.;

  xcurrent/=(double)width;
  ycurrent/=(double)height;

  noupdate=TRUE;


  switch (framedraw->type) {
  case LIVES_PARAM_SPECIAL_TYPE_RECT_MULTRECT: {
    double xscale,yscale;

    xscale=xcurrent-xstart;
    yscale=ycurrent-ystart;

    if (xscale>0.) {
      if (framedraw->xend_param->dp>0)
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->xend_param->widgets[0]),xscale);
      else
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->xend_param->widgets[0]),(int)(xscale*(double)cfile->hsize+.5));
    } else {
      if (framedraw->xstart_param->dp>0) {
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->xend_param->widgets[0]),-xscale);
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->xstart_param->widgets[0]),xcurrent);
      } else {
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->xend_param->widgets[0]),(int)(-xscale*(double)cfile->hsize-.5));
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->xstart_param->widgets[0]),(int)(xcurrent*(double)cfile->hsize+.5));
      }
    }

    if (yscale>0.) {
      if (framedraw->yend_param->dp>0)
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->yend_param->widgets[0]),yscale);
      else
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->yend_param->widgets[0]),(int)(yscale*(double)cfile->vsize+.5));
    } else {
      if (framedraw->xstart_param->dp>0) {
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->yend_param->widgets[0]),-yscale);
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->ystart_param->widgets[0]),ycurrent);
      } else {
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->yend_param->widgets[0]),(int)(-yscale*(double)cfile->vsize-.5));
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->ystart_param->widgets[0]),(int)(ycurrent*(double)cfile->vsize+.5));
      }
    }

  }
  break;
  case LIVES_PARAM_SPECIAL_TYPE_RECT_MULTIRECT:
  case LIVES_PARAM_SPECIAL_TYPE_RECT_DEMASK:

    if (xcurrent>xstart) {
      if (framedraw->xend_param->dp>0)
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->xend_param->widgets[0]),xcurrent);
      else
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->xend_param->widgets[0]),(int)(xcurrent*(double)cfile->hsize+.5));
    } else {
      if (framedraw->xstart_param->dp>0) {
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->xend_param->widgets[0]),xstart);
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->xstart_param->widgets[0]),xcurrent);
      } else {
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->xend_param->widgets[0]),(int)(xstart*(double)cfile->hsize+.5));
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->xstart_param->widgets[0]),(int)(xcurrent*(double)cfile->hsize+.5));
      }
    }

    if (ycurrent>ystart) {
      if (framedraw->yend_param->dp>0)
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->yend_param->widgets[0]),ycurrent);
      else
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->yend_param->widgets[0]),(int)(ycurrent*(double)cfile->vsize+.5));
    } else {
      if (framedraw->xstart_param->dp>0) {
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->yend_param->widgets[0]),ystart);
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->ystart_param->widgets[0]),ycurrent);
      } else {
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->yend_param->widgets[0]),(int)(ystart*(double)cfile->vsize+.5));
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->ystart_param->widgets[0]),(int)(ycurrent*(double)cfile->vsize+.5));
      }
    }


    break;

  default:
    break;

  }

  if (mainw->framedraw_reset!=NULL) {
    lives_widget_set_sensitive(mainw->framedraw_reset,TRUE);
  }
  if (mainw->framedraw_preview!=NULL) {
    lives_widget_set_sensitive(mainw->framedraw_preview,TRUE);
  }

  noupdate=FALSE;
  framedraw_redraw(framedraw, FALSE, NULL);

  return FALSE;
}


boolean on_framedraw_mouse_reset(LiVESWidget *widget, LiVESXEventButton *event, lives_special_framedraw_rect_t *framedraw) {
  // user released the mouse button in framedraw widget
  if (event->button!=1||!b1_held) return FALSE;

  b1_held=FALSE;

  if (framedraw==NULL&&mainw->multitrack!=NULL) framedraw=mainw->multitrack->framedraw;
  if (framedraw==NULL) return FALSE;
  if (mainw->multitrack!=NULL&&mainw->multitrack->track_index==-1) return FALSE;

  if ((framedraw->type==LIVES_PARAM_SPECIAL_TYPE_RECT_MULTRECT||
       framedraw->type==LIVES_PARAM_SPECIAL_TYPE_RECT_MULTIRECT||
       framedraw->type==LIVES_PARAM_SPECIAL_TYPE_RECT_DEMASK)&&
      (mainw->multitrack==NULL||mainw->multitrack->cursor_style==0)) {
    if (mainw->multitrack==NULL) {
      lives_set_cursor_style(LIVES_CURSOR_TOP_LEFT_CORNER,widget);
    } else {
      lives_set_cursor_style(LIVES_CURSOR_TOP_LEFT_CORNER,mainw->multitrack->play_box);
    }
  }

  framedraw_redraw(framedraw, FALSE, NULL);
  return FALSE;
}


void after_framedraw_widget_changed(LiVESWidget *widget, lives_special_framedraw_rect_t *framedraw) {
  if (mainw->block_param_updates||noupdate) return;

  // redraw mask when spin values change
  framedraw_redraw(framedraw, FALSE, NULL);
  if (mainw->framedraw_reset!=NULL) {
    lives_widget_set_sensitive(mainw->framedraw_reset,TRUE);
  }
  if (mainw->framedraw_preview!=NULL) {
    lives_widget_set_sensitive(mainw->framedraw_preview,TRUE);
  }
}



void on_framedraw_reset_clicked(LiVESButton *button, lives_special_framedraw_rect_t *framedraw) {
  // reset to defaults

  noupdate=TRUE;
  if (framedraw->xend_param!=NULL) {
    if (framedraw->xend_param->dp==0)
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->xend_param->widgets[0]),(double)get_int_param(framedraw->xend_param->def));
    else
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->xend_param->widgets[0]),get_double_param(framedraw->xend_param->def));
  }
  if (framedraw->yend_param!=NULL) {
    if (framedraw->yend_param->dp==0)
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->yend_param->widgets[0]),(double)get_int_param(framedraw->yend_param->def));
    else
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->yend_param->widgets[0]),get_double_param(framedraw->yend_param->def));
  }
  if (framedraw->xstart_param!=NULL) {
    if (framedraw->xstart_param->dp==0)
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->xstart_param->widgets[0]),(double)get_int_param(framedraw->xstart_param->def));
    else
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->xstart_param->widgets[0]),get_double_param(framedraw->xstart_param->def));
  }
  if (framedraw->ystart_param!=NULL) {
    if (framedraw->ystart_param->dp==0)
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->ystart_param->widgets[0]),(double)get_int_param(framedraw->ystart_param->def));
    else
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->ystart_param->widgets[0]),get_double_param(framedraw->ystart_param->def));
  }

  if (mainw->framedraw_reset!=NULL) {
    lives_widget_set_sensitive(mainw->framedraw_reset,TRUE);
  }
  if (mainw->framedraw_preview!=NULL) {
    lives_widget_set_sensitive(mainw->framedraw_preview,TRUE);
  }

  // update widgets now
  lives_widget_context_update();

  noupdate=FALSE;

  framedraw_redraw(framedraw, FALSE, NULL);

}
