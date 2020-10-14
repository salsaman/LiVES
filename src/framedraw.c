// framedraw.c
// LiVES
// (c) G. Finch (salsaman+lives@gmail.com) 2002 - 2019
// see file COPYING for licensing details : released under the GNU GPL 3 or later

// functions for the 'framedraw' widget - lets users draw on frames :-)

#include "main.h"
#include "callbacks.h"
#include "interface.h"
#include "effects.h"
#include "cvirtual.h"
#include "framedraw.h"

// set by mouse button press
static double xstart, ystart;
static double xcurrent, ycurrent;
static double xinit, yinit;
static volatile boolean b1_held;

static volatile boolean noupdate = FALSE;

static LiVESWidget *fbord_eventbox;


static double calc_fd_scale(int width, int height) {
  double scale = 1.;

  if (width < MIN_PRE_X) {
    width = MIN_PRE_X;
  }
  if (height < MIN_PRE_Y) {
    height = MIN_PRE_Y;
  }

  if (width > MAX_PRE_X) scale = (double)width / (double)MAX_PRE_X;
  if (height > MAX_PRE_Y && (height / MAX_PRE_Y < scale)) scale = (double)height / (double)MAX_PRE_Y;
  return scale;
}


void invalidate_preview(lives_special_framedraw_rect_t *frame_draw) {
  /// this is called when a parameter in a rendered effect is changed
  /// the current preview is invalid and we must reset back to the start frame
  ///
  /// the exception is for effects which can resize, since we can only show
  /// an approximate preview anyway

  if (frame_draw->rfx->props & RFX_PROPS_MAY_RESIZE) return;

  lives_widget_set_sensitive(mainw->framedraw_preview, TRUE);
  lives_signal_handler_block(mainw->framedraw_spinbutton, mainw->fd_spin_func);
  lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->framedraw_spinbutton), cfile->start);
  lives_range_set_value(LIVES_RANGE(mainw->framedraw_scale), cfile->start);
  lives_spin_button_set_range(LIVES_SPIN_BUTTON(mainw->framedraw_spinbutton), cfile->start, cfile->start);
  mainw->framedraw_frame = cfile->start;
  lives_signal_handler_unblock(mainw->framedraw_spinbutton, mainw->fd_spin_func);
}


static void start_preview(LiVESButton *button, lives_rfx_t *rfx) {
  int i;
  char *com;

  if (fx_dialog[0]) {
    if (fx_dialog[0]->okbutton) lives_widget_set_sensitive(fx_dialog[0]->okbutton, FALSE);
    if (fx_dialog[0]->cancelbutton) lives_widget_set_sensitive(fx_dialog[0]->cancelbutton, FALSE);
  }

  if (!check_filewrite_overwrites()) {
    if (fx_dialog[0]) {
      if (fx_dialog[0]->okbutton) lives_widget_set_sensitive(fx_dialog[0]->okbutton, TRUE);
      if (fx_dialog[0]->cancelbutton) lives_widget_set_sensitive(fx_dialog[0]->cancelbutton, TRUE);
    }
    return;
  }

  clear_widget_bg(LIVES_WIDGET(mainw->framedraw), mainw->fd_surface);
  lives_widget_set_sensitive(mainw->framedraw_preview, FALSE);

  if (mainw->did_rfx_preview) {
    lives_kill_subprocesses(cfile->handle, TRUE);

    if (cfile->start == 0) {
      cfile->start = 1;
      cfile->end = cfile->frames;
    }

    do_rfx_cleanup(rfx);
  }

  com = lives_strdup_printf("%s clear_pre_files \"%s\" 2>%s", prefs->backend_sync, cfile->handle, LIVES_DEVNULL);
  lives_system(com, TRUE); // clear any .pre files from before

  for (i = 0; i < rfx->num_params; i++) {
    rfx->params[i].changed = FALSE;
  }

  mainw->cancelled = CANCEL_NONE;
  mainw->error = FALSE;

  if (rfx->num_in_channels == 0) {
    // reset values to specified
    cfile->hsize = cfile->ohsize;
    cfile->vsize = cfile->ovsize;
    cfile->fps = cfile->pb_fps;
  }

  // within do_effect() we check and if
  do_effect(rfx, TRUE); // actually start effect processing in the background
  load_rfx_preview(rfx);

  lives_widget_set_sensitive(mainw->framedraw_spinbutton, TRUE);
  lives_widget_set_sensitive(mainw->framedraw_scale, TRUE);
  if (fx_dialog[0]) {
    if (fx_dialog[0]->okbutton) lives_widget_set_sensitive(fx_dialog[0]->okbutton, TRUE);
    if (fx_dialog[0]->cancelbutton) lives_widget_set_sensitive(fx_dialog[0]->cancelbutton, TRUE);
  }
  lives_spin_button_set_range(LIVES_SPIN_BUTTON(mainw->framedraw_spinbutton), cfile->start, cfile->start + 1);
  mainw->did_rfx_preview = TRUE;
}


static void framedraw_redraw_cb(LiVESWidget *widget, lives_special_framedraw_rect_t *framedraw) {
  if (mainw->multitrack) return;
  framedraw_redraw(framedraw, mainw->fd_layer_orig);
}


static void after_framedraw_frame_spinbutton_changed(LiVESSpinButton *spinbutton, lives_special_framedraw_rect_t *framedraw) {
  // update the single frame/framedraw preview
  // after the "frame number" spinbutton has changed
  lives_signal_handler_block(mainw->framedraw_spinbutton, mainw->fd_spin_func);
  if (fx_dialog[0]) {
    if (fx_dialog[0]->okbutton) lives_widget_set_sensitive(fx_dialog[0]->okbutton, FALSE);
    if (fx_dialog[0]->cancelbutton) lives_widget_set_sensitive(fx_dialog[0]->cancelbutton, FALSE);
  }
  mainw->framedraw_frame = lives_spin_button_get_value_as_int(spinbutton);
  if (!(framedraw->rfx->props & RFX_PROPS_MAY_RESIZE)) {
    if (mainw->framedraw_preview) lives_widget_set_sensitive(mainw->framedraw_preview, FALSE);
    //lives_widget_context_update();
    load_rfx_preview(framedraw->rfx);
  } else framedraw_redraw(framedraw, NULL);
  if (fx_dialog[0]) {
    if (fx_dialog[0]->okbutton) lives_widget_set_sensitive(fx_dialog[0]->okbutton, TRUE);
    if (fx_dialog[0]->cancelbutton) lives_widget_set_sensitive(fx_dialog[0]->cancelbutton, TRUE);
  }
  lives_signal_handler_unblock(mainw->framedraw_spinbutton, mainw->fd_spin_func);
}


void framedraw_connect_spinbutton(lives_special_framedraw_rect_t *framedraw, lives_rfx_t *rfx) {
  framedraw->rfx = rfx;

  mainw->fd_spin_func = lives_signal_connect_after(LIVES_GUI_OBJECT(mainw->framedraw_spinbutton),
                        LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                        LIVES_GUI_CALLBACK(after_framedraw_frame_spinbutton_changed), framedraw);
  lives_signal_connect_after(LIVES_GUI_OBJECT(mainw->framedraw_opscale), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                             LIVES_GUI_CALLBACK(framedraw_redraw_cb), framedraw);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->framedraw_cbutton), LIVES_WIDGET_COLOR_SET_SIGNAL,
                       LIVES_GUI_CALLBACK(framedraw_redraw_cb), framedraw);
}


void framedraw_connect(lives_special_framedraw_rect_t *framedraw, int width, int height, lives_rfx_t *rfx) {
  // add mouse fn's so we can draw on frames
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->framedraw), LIVES_WIDGET_MOTION_NOTIFY_EVENT,
                       LIVES_GUI_CALLBACK(on_framedraw_mouse_update), framedraw);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->framedraw), LIVES_WIDGET_BUTTON_RELEASE_EVENT,
                       LIVES_GUI_CALLBACK(on_framedraw_mouse_reset), framedraw);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->framedraw), LIVES_WIDGET_BUTTON_PRESS_EVENT,
                       LIVES_GUI_CALLBACK(on_framedraw_mouse_start), framedraw);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->framedraw), LIVES_WIDGET_ENTER_EVENT,
                       LIVES_GUI_CALLBACK(on_framedraw_enter), framedraw);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->framedraw), LIVES_WIDGET_LEAVE_NOTIFY_EVENT,
                       LIVES_GUI_CALLBACK(on_framedraw_leave), framedraw);

  framedraw_connect_spinbutton(framedraw, rfx);

  lives_widget_set_bg_color(mainw->fd_frame, LIVES_WIDGET_STATE_NORMAL, &palette->light_red);
  lives_widget_set_bg_color(fbord_eventbox, LIVES_WIDGET_STATE_NORMAL, &palette->light_red);

  if (mainw->fd_layer) framedraw_redraw(framedraw, mainw->fd_layer_orig);
}


void framedraw_add_label(LiVESVBox *box) {
  LiVESWidget *label;

  // TRANSLATORS - Preview refers to preview window; keep this phrase short
  widget_opts.justify = LIVES_JUSTIFY_CENTER;
  label = lives_standard_label_new(_("You can click in Preview to change these values"));
  lives_box_pack_start(LIVES_BOX(box), label, FALSE, FALSE, widget_opts.packing_height / 2.);
  widget_opts.justify = LIVES_JUSTIFY_DEFAULT;
}


void framedraw_add_reset(LiVESVBox *box, lives_special_framedraw_rect_t *framedraw) {
  LiVESWidget *hbox_rst;

  framedraw_add_label(box);

  mainw->framedraw_reset = lives_standard_button_new_from_stock(LIVES_STOCK_REFRESH, NULL,
                           DEF_BUTTON_WIDTH, DEF_BUTTON_HEIGHT);
  hbox_rst = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(box), hbox_rst, FALSE, FALSE, 4. * widget_opts.scale);

  lives_button_set_label(LIVES_BUTTON(mainw->framedraw_reset), _("_Reset Values"));
  lives_box_pack_start(LIVES_BOX(hbox_rst), mainw->framedraw_reset, TRUE, FALSE, 0);
  lives_widget_set_sensitive(mainw->framedraw_reset, FALSE);

  lives_signal_connect(mainw->framedraw_reset, LIVES_WIDGET_CLICKED_SIGNAL, LIVES_GUI_CALLBACK(on_framedraw_reset_clicked),
                       framedraw);
}


static void redraw_framedraw_image(weed_layer_t *layer) {
  // layer should be mainw->fd_frame
  // size layer, convert palette
  // then draw it in mainw->fd_surface
  // changes 'layer' itself

  lives_painter_t *cr, *cr2;

  int fd_width, fd_height;
  int width, height, cx, cy;

  if (!layer) return;

  if (!CURRENT_CLIP_IS_VALID) return;

  if (!LIVES_IS_WIDGET(mainw->framedraw)) return;

  fd_width = lives_widget_get_allocation_width(mainw->framedraw);
  fd_height = lives_widget_get_allocation_height(mainw->framedraw);

  width = cfile->hsize;
  height = cfile->vsize;

  calc_maxspect(fd_width, fd_height, &width, &height);

  // resize to correct size
  resize_layer(layer, width, height, LIVES_INTERP_BEST,
               LIVES_PAINTER_COLOR_PALETTE(capable->byte_order), 0);

  cr2 = lives_painter_create_from_surface(mainw->fd_surface);

  cr = layer_to_lives_painter(layer);

  cx = (fd_width - width) / 2;
  cy = (fd_height - height) / 2;

  lives_painter_set_source_surface(cr2, lives_painter_get_target(cr), cx, cy);
  lives_painter_rectangle(cr2, cx, cy, width, height);
  lives_painter_fill(cr2);
  lives_painter_destroy(cr2);

  lives_painter_to_layer(cr, layer);
  lives_widget_queue_draw(mainw->framedraw);
}


static boolean expose_fd_event(LiVESWidget *widget, lives_painter_t *cr, livespointer user_data) {
  if (!LIVES_IS_WIDGET(widget)) return TRUE;
  //redraw_framedraw_image(mainw->fd_layer, cr);
  lives_painter_set_source_surface(cr, mainw->fd_surface, 0., 0.);
  lives_painter_paint(cr);
  return TRUE;
}


void widget_add_framedraw(LiVESVBox *box, int start, int end, boolean add_preview_button, int width, int height,
                          lives_rfx_t *rfx) {
  // adds the frame draw widget to box
  // the redraw button should be connected to an appropriate redraw function
  // after calling this function

  LiVESAdjustment *spinbutton_adj;

  LiVESWidget *vseparator;
  LiVESWidget *vbox;
  LiVESWidget *hbox;
  LiVESWidget *label;
  LiVESWidget *cbutton;
  LiVESWidget *frame;
  lives_colRGBA64_t opcol;

  double fd_scale;

  b1_held = FALSE;

  mainw->framedraw_reset = NULL;

  vseparator = lives_vseparator_new();
  lives_box_pack_start(LIVES_BOX(lives_widget_get_parent(LIVES_WIDGET(box))), vseparator, FALSE, FALSE, 0);
  lives_widget_show(vseparator);

  vbox = lives_vbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(lives_widget_get_parent(LIVES_WIDGET(box))), vbox, FALSE, FALSE, 0);
  lives_container_set_border_width(LIVES_CONTAINER(vbox), widget_opts.border_width);

  fd_scale = calc_fd_scale(width, height);
  width /= fd_scale;
  height /= fd_scale;

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, 0);

  fbord_eventbox = lives_event_box_new();
  lives_container_set_border_width(LIVES_CONTAINER(fbord_eventbox), widget_opts.border_width);

  frame = lives_standard_frame_new(_("Preview"), 0., TRUE);

  lives_box_pack_start(LIVES_BOX(hbox), frame, TRUE, TRUE, 0);

  mainw->fd_frame = frame;

  hbox = lives_hbox_new(FALSE, 0);
  lives_container_set_border_width(LIVES_CONTAINER(hbox), 0);
  lives_container_add(LIVES_CONTAINER(frame), hbox);

  if (palette->style & STYLE_1) {
    lives_widget_set_bg_color(frame, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
    lives_widget_set_bg_color(hbox, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
  }

  //mainw->framedraw = lives_event_box_new();
  mainw->framedraw = lives_standard_drawing_area_new(LIVES_GUI_CALLBACK(expose_fd_event),
                     &mainw->fd_surface);
  lives_widget_set_size_request(mainw->framedraw, width, height);

  if (palette->style & STYLE_1) {
    lives_widget_set_bg_color(hbox, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
  }

  lives_widget_set_events(mainw->framedraw, LIVES_BUTTON1_MOTION_MASK | LIVES_BUTTON_RELEASE_MASK |
                          LIVES_BUTTON_PRESS_MASK | LIVES_ENTER_NOTIFY_MASK | LIVES_LEAVE_NOTIFY_MASK);

  mainw->framedraw_frame = start;

  lives_box_pack_start(LIVES_BOX(hbox), fbord_eventbox, FALSE, FALSE, 0);

  lives_container_add(LIVES_CONTAINER(fbord_eventbox), mainw->framedraw);

  if (palette->style & STYLE_1) {
    lives_widget_set_bg_color(mainw->framedraw, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
    lives_widget_set_fg_color(mainw->framedraw, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
  }

  /* if (mainw->multitrack == NULL) */
  /*   lives_signal_connect_after(LIVES_GUI_OBJECT(mainw->framedraw), LIVES_WIDGET_EXPOSE_EVENT, */
  /*                              LIVES_GUI_CALLBACK(expose_fd_event), NULL); */

  // mask colour and opacity controls
  mainw->framedraw_maskbox = lives_hbox_new(FALSE, 2);
  lives_box_pack_start(LIVES_BOX(vbox), mainw->framedraw_maskbox, FALSE, FALSE, widget_opts.packing_height);
  label = lives_standard_label_new(_("Mask: opacity"));
  lives_box_pack_start(LIVES_BOX(mainw->framedraw_maskbox), label, FALSE, FALSE, widget_opts.packing_width);
  spinbutton_adj = lives_adjustment_new(DEF_MASK_OPACITY, 0.0, 1.0, 0.1, 0.1, 0);
  mainw->framedraw_opscale = lives_standard_hscale_new(LIVES_ADJUSTMENT(spinbutton_adj));
  lives_box_pack_start(LIVES_BOX(mainw->framedraw_maskbox), mainw->framedraw_opscale, TRUE, TRUE, 0);
  opcol = lives_rgba_col_new(0, 0, 0, 65535);
  cbutton = lives_standard_color_button_new(LIVES_BOX(mainw->framedraw_maskbox), _("color"),
            FALSE, &opcol, NULL, NULL, NULL, NULL);
  mainw->framedraw_cbutton = cbutton;

  hbox = lives_hbox_new(FALSE, 2);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

  mainw->framedraw_spinbutton = lives_standard_spin_button_new(_("_Frame"),
                                start, start, end, 1., 10., 0, LIVES_BOX(hbox), NULL);

  spinbutton_adj = lives_spin_button_get_adjustment(LIVES_SPIN_BUTTON(mainw->framedraw_spinbutton));

  mainw->framedraw_preview = lives_standard_button_new_from_stock(LIVES_STOCK_REFRESH, _("Preview"),
                             DEF_BUTTON_WIDTH, DEF_BUTTON_HEIGHT);

  lives_box_pack_start(LIVES_BOX(hbox), mainw->framedraw_preview, TRUE, FALSE, 0);
  lives_widget_set_no_show_all(mainw->framedraw_preview, TRUE);

  hbox = lives_hbox_new(FALSE, 2);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

  mainw->framedraw_scale = lives_standard_hscale_new(LIVES_ADJUSTMENT(spinbutton_adj));
  if (palette->style & STYLE_1) {
    lives_widget_set_fg_color(mainw->framedraw_scale, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
  }
  lives_box_pack_start(LIVES_BOX(hbox), mainw->framedraw_scale, TRUE, TRUE, widget_opts.border_width);
  if (!(rfx->props & RFX_PROPS_MAY_RESIZE)) {
    gtk_range_set_show_fill_level(GTK_RANGE(mainw->framedraw_scale), TRUE);
    gtk_range_set_restrict_to_fill_level(GTK_RANGE(mainw->framedraw_scale), TRUE);
    gtk_range_set_fill_level(GTK_RANGE(mainw->framedraw_scale), end > start ? (double)start + 1. :
                             (double)start);
    lives_widget_set_sensitive(mainw->framedraw_spinbutton, FALSE);
    lives_widget_set_sensitive(mainw->framedraw_scale, FALSE);
  }
  lives_signal_connect(mainw->framedraw_preview, LIVES_WIDGET_CLICKED_SIGNAL, LIVES_GUI_CALLBACK(start_preview), rfx);

  lives_widget_show_all(vbox);

  if (add_preview_button) {
    lives_widget_set_no_show_all(mainw->framedraw_preview, FALSE);
    lives_widget_show_all(mainw->framedraw_preview);
  }

  lives_widget_hide(mainw->framedraw_maskbox);

  if (start == 0) {
    mainw->fd_max_frame = 1;
  } else {
    mainw->fd_max_frame = start;
  }
}


weed_plant_t *framedraw_redraw(lives_special_framedraw_rect_t *framedraw, weed_layer_t *layer) {
  // overlay framedrawing on a layer
  // if layer is NULL then we reload the frame from current file into mainw->fd_layer_orig
  // if called from multitrack then the preview layer is passed in via mt_framedraw()
  // for overlaying we pass in mainw->fd_layer_orig again
  // from c.e, layer is mainw->fd_orig_layer
  //
  // the input layer is copied: (mainw->fd_layer_orig -> mainw->fd_layer)
  // mainw->fd_layer is drawn over, resized, gamma corrected and painted
  // the output layer is also returned (mainw->fd_layer)

  int fd_height;
  int fd_width;
  int width, height;

  double xstartf, ystartf, xendf, yendf;

  lives_painter_t *cr;

  if (!CURRENT_CLIP_IS_VALID) return NULL;

  if (framedraw->rfx->source_type == LIVES_RFX_SOURCE_RFX)
    if (noupdate) return NULL; // static volatile

  fd_width = lives_widget_get_allocation_width(mainw->framedraw);
  fd_height = lives_widget_get_allocation_height(mainw->framedraw);

  if (fd_width < 4 || fd_height < 4) return NULL;

  width = cfile->hsize;
  height = cfile->vsize;

  if (mainw->multitrack && prefs->letterbox_mt) {
    fd_width = mainw->files[mainw->multitrack->render_file]->hsize;
    fd_height = mainw->files[mainw->multitrack->render_file]->vsize;
    calc_maxspect(mainw->multitrack->play_width, mainw->multitrack->play_height,
		  &fd_width, &fd_height);
  }

  if (fd_width > width) fd_width = width;
  if (fd_height > height) fd_height = height;

  calc_maxspect(fd_width, fd_height, &width, &height);

  // copy from orig, resize

  if (!layer) {
    // forced reload: get frame from clip, and set in mainw->fd_layer_orig
    const char *img_ext;
    if (framedraw->rfx->num_in_channels > 0 && !(framedraw->rfx->props & RFX_PROPS_MAY_RESIZE)) {
      img_ext = LIVES_FILE_EXT_PRE;
    } else {
      img_ext = get_image_ext_for_type(cfile->img_type);
    }

    layer = lives_layer_new_for_frame(mainw->current_file, mainw->framedraw_frame);
    if (!pull_frame(layer, img_ext, 0)) {
      weed_plant_free(layer);
      return NULL;
    }
    if (mainw->fd_layer_orig) weed_layer_free(mainw->fd_layer_orig);
    mainw->fd_layer_orig = layer;
  } else {
    if (mainw->fd_layer_orig && mainw->fd_layer_orig != layer) {
      weed_layer_free(mainw->fd_layer_orig);
      mainw->fd_layer_orig = layer;
    }
  }

  // copy orig layer to new layer
  if (mainw->fd_layer) {
    weed_layer_free(mainw->fd_layer);
    mainw->fd_layer = NULL;
  }

  mainw->fd_layer = weed_layer_copy(NULL, layer);

  // resize to correct size
  resize_layer(mainw->fd_layer, width, height, LIVES_INTERP_BEST,
               LIVES_PAINTER_COLOR_PALETTE(capable->byte_order), 0);

  cr = layer_to_lives_painter(mainw->fd_layer);

  // draw on the lives_painter

  switch (framedraw->type) {
  case LIVES_PARAM_SPECIAL_TYPE_RECT_MULTIRECT:
  case LIVES_PARAM_SPECIAL_TYPE_RECT_DEMASK:
    if (framedraw->xstart_param->dp == 0) {
      xstartf = (double)lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(framedraw->xstart_param->widgets[0]));
      xstartf = xstartf / (double)cfile->hsize * (double)width;
    } else {
      xstartf = lives_spin_button_get_value(LIVES_SPIN_BUTTON(framedraw->xstart_param->widgets[0]));
      xstartf = xstartf * (double)width;
    }

    if (framedraw->xend_param->dp == 0) {
      xendf = (double)lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(framedraw->xend_param->widgets[0]));
      xendf = xendf / (double)cfile->hsize * (double)width;
    } else {
      xendf = lives_spin_button_get_value(LIVES_SPIN_BUTTON(framedraw->xend_param->widgets[0]));
      xendf = xendf * (double)width;
    }

    if (framedraw->ystart_param->dp == 0) {
      ystartf = (double)lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(framedraw->ystart_param->widgets[0]));
      ystartf = ystartf / (double)cfile->vsize * (double)height;
    } else {
      ystartf = lives_spin_button_get_value(LIVES_SPIN_BUTTON(framedraw->ystart_param->widgets[0]));
      ystartf = ystartf * (double)height;
    }

    if (framedraw->yend_param->dp == 0) {
      yendf = (double)lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(framedraw->yend_param->widgets[0]));
      yendf = yendf / (double)cfile->vsize * (double)height;
    } else {
      yendf = lives_spin_button_get_value(LIVES_SPIN_BUTTON(framedraw->yend_param->widgets[0]));
      yendf = yendf * (double)height;
    }

    if (framedraw->type == LIVES_PARAM_SPECIAL_TYPE_RECT_MULTIRECT) {
      lives_painter_set_source_rgb(cr, 1., 0., 0.);  // TODO - make colour configurable
      lives_painter_rectangle(cr, xstartf - 1., ystartf - 1., xendf, yendf);
      lives_painter_stroke(cr);
    } else {
      if (b1_held) {
        lives_painter_set_source_rgb(cr, 1., 0., 0.); // TODO - make colour configurable
        lives_painter_rectangle(cr, xstartf - 1., ystartf - 1., xendf - xstartf + 2., yendf - ystartf + 2.);
        lives_painter_stroke(cr);
      }
      // create a mask which is only opaque within the clipping area
      LiVESWidgetColor maskcol;
      double opacity = lives_range_get_value(LIVES_RANGE(mainw->framedraw_opscale));
      lives_color_button_get_color(LIVES_COLOR_BUTTON(mainw->framedraw_cbutton), &maskcol);
      lives_painter_rectangle(cr, 0, 0, width, height);
      lives_painter_rectangle(cr, xstartf, ystartf, xendf - xstartf + 1., yendf - ystartf + 1.);
      lives_painter_set_source_rgba(cr, LIVES_WIDGET_COLOR_SCALE(maskcol.red), LIVES_WIDGET_COLOR_SCALE(maskcol.green),
                                    LIVES_WIDGET_COLOR_SCALE(maskcol.blue), opacity);
      lives_painter_set_fill_rule(cr, LIVES_PAINTER_FILL_RULE_EVEN_ODD);
      lives_painter_fill(cr);
    }
    break;

  case LIVES_PARAM_SPECIAL_TYPE_SINGLEPOINT:
  case LIVES_PARAM_SPECIAL_TYPE_SCALEDPOINT:
    if (framedraw->type == LIVES_PARAM_SPECIAL_TYPE_SINGLEPOINT) {
      if (framedraw->xstart_param->dp == 0) {
        xstartf = (double)lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(framedraw->xstart_param->widgets[0]));
        xstartf = xstartf / (double)cfile->hsize * (double)width;
      } else {
        xstartf = lives_spin_button_get_value(LIVES_SPIN_BUTTON(framedraw->xstart_param->widgets[0]));
        xstartf *= (double)width;
      }

      if (framedraw->ystart_param->dp == 0) {
        ystartf = (double)lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(framedraw->ystart_param->widgets[0]));
        ystartf = ystartf / (double)cfile->vsize * (double)height;
      } else {
        ystartf = lives_spin_button_get_value(LIVES_SPIN_BUTTON(framedraw->ystart_param->widgets[0]));
        ystartf *= (double)height;
      }
    } else {
      if (b1_held) {
        xstartf = xcurrent * (double)width;
        ystartf = ycurrent * (double)height;
      } else {
        double xpos, ypos;
        double scale = lives_spin_button_get_value(LIVES_SPIN_BUTTON(framedraw->scale_param->widgets[0]));
        if (scale == 0.) break;

        if (framedraw->xstart_param->dp > 0)
          xpos = lives_spin_button_get_value(LIVES_SPIN_BUTTON(framedraw->xstart_param->widgets[0]));
        else
          xpos = (double)lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(framedraw->xstart_param->widgets[0]))
                 / (double)cfile->hsize;
        if (framedraw->ystart_param->dp > 0)
          ypos = lives_spin_button_get_value(LIVES_SPIN_BUTTON(framedraw->ystart_param->widgets[0]));
        else
          ypos = (double)lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(framedraw->ystart_param->widgets[0]))
                 / (double)cfile->vsize;

        xstartf = 0.5;
        ystartf = 0.5;

        if (xpos - xstartf / scale < 0.) xstartf = xpos * scale;
        else if (xpos + xstartf / scale > 1.) xstartf = 1. - (1. - xpos) * scale;
        if (ypos - ystartf / scale < 0.) ystartf = ypos * scale;
        else if (ypos + ystartf / scale > 1.) ystartf = 1. - (1. - ypos) * scale;
        xstartf *= (double)width;
        ystartf *= (double)height;
      }
    }

    // draw a crosshair
    lives_painter_set_source_rgb(cr, 1., 0., 0.); // TODO - make colour configurable

    lives_painter_move_to(cr, xstartf, ystartf - CROSSHAIR_SIZE);
    lives_painter_line_to(cr, xstartf, ystartf + CROSSHAIR_SIZE);

    lives_painter_stroke(cr);

    lives_painter_move_to(cr, xstartf - CROSSHAIR_SIZE, ystartf);
    lives_painter_line_to(cr, xstartf + CROSSHAIR_SIZE, ystartf);

    lives_painter_stroke(cr);
    break;

  default:
    break;
  }

  lives_painter_to_layer(cr, mainw->fd_layer);

  if (!mainw->multitrack)
    redraw_framedraw_image(mainw->fd_layer);
  else {
    LiVESPixbuf *pixbuf;
    int palette = weed_layer_get_palette(mainw->fd_layer);
    if (weed_palette_has_alpha(palette)) {
      palette = WEED_PALETTE_RGBA32;
    } else {
      palette = WEED_PALETTE_RGB24;
    }

    resize_layer(mainw->fd_layer, weed_layer_get_width(mainw->fd_layer_orig)
                 * weed_palette_get_pixels_per_macropixel(weed_layer_get_palette(mainw->fd_layer_orig)),
                 weed_layer_get_height(mainw->fd_layer_orig), LIVES_INTERP_BEST, palette, 0);

    convert_layer_palette(mainw->fd_layer, palette, 0);
    pixbuf = layer_to_pixbuf(mainw->fd_layer, TRUE, TRUE);
    weed_layer_nullify_pixel_data(mainw->fd_layer);
    weed_layer_free(mainw->fd_layer);
    mainw->fd_layer = NULL;
    if (pixbuf) {
      if (mainw->multitrack->frame_pixbuf != mainw->imframe) {
        if (mainw->multitrack->frame_pixbuf) lives_widget_object_unref(mainw->multitrack->frame_pixbuf);
      }
      // set frame_pixbuf, this gets painted in in expose_event
      mainw->multitrack->frame_pixbuf = pixbuf;
      set_drawing_area_from_pixbuf(mainw->play_image, pixbuf, mainw->play_surface);
      lives_widget_queue_draw(mainw->multitrack->preview_eventbox);
      lives_widget_object_unref(mainw->multitrack->frame_pixbuf);
      mainw->multitrack->frame_pixbuf = NULL;
    }
  }
  // update the widget
  return mainw->fd_layer;
}


void load_rfx_preview(lives_rfx_t *rfx) {
  // load a preview of an rfx (rendered effect) in clip editor
  weed_layer_t *layer;
  FILE *infofile = NULL;
  lives_alarm_t alarm_handle;
  ticks_t timeout;
  int tot_frames = 0;
  int retval;
  int current_file = mainw->current_file;

  const char *img_ext;

  if (mainw->framedraw_frame > mainw->fd_max_frame) {
    lives_signal_handler_block(mainw->framedraw_spinbutton, mainw->fd_spin_func);
    lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->framedraw_spinbutton), mainw->fd_max_frame);
    lives_signal_handler_unblock(mainw->framedraw_spinbutton, mainw->fd_spin_func);
    //lives_widget_context_update();
  }

  if (mainw->framedraw_frame == 0) mainw->framedraw_frame = 1;

  lives_set_cursor_style(LIVES_CURSOR_BUSY, NULL);

  clear_mainw_msg();
  THREADVAR(write_failed) = FALSE;

  if (cfile->clip_type == CLIP_TYPE_FILE && cfile->fx_frame_pump && !cfile->pumper) {
    // pull frames in background
    cfile->pumper = lives_proc_thread_create(LIVES_THRDATTR_NONE, (lives_funcptr_t)virtual_to_images,
                    -1, "iiibV", mainw->current_file,
                    cfile->undo_start, cfile->undo_end, FALSE, NULL);
  }

  if (mainw->cancelled) {
    if (cfile->pumper) {
      lives_nanosleep_until_nonzero(lives_proc_thread_cancel(cfile->pumper));
      weed_plant_free(cfile->pumper);
      cfile->pumper = NULL;
    }
    lives_set_cursor_style(LIVES_CURSOR_NORMAL, NULL);
    return;
  }

  // get message from back end processor
  alarm_handle = lives_alarm_set(LIVES_LONGER_TIMEOUT);
  while ((timeout = lives_alarm_check(alarm_handle)) > 0 && !(infofile = fopen(cfile->info_file, "r")) && !mainw->cancelled) {
    // wait until we get at least 1 frame
    //lives_widget_context_update();
    lives_nanosleep(1000);
    sched_yield();
  }
  lives_alarm_clear(alarm_handle);

  if (!timeout) {
    lives_set_cursor_style(LIVES_CURSOR_NORMAL, NULL);
    return;
  }

  if (mainw->cancelled) {
    if (infofile) fclose(infofile);
    lives_set_cursor_style(LIVES_CURSOR_NORMAL, NULL);
    if (cfile->pumper) {
      lives_nanosleep_until_nonzero(lives_proc_thread_cancel(cfile->pumper));
      weed_plant_free(cfile->pumper);
      cfile->pumper = NULL;
    }
    return;
  }

  do {
    retval = 0;
    lives_fgets(mainw->msg, MAINW_MSG_SIZE, infofile);
    if (THREADVAR(read_failed)) {
      // TODO : should check fd
      THREADVAR(read_failed) = 0;
      retval = do_read_failed_error_s_with_retry(cfile->info_file, NULL);
    }
  } while (retval == LIVES_RESPONSE_RETRY);

  fclose(infofile);

  if (lives_strncmp(mainw->msg, "completed", 9)) {
    if (rfx->num_in_channels > 0) {
      mainw->fd_max_frame = atoi(mainw->msg);
      tot_frames = cfile->end;
    } else {
      int numtok = get_token_count(mainw->msg, '|');
      if (numtok > 4) {
        char **array = lives_strsplit(mainw->msg, "|", numtok);
        mainw->fd_max_frame = atoi(array[0]);
        cfile->hsize = atoi(array[1]);
        cfile->vsize = atoi(array[2]);
        cfile->fps = cfile->pb_fps = strtod(array[3], NULL);
        if (cfile->fps == 0) cfile->fps = cfile->pb_fps = prefs->default_fps;
        tot_frames = atoi(array[4]);
        lives_strfreev(array);
      }
    }
  } else {
    tot_frames = mainw->fd_max_frame = cfile->end;
  }

  if (!(rfx->props & RFX_PROPS_MAY_RESIZE)) {
    lives_set_cursor_style(LIVES_CURSOR_NORMAL, NULL);

    if (mainw->fd_max_frame > 0) {
      int maxlen = calc_spin_button_width(1., (double)tot_frames, 0);
      lives_spin_button_set_range(LIVES_SPIN_BUTTON(mainw->framedraw_spinbutton), cfile->start, tot_frames);
      lives_entry_set_width_chars(LIVES_ENTRY(mainw->framedraw_spinbutton), maxlen);
      lives_widget_queue_draw(mainw->framedraw_spinbutton);
      lives_widget_queue_draw(mainw->framedraw_scale);
      if (rfx->num_in_channels == 0) {
        cfile->frames = tot_frames;
      }

      if (!(rfx->props & RFX_PROPS_MAY_RESIZE)) {
        gtk_range_set_fill_level(GTK_RANGE(mainw->framedraw_scale), (double)mainw->fd_max_frame);
        if (mainw->framedraw_frame > mainw->fd_max_frame) {
          lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->framedraw_spinbutton), mainw->fd_max_frame);
          lives_range_set_value(LIVES_RANGE(mainw->framedraw_scale), mainw->fd_max_frame);
          mainw->current_file = current_file;
          return;
	  // *INDENT-OFF*
        }}}}
  // *INDENT-ON*

  layer = lives_layer_new_for_frame(mainw->current_file, mainw->framedraw_frame);
  if (rfx->num_in_channels > 0 && !(rfx->props & RFX_PROPS_MAY_RESIZE)) {
    img_ext = LIVES_FILE_EXT_PRE;
  } else {
    img_ext = get_image_ext_for_type(cfile->img_type);
  }
  if (!pull_frame(layer, img_ext, 0)) {
    weed_plant_free(layer);
  } else {
    if (mainw->fd_layer_orig && mainw->fd_layer_orig != layer)
      weed_layer_free(mainw->fd_layer_orig);
    mainw->fd_layer_orig = layer;
    if (mainw->fd_layer) weed_layer_free(mainw->fd_layer);
    mainw->fd_layer = weed_layer_copy(NULL, mainw->fd_layer_orig);
    redraw_framedraw_image(mainw->fd_layer);
  }
  mainw->current_file = current_file;
}


// change cursor maybe when we enter or leave the framedraw window

boolean on_framedraw_enter(LiVESWidget * widget, LiVESXEventCrossing * event, lives_special_framedraw_rect_t *framedraw) {
  if (!framedraw && mainw->multitrack) {
    framedraw = mainw->multitrack->framedraw;
    if (!framedraw && mainw->multitrack->cursor_style == LIVES_CURSOR_NORMAL)
      lives_set_cursor_style(LIVES_CURSOR_NORMAL, mainw->multitrack->preview_eventbox);
  }

  if (!framedraw) return FALSE;
  if (mainw->multitrack && (mainw->multitrack->track_index == -1 ||
                            mainw->multitrack->cursor_style != LIVES_CURSOR_NORMAL)) return FALSE;

  switch (framedraw->type) {
  case LIVES_PARAM_SPECIAL_TYPE_RECT_DEMASK:
  case LIVES_PARAM_SPECIAL_TYPE_RECT_MULTIRECT:
    if (!mainw->multitrack) {
      lives_set_cursor_style(LIVES_CURSOR_TOP_LEFT_CORNER, mainw->framedraw);
    } else {
      lives_set_cursor_style(LIVES_CURSOR_TOP_LEFT_CORNER, mainw->multitrack->preview_eventbox);
    }
    break;

  case LIVES_PARAM_SPECIAL_TYPE_SINGLEPOINT:
  case LIVES_PARAM_SPECIAL_TYPE_SCALEDPOINT:
    if (!mainw->multitrack) {
      lives_set_cursor_style(LIVES_CURSOR_CROSSHAIR, mainw->framedraw);
    } else {
      lives_set_cursor_style(LIVES_CURSOR_CROSSHAIR, mainw->multitrack->preview_eventbox);
    }
    break;

  default:
    break;
  }
  return FALSE;
}

boolean on_framedraw_leave(LiVESWidget * widget, LiVESXEventCrossing * event, lives_special_framedraw_rect_t *framedraw) {
  if (!framedraw) return FALSE;
  lives_set_cursor_style(LIVES_CURSOR_NORMAL, mainw->framedraw);
  return FALSE;
}


// using these 3 functions, the user can draw on frames

boolean on_framedraw_mouse_start(LiVESWidget * widget, LiVESXEventButton * event, lives_special_framedraw_rect_t *framedraw) {
  // user clicked in the framedraw widget (or multitrack playback widget)
  double xend, yend;

  int fd_height;
  int fd_width;

  int width = cfile->hsize;
  int height = cfile->vsize;

  int xstarti, ystarti;

  if (!framedraw && mainw->multitrack) framedraw = mainw->multitrack->framedraw;

  if (!framedraw) return FALSE;
  if (mainw->multitrack && mainw->multitrack->track_index == -1) return FALSE;

  if (!framedraw && mainw->multitrack && event->button == 3) {
    // right click brings up context menu
    frame_context(widget, event, LIVES_INT_TO_POINTER(0));
  }

  if (event->button != 1) return FALSE;

  b1_held = TRUE;

  lives_widget_get_pointer((LiVESXDevice *)mainw->mgeom[widget_opts.monitor].mouse_device,
                           widget, &xstarti, &ystarti);
  xstarti -= widget_opts.border_width;

  if ((framedraw->type == LIVES_PARAM_SPECIAL_TYPE_RECT_MULTIRECT ||
       framedraw->type == LIVES_PARAM_SPECIAL_TYPE_RECT_DEMASK) &&
      (!mainw->multitrack || mainw->multitrack->cursor_style == 0)) {
    if (!mainw->multitrack) {
      lives_set_cursor_style(LIVES_CURSOR_BOTTOM_RIGHT_CORNER, widget);
    } else {
      lives_set_cursor_style(LIVES_CURSOR_BOTTOM_RIGHT_CORNER, mainw->multitrack->preview_eventbox);
    }
  }

  fd_width = lives_widget_get_allocation_width(widget);
  fd_height = lives_widget_get_allocation_height(widget);

  calc_maxspect(fd_width, fd_height, &width, &height);

  xstart = (double)xstarti - (double)(fd_width - width) / 2.;
  ystart = (double)ystarti - (double)(fd_height - height) / 2.;

  xstart /= (double)(width - 1);
  ystart /= (double)(height - 1);

  xend = xcurrent = xstart;
  yend = ycurrent = ystart;

  noupdate = TRUE;

  switch (framedraw->type) {
  case LIVES_PARAM_SPECIAL_TYPE_SCALEDPOINT: {
    // current center is xinit, yinit
    // ystart, xstart equal cursor (crosshair posn.)

    if (framedraw->xstart_param->dp > 0)
      xinit = lives_spin_button_get_value(LIVES_SPIN_BUTTON(framedraw->xstart_param->widgets[0]));
    else
      xinit = (double)lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(framedraw->xstart_param->widgets[0]))
              / (double)cfile->hsize;
    if (framedraw->ystart_param->dp > 0)
      yinit = lives_spin_button_get_value(LIVES_SPIN_BUTTON(framedraw->ystart_param->widgets[0]));
    else
      yinit = (double)lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(framedraw->ystart_param->widgets[0]))
              / (double)cfile->vsize;
  }
  break;

  case LIVES_PARAM_SPECIAL_TYPE_RECT_MULTIRECT:
    xend = yend = 0.;

  case LIVES_PARAM_SPECIAL_TYPE_RECT_DEMASK:
    if (framedraw->xstart_param->dp > 0)
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->xstart_param->widgets[0]), xstart);
    else
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->xstart_param->widgets[0]),
                                  (int)(xstart * (double)cfile->hsize + .5));
    if (framedraw->xstart_param->dp > 0)
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->ystart_param->widgets[0]), ystart);
    else
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->ystart_param->widgets[0]),
                                  (int)(ystart * (double)cfile->vsize + .5));

    if (framedraw->xend_param->dp > 0)
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->xend_param->widgets[0]), xend);
    else
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->xend_param->widgets[0]), (int)(xend * (double)cfile->hsize + .5));
    if (framedraw->xend_param->dp > 0)
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->yend_param->widgets[0]), yend);
    else
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->yend_param->widgets[0]), (int)(yend * (double)cfile->vsize + .5));

    break;

  default:
    break;
  }

  if (mainw->framedraw_reset) {
    lives_widget_set_sensitive(mainw->framedraw_reset, TRUE);
  }
  if (mainw->framedraw_preview) {
    lives_widget_set_sensitive(mainw->framedraw_preview, TRUE);
  }

  noupdate = FALSE;

  framedraw_redraw(framedraw, mainw->fd_layer_orig);

  return FALSE;
}

boolean on_framedraw_mouse_update(LiVESWidget * widget, LiVESXEventMotion * event, lives_special_framedraw_rect_t *framedraw) {
  // pointer moved in the framedraw widget
  int xcurrenti, ycurrenti;

  int fd_width, fd_height, width, height;

  if (noupdate) return FALSE;

  if (!b1_held) return FALSE;

  if (!framedraw && mainw->multitrack) framedraw = mainw->multitrack->framedraw;
  if (!framedraw) return FALSE;
  if (mainw->multitrack && mainw->multitrack->track_index == -1) return FALSE;

  lives_widget_get_pointer((LiVESXDevice *)mainw->mgeom[widget_opts.monitor].mouse_device,
                           widget, &xcurrenti, &ycurrenti);
  xcurrenti -= widget_opts.border_width;

  width = cfile->hsize;
  height = cfile->vsize;

  fd_width = lives_widget_get_allocation_width(widget);
  fd_height = lives_widget_get_allocation_height(widget);

  calc_maxspect(fd_width, fd_height, &width, &height);

  xcurrent = (double)xcurrenti - (fd_width - width) / 2.;
  ycurrent = (double)ycurrenti - (fd_height - height) / 2.;

  xcurrent /= (double)(width - 1);
  ycurrent /= (double)(height - 1);

  noupdate = TRUE;

  switch (framedraw->type) {
  case LIVES_PARAM_SPECIAL_TYPE_RECT_MULTIRECT: {
    // end parameters provide a scale relative to the output channel
    double xscale, yscale;

    xscale = xcurrent - xstart;
    yscale = ycurrent - ystart;

    if (xscale > 0.) {
      if (framedraw->xend_param->dp > 0)
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->xend_param->widgets[0]), xscale);
      else
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->xend_param->widgets[0]),
                                    (int)(xscale * (double)cfile->hsize + .5));
    } else {
      if (framedraw->xstart_param->dp > 0) {
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->xend_param->widgets[0]), -xscale);
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->xstart_param->widgets[0]), xcurrent);
      } else {
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->xend_param->widgets[0]),
                                    (int)(-xscale * (double)cfile->hsize - .5));
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->xstart_param->widgets[0]),
                                    (int)(xcurrent * (double)cfile->hsize + .5));
      }
    }

    if (yscale > 0.) {
      if (framedraw->yend_param->dp > 0)
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->yend_param->widgets[0]), yscale);
      else
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->yend_param->widgets[0]),
                                    (int)(yscale * (double)cfile->vsize + .5));
    } else {
      if (framedraw->xstart_param->dp > 0) {
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->yend_param->widgets[0]), -yscale);
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->ystart_param->widgets[0]), ycurrent);
      } else {
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->yend_param->widgets[0]),
                                    (int)(-yscale * (double)cfile->vsize - .5));
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->ystart_param->widgets[0]),
                                    (int)(ycurrent * (double)cfile->vsize + .5));
      }
    }
  }
  break;

  case LIVES_PARAM_SPECIAL_TYPE_RECT_DEMASK:
    // end parameters provide the absolute point, in output channel ratio
    if (xcurrent > xstart) {
      if (framedraw->xend_param->dp > 0)
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->xend_param->widgets[0]), xcurrent);
      else
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->xend_param->widgets[0]),
                                    (int)(xcurrent * (double)cfile->hsize + .5));
    } else {
      if (framedraw->xstart_param->dp > 0) {
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->xend_param->widgets[0]), xstart);
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->xstart_param->widgets[0]), xcurrent);
      } else {
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->xend_param->widgets[0]),
                                    (int)(xstart * (double)cfile->hsize + .5));
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->xstart_param->widgets[0]),
                                    (int)(xcurrent * (double)cfile->hsize + .5));
      }
    }

    if (ycurrent > ystart) {
      if (framedraw->yend_param->dp > 0)
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->yend_param->widgets[0]), ycurrent);
      else
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->yend_param->widgets[0]),
                                    (int)(ycurrent * (double)cfile->vsize + .5));
    } else {
      if (framedraw->xstart_param->dp > 0) {
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->yend_param->widgets[0]), ystart);
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->ystart_param->widgets[0]), ycurrent);
      } else {
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->yend_param->widgets[0]),
                                    (int)(ystart * (double)cfile->vsize + .5));
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->ystart_param->widgets[0]),
                                    (int)(ycurrent * (double)cfile->vsize + .5));
      }
    }
    break;

  case LIVES_PARAM_SPECIAL_TYPE_SCALEDPOINT: {
    // current center is xinit, yinit
    // ystart, xstart equal cursor (crosshair posn.)
    double offs_x, offs_y;
    double scale = lives_spin_button_get_value(LIVES_SPIN_BUTTON(framedraw->scale_param->widgets[0]));
    if (scale == 0.) break;
    offs_x = (xstart - xcurrent) / scale;
    offs_y = (ystart - ycurrent) / scale;

    if (xinit + offs_x < .5 / scale) offs_x = .5 / scale - xinit;
    if (xinit + offs_x > 1. - .5 / scale) offs_x = 1. - .5 / scale - xinit;
    if (yinit + offs_y < .5 / scale) offs_y = .5 / scale - yinit;
    if (yinit + offs_y > 1. - .5 / scale) offs_y = 1. - .5 / scale - yinit;

    if (framedraw->xstart_param->dp > 0)
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->xstart_param->widgets[0]), xinit + offs_x);
    else
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->xstart_param->widgets[0]), (int)((xinit + offs_x)
                                  * (double)cfile->hsize + .5));
    if (framedraw->xstart_param->dp > 0)
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->ystart_param->widgets[0]), yinit + offs_y);
    else
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->ystart_param->widgets[0]), (int)((yinit + offs_y)
                                  * (double)cfile->vsize + .5));
  }
  break;

  default:
    break;
  }

#if !GTK_CHECK_VERSION(3, 0, 0)
  lives_widget_context_update();
#endif

  if (mainw->framedraw_reset) {
    lives_widget_set_sensitive(mainw->framedraw_reset, TRUE);
  }
  if (mainw->framedraw_preview) {
    lives_widget_set_sensitive(mainw->framedraw_preview, TRUE);
  }

  noupdate = FALSE;
  framedraw_redraw(framedraw, mainw->fd_layer_orig);

  return FALSE;
}

boolean on_framedraw_mouse_reset(LiVESWidget * widget, LiVESXEventButton * event, lives_special_framedraw_rect_t *framedraw) {
  // user released the mouse button in framedraw widget
  int xcurrenti, ycurrenti;
  int fd_width, fd_height, width, height;

  if (event->button != 1 || !b1_held) return FALSE;

  b1_held = FALSE;

  if (!framedraw && mainw->multitrack) framedraw = mainw->multitrack->framedraw;
  if (!framedraw) return FALSE;
  if (mainw->multitrack && mainw->multitrack->track_index == -1) return FALSE;

  lives_widget_get_pointer((LiVESXDevice *)mainw->mgeom[widget_opts.monitor].mouse_device,
                           widget, &xcurrenti, &ycurrenti);
  xcurrenti -= widget_opts.border_width;

  width = cfile->hsize;
  height = cfile->vsize;

  fd_width = lives_widget_get_allocation_width(widget);
  fd_height = lives_widget_get_allocation_height(widget);

  calc_maxspect(fd_width, fd_height, &width, &height);

  xcurrent = (double)xcurrenti - (fd_width - width) / 2.;
  ycurrent = (double)ycurrenti - (fd_height - height) / 2.;

  xcurrent /= (double)(width - 1);
  ycurrent /= (double)(height - 1);

  switch (framedraw->type) {
  case LIVES_PARAM_SPECIAL_TYPE_RECT_MULTIRECT:
  case LIVES_PARAM_SPECIAL_TYPE_RECT_DEMASK:
    if (!mainw->multitrack) {
      lives_set_cursor_style(LIVES_CURSOR_TOP_LEFT_CORNER, widget);
    } else if (mainw->multitrack->cursor_style == 0) {
      lives_set_cursor_style(LIVES_CURSOR_TOP_LEFT_CORNER, mainw->multitrack->preview_eventbox);
    }
    break;

  case LIVES_PARAM_SPECIAL_TYPE_SCALEDPOINT: {
    double offs_x, offs_y, scale, xend, yend;
    if (noupdate) break;

    scale = lives_spin_button_get_value(LIVES_SPIN_BUTTON(framedraw->scale_param->widgets[0]));
    if (scale == 0.) break;

    if (framedraw->xstart_param->dp > 0)
      xend = lives_spin_button_get_value(LIVES_SPIN_BUTTON(framedraw->xstart_param->widgets[0]));
    else
      xend = (double)lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(framedraw->xstart_param->widgets[0]))
             / (double)cfile->hsize;
    if (framedraw->ystart_param->dp > 0)
      yend = lives_spin_button_get_value(LIVES_SPIN_BUTTON(framedraw->ystart_param->widgets[0]));
    else
      yend = (double)lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(framedraw->ystart_param->widgets[0]))
             / (double)cfile->vsize;

    if (xend == xinit && yend == yinit && xcurrent == xstart && ycurrent == ystart) {
      /// the focus is at xend, yend, but the crosshair may be offcenter. We need its position, and then we can find the offset to
      /// xcurrent, ycurrent. The offset is then added to the center.
      xstart = 0.5;
      ystart = 0.5;

      if (xend - xstart / scale < 0.) xstart = xend * scale;
      else if (xend + xstart / scale > 1.) xstart = 1. - (1. - xend) * scale;
      if (yend - ystart / scale < 0.) ystart = yend * scale;
      else if (yend + ystart / scale > 1.) ystart = 1. - (1. - yend) * scale;

      offs_x = (xcurrent - xstart) / scale;
      offs_y = (ycurrent - ystart) / scale;

      if (framedraw->xstart_param->dp > 0)
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->xstart_param->widgets[0]), xend + offs_x);
      else
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->xstart_param->widgets[0]), (int)((xend + offs_x)
                                    * (double)cfile->hsize + .5));
      if (framedraw->xstart_param->dp > 0)
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->ystart_param->widgets[0]), yend + offs_y);
      else
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->ystart_param->widgets[0]), (int)((yend + offs_y)
                                    * cfile->vsize + .5));
    }
  }
  break;

  case LIVES_PARAM_SPECIAL_TYPE_SINGLEPOINT:
    if (framedraw->xstart_param->dp > 0)
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->xstart_param->widgets[0]), xstart);
    else
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->xstart_param->widgets[0]),
                                  (int)(xstart * (double)cfile->hsize + .5));
    if (framedraw->xstart_param->dp > 0)
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->ystart_param->widgets[0]), ystart);
    else
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->ystart_param->widgets[0]),
                                  (int)(ystart * (double)cfile->vsize + .5));
    break;

  default:
    break;
  }
  framedraw_redraw(framedraw, mainw->fd_layer_orig);
  return FALSE;
}

boolean on_framedraw_scroll(LiVESWidget * widget, LiVESXEventScroll * event,
                            lives_special_framedraw_rect_t *framedraw) {
  if (!framedraw && mainw->multitrack) framedraw = mainw->multitrack->framedraw;
  if (!framedraw) return FALSE;
  if (mainw->multitrack && mainw->multitrack->track_index == -1) return FALSE;

  if (framedraw->type == LIVES_PARAM_SPECIAL_TYPE_SCALEDPOINT) {
    LiVESSpinButton *spb = (LiVESSpinButton *)framedraw->scale_param->widgets[0];
    LiVESAdjustment *adj = lives_spin_button_get_adjustment(spb);
    double step = lives_adjustment_get_step_increment(adj);
    double scale = lives_spin_button_get_value(spb);
    if (lives_get_scroll_direction(event) == LIVES_SCROLL_UP) scale += step;
    if (lives_get_scroll_direction(event) == LIVES_SCROLL_DOWN) scale -= step;
    lives_spin_button_set_value(spb, scale);
  }
  return FALSE;
}


void after_framedraw_widget_changed(LiVESWidget * widget, lives_special_framedraw_rect_t *framedraw) {
  if (mainw->block_param_updates || noupdate) return;

  // redraw mask when spin values change
  framedraw_redraw(framedraw, mainw->fd_layer_orig);
  if (mainw->framedraw_reset) {
    lives_widget_set_sensitive(mainw->framedraw_reset, TRUE);
  }
  if (mainw->framedraw_preview) {
    lives_widget_set_sensitive(mainw->framedraw_preview, TRUE);
  }
}


void on_framedraw_reset_clicked(LiVESButton * button, lives_special_framedraw_rect_t *framedraw) {
  // reset to defaults
  if (fx_dialog[0]) {
    if (fx_dialog[0]->okbutton) lives_widget_set_sensitive(fx_dialog[0]->okbutton, FALSE);
    if (fx_dialog[0]->cancelbutton) lives_widget_set_sensitive(fx_dialog[0]->cancelbutton, FALSE);
  }

  noupdate = TRUE;
  if (framedraw->xend_param) {
    if (framedraw->xend_param->dp == 0)
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->xend_param->widgets[0]),
                                  (double)get_int_param(framedraw->xend_param->def));
    else
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->xend_param->widgets[0]), get_double_param(framedraw->xend_param->def));
  }
  if (framedraw->yend_param) {
    if (framedraw->yend_param->dp == 0)
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->yend_param->widgets[0]),
                                  (double)get_int_param(framedraw->yend_param->def));
    else
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->yend_param->widgets[0]), get_double_param(framedraw->yend_param->def));
  }
  if (framedraw->xstart_param) {
    if (framedraw->xstart_param->dp == 0)
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->xstart_param->widgets[0]),
                                  (double)get_int_param(framedraw->xstart_param->def));
    else
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->xstart_param->widgets[0]),
                                  get_double_param(framedraw->xstart_param->def));
  }
  if (framedraw->ystart_param) {
    if (framedraw->ystart_param->dp == 0)
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->ystart_param->widgets[0]),
                                  (double)get_int_param(framedraw->ystart_param->def));
    else
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(framedraw->ystart_param->widgets[0]),
                                  get_double_param(framedraw->ystart_param->def));
  }

  if (mainw->framedraw_reset) {
    lives_widget_set_sensitive(mainw->framedraw_reset, TRUE);
  }
  if (mainw->framedraw_preview) {
    lives_widget_set_sensitive(mainw->framedraw_preview, TRUE);
  }

  // update widgets now
  //lives_widget_context_update();

  noupdate = FALSE;

  framedraw_redraw(framedraw, mainw->fd_layer_orig);
  if (fx_dialog[0]) {
    if (fx_dialog[0]->okbutton) lives_widget_set_sensitive(fx_dialog[0]->okbutton, TRUE);
    if (fx_dialog[0]->cancelbutton) lives_widget_set_sensitive(fx_dialog[0]->cancelbutton, TRUE);
  }
}
