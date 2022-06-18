// interface.c
// LiVES
// (c) G. Finch 2003 - 2020 <salsaman+lives@gmail.com>
// Released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#include "main.h"
#include "callbacks.h"
#include "interface.h"
#include "paramwindow.h"
#include "merge.h"
#include "resample.h"
#include "startup.h"
#include "omc-learn.h" // for OSC_NOTIFY mapping

// functions called in multitrack.c
extern void multitrack_preview_clicked(LiVESButton *, livespointer user_data);
extern void mt_change_disp_tracks_ok(LiVESButton *, livespointer user_data);

static void dsu_fill_details(LiVESWidget *widget, livespointer data);
static void qslider_changed(LiVESWidget *slid, livespointer data);

void add_suffix_check(LiVESBox *box, const char *ext) {
  char *ltext;

  LiVESWidget *checkbutton;

  if (!ext) ltext = (_("Let LiVES set the _file extension"));
  else ltext = lives_strdup_printf(_("Let LiVES set the _file extension (.%s)"), ext);
  checkbutton = lives_standard_check_button_new(ltext, mainw->fx1_bool, box, NULL);
  lives_free(ltext);
  lives_signal_sync_connect_after(LIVES_GUI_OBJECT(checkbutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                                  LIVES_GUI_CALLBACK(on_boolean_toggled), &mainw->fx1_bool);
}


LiVESWidget *add_deinterlace_checkbox(LiVESBox *for_deint) {
  char *tmp, *tmp2;
  LiVESWidget *hbox = lives_hbox_new(FALSE, 0);
  LiVESWidget *checkbutton = lives_standard_check_button_new((tmp = (_("Apply _Deinterlace"))), mainw->open_deint,
                             LIVES_BOX(hbox),
                             (tmp2 = (_("If this is set, frames will be deinterlaced as they are imported."))));
  lives_free(tmp); lives_free(tmp2);

  if (LIVES_IS_HBOX(for_deint)) {
    lives_box_pack_start(for_deint, hbox, FALSE, FALSE, widget_opts.packing_width);
    if (LIVES_IS_BUTTON_BOX(for_deint)) lives_button_box_make_first(LIVES_BUTTON_BOX(for_deint), hbox);
  } else lives_box_pack_start(for_deint, hbox, FALSE, FALSE, widget_opts.packing_height);

  lives_signal_sync_connect_after(LIVES_GUI_OBJECT(checkbutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                                  LIVES_GUI_CALLBACK(on_boolean_toggled), &mainw->open_deint);

  lives_widget_show_all(LIVES_WIDGET(for_deint));

  return hbox;
}


void show_playbar_labels(int clipno) {
  lives_clip_t *sfile = mainw->files[clipno];
  char *tmp, *tmpch;
  char *str_video = (_("Video")), *str_opening;
  boolean hhr, hvb, hla, hra;

  lives_label_set_text(LIVES_LABEL(mainw->vidbar), str_video);
  tmp = get_achannel_name(2, 0);
  lives_label_set_text(LIVES_LABEL(mainw->laudbar), tmp);
  lives_free(tmp);
  tmp = get_achannel_name(2, 1);
  lives_label_set_text(LIVES_LABEL(mainw->raudbar), tmp);
  lives_free(tmp);

  tmp = (_("(No video)"));

  if (palette->style & STYLE_1) {
    hhr = hvb = hra = TRUE;
    hla = FALSE;
  } else {
    hhr = hvb = hla = hra = FALSE;
  }

  if (!IS_VALID_CLIP(clipno)) {
    lives_label_set_text(LIVES_LABEL(mainw->vidbar), tmp);
    lives_free(tmp);

    lives_free(str_video);
    hhr = hvb = hla = hra = TRUE;
    goto showhide;
  }

  str_opening = (_("[opening...]"));

  if (CLIP_HAS_VIDEO(clipno)) {
    if (sfile->opening_loc || (sfile->frames == 123456789 && sfile->opening)) {
      lives_free(tmp);
      tmp = lives_strdup_printf(_("%s %s"), str_video, str_opening);
    } else {
      if (sfile->fps > 0.) {
        sfile->video_time = sfile->frames / sfile->fps;
      }
      if (sfile->video_time > 0.) {
        lives_free(tmp);
        tmp = lives_strdup_printf(_("%s [%.2f sec]"), str_video, sfile->video_time);
      } else {
        if (sfile->video_time <= 0. && sfile->frames > 0) {
          lives_free(tmp);
          tmp = (_("(Undefined)"));
        }
      }
    }
    lives_label_set_text(LIVES_LABEL(mainw->vidbar), tmp);
    lives_free(tmp);

    hhr = hvb = FALSE;
  }

  lives_free(str_video);

  if (!CLIP_HAS_AUDIO(clipno)) {
    tmp = (_("(No audio)"));
  } else {
    hhr = FALSE;

    tmpch = get_achannel_name(sfile->achans, 0);
    if (sfile->opening_audio) {
      tmp = lives_strdup_printf(_("%s %s"), tmpch, str_opening);
    } else {
      tmp = lives_strdup_printf(_("%s [%.2f sec]"), tmpch, sfile->laudio_time);
    }
    lives_free(tmpch);
  }

  lives_label_set_text(LIVES_LABEL(mainw->laudbar), tmp);
  lives_free(tmp);

  if (sfile->achans > 1) {
    tmpch = get_achannel_name(sfile->achans, 1);
    if (sfile->opening_audio) {
      tmp = lives_strdup_printf(_("%s %s"), tmpch, str_opening);
    } else {
      tmp = lives_strdup_printf(_("%s [%.2f sec]"), tmpch, sfile->raudio_time);
    }
    lives_free(tmpch);
    lives_label_set_text(LIVES_LABEL(mainw->raudbar), tmp);
    lives_free(tmp);
    hra = FALSE;
  }

  lives_free(str_opening);

showhide:
  if (!hhr && !lives_widget_is_visible(mainw->hruler)) lives_widget_show(mainw->hruler);
  else if (hhr && lives_widget_is_visible(mainw->hruler)) lives_widget_hide(mainw->hruler);
  if (!hvb && !lives_widget_is_visible(mainw->vidbar)) lives_widget_show(mainw->vidbar);
  else if (hvb && lives_widget_is_visible(mainw->vidbar)) lives_widget_hide(mainw->vidbar);
  if (!hla && !lives_widget_is_visible(mainw->laudbar)) lives_widget_show(mainw->laudbar);
  else if (hla && lives_widget_is_visible(mainw->laudbar)) lives_widget_hide(mainw->laudbar);
  if (!hra && !lives_widget_is_visible(mainw->raudbar)) lives_widget_show(mainw->raudbar);
  else if (hra && lives_widget_is_visible(mainw->raudbar)) lives_widget_hide(mainw->raudbar);
}


void clear_tbar_bgs(int posx, int posy, int width, int height, int which) {
  // empirically we need to draw wider
  posx -= OVERDRAW_MARGIN;
  if (width > 0) width += OVERDRAW_MARGIN;

  if (posx < 0) posx = 0;
  if (posy < 0) posy = 0;

  if (which == 0 || which == 2) {
    if (mainw->laudio_drawable) {
      clear_widget_bg_area(mainw->laudio_draw, mainw->laudio_drawable, posx, posy, width, height);
    }
  }

  if (which == 0 || which == 3) {
    if (mainw->raudio_drawable) {
      clear_widget_bg_area(mainw->raudio_draw, mainw->raudio_drawable, posx, posy, width, height);
    }
  }

  if (which == 0 || which == 1) {
    clear_widget_bg_area(mainw->video_draw, mainw->video_drawable, posx, posy, width, height);
  }
}


double lives_ce_update_timeline(int frame, double x) {
  // update clip editor timeline
  // sets real_pointer_time and pointer_time
  // if frame == 0 then x must be a time value

  // returns the pointer time (quantised to frame)

  static int last_current_file = -1;

  if (!prefs->show_gui) return 0.;

  if (lives_widget_get_allocation_width(mainw->vidbar) <= 0) {
    return 0.;
  }

  if (!CURRENT_CLIP_IS_VALID) {
    if (!prefs->hide_framebar) {
      lives_entry_set_text(LIVES_ENTRY(mainw->framecounter), "");
    }
    clear_tbar_bgs(0, 0, 0, 0, 0);
    show_playbar_labels(-1);
    return -1.;
  }

  if (x < 0.) x = 0.;

  if (frame == 0) frame = calc_frame_from_time4(mainw->current_file, x);

  x = calc_time_from_frame(mainw->current_file, frame);
  if (x > CLIP_TOTAL_TIME(mainw->current_file)) x = CLIP_TOTAL_TIME(mainw->current_file);
  cfile->real_pointer_time = x;

  if (cfile->frames > 0 && frame > cfile->frames) frame = cfile->frames;
  x = calc_time_from_frame(mainw->current_file, frame);
  cfile->pointer_time = x;

  cfile->frameno = cfile->last_frameno = frame;
  if (cfile->achans) {
    cfile->aseek_pos = (off64_t)((double)(cfile->real_pointer_time * cfile->arate) * cfile->achans *
                                 (cfile->asampsize / 8));
    if (cfile->aseek_pos > cfile->afilesize) cfile->aseek_pos = 0.;
  }

#ifndef ENABLE_GIW_3
  lives_ruler_set_value(LIVES_RULER(mainw->hruler), x);
  lives_widget_queue_draw_if_visible(mainw->hruler);
#endif

  if (!LIVES_IS_PLAYING && prefs->show_gui && !prefs->hide_framebar && cfile->frames > 0) {
    char *framecount;
    if (cfile->frames > 0) framecount = lives_strdup_printf("%9d / %d", frame, cfile->frames);
    else framecount = lives_strdup_printf("%9d", frame);
    lives_entry_set_text(LIVES_ENTRY(mainw->framecounter), framecount);
    lives_free(framecount);
  }

  if (!LIVES_IS_PLAYING && mainw->play_window && cfile->is_loaded && !mainw->multitrack) {
    if (mainw->prv_link == PRV_PTR && mainw->preview_frame != frame) {
      if (cfile->frames > 0) {
        cfile->frameno = frame;
        load_preview_image(FALSE);
      }
    }
  }

  if (mainw->is_ready && !LIVES_IS_PLAYING && !prefs->hide_framebar && mainw->current_file != last_current_file) {
    lives_signal_handler_block(mainw->spinbutton_pb_fps, mainw->pb_fps_func);
    lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_pb_fps), cfile->pb_fps);
    lives_signal_handler_unblock(mainw->spinbutton_pb_fps, mainw->pb_fps_func);
  }

  //do_tl_redraw(NULL, LIVES_INT_TO_POINTER(mainw->current_file));

  last_current_file = mainw->current_file;
  return cfile->pointer_time;
}


boolean update_timer_bars(int posx, int posy, int width, int height, int which) {
  // update the on-screen timer bars,
  // and if we are not playing,
  // get play times for video, audio channels, and total (longest) time

  // refresh = reread audio waveforms

  // which 0 = all, 1 = vidbar, 2 = laudbar, 3 = raudbar

  lives_painter_t *cr = NULL;
  lives_clip_t *sfile = cfile;
  char *filename;

  double allocwidth;
  double atime;

  double y = 0., scalex;

  boolean is_thread = FALSE;
  int start, offset_left = 0, offset_right = 0, offset_end;
  int lpos = -9999, pos;

  int current_file = mainw->current_file, fileno;
  int xwidth, zwidth;
  int afd = -1;
  int bar_height;

  int i;

  if (CURRENT_CLIP_IS_VALID && cfile->cb_src != -1) {
    mainw->current_file = cfile->cb_src;
    sfile = cfile;
  }

  fileno = mainw->current_file;

  if (!IS_VALID_CLIP(fileno) || (IS_VALID_CLIP(fileno) && sfile->fps == 0.)
      || mainw->foreign || mainw->multitrack || mainw->recoverable_layout) {
    goto bail;
  }

  if (mainw->drawtl_thread && THREADVAR(tinfo) == mainw->drawtl_thread) {
    is_thread = TRUE;
  }

  if (mainw->current_file != fileno
      || (is_thread && lives_proc_thread_get_cancelled(mainw->drawtl_thread))) goto bail;

  if (!LIVES_IS_PLAYING) get_total_time(sfile);

  if (!mainw->is_ready || !prefs->show_gui) goto bail;

  if (mainw->current_file != fileno
      || (is_thread && lives_proc_thread_get_cancelled(mainw->drawtl_thread))) goto bail;

  // draw timer bars
  // first the background
  clear_tbar_bgs(posx, posy, width, height, which);

  // empirically we need to draw wider
  posx -= OVERDRAW_MARGIN;
  if (width > 0) width += OVERDRAW_MARGIN;
  if (posx < 0) posx = 0;
  if (posy < 0) posy = 0;

  if (sfile->frames > 0 && mainw->video_drawable && (which == 0 || which == 1)) {
    bar_height = CE_VIDBAR_HEIGHT;
    allocwidth = lives_widget_get_allocation_width(mainw->video_draw);
    scalex = (double)allocwidth / CLIP_TOTAL_TIME(fileno);

    offset_left = ROUND_I((double)(sfile->start - 1.) / sfile->fps * scalex);
    offset_right = ROUND_I((double)(sfile->end) / sfile->fps * scalex);

    cr = lives_painter_create_from_surface(mainw->video_drawable);
    xwidth = UTIL_CLAMP(width, allocwidth);

    if (offset_left > posx) {
      // unselected
      lives_painter_set_source_rgb_from_lives_rgba(cr, &palette->ce_unsel);
      lives_painter_rectangle(cr, posx, 0,
                              NORMAL_CLAMP(offset_left - posx, xwidth),
                              bar_height);
      lives_painter_fill(cr);
    }

    if (offset_right > posx) {
      if (offset_left < posx) offset_left = posx;
      if (offset_right > posx + xwidth) offset_right = posx + xwidth;
      // selected
      lives_painter_set_source_rgb_from_lives_rgba(cr, &palette->ce_sel);
      lives_painter_rectangle(cr, offset_left, 0, offset_right - offset_left,
                              bar_height);
      lives_painter_fill(cr);
    }

    if (offset_right < posx + xwidth) {
      if (posx > offset_right) offset_right = posx;
      zwidth = ROUND_I(sfile->video_time * scalex) - offset_right;
      if (posx < offset_right) xwidth -= offset_right - posx;
      zwidth = NORMAL_CLAMP(zwidth, xwidth);
      // unselected
      lives_painter_set_source_rgb_from_lives_rgba(cr, &palette->ce_unsel);
      lives_painter_rectangle(cr, offset_right, 0, zwidth, bar_height);
      lives_painter_fill(cr);
    }
    lives_painter_destroy(cr);
    cr = NULL;
  }

  bar_height = CE_AUDBAR_HEIGHT / 2.;

  if (sfile->achans > 0 && mainw->laudio_drawable && mainw->laudio_drawable == sfile->laudio_drawable
      && (which == 0 || which == 2)) {
    allocwidth = lives_widget_get_allocation_width(mainw->laudio_draw);
    scalex = (double)allocwidth / CLIP_TOTAL_TIME(fileno);
    offset_left = ROUND_I((double)(sfile->start - 1.) / sfile->fps * scalex);
    offset_right = ROUND_I((double)(sfile->end) / sfile->fps * scalex);
    offset_end = ROUND_I(sfile->laudio_time * scalex);

    if (!sfile->audio_waveform) {
      sfile->audio_waveform = (float **)lives_calloc(sfile->achans, sizeof(float *));
      sfile->aw_sizes = (size_t *)lives_calloc(sfile->achans, sizeof(size_t));
    }

    start = offset_end;
    if (!sfile->audio_waveform[0]) {
      // re-read the audio
      sfile->audio_waveform[0] = (float *)lives_calloc((int)offset_end, sizeof(float));
      start = sfile->aw_sizes[0] = 0;
    } else if (sfile->aw_sizes[0] != offset_end) {
      start = 0;
      (float *)lives_recalloc(sfile->audio_waveform[0], (int)offset_end, sfile->aw_sizes[0],  sizeof(float));
    }

    if (sfile->audio_waveform[0]) {
      if (start != offset_end) {
        filename = lives_get_audio_file_name(fileno);
        afd = lives_open_buffered_rdonly(filename);
        lives_free(filename);
        if (afd < 0) {
          THREADVAR(read_failed) = -2;
          goto bail;
        }

        if (mainw->current_file != fileno
            || (is_thread && lives_proc_thread_get_cancelled(mainw->drawtl_thread))) goto bail;
        lives_buffered_rdonly_slurp(afd, 0);
        if (mainw->current_file != fileno
            || (is_thread && lives_proc_thread_get_cancelled(mainw->drawtl_thread))) goto bail;
        for (i = start; i < offset_end; i++) {
          if (mainw->current_file != fileno
              || (is_thread && lives_proc_thread_get_cancelled(mainw->drawtl_thread))) goto bail;
          atime = (double)i / scalex;
          sfile->audio_waveform[0][i] =
            sfile->vol * get_float_audio_val_at_time(fileno,
                afd, atime, 0, sfile->achans) * 2.;
        }
        sfile->aw_sizes[0] = offset_end;
        //lives_close_buffered(afd);
      }

      if (mainw->current_file != fileno
          || (is_thread && lives_proc_thread_get_cancelled(mainw->drawtl_thread))) goto bail;

      cr = lives_painter_create_from_surface(mainw->laudio_drawable);
      offset_right = NORMAL_CLAMP(offset_right, sfile->laudio_time * scalex);
      xwidth = UTIL_CLAMP(width, allocwidth);
      if (offset_end > posx + xwidth) offset_end = posx + xwidth;
      lives_painter_set_source_rgb_from_lives_rgba(cr, &palette->ce_unsel);
      lpos = -9999;
      lives_painter_move_to(cr, posx, bar_height);
      for (i = posx; i < offset_left && i < offset_end; i++) {
        if (mainw->current_file != fileno
            || (is_thread && lives_proc_thread_get_cancelled(mainw->drawtl_thread))) goto bail;
        pos = ROUND_I((double)(i * sfile->fps / scalex) / sfile->fps * scalex);
        if (pos != lpos) {
          lpos = pos;
          y = bar_height * (1. - sfile->audio_waveform[0][pos] / 2.);
        }

        lives_painter_line_to(cr, i, bar_height);
        lives_painter_line_to(cr, i, y);
        lives_painter_line_to(cr, i, bar_height);
      }
      lives_painter_close_path(cr);
      lives_painter_stroke(cr);

      if (mainw->current_file != fileno
          || (is_thread && lives_proc_thread_get_cancelled(mainw->drawtl_thread))) goto bail;

      lives_painter_set_source_rgb_from_lives_rgba(cr, &palette->ce_sel);
      lpos = -9999;

      lives_painter_move_to(cr, i, bar_height);
      for (; i < offset_right && i < offset_end; i++) {
        if (mainw->current_file != fileno
            || (is_thread && lives_proc_thread_get_cancelled(mainw->drawtl_thread))) goto bail;
        pos = ROUND_I((double)(i * sfile->fps / scalex) / sfile->fps * scalex);
        if (pos != lpos) {
          lpos = pos;
          y = bar_height * (1. - sfile->audio_waveform[0][pos] / 2.);
        }

        lives_painter_line_to(cr, i, bar_height);
        lives_painter_line_to(cr, i, y);
        lives_painter_line_to(cr, i, bar_height);
      }
      lives_painter_close_path(cr);
      lives_painter_stroke(cr);

      lives_painter_set_source_rgb_from_lives_rgba(cr, &palette->ce_unsel);
      lpos = -9999;
      lives_painter_move_to(cr, offset_right, bar_height);
      for (; i < offset_end; i++) {
        if (mainw->current_file != fileno
            || (is_thread && lives_proc_thread_get_cancelled(mainw->drawtl_thread))) goto bail;

        pos = ROUND_I((double)(i * sfile->fps / scalex) / sfile->fps * scalex);
        if (pos != lpos) {
          lpos = pos;
          y = bar_height * (1. - sfile->audio_waveform[0][pos] / 2.);
        }

        lives_painter_line_to(cr, i, bar_height);
        lives_painter_line_to(cr, i, y);
        lives_painter_line_to(cr, i, bar_height);
      }
      lives_painter_close_path(cr);
      lives_painter_stroke(cr);
      lives_painter_destroy(cr);
      cr = NULL;
    }
  }

  if (mainw->current_file != fileno
      || (is_thread && lives_proc_thread_get_cancelled(mainw->drawtl_thread))) goto bail;

  if (sfile->achans > 1 && mainw->raudio_drawable && mainw->raudio_drawable == sfile->raudio_drawable
      && (which == 0 || which == 3)) {
    allocwidth = lives_widget_get_allocation_width(mainw->raudio_draw);
    scalex = (double)allocwidth / CLIP_TOTAL_TIME(fileno);
    offset_left = ROUND_I((double)(sfile->start - 1.) / sfile->fps * scalex);
    offset_right = ROUND_I((double)(sfile->end) / sfile->fps * scalex);
    offset_end = ROUND_I(sfile->raudio_time * scalex);

    start = offset_end;
    if (!sfile->audio_waveform[1]) {
      // re-read the audio and force a redraw
      sfile->audio_waveform[1] = (float *)lives_calloc((int)offset_end, sizeof(float));
      start = sfile->aw_sizes[1] = 0;
    } else if (sfile->aw_sizes[1] != offset_end) {
      start = 0;
      sfile->audio_waveform[1] =
        (float *)lives_recalloc(sfile->audio_waveform[1], (int)offset_end, sfile->aw_sizes[1],  sizeof(float));
    }

    if (sfile->audio_waveform[1]) {
      if (start != offset_end) {
        if (afd < 0) {
          filename = lives_get_audio_file_name(fileno);
          afd = lives_open_buffered_rdonly(filename);
          lives_free(filename);
          if (afd < 0) {
            THREADVAR(read_failed) = -2;
            goto bail;
          }
          if (mainw->current_file != fileno
              || (is_thread && lives_proc_thread_get_cancelled(mainw->drawtl_thread))) goto bail;
          lives_buffered_rdonly_slurp(afd, 0);
          if (mainw->current_file != fileno
              || (is_thread && lives_proc_thread_get_cancelled(mainw->drawtl_thread))) goto bail;
        }

        for (i = start; i < offset_end; i++) {
          if (mainw->current_file != fileno
              || (is_thread && lives_proc_thread_get_cancelled(mainw->drawtl_thread))) goto bail;
          atime = (double)i / scalex;
          sfile->audio_waveform[1][i] =
            sfile->vol * get_float_audio_val_at_time(fileno,
                afd, atime, 1, sfile->achans) * 2.;
        }
        sfile->aw_sizes[1] = offset_end;
      }

      offset_right = NORMAL_CLAMP(offset_right, sfile->raudio_time * scalex);
      xwidth = UTIL_CLAMP(width, allocwidth);

      cr = lives_painter_create_from_surface(mainw->raudio_drawable);
      if (offset_end > posx + xwidth) offset_end = posx + xwidth;
      lives_painter_set_source_rgb_from_lives_rgba(cr, &palette->ce_unsel);
      lpos = -9999;
      lives_painter_move_to(cr, posx, bar_height);
      for (i = posx; i < offset_left && i < offset_end; i++) {
        if (mainw->current_file != fileno
            || (is_thread && lives_proc_thread_get_cancelled(mainw->drawtl_thread))) goto bail;
        pos = ROUND_I((double)(i * sfile->fps / scalex) / sfile->fps * scalex);
        if (pos != lpos) {
          lpos = pos;
          y = bar_height * (1. - sfile->audio_waveform[1][pos] / 2.);
        }

        lives_painter_line_to(cr, i, bar_height);
        lives_painter_line_to(cr, i, y);
        lives_painter_line_to(cr, i, bar_height);
      }
      lives_painter_close_path(cr);
      lives_painter_stroke(cr);

      if (mainw->current_file != fileno
          || (is_thread && lives_proc_thread_get_cancelled(mainw->drawtl_thread))) goto bail;

      lives_painter_set_source_rgb_from_lives_rgba(cr, &palette->ce_sel);
      lpos = -9999;

      lives_painter_move_to(cr, i, bar_height);
      for (; i < offset_right && i < offset_end; i++) {
        if (mainw->current_file != fileno
            || (is_thread && lives_proc_thread_get_cancelled(mainw->drawtl_thread))) goto bail;
        pos = ROUND_I((double)(i * sfile->fps / scalex) / sfile->fps * scalex);
        if (pos != lpos) {
          lpos = pos;
          y = bar_height * (1. - sfile->audio_waveform[1][pos] / 2.);
        }

        lives_painter_line_to(cr, i, bar_height);
        lives_painter_line_to(cr, i, y);
        lives_painter_line_to(cr, i, bar_height);
      }
      lives_painter_close_path(cr);
      lives_painter_stroke(cr);

      if (mainw->current_file != fileno
          || (is_thread && lives_proc_thread_get_cancelled(mainw->drawtl_thread))) goto bail;

      lives_painter_set_source_rgb_from_lives_rgba(cr, &palette->ce_unsel);
      lpos = -9999;
      lives_painter_move_to(cr, offset_right, bar_height);
      for (; i < offset_end; i++) {
        if (mainw->current_file != fileno
            || (is_thread && lives_proc_thread_get_cancelled(mainw->drawtl_thread))) goto bail;
        pos = ROUND_I((double)(i * sfile->fps / scalex) / sfile->fps * scalex);
        if (pos != lpos) {
          lpos = pos;
          y = bar_height * (1. - sfile->audio_waveform[1][pos] / 2.);
        }

        lives_painter_line_to(cr, i, bar_height);
        lives_painter_line_to(cr, i, y);
        lives_painter_line_to(cr, i, bar_height);
      }
      lives_painter_close_path(cr);
      lives_painter_stroke(cr);
      lives_painter_destroy(cr);
      cr = NULL;
    }

    if (afd >= 0) lives_close_buffered(afd);
    afd = -1;
  }

  if (mainw->current_file != fileno
      || (is_thread && lives_proc_thread_get_cancelled(mainw->drawtl_thread))) goto bail;

  if (which == 0) {
    // playback cursors
    if (CLIP_TOTAL_TIME(fileno) > 0.) {
      // set the range of the timeline
      if (!sfile->opening_loc && which == 0) {
        if (!lives_widget_is_visible(mainw->hruler)) {
          lives_widget_show(mainw->hruler);
        }
      }

      if (mainw->current_file != fileno
          || (is_thread && lives_proc_thread_get_cancelled(mainw->drawtl_thread))) goto bail;

      if (!lives_widget_is_visible(mainw->video_draw)) {
        lives_widget_show(mainw->hruler);
        lives_widget_show(mainw->video_draw);
        lives_widget_show(mainw->laudio_draw);
        lives_widget_show(mainw->raudio_draw);
      }

#ifdef ENABLE_GIW
      giw_timeline_set_max_size(GIW_TIMELINE(mainw->hruler), CLIP_TOTAL_TIME(fileno));
#endif
      lives_ruler_set_upper(LIVES_RULER(mainw->hruler), CLIP_TOTAL_TIME(fileno));
      lives_widget_queue_draw(mainw->hruler);
    }
    if (mainw->current_file != fileno
        || (is_thread && lives_proc_thread_get_cancelled(mainw->drawtl_thread))) goto bail;
    show_playbar_labels(fileno);
  }

  if (mainw->current_file != fileno
      || (is_thread && lives_proc_thread_get_cancelled(mainw->drawtl_thread))) goto bail;

  mainw->current_file = current_file;
  if (which == 0 || which == 1)
    main_thread_execute((lives_funcptr_t)lives_widget_queue_draw_if_visible, 0, NULL, "V", mainw->video_draw);
  if (which == 0 || which == 2)
    main_thread_execute((lives_funcptr_t)lives_widget_queue_draw_if_visible, 0, NULL, "V", mainw->laudio_draw);
  if (which == 0 || which == 3)
    main_thread_execute((lives_funcptr_t)lives_widget_queue_draw_if_visible, 0, NULL, "V", mainw->raudio_draw);
  return TRUE;

bail:
  if (cr) lives_painter_destroy(cr);
  if (afd >= 0) lives_close_buffered(afd);
  if (mainw->current_file == fileno && fileno != current_file)
    mainw->current_file = current_file;
  return FALSE;
}


void redraw_timer_bars(double oldx, double newx, int which) {
  // redraw region from cache
  // oldx and newx are in seconds
  double scalex;
  int allocwidth;

  if (oldx == newx) return;
  if (CURRENT_CLIP_TOTAL_TIME == 0.) return;

  allocwidth = lives_widget_get_allocation_width(mainw->video_draw);

  if (allocwidth == 0) return;

  scalex = allocwidth / CURRENT_CLIP_TOTAL_TIME;

  if (newx > oldx) {
    update_timer_bars(ROUND_I(oldx * scalex - .5), 0, ROUND_I((newx - oldx) * scalex + .5), 0, which);
  } else {
    update_timer_bars(ROUND_I(newx * scalex - .5), 0, ROUND_I((oldx - newx) * scalex + .5), 0, which);
  }
}


static boolean on_fsp_click(LiVESWidget *widget, LiVESXEventButton *event, livespointer user_data) {
  lives_button_clicked(LIVES_BUTTON(user_data));
  return FALSE;
}


static void set_pb_active(LiVESWidget *fchoo, LiVESWidget *pbutton) {
  LiVESSList *slist;
  if (!LIVES_IS_FILE_CHOOSER(fchoo)) return;
  slist = lives_file_chooser_get_filenames(LIVES_FILE_CHOOSER(fchoo));
  end_fs_preview(LIVES_FILE_CHOOSER(fchoo), pbutton);
  if (!slist || !slist->data || lives_slist_length(slist) > 1 ||
      !(lives_file_test((char *)lives_slist_nth_data(slist, 0), LIVES_FILE_TEST_IS_REGULAR))) {
    lives_widget_set_sensitive(pbutton, FALSE);
  } else lives_widget_set_sensitive(pbutton, TRUE);

  lives_slist_free_all(&slist);
  if (mainw->fs_playframe) {
    lives_widget_show_all(lives_widget_get_parent(mainw->fs_playframe));
  }
}


LiVESWidget *widget_add_preview(LiVESWidget *widget, LiVESBox *for_preview, LiVESBox *for_button, LiVESBox *for_deint,
                                int preview_type) {
  LiVESWidget *preview_button = NULL;
  int woat = widget_opts.apply_theme;

  if (preview_type == LIVES_PREVIEW_TYPE_VIDEO_AUDIO || preview_type == LIVES_PREVIEW_TYPE_RANGE ||
      preview_type == LIVES_PREVIEW_TYPE_IMAGE_ONLY) {
    if (LIVES_IS_FILE_CHOOSER(widget) && preview_type != LIVES_PREVIEW_TYPE_RANGE) {
      if (woat) widget_opts.apply_theme = 2;
    }
    mainw->fs_playframe = lives_standard_frame_new(_("Preview"), 0.5, FALSE);
    widget_opts.apply_theme = woat;
    mainw->fs_playalign = lives_alignment_new(0.5, 0.5, 1., 1.);

    mainw->fs_playimg = lives_drawing_area_new();

    lives_widget_set_app_paintable(mainw->fs_playimg, TRUE);

    mainw->fs_playarea = lives_event_box_new();

    lives_widget_nullify_with(widget, (void **)&mainw->fs_playframe);
    lives_widget_nullify_with(widget, (void **)&mainw->fs_playarea);
    lives_widget_nullify_with(widget, (void **)&mainw->fs_playalign);
    lives_widget_nullify_with(widget, (void **)&mainw->fs_playimg);

    lives_widget_set_events(mainw->fs_playframe, LIVES_BUTTON_PRESS_MASK);

    if (LIVES_IS_FILE_CHOOSER(widget) && preview_type != LIVES_PREVIEW_TYPE_RANGE) {
      lives_widget_apply_theme2(mainw->fs_playframe, LIVES_WIDGET_STATE_NORMAL, TRUE);
      lives_widget_apply_theme2(mainw->fs_playalign, LIVES_WIDGET_STATE_NORMAL, TRUE);
      lives_widget_set_fg_color(mainw->fs_playalign, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
      lives_widget_apply_theme2(mainw->fs_playarea, LIVES_WIDGET_STATE_NORMAL, TRUE);
    } else {
      lives_widget_apply_theme(mainw->fs_playframe, LIVES_WIDGET_STATE_NORMAL);
      lives_widget_apply_theme(mainw->fs_playalign, LIVES_WIDGET_STATE_NORMAL);
      lives_widget_set_fg_color(mainw->fs_playalign, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
      lives_widget_apply_theme(mainw->fs_playarea, LIVES_WIDGET_STATE_NORMAL);
    }
    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(mainw->fs_playarea), PIXBUF_KEY, NULL);

    lives_container_set_border_width(LIVES_CONTAINER(mainw->fs_playframe), 0);

    if (preview_type != LIVES_PREVIEW_TYPE_RANGE) {
      lives_widget_set_size_request(mainw->fs_playframe,
                                    ((int)(DEF_FRAME_HSIZE_UNSCALED * (widget_opts.scale < 1.
                                           ? widget_opts.scale : 1.)) >> 2) << 1,
                                    ((int)(DEF_FRAME_VSIZE_UNSCALED * (widget_opts.scale < 1.
                                           ? widget_opts.scale : 1.)) >> 2) << 1);
    } else lives_widget_set_vexpand(mainw->fs_playframe, TRUE);

    lives_container_add(LIVES_CONTAINER(mainw->fs_playframe), mainw->fs_playalign);
    lives_container_add(LIVES_CONTAINER(mainw->fs_playalign), mainw->fs_playarea);
    lives_container_add(LIVES_CONTAINER(mainw->fs_playarea), mainw->fs_playimg);
  } else mainw->fs_playframe = mainw->fs_playalign = mainw->fs_playarea = mainw->fs_playimg = NULL; // AUDIO_ONLY

  if (preview_type == LIVES_PREVIEW_TYPE_VIDEO_AUDIO) {
    widget_opts.justify = LIVES_JUSTIFY_CENTER;
    preview_button =
      lives_standard_button_new_from_stock_full(NULL, _("Click here to _Preview the Selected Video, "
          "Image or Audio File"),
          -1, DEF_BUTTON_HEIGHT, NULL, TRUE, NULL);
    widget_opts.justify = LIVES_JUSTIFY_DEFAULT;
    lives_widget_set_halign(preview_button, LIVES_ALIGN_END);
  } else if (preview_type == LIVES_PREVIEW_TYPE_AUDIO_ONLY) {
    preview_button = lives_standard_button_new_with_label(_("Click here to _Preview the Selected "
                     "Audio File"), -1, DEF_BUTTON_HEIGHT);
    lives_widget_set_halign(preview_button, LIVES_ALIGN_CENTER);
  } else if (preview_type == LIVES_PREVIEW_TYPE_RANGE) {
    preview_button = lives_standard_button_new_with_label(_("Click here to _Preview the Selection"),
                     -1, DEF_BUTTON_HEIGHT);
    lives_widget_set_hexpand(mainw->fs_playframe, TRUE);
    lives_widget_set_vexpand(mainw->fs_playframe, TRUE);
    lives_widget_set_halign(preview_button, LIVES_ALIGN_CENTER);
  } else {
    preview_button = lives_standard_button_new_with_label(_("Click here to _Preview the file"),
                     -1, DEF_BUTTON_HEIGHT);
    lives_widget_set_halign(preview_button, LIVES_ALIGN_END);
  }

  if (LIVES_IS_FILE_CHOOSER(widget) && preview_type != LIVES_PREVIEW_TYPE_RANGE) {
    gtk_file_chooser_set_preview_widget(GTK_FILE_CHOOSER(widget), mainw->fs_playframe);
    //gtk_file_chooser_set_preview_widget_active(GTK_FILE_CHOOSER(widget), FALSE);
    lives_signal_sync_connect(LIVES_GUI_OBJECT(widget), "update-preview",
                              LIVES_GUI_CALLBACK(set_pb_active), preview_button);
  } else {
    lives_box_pack_start(for_preview, mainw->fs_playframe, FALSE, TRUE, 0);
    lives_widget_show_all(LIVES_WIDGET(for_preview));
  }

  if (preview_type == LIVES_PREVIEW_TYPE_VIDEO_AUDIO || preview_type == LIVES_PREVIEW_TYPE_RANGE ||
      preview_type == LIVES_PREVIEW_TYPE_IMAGE_ONLY) {
    if (for_button) lives_box_pack_start(for_button, preview_button, FALSE, FALSE, widget_opts.packing_width);

    lives_signal_connect(LIVES_GUI_OBJECT(mainw->fs_playframe), LIVES_WIDGET_BUTTON_PRESS_EVENT,
                         LIVES_GUI_CALLBACK(on_fsp_click), preview_button);
  }

  if (preview_type == LIVES_PREVIEW_TYPE_VIDEO_AUDIO || preview_type == LIVES_PREVIEW_TYPE_RANGE) {
    if (LIVES_IS_FILE_CHOOSER(widget)) {
      gtk_file_chooser_add_choice(LIVES_FILE_CHOOSER(widget), "deint", _("Apply deinterlace"), NULL, NULL);
    } else add_deinterlace_checkbox(for_deint);
  }

  SET_INT_DATA(preview_button, PRV_TYPE_KEY, preview_type);
  if (preview_type != LIVES_PREVIEW_TYPE_RANGE)
    lives_signal_sync_connect(LIVES_GUI_OBJECT(preview_button), LIVES_WIDGET_CLICKED_SIGNAL,
                              LIVES_GUI_CALLBACK(on_fs_preview_clicked1), widget);

  if (LIVES_IS_FILE_CHOOSER(widget) && preview_type != LIVES_PREVIEW_TYPE_RANGE) {
    lives_widget_set_sensitive(preview_button, FALSE);
  } else lives_widget_set_sensitive(preview_button, TRUE);
  if (mainw->fs_playframe) lives_widget_show_all(mainw->fs_playframe);

  lives_widget_set_hexpand(preview_button, TRUE);

  return preview_button;
}


static void on_dth_cancel_clicked(LiVESButton *button, livespointer user_data) {
  if (LIVES_POINTER_TO_INT(user_data) == 1) mainw->cancelled = CANCEL_KEEP;
  else mainw->cancelled = CANCEL_USER;
}


static ticks_t last_t;

xprocess *create_threaded_dialog(char *text, boolean has_cancel, boolean *td_had_focus) {
  LiVESWidget *dialog_vbox;
  LiVESWidget *vbox;
  LiVESWidget *hbox;
  xprocess *procw;
  char tmp_label[256];

  last_t = lives_get_current_ticks();

  procw = (xprocess *)(lives_calloc(sizeof(xprocess), 1));

  procw->processing = lives_standard_dialog_new(_("Processing..."), FALSE, -1, -1);
  lives_widget_set_minimum_size(procw->processing, DEF_DIALOG_WIDTH * .75, DEF_DIALOG_HEIGHT >> 1);

  lives_window_set_decorated(LIVES_WINDOW(procw->processing), FALSE);

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(procw->processing));

  vbox = lives_vbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(dialog_vbox), vbox, TRUE, TRUE, 0);

  lives_snprintf(tmp_label, 256, "%s...\n", text);
  widget_opts.justify = LIVES_JUSTIFY_CENTER;
  widget_opts.mnemonic_label = FALSE;
  procw->label = lives_standard_label_new(tmp_label);
  widget_opts.justify = LIVES_JUSTIFY_DEFAULT;
  widget_opts.mnemonic_label = TRUE;
  lives_box_pack_start(LIVES_BOX(vbox), procw->label, FALSE, TRUE, 0);

  procw->progressbar = lives_standard_progress_bar_new();

  lives_progress_bar_set_pulse_step(LIVES_PROGRESS_BAR(procw->progressbar), .01);
  lives_box_pack_start(LIVES_BOX(vbox), procw->progressbar, FALSE, FALSE, 0);

  if (widget_opts.apply_theme && (palette->style & STYLE_1)) {
    lives_widget_set_fg_color(procw->progressbar, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
  }

  widget_opts.justify = LIVES_JUSTIFY_CENTER;
  procw->label2 = lives_standard_label_new(_(""));
  widget_opts.justify = LIVES_JUSTIFY_DEFAULT;
  lives_box_pack_start(LIVES_BOX(vbox), procw->label2, FALSE, FALSE, 0);

  widget_opts.justify = LIVES_JUSTIFY_CENTER;
#ifdef PROGBAR_IS_ENTRY
  procw->label3 = procw->progressbar;
#else
  procw->label3 = lives_standard_label_new("");
  lives_box_pack_start(LIVES_BOX(vbox), procw->label3, FALSE, FALSE, 0);
#endif
  widget_opts.justify = LIVES_JUSTIFY_DEFAULT;

  add_procdlg_opts(procw, LIVES_VBOX(vbox));

  widget_opts.expand = LIVES_EXPAND_EXTRA;
  hbox = lives_hbox_new(FALSE, widget_opts.filler_len * 8);
  add_fill_to_box(LIVES_BOX(hbox));
  add_fill_to_box(LIVES_BOX(hbox));
  widget_opts.expand = LIVES_EXPAND_DEFAULT;
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, 0);

  if (has_cancel) {
    if (CURRENT_CLIP_IS_VALID && mainw->cancel_type == CANCEL_SOFT) {
      LiVESWidget *enoughbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(procw->processing),
                                  NULL, _("_Enough"), LIVES_RESPONSE_CANCEL);
      lives_widget_set_can_default(enoughbutton, TRUE);

      lives_signal_sync_connect(LIVES_GUI_OBJECT(enoughbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                                LIVES_GUI_CALLBACK(on_dth_cancel_clicked), LIVES_INT_TO_POINTER(1));

      lives_window_add_escape(LIVES_WINDOW(procw->processing), enoughbutton);
      lives_button_uncenter(enoughbutton, DLG_BUTTON_WIDTH);
    } else {
      procw->cancel_button = lives_dialog_add_button_from_stock(LIVES_DIALOG(procw->processing),
                             LIVES_STOCK_CANCEL, NULL, LIVES_RESPONSE_CANCEL);
      lives_widget_set_can_default(procw->cancel_button, TRUE);

      lives_window_add_escape(LIVES_WINDOW(procw->processing), procw->cancel_button);

      lives_signal_sync_connect(LIVES_GUI_OBJECT(procw->cancel_button), LIVES_WIDGET_CLICKED_SIGNAL,
                                LIVES_GUI_CALLBACK(on_dth_cancel_clicked), LIVES_INT_TO_POINTER(0));
      lives_button_uncenter(procw->cancel_button, DLG_BUTTON_WIDTH);
    }
  }

  if (lives_has_toplevel_focus(LIVES_MAIN_WINDOW_WIDGET)) {
    *td_had_focus = TRUE;
  } else *td_had_focus = FALSE;

  lives_widget_show_all(procw->processing);

  lives_set_cursor_style(LIVES_CURSOR_BUSY, procw->processing);
  lives_set_cursor_style(LIVES_CURSOR_BUSY, NULL);

  procw->is_ready = TRUE;
  return procw;
}


void add_procdlg_opts(xprocess *procw, LiVESVBox *vbox) {
  char *tmp;
  if (mainw->internal_messaging && mainw->rte != 0 && !mainw->transrend_proc) {
    procw->rte_off_cb = lives_standard_check_button_new(_("Switch off real time effects when finished"),
                        TRUE, LIVES_BOX(vbox),
                        (tmp = H_("Switch off all real time effects after processing\n"
                                  "so that the result can be viewed without them")));
    lives_free(tmp);
  }

  if (mainw->is_rendering && AUD_SRC_EXTERNAL) {
    procw->audint_cb = lives_standard_check_button_new(_("Switch to intenal audio when finished"),
                       FALSE, LIVES_BOX(vbox), NULL);
  }

  procw->notify_cb = lives_standard_check_button_new(_("Notify when finished"),
                     FALSE, LIVES_BOX(vbox),
                     (tmp = H_("Send a desktop notification\nwhen complete")));
  lives_free(tmp);

  lives_widget_set_opacity(lives_widget_get_parent(procw->notify_cb), 0.);
  lives_widget_set_sensitive(procw->notify_cb, FALSE);
}


xprocess *create_processing(const char *text) {
  LiVESWidget *dialog_vbox;
  LiVESWidget *hbox;
  LiVESWidget *vbox2;
  LiVESWidget *vbox3;

  xprocess *procw = (xprocess *)(lives_calloc(sizeof(xprocess), 1));

  char tmp_label[256];
  boolean markup = widget_opts.use_markup;

  widget_opts.use_markup = FALSE;

  procw->frac_done = -1.;

  procw->processing = lives_standard_dialog_new(_("Processing..."), FALSE, -1, -1);
  lives_widget_set_minimum_size(procw->processing, DEF_DIALOG_WIDTH * .75, DEF_DIALOG_HEIGHT >> 1);

  lives_window_set_decorated(LIVES_WINDOW(procw->processing), FALSE);

  if (prefs->gui_monitor != 0) {
    lives_window_set_monitor(LIVES_WINDOW(procw->processing), widget_opts.monitor);
  }

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(procw->processing));

  vbox2 = lives_vbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(dialog_vbox), vbox2, TRUE, TRUE, 0);

  vbox3 = lives_vbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox2), vbox3, TRUE, TRUE, 0);

  procw->text = lives_strdup(text);

  widget_opts.use_markup = markup;
  lives_snprintf(tmp_label, 256, "%s...\n", text);
  widget_opts.use_markup = FALSE;
  widget_opts.justify = LIVES_JUSTIFY_CENTER;
  widget_opts.mnemonic_label = FALSE;
  procw->label = lives_standard_label_new(tmp_label);
  widget_opts.mnemonic_label = TRUE;
  widget_opts.justify = LIVES_JUSTIFY_DEFAULT;

  lives_box_pack_start(LIVES_BOX(vbox3), procw->label, TRUE, TRUE, 0);

  procw->progressbar = lives_standard_progress_bar_new();

  lives_box_pack_start(LIVES_BOX(vbox3), procw->progressbar, FALSE, FALSE, 0);
  if (palette->style & STYLE_1) {
    lives_widget_set_fg_color(procw->progressbar, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
  }

  widget_opts.justify = LIVES_JUSTIFY_CENTER;
  procw->label2 = lives_standard_label_new("");
  lives_box_pack_start(LIVES_BOX(vbox3), procw->label2, FALSE, FALSE, 0);

#ifdef PROGBAR_IS_ENTRY
  procw->label3 = procw->progressbar;
#else
  procw->label3 = lives_standard_label_new("");
  lives_box_pack_start(LIVES_BOX(vbox3), procw->label3, FALSE, FALSE, 0);
#endif

  widget_opts.justify = LIVES_JUSTIFY_DEFAULT;

  add_procdlg_opts(procw, LIVES_VBOX(vbox3));

  widget_opts.last_container = vbox3;
  lives_hooks_trigger(NULL, THREADVAR(hook_closures), TX_START_HOOK);

  widget_opts.expand = LIVES_EXPAND_EXTRA;
  hbox = lives_hbox_new(FALSE, widget_opts.filler_len * 8);
  add_fill_to_box(LIVES_BOX(hbox));
  add_fill_to_box(LIVES_BOX(hbox));
  widget_opts.expand = LIVES_EXPAND_DEFAULT;

  lives_box_pack_start(LIVES_BOX(vbox3), hbox, FALSE, FALSE, 0);

  if (mainw->iochan) {
    // add "show details" arrow
    int woat = widget_opts.apply_theme;
    widget_opts.apply_theme = 0;
    widget_opts.expand = LIVES_EXPAND_EXTRA;
    procw->scrolledwindow = lives_standard_scrolled_window_new(ENC_DETAILS_WIN_H, ENC_DETAILS_WIN_V,
                            LIVES_WIDGET(mainw->optextview));
    widget_opts.expand = LIVES_EXPAND_DEFAULT;
    widget_opts.apply_theme = woat;
    lives_standard_expander_new(_("Show Details"), _("Hide Details"), LIVES_BOX(vbox3), procw->scrolledwindow);
  }

  procw->stop_button = procw->preview_button = procw->pause_button = NULL;

  if (CURRENT_CLIP_IS_VALID) {
    if (cfile->opening_loc
#ifdef ENABLE_JACK
        || mainw->jackd_read
#endif
#ifdef HAVE_PULSE_AUDIO
        || mainw->pulsed_read
#endif
       ) {
      // the "enough" button for opening
      procw->stop_button = lives_dialog_add_button_from_stock(LIVES_DIALOG(procw->processing), NULL, _("_Enough"),
                           LIVES_RESPONSE_ACCEPT); // used only for open location and for audio recording
      lives_widget_set_can_default(procw->stop_button, TRUE);
    }

    if (cfile->nokeep) procw->pause_button = lives_dialog_add_button_from_stock(LIVES_DIALOG(procw->processing),
          NULL, _("Paus_e"), LIVES_RESPONSE_ACCEPT);
    else procw->pause_button = lives_dialog_add_button_from_stock(LIVES_DIALOG(procw->processing), NULL, _("Pause/_Enough"),
                                 LIVES_RESPONSE_ACCEPT);
    lives_widget_set_can_default(procw->pause_button, TRUE);

    procw->preview_button = lives_dialog_add_button_from_stock(LIVES_DIALOG(procw->processing), NULL, _("_Preview"),
                            LIVES_RESPONSE_SHOW_DETAILS);
    lives_widget_set_can_default(procw->preview_button, TRUE);
  }

  procw->cancel_button = lives_dialog_add_button_from_stock(LIVES_DIALOG(procw->processing), LIVES_STOCK_CANCEL, NULL,
                         LIVES_RESPONSE_CANCEL);

  lives_widget_set_can_default(procw->cancel_button, TRUE);
  lives_button_grab_default_special(procw->cancel_button);

  lives_window_add_escape(LIVES_WINDOW(procw->processing), procw->cancel_button);

  if (procw->stop_button)
    lives_signal_sync_connect(LIVES_GUI_OBJECT(procw->stop_button), LIVES_WIDGET_CLICKED_SIGNAL,
                              LIVES_GUI_CALLBACK(on_stop_clicked), NULL);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(procw->pause_button), LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_effects_paused), NULL);

  if (mainw->multitrack && mainw->multitrack->is_rendering) {
    lives_signal_sync_connect(LIVES_GUI_OBJECT(procw->preview_button), LIVES_WIDGET_CLICKED_SIGNAL,
                              LIVES_GUI_CALLBACK(multitrack_preview_clicked), mainw->multitrack);
  } else {
    lives_signal_sync_connect(LIVES_GUI_OBJECT(procw->preview_button), LIVES_WIDGET_CLICKED_SIGNAL,
                              LIVES_GUI_CALLBACK(on_preview_clicked), NULL);
  }

  lives_signal_sync_connect(LIVES_GUI_OBJECT(procw->cancel_button), LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_cancel_keep_button_clicked), NULL);

  if (mainw->show_procd) {
    if (procw->preview_button) lives_widget_set_no_show_all(procw->preview_button, TRUE);
    if (procw->pause_button) lives_widget_set_no_show_all(procw->pause_button, TRUE);
    if (procw->stop_button) lives_widget_set_no_show_all(procw->stop_button, TRUE);

    lives_widget_show_all(procw->processing);

    if (procw->preview_button) lives_widget_set_no_show_all(procw->preview_button, FALSE);
    if (procw->pause_button) lives_widget_set_no_show_all(procw->pause_button, FALSE);
    if (procw->stop_button) lives_widget_set_no_show_all(procw->stop_button, FALSE);
  }

  return procw;
}


static LiVESWidget *vid_text_view_new(void) {
  LiVESWidget *textview;
  widget_opts.justify = LIVES_JUSTIFY_CENTER;
  textview = lives_standard_text_view_new(NULL, NULL);
  widget_opts.justify = LIVES_JUSTIFY_DEFAULT;
  lives_widget_set_size_request(textview, TB_WIDTH, TB_HEIGHT_VID);
  if (palette->style & STYLE_3) {
    lives_widget_set_bg_color(textview, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
  }
  if (mainw->multitrack) {
    lives_text_view_set_top_margin(LIVES_TEXT_VIEW(textview), 2);
    lives_text_view_set_bottom_margin(LIVES_TEXT_VIEW(textview), 20);
    lives_widget_set_valign(textview, LIVES_ALIGN_FILL);
  } else {
    lives_text_view_set_bottom_margin(LIVES_TEXT_VIEW(textview), TB_HEIGHT_VID >> 2);
    lives_text_view_set_top_margin(LIVES_TEXT_VIEW(textview), TB_HEIGHT_VID >> 2);
  }
  return textview;
}


static LiVESWidget *aud_text_view_new(void) {
  LiVESWidget *textview;
  widget_opts.justify = LIVES_JUSTIFY_CENTER;
  textview = lives_standard_text_view_new(NULL, NULL);
  widget_opts.justify = LIVES_JUSTIFY_DEFAULT;
  lives_widget_set_size_request(textview, TB_WIDTH, TB_HEIGHT_AUD);
  if (palette->style & STYLE_3) {
    lives_widget_set_bg_color(textview, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
  }
  if (mainw->multitrack) {
    lives_text_view_set_bottom_margin(LIVES_TEXT_VIEW(textview), 20);
  } else {
    lives_text_view_set_bottom_margin(LIVES_TEXT_VIEW(textview), TB_HEIGHT_AUD >> 2);
    lives_text_view_set_top_margin(LIVES_TEXT_VIEW(textview), TB_HEIGHT_AUD >> 2);
  }
  return textview;
}


lives_clipinfo_t *create_clip_info_window(int audio_channels, boolean is_mt) {
  LiVESWidget *dialog_vbox;
  LiVESWidget *table;
  LiVESWidget *label;
  LiVESWidget *vidframe;
  LiVESWidget *laudframe;
  LiVESWidget *raudframe;
  LiVESWidget *okbutton;
  LiVESWidget *vbox;
  LiVESWidget *hbox;
  LiVESWidget *layout;

  lives_clipinfo_t *filew;

  char *title;
  char *tmp;

  int offset = 0;

  filew = (lives_clipinfo_t *)(lives_calloc(1, sizeof(lives_clipinfo_t)));

  if (!is_mt) {
    if (!CURRENT_CLIP_IS_VALID) return NULL;
    title = get_menu_name(cfile, TRUE);
    if (prefs->show_dev_opts) {
      title = lives_concat(title, lives_strdup_printf(" (%s)", cfile->handle));
    }
  } else {
    offset = 2;
    title = (_("Multitrack Details"));
  }

  filew->dialog = lives_standard_dialog_new(title, FALSE, -1, -1);
  lives_free(title);

  lives_signal_handlers_disconnect_by_func(filew->dialog, LIVES_GUI_CALLBACK(return_true), NULL);

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(filew->dialog));
  lives_container_set_border_width(LIVES_CONTAINER(dialog_vbox), 2);

  if (cfile->frames > 0 || is_mt) {
    vidframe = lives_standard_frame_new(_("Video"), 0., FALSE);

    lives_box_pack_start(LIVES_BOX(dialog_vbox), vidframe, TRUE, TRUE, 0);

    vbox = lives_vbox_new(FALSE, 0);
    lives_container_add(LIVES_CONTAINER(vidframe), vbox);
    lives_container_set_border_width(LIVES_CONTAINER(vbox), widget_opts.border_width);

    layout = lives_layout_new(LIVES_BOX(vbox));

    label = lives_layout_add_label(LIVES_LAYOUT(layout), _("Format"), TRUE);
    if (palette->style & STYLE_3) {
      lives_widget_set_bg_color(label, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
    }

    filew->textview_type = vid_text_view_new();
    hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
    lives_layout_pack(LIVES_HBOX(hbox), filew->textview_type);
    lives_widget_set_valign(filew->textview_type, LIVES_ALIGN_FILL);

    widget_opts.expand |= LIVES_EXPAND_EXTRA_WIDTH;
    lives_layout_add_fill(LIVES_LAYOUT(layout), TRUE);
    widget_opts.expand = LIVES_EXPAND_DEFAULT;

    label = lives_layout_add_label(LIVES_LAYOUT(layout), _("FPS"), TRUE);
    if (palette->style & STYLE_3) {
      lives_widget_set_bg_color(label, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
    }

    filew->textview_fps = vid_text_view_new();
    hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
    lives_layout_pack(LIVES_HBOX(hbox), filew->textview_fps);

    lives_layout_add_row(LIVES_LAYOUT(layout));

    label = lives_layout_add_label(LIVES_LAYOUT(layout), _("Frame Size"), TRUE);
    if (palette->style & STYLE_3) {
      lives_widget_set_bg_color(label, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
    }

    filew->textview_size = vid_text_view_new();
    hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
    lives_layout_pack(LIVES_HBOX(hbox), filew->textview_size);

    widget_opts.expand |= LIVES_EXPAND_EXTRA_WIDTH;
    lives_layout_add_fill(LIVES_LAYOUT(layout), TRUE);
    widget_opts.expand = LIVES_EXPAND_DEFAULT;

    label = lives_layout_add_label(LIVES_LAYOUT(layout), _("Frames"), TRUE);
    if (palette->style & STYLE_3) {
      lives_widget_set_bg_color(label, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
    }

    filew->textview_frames = vid_text_view_new();
    hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
    lives_layout_pack(LIVES_HBOX(hbox), filew->textview_frames);
    lives_widget_set_valign(filew->textview_frames, LIVES_ALIGN_FILL);

    lives_layout_add_row(LIVES_LAYOUT(layout));

    label = lives_layout_add_label(LIVES_LAYOUT(layout), _("File Size"), TRUE);
    if (palette->style & STYLE_3) {
      lives_widget_set_bg_color(label, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
    }

    filew->textview_fsize = vid_text_view_new();
    hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
    lives_layout_pack(LIVES_HBOX(hbox), filew->textview_fsize);

    widget_opts.expand |= LIVES_EXPAND_EXTRA_WIDTH;
    lives_layout_add_fill(LIVES_LAYOUT(layout), TRUE);
    widget_opts.expand = LIVES_EXPAND_DEFAULT;

    label = lives_layout_add_label(LIVES_LAYOUT(layout), _("Total Time"), TRUE);
    if (palette->style & STYLE_3) {
      lives_widget_set_bg_color(label, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
    }

    filew->textview_vtime = vid_text_view_new();
    hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
    lives_layout_pack(LIVES_HBOX(hbox), filew->textview_vtime);
    if (mainw->multitrack) lives_text_view_set_top_margin(LIVES_TEXT_VIEW(filew->textview_vtime), 10);
  }

  if (audio_channels > 0) {
    if (audio_channels > 1) tmp = get_achannel_name(2, 0);
    else tmp = (_("Audio"));

    laudframe = lives_standard_frame_new(tmp, 0., FALSE);
    lives_free(tmp);

    lives_box_pack_start(LIVES_BOX(dialog_vbox), laudframe, TRUE, TRUE, 0);

    table = lives_table_new(1, 4, TRUE);

    lives_table_set_col_spacings(LIVES_TABLE(table), widget_opts.packing_width * 4);
    lives_table_set_row_spacings(LIVES_TABLE(table), widget_opts.packing_height);
    lives_container_set_border_width(LIVES_CONTAINER(table), widget_opts.border_width);

    lives_container_add(LIVES_CONTAINER(laudframe), table);

    label = lives_standard_label_new(_("Rate/size"));
    if (palette->style & STYLE_3) {
      lives_widget_set_bg_color(label, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
    }
    lives_label_set_hpadding(LIVES_LABEL(label), 4);
    lives_table_attach(LIVES_TABLE(table), label, 0 + offset, 1 + offset, 0, 1,
                       (LiVESAttachOptions)(0), (LiVESAttachOptions)(0), 0, 0);

    filew->textview_lrate = aud_text_view_new();
    lives_table_attach(LIVES_TABLE(table), filew->textview_lrate, 1 + offset, 2 + offset, 0, 1,
                       (LiVESAttachOptions)(0), (LiVESAttachOptions)(0), 0, 0);
    lives_widget_set_valign(filew->textview_lrate, LIVES_ALIGN_FILL);

    if (!is_mt) {
      label = lives_standard_label_new(_("Total time"));
      if (palette->style & STYLE_3) {
        lives_widget_set_bg_color(label, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
      }
      lives_table_attach(LIVES_TABLE(table), label, 2, 3, 0, 1,
                         (LiVESAttachOptions)(0), (LiVESAttachOptions)(0), 0, 0);

      filew->textview_ltime = aud_text_view_new();
      lives_table_attach(LIVES_TABLE(table), filew->textview_ltime, 3, 4, 0, 1,
                         (LiVESAttachOptions)(0), (LiVESAttachOptions)(0), 0, 0);
    }
  }

  if (audio_channels > 1) {
    tmp = get_achannel_name(2, 1);
    raudframe = lives_standard_frame_new(tmp, 0., FALSE);
    lives_free(tmp);

    lives_box_pack_start(LIVES_BOX(dialog_vbox), raudframe, TRUE, TRUE, 0);

    table = lives_table_new(1, 4, TRUE);

    lives_table_set_col_spacings(LIVES_TABLE(table), widget_opts.packing_width * 4);
    lives_table_set_row_spacings(LIVES_TABLE(table), widget_opts.packing_height);
    lives_container_set_border_width(LIVES_CONTAINER(table), widget_opts.border_width);

    lives_container_add(LIVES_CONTAINER(raudframe), table);


    label = lives_standard_label_new(_("Rate/size"));
    if (palette->style & STYLE_3) {
      lives_widget_set_bg_color(label, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
    }
    lives_label_set_hpadding(LIVES_LABEL(label), 4);
    lives_table_attach(LIVES_TABLE(table), label, 0 + offset, 1 + offset, 0, 1,
                       (LiVESAttachOptions)(0), (LiVESAttachOptions)(0), 0, 0);

    filew->textview_rrate = aud_text_view_new();
    lives_table_attach(LIVES_TABLE(table), filew->textview_rrate, 1 + offset, 2 + offset, 0, 1,
                       (LiVESAttachOptions)(0), (LiVESAttachOptions)(0), 0, 0);
    lives_widget_set_valign(filew->textview_rrate, LIVES_ALIGN_FILL);

    if (!is_mt) {
      label = lives_standard_label_new(_("Total time"));
      if (palette->style & STYLE_3) {
        lives_widget_set_bg_color(label, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
      }
      lives_label_set_hpadding(LIVES_LABEL(label), 4);
      lives_table_attach(LIVES_TABLE(table), label, 2, 3, 0, 1,
                         (LiVESAttachOptions)(0), (LiVESAttachOptions)(0), 0, 0);

      filew->textview_rtime = aud_text_view_new();
      lives_table_attach(LIVES_TABLE(table), filew->textview_rtime, 3, 4, 0, 1,
                         (LiVESAttachOptions)(0), (LiVESAttachOptions)(0), 0, 0);
    }
  }

  okbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(filew->dialog), LIVES_STOCK_CLOSE, _("_Close Window"),
             LIVES_RESPONSE_OK);
  lives_button_grab_default_special(okbutton);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(okbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(lives_general_button_clicked), filew);

  lives_window_add_escape(LIVES_WINDOW(filew->dialog), okbutton);

  lives_widget_show_all(filew->dialog);

  return filew;
}


static void on_resizecb_toggled(LiVESToggleButton *t, livespointer user_data) {
  LiVESWidget *cb = (LiVESWidget *)user_data;

  if (!lives_toggle_button_get_active(t)) {
    lives_widget_set_sensitive(cb, FALSE);
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(cb), FALSE);
  } else {
    lives_widget_set_sensitive(cb, TRUE);
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(cb), prefs->enc_letterbox);
  }
}


LiVESWidget *create_encoder_prep_dialog(const char *text1, const char *text2, boolean opt_resize) {
  LiVESWidget *dialog;
  LiVESWidget *dialog_vbox;
  LiVESWidget *okbutton;
  LiVESWidget *checkbutton = NULL;
  LiVESWidget *checkbutton2;
  LiVESWidget *label;
  LiVESWidget *hbox;

  char *labeltext, *tmp, *tmp2;

  dialog = create_question_dialog(_("Encoding Options"), text1);
  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(dialog));

  if (opt_resize) {
    if (text2) labeltext = (_("<------------- (Check the box to re_size as suggested)"));
    else labeltext = (_("<------------- (Check the box to use the _size recommendation)"));

    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, FALSE, FALSE, widget_opts.packing_width);

    checkbutton = lives_standard_check_button_new(labeltext, FALSE, LIVES_BOX(hbox), NULL);

    lives_free(labeltext);

    lives_signal_sync_connect_after(LIVES_GUI_OBJECT(checkbutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                                    LIVES_GUI_CALLBACK(on_boolean_toggled), &mainw->fx1_bool);
  } else if (!text2) mainw->fx1_bool = TRUE;

  if (text2 && (mainw->fx1_bool || opt_resize)) {
    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

    checkbutton2 = lives_standard_check_button_new
                   ((tmp = (_("Use _letterboxing to maintain aspect ratio (optional)"))), FALSE, LIVES_BOX(hbox),
                    (tmp2 = (H_("Draw black rectangles either above or to the sides of the image, "
                                "to prevent it from stretching."))));

    lives_free(tmp); lives_free(tmp2);

    if (opt_resize) {
      lives_widget_set_sensitive(checkbutton2, FALSE);
    } else lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(checkbutton2), prefs->enc_letterbox);

    lives_signal_sync_connect_after(LIVES_GUI_OBJECT(checkbutton2), LIVES_WIDGET_TOGGLED_SIGNAL,
                                    LIVES_GUI_CALLBACK(on_boolean_toggled), &prefs->enc_letterbox);

    if (opt_resize)
      lives_signal_sync_connect_after(LIVES_GUI_OBJECT(checkbutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                                      LIVES_GUI_CALLBACK(on_resizecb_toggled), checkbutton2);
  }

  if (text2) {
    label = lives_standard_label_new(text2);
    lives_box_pack_start(LIVES_BOX(dialog_vbox), label, TRUE, TRUE, 0);
    lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_CANCEL, NULL,
                                       LIVES_RESPONSE_CANCEL);
    okbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_OK, NULL,
               LIVES_RESPONSE_OK);
  } else {
    widget_opts.expand = LIVES_EXPAND_EXTRA_WIDTH | LIVES_EXPAND_DEFAULT_HEIGHT;
    lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), NULL, _("Keep _my settings"),
                                       LIVES_RESPONSE_CANCEL);
    okbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), NULL, _("Use _recommended settings"),
               LIVES_RESPONSE_OK);
    widget_opts.expand = LIVES_EXPAND_DEFAULT;
  }

  lives_button_grab_default_special(okbutton);

  lives_widget_show_all(dialog);
  return dialog;
}


LiVESWidget *scrolled_textview(const char *text, LiVESTextBuffer *textbuffer, int window_width,
                               LiVESWidget **ptextview) {
  LiVESWidget *scrolledwindow = NULL;
  LiVESWidget *textview = lives_standard_text_view_new(text, textbuffer);
  if (textview) {
    int woex = widget_opts.expand;
    int height = RFX_WINSIZE_V;
    if (!LIVES_SHOULD_EXPAND_HEIGHT) height >>= 1;
    widget_opts.expand = LIVES_EXPAND_EXTRA_WIDTH | LIVES_EXPAND_DEFAULT_HEIGHT;
    scrolledwindow = lives_standard_scrolled_window_new(window_width, height, textview);
    widget_opts.expand = woex;
    lives_container_set_border_width(LIVES_CONTAINER(scrolledwindow), widget_opts.border_width);
    if (palette->style & STYLE_1) {
      lives_widget_set_bg_color(lives_bin_get_child(LIVES_BIN(scrolledwindow)),
                                LIVES_WIDGET_STATE_NORMAL, &palette->info_base);
    }
  }
  if (ptextview) *ptextview = textview;
  return scrolledwindow;
}


text_window *create_text_window(const char *title, const char *text, LiVESTextBuffer *textbuffer,
                                boolean add_buttons) {
  // general text window
  LiVESWidget *dialog_vbox;

  int woat;
  int window_width = RFX_WINSIZE_H;

  textwindow = (text_window *)lives_malloc(sizeof(text_window));

  if (LIVES_SHOULD_EXPAND_EXTRA_WIDTH) window_width
      = RFX_WINSIZE_H * 1.5 * widget_opts.scale;

  textwindow->dialog = lives_standard_dialog_new(title, FALSE, window_width,
                       LIVES_SHOULD_EXPAND_HEIGHT ? DEF_DIALOG_HEIGHT
                       : DEF_DIALOG_HEIGHT >> 1);

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(textwindow->dialog));
  textwindow->vbox = lives_vbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(dialog_vbox), textwindow->vbox, TRUE, TRUE, 0);

  textwindow->textview = textwindow->table = NULL;

  woat = widget_opts.apply_theme;
  widget_opts.apply_theme = 0;
  if (textbuffer || text)
    textwindow->scrolledwindow = scrolled_textview(text, textbuffer, window_width, &textwindow->textview);
  else {
    widget_opts.expand = LIVES_EXPAND_EXTRA_WIDTH;
    textwindow->table = lives_standard_table_new(1, 1, FALSE);
    widget_opts.expand = LIVES_EXPAND_DEFAULT;
    textwindow->scrolledwindow = lives_standard_scrolled_window_new(window_width, RFX_WINSIZE_V, textwindow->table);
  }
  widget_opts.apply_theme = woat;

  lives_box_pack_start(LIVES_BOX(textwindow->vbox), textwindow->scrolledwindow, TRUE, TRUE, 0);

  if (add_buttons && (text || mainw->iochan || textwindow->table)) {
    if (!textwindow->table) {
      LiVESWidget *savebutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(textwindow->dialog),
                                LIVES_STOCK_SAVE, _("_Save to file"), LIVES_RESPONSE_YES);
      lives_signal_sync_connect(LIVES_GUI_OBJECT(savebutton), LIVES_WIDGET_CLICKED_SIGNAL,
                                LIVES_GUI_CALLBACK(on_save_textview_clicked), textwindow->textview);
    }

    textwindow->button = lives_dialog_add_button_from_stock(LIVES_DIALOG(textwindow->dialog),
                         LIVES_STOCK_CLOSE, _("_Close Window"), LIVES_RESPONSE_CANCEL);

    if (!textwindow->table) {
      LiVESWidget *bbox = lives_dialog_get_action_area(LIVES_DIALOG(textwindow->dialog));
      lives_button_box_set_layout(LIVES_BUTTON_BOX(bbox), LIVES_BUTTONBOX_SPREAD);
    }

    lives_button_grab_default_special(textwindow->button);

    lives_signal_sync_connect(LIVES_GUI_OBJECT(textwindow->button), LIVES_WIDGET_CLICKED_SIGNAL,
                              LIVES_GUI_CALLBACK(lives_general_button_clickedp), &textwindow);

    lives_window_add_escape(LIVES_WINDOW(textwindow->dialog), textwindow->button);
  }

  if (prefs->show_gui)
    lives_widget_show_all(textwindow->dialog);

  return textwindow;
}


void do_logger_dialog(const char *title, const char *text, const char *buff, boolean add_abort) {
  text_window *textwindow;
  LiVESWidget *top_vbox, *label;

  widget_opts.expand = LIVES_EXPAND_EXTRA_WIDTH;
  textwindow = create_text_window(title, buff, NULL, FALSE);
  widget_opts.expand = LIVES_EXPAND_DEFAULT;

  top_vbox = lives_dialog_get_content_area(LIVES_DIALOG(textwindow->dialog));

  label = lives_standard_formatted_label_new(text);

  lives_box_pack_start(LIVES_BOX(top_vbox), label, FALSE, TRUE, 0);

  lives_widget_object_ref(textwindow->vbox);
  lives_widget_unparent(textwindow->vbox);

  widget_opts.justify = LIVES_JUSTIFY_CENTER;
  lives_standard_expander_new(_("Show _Log"), _("Hide _Log"), LIVES_BOX(top_vbox),
                              textwindow->vbox);

  widget_opts.justify = LIVES_JUSTIFY_DEFAULT;
  lives_widget_object_unref(textwindow->vbox);

  add_fill_to_box(LIVES_BOX(top_vbox));

  if (!add_abort) {
    LiVESWidget *button = lives_dialog_add_button_from_stock(LIVES_DIALOG(textwindow->dialog),
                          LIVES_STOCK_CLOSE, LIVES_STOCK_LABEL_CLOSE_WINDOW,
                          LIVES_RESPONSE_OK);
    lives_window_add_escape(LIVES_WINDOW(textwindow->dialog), button);
    lives_button_grab_default_special(button);
  } else {
    lives_dialog_add_button_from_stock(LIVES_DIALOG(textwindow->dialog),
                                       LIVES_STOCK_QUIT, _("_Abort"), LIVES_RESPONSE_ABORT);

  }

  lives_dialog_run(LIVES_DIALOG(textwindow->dialog));
  lives_widget_destroy(textwindow->dialog);
  lives_free(textwindow);

  if (add_abort) maybe_abort(FALSE, NULL);
}


static int ret_int(void *data) {
  if (data) return *(int *)data;
  return 0;
}

_insertw *create_insert_dialog(void) {
  LiVESWidget *dialog_vbox;
  LiVESWidget *hbox1;
  LiVESWidget *hbox;
  LiVESWidget *table;
  LiVESWidget *radiobutton;
  LiVESWidget *vseparator;
  LiVESWidget *cancelbutton;
  LiVESWidget *okbutton;
  LiVESWidget *label;

  LiVESSList *radiobutton1_group = NULL;
  LiVESSList *radiobutton2_group = NULL;

  char *tmp, *tmp2;

  _insertw *insertw = (_insertw *)(lives_malloc(sizeof(_insertw)));

  insertw->insert_dialog = lives_standard_dialog_new(_("Insert"), FALSE, -1, -1);
  lives_signal_handlers_disconnect_by_func(insertw->insert_dialog, LIVES_GUI_CALLBACK(return_true),
      NULL);

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(insertw->insert_dialog));

  hbox1 = lives_hbox_new(FALSE, 0);

  lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox1, TRUE, TRUE, widget_opts.packing_height);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(hbox1), hbox, FALSE, FALSE, widget_opts.packing_width);

  insertw->spinbutton_times = lives_standard_spin_button_new(_("_Number of times to insert"),
                              1., 1., 10000., 1., 10., 0., LIVES_BOX(hbox), NULL);

  lives_widget_grab_focus(insertw->spinbutton_times);

  add_fill_to_box(LIVES_BOX(hbox1));

  hbox = lives_hbox_new(FALSE, 0);

  lives_box_pack_start(LIVES_BOX(hbox1), hbox, FALSE, FALSE, widget_opts.packing_width);

  if (cfile->frames == 0)
    insertw->fit_checkbutton = lives_standard_check_button_new(_("_Insert to fit audio"), mainw->fx1_bool, LIVES_BOX(hbox), NULL);
  else
    insertw->fit_checkbutton = lives_standard_check_button_new(_("_Insert from selection end to audio end"),
                               mainw->fx1_bool, LIVES_BOX(hbox), NULL);
  label = widget_opts.last_label;
  add_hsep_to_box(LIVES_BOX(dialog_vbox));

  table = lives_table_new(2, 3, FALSE);
  lives_box_pack_start(LIVES_BOX(dialog_vbox), table, TRUE, TRUE, widget_opts.packing_height);
  lives_table_set_col_spacings(LIVES_TABLE(table), widget_opts.packing_width * 4);
  lives_table_set_row_spacings(LIVES_TABLE(table), widget_opts.packing_height * 2);

  hbox = lives_hbox_new(FALSE, 0);

  radiobutton = lives_standard_radio_button_new((tmp = (_("Insert _before selection"))),
                &radiobutton1_group, LIVES_BOX(hbox),
                (tmp2 = (_("Insert clipboard before selected frames"))));

  lives_free(tmp); lives_free(tmp2);

  lives_table_attach(LIVES_TABLE(table), hbox, 0, 1, 0, 1,
                     (LiVESAttachOptions)(LIVES_FILL),
                     (LiVESAttachOptions)(0), 0, 0);

  if (cfile->frames == 0) lives_widget_set_sensitive(radiobutton, FALSE);

  toggle_sets_sensitive_cond(LIVES_TOGGLE_BUTTON(insertw->fit_checkbutton), radiobutton, ret_int, &cfile->frames, NULL, NULL,
                             TRUE);

  hbox = lives_hbox_new(FALSE, 0);

  radiobutton = lives_standard_radio_button_new((tmp = (_("Insert _after selection"))),
                &radiobutton1_group, LIVES_BOX(hbox),
                (tmp2 = (_("Insert clipboard after selected frames"))));

  lives_table_attach(LIVES_TABLE(table), hbox, 0, 1, 1, 2,
                     (LiVESAttachOptions)(LIVES_FILL),
                     (LiVESAttachOptions)(0), 0, 0);

  toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(radiobutton), insertw->fit_checkbutton, FALSE);
  toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(insertw->fit_checkbutton), radiobutton, TRUE);
  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(radiobutton), mainw->insert_after);

  hbox = lives_hbox_new(FALSE, 0);

  if (clipboard->achans == 0)
    insertw->with_sound = lives_standard_radio_button_new(_("Insert _with silence"),
                          &radiobutton2_group, LIVES_BOX(hbox), NULL);
  else
    insertw->with_sound = lives_standard_radio_button_new(_("Insert _with sound"),
                          &radiobutton2_group, LIVES_BOX(hbox), NULL);

  lives_table_attach(LIVES_TABLE(table), hbox, 2, 3, 0, 1,
                     (LiVESAttachOptions)(LIVES_FILL),
                     (LiVESAttachOptions)(0), 0, 0);

  hbox = lives_hbox_new(FALSE, 0);

  insertw->without_sound = lives_standard_radio_button_new(_("Insert with_out sound"),
                           &radiobutton2_group, LIVES_BOX(hbox), NULL);

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(insertw->without_sound),
                                 (cfile->achans == 0 && clipboard->achans == 0) || !mainw->ccpd_with_sound);

  lives_table_attach(LIVES_TABLE(table), hbox, 2, 3, 1, 2,
                     (LiVESAttachOptions)(LIVES_FILL),
                     (LiVESAttachOptions)(0), 0, 0);

  vseparator = lives_vseparator_new();
  lives_table_attach(LIVES_TABLE(table), vseparator, 1, 2, 0, 1,
                     (LiVESAttachOptions)(LIVES_EXPAND | LIVES_FILL),
                     (LiVESAttachOptions)(LIVES_FILL), 0, 0);

  vseparator = lives_vseparator_new();
  lives_table_attach(LIVES_TABLE(table), vseparator, 1, 2, 1, 2,
                     (LiVESAttachOptions)(LIVES_FILL),
                     (LiVESAttachOptions)(LIVES_FILL), 0, 0);

  add_fill_to_box(LIVES_BOX(dialog_vbox));

  if (cfile->achans == 0 || (double)cfile->end / cfile->fps >= cfile->laudio_time - 0.0001) {
    lives_widget_set_no_show_all(insertw->fit_checkbutton, TRUE);
    lives_widget_set_no_show_all(label, TRUE);
  } else {
    toggle_toggles_var(LIVES_TOGGLE_BUTTON(insertw->fit_checkbutton), &mainw->fx1_bool, FALSE);
    toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(insertw->fit_checkbutton), insertw->spinbutton_times, TRUE);
    toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(insertw->fit_checkbutton), insertw->with_sound, TRUE);
    toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(insertw->fit_checkbutton), insertw->without_sound, TRUE);
  }

  cancelbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(insertw->insert_dialog), LIVES_STOCK_CANCEL, NULL,
                 LIVES_RESPONSE_CANCEL);
  okbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(insertw->insert_dialog), LIVES_STOCK_OK, NULL,
             LIVES_RESPONSE_OK);

  lives_button_grab_default_special(okbutton);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(insertw->with_sound), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_insertwsound_toggled),  insertw);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(radiobutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_boolean_toggled), &mainw->insert_after);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(cancelbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(lives_general_button_clicked), insertw);
  lives_signal_connect(LIVES_GUI_OBJECT(okbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                       LIVES_GUI_CALLBACK(on_insert_activate), insertw);
  lives_signal_sync_connect_after(LIVES_GUI_OBJECT(insertw->spinbutton_times), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                                  LIVES_GUI_CALLBACK(on_spin_value_changed), LIVES_INT_TO_POINTER(1));
  lives_window_add_escape(LIVES_WINDOW(insertw->insert_dialog), cancelbutton);

  lives_widget_show_all(insertw->insert_dialog);

  return insertw;
}


LiVESWidget *trash_rb(LiVESButtonBox *parent) {
  /// parent should be a bbox
  LiVESWidget *rb = NULL;

  if (check_for_executable(&capable->has_gio, EXEC_GIO) == PRESENT) {
    LiVESSList *rb_group = NULL;
    LiVESWidget *vbox, *hbox;
    char *tmp, *tmp2;

    hbox = lives_hbox_new(FALSE, 0);
    vbox = lives_vbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(hbox), vbox, FALSE, FALSE, widget_opts.packing_width);

    widget_opts.expand = LIVES_EXPAND_DEFAULT_WIDTH;
    rb = lives_standard_radio_button_new((tmp = (_("Send to Trash"))), &rb_group,
                                         LIVES_BOX(vbox), (tmp2 = (H_("Send deleted items to filesystem Trash\n"
                                             "instead of erasing them permanently"))));
    lives_free(tmp); lives_free(tmp2);

    rb = lives_standard_radio_button_new((tmp = (_("Delete"))), &rb_group, LIVES_BOX(vbox),
                                         (tmp2 = (H_("Permanently erase items from the disk"))));

    lives_free(tmp); lives_free(tmp2);
    widget_opts.expand = LIVES_EXPAND_DEFAULT;

    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(rb), !prefs->pref_trash);

    lives_signal_sync_connect(LIVES_GUI_OBJECT(rb), LIVES_WIDGET_ACTIVATE_SIGNAL,
                              LIVES_GUI_CALLBACK(toggle_sets_pref), PREF_PREF_TRASH);

    lives_box_pack_start(LIVES_BOX(parent), hbox, FALSE, FALSE, 0);
    lives_button_box_make_first(LIVES_BUTTON_BOX(parent), hbox);
  }
  return rb;
}

static LiVESResponseType filtresp;
static char *rec_text = NULL, *rem_text = NULL, *leave_text = NULL;

static void filt_cb_toggled(LiVESWidget *cb, lives_file_dets_t *filedets) {
  if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(cb))) {
    if (filedets->widgets[1]) {
      lives_widget_set_sensitive(filedets->widgets[1], TRUE);
      if (lives_toggle_button_get_active(filedets->widgets[1]))
        lives_label_set_text(LIVES_LABEL(filedets->widgets[8]), rec_text);
      else
        lives_label_set_text(LIVES_LABEL(filedets->widgets[8]), rem_text);
    } else
      lives_label_set_text(LIVES_LABEL(filedets->widgets[8]), rem_text);
  } else {
    if (filedets->widgets[1]) {
      lives_widget_set_sensitive(filedets->widgets[1], FALSE);
    }
    lives_label_set_text(LIVES_LABEL(filedets->widgets[8]), leave_text);
  }
}

static void filt_sw_toggled(LiVESWidget *sw, lives_file_dets_t *filedets) {
  if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(sw)))
    lives_label_set_text(LIVES_LABEL(filedets->widgets[8]), rec_text);
  else
    lives_label_set_text(LIVES_LABEL(filedets->widgets[8]), rem_text);
}

static void filt_all_toggled(LiVESWidget *cb, LiVESList *list) {
  lives_file_dets_t *filedets;
  boolean act = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(cb));
  for (; list && list->data; list = list->next) {
    filedets = (lives_file_dets_t *)(list->data);
    lives_toggle_button_set_active(filedets->widgets[0], act);
  }
}

static void filt_reset_clicked(LiVESWidget *layout, LiVESWidget *rbut) {
  LiVESList *list, *xlist;
  LiVESWidget *cb =
    (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(layout), "cb");
  int ptype;
  lives_file_dets_t *filedets;

  if (!cb) return;

  ptype = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(layout), "ptype"));
  xlist = list = (LiVESList *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(layout), "list");

  switch (ptype) {
  case 0:
    for (; xlist && xlist->data; xlist = xlist->next) {
      filedets = (lives_file_dets_t *)(xlist->data);
      lives_toggle_button_set_active(filedets->widgets[1], TRUE);
    }
  // fall through
  case 1:
    if (!lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(cb)))
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(cb), TRUE);
    else
      filt_all_toggled(cb, list);
    break;
  case 2:
    if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(cb)))
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(cb), FALSE);
    else
      filt_all_toggled(cb, list);
    break;
  default: break;
  }
}

static boolean filtc_response(LiVESWidget *w, LiVESResponseType resp, livespointer data) {
  filtresp = resp;
  return TRUE;
}

#define NMLEN_MAX 33
static boolean fill_filt_section(LiVESList **listp, int pass, int type, LiVESWidget *layout) {
  LiVESList *list = (LiVESList *)*listp;
  lives_file_dets_t *filedets;
  LiVESWidget *dialog = NULL;
  LiVESWidget *hbox;
  LiVESWidget *cb = NULL;

  char *txt, *dtxt;
  boolean needs_recheck = FALSE;

  if (!pass) widget_opts.mnemonic_label = FALSE;

  if (!list->data) {
    if (!pass) {
      txt = lives_strdup_printf("  - %s -  ",
                                mainw->string_constants[LIVES_STRING_CONSTANT_NONE]);
      widget_opts.expand = LIVES_EXPAND_EXTRA_WIDTH | LIVES_EXPAND_DEFAULT_HEIGHT;
      widget_opts.justify = LIVES_JUSTIFY_CENTER;
      lives_layout_add_label(LIVES_LAYOUT(layout), txt, FALSE);
      widget_opts.justify = LIVES_JUSTIFY_DEFAULT;;
      widget_opts.expand = LIVES_EXPAND_DEFAULT;
      lives_free(txt);
      widget_opts.mnemonic_label = TRUE;
    }
    return FALSE;
  }
  if (!pass) {
    int woat = widget_opts.apply_theme;
    int woph = widget_opts.packing_height;

    if (!rec_text) rec_text = (_("Recover"));
    if (!rem_text) rem_text = (_("Delete"));
    if (!leave_text) leave_text = (_("Leave"));

    dialog = lives_widget_get_toplevel(layout);

    widget_opts.apply_theme = 2;
    hbox = lives_layout_row_new(LIVES_LAYOUT(layout));

    //widget_opts.expand = LIVES_EXPAND_NONE;

    // do this to counter effect of setting margin
    widget_opts.packing_height = 0;
    cb = lives_standard_check_button_new(_("All"), type != 2, LIVES_BOX(hbox), NULL);
    lives_widget_set_halign(hbox, LIVES_ALIGN_FILL);
    widget_opts.packing_height = woph;
    lives_widget_set_margin_left(cb, widget_opts.border_width >> 1);
    lives_widget_set_margin_right(cb, widget_opts.border_width);
    lives_widget_set_margin_top(cb, widget_opts.border_width >> 1);
    lives_widget_set_margin_bottom(cb, widget_opts.border_width >> 1);
    lives_box_set_child_packing(LIVES_BOX(hbox), cb, FALSE, FALSE, widget_opts.packing_width >> 1,
                                LIVES_PACK_START);

    lives_widget_set_sensitive(cb, FALSE);
    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(layout), "cb", (livespointer)cb);

    if (!type) {
      widget_opts.justify = LIVES_JUSTIFY_CENTER;
      lives_layout_add_label(LIVES_LAYOUT(layout), _("Action"), TRUE);
      widget_opts.justify = LIVES_JUSTIFY_DEFAULT;
    }

    widget_opts.expand = LIVES_EXPAND_EXTRA_WIDTH | LIVES_EXPAND_DEFAULT_HEIGHT;
    lives_layout_add_label(LIVES_LAYOUT(layout), _("Name"), TRUE);
    lives_layout_add_fill(LIVES_LAYOUT(layout), TRUE);
    lives_layout_add_label(LIVES_LAYOUT(layout), _("Size"), TRUE);
    lives_layout_add_fill(LIVES_LAYOUT(layout), TRUE);
    lives_layout_add_label(LIVES_LAYOUT(layout), _("Modified Date"), TRUE);
    lives_layout_add_fill(LIVES_LAYOUT(layout), TRUE);
    lives_layout_add_label(LIVES_LAYOUT(layout), _("Details"), TRUE);
    widget_opts.expand = LIVES_EXPAND_DEFAULT;
    widget_opts.apply_theme = woat;
    lives_layout_add_separator(LIVES_LAYOUT(layout), FALSE);
  }

  while (list->data) {
    // put from recover subdir
    filedets = (lives_file_dets_t *)(list->data);
    if (!pass) {
      hbox = lives_layout_row_new(LIVES_LAYOUT(layout));

      filedets->widgets[0] = lives_standard_check_button_new("", type != 2, LIVES_BOX(hbox), NULL);
      filedets->widgets[8] = widget_opts.last_label;

      if (!type) {
        hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
        filedets->widgets[1] = lives_standard_switch_new(NULL, TRUE, LIVES_BOX(hbox), NULL);
        lives_signal_sync_connect(LIVES_GUI_OBJECT(filedets->widgets[1]), LIVES_WIDGET_TOGGLED_SIGNAL,
                                  LIVES_GUI_CALLBACK(filt_sw_toggled), (livespointer)filedets);
      } else filedets->widgets[1] = NULL;

      lives_signal_sync_connect(LIVES_GUI_OBJECT(filedets->widgets[0]), LIVES_WIDGET_TOGGLED_SIGNAL,
                                LIVES_GUI_CALLBACK(filt_cb_toggled), (livespointer)filedets);

      filt_cb_toggled(filedets->widgets[0], filedets);

      txt = lives_pad_ellipsize(filedets->name, NMLEN_MAX, LIVES_ALIGN_START, LIVES_ELLIPSIZE_MIDDLE);
      lives_layout_add_label(LIVES_LAYOUT(layout), txt, TRUE);
      if (txt != filedets->name) {
        lives_free(txt);
        lives_widget_set_tooltip_text(widget_opts.last_label, filedets->name);
      }
      lives_layout_add_fill(LIVES_LAYOUT(layout), TRUE);
      hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));

      filedets->widgets[2] = lives_standard_label_new(NULL);
      lives_layout_pack(LIVES_BOX(hbox), filedets->widgets[2]);
      lives_widget_hide(filedets->widgets[2]);
      lives_widget_set_no_show_all(filedets->widgets[2], TRUE);

      if (filedets->size == -1) {
        filedets->widgets[3] = lives_spinner_new();
        if (filedets->widgets[3]) {
          lives_layout_pack(LIVES_BOX(hbox), filedets->widgets[3]);
          lives_spinner_start(LIVES_SPINNER(filedets->widgets[3]));
        } else filedets->widgets[3] = filedets->widgets[2];
      } else filedets->widgets[3] = filedets->widgets[2];
    }

    if (filedets->widgets[3]) {
      if (filedets->size != -1) {
        if (filedets->widgets[3] != filedets->widgets[2]) {
          lives_spinner_stop(LIVES_SPINNER(filedets->widgets[3]));
          lives_widget_hide(filedets->widgets[3]);
          lives_widget_set_no_show_all(filedets->widgets[3], TRUE);
        }
        lives_widget_set_no_show_all(filedets->widgets[2], FALSE);
        lives_widget_show_all(filedets->widgets[2]);

        if (filedets->size == -2) {
          lives_label_set_text(LIVES_LABEL(filedets->widgets[2]), "????");
        }
        if (filedets->size > 0) {
          txt = lives_format_storage_space_string(filedets->size);
          lives_label_set_text(LIVES_LABEL(filedets->widgets[2]), txt);
          lives_free(txt);
        }
        filedets->widgets[3] = NULL;
      } else needs_recheck = TRUE;
    }

    if (!pass) {
      lives_layout_add_fill(LIVES_LAYOUT(layout), TRUE);
      hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
      filedets->widgets[4] = lives_standard_label_new(NULL);
      lives_layout_pack(LIVES_BOX(hbox), filedets->widgets[4]);
      lives_widget_hide(filedets->widgets[4]);
      lives_widget_set_no_show_all(filedets->widgets[4], TRUE);

      if (!filedets->extra_details) {
        filedets->widgets[5] = lives_spinner_new();
        if (filedets->widgets[5]) {
          lives_layout_pack(LIVES_BOX(hbox), filedets->widgets[5]);
          lives_spinner_start(LIVES_SPINNER(filedets->widgets[5]));
        } else filedets->widgets[5] = filedets->widgets[4];
      } else filedets->widgets[5] = filedets->widgets[4];

      lives_layout_add_fill(LIVES_LAYOUT(layout), TRUE);
      hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
      filedets->widgets[6] = lives_standard_label_new(NULL);
      lives_layout_pack(LIVES_BOX(hbox), filedets->widgets[6]);
      lives_widget_hide(filedets->widgets[6]);
      lives_widget_set_no_show_all(filedets->widgets[6], TRUE);

      if (!filedets->extra_details) {
        filedets->widgets[7] = lives_spinner_new();
        if (filedets->widgets[7]) {
          widget_opts.justify = LIVES_JUSTIFY_CENTER;
          widget_opts.expand = LIVES_EXPAND_EXTRA_WIDTH | LIVES_EXPAND_DEFAULT_HEIGHT;
          lives_layout_pack(LIVES_BOX(hbox), filedets->widgets[7]);
          widget_opts.expand = LIVES_EXPAND_DEFAULT;
          widget_opts.justify = LIVES_JUSTIFY_DEFAULT;
          lives_spinner_start(LIVES_SPINNER(filedets->widgets[7]));
        } else filedets->widgets[7] = filedets->widgets[6];
      } else filedets->widgets[7] = filedets->widgets[6];
    }

    if (filedets->widgets[5]) {
      if (filedets->extra_details) {
        if (filedets->widgets[5] != filedets->widgets[4]) {
          lives_spinner_stop(LIVES_SPINNER(filedets->widgets[5]));
          lives_widget_hide(filedets->widgets[5]);
          lives_widget_set_no_show_all(filedets->widgets[5], TRUE);
        }
        if (filedets->widgets[4]) {
          lives_widget_set_no_show_all(filedets->widgets[4], FALSE);
          lives_widget_show_all(filedets->widgets[4]);
          if (!filedets->mtime_sec) {
            lives_label_set_text(LIVES_LABEL(filedets->widgets[4]), "????");
          } else {
            txt = lives_datetime(filedets->mtime_sec, TRUE);
            dtxt = lives_datetime_rel(txt);
            lives_label_set_text(LIVES_LABEL(filedets->widgets[4]), dtxt);
            if (dtxt != txt) lives_free(dtxt);
            lives_free(txt);
          }
        }

        if (filedets->widgets[7] != filedets->widgets[6]) {
          lives_spinner_stop(LIVES_SPINNER(filedets->widgets[7]));
          lives_widget_hide(filedets->widgets[7]);
          lives_widget_set_no_show_all(filedets->widgets[7], TRUE);
        }
        lives_widget_set_no_show_all(filedets->widgets[6], FALSE);
        lives_widget_show_all(filedets->widgets[6]);

        if ((filedets->type & LIVES_FILE_TYPE_MASK) == LIVES_FILE_TYPE_UNKNOWN) {
          lives_label_set_text(LIVES_LABEL(filedets->widgets[6]), "????");
        } else {
          if (LIVES_FILE_IS_FILE(filedets->type))
            txt = lives_strdup_printf(_("\tFile\t\t:\t%s"),
                                      filedets->extra_details ? filedets->extra_details : " - ");
          else if (LIVES_FILE_IS_DIRECTORY(filedets->type))
            txt = lives_strdup_printf(_("\tDirectory\t\t:\t%s"),
                                      filedets->extra_details ? filedets->extra_details : " - ");
          else
            txt = lives_strdup_printf(_("\t????????\t\t:\t%s"),
                                      filedets->extra_details ? filedets->extra_details : " - ");
          lives_label_set_line_wrap(LIVES_LABEL(filedets->widgets[6]), TRUE);
          lives_label_set_text(LIVES_LABEL(filedets->widgets[6]), txt);
          lives_free(txt);
        }
        filedets->widgets[5] = NULL;
      } else needs_recheck = TRUE;
    }

    if (!pass) {
      lives_widget_show_all(dialog);
      do {
        lives_widget_context_update();
        //lives_widget_process_updates(dialog);
        lives_nanosleep(100);
      } while (!list->next && filtresp == LIVES_RESPONSE_NONE);
    }
    if (filtresp != LIVES_RESPONSE_NONE) goto ffxdone;
    list = list->next;
  }
  if (cb) {
    lives_signal_sync_connect(LIVES_GUI_OBJECT(cb), LIVES_WIDGET_TOGGLED_SIGNAL,
                              LIVES_GUI_CALLBACK(filt_all_toggled), (livespointer)*listp);
    lives_widget_set_sensitive(cb, TRUE);
  }
ffxdone:
  widget_opts.mnemonic_label = TRUE;
  return needs_recheck;
}


LiVESResponseType filter_cleanup(const char *trashdir, LiVESList **rec_list, LiVESList **rem_list,
                                 LiVESList **left_list) {
  LiVESWidget *dialog;
  LiVESWidget *layout, *layout_rec, *layout_rem, *layout_leave;
  LiVESWidget *top_vbox;
  LiVESWidget *scrolledwindow;
  LiVESWidget *cancelb;
  LiVESWidget *resetb = NULL;
  LiVESWidget *accb;
  LiVESWidget *vbox;

  int winsize_h = GUI_SCREEN_WIDTH - SCR_WIDTH_SAFETY;
  int winsize_v = GUI_SCREEN_HEIGHT - SCR_HEIGHT_SAFETY * 2;
  int rec_recheck, rem_recheck, leave_recheck;
  int pass = 0;
  int woat = widget_opts.apply_theme;
  int wopw = widget_opts.packing_width;

  char *txt;

  // get size, type (dir or file), nitems, extra_dets
  // cr dat, mod date

  filtresp = LIVES_RESPONSE_NONE;

  dialog = lives_standard_dialog_new(_("Disk Cleanup"), FALSE, winsize_h, winsize_v);
  lives_widget_set_maximum_size(dialog, winsize_h, winsize_v);

  if ((*rec_list && (*rec_list)->data) || (*rem_list && (*rem_list)->data)
      || (*left_list && (*left_list)->data)) {
    LiVESWidget *bbox = lives_dialog_get_action_area(LIVES_DIALOG(dialog));
    cancelb = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog),
              LIVES_STOCK_CANCEL, NULL, LIVES_RESPONSE_CANCEL);

    lives_window_add_escape(LIVES_WINDOW(dialog), cancelb);

    resetb = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog),
             LIVES_STOCK_UNDO, LIVES_STOCK_LABEL_RESET, LIVES_RESPONSE_NONE);
    lives_widget_set_sensitive(resetb, FALSE);

    widget_opts.expand = LIVES_EXPAND_EXTRA_WIDTH | LIVES_EXPAND_DEFAULT_HEIGHT;
    accb = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog),
           LIVES_STOCK_GO_FORWARD, _("_Accept and Continue"), LIVES_RESPONSE_ACCEPT);
    widget_opts.expand = LIVES_EXPAND_DEFAULT;
    lives_widget_set_sensitive(accb, FALSE);

    trash_rb(LIVES_BUTTON_BOX(bbox));
    lives_dialog_set_button_layout(LIVES_DIALOG(dialog), LIVES_BUTTONBOX_CENTER);
  } else {
    accb = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog),
           LIVES_STOCK_CLOSE, LIVES_STOCK_LABEL_CLOSE_WINDOW, LIVES_RESPONSE_OK);
    lives_window_add_escape(LIVES_WINDOW(dialog), accb);

    lives_button_grab_default_special(accb);
  }

  lives_signal_sync_connect(dialog, LIVES_WIDGET_RESPONSE_SIGNAL,
                            LIVES_GUI_CALLBACK(filtc_response), NULL);

  top_vbox = lives_dialog_get_content_area(LIVES_DIALOG(dialog));

  layout = lives_layout_new(LIVES_BOX(top_vbox));
  lives_layout_add_fill(LIVES_LAYOUT(layout), FALSE);
  txt = lives_strdup_printf(_("Analysis of directory: %s"), prefs->workdir);
  lives_layout_add_label(LIVES_LAYOUT(layout), txt, FALSE);
  lives_free(txt);
  lives_layout_add_fill(LIVES_LAYOUT(layout), FALSE);

  vbox = lives_vbox_new(FALSE, 0);
  widget_opts.apply_theme = 0;
  scrolledwindow = lives_standard_scrolled_window_new(winsize_h, winsize_v, vbox);
  widget_opts.apply_theme = woat;
  lives_box_pack_start(LIVES_BOX(top_vbox), scrolledwindow, TRUE, TRUE, widget_opts.packing_height);

  /// items for recovery /////////////////////////

  layout_rec = lives_layout_new(LIVES_BOX(vbox));
  widget_opts.expand = LIVES_EXPAND_EXTRA_WIDTH | LIVES_EXPAND_DEFAULT_HEIGHT;
  widget_opts.justify = LIVES_JUSTIFY_CENTER;
  lives_layout_add_label(LIVES_LAYOUT(layout_rec), _("Possibly Recoverable Clips"), FALSE);
  widget_opts.justify = LIVES_JUSTIFY_DEFAULT;
  widget_opts.expand = LIVES_EXPAND_DEFAULT;
  lives_layout_add_fill(LIVES_LAYOUT(layout_rec), FALSE);

  lives_widget_show_all(dialog);

  do {
    lives_widget_process_updates(dialog);
    if (!*rec_list && filtresp == LIVES_RESPONSE_NONE) lives_nanosleep(1000);
  } while (!*rec_list && filtresp == LIVES_RESPONSE_NONE);

  if (filtresp != LIVES_RESPONSE_NONE) goto harlem_shuffle;

  rec_recheck = fill_filt_section(rec_list, pass, 0, layout_rec);
  lives_layout_add_fill(LIVES_LAYOUT(layout_rec), FALSE);

  add_hsep_to_box(LIVES_BOX(vbox));

  layout_rem = lives_layout_new(LIVES_BOX(vbox));
  widget_opts.packing_width = wopw;
  lives_layout_add_fill(LIVES_LAYOUT(layout_rem), FALSE);
  lives_layout_add_fill(LIVES_LAYOUT(layout_rem), FALSE);
  widget_opts.expand = LIVES_EXPAND_EXTRA_WIDTH | LIVES_EXPAND_DEFAULT_HEIGHT;
  widget_opts.justify = LIVES_JUSTIFY_CENTER;
  lives_layout_add_label(LIVES_LAYOUT(layout_rem), _("Items for Automatic Removal"), FALSE);
  widget_opts.justify = LIVES_JUSTIFY_DEFAULT;
  widget_opts.expand = LIVES_EXPAND_DEFAULT;
  lives_layout_add_fill(LIVES_LAYOUT(layout_rem), FALSE);

  lives_widget_show_all(dialog);

  do {
    lives_widget_process_updates(dialog);
    if (!*rem_list && filtresp == LIVES_RESPONSE_NONE) lives_nanosleep(1000);
  } while (!*rem_list && filtresp == LIVES_RESPONSE_NONE);

  if (filtresp != LIVES_RESPONSE_NONE) goto harlem_shuffle;

  rem_recheck = fill_filt_section(rem_list, pass, 1, layout_rem);
  lives_layout_add_fill(LIVES_LAYOUT(layout_rem), FALSE);

  add_hsep_to_box(LIVES_BOX(vbox));

  /// items for manual removal
  layout_leave = lives_layout_new(LIVES_BOX(vbox));
  lives_layout_add_fill(LIVES_LAYOUT(layout_leave), FALSE);
  lives_layout_add_fill(LIVES_LAYOUT(layout_leave), FALSE);
  widget_opts.expand = LIVES_EXPAND_EXTRA_WIDTH | LIVES_EXPAND_DEFAULT_HEIGHT;
  widget_opts.justify = LIVES_JUSTIFY_CENTER;
  lives_layout_add_label(LIVES_LAYOUT(layout_leave), _("Items for Manual Removal"), FALSE);
  widget_opts.justify = LIVES_JUSTIFY_DEFAULT;
  widget_opts.expand = LIVES_EXPAND_DEFAULT;
  lives_layout_add_fill(LIVES_LAYOUT(layout_leave), FALSE);

  lives_widget_show_all(dialog);

  do {
    lives_widget_process_updates(dialog);
    if (!*left_list && filtresp == LIVES_RESPONSE_NONE) lives_nanosleep(1000);
  } while (!*left_list && filtresp == LIVES_RESPONSE_NONE);

  if (filtresp != LIVES_RESPONSE_NONE) goto harlem_shuffle;

  leave_recheck = fill_filt_section(left_list, pass, 2, layout_leave);
  lives_layout_add_fill(LIVES_LAYOUT(layout_leave), FALSE);


  if (resetb) {
    /// reset button
    if (!pass) {
      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(layout_rec), "list", *rec_list);
      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(layout_rec), "ptype",
                                   LIVES_INT_TO_POINTER(0));
      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(layout_rem), "list", *rem_list);
      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(layout_rem), "ptype",
                                   LIVES_INT_TO_POINTER(1));
      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(layout_leave), "list", *left_list);
      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(layout_leave), "ptype",
                                   LIVES_INT_TO_POINTER(2));

      lives_signal_sync_connect_swapped(LIVES_GUI_OBJECT(resetb), LIVES_WIDGET_CLICKED_SIGNAL,
                                        LIVES_GUI_CALLBACK(filt_reset_clicked), layout_rec);
      lives_signal_sync_connect_swapped(LIVES_GUI_OBJECT(resetb), LIVES_WIDGET_CLICKED_SIGNAL,
                                        LIVES_GUI_CALLBACK(filt_reset_clicked), layout_rem);
      lives_signal_sync_connect_swapped(LIVES_GUI_OBJECT(resetb), LIVES_WIDGET_CLICKED_SIGNAL,
                                        LIVES_GUI_CALLBACK(filt_reset_clicked), layout_leave);

      lives_widget_set_sensitive(resetb, TRUE);
      lives_widget_set_sensitive(accb, TRUE);
      lives_button_grab_default_special(accb);
    }
  }

  /////////
  while (filtresp == LIVES_RESPONSE_NONE && (rec_recheck || rem_recheck || leave_recheck)) {
    ++pass;
    if (rec_recheck) rec_recheck = fill_filt_section(rec_list, pass, 0, layout_rec);
    if (rem_recheck) rem_recheck = fill_filt_section(rem_list, pass, 1, layout_rem);
    if (leave_recheck) leave_recheck = fill_filt_section(left_list, pass, 2, layout_leave);
    lives_widget_process_updates(dialog);
    if (filtresp == LIVES_RESPONSE_NONE && (rec_recheck || rem_recheck || leave_recheck)) lives_nanosleep(100);
  };

  while (filtresp == LIVES_RESPONSE_NONE) lives_dialog_run(LIVES_DIALOG(dialog));

harlem_shuffle:

  if (filtresp != LIVES_RESPONSE_CANCEL && filtresp != LIVES_RESPONSE_OK) {
    // we need to shuffle the lists around before destroying the dialog; caller will move
    // actual pointer files
    LiVESList *list, *listnext;
    lives_file_dets_t *filedets;
    lives_widget_hide(dialog);
    lives_widget_process_updates(dialog);

    for (pass = 0; pass < 3; pass++) {
      if (!pass) list = *rec_list;
      else if (pass == 1) list = *rem_list;
      else list = *left_list;
      for (; list && list->data; list = listnext) {
        listnext = list->next;
        // entries can move to rem_list or left_list
        // we no longer care about type, so the field will be reused to
        // store the origin list number
        // for this we will re-use pass, 0 -> rec_list, 1 -> rem_list, 2 -> left_list
        filedets = (lives_file_dets_t *)list->data;
        if (filedets->type & LIVES_FILE_TYPE_FLAG_SPECIAL) continue;
        filedets->type = ((uint64_t)pass | (uint64_t)LIVES_FILE_TYPE_FLAG_SPECIAL);
        if (!pass || pass == 2) {
          if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(filedets->widgets[0]))) {
            if (pass == 2 || !lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(filedets->widgets[1]))) {
              // move into delete from recover or leave
              if (list->prev) list->prev->next = list->next;
              if (list->next) list->next->prev = list->prev;
              list->prev = NULL;
              if (list == *rec_list) *rec_list = list->next;
              else if (list == *left_list) *left_list = list->next;
              list->next = *rem_list;
              (*rem_list)->prev = list;
              *rem_list = list;
            }
          }
        }
        if (pass != 2) {
          if (!lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(filedets->widgets[0]))) {
            // move to leave, from rec or rem
            if (list->prev) list->prev->next = list->next;
            if (list->next) list->next->prev = list->prev;
            list->prev = NULL;
            if (list == *rec_list) *rec_list = list->next;
            else if (list == *rem_list) *rem_list = list->next;
            list->next = *left_list;
            (*left_list)->prev = list;
            *left_list = list;
	    // *INDENT-OFF*
	  }}}}}
  // *INDENT-ON*
  lives_widget_destroy(dialog);
  //lives_widget_context_update();
  return filtresp;
}


static void set_maxflabel(opensel_win * oswin) {
  double sttime = lives_spin_button_get_value(LIVES_SPIN_BUTTON(oswin->sp_start));
  frames_t nframes = oswin->frames - (frames_t)(sttime * oswin->fps + .99999);
  char *text = lives_strdup_printf(_("[ maximum =  %d ]"), nframes);
  lives_label_set_text(LIVES_LABEL(oswin->maxflabel), text);
  lives_free(text);
  lives_spin_button_set_max(LIVES_SPIN_BUTTON(oswin->sp_frames), nframes);
  lives_spin_button_update(LIVES_SPIN_BUTTON(oswin->sp_frames));
}

static void sp_start_changed(LiVESWidget * spb, opensel_win * oswin) {set_maxflabel(oswin);}

opensel_win *create_opensel_window(int frames, double fps) {
  opensel_win *openselwin;
  LiVESWidget *dialog_vbox;
  LiVESWidget *hbox, *bbox;
  LiVESWidget *layout;
  LiVESWidget *cancelbutton;
  LiVESWidget *okbutton;

  double tottime = 0.;

  char *text, *tmp;

  frames_t def_frames = fps * 10.;
  def_frames = (frames_t)((def_frames + 99) / 100) * 100;

  if (def_frames > frames) def_frames = frames;
  if (fps > 0.) tottime = (double)frames / fps;

  openselwin = (opensel_win *)lives_calloc(1, sizeof(opensel_win));
  openselwin->frames = frames;
  openselwin->fps = fps;
  openselwin->dialog = lives_standard_dialog_new(_("Open Selection"), FALSE, RFX_WINSIZE_H, RFX_WINSIZE_V);
  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(openselwin->dialog));

  layout = lives_layout_new(LIVES_BOX(dialog_vbox));
  lives_layout_add_fill(LIVES_LAYOUT(layout), TRUE);
  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));

  openselwin->sp_start = lives_standard_spin_button_new(_("Selection start time (sec.)"),
                         0., 0., tottime, 1., 100., 2, LIVES_BOX(hbox), NULL);

  if (frames > 0 && fps > 0.)
    text = lives_strdup_printf(_("[ maximum =  %.2f ]"), tottime);
  else text = lives_strdup("");
  lives_layout_add_label(LIVES_LAYOUT(layout), text, TRUE);
  lives_free(text);

  lives_signal_sync_connect_after(LIVES_GUI_OBJECT(openselwin->sp_start), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                                  LIVES_GUI_CALLBACK(sp_start_changed), openselwin);

  lives_layout_add_fill(LIVES_LAYOUT(layout), TRUE);

  lives_layout_add_fill(LIVES_LAYOUT(layout), FALSE);
  lives_layout_add_row(LIVES_LAYOUT(layout));

  lives_layout_add_fill(LIVES_LAYOUT(layout), TRUE);
  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));

  if (fps) tmp = lives_strdup_printf(_(" at %.2f fps"), fps);
  else tmp = lives_strdup("");

  text = lives_strdup_printf(_("Number of frames to open%s"), tmp);
  lives_free(tmp);
  openselwin->sp_frames = lives_standard_spin_button_new(text, (double)def_frames, 1., (double)frames, 1.,
                          100., 0, LIVES_BOX(hbox), NULL);
  lives_free(text);

  openselwin->maxflabel = lives_layout_add_label(LIVES_LAYOUT(layout), "", TRUE);
  if (frames) set_maxflabel(openselwin);

  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
  openselwin->cb_allframes = lives_standard_check_button_new(_("Open to end"), FALSE, LIVES_BOX(hbox), NULL);
  toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(openselwin->cb_allframes), openselwin->sp_frames, TRUE);

  lives_layout_add_fill(LIVES_LAYOUT(layout), FALSE);

  bbox = lives_dialog_get_action_area(LIVES_DIALOG(openselwin->dialog));

  openselwin->preview_button =
    widget_add_preview(openselwin->dialog, LIVES_BOX(dialog_vbox), LIVES_BOX(dialog_vbox),
                       LIVES_BOX(bbox), LIVES_PREVIEW_TYPE_RANGE);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(openselwin->preview_button), LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_fs_preview_clicked2), openselwin);

  cancelbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(openselwin->dialog), LIVES_STOCK_CANCEL, NULL,
                 LIVES_RESPONSE_CANCEL);

  lives_window_add_escape(LIVES_WINDOW(openselwin->dialog), cancelbutton);

  okbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(openselwin->dialog), LIVES_STOCK_OK, NULL,
             LIVES_RESPONSE_OK);

  lives_button_grab_default_special(okbutton);

  lives_window_set_resizable(LIVES_WINDOW(openselwin->dialog), TRUE);

  if (prefs->open_maximised || prefs->fileselmax) {
    lives_window_maximize(LIVES_WINDOW(openselwin->dialog));
  }

  return openselwin;
}


_entryw *create_location_dialog(void) {
  LiVESWidget *dialog_vbox;
  LiVESWidget *cancelbutton;
  LiVESWidget *okbutton;
  LiVESWidget *label;
  LiVESWidget *checkbutton;
  LiVESWidget *hbox;

  _entryw *locw = (_entryw *)(lives_malloc(sizeof(_entryw)));

  char *title, *tmp, *tmp2;

  title = (_("Open Location"));

  locw->dialog = lives_standard_dialog_new(title, FALSE, -1, -1);
  lives_signal_handlers_disconnect_by_func(locw->dialog, LIVES_GUI_CALLBACK(return_true), NULL);

  lives_free(title);

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(locw->dialog));

  widget_opts.justify = LIVES_JUSTIFY_CENTER;

  label = lives_standard_label_new(
            _("\n\nTo open a stream, you must make sure that you have the correct libraries "
              "compiled in mplayer (or mpv).\n"
              "Also make sure you have set your bandwidth in Preferences|Streaming\n\n"));

  widget_opts.justify = LIVES_JUSTIFY_DEFAULT;

  lives_box_pack_start(LIVES_BOX(dialog_vbox), label, FALSE, FALSE, 0);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, TRUE, TRUE, widget_opts.packing_height * 2);

  locw->entry = lives_standard_entry_new(_("URL : "), "", LONG_ENTRY_WIDTH, 32768, LIVES_BOX(hbox), NULL);

  add_fill_to_box(LIVES_BOX(hbox));

  hbox = lives_hbox_new(FALSE, 0);
  checkbutton = lives_standard_check_button_new((tmp = (_("Do not send bandwidth information"))),
                prefs->no_bandwidth, LIVES_BOX(hbox),
                (tmp2 = (_("Try this setting if you are having problems getting a stream"))));

  lives_free(tmp); lives_free(tmp2);

  lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, FALSE, FALSE, widget_opts.packing_height * 2);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(checkbutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_boolean_toggled),
                            &prefs->no_bandwidth);

  add_deinterlace_checkbox(LIVES_BOX(dialog_vbox));

  cancelbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(locw->dialog), LIVES_STOCK_CANCEL, NULL,
                 LIVES_RESPONSE_CANCEL);

  okbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(locw->dialog), LIVES_STOCK_OK, NULL,
             LIVES_RESPONSE_OK);
  lives_button_grab_default_special(okbutton);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(cancelbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(lives_general_button_clicked), locw);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(okbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_location_select), locw);

  lives_window_add_escape(LIVES_WINDOW(locw->dialog), cancelbutton);

  lives_widget_show_all(locw->dialog);

  return locw;
}


static char *mkszlabel(const char *set, ssize_t size, int ccount, int lcount) {
  char *tmp;
  char *bit1 = lives_strdup_printf(_("<big>Contents of Set: <b>%s</b></big> in volume %s"),
                                   (tmp = lives_markup_escape_text(set, -1)), prefs->workdir), *bit2;
  char *szstr, *label, *laystr, *clpstr;
  lives_free(tmp);
  if (size < 0) szstr = lives_strdup(_("Calculating..."));
  else szstr = lives_format_storage_space_string(size);
  if (ccount == -1) clpstr = (_("counting..."));
  else clpstr = lives_strdup_printf("%d", ccount);
  if (lcount == -1) laystr = (_("counting..."));
  else laystr = lives_strdup_printf("%d", lcount);
  bit2 = lives_strdup_printf(_("<big>Total size = %s\tclips: <b>%s</b>\tlayouts: <b>%s</b></big>"), szstr, clpstr, laystr);
  label = lives_strdup_printf("%s\n%s\n", bit1, bit2);
  lives_free(bit1); lives_free(bit2);
  lives_free(laystr); lives_free(clpstr);
  return label;
}


static void on_set_exp(LiVESWidget * exp, _entryw * entryw) {
  lives_proc_thread_t layinfo = NULL, clipsinfo = NULL, sizinfo = NULL;
  LiVESList *lists[2];
  LiVESList **laylist = &lists[0], **clipslist = &lists[1];

  *laylist = *clipslist = NULL;

  if (!lives_expander_get_expanded(LIVES_EXPANDER(exp))) {
    if (entryw->layouts_layout) {
      lives_widget_destroy(entryw->layouts_layout);
      entryw->layouts_layout = NULL;
    }
    if (entryw->clips_layout) {
      lives_widget_destroy(entryw->clips_layout);
      entryw->clips_layout = NULL;
    }
    lives_widget_set_sensitive(entryw->entry, TRUE);
    return;
  } else {
    lives_file_dets_t *filedets;
    const char *set = lives_entry_get_text(LIVES_ENTRY(entryw->entry));
    LiVESList *list;
    ssize_t totsize = -1;
    char *txt, *dtxt;
    char *setdir = SET_DIR(set);
    char *ldirname = LAYOUTS_DIR(set);
    char *ordfilename = SET_ORDER_FILE(set);
    int woat = widget_opts.apply_theme;
    int lcount = 0, ccount = 0;

    sizinfo = lives_proc_thread_create(LIVES_THRDATTR_NONE, (lives_funcptr_t)get_dir_size, WEED_SEED_INT64, "s",
                                       setdir);

    layinfo = dir_to_file_details(laylist, ldirname, NULL, 0);

    clipsinfo =
      ordfile_to_file_details(clipslist, ordfilename, prefs->workdir,
                              EXTRA_DETAILS_CLIPHDR | EXTRA_DETAILS_DIRSIZE
                              | EXTRA_DETAILS_CHECK_MISSING);

    if (lives_proc_thread_check_finished(sizinfo)) totsize = lives_proc_thread_join_int64(sizinfo);
    txt = mkszlabel(set, totsize, -1, -1);
    widget_opts.use_markup = TRUE;
    lives_label_set_text(LIVES_LABEL(entryw->exp_label), txt);
    widget_opts.use_markup = FALSE;
    lives_free(txt);

    lives_free(ldirname); lives_free(ordfilename);

    lives_widget_set_sensitive(entryw->entry, FALSE);

    // layouts
    entryw->layouts_layout = lives_layout_new(LIVES_BOX(entryw->exp_vbox));
    widget_opts.expand = LIVES_EXPAND_EXTRA_WIDTH | LIVES_EXPAND_DEFAULT_HEIGHT;
    widget_opts.justify = LIVES_JUSTIFY_CENTER;
    lives_layout_add_label(LIVES_LAYOUT(entryw->layouts_layout), _("Layouts"), FALSE);
    widget_opts.justify = LIVES_JUSTIFY_DEFAULT;
    widget_opts.expand = LIVES_EXPAND_DEFAULT;
    lives_layout_add_fill(LIVES_LAYOUT(entryw->layouts_layout), FALSE);

    lives_widget_show_all(entryw->expander);

    do {
      lives_widget_process_updates(entryw->dialog);
      if (!*laylist && lives_expander_get_expanded(LIVES_EXPANDER(exp))) lives_nanosleep(1000);
    } while (!*laylist && lives_expander_get_expanded(LIVES_EXPANDER(exp)));

    if (!lives_expander_get_expanded(LIVES_EXPANDER(exp))) goto thrdjoin;

    if (totsize == -1) {
      if (lives_proc_thread_check_finished(sizinfo)) totsize = lives_proc_thread_join_int64(sizinfo);
      txt = mkszlabel(set, totsize, -1, -1);
      widget_opts.use_markup = TRUE;
      lives_label_set_text(LIVES_LABEL(entryw->exp_label), txt);
      widget_opts.use_markup = FALSE;
      lives_free(txt);
    }

    list = *laylist;
    if (!list->data) {
      // NONE label
      //lives_layout_add_label(LIVES_LAYOUT(entryw->layouts_layout), mainw->string_constants[LIVES_STRING_CONSTANT_NONE], FALSE);
      lives_widget_hide(entryw->layouts_layout);
      txt = mkszlabel(set, totsize, -1, 0);
      widget_opts.use_markup = TRUE;
      lives_label_set_text(LIVES_LABEL(entryw->exp_label), txt);
      widget_opts.use_markup = FALSE;
      lives_free(txt);
    } else {
      widget_opts.apply_theme = 2;
      lives_layout_add_row(LIVES_LAYOUT(entryw->layouts_layout));
      lives_layout_add_label(LIVES_LAYOUT(entryw->layouts_layout), _("Modified Date"), TRUE);
      lives_layout_add_label(LIVES_LAYOUT(entryw->layouts_layout), _("Size"), TRUE);
      lives_layout_add_label(LIVES_LAYOUT(entryw->layouts_layout), _("Name"), TRUE);
      widget_opts.apply_theme = woat;

      lives_widget_show_all(entryw->layouts_layout);

      while (list->data && lives_expander_get_expanded(LIVES_EXPANDER(exp))) {
        lcount++;
        filedets = (lives_file_dets_t *)(list->data);
        do {
          // wait for size
          lives_widget_process_updates(entryw->dialog);
          if (lives_expander_get_expanded(LIVES_EXPANDER(exp))) {
            if (!filedets->extra_details) lives_nanosleep(1000);
            if (totsize == -1) {
              if (lives_proc_thread_check_finished(sizinfo)) {
                totsize = lives_proc_thread_join_int64(sizinfo);
                txt = mkszlabel(set, totsize, -1, lcount);
                widget_opts.use_markup = TRUE;
                lives_label_set_text(LIVES_LABEL(entryw->exp_label), txt);
                widget_opts.use_markup = FALSE;
                lives_free(txt);
              }
            }
          }
        } while (!filedets->extra_details && lives_expander_get_expanded(LIVES_EXPANDER(exp)));
        if (!lives_expander_get_expanded(LIVES_EXPANDER(exp))) goto thrdjoin;
        lives_layout_add_row(LIVES_LAYOUT(entryw->layouts_layout));
        txt = lives_datetime(filedets->mtime_sec, TRUE);
        dtxt = lives_datetime_rel(txt);
        lives_layout_add_label(LIVES_LAYOUT(entryw->layouts_layout), dtxt, TRUE);
        if (dtxt != txt) lives_free(dtxt);
        lives_free(txt);
        txt = lives_format_storage_space_string(filedets->size);
        lives_layout_add_label(LIVES_LAYOUT(entryw->layouts_layout), txt, TRUE);
        lives_free(txt);
        lives_layout_add_label(LIVES_LAYOUT(entryw->layouts_layout), filedets->name, TRUE);
        lives_widget_show_all(entryw->expander);
        do {
          lives_widget_process_updates(entryw->dialog);
          if (lives_expander_get_expanded(LIVES_EXPANDER(exp))) {
            if (totsize == -1) {
              if (lives_proc_thread_check_finished(sizinfo)) {
                totsize = lives_proc_thread_join_int64(sizinfo);
                txt = mkszlabel(set, totsize, -1, lcount);
                widget_opts.use_markup = TRUE;
                lives_label_set_text(LIVES_LABEL(entryw->exp_label), txt);
                widget_opts.use_markup = FALSE;
                lives_free(txt);
                lives_widget_process_updates(entryw->dialog);
              }
            }
          }
          if (lives_expander_get_expanded(LIVES_EXPANDER(exp)) && !list->next) lives_nanosleep(1000);
        } while (!list->next && lives_expander_get_expanded(LIVES_EXPANDER(exp)));
        if (!lives_expander_get_expanded(LIVES_EXPANDER(exp))) goto thrdjoin;
        list = list->next;
      }
    }

    if (!lives_expander_get_expanded(LIVES_EXPANDER(exp))) goto thrdjoin;

    do {
      lives_widget_process_updates(entryw->dialog);
      if (lives_expander_get_expanded(LIVES_EXPANDER(exp))) {
        if (totsize == -1) {
          if (lives_proc_thread_check_finished(sizinfo)) {
            totsize = lives_proc_thread_join_int64(sizinfo);
            txt = mkszlabel(set, totsize, lcount, -1);
            widget_opts.use_markup = TRUE;
            lives_label_set_text(LIVES_LABEL(entryw->exp_label), txt);
            widget_opts.use_markup = FALSE;
            lives_free(txt);
            lives_widget_process_updates(entryw->dialog);
          }
        }
      }
      if (!*clipslist && lives_expander_get_expanded(LIVES_EXPANDER(exp))) lives_nanosleep(1000);
    } while (!*clipslist && lives_expander_get_expanded(LIVES_EXPANDER(exp)));

    if (!lives_expander_get_expanded(LIVES_EXPANDER(exp))) goto thrdjoin;

    // clips
    entryw->clips_layout = lives_layout_new(LIVES_BOX(entryw->exp_vbox));
    widget_opts.expand = LIVES_EXPAND_EXTRA_WIDTH | LIVES_EXPAND_DEFAULT_HEIGHT;
    widget_opts.justify = LIVES_JUSTIFY_CENTER;
    lives_layout_add_label(LIVES_LAYOUT(entryw->clips_layout), _("Clips"), FALSE);
    widget_opts.justify = LIVES_JUSTIFY_DEFAULT;
    widget_opts.expand = LIVES_EXPAND_DEFAULT;
    lives_layout_add_fill(LIVES_LAYOUT(entryw->clips_layout), FALSE);

    list = *clipslist;
    if (!list->data) {
      // NONE label
      lives_layout_add_label(LIVES_LAYOUT(entryw->clips_layout), mainw->string_constants[LIVES_STRING_CONSTANT_NONE], FALSE);
    } else {
      int pass = 0;
      widget_opts.apply_theme = 2;
      lives_layout_add_row(LIVES_LAYOUT(entryw->clips_layout));
      lives_layout_add_label(LIVES_LAYOUT(entryw->clips_layout), _("Modified Date"), TRUE);
      lives_layout_add_label(LIVES_LAYOUT(entryw->clips_layout), _("Size"), TRUE);
      lives_layout_add_label(LIVES_LAYOUT(entryw->clips_layout), _("Details"), TRUE);
      widget_opts.apply_theme = woat;

      while (list->data && lives_expander_get_expanded(LIVES_EXPANDER(exp))) {
        boolean needs_more = FALSE;
        filedets = (lives_file_dets_t *)(list->data);

        if (!pass) {
          ccount++;
          lives_layout_add_row(LIVES_LAYOUT(entryw->clips_layout));
          if (!filedets->mtime_sec) dtxt = txt = lives_strdup("????");
          else {
            txt = lives_datetime(filedets->mtime_sec, TRUE);
            dtxt = lives_datetime_rel(txt);
          }
          filedets->widgets[0] = lives_layout_add_label(LIVES_LAYOUT(entryw->clips_layout), dtxt, TRUE);
          if (dtxt != txt) lives_free(dtxt);
          lives_free(txt);
        }

        if (!pass) {
          LiVESWidget *hbox = lives_layout_hbox_new(LIVES_LAYOUT(entryw->clips_layout));
          filedets->widgets[1] = lives_standard_label_new(NULL);
          lives_layout_pack(LIVES_BOX(hbox), filedets->widgets[1]);
          if (filedets->size == -1) {
            lives_widget_hide(filedets->widgets[1]);
            lives_widget_set_no_show_all(filedets->widgets[1], TRUE);
            filedets->widgets[2] = lives_spinner_new();
            if (filedets->widgets[2]) {
              lives_layout_pack(LIVES_BOX(hbox), filedets->widgets[2]);
              lives_spinner_start(LIVES_SPINNER(filedets->widgets[2]));
            } else filedets->widgets[2] = filedets->widgets[1];
          } else filedets->widgets[2] = filedets->widgets[1];
        }

        if (filedets->size == -1) {
          needs_more = TRUE;
        }

        if (filedets->size != -1) {
          if (filedets->mtime_sec) {
            txt = lives_datetime(filedets->mtime_sec, TRUE);
            dtxt = lives_datetime_rel(txt);
            lives_label_set_text(LIVES_LABEL(filedets->widgets[0]), dtxt);
            if (dtxt != txt) lives_free(dtxt);
            lives_free(txt);
          }
          if (filedets->widgets[2]) {
            if (filedets->widgets[2] != filedets->widgets[1]) {
              // remove spinner
              lives_spinner_stop(LIVES_SPINNER(filedets->widgets[2]));
              lives_widget_destroy(filedets->widgets[2]);
            }
            if (filedets->size == -2) {
              lives_label_set_text(LIVES_LABEL(filedets->widgets[1]), "????");
            }
            if (filedets->size > 0) {
              txt = lives_format_storage_space_string(filedets->size);
              lives_label_set_text(LIVES_LABEL(filedets->widgets[1]), txt);
              lives_free(txt);
            }
            lives_widget_set_no_show_all(filedets->widgets[1], FALSE);
            lives_widget_show(filedets->widgets[1]);
            filedets->widgets[2] = NULL;
          }
        }

        if (!pass) {
          LiVESWidget *hbox = lives_layout_hbox_new(LIVES_LAYOUT(entryw->clips_layout));
          filedets->widgets[4] = filedets->widgets[3] = lives_standard_label_new(NULL);
          lives_layout_pack(LIVES_BOX(hbox), filedets->widgets[3]);
          if (!filedets->extra_details) {
            lives_widget_hide(filedets->widgets[3]);
            lives_widget_set_no_show_all(filedets->widgets[3], TRUE);
            filedets->widgets[4] = lives_spinner_new();
            if (filedets->widgets[4]) {
              lives_layout_pack(LIVES_BOX(hbox), filedets->widgets[4]);
              lives_spinner_start(LIVES_SPINNER(filedets->widgets[4]));
            }
          }
        }

        if (!filedets->extra_details) needs_more = TRUE;
        else {
          if (filedets->widgets[4]) {
            if (filedets->widgets[4] != filedets->widgets[3]) {
              // remove spinner
              lives_spinner_stop(LIVES_SPINNER(filedets->widgets[4]));
              lives_widget_destroy(filedets->widgets[4]);
              filedets->widgets[4] = NULL;
            }
          }
          if (filedets->widgets[3]) {
            lives_label_set_line_wrap(LIVES_LABEL(filedets->widgets[3]), TRUE);
            lives_label_set_text(LIVES_LABEL(filedets->widgets[3]), filedets->extra_details);
            lives_widget_set_no_show_all(filedets->widgets[3], FALSE);
            lives_widget_show(filedets->widgets[3]);
            if (LIVES_FILE_IS_MISSING(filedets->type))
              show_warn_image(filedets->widgets[3], NULL);
          }
        }
        lives_widget_show_all(entryw->clips_layout);

        do {
          lives_widget_process_updates(entryw->dialog);
          if (lives_expander_get_expanded(LIVES_EXPANDER(exp)) && !pass && !list->next) lives_nanosleep(1000);
          if (totsize == -1) {
            if (lives_proc_thread_check_finished(sizinfo)) {
              totsize = lives_proc_thread_join_int64(sizinfo);
              txt = mkszlabel(set, totsize, ccount, lcount);
              widget_opts.use_markup = TRUE;
              lives_label_set_text(LIVES_LABEL(entryw->exp_label), txt);
              widget_opts.use_markup = FALSE;
              lives_free(txt);
              lives_widget_process_updates(entryw->dialog);
            }
          }
        } while (!pass && !list->next && lives_expander_get_expanded(LIVES_EXPANDER(exp)));

        if (!lives_expander_get_expanded(LIVES_EXPANDER(exp))) goto thrdjoin;
        list = list->next;
        if (!list->data && needs_more) {
          list = *clipslist;
          pass++;
        }
      }
    }
    while (lives_expander_get_expanded(LIVES_EXPANDER(exp)) && totsize == -1) {
      if (lives_proc_thread_check_finished(sizinfo)) {
        totsize = lives_proc_thread_join_int64(sizinfo);
      }
      if (lives_expander_get_expanded(LIVES_EXPANDER(exp))) lives_widget_process_updates(entryw->dialog);
      if (lives_expander_get_expanded(LIVES_EXPANDER(exp))) lives_nanosleep(1000);
    }
    txt = mkszlabel(set, totsize, ccount, lcount);
    widget_opts.use_markup = TRUE;
    lives_label_set_text(LIVES_LABEL(entryw->exp_label), txt);
    widget_opts.use_markup = FALSE;
    lives_free(txt);
    if (lives_expander_get_expanded(LIVES_EXPANDER(exp))) lives_widget_process_updates(entryw->dialog);
  }

thrdjoin:
  lives_proc_thread_cancel(layinfo, TRUE);
  lives_proc_thread_cancel(clipsinfo, TRUE);
  lives_proc_thread_cancel(sizinfo, TRUE);
  if (*laylist) free_fdets_list(laylist);
  if (*clipslist) free_fdets_list(clipslist);
}


static void close_expander(LiVESWidget * button, _entryw * entryw) {
  if (lives_expander_get_expanded(LIVES_EXPANDER(entryw->expander)))
    lives_expander_set_expanded(LIVES_EXPANDER(entryw->expander), FALSE);
}

static void entryw_entry_changed(LiVESEntry * entry, LiVESWidget * other) {
  if (!*(lives_entry_get_text(entry))) lives_widget_set_sensitive(other, FALSE);
  else lives_widget_set_sensitive(other, TRUE);
}


_entryw *create_entry_dialog(int type) {
  LiVESWidget *dialog_vbox, *hbox, *vbox;
  LiVESWidget *logo = NULL;
  LiVESWidget *label;
  LiVESWidget *checkbutton;
  LiVESWidget *set_combo;

  char *title = NULL, *workdir, *tmp, *tmp2;

  _entryw *entryw = (_entryw *)(lives_calloc(1, sizeof(_entryw)));

  if (type == ENTRYW_CLIP_RENAME) {
    title = (_("Rename Clip"));
  } else if (type == ENTRYW_SAVE_SET || type == ENTRYW_SAVE_SET_MT
             || type == ENTRYW_SAVE_SET_PROJ_EXPORT) {
    title = (_("Enter Set Name to Save as"));
  } else if (type == ENTRYW_RELOAD_SET) {
    title = (_("Enter a Set Name to Reload"));
  } else if (type == ENTRYW_INIT_WORKDIR) {
    title = (_("Choose a Working Directory"));
  } else if (type == ENTRYW_TRACKNAME_MT) {
    title = (_("Rename Current Track"));
  } else if (type == ENTRYW_EXPORT_THEME) {
    title = (_("Enter a Name for Your Theme"));
  }

  entryw->dialog = lives_standard_dialog_new(title, FALSE, -1, -1);
  lives_free(title);

  lives_signal_handlers_disconnect_by_func(entryw->dialog, LIVES_GUI_CALLBACK(return_true), NULL);

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(entryw->dialog));

  if (type == ENTRYW_SAVE_SET_MT) {
    label = lives_standard_label_new
            (_("You need to enter a name for the current clip set.\n"
               "This will allow you reload the layout with the same clips later.\n"
               "Please enter the set name you wish to use.\n"
               "LiVES will remind you to save the clip set later when you try to exit.\n"));
    lives_box_pack_start(LIVES_BOX(dialog_vbox), label, FALSE, FALSE, 0);
  }

  if (type == ENTRYW_SAVE_SET_PROJ_EXPORT) {
    label = lives_standard_label_new
            (_("In order to export this project, you must enter a name for this clip set.\n"
               "This will also be used for the project name.\n"));
    lives_box_pack_start(LIVES_BOX(dialog_vbox), label, FALSE, FALSE, 0);
  }

  if (type == ENTRYW_INIT_WORKDIR && !mainw->is_ready) {
    tmp = lives_big_and_bold(_("Welcome to LiVES !"));
    widget_opts.justify = LIVES_JUSTIFY_CENTER;
    widget_opts.use_markup = TRUE;
    label = lives_standard_label_new(tmp);
    widget_opts.use_markup = FALSE;
    widget_opts.justify = LIVES_JUSTIFY_DEFAULT;
    lives_free(tmp);
    lives_box_pack_start(LIVES_BOX(dialog_vbox), label, FALSE, FALSE, widget_opts.packing_height);

    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

    vbox = lives_vbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(hbox), vbox, FALSE, FALSE, widget_opts.packing_width);

    logo = lives_image_new_from_stock_at_size("livestock-lives", LIVES_ICON_SIZE_CUSTOM, 128);
    lives_box_pack_start(LIVES_BOX(hbox), logo, TRUE, TRUE, widget_opts.packing_width);
    lives_widget_set_valign(logo, LIVES_ALIGN_START);

    set_css_value_direct(logo, LIVES_WIDGET_STATE_NORMAL, "", "opacity", "0.5");

    entryw->ylabel = lives_standard_label_new
                     (_("This startup wizard will guide you through the\n"
                        "initial setup so that you can get the most from this application."));
    lives_box_pack_start(LIVES_BOX(vbox), entryw->ylabel, FALSE, FALSE, widget_opts.packing_height);

    widget_opts.use_markup = TRUE;
    entryw->xlabel = lives_standard_label_new
                     (_("First of all you need to <b>choose a working directory</b> for LiVES.\n"
                        "This should be a directory with plenty of disk space available."));
    widget_opts.use_markup = FALSE;
    lives_box_pack_end(LIVES_BOX(vbox), entryw->xlabel, FALSE, FALSE, widget_opts.packing_height);
  }

  if (type == ENTRYW_INIT_WORKDIR && mainw->is_ready) {
    label = lives_standard_label_new(_("If the value of the working directory is changed, you can choose whether \n"
                                       "the contents of the current"
                                       "directory should be left, deleted, or moved \n- and if applicable, they may be combined \n"
                                       "with an existing LiVES directory\n"));
    lives_box_pack_start(LIVES_BOX(dialog_vbox), label, FALSE, FALSE, widget_opts.packing_height);
  }

  hbox = lives_hbox_new(FALSE, 0);

  if (type == ENTRYW_RELOAD_SET) {
    lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, TRUE, FALSE, widget_opts.packing_height * 4);
  } else if (type == ENTRYW_SAVE_SET || type == ENTRYW_SAVE_SET_MT
             || type == ENTRYW_SAVE_SET_PROJ_EXPORT) {
    lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, TRUE, TRUE, widget_opts.packing_height * 2);
  } else {
    lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, TRUE, TRUE, widget_opts.packing_height * 4);
  }

  if (type == ENTRYW_CLIP_RENAME || type == ENTRYW_TRACKNAME_MT) {
    label = lives_standard_label_new(_("New name "));
  } else if (type == ENTRYW_SAVE_SET || type == ENTRYW_RELOAD_SET
             || type == ENTRYW_SAVE_SET_MT || type == ENTRYW_SAVE_SET_PROJ_EXPORT) {
    label = lives_standard_label_new(_("Set name "));
  } else if (type == ENTRYW_EXPORT_THEME) {
    label = lives_standard_label_new(_("Theme name "));
  } else {
    label = lives_standard_label_new("");
  }

  lives_box_pack_start(LIVES_BOX(hbox), label, FALSE, FALSE, widget_opts.packing_width * 4);

  if (type == ENTRYW_RELOAD_SET) {
    if (mainw->num_sets == -1) {
      mainw->set_list = get_set_list(prefs->workdir, TRUE);
      if (mainw->set_list) {
        mainw->num_sets = lives_list_length(mainw->set_list);
        if (mainw->was_set) mainw->num_sets--;
      } else mainw->num_sets = 0;
      if (!mainw->num_sets) {
        if (!mainw->was_set) do_no_sets_dialog(prefs->workdir);
        return NULL;
      }
    }
    mainw->set_list = lives_list_sort_alpha(mainw->set_list, TRUE);
    set_combo = lives_standard_combo_new(NULL, mainw->set_list, LIVES_BOX(hbox), NULL);
    entryw->entry = lives_combo_get_entry(LIVES_COMBO(set_combo));
    lives_entry_set_editable(LIVES_ENTRY(entryw->entry), TRUE);
    lives_entry_set_max_length(LIVES_ENTRY(entryw->entry), MAX_SET_NAME_LEN);

    if (*prefs->ar_clipset_name) {
      // set default to our auto-reload clipset
      lives_entry_set_text(LIVES_ENTRY(entryw->entry), prefs->ar_clipset_name);
    }
    lives_entry_set_completion_from_list(LIVES_ENTRY(entryw->entry), mainw->set_list);
  } else {
    if (type == ENTRYW_INIT_WORKDIR) {
      if (*prefs->workdir) workdir = lives_strdup(prefs->workdir);
      else workdir = lives_build_path(capable->home_dir, LIVES_DEF_WORK_SUBDIR, NULL);
      entryw->entry = lives_standard_direntry_new("", (tmp = F2U8(workdir)),
                      LONG_ENTRY_WIDTH, PATH_MAX, LIVES_BOX(hbox),
                      (tmp2 = (_("LiVES working directory."))));

      entryw->dirbutton = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(entryw->entry), BUTTON_KEY);

      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(entryw->dirbutton), FILESEL_TYPE_KEY,
                                   LIVES_INT_TO_POINTER(LIVES_DIR_SELECTION_WORKDIR));

      lives_free(tmp);
      lives_free(workdir);
    } else {
      entryw->entry = lives_standard_entry_new(NULL, NULL, -1, -1, LIVES_BOX(hbox), NULL);
      lives_entry_set_max_length(LIVES_ENTRY(entryw->entry), type == ENTRYW_INIT_WORKDIR ? PATH_MAX
                                 : type == ENTRYW_TRACKNAME_MT ? 16 : 128);
      if (type == ENTRYW_SAVE_SET && *mainw->set_name) {
        lives_entry_set_text(LIVES_ENTRY(entryw->entry), (tmp = F2U8(mainw->set_name)));
        lives_free(tmp);
      }
    }
  }

  add_fill_to_box(LIVES_BOX(dialog_vbox));

  if (type == ENTRYW_RELOAD_SET) {
    /// add set details expander
    int winsize_h = RFX_WINSIZE_H;
    int winsize_v = RFX_WINSIZE_V / 2;
    LiVESWidget *layout;
    LiVESWidget *scrolledwindow;
    LiVESWidget *vbox = lives_vbox_new(FALSE, 0);
    int woat = widget_opts.apply_theme;

    if (GUI_SCREEN_WIDTH >= SCREEN_SCALE_DEF_WIDTH) {
      winsize_h *= 2;
    }

    layout = lives_layout_new(LIVES_BOX(vbox));
    lives_layout_add_fill(LIVES_LAYOUT(layout), FALSE);
    entryw->exp_label = lives_layout_add_label(LIVES_LAYOUT(layout), "", FALSE);
    lives_layout_add_fill(LIVES_LAYOUT(layout), FALSE);

    entryw->exp_vbox = lives_vbox_new(FALSE, 0);
    widget_opts.apply_theme = 0;
    scrolledwindow = lives_standard_scrolled_window_new(winsize_h, winsize_v, entryw->exp_vbox);
    widget_opts.apply_theme = woat;
    lives_box_pack_start(LIVES_BOX(vbox), scrolledwindow, TRUE, TRUE, widget_opts.packing_height);
    widget_opts.justify = LIVES_JUSTIFY_CENTER;
    entryw->expander = lives_standard_expander_new(_("<b>Show Contents</b>"), _("<b>Hide Contents</b>"),
                       LIVES_BOX(dialog_vbox), vbox);
    widget_opts.justify = LIVES_JUSTIFY_DEFAULT;
    lives_signal_sync_connect_after(LIVES_GUI_OBJECT(entryw->expander), LIVES_WIDGET_ACTIVATE_SIGNAL,
                                    LIVES_GUI_CALLBACK(on_set_exp), entryw);
    on_set_exp(entryw->expander, entryw);
    add_fill_to_box(LIVES_BOX(dialog_vbox));
    add_fill_to_box(LIVES_BOX(dialog_vbox));
    lives_signal_sync_connect_after(LIVES_GUI_OBJECT(entryw->entry), LIVES_WIDGET_CHANGED_SIGNAL,
                                    LIVES_GUI_CALLBACK(entryw_entry_changed), entryw->expander);
    entryw_entry_changed(LIVES_ENTRY(entryw->entry), entryw->expander);
  }

  if (type == ENTRYW_EXPORT_THEME) {
    mainw->fx1_bool = FALSE;
    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, TRUE, TRUE, widget_opts.packing_height * 4);

    checkbutton = lives_standard_check_button_new(_("Save extended colors"), FALSE, LIVES_BOX(hbox), NULL);

    lives_signal_sync_connect_after(LIVES_GUI_OBJECT(checkbutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                                    LIVES_GUI_CALLBACK(on_boolean_toggled), &mainw->fx1_bool);
  }

  lives_entry_set_width_chars(LIVES_ENTRY(entryw->entry), MEDIUM_ENTRY_WIDTH);

  if (!(type == ENTRYW_SAVE_SET_MT && !LIVES_IS_INTERACTIVE)) {
    if (type == ENTRYW_INIT_WORKDIR && !mainw->is_ready)
      entryw->cancelbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(entryw->dialog), LIVES_STOCK_CANCEL,
                             _("Quit from Setup"),
                             LIVES_RESPONSE_CANCEL);
    else
      entryw->cancelbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(entryw->dialog), LIVES_STOCK_CANCEL, NULL,
                             LIVES_RESPONSE_CANCEL);
    lives_window_add_escape(LIVES_WINDOW(entryw->dialog), entryw->cancelbutton);
  }

  if (type == ENTRYW_INIT_WORKDIR && !mainw->is_ready) {
    entryw->okbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(entryw->dialog), LIVES_STOCK_GO_FORWARD, _("_Next"),
                       LIVES_RESPONSE_OK);
  } else entryw->okbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(entryw->dialog), LIVES_STOCK_OK,
                              NULL, LIVES_RESPONSE_OK);

  lives_button_grab_default_special(entryw->okbutton);

  if (type != 3 && type != 6 && entryw->cancelbutton) {
    lives_signal_sync_connect(LIVES_GUI_OBJECT(entryw->cancelbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                              LIVES_GUI_CALLBACK(lives_general_button_clicked), entryw);
  }

  if (type == ENTRYW_RELOAD_SET) {
    if (entryw->cancelbutton) {
      lives_signal_sync_connect(LIVES_GUI_OBJECT(entryw->cancelbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                                LIVES_GUI_CALLBACK(close_expander), entryw);
    }
    if (entryw->okbutton) {
      lives_signal_sync_connect(LIVES_GUI_OBJECT(entryw->okbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                                LIVES_GUI_CALLBACK(close_expander), entryw);
    }
  }

  if (type == ENTRYW_CLIP_RENAME) {
    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(entryw->okbutton), STRUCT_KEY, entryw);
    lives_signal_sync_connect(LIVES_GUI_OBJECT(entryw->okbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                              LIVES_GUI_CALLBACK(on_rename_clip_name), NULL);
  }

  lives_widget_show_all(entryw->dialog);
  lives_widget_grab_focus(entryw->entry);

  if (type == ENTRYW_INIT_WORKDIR) {
    LiVESWidget *bbox = lives_dialog_get_action_area(LIVES_DIALOG(entryw->dialog));
    lives_button_box_set_layout(LIVES_BUTTON_BOX(bbox), LIVES_BUTTONBOX_EDGE);
    if (logo) lives_widget_set_opacity(logo, .7);
  }

  return entryw;
}


void on_liveinp_advanced_clicked(LiVESButton * button, livespointer user_data) {
  lives_tvcardw_t *tvcardw = (lives_tvcardw_t *)(user_data);

  tvcardw->use_advanced = !tvcardw->use_advanced;

  if (tvcardw->use_advanced) {
    lives_widget_show(tvcardw->adv_vbox);
    lives_button_set_label(LIVES_BUTTON(tvcardw->advbutton), _("Use def_aults"));
  } else {
    lives_button_set_label(LIVES_BUTTON(tvcardw->advbutton), _("_Advanced"));
    lives_window_resize(LIVES_WINDOW(lives_widget_get_toplevel(tvcardw->adv_vbox)), 4, 40);
    lives_widget_hide(tvcardw->adv_vbox);
  }

  lives_widget_queue_resize(lives_widget_get_parent(tvcardw->adv_vbox));
}


static void rb_tvcarddef_toggled(LiVESToggleButton * tbut, livespointer user_data) {
  lives_tvcardw_t *tvcardw = (lives_tvcardw_t *)(user_data);

  if (!lives_toggle_button_get_active(tbut)) {
    lives_widget_set_sensitive(tvcardw->spinbuttonw, TRUE);
    lives_widget_set_sensitive(tvcardw->spinbuttonh, TRUE);
    lives_widget_set_sensitive(tvcardw->spinbuttonf, TRUE);
  } else {
    lives_widget_set_sensitive(tvcardw->spinbuttonw, FALSE);
    lives_widget_set_sensitive(tvcardw->spinbuttonh, FALSE);
    lives_widget_set_sensitive(tvcardw->spinbuttonf, FALSE);
  }
}


static void after_dialog_combo_changed(LiVESWidget * combo, livespointer plist) {
  // set mainw->fx1_val to the index of combo text in plist
  LiVESList *list = (LiVESList *)plist;
  const char *etext = lives_combo_get_active_text(LIVES_COMBO(combo));
  mainw->fx1_val = lives_list_strcmp_index(list, etext, TRUE);
}


LiVESWidget *create_combo_dialog(int type, LiVESList * list) {
  // create a dialog with combo box selector

  // type 1 == unicap device

  // afterwards, mainw->fx1_val points to index selected

  LiVESWidget *combo_dialog;
  LiVESWidget *dialog_vbox;
  LiVESWidget *label;
  LiVESWidget *combo;

  char *label_text = NULL, *title = NULL;

  if (type == 1) {
    title = (_("Select input device"));
  }

  combo_dialog = lives_standard_dialog_new(title, TRUE, -1, -1);
  if (title) lives_free(title);

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(combo_dialog));

  if (type == 1) {
    label_text = (_("Select input device:"));
  }

  if (label_text) {
    label = lives_standard_label_new(label_text);
    lives_free(label_text);
    lives_box_pack_start(LIVES_BOX(dialog_vbox), label, TRUE, TRUE, 0);
  }

  widget_opts.packing_height <<= 1;
  combo = lives_standard_combo_new(NULL, list, LIVES_BOX(dialog_vbox), NULL);
  widget_opts.packing_height >>= 1;

  lives_signal_sync_connect_after(LIVES_WIDGET_OBJECT(combo), LIVES_WIDGET_CHANGED_SIGNAL,
                                  LIVES_GUI_CALLBACK(after_dialog_combo_changed), list);
  return combo_dialog;
}


LiVESWidget *create_cdtrack_dialog(int type, livespointer user_data) {
  // general purpose device dialog with label and up to 2 spinbuttons

  // type 0 = cd track
  // type 1 = dvd title/chapter/aid
  // type 2 = vcd title -- do we need chapter as well ?

  // type 3 = number of tracks in mt

  // type 4 = TV card (device and channel)
  // type 5 = fw card

  // TODO - for CD make this nicer - get track names

  lives_tvcardw_t *tvcardw = NULL;

  LiVESWidget *cd_dialog;
  LiVESWidget *dialog_vbox;
  LiVESWidget *hbox;
  LiVESWidget *spinbutton;
  LiVESWidget *cancelbutton;
  LiVESWidget *okbutton;

  LiVESSList *radiobutton_group = NULL;

  char *label_text = NULL, *title;

  int ph_mult = 4;

  if (type == LIVES_DEVICE_CD) {
    title = (_("Load CD Track"));
  } else if (type == LIVES_DEVICE_DVD) {
    title = (_("Select DVD Title/Chapter"));
  } else if (type == LIVES_DEVICE_VCD) {
    title = (_("Select VCD Title"));
  } else if (type == LIVES_DEVICE_INTERNAL) {
    title = (_("Change Maximum Visible Tracks"));
  } else {
    title = (_("Device details"));
  }

  cd_dialog = lives_standard_dialog_new(title, FALSE, -1, -1);
  lives_free(title);
  lives_signal_handlers_disconnect_by_func(cd_dialog, LIVES_GUI_CALLBACK(return_true), NULL);

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(cd_dialog));

  if (type == LIVES_DEVICE_DVD || type == LIVES_DEVICE_TV_CARD) ph_mult = 2;

  hbox = lives_hbox_new(FALSE, widget_opts.packing_width * 5);
  lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, TRUE, TRUE, widget_opts.packing_height * ph_mult);

  if (type == LIVES_DEVICE_CD) {
    label_text = lives_strdup_printf(_("Track to load (from %s)"), prefs->cdplay_device);
  } else if (type == LIVES_DEVICE_DVD) {
    label_text = (_("DVD Title"));
  } else if (type == LIVES_DEVICE_VCD) {
    label_text = (_("VCD Title"));
  } else if (type == LIVES_DEVICE_INTERNAL) {
    label_text = (_("Maximum number of tracks to display"));
  } else if (type == LIVES_DEVICE_TV_CARD) {
    label_text = (_("Device:        /dev/video"));
  } else if (type == LIVES_DEVICE_FW_CARD) {
    label_text = (_("Device:        fw:"));
  }

  widget_opts.mnemonic_label = FALSE;
  if (type == LIVES_DEVICE_CD || type == LIVES_DEVICE_DVD || type == LIVES_DEVICE_VCD) {
    spinbutton = lives_standard_spin_button_new(label_text, mainw->fx1_val,
                 1., 256., 1., 10., 0, LIVES_BOX(hbox), NULL);
  } else if (type == LIVES_DEVICE_INTERNAL) {
    spinbutton = lives_standard_spin_button_new(label_text, mainw->fx1_val,
                 5., 15., 1., 1., 0, LIVES_BOX(hbox), NULL);
  } else {
    spinbutton = lives_standard_spin_button_new(label_text, 0.,
                 0., 31., 1., 1., 0, LIVES_BOX(hbox), NULL);
  }
  widget_opts.mnemonic_label = TRUE;

  lives_free(label_text);

  lives_signal_sync_connect_after(LIVES_GUI_OBJECT(spinbutton), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                                  LIVES_GUI_CALLBACK(on_spin_value_changed),
                                  LIVES_INT_TO_POINTER(1));

  add_fill_to_box(LIVES_BOX(hbox));

  if (type == LIVES_DEVICE_DVD || type == LIVES_DEVICE_TV_CARD) {
    hbox = lives_hbox_new(FALSE, widget_opts.packing_width * 5);
    lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, TRUE, TRUE, widget_opts.packing_height * ph_mult);

    if (type == LIVES_DEVICE_DVD) {
      spinbutton = lives_standard_spin_button_new(_("Chapter  "), mainw->fx2_val,
                   1., 1024., 1., 10., 0, LIVES_BOX(hbox), NULL);
    } else {
      spinbutton = lives_standard_spin_button_new(_("Channel  "), 1.,
                   1., 69., 1., 1., 0, LIVES_BOX(hbox), NULL);
    }

    lives_signal_sync_connect_after(LIVES_GUI_OBJECT(spinbutton), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                                    LIVES_GUI_CALLBACK(on_spin_value_changed), LIVES_INT_TO_POINTER(2));

    if (type == LIVES_DEVICE_DVD) {
      hbox = lives_hbox_new(FALSE, widget_opts.packing_width * 5);
      lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, TRUE, TRUE, widget_opts.packing_height * ph_mult);

      spinbutton = lives_standard_spin_button_new(_("Audio ID  "), mainw->fx3_val,
                   DVD_AUDIO_CHAN_MIN, DVD_AUDIO_CHAN_MAX, 1., 1., 0, LIVES_BOX(hbox), NULL);

      lives_signal_sync_connect_after(LIVES_GUI_OBJECT(spinbutton), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                                      LIVES_GUI_CALLBACK(on_spin_value_changed),
                                      LIVES_INT_TO_POINTER(3));
    }
  }

  if (type == LIVES_DEVICE_TV_CARD || type == LIVES_DEVICE_FW_CARD) {
    hbox = add_deinterlace_checkbox(LIVES_BOX(dialog_vbox));
    add_fill_to_box(LIVES_BOX(hbox));
  }

  if (type == LIVES_DEVICE_TV_CARD) {
    LiVESList *dlist = NULL;
    LiVESList *olist = NULL;
    char const *str;
    char *tvcardtypes[] = LIVES_TV_CARD_TYPES;
    register int i;

    tvcardw = (lives_tvcardw_t *)lives_malloc(sizeof(lives_tvcardw_t));
    tvcardw->use_advanced = FALSE;

    for (i = 0; (str = tvcardtypes[i]); i++) {
      dlist = lives_list_append(dlist, (livespointer)tvcardtypes[i]);
    }

    lives_box_set_spacing(LIVES_BOX(dialog_vbox), widget_opts.packing_height * 2);

    hbox = lives_hbox_new(FALSE, widget_opts.packing_width * 5);

    lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, FALSE, FALSE, widget_opts.packing_height * 2);

    add_fill_to_box(LIVES_BOX(hbox));

    tvcardw->advbutton = lives_standard_button_new_from_stock(LIVES_STOCK_PREFERENCES, _("_Advanced"),
                         DEF_BUTTON_WIDTH, DEF_BUTTON_HEIGHT);

    lives_box_pack_start(LIVES_BOX(hbox), tvcardw->advbutton, TRUE, TRUE, widget_opts.packing_width * 4);

    add_fill_to_box(LIVES_BOX(hbox));

    tvcardw->adv_vbox = lives_vbox_new(FALSE, widget_opts.packing_width * 5);
    lives_box_pack_start(LIVES_BOX(dialog_vbox), tvcardw->adv_vbox, TRUE, TRUE, widget_opts.packing_height * 2);

    // add input, width, height, fps, driver and outfmt

    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(tvcardw->adv_vbox), hbox, TRUE, FALSE, 0);

    tvcardw->spinbuttoni = lives_standard_spin_button_new(_("Input number"),
                           0., 0., 16., 1., 1., 0, LIVES_BOX(hbox), NULL);

    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(tvcardw->adv_vbox), hbox, TRUE, FALSE, 0);

    tvcardw->radiobuttond = lives_standard_radio_button_new(_("Use default width, height and FPS"),
                            &radiobutton_group, LIVES_BOX(hbox), NULL);

    lives_signal_sync_connect_after(LIVES_GUI_OBJECT(tvcardw->radiobuttond), LIVES_WIDGET_TOGGLED_SIGNAL,
                                    LIVES_GUI_CALLBACK(rb_tvcarddef_toggled), (livespointer)tvcardw);

    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(tvcardw->adv_vbox), hbox, TRUE, FALSE, 0);

    lives_standard_radio_button_new(NULL, &radiobutton_group, LIVES_BOX(hbox), NULL);

    tvcardw->spinbuttonw = lives_standard_spin_button_new(_("Width"),
                           640., 4., 4096., -4., 16., 0, LIVES_BOX(hbox), NULL);

    lives_widget_set_sensitive(tvcardw->spinbuttonw, FALSE);

    tvcardw->spinbuttonh = lives_standard_spin_button_new(_("Height"),
                           480., 4., 4096., -4., 16., 0, LIVES_BOX(hbox), NULL);

    lives_widget_set_sensitive(tvcardw->spinbuttonh, FALSE);

    tvcardw->spinbuttonf = lives_standard_spin_button_new(_("FPS"),
                           25., 1., FPS_MAX, 1., 10., 3, LIVES_BOX(hbox), NULL);

    lives_widget_set_sensitive(tvcardw->spinbuttonf, FALSE);

    hbox = lives_hbox_new(FALSE, 0);

    tvcardw->combod = lives_standard_combo_new(_("_Driver"), dlist, LIVES_BOX(hbox), NULL);
    lives_combo_set_active_index(LIVES_COMBO(tvcardw->combod), 0);

    tvcardw->comboo = lives_standard_combo_new(_("_Output format"), olist, LIVES_BOX(hbox), NULL);

    lives_widget_show_all(hbox);
    lives_box_pack_start(LIVES_BOX(tvcardw->adv_vbox), hbox, TRUE, FALSE, 0);

    lives_signal_sync_connect(LIVES_GUI_OBJECT(tvcardw->advbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                              LIVES_GUI_CALLBACK(on_liveinp_advanced_clicked), tvcardw);

    lives_widget_hide(tvcardw->adv_vbox);

    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(cd_dialog), "tvcard_data", tvcardw);
  }

  add_fill_to_box(LIVES_BOX(dialog_vbox));

  cancelbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(cd_dialog), LIVES_STOCK_CANCEL, NULL,
                 LIVES_RESPONSE_CANCEL);
  okbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(cd_dialog), LIVES_STOCK_OK, NULL,
             LIVES_RESPONSE_OK);
  lives_button_grab_default_special(okbutton);

  lives_window_add_escape(LIVES_WINDOW(cd_dialog), cancelbutton);

  if (type != LIVES_DEVICE_TV_CARD && type != LIVES_DEVICE_FW_CARD) {
    lives_signal_sync_connect(LIVES_GUI_OBJECT(cancelbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                              LIVES_GUI_CALLBACK(lives_general_button_clicked), NULL);
  }

  if (type == LIVES_DEVICE_CD) {
    lives_signal_connect(LIVES_GUI_OBJECT(okbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                         LIVES_GUI_CALLBACK(on_load_cdtrack_ok_clicked), NULL);
  } else if (type == LIVES_DEVICE_DVD || type == LIVES_DEVICE_VCD)  {
    lives_signal_connect(LIVES_GUI_OBJECT(okbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                         LIVES_GUI_CALLBACK(on_load_vcd_ok_clicked), LIVES_INT_TO_POINTER(type));
  } else if (type == LIVES_DEVICE_INTERNAL)  {
    lives_signal_connect(LIVES_GUI_OBJECT(okbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                         LIVES_GUI_CALLBACK(mt_change_disp_tracks_ok), user_data);
  }

  lives_widget_show_all(cd_dialog);

  if (type == LIVES_DEVICE_TV_CARD) lives_widget_hide(tvcardw->adv_vbox);

  return cd_dialog;
}


static void on_avolch_ok(LiVESButton * button, livespointer data) {
  if (fabs(cfile->vol - mainw->fx1_val) > .005) {
    uint32_t chk_mask = WARN_MASK_LAYOUT_ALTER_AUDIO;
    char *tmp = (_("Changing the audio volume"));
    lives_general_button_clicked(button, NULL);
    if (check_for_layout_errors(tmp, mainw->current_file, 1,
                                calc_frame_from_time4(mainw->current_file, CLIP_AUDIO_TIME(mainw->current_file)), &chk_mask)) {
      d_print(_("Adjusting clip volume..."));
      lives_set_cursor_style(LIVES_CURSOR_BUSY, NULL);
      if (!adjust_clip_volume(mainw->current_file, lives_vol_from_linear(mainw->fx1_val), !prefs->conserve_space)) {
        d_print_failed();
        unbuffer_lmap_errors(FALSE);
        lives_set_cursor_style(LIVES_CURSOR_NORMAL, NULL);
        return;
      }
      set_undoable(_("Volume Adjustment"), TRUE);
      cfile->undo_action = UNDO_AUDIO_VOL;
      update_timer_bars(0, 0, 0, 0, 2);
      update_timer_bars(0, 0, 0, 0, 3);
      d_print(_("clip volume adjusted by a factor of %.2f\n"), cfile->vol);
      cfile->vol = 1.;
    } else d_print_cancelled();
    lives_free(tmp);
    lives_set_cursor_style(LIVES_CURSOR_NORMAL, NULL);
  }
}


// this is a little tricky to handle - we want to redraw the timeline in a bg thread, however
// GUI updates need to be done in the main thread

// so: if main thread calls this, we kick off a bg thread to rerun this and then that thread will
// push the GUI updates back

// if a bg thread calls this, and there is no current fg_service being run, then continue
// if an fg_service is being run, we cannot stack calls, so we must defer it till the fg_service finishes

// if there is a bg thread running this then we first cancel it, so it should be cancelled on return here

void redraw_timeline(int clipno) {
  lives_clip_t *sfile;

  if (mainw->ce_thumbs) return;
  if (!IS_VALID_CLIP(clipno)) return;
  sfile = mainw->files[clipno];
  if (sfile->clip_type == CLIP_TYPE_TEMP) return;

  if (LIVES_IS_PLAYING && (mainw->fs || mainw->faded)) return;

  if (mainw->drawtl_thread && THREADVAR(tinfo) == mainw->drawtl_thread) {
    // check if this is the thread that was assigned to run this
    if (lives_proc_thread_get_cancelled(mainw->drawtl_thread)) return;
    lives_proc_thread_set_cancellable(mainw->drawtl_thread);
    if (lives_proc_thread_get_cancelled(mainw->drawtl_thread)) return;
  } else {
    if (is_fg_thread()) {
      // if this the fg thread, kick off a bg thread to actually run this
      lives_mutex_lock_carefully(&mainw->tlthread_mutex);
      if (mainw->drawtl_thread) {
        if (!lives_proc_thread_check_finished(mainw->drawtl_thread)) {
          lives_proc_thread_cancel(mainw->drawtl_thread, FALSE);
        }
        pthread_mutex_unlock(&mainw->tlthread_mutex);
        lives_nanosleep_until_zero(mainw->drawtl_thread);
      } else pthread_mutex_unlock(&mainw->tlthread_mutex);
      if (mainw->multitrack || mainw->reconfig) return;
      lives_mutex_lock_carefully(&mainw->tlthread_mutex);
      if (!mainw->drawtl_thread) {
        mainw->drawtl_thread = lives_proc_thread_create(LIVES_THRDATTR_WAIT_SYNC,
                               (lives_funcptr_t)redraw_timeline, -1,
                               "i", clipno);
        lives_proc_thread_sync_ready(mainw->drawtl_thread);
        lives_nanosleep_while_false(lives_proc_thread_get_cancellable(mainw->drawtl_thread));
	lives_proc_thread_dontcare_nullify(mainw->drawtl_thread, (void **)&mainw->drawtl_thread);
      }
      pthread_mutex_unlock(&mainw->tlthread_mutex);
      return;
    } else {
      // if a bg thread, we either call the main thread to run this which will spawn another bg thread,
      // or if we are running it adds to deferral hooks
      pthread_mutex_lock(&mainw->tlthread_mutex);
      if (mainw->drawtl_thread) {
        if (!lives_proc_thread_check_finished(mainw->drawtl_thread)) {
          lives_proc_thread_cancel(mainw->drawtl_thread, FALSE);
        }
      }
      pthread_mutex_unlock(&mainw->tlthread_mutex);

      THREADVAR(hook_flag_hints) = HOOK_UNIQUE_REPLACE_OR_ADD;
      main_thread_execute((lives_funcptr_t)redraw_timeline, 0, NULL, "i", clipno);
      THREADVAR(hook_flag_hints) = 0;

      return;
    }
  }

  mainw->drawsrc = clipno;

  if (!mainw->video_drawable) {
    mainw->video_drawable = lives_widget_create_painter_surface(mainw->video_draw);
  }
  if (!update_timer_bars(0, 0, 0, 0, 1)) return;

  if (!sfile->laudio_drawable) {
    sfile->laudio_drawable = lives_widget_create_painter_surface(mainw->laudio_draw);
    mainw->laudio_drawable = sfile->laudio_drawable;
    clear_tbar_bgs(0, 0, 0, 0, 2);
    if (!update_timer_bars(0, 0, 0, 0, 2)) return;
  } else {
    mainw->laudio_drawable = sfile->laudio_drawable;
    if (1 || !LIVES_IS_PLAYING) {
      if (!update_timer_bars(0, 0, 0, 0, 2)) return;
    }
  }
  if (!sfile->raudio_drawable) {
    sfile->raudio_drawable = lives_widget_create_painter_surface(mainw->raudio_draw);
    mainw->raudio_drawable = sfile->raudio_drawable;
    clear_tbar_bgs(0, 0, 0, 0, 3);
    if (!update_timer_bars(0, 0, 0, 0, 3)) return;
  } else {
    mainw->raudio_drawable = sfile->raudio_drawable;
    if (1 || !LIVES_IS_PLAYING) {
      if (!update_timer_bars(0, 0, 0, 0, 3)) return;
    }
  }

  lives_widget_queue_draw(mainw->video_draw);
  lives_widget_queue_draw(mainw->laudio_draw);
  lives_widget_queue_draw(mainw->raudio_draw);
  lives_widget_queue_draw(mainw->eventbox2);
}


//static void preview_aud_vol_cb(LiVESButton *button, livespointer data) {preview_aud_vol();}

void create_new_pb_speed(short type) {
  // type 1 = change speed
  // type 2 = resample
  // type 3 = clip audio volume

  LiVESWidget *new_pb_speed;
  LiVESWidget *dialog_vbox;
  LiVESWidget *vbox;
  LiVESWidget *hbox;
  LiVESWidget *ca_hbox;
  LiVESWidget *label;
  LiVESWidget *label2;
  LiVESWidget *radiobutton1 = NULL;
  LiVESWidget *radiobutton2 = NULL;
  LiVESWidget *spinbutton_pb_speed;
  LiVESWidget *spinbutton_pb_time = NULL;
  LiVESWidget *cancelbutton;
  LiVESWidget *change_pb_ok;
  LiVESWidget *change_audio_speed = NULL;
  LiVESWidget *tmpoonly = NULL;

  LiVESSList *rbgroup = NULL;

  char label_text[256];

  char *title = NULL;

  if (type == 1) {
    title = (_("Change Playback Speed"));
  } else if (type == 2) {
    title = (_("Resample Video"));
  } else {
    title = (_("Adjust Clip Volume"));
  }

  new_pb_speed = lives_standard_dialog_new(title, FALSE, -1, -1);
  lives_signal_handlers_disconnect_by_func(new_pb_speed, LIVES_GUI_CALLBACK(return_true), NULL);
  lives_free(title);

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(new_pb_speed));

  vbox = lives_vbox_new(FALSE, widget_opts.packing_height * 2);

  lives_box_pack_start(LIVES_BOX(dialog_vbox), vbox, TRUE, TRUE, 0);

  if (type == 1) {
    lives_snprintf(label_text, 256,
                   _("Current playback speed is %.3f frames per second.\n\n"
                     "Please enter the desired playback speed\nin _frames per second"),
                   cfile->fps);
  } else if (type == 2) {
    lives_snprintf(label_text, 256,
                   _("Current playback speed is %.3f frames per second.\n\n"
                     "Please enter the _resampled rate\nin frames per second"),
                   cfile->fps);
  } else if (type == 3) {
    lives_snprintf(label_text, 256,
                   _("Current volume level for this clip is %.2f.\n\n"
                     "You may select a new  _volume level here.\n\n"
                     "Please note that the volume can also be varied during playback\n"
                     "using the %s and %s keys.\nChanging it here will make "
                     "the adjustment permanent.\n"), "'<'", "'>'", cfile->vol);
  }

  label = lives_standard_label_new_with_mnemonic_widget(label_text, NULL);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), label, FALSE, FALSE, widget_opts.packing_height);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

  if (type == 3) {
    add_fill_to_box(LIVES_BOX(hbox));
    spinbutton_pb_speed = lives_standard_spin_button_new(NULL, (double)cfile->vol, 0., 4., .01, .01, 2, LIVES_BOX(hbox), NULL);
    add_fill_to_box(LIVES_BOX(hbox));
  } else if (type == 2) {
    add_fill_to_box(LIVES_BOX(hbox));
    spinbutton_pb_speed = lives_standard_spin_button_new(NULL, cfile->fps, 1., FPS_MAX, .01, .1, 3, LIVES_BOX(hbox), NULL);
    add_fill_to_box(LIVES_BOX(hbox));
  } else {
    radiobutton1 = lives_standard_radio_button_new(NULL, &rbgroup, LIVES_BOX(hbox), NULL);

    spinbutton_pb_speed = lives_standard_spin_button_new(NULL, cfile->fps, 1., FPS_MAX, .01, .1, 3, LIVES_BOX(hbox), NULL);

    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

    add_fill_to_box(LIVES_BOX(hbox));

    label2 = lives_standard_label_new_with_mnemonic_widget(_("OR enter the desired clip length in _seconds"), NULL);
    lives_box_pack_start(LIVES_BOX(hbox), label2, TRUE, TRUE, widget_opts.packing_width);

    add_fill_to_box(LIVES_BOX(hbox));

    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

    radiobutton2 = lives_standard_radio_button_new(NULL, &rbgroup, LIVES_BOX(hbox), NULL);

    spinbutton_pb_time = lives_standard_spin_button_new(NULL,
                         (double)((int)(cfile->frames / cfile->fps * 100.)) / 100.,
                         1. / FPS_MAX, cfile->frames, 1., 10., 2, LIVES_BOX(hbox), NULL);

    lives_label_set_mnemonic_widget(LIVES_LABEL(label2), spinbutton_pb_time);
  }

  lives_label_set_mnemonic_widget(LIVES_LABEL(label), spinbutton_pb_speed);

  add_fill_to_box(LIVES_BOX(vbox));

  if (type == 1 && cfile->achans) {
    ca_hbox = lives_hbox_new(FALSE, 0);
    change_audio_speed = lives_standard_check_button_new
                         (_("Change the _audio speed as well"), mainw->fx1_bool, LIVES_BOX(ca_hbox), NULL);

    lives_box_pack_start(LIVES_BOX(vbox), ca_hbox, TRUE, TRUE, widget_opts.packing_height);

    if (capable->has_sox_sox) {
      ca_hbox = lives_hbox_new(FALSE, 0);

      tmpoonly = lives_standard_check_button_new
                 (_("Maintain audio pitch"), mainw->fx3_bool, LIVES_BOX(ca_hbox), NULL);

      lives_box_pack_start(LIVES_BOX(vbox), ca_hbox, TRUE, TRUE, widget_opts.packing_height);
    }

    add_fill_to_box(LIVES_BOX(vbox));
  }

  cancelbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(new_pb_speed), LIVES_STOCK_CANCEL, NULL,
                 LIVES_RESPONSE_CANCEL);

  lives_window_add_escape(LIVES_WINDOW(new_pb_speed), cancelbutton);

  /// TODO: needs more work to enable easy playback of audio without the video
  /* if (type == 3) { */
  /*   char *tmp; */
  /*   LiVESWidget *playbutt = lives_dialog_add_button_from_stock(LIVES_DIALOG(new_pb_speed), */
  /*                           LIVES_STOCK_MEDIA_PLAY, */
  /*                           (tmp = (cfile->real_pointer_time > 0. || (cfile->start == 1 && cfile->end == cfile->frames) ? */
  /*                                   (_("Preview audio")) : */
  /*                                   (_("Preview audio in selected range")))), */
  /*                           LIVES_RESPONSE_CANCEL); */
  /*   lives_free(tmp); */
  /*   lives_signal_connect(LIVES_GUI_OBJECT(playbutt), LIVES_WIDGET_CLICKED_SIGNAL, */
  /*                        LIVES_GUI_CALLBACK(preview_aud_vol_cb), */
  /*                        NULL); */
  /* } */

  change_pb_ok = lives_dialog_add_button_from_stock(LIVES_DIALOG(new_pb_speed), LIVES_STOCK_OK, NULL,
                 LIVES_RESPONSE_OK);

  lives_button_grab_default_special(change_pb_ok);
  lives_widget_grab_focus(spinbutton_pb_speed);

  if (type < 3) {
    reorder_leave_back_set(FALSE);
  }
  lives_signal_sync_connect(LIVES_GUI_OBJECT(cancelbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(lives_general_button_clicked), NULL);
  if (type == 1) {
    toggle_toggles_var(LIVES_TOGGLE_BUTTON(change_audio_speed), &mainw->fx1_bool, FALSE);
    toggle_toggles_var(LIVES_TOGGLE_BUTTON(tmpoonly), &mainw->fx3_bool, FALSE);

    lives_signal_sync_connect(LIVES_GUI_OBJECT(change_pb_ok), LIVES_WIDGET_CLICKED_SIGNAL,
                              LIVES_GUI_CALLBACK(on_change_speed_ok_clicked), NULL);
  } else if (type == 2) {
    lives_signal_connect(LIVES_GUI_OBJECT(change_pb_ok), LIVES_WIDGET_CLICKED_SIGNAL,
                         LIVES_GUI_CALLBACK(on_resample_vid_ok), NULL);

  } else if (type == 3) {
    lives_signal_sync_connect(LIVES_GUI_OBJECT(change_pb_ok), LIVES_WIDGET_CLICKED_SIGNAL,
                              LIVES_GUI_CALLBACK(on_avolch_ok), NULL);
  }

  lives_signal_sync_connect_after(LIVES_GUI_OBJECT(spinbutton_pb_speed), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                                  LIVES_GUI_CALLBACK(on_spin_value_changed), LIVES_INT_TO_POINTER(1));

  if (type == 1) {
    lives_signal_sync_connect_after(LIVES_GUI_OBJECT(spinbutton_pb_time), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                                    LIVES_GUI_CALLBACK(on_spin_value_changed), LIVES_INT_TO_POINTER(2));
    lives_signal_sync_connect_after(LIVES_GUI_OBJECT(spinbutton_pb_speed), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                                    LIVES_GUI_CALLBACK(widget_act_toggle), radiobutton1);
    lives_signal_sync_connect_after(LIVES_GUI_OBJECT(spinbutton_pb_time), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                                    LIVES_GUI_CALLBACK(widget_act_toggle), radiobutton2);
    lives_signal_sync_connect(LIVES_GUI_OBJECT(radiobutton2), LIVES_WIDGET_TOGGLED_SIGNAL,
                              LIVES_GUI_CALLBACK(on_boolean_toggled), &mainw->fx2_bool);
  }

  lives_widget_show_all(new_pb_speed);
}


static void rb_aud_sel_pressed(LiVESButton * button, livespointer user_data) {
  aud_dialog_t *audd = (aud_dialog_t *)user_data;
  audd->is_sel = !audd->is_sel;
  lives_widget_set_sensitive(audd->time_spin, !audd->is_sel);
}


aud_dialog_t *create_audfade_dialog(int type) {
  // type 0 = fade in
  // type 1 = fade out

  LiVESWidget *dialog_vbox;
  LiVESWidget *hbox;
  LiVESWidget *rb_sel;
  LiVESWidget *label;

  char *label_text = NULL, *label_text2 = NULL, *title;

  double max;

  LiVESSList *radiobutton_group = NULL;

  aud_dialog_t *audd = (aud_dialog_t *)lives_malloc(sizeof(aud_dialog_t));

  if (type == 0) {
    title = (_("Fade Audio In"));
  } else {
    title = (_("Fade Audio Out"));
  }

  audd->dialog = lives_standard_dialog_new(title, TRUE, -1, -1);
  lives_signal_handlers_disconnect_by_func(audd->dialog, LIVES_GUI_CALLBACK(return_true), NULL);
  lives_free(title);

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(audd->dialog));

  hbox = lives_hbox_new(FALSE, TB_HEIGHT_AUD);
  lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, TRUE, TRUE, widget_opts.packing_height);

  if (type == 0) {
    label_text = (_("Fade in over  "));
    label_text2 = (_("first"));
  } else if (type == 1) {
    label_text = (_("Fade out over  "));
    label_text2 = (_("last"));
  }

  label = lives_standard_label_new(label_text);
  if (label_text) lives_free(label_text);

  lives_box_pack_start(LIVES_BOX(hbox), label, FALSE, FALSE, 0);

  hbox = lives_hbox_new(FALSE, widget_opts.packing_width * 5);
  lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, TRUE, TRUE, widget_opts.packing_height);

  lives_standard_radio_button_new(label_text2, &radiobutton_group, LIVES_BOX(hbox), NULL);

  if (label_text2) lives_free(label_text2);

  max = cfile->laudio_time;

  widget_opts.swap_label = TRUE;
  audd->time_spin = lives_standard_spin_button_new(_("seconds."),
                    max / 2. > DEF_AUD_FADE_SECS ? DEF_AUD_FADE_SECS : max / 2., .1, max, 1., 10., 2,
                    LIVES_BOX(hbox), NULL);
  widget_opts.swap_label = FALSE;

  hbox = lives_hbox_new(FALSE, widget_opts.packing_width * 5);
  lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, TRUE, TRUE, widget_opts.packing_height);

  rb_sel = lives_standard_radio_button_new(_("selection"), &radiobutton_group, LIVES_BOX(hbox), NULL);
  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(rb_sel), FALSE);
  audd->is_sel = FALSE;

  if ((cfile->end - 1.) / cfile->fps > cfile->laudio_time) {
    // if selection is longer than audio time, we cannot use sel len
    lives_widget_set_sensitive(rb_sel, FALSE);
  }

  lives_signal_sync_connect_after(LIVES_GUI_OBJECT(rb_sel), LIVES_WIDGET_TOGGLED_SIGNAL,
                                  LIVES_GUI_CALLBACK(rb_aud_sel_pressed), (livespointer)audd);

  add_fill_to_box(LIVES_BOX(hbox));

  lives_widget_show_all(audd->dialog);

  return audd;
}


_commentsw *create_comments_dialog(lives_clip_t *sfile, char *filename) {
  LiVESWidget *dialog_vbox;
  LiVESWidget *table;
  LiVESWidget *label;
  LiVESWidget *vbox;
  LiVESWidget *hbox;
  LiVESWidget *buttond;

  char *extrabit, *title;

  _commentsw *commentsw = (_commentsw *)(lives_malloc(sizeof(_commentsw)));

  if (filename) extrabit = (_(" (Optional)"));
  else extrabit = lives_strdup("");

  title = lives_strdup_printf(_("File Comments%s"), extrabit);

  commentsw->comments_dialog = lives_standard_dialog_new(title, TRUE, -1, -1);
  lives_free(title);
  lives_free(extrabit);
  lives_signal_handlers_disconnect_by_func(commentsw->comments_dialog, LIVES_GUI_CALLBACK(return_true), NULL);

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(commentsw->comments_dialog));

  if (filename) {
    widget_opts.mnemonic_label = FALSE;
    label = lives_standard_label_new((extrabit = lives_strdup_printf(_("File Name: %s"), filename)));
    widget_opts.mnemonic_label = TRUE;
    lives_free(extrabit);
    lives_box_pack_start(LIVES_BOX(dialog_vbox), label, TRUE, TRUE, widget_opts.packing_height);
  }

  table = lives_table_new(4, 2, FALSE);
  lives_container_set_border_width(LIVES_CONTAINER(table), widget_opts.border_width);

  lives_table_set_row_spacings(LIVES_TABLE(table), widget_opts.packing_height * 4);

  lives_box_pack_start(LIVES_BOX(dialog_vbox), table, TRUE, TRUE, widget_opts.packing_height);

  label = lives_standard_label_new(_("Title/Name : "));

  lives_table_attach(LIVES_TABLE(table), label, 0, 1, 0, 1, (LiVESAttachOptions)(LIVES_FILL),
                     (LiVESAttachOptions)(0), 0, 0);

  label = lives_standard_label_new(_("Author/Artist : "));

  lives_table_attach(LIVES_TABLE(table), label, 0, 1, 1, 2,
                     (LiVESAttachOptions)(LIVES_FILL),
                     (LiVESAttachOptions)(0), 0, 0);

  label = lives_standard_label_new(_("Comments : "));

  lives_table_attach(LIVES_TABLE(table), label, 0, 1, 3, 4,
                     (LiVESAttachOptions)(LIVES_FILL),
                     (LiVESAttachOptions)(0), 0, 0);

  commentsw->title_entry = lives_standard_entry_new(NULL, cfile->title, MEDIUM_ENTRY_WIDTH, 1023, NULL, NULL);

  lives_table_attach(LIVES_TABLE(table), commentsw->title_entry, 1, 2, 0, 1,
                     (LiVESAttachOptions)(LIVES_EXPAND | LIVES_FILL),
                     (LiVESAttachOptions)(LIVES_EXPAND), 0, 0);

  commentsw->author_entry = lives_standard_entry_new(NULL, cfile->author, MEDIUM_ENTRY_WIDTH, 1023, NULL, NULL);

  lives_table_attach(LIVES_TABLE(table), commentsw->author_entry, 1, 2, 1, 2,
                     (LiVESAttachOptions)(LIVES_EXPAND | LIVES_FILL),
                     (LiVESAttachOptions)(LIVES_EXPAND), 0, 0);

  commentsw->comment_entry = lives_standard_entry_new(NULL, cfile->comment, MEDIUM_ENTRY_WIDTH, 1023, NULL, NULL);

  lives_table_attach(LIVES_TABLE(table), commentsw->comment_entry, 1, 2, 3, 4,
                     (LiVESAttachOptions)(LIVES_EXPAND | LIVES_FILL),
                     (LiVESAttachOptions)(LIVES_EXPAND), 0, 0);

  if (filename) {
    // options
    vbox = lives_vbox_new(FALSE, 0);

    add_fill_to_box(LIVES_BOX(vbox));

    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

    commentsw->subt_checkbutton = lives_standard_check_button_new(_("Save _subtitles to file"), FALSE, LIVES_BOX(hbox), NULL);

    if (!sfile->subt) {
      lives_widget_set_sensitive(commentsw->subt_checkbutton, FALSE);
    } else lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(commentsw->subt_checkbutton), TRUE);

    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

    commentsw->subt_entry = lives_standard_entry_new(_("Subtitle file"), NULL, SHORT_ENTRY_WIDTH, -1, LIVES_BOX(hbox), NULL);

    buttond = lives_standard_button_new_with_label(_("Browse..."), DEF_BUTTON_WIDTH, DEF_BUTTON_HEIGHT);

    lives_signal_sync_connect(buttond, LIVES_WIDGET_CLICKED_SIGNAL, LIVES_GUI_CALLBACK(on_save_subs_activate),
                              (livespointer)commentsw->subt_entry);

    lives_box_pack_start(LIVES_BOX(hbox), buttond, FALSE, FALSE, widget_opts.packing_width);

    add_fill_to_box(LIVES_BOX(vbox));

    if (!sfile->subt) {
      lives_widget_set_sensitive(commentsw->subt_entry, FALSE);
      lives_widget_set_sensitive(buttond, FALSE);
    } else {
      char xfilename[512];
      char *osubfname = NULL;

      lives_snprintf(xfilename, 512, "%s", filename);
      get_filename(xfilename, FALSE); // strip extension
      switch (sfile->subt->type) {
      case SUBTITLE_TYPE_SRT:
        osubfname = lives_strdup_printf("%s.%s", xfilename, LIVES_FILE_EXT_SRT);
        break;

      case SUBTITLE_TYPE_SUB:
        osubfname = lives_strdup_printf("%s.%s", xfilename, LIVES_FILE_EXT_SUB);
        break;

      default:
        break;
      }
      lives_entry_set_text(LIVES_ENTRY(commentsw->subt_entry), osubfname);
      mainw->subt_save_file = osubfname; // assign instead of free
    }

    lives_widget_set_size_request(vbox, ENC_DETAILS_WIN_H, ENC_DETAILS_WIN_V);
    lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);
    lives_standard_expander_new(_("_Options"), _("Hide _Options"), LIVES_BOX(dialog_vbox), vbox);
  }

  lives_widget_show_all(commentsw->comments_dialog);

  return commentsw;
}


static char last_good_folder[PATH_MAX];

static void chooser_check_dir(LiVESFileChooser * chooser, livespointer user_data) {
  char *cwd = lives_get_current_dir();
  char *new_dir;

#ifdef GUI_GTK
  new_dir = gtk_file_chooser_get_current_folder(chooser);
#endif
#ifdef GUI_QT
  QFileDialog *qchooser = static_cast<QFileDialog *>(chooser);
  new_dir = qchooser->directory().path().toLocal8Bit().data();
#endif

  if (!lives_strcmp(new_dir, last_good_folder)) {
    lives_free(cwd);
    return;
  }

  if (lives_chdir(new_dir, TRUE)) {
    lives_free(cwd);
#ifdef GUI_GTK
    gtk_file_chooser_set_current_folder(chooser, last_good_folder);
#endif
#ifdef GUI_QT
    qchooser->setDirectory(last_good_folder);
#endif
    do_dir_perm_access_error(new_dir);
    lives_free(new_dir);
    return;
  }
  lives_snprintf(last_good_folder, PATH_MAX, "%s", new_dir);
  lives_chdir(cwd, FALSE);
  lives_free(new_dir);
  lives_free(cwd);
}


void restore_wm_stackpos(LiVESButton * button) {
  LiVESWindow *dialog;
  if ((dialog = (LiVESWindow *)lives_widget_object_get_data
                (LIVES_WIDGET_OBJECT(button), KEEPABOVE_KEY)) != NULL) {
    lives_widget_show_all(LIVES_WIDGET(dialog));
    lives_window_present(LIVES_WINDOW(dialog));
  }
}


/**
   @brief callback for lives_standard filesel button
   same callback is used for dierctory buttons
   object_data in button refinses the behaviousr, see code for details

   such buttons may be created independently (e.g for the RFX "fileread" / "filewrite" special types
   or via lives_standard_direntry_new() / lives_standard_fileentry_new()
*/
void on_filesel_button_clicked(LiVESButton * button, livespointer user_data) {
  LiVESWidget *tentry = LIVES_WIDGET(user_data);
  lives_rfx_t *rfx;
  char **filt = NULL;
  char *dirname = NULL, *fname, *tmp, *def_dir = NULL;
  boolean is_dir = FALSE, free_def_dir = FALSE, show_hidden = FALSE, free_filt = FALSE;
  int filesel_type = LIVES_FILE_SELECTION_UNDEFINED;

  if (button) {
    /// various data can be set in the button object, including:
    /// (set in lives_standard_file_button_new())
    /// DEFDIR_KEY (char *)
    /// ISDIR_KEY (boolean)
    ///
    /// FILTER_KEY (char **)
    /// FILESEL_TYPE_KEY (int (enum))

    // selects between file mode and directory mode
    is_dir = GET_INT_DATA(button, ISDIR_KEY);

    // default dir for directory mode
    def_dir = (char *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(button), DEFDIR_KEY);

    /// NULL terminated array of char * filters (file extensions)
    filt = (char **)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(button), FILTER_KEY);

    /// fine tunes for the file selection / dir selection target
    filesel_type = GET_INT_DATA(button, FILESEL_TYPE_KEY);
    if (filesel_type & LIVES_SELECTION_SHOW_HIDDEN) {
      show_hidden = TRUE;
      filesel_type ^= LIVES_SELECTION_SHOW_HIDDEN;
    }
  }

  /// take the filename from the text entry widget
  if (LIVES_IS_TEXT_VIEW(tentry)) fname = lives_text_view_get_text(LIVES_TEXT_VIEW(tentry));
  else fname = lives_strdup(lives_entry_get_text(LIVES_ENTRY(tentry)));

  g_print("GOT FNAME %s\n", fname);

  /// TODO: only do this for directory mode, blank text is valid filename
  if (is_dir) {
    /// if no text, we look instead in def_dir (if present)
    if (!*fname) {
      lives_free(fname);
      fname = def_dir;
    }
  }

  if (!mainw->is_ready) {
    LiVESWindow *dialog;
    if ((dialog = (LiVESWindow *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(button), KEEPABOVE_KEY)) != NULL)
      lives_window_set_keep_above(dialog, FALSE);
  }

  /// can this be removed ?
  lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);

  switch (filesel_type) {
  case LIVES_FILE_SELECTION_UNDEFINED:
    if (!is_dir && *fname && (!def_dir || !(*def_dir))) {
      def_dir = get_dir(fname);
      free_def_dir = TRUE;
    }

    dirname = choose_file(is_dir ? fname : def_dir, is_dir ? NULL : fname, filt,
                          is_dir ? LIVES_FILE_CHOOSER_ACTION_SELECT_FOLDER :
                          (fname == def_dir && def_dir && !lives_strcmp(def_dir, LIVES_DEVICE_DIR))
                          ? LIVES_FILE_CHOOSER_ACTION_SELECT_DEVICE :
                          LIVES_FILE_CHOOSER_ACTION_OPEN, NULL, NULL);
    break;

  case LIVES_DIR_SELECTION_CREATE_FOLDER:
    dirname = choose_file(fname, NULL, NULL, LIVES_FILE_CHOOSER_ACTION_CREATE_FOLDER, NULL, NULL);
    break;
  case LIVES_DIR_SELECTION_SELECT_FOLDER:
    dirname = choose_file(fname, NULL, NULL, LIVES_FILE_CHOOSER_ACTION_SELECT_FOLDER, NULL, NULL);
    break;
  case LIVES_DIR_SELECTION_WORKDIR_INIT:
    dirname = choose_file(capable->home_dir, LIVES_DEF_WORK_SUBDIR, NULL, LIVES_FILE_CHOOSER_ACTION_CREATE_FOLDER, NULL, NULL);
    break;
  case LIVES_DIR_SELECTION_WORKDIR:
    dirname = choose_file(prefs->workdir, LIVES_DEF_WORK_SUBDIR, NULL, LIVES_FILE_CHOOSER_ACTION_CREATE_FOLDER, NULL, NULL);

    if (lives_strcmp(dirname, fname)) {
      /// apply extra validity checks (check writeable, warn if set to home dir, etc)
      if (check_workdir_valid(&dirname, LIVES_DIALOG(lives_widget_get_toplevel(LIVES_WIDGET(button))),
                              FALSE) == LIVES_RESPONSE_RETRY) {
        lives_free(dirname);
        dirname = lives_strdup(fname);
      }
    }
    break;
  case LIVES_FILE_SELECTION_OPEN:
  case LIVES_FILE_SELECTION_SAVE: {
    char fnamex[PATH_MAX], dirnamex[PATH_MAX];

    lives_snprintf(dirnamex, PATH_MAX, "%s", fname);
    lives_snprintf(fnamex, PATH_MAX, "%s", fname);

    get_dirname(dirnamex);
    get_basename(fnamex);

    if (filesel_type == LIVES_FILE_SELECTION_OPEN) {
      if (!show_hidden)
        dirname = choose_file(def_dir ? def_dir : dirnamex, fnamex, filt,
                              LIVES_FILE_CHOOSER_ACTION_OPEN, NULL, NULL);
      else  {
        dirname = choose_file(def_dir ? def_dir : dirnamex, fnamex, filt,
                              LIVES_FILE_CHOOSER_ACTION_SELECT_HIDDEN_FILE, NULL, NULL);
      }
    } else {
      if (!is_dir && !filt && *fnamex) {
        /// for save and not is_dir, we break filename into directory, filename components
        /// and set a filter with the filename extension (can be overridden by setting FILTER_KEY)
        char *tmp;
        filt = (char **)lives_malloc(2 * sizeof(char *));
        filt[0] = lives_strdup_printf("*.%s", (tmp = get_extension(fnamex)));
        filt[1] = NULL;
        free_filt = TRUE;
        lives_free(tmp);
      }

      dirname = choose_file(def_dir ? def_dir : dirnamex, fnamex, filt,
                            LIVES_FILE_CHOOSER_ACTION_SAVE, NULL, NULL);
    }
  }
  break;

  default: {
    /// other types get a filechooser with preview
    LiVESWidget *chooser = choose_file_with_preview(def_dir, fname, filt, filesel_type);
    int resp = lives_dialog_run(LIVES_DIALOG(chooser));

    end_fs_preview(NULL, NULL);

    if (resp == LIVES_RESPONSE_ACCEPT) {
      dirname = lives_file_chooser_get_filename(LIVES_FILE_CHOOSER(chooser));
    }
    lives_widget_destroy(LIVES_WIDGET(chooser));
  }
  }

  if (free_filt) {
    lives_free(filt[0]);
    lives_free(filt);
  }

  if (!mainw->is_ready) restore_wm_stackpos(button);

  if (fname && fname != def_dir) lives_free(fname);
  if (free_def_dir) lives_free(def_dir);

  /// we set dirname in both file mode and dir mode
  if (!dirname) return;

  /// update text widget
  if (LIVES_IS_ENTRY(tentry)) lives_entry_set_text(LIVES_ENTRY(tentry),
        (tmp = lives_filename_to_utf8(dirname, -1, NULL, NULL, NULL)));
  else lives_text_view_set_text(LIVES_TEXT_VIEW(tentry)
                                  , (tmp = lives_filename_to_utf8(dirname, -1, NULL, NULL, NULL)), -1);
  lives_free(tmp); lives_free(dirname);

  if ((rfx = (lives_rfx_t *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(tentry), RFX_KEY)) != NULL) {
    /// if running inside a parameter window, reflect update in related parameter values
    int param_number =
      LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(tentry), PARAM_NUMBER_KEY));
    after_param_text_changed(tentry, rfx);

    /// clear flag bit since no unapplied edits have been made
    rfx->params[param_number].flags &= ~PARAM_FLAG_VALUE_SET;
  }
}



#if GTK_CHECK_VERSION(3, 10, 0)
#define DETECTOR_STRING "a0b1c2d3e4f5"
#endif

static void fc_sel_changed(LiVESFileChooser * chooser, livespointer user_data) {
  // what should happen with gtkfilechooser, but doesnt (!)
  // setting mode to CREATE_DIRECTORY should allow creating a new directory or entering an existing one
  //        - this is fine
  // when first shown or after a double-clisk (enter directory), the directory path should be shown in the file name bar
  // the select (activate) button should be sensitive, clicking it should select the current directory
  // gtk enters the parent directory and shows the target dir as selected - this is fine, except nothing is show in the entry
  // and the select button is insensitive !
  // you have to actually click a few times for the "select" button to light up. still nothing in file entry

  // -  what happens: directory name is not shown in the file entry, select button is insensitive
  // single clicking on a directory with should show its path in the filentry box, the select button should be sensitive
  // also the directory should appear as "selected"

  // double clicking on a subdir, opens the subdir, but still nothing in the entry. and select button is insensitive
  // - creating a directory makes the filechooser enter the directory but the select button is off, you have to go up one dir to the parent
  // so what is the point of entering the empty dir ?

  // this callback fixes all of this
  static boolean no = FALSE;
  if (no) {
    no = FALSE;
    return;
  }
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 10, 0)
  struct fc_dissection *diss = (struct fc_dissection *)user_data;
#else
  void *diss = NULL;
#endif
#endif
  char *tmp;

  // folder for non-idrs
  char *fold = NULL;

  // filename for non-dirs
  char *dirname = NULL;
  char *extra_dir = lives_widget_object_get_data(LIVES_WIDGET_OBJECT(chooser), DEF_FILE_KEY);

  int act = GET_INT_DATA(chooser, FC_ACTION_KEY);

  tmp = lives_file_chooser_get_filename(LIVES_FILE_CHOOSER(chooser));

  if (tmp) {
    get_dirname(tmp);
    if (*tmp) dirname = lives_filename_to_utf8(tmp, -1, NULL, NULL, NULL);
    lives_free(tmp);
  }

  tmp = gtk_file_chooser_get_current_folder(LIVES_FILE_CHOOSER(chooser));

  if (tmp) {
    fold = lives_filename_to_utf8(tmp, -1, NULL, NULL, NULL);
    lives_free(tmp);
  }

  if (!dirname && !fold) return;

  if (act != LIVES_FILE_CHOOSER_ACTION_OPEN) {
    if (diss && diss->selbut) {
      if (lives_file_test(dirname, LIVES_FILE_TEST_IS_REGULAR)) {
        lives_widget_set_sensitive(diss->selbut, FALSE);
        return;
      }
    }
    tmp = lives_build_path(dirname, extra_dir, NULL);
    if (lives_file_test(tmp, LIVES_FILE_TEST_IS_DIR)) {
      no = TRUE;
      gtk_file_chooser_select_filename(chooser, tmp);
    }
  }

#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 10, 0)
  if (diss) {
    char *path;
    char buff[PATH_MAX];
    size_t s1 = lives_strlen(dirname);
    size_t s2 = lives_strlen(fold);
    boolean folhas = FALSE, fnahas = FALSE;

    if (extra_dir) {
      if (lives_string_ends_with(dirname, "%s", extra_dir)) {
        fnahas = TRUE;
        s1 -= lives_strlen(extra_dir) + 1;
      }
      if (lives_string_ends_with(fold, "%s", extra_dir)) {
        folhas = TRUE;
        s2 -= lives_strlen(extra_dir) + 1;
      }

      if (s1 > s2) {
        if (!fnahas) path = lives_build_path(dirname, extra_dir, NULL);
        else path = dirname;
      } else {
        if (!folhas) path = lives_build_path(fold, extra_dir, NULL);
        else path = fold;
      }
    } else path = fold;

    lives_snprintf(buff, PATH_MAX, "%s", path);
    if (path != dirname && path != fold) lives_free(path);

    if (act != LIVES_FILE_CHOOSER_ACTION_SAVE && act != LIVES_FILE_CHOOSER_ACTION_OPEN) {
      if (!lives_file_test(buff, LIVES_FILE_TEST_IS_DIR)) {
        ensure_isdir(buff);
      }
    }

    if (diss->new_entry) {
      lives_entry_set_text(LIVES_ENTRY(diss->new_entry), buff);
      for (LiVESWidget *widget = lives_widget_get_parent(diss->new_entry); widget;
           widget = lives_widget_get_parent(widget)) {
        if (LIVES_IS_GRID(widget)) {
          set_child_alt_colour(widget, TRUE);
          break;
        }
      }
    }
    if (diss->selbut) lives_widget_set_sensitive(diss->selbut, TRUE);
  }
#endif
#endif
}


static void fc_folder_changed(LiVESFileChooser * chooser, livespointer user_data) {
  //struct fc_dissection *diss = (struct fc_dissection *)user_data;
  static boolean no = FALSE;
  if (no) {
    // prevents recursion after updating selection; blocking signal seems not to work
    no = FALSE;
    return;
  } else {
    char *tmp;
    char *extra_dir = lives_widget_object_get_data(LIVES_WIDGET_OBJECT(chooser), DEF_FILE_KEY);
    char *dirname = lives_file_chooser_get_filename(chooser);
    int act = GET_INT_DATA(chooser, FC_ACTION_KEY);

#if GTK_CHECK_VERSION(3, 10, 0)
    struct fc_dissection *diss = (struct fc_dissection *)user_data;
    //if (diss && diss->treeview) set_child_colour(diss->treeview, TRUE);
#endif

#if GTK_CHECK_VERSION(3, 10, 0)
    if (diss && act == LIVES_FILE_CHOOSER_ACTION_SELECT_FOLDER) {
      lives_widget_set_sensitive(diss->selbut, FALSE);
    }
#endif
    tmp = lives_build_path(dirname, extra_dir, NULL);
    if (lives_file_test(tmp, LIVES_FILE_TEST_IS_DIR)) {
      no = TRUE;
      gtk_file_chooser_select_filename(chooser, tmp);
#if GTK_CHECK_VERSION(3, 10, 0)
      if (diss && act == LIVES_FILE_CHOOSER_ACTION_SELECT_FOLDER) {
        lives_widget_set_sensitive(diss->selbut, TRUE);
      }
#endif
    }
    lives_free(tmp);
  }
}


static boolean dir_only_filt(const GtkFileFilterInfo * filter_info, livespointer data) {
  return lives_file_test(filter_info->filename, LIVES_FILE_TEST_IS_DIR);
}


static char *_choose_file(const char *dir, const char *fname, char **const filt, LiVESFileChooserAction act,
                          const char *title, LiVESWidget * extra_widget) {
  // new style file chooser
  LiVESWidget *chooser, *action_box;

#if GTK_CHECK_VERSION(3, 10, 0)
  struct fc_dissection *diss;
  LiVESList *elist = NULL;
  LiVESWidget *hbox;
  char *oldname;
#else
  void *diss = NULL;
#endif
  // in/out values are in utf8 encoding
  GtkFileFilter *custfilt = NULL;

  char *mytitle;
  char *filename = NULL;
  boolean show_hidden = FALSE;
  LiVESResponseType response;

  if (act == LIVES_FILE_CHOOSER_ACTION_SELECT_HIDDEN_FILE) {
    show_hidden = TRUE;
    act = LIVES_FILE_CHOOSER_ACTION_OPEN;
  }

  if (!title) {
    if (act == LIVES_FILE_CHOOSER_ACTION_SELECT_DEVICE) {
      mytitle = lives_strdup_printf(_("%sChoose a Device"), widget_opts.title_prefix);
      act = LIVES_FILE_CHOOSER_ACTION_OPEN;
    } else if (act == LIVES_FILE_CHOOSER_ACTION_SELECT_FOLDER || act == LIVES_FILE_CHOOSER_ACTION_CREATE_FOLDER
               || act == LIVES_FILE_CHOOSER_ACTION_SELECT_CREATE_FOLDER) {
      mytitle = lives_strdup_printf(_("%sChoose a Directory"), widget_opts.title_prefix);
    } else {
      mytitle = lives_strdup_printf(_("%sChoose a File"), widget_opts.title_prefix);
    }
  } else mytitle = lives_strdup_printf("%s%s", widget_opts.title_prefix, title);

#ifdef GUI_GTK
  if (act != LIVES_FILE_CHOOSER_ACTION_SAVE) {
    const char *stocklabel;
    response = LIVES_RESPONSE_ACCEPT;
    if (act == LIVES_FILE_CHOOSER_ACTION_OPEN) {
      stocklabel = LIVES_STOCK_LABEL_OPEN;
    } else {
      response = LIVES_RESPONSE_NO;
      stocklabel = LIVES_STOCK_LABEL_SELECT;
    }
    chooser = gtk_file_chooser_dialog_new(mytitle, LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET), (LiVESFileChooserAction)act,
                                          LIVES_STOCK_LABEL_CANCEL, LIVES_RESPONSE_CANCEL,
                                          stocklabel, response, NULL);
  } else {
    chooser = gtk_file_chooser_dialog_new(mytitle, LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET), (LiVESFileChooserAction)act,
                                          LIVES_STOCK_LABEL_CANCEL, LIVES_RESPONSE_CANCEL,
                                          LIVES_STOCK_LABEL_SAVE, LIVES_RESPONSE_ACCEPT, NULL);
  }

  if (act == LIVES_FILE_CHOOSER_ACTION_SELECT_FOLDER || act == LIVES_FILE_CHOOSER_ACTION_CREATE_FOLDER
      || act == LIVES_FILE_CHOOSER_ACTION_SELECT_CREATE_FOLDER) {
    if (!custfilt) {
      custfilt = gtk_file_filter_new();
      if (fname) gtk_file_filter_set_name(custfilt, fname);
      else gtk_file_filter_set_name(custfilt, _("Directories"));
      gtk_file_filter_add_custom(custfilt, GTK_FILE_FILTER_FILENAME, dir_only_filt, NULL, NULL);
    }
    gtk_file_chooser_add_filter(LIVES_FILE_CHOOSER(chooser), custfilt);
  }

  SET_INT_DATA(chooser, FC_ACTION_KEY, act);

  lives_widget_show_all(chooser);

  if (prefs->fileselmax) {
    lives_window_maximize(LIVES_WINDOW(chooser));
  }

  gtk_file_chooser_set_local_only(LIVES_FILE_CHOOSER(chooser), TRUE);

  action_box = lives_dialog_get_action_area(LIVES_DIALOG(chooser));

  if (mainw->is_ready && palette->style & STYLE_1) {
    lives_widget_set_bg_color(chooser, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
    lives_widget_set_fg_color(chooser, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
    lives_widget_set_bg_color(action_box, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
    lives_widget_set_fg_color(action_box, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
  }

#if GTK_CHECK_VERSION(3, 10, 0)
  diss = set_child_colour(chooser, FALSE);
  if (diss) {
    diss->selbut = lives_dialog_get_widget_for_response(LIVES_DIALOG(chooser), LIVES_RESPONSE_NO);

    if (act == LIVES_FILE_CHOOSER_ACTION_CREATE_FOLDER || act == LIVES_FILE_CHOOSER_ACTION_SELECT_CREATE_FOLDER
        || act == LIVES_FILE_CHOOSER_ACTION_SELECT_FILE
       ) {
      // only these actions are allowed to call set_current_name
      oldname = gtk_file_chooser_get_filename(LIVES_FILE_CHOOSER(chooser));
      gtk_file_chooser_set_current_name(LIVES_FILE_CHOOSER(chooser), DETECTOR_STRING);
      elist = diss->entry_list;
      for (; elist; elist = elist->next) {
        if (!lives_strcmp(lives_entry_get_text((LiVESEntry *)elist->data), DETECTOR_STRING)) {
          diss->old_entry = (LiVESWidget *)elist->data;
          break;
        }
      }
      if (oldname) {
        gtk_file_chooser_set_current_name(LIVES_FILE_CHOOSER(chooser), oldname);
        lives_free(oldname);
      } else gtk_file_chooser_set_current_name(LIVES_FILE_CHOOSER(chooser), "");
      if (diss->old_entry) {
        lives_widget_hide(diss->old_entry);

        hbox = lives_hbox_new(FALSE, 0);
        widget_opts.expand = LIVES_EXPAND_DEFAULT_HEIGHT | LIVES_EXPAND_EXTRA_WIDTH;
        diss->new_entry = lives_standard_entry_new(NULL, fname, -1, PATH_MAX, LIVES_BOX(hbox),
                          H_("Select an existing directory or create a new one"));
        widget_opts.expand = LIVES_EXPAND_DEFAULT;
        lives_grid_attach_next_to(LIVES_GRID(lives_widget_get_parent(diss->old_entry)),
                                  hbox, diss->old_entry, LIVES_POS_RIGHT, 1, 1);
        lives_widget_show_all(hbox);
      }
    }
  }
#endif

  if (dir) gtk_file_chooser_set_current_folder(LIVES_FILE_CHOOSER(chooser), dir);

  gtk_file_chooser_set_show_hidden(LIVES_FILE_CHOOSER(chooser), show_hidden);

  SET_INT_DATA(chooser, FC_ACTION_KEY, act);

  if (filt) {
    int i;
    GtkFileFilter *filter = gtk_file_filter_new();
    for (i = 0; filt[i]; i++) gtk_file_filter_add_pattern(filter, filt[i]);
    gtk_file_chooser_set_filter(LIVES_FILE_CHOOSER(chooser), filter);
    if (i == 1 && act == LIVES_FILE_CHOOSER_ACTION_SAVE)
      gtk_file_chooser_set_current_name(LIVES_FILE_CHOOSER(chooser), filt[0]); //utf-8
  }

  if (fname) {
    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(chooser), DEF_FILE_KEY, (livespointer)fname);

    if (act == LIVES_FILE_CHOOSER_ACTION_SELECT_FOLDER) {
      gtk_file_chooser_select_filename(LIVES_FILE_CHOOSER(chooser), fname);
      lives_signal_sync_connect_after(LIVES_GUI_OBJECT(chooser), LIVES_WIDGET_CURRENT_FOLDER_CHANGED_SIGNAL,
                                      LIVES_GUI_CALLBACK(fc_folder_changed), diss);
    } else {
      if (act == LIVES_FILE_CHOOSER_ACTION_SAVE || act == LIVES_FILE_CHOOSER_ACTION_CREATE_FOLDER
          || act == LIVES_FILE_CHOOSER_ACTION_OPEN) {
        if (dir) {
          char *ffname = lives_build_filename(dir, fname, NULL);
          if (act != LIVES_FILE_CHOOSER_ACTION_OPEN)
            gtk_file_chooser_set_current_name(LIVES_FILE_CHOOSER(chooser), fname); // utf-8
          gtk_file_chooser_select_filename(LIVES_FILE_CHOOSER(chooser), ffname); // must be dir and file
          lives_free(ffname);
        } else {
          if (!lives_file_test(fname, LIVES_FILE_TEST_IS_DIR)) {
            gtk_file_chooser_set_filename(LIVES_FILE_CHOOSER(chooser), fname);
            gtk_file_chooser_set_current_name(LIVES_FILE_CHOOSER(chooser), fname);
          } else {
            gtk_file_chooser_select_filename(LIVES_FILE_CHOOSER(chooser), fname);
            gtk_file_chooser_set_filename(LIVES_FILE_CHOOSER(chooser), fname);
          }
        }

        lives_signal_sync_connect_after(LIVES_GUI_OBJECT(chooser), LIVES_WIDGET_SELECTION_CHANGED_SIGNAL,
                                        LIVES_GUI_CALLBACK(fc_sel_changed), diss);

        if (act != LIVES_FILE_CHOOSER_ACTION_OPEN) {
          lives_signal_sync_connect_after(LIVES_GUI_OBJECT(chooser), LIVES_WIDGET_CURRENT_FOLDER_CHANGED_SIGNAL,
                                          LIVES_GUI_CALLBACK(fc_folder_changed), diss);
	// *INDENT-OFF*
	}}}}
  // *INDENT-ON*
  else {
    /* if (act == LIVES_FILE_CHOOSER_ACTION_SAVE || act == LIVES_FILE_CHOOSER_ACTION_CREATE_FOLDER */
    /* 	|| act == LIVES_FILE_CHOOSER_ACTION_OPEN) { */
    /* 	lives_signal_sync_connect_after(LIVES_GUI_OBJECT(chooser), LIVES_WIDGET_SELECTION_CHANGED_SIGNAL, */
    /* 					LIVES_GUI_CALLBACK(fc_sel_changed), diss); */
    /* } */
  }
#endif

  lives_container_set_border_width(LIVES_CONTAINER(chooser), widget_opts.border_width);

  if (prefs->show_gui) {
    LiVESWindow *transient = widget_opts.transient;
    if (!transient) transient = get_transient_full();
    if (transient)
      lives_window_set_transient_for(LIVES_WINDOW(chooser), transient);
  }

  lives_signal_sync_connect(chooser, LIVES_WIDGET_CURRENT_FOLDER_CHANGED_SIGNAL, LIVES_GUI_CALLBACK(chooser_check_dir), NULL);
  lives_window_center(LIVES_WINDOW(chooser));
  lives_window_set_modal(LIVES_WINDOW(chooser), TRUE);
  lives_memset(last_good_folder, 0, 1);

  if (dir) {
    gtk_file_chooser_set_current_folder(LIVES_FILE_CHOOSER(chooser), dir);
    gtk_file_chooser_add_shortcut_folder(LIVES_FILE_CHOOSER(chooser), dir, NULL);
  }

  pop_to_front(chooser, NULL);

  if (extra_widget && extra_widget == LIVES_MAIN_WINDOW_WIDGET) {
    return (char *)chooser; // kludge to allow custom adding of extra widgets
  }

rundlg:
  if ((response = lives_dialog_run(LIVES_DIALOG(chooser))) != LIVES_RESPONSE_CANCEL) {
    char *tmp;
    if (diss && diss->new_entry) {
      if (act == LIVES_FILE_CHOOSER_ACTION_SAVE) {
        filename = lives_build_filename(gtk_file_chooser_get_current_folder(LIVES_FILE_CHOOSER(chooser)),
                                        lives_entry_get_text(LIVES_ENTRY(diss->new_entry)), NULL);
      } else filename = lives_strdup(lives_entry_get_text(LIVES_ENTRY(diss->new_entry)));
    } else {
      filename = lives_filename_to_utf8((tmp = lives_file_chooser_get_filename(LIVES_FILE_CHOOSER(chooser))),
                                        -1, NULL, NULL, NULL);
      lives_free(tmp);
    }
  } else filename = NULL;

  if (response && filename && act == LIVES_FILE_CHOOSER_ACTION_SAVE) {
    if (!check_file(filename, TRUE)) {
      lives_free(filename);
      filename = NULL;
      goto rundlg;
    }
  }

  lives_free(mytitle);
  lives_widget_destroy(chooser);

#if GTK_CHECK_VERSION(3, 10, 0)
  if (diss) {
    if (diss->entry_list) lives_list_free(diss->entry_list);
    lives_free(diss);
  }
#endif
  return filename;
}

char *choose_file(const char *dir, const char *fname, char **const filt, LiVESFileChooserAction act,
                  const char *title, LiVESWidget * extra_widget) {
  char *cret;
  main_thread_execute((lives_funcptr_t)_choose_file, WEED_SEED_STRING,
                      &cret, "ssvisv", dir, fname, filt, act, title, extra_widget);
  return cret;
}


LiVESWidget *choose_file_with_preview(const char *dir, const char *title, char **const filt, int filesel_type) {
  // filesel_type 1 - video and audio open (single - opensel)
  //LIVES_FILE_SELECTION_VIDEO_AUDIO

  // preview type 2 - import audio
  // LIVES_FILE_SELECTION_AUDIO_ONLY

  // filesel_type 3 - video and audio open (multiple)
  //LIVES_FILE_SELECTION_VIDEO_AUDIO_MULTI

  // type 4
  // LIVES_FILE_SELECTION_VIDEO_RANGE

  // type 5
  // LIVES_FILE_SELECTION_IMAGE_ONLY

  // unfortunately we cannot simply run this and return a filename, in case there is a selection

  LiVESWidget *chooser, *pbut, *action_box;
  int preview_type;

  if (filesel_type == LIVES_DIR_SELECTION_CREATE_FOLDER) {
    chooser = (LiVESWidget *)_choose_file(dir, NULL, filt, LIVES_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                          title, LIVES_MAIN_WINDOW_WIDGET);
    gtk_file_chooser_set_create_folders(LIVES_FILE_CHOOSER(chooser), TRUE);
  } else if (filesel_type == LIVES_DIR_SELECTION_SELECT_FOLDER)
    chooser = (LiVESWidget *)_choose_file(dir, NULL, filt, LIVES_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                          title, LIVES_MAIN_WINDOW_WIDGET);

  chooser = (LiVESWidget *)_choose_file(dir, NULL, filt, LIVES_FILE_CHOOSER_ACTION_OPEN, title, LIVES_MAIN_WINDOW_WIDGET);

  if (filesel_type == LIVES_FILE_SELECTION_VIDEO_AUDIO_MULTI) {
#ifdef GUI_GTK
    gtk_file_chooser_set_select_multiple(LIVES_FILE_CHOOSER(chooser), TRUE);
#endif
  }

  switch (filesel_type) {
  case LIVES_FILE_SELECTION_VIDEO_AUDIO:
  case LIVES_FILE_SELECTION_VIDEO_AUDIO_MULTI:
    preview_type = LIVES_PREVIEW_TYPE_VIDEO_AUDIO;
    break;
  case LIVES_FILE_SELECTION_IMAGE_ONLY:
    preview_type = LIVES_PREVIEW_TYPE_IMAGE_ONLY;
    break;
  default:
    preview_type = LIVES_PREVIEW_TYPE_AUDIO_ONLY;
  }

  action_box = lives_dialog_get_action_area(LIVES_DIALOG(chooser));

  pbut = widget_add_preview(chooser, LIVES_BOX(lives_dialog_get_content_area(LIVES_DIALOG(chooser))),
                            NULL, LIVES_BOX(action_box), preview_type);

  gtk_file_chooser_set_extra_widget(LIVES_FILE_CHOOSER(chooser), pbut);

  if (prefs->fileselmax) {
    int scr_width = GUI_SCREEN_WIDTH;
    int scr_height = GUI_SCREEN_HEIGHT;
    int bx, by, w, h;

    lives_widget_show_all(chooser);
    lives_widget_process_updates(chooser);

    get_border_size(chooser, &bx, &by);
    w = lives_widget_get_allocation_width(chooser);
    h = lives_widget_get_allocation_height(chooser);
    //#define DEBUG_OVERFLOW
    if (w > scr_width - bx || h > scr_height - by) {
      if (w > scr_width - bx || h > scr_height - by) {
        int overflowx = w - (scr_width - bx);
        int overflowy = h - (scr_height - by);

        int mywidth = lives_widget_get_allocation_width(chooser);
        int myheight = lives_widget_get_allocation_height(chooser);

#ifdef DEBUG_OVERFLOW
        g_print("overflow is %d X %d\n", overflowx, overflowy);
#endif
        if (overflowx > 0) mywidth -= overflowx;
        if (overflowy > 0) myheight -= overflowy;

        lives_widget_process_updates(chooser);

        if (overflowx > 0 || overflowy > 0) {
          lives_widget_set_size_request(chooser, mywidth, myheight);
        }
        lives_widget_process_updates(chooser);

        w = scr_width - bx;
        h = scr_height - by;

        lives_window_unmaximize(LIVES_WINDOW(chooser));
        lives_widget_process_updates(chooser);
        lives_window_resize(LIVES_WINDOW(chooser), w, h);
        lives_widget_process_updates(chooser);

        if (prefs->open_maximised) {
          lives_window_maximize(LIVES_WINDOW(chooser));
        }
      }
    } else {
      lives_window_maximize(LIVES_WINDOW(chooser));
    }
    lives_widget_process_updates(chooser);
  }
  return chooser;
}


LIVES_GLOBAL_INLINE LiVESWidget *make_autoreload_check(LiVESHBox * hbox, boolean is_active) {
  return lives_standard_check_button_new(_("_Autoreload next time"), is_active, LIVES_BOX(hbox), NULL);
}


boolean do_st_end_times_dlg(int clipno, double * start, double * end) {
  lives_clip_t *sfile;
  LiVESWidget *dialog = lives_standard_dialog_new(_("Start and End Times"), TRUE, -1, -1);
  LiVESWidget *dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(dialog));
  LiVESWidget *layout = lives_layout_new(LIVES_BOX(dialog_vbox));
  LiVESWidget *sttime, *entime, *cb_quant = NULL, *hbox;
  LiVESResponseType resp;

  if (!IS_VALID_CLIP(clipno)) return FALSE;
  sfile = mainw->files[clipno];

  lives_layout_set_row_spacings(LIVES_LAYOUT(layout), widget_opts.packing_height << 2);

  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
  sttime = lives_standard_spin_button_new(_("Start time (seconds)"), 0., 0., sfile->laudio_time,
                                          1., 1, 2, LIVES_BOX(hbox), NULL);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  entime = lives_standard_spin_button_new(_("End time (seconds)"), 0., 0., sfile->laudio_time,
                                          1., 1, 2, LIVES_BOX(hbox), NULL);

  spin_ranges_set_exclusive(LIVES_SPIN_BUTTON(sttime), LIVES_SPIN_BUTTON(entime), 0.);

  if (sfile->frames) {
    hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
    cb_quant = lives_standard_check_button_new(_("Snap to nearest frame start"), FALSE, LIVES_BOX(hbox), NULL);
  }

  lives_layout_add_fill(LIVES_LAYOUT(layout), FALSE);

  resp = lives_dialog_run(LIVES_DIALOG(dialog));
  if (resp == LIVES_RESPONSE_CANCEL) {
    return FALSE;
  }

  *start = lives_spin_button_get_value(LIVES_SPIN_BUTTON(sttime));
  *end = lives_spin_button_get_value(LIVES_SPIN_BUTTON(entime));

  if (sfile->frames && lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(cb_quant))) {
    *start = q_dbl(*start, sfile->fps);
    *end = q_dbl(*end, sfile->fps);
  }

  lives_widget_destroy(dialog);

  return TRUE;
}


//cancel/discard/save dialog
_entryw *create_cds_dialog(int type) {
  // values for type are:
  // 0 == leave multitrack, user pref is warn when leave multitrack
  // 1 == exit from LiVES, or save set
  //    -- called from function prompt_for_save_set()
  // 2 == for layouts loaded from disk, prompts for delete disk copy
  // 3 == wipe layout confirmation
  // 4 == prompt for render after recording / viewing in mt
  // called from
  //     check_for_layout_del()

  LiVESWidget *dialog_vbox;
  LiVESWidget *cancelbutton;
  LiVESWidget *discardbutton;
  LiVESWidget *savebutton = NULL;

  char *labeltext = NULL;

  _entryw *cdsw = (_entryw *)(lives_malloc(sizeof(_entryw)));

  cdsw->warn_checkbutton = NULL;

  if (type == 0) {
    if (!*mainw->multitrack->layout_name) {
      labeltext = lives_strdup(
                    _("You are about to leave multitrack mode.\n"
                      "The current layout has not been saved.\nWhat would you like to do ?\n"));
    } else {
      labeltext = lives_strdup(
                    _("You are about to leave multitrack mode.\n"
                      "The current layout has been changed since the last save.\n"
                      "What would you like to do ?\n"));
    }
  } else if (type == 1) {
    if (!mainw->only_close) labeltext = lives_strdup(
                                            _("You are about to exit LiVES.\n"
                                              "The current clip set can be saved.\n"
                                              "What would you like to do ?\n"));
    else labeltext = (_("The current clip set has not been saved.\nWhat would you like to do ?\n"));
  } else if (type == 2 || type == 3) {
    if ((mainw->multitrack && mainw->multitrack->changed) || (mainw->stored_event_list &&
        mainw->stored_event_list_changed)) {
      labeltext = (_("The current layout has not been saved.\nWhat would you like to do ?\n"));
    } else {
      labeltext = lives_strdup(
                    _("The current layout has *NOT BEEN CHANGED* since it was last saved.\n"
                      "What would you like to do ?\n"));
    }
  } else if (type == 4) {
    labeltext = lives_strdup(
                  _("You are about to leave multitrack mode.\n"
                    "The current layout contains generated frames and cannot be retained.\n"
                    "What do you wish to do ?"));
  }

  cdsw->dialog = create_question_dialog(_("Save or Delete Set"), labeltext);

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(cdsw->dialog));

  if (labeltext) lives_free(labeltext);

  if (type == 1) {
    LiVESWidget *checkbutton;
    LiVESWidget *hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

    cdsw->entry = lives_standard_entry_new(_("Clip set _name"), strlen(mainw->set_name)
                                           ? mainw->set_name : "",
                                           SHORT_ENTRY_WIDTH, 128, LIVES_BOX(hbox), NULL);

    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

    if (*future_prefs->workdir && lives_strcmp(future_prefs->workdir, prefs->workdir)) {
      prefs->ar_clipset = prefs->ar_layout = FALSE;
    } else {
      prefs->ar_clipset = !mainw->only_close;
      if (!mainw->only_close) {
        checkbutton = make_autoreload_check(LIVES_HBOX(hbox), prefs->ar_clipset);

        lives_signal_sync_connect(LIVES_GUI_OBJECT(checkbutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                                  LIVES_GUI_CALLBACK(toggle_sets_pref), (livespointer)PREF_AR_CLIPSET);
      }
    }
  }
  if (type == 0 && !(prefs->warning_mask & WARN_MASK_EXIT_MT)) {
    add_warn_check(LIVES_BOX(dialog_vbox), WARN_MASK_EXIT_MT, NULL);
  }

  cancelbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(cdsw->dialog), LIVES_STOCK_CANCEL, NULL,
                 LIVES_RESPONSE_CANCEL);

  lives_window_add_escape(LIVES_WINDOW(cdsw->dialog), cancelbutton);

  discardbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(cdsw->dialog), LIVES_STOCK_DELETE, NULL,
                  (type == 2) ? LIVES_RESPONSE_ABORT : LIVES_RESPONSE_RESET);

  if ((type == 0 && !*mainw->multitrack->layout_name) || type == 3 || type == 4)
    lives_button_set_label(LIVES_BUTTON(discardbutton), _("_Wipe layout"));
  else if (type == 0) lives_button_set_label(LIVES_BUTTON(discardbutton), _("_Ignore changes"));
  else if (type == 1) {
    if (mainw->was_set && prefs->workdir_tx_intent != LIVES_INTENTION_DELETE)
      lives_button_set_label(LIVES_BUTTON(discardbutton), _("_Delete clip set"));
    else
      lives_button_set_label(LIVES_BUTTON(discardbutton), _("_Discard all clips"));
  } else if (type == 2) lives_button_set_label(LIVES_BUTTON(discardbutton), _("_Delete layout"));

  if (prefs->workdir_tx_intent != LIVES_INTENTION_UNKNOWN) {
    if (prefs->workdir_tx_intent == LIVES_INTENTION_LEAVE) {
      savebutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(cdsw->dialog),
                   LIVES_STOCK_SAVE, _("Save in _Current Work Directory"),
                   LIVES_RESPONSE_ACCEPT);
    }

    lives_dialog_add_button_from_stock(LIVES_DIALOG(cdsw->dialog),
                                       LIVES_STOCK_GO_FORWARD, _("Transfer to _New Directory"),
                                       LIVES_RESPONSE_YES);
  } else {
    if (type != 4) savebutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(cdsw->dialog), LIVES_STOCK_SAVE, NULL,
                                  (type == 2) ? LIVES_RESPONSE_RETRY : LIVES_RESPONSE_ACCEPT);
    if (type == 0 || type == 3) lives_button_set_label(LIVES_BUTTON(savebutton), _("_Save layout"));
    else if (type == 1) lives_button_set_label(LIVES_BUTTON(savebutton), _("_Save clip set"));
    else if (type == 2) lives_button_set_label(LIVES_BUTTON(savebutton), _("_Wipe layout"));
    if (type == 1 || type == 2) lives_button_grab_default_special(savebutton);
  }
  lives_widget_show_all(cdsw->dialog);

  if (type == 1) {
    lives_widget_grab_focus(cdsw->entry);
  }

  if (!LIVES_IS_INTERACTIVE) lives_widget_set_sensitive(cancelbutton, FALSE);

  return cdsw;
}


static void flip_cdisk_bit(LiVESToggleButton * t, livespointer user_data) {
  uint32_t bitmask = LIVES_POINTER_TO_INT(user_data);
  prefs->clear_disk_opts ^= bitmask;
}


LiVESWidget *create_cleardisk_advanced_dialog(void) {
  LiVESWidget *dialog;
  LiVESWidget *dialog_vbox;
  LiVESWidget *scrollw;
  LiVESWidget *vbox;
  LiVESWidget *hbox;
  LiVESWidget *checkbutton;
  LiVESWidget *okbutton;

  int woat = widget_opts.apply_theme;

  char *tmp, *tmp2;

  dialog = lives_standard_dialog_new(_("Disk Recovery Options"), FALSE, DEF_DIALOG_WIDTH, DEF_DIALOG_HEIGHT);

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(dialog));

  vbox = lives_vbox_new(FALSE, 0);
  lives_container_set_border_width(LIVES_CONTAINER(vbox), widget_opts.border_width * 2);

  widget_opts.apply_theme = 0;
  scrollw = lives_standard_scrolled_window_new(DEF_DIALOG_WIDTH, DEF_DIALOG_HEIGHT, vbox);
  widget_opts.apply_theme = woat;

  lives_container_add(LIVES_CONTAINER(dialog_vbox), scrollw);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, TRUE, widget_opts.packing_height);

  checkbutton = lives_standard_check_button_new((tmp = (_("Check for Lost Clips"))),
                !(prefs->clear_disk_opts & LIVES_CDISK_REMOVE_ORPHAN_CLIPS), LIVES_BOX(hbox),
                (tmp2 = (H_("Enable attempted recovery of potential lost clips before deleting them.\n"
                            "Can be overridden after disk analysis."))));

  lives_free(tmp); lives_free(tmp2);

  lives_signal_sync_connect_after(LIVES_GUI_OBJECT(checkbutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                                  LIVES_GUI_CALLBACK(flip_cdisk_bit),
                                  LIVES_INT_TO_POINTER(LIVES_CDISK_REMOVE_ORPHAN_CLIPS));

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, TRUE, widget_opts.packing_height);

  checkbutton = lives_standard_check_button_new((tmp = (_("Remove Empty Directories"))),
                !(prefs->clear_disk_opts & LIVES_CDISK_LEAVE_EMPTY_DIRS), LIVES_BOX(hbox),
                (tmp2 = (H_("Remove any empty directories within the working directory"))));

  lives_free(tmp); lives_free(tmp2);

  lives_signal_sync_connect_after(LIVES_GUI_OBJECT(checkbutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                                  LIVES_GUI_CALLBACK(flip_cdisk_bit),
                                  LIVES_INT_TO_POINTER(LIVES_CDISK_LEAVE_ORPHAN_SETS));

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, TRUE, widget_opts.packing_height);

  checkbutton = lives_standard_check_button_new((tmp = (_("Delete _Orphaned Clips"))),
                !(prefs->clear_disk_opts & LIVES_CDISK_LEAVE_ORPHAN_SETS), LIVES_BOX(hbox),
                (tmp2 = (H_("Delete any clips which are not currently loaded or part of a set\n"
                            "If 'Check for Lost Clips' is set, LiVES will try to recover them first"))));

  lives_free(tmp); lives_free(tmp2);

  lives_signal_sync_connect_after(LIVES_GUI_OBJECT(checkbutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                                  LIVES_GUI_CALLBACK(flip_cdisk_bit),
                                  LIVES_INT_TO_POINTER(LIVES_CDISK_LEAVE_ORPHAN_SETS));

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, TRUE, widget_opts.packing_height);

  checkbutton = lives_standard_check_button_new(_("Clear Useless  _Backup Files from Clips"),
                !(prefs->clear_disk_opts & LIVES_CDISK_LEAVE_BFILES), LIVES_BOX(hbox), NULL);

  lives_signal_sync_connect_after(LIVES_GUI_OBJECT(checkbutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                                  LIVES_GUI_CALLBACK(flip_cdisk_bit),
                                  LIVES_INT_TO_POINTER(LIVES_CDISK_LEAVE_BFILES));

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, TRUE, widget_opts.packing_height);

  checkbutton = lives_standard_check_button_new(_("Remove Sets which have _Layouts but no Clips"),
                (prefs->clear_disk_opts & LIVES_CDISK_REMOVE_ORPHAN_LAYOUTS), LIVES_BOX(hbox), NULL);

  lives_signal_sync_connect_after(LIVES_GUI_OBJECT(checkbutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                                  LIVES_GUI_CALLBACK(flip_cdisk_bit),
                                  LIVES_INT_TO_POINTER(LIVES_CDISK_REMOVE_ORPHAN_LAYOUTS));

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, TRUE, widget_opts.packing_height);

  checkbutton = lives_standard_check_button_new(_("Remove temporary staging directories"),
                !(prefs->clear_disk_opts & LIVES_CDISK_LEAVE_STAGING_DIRS), LIVES_BOX(hbox),
                H_("Staging directories may be used during the very early process of opening a file.\n"
                   "Any remnants are unlikely to be useful, and will in any case eventually be removed "
                   "by the operating system."));

  lives_signal_sync_connect_after(LIVES_GUI_OBJECT(checkbutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                                  LIVES_GUI_CALLBACK(flip_cdisk_bit),
                                  LIVES_INT_TO_POINTER(LIVES_CDISK_LEAVE_STAGING_DIRS));


  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, TRUE, widget_opts.packing_height);

  checkbutton = lives_standard_check_button_new(_("Check files marked as 'Ignore'"),
                !(prefs->clear_disk_opts & LIVES_CDISK_LEAVE_STAGING_DIRS), LIVES_BOX(hbox),
                H_("In rare circumstances it is not possible to reload a clip during normal startup\n"
                   "Such clips may be marked as 'Ignored' to prevent them from constantly trying to be reloaded.\n"
                   "Enabling this option allows the cleanup / recovery process to consider these files\n"
                   "again and make another attempt to recover them."));

  lives_signal_sync_connect_after(LIVES_GUI_OBJECT(checkbutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                                  LIVES_GUI_CALLBACK(flip_cdisk_bit),
                                  LIVES_INT_TO_POINTER(LIVES_CDISK_CONSIDER_IGNORED));

  // resetbutton
  lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_REFRESH, _("_Reset to Defaults"),
                                     LIVES_RESPONSE_RESET);

  okbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_OK, NULL,
             LIVES_RESPONSE_OK);

  lives_button_grab_default_special(okbutton);

  return dialog;
}


#ifdef GTK_TEXT_VIEW_DRAW_BUG

static ulong expt;

static boolean exposetview(LiVESWidget * widget, lives_painter_t *cr, livespointer user_data) {
  LiVESWidgetColor fgcol, bgcol;
  lives_colRGBA64_t fg, bg;
  LingoLayout *layout = NULL;
  lives_painter_surface_t *surface;
  char *text = lives_text_view_get_text(LIVES_TEXT_VIEW(widget));
  double top = 0;
  int offsx = 0;
  int height = lives_widget_get_allocation_height(widget);

  lives_signal_handler_block(widget, expt);

  surface = lives_painter_get_target(cr);
  lives_painter_surface_flush(surface);

  lives_widget_get_fg_state_color(widget, lives_widget_get_state(widget), &fgcol);
  lives_widget_get_bg_state_color(widget, lives_widget_get_state(widget), &bgcol);

  widget_color_to_lives_rgba(&fg, &fgcol);
  widget_color_to_lives_rgba(&bg, &bgcol);

  layout = render_text_to_cr(widget, cr, text, "", 0.0,
                             LIVES_TEXT_MODE_FOREGROUND_ONLY, &fg, &bg, FALSE, FALSE, &top, &offsx,
                             lives_widget_get_allocation_width(widget), &height);

  lives_free(text);

  if (layout) {
    if (LINGO_IS_LAYOUT(layout)) {
      lingo_painter_show_layout(cr, layout);
    }
    lives_widget_object_unref(layout);
    //if (LIVES_IS_WIDGET_OBJECT(layout)) lives_widget_object_unref(layout);
  }

  //lives_painter_fill(cr);

  lives_signal_handler_unblock(widget, expt);

  return FALSE;
}

#endif


LiVESTextView *create_output_textview(void) {
  LiVESWidget *textview = lives_standard_text_view_new(NULL, NULL);

#ifdef GTK_TEXT_VIEW_DRAW_BUG
  expt = lives_signal_sync_connect(LIVES_GUI_OBJECT(textview), LIVES_WIDGET_EXPOSE_EVENT,
                                   LIVES_GUI_CALLBACK(exposetview), NULL);
#endif
  lives_widget_object_ref(textview);
  return LIVES_TEXT_VIEW(textview);
}


static int currow;

static void pair_add(LiVESWidget * table, const char *key, const char *meaning) {
  LiVESWidget *labelk, *labelm, *align;
  double kalign = 0., malign = 0.;
  boolean key_all = FALSE;

  if (!key) {
    // NULL, NULL ->  hsep all TODO
    if (!meaning) {
      labelk = lives_standard_hseparator_new();
      key_all = TRUE;
    } else {
      if (*meaning) {
        // NULL, meaning -> centered meaning; hsep key
        pair_add(table, meaning, "");
        pair_add(table, NULL, "");
        return;
      } else {
        // NULL, "" -> hsep key
        labelk = lives_standard_hseparator_new();
        labelm = lives_standard_label_new("");
      }
    }
  } else {
    if (!*key) {
      // "", NULL -> hsep meaning
      if (!meaning) {
        labelk = lives_standard_label_new("");
        labelm = lives_standard_hseparator_new();
      } else {
        if (!*meaning) {
          //// "", "" -> newline
          labelk = lives_standard_label_new("");
          labelm = lives_standard_label_new("");
        } else {
          /// "", meaning -> "" | centered meaning
          labelk = lives_standard_label_new("");
          labelm = lives_standard_label_new(meaning);
          malign = .5;
        }
      }
    } else {
      // key, NULL ->   all centered key
      if (!meaning) {
        labelk = lives_standard_label_new(key);
        kalign = .5;
        key_all = TRUE;
      } else {
        // key, meaning ->  key | meaning
        if (*meaning) {
          labelk = lives_standard_label_new(key);
          labelm = lives_standard_label_new(meaning);
        } else {
          // key, "" -> center key
          labelk = lives_standard_label_new(key);
          labelm = lives_standard_label_new(" ");
          kalign = .5;
	  // *INDENT-OFF*
        }}}}
  // *INDENT-ON*

  align = lives_alignment_new(kalign, .5, 1., 0.);
  lives_container_add(LIVES_CONTAINER(align), labelk);

  if (!key_all) {
    lives_table_attach(LIVES_TABLE(table), align, 0, 1, currow, currow + 1,
                       (LiVESAttachOptions)(LIVES_FILL),
                       (LiVESAttachOptions)(0), 0, 0);

    align = lives_alignment_new(malign, .5, 0., 0.);
    lives_container_add(LIVES_CONTAINER(align), labelm);

    lives_table_attach(LIVES_TABLE(table), align, 1, 40, currow, currow + 1,
                       (LiVESAttachOptions)(LIVES_EXPAND),
                       (LiVESAttachOptions)(0), 0, 0);
  } else {
    lives_table_attach(LIVES_TABLE(table), align, 0, 40, currow, currow + 1,
                       (LiVESAttachOptions)(LIVES_EXPAND | LIVES_FILL),
                       (LiVESAttachOptions)(0), 0, 0);
  }

  currow++;

  lives_widget_show_all(table);
}


#define ADD_KEYDEF(key, desc) pair_add(textwindow->table, (tmp = lives_strdup(key)), (tmp2 = lives_strdup(desc))); \
  lives_free(tmp); lives_free(tmp2)

void do_keys_window(void) {
  char *tmp = (_("Show Keys")), *tmp2;
  text_window *textwindow;

  widget_opts.expand = LIVES_EXPAND_EXTRA_WIDTH | LIVES_EXPAND_DEFAULT_HEIGHT;
  textwindow = create_text_window(tmp, NULL, NULL, TRUE);
  lives_free(tmp);
  widget_opts.expand = LIVES_EXPAND_DEFAULT;;

  lives_table_resize(LIVES_TABLE(textwindow->table), 1, 40);
  currow = 0;

  ADD_KEYDEF(_("You can use the following keys during playback to control LiVES:-"), NULL);
  ADD_KEYDEF(NULL, NULL);
  ADD_KEYDEF(NULL, _("Recordable keys (press 'r' before or during playback to toggle recording)"));
  ADD_KEYDEF(_("ctrl + left"), _("skip / scratch backwards\nWhen not playing moves the playback cursor"));
  ADD_KEYDEF(_("ctrl + right"), _("skip / scratch forwards\nWhen not playing moves the playback cursor"));
  ADD_KEYDEF(_("ctrl + up"), _("play faster"));
  ADD_KEYDEF(_("ctrl + down"), _("play slower"));
  ADD_KEYDEF(_("shift + up"), _("background clip play faster"));
  ADD_KEYDEF(_("shift + down"), _("background clip play slower"));
  ADD_KEYDEF(_("(The 'effect parameter' here is the first 'simple' numerical parameter)"), NULL);
  ADD_KEYDEF(_("alt + up"), _("increase effect parameter for keygrabbed effect"));
  ADD_KEYDEF(_("alt + down"), _("decrease effect parameter for keygrabbed effect"));
  ADD_KEYDEF(_("ctrl + enter"), _("reset frame rate / resync audio (foreground clip)"));
  ADD_KEYDEF(_("shift + enter"), _("reset frame rate (background clip)"));
  ADD_KEYDEF(_("ctrl + space"), _("reverse direction (foreground clip)"));
  ADD_KEYDEF(_("ctrl + shift + space"), _("reverse direction (background clip)"));
  ADD_KEYDEF(_("ctrl + alt + space"),
             _("Loop Lock\n(press once to mark IN point, then again to mark OUT point;\n"
               "ctrl-space, ctrl-enter, or switching clips clears)"));
  ADD_KEYDEF(_("ctrl + backspace"), _("freeze frame (foreground and background)"));
  ADD_KEYDEF(_("ctrl + alt + backspace"), _("freeze frame (background clip only)"));
  ADD_KEYDEF("x", _("swap background / foreground clips"));

  ADD_KEYDEF(NULL, _("The following may also be used outside of playback:-"));
  ADD_KEYDEF("n", _("nervous mode"));
  ADD_KEYDEF(_("ctrl + page-up"), _("previous clip"));
  ADD_KEYDEF(_("ctrl + page-down"), _("next clip"));
  ADD_KEYDEF("", "");
  ADD_KEYDEF(_("ctrl + 1"), _("toggle real-time effect 1"));
  ADD_KEYDEF(_("ctrl + 2"), _("toggle real-time effect 2"));
  ADD_KEYDEF(_("...etc..."), "");
  ADD_KEYDEF(_("ctrl + 9"), _("toggle real-time effect 9"));
  ADD_KEYDEF(_("ctrl + 0"), _("real-time effects (1 - 9) OFF"));
  ADD_KEYDEF(_("ctrl + minus"), _("toggle real-time effect 10 (unaffected by ctrl-0)"));
  ADD_KEYDEF(_("ctrl + equals"), _("toggle real-time effect 11 (unaffected by ctrl-0)"));
  ADD_KEYDEF("", "");
  ADD_KEYDEF("a",
             _("audio lock ON: lock audio to the current foreground clip;\nignore video clip switches and rate / direction changes"));
  ADD_KEYDEF("A (shift + a)", _("audio lock OFF; audio follows the foreground video clip\n(unless overridden in Preferences)"));
  ADD_KEYDEF("k", _("grab keyboard for last activated effect key\n(affects m, M, t, tab and ctrl-alt-up, ctrl-alt-down keys)"));
  ADD_KEYDEF("m", _("next effect mode (for whichever key has keyboard grab)"));
  ADD_KEYDEF("M (shift + m)", _("previous effect mode (for whichever key has keyboard grab)"));
  ADD_KEYDEF(_("ctrl + alt + 1"), _("grab keyboard for effect key 1 (similar to k key)"));
  ADD_KEYDEF(_("ctrl + alt + 2"), _("grab keyboard for effect key 2"));
  ADD_KEYDEF(_("...etc..."), "");
  ADD_KEYDEF("t", _("enter text parameter (when effect has keyboard grab)"));
  ADD_KEYDEF(_("TAB"), _("leave text parameter (reverse of 't')"));
  ADD_KEYDEF(_("F1"), _("store/switch to bookmark 1 (first press stores clip and frame)"));
  ADD_KEYDEF(_("shift + F1"), _("clear bookmark 1"));
  ADD_KEYDEF(_("F2"), _("store/switch to bookmark 2"));
  ADD_KEYDEF(_("shift + F2"), _("clear bookmark 2"));
  ADD_KEYDEF(_("...etc..."), "");
  ADD_KEYDEF(_("F12"), _("clear function keys (bookmarks)"));
  ADD_KEYDEF("", "");
  ADD_KEYDEF(NULL, _("Other playback keys"));
  ADD_KEYDEF("p", _("play all"));
  ADD_KEYDEF("y", _("play selection"));
  ADD_KEYDEF("q", _("stop"));
  ADD_KEYDEF("f", _("fullscreen"));
  ADD_KEYDEF("s", _("separate window"));
  ADD_KEYDEF("d", _("double sized playarea (only in clip edit mode)"));
  ADD_KEYDEF("r", _("toggle recording mode (clip edit mode only)"));
  ADD_KEYDEF("b", _("blank / unblank the interface background (clip editor only)"));
  ADD_KEYDEF("o", _("activate / deactivate continuous looping"));
  ADD_KEYDEF("g", _("enable / disable ping pong looping"));
  ADD_KEYDEF("l", _("enable / disable stop on audio end\n(ignored if continuous loop is active)"));
  ADD_KEYDEF("<", _("lower the volume of current audio clip"));
  ADD_KEYDEF(">", _("increase the volume of current audio clip"));
  ADD_KEYDEF("w", _("display a/v sync status (developer mode)"));
  ADD_KEYDEF("", "");
}


void do_mt_keys_window(void) {
  text_window *textwindow;
  char *tmp = (_("Multitrack Keys")), *tmp2;

  widget_opts.expand = LIVES_EXPAND_EXTRA_WIDTH | LIVES_EXPAND_DEFAULT_HEIGHT;
  textwindow = create_text_window(tmp, NULL, NULL, TRUE);
  lives_free(tmp);
  widget_opts.expand = LIVES_EXPAND_DEFAULT;;

  lives_table_resize(LIVES_TABLE(textwindow->table), 1, 40);
  currow = 0;

  ADD_KEYDEF(_("You can use the following keys to control the multitrack window:"), NULL);
  ADD_KEYDEF(NULL, NULL);
  ADD_KEYDEF(_("ctrl + left-arrow"), _("move timeline cursor left 1 second"));
  ADD_KEYDEF(_("ctrl + right-arrow"), _("move timeline cursor right 1 second"));
  ADD_KEYDEF(_("shift + left-arrow"), _("move timeline cursor left 1 frame"));
  ADD_KEYDEF(_("shift + right-arrow"), _("move timeline cursor right 1 frame"));
  ADD_KEYDEF(_("ctrl + up-arrow"), _("move current track up"));
  ADD_KEYDEF(_("ctrl + down-arrow"), _("move current track down"));
  ADD_KEYDEF(_("ctrl + page-up"), _("select previous clip"));
  ADD_KEYDEF(_("ctrl + page-down"), _("select next clip"));
  ADD_KEYDEF(_("ctrl + space"), _("select/deselect current track"));
  ADD_KEYDEF(_("ctrl + plus"), _("zoom in"));
  ADD_KEYDEF(_("ctrl + minus"), _("zoom out"));
  ADD_KEYDEF("m", _("make a mark on the timeline (during playback)"));
  ADD_KEYDEF("w", _("rewind to play start."));
  ADD_KEYDEF("", "");
  ADD_KEYDEF("", _("For other keys, see the menus.\n"));
  ADD_KEYDEF("", "");
}


autolives_window *autolives_pre_dialog(void) {
  // dialog for autolives activation
  // options: trigger: auto, time
  //                   omc - user1

  // TODO: add port numbers, add change types and probabilities.
  autolives_window *alwindow;

  LiVESWidget *trigframe;
  LiVESWidget *dialog_vbox;
  LiVESWidget *layout;
  LiVESWidget *vbox;
  LiVESWidget *hbox;
  LiVESWidget *radiobutton;

  LiVESSList *radiobutton1_group = NULL;
  LiVESSList *radiobutton2_group = NULL;

  char *tmp, *tmp2;

  alwindow = (autolives_window *)lives_malloc(sizeof(autolives_window));

  alwindow->dialog = lives_standard_dialog_new(_("Autolives Options"), TRUE, DEF_DIALOG_WIDTH, DEF_DIALOG_HEIGHT);

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(alwindow->dialog));

  trigframe = lives_standard_frame_new(_("Trigger"), 0., FALSE);

  lives_box_pack_start(LIVES_BOX(dialog_vbox), trigframe, FALSE, FALSE, widget_opts.packing_height);

  vbox = lives_vbox_new(FALSE, 0);
  lives_container_set_border_width(LIVES_CONTAINER(vbox), widget_opts.border_width);
  lives_container_add(LIVES_CONTAINER(trigframe), vbox);

  hbox = lives_hbox_new(FALSE, widget_opts.packing_width);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, TRUE, TRUE, widget_opts.packing_height);
  alwindow->atrigger_button = lives_standard_radio_button_new((tmp = (_("Timed:"))),
                              &radiobutton1_group, LIVES_BOX(hbox),
                              (tmp2 = (_("Trigger changes at regular intervals based on time"))));

  lives_free(tmp); lives_free(tmp2);

  alwindow->atrigger_spin = lives_standard_spin_button_new(_("change time (seconds)"), 1., 1., 1800.,
                            1., 10., 0, LIVES_BOX(hbox), NULL);

  hbox = lives_hbox_new(FALSE, widget_opts.packing_width);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, TRUE, TRUE, widget_opts.packing_height);
  radiobutton = lives_standard_radio_button_new((tmp = (_("OMC"))),
                &radiobutton1_group, LIVES_BOX(hbox),
                (tmp2 = (H_("Trigger changes based on receiving OSC messages\n"
                            "using the OMC (Open Media Control) syntax.\n"
                            "OMC triggers can be connected to output parameters of filters\n"
                            "in the real time effect mapper window,\n"
                            "as well as in the MIDI / joystick learner\n"))));

  lives_free(tmp); lives_free(tmp2);

  if (has_devicemap(OSC_NOTIFY)) {
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(radiobutton), TRUE);
  }

  layout = lives_layout_new(LIVES_BOX(dialog_vbox));
  lives_layout_add_label(LIVES_LAYOUT(layout), _("Playback start:"), TRUE);
  lives_layout_add_fill(LIVES_LAYOUT(layout), TRUE);
  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  alwindow->apb_button = lives_standard_radio_button_new((tmp = (_("Automatic"))),
                         &radiobutton2_group, LIVES_BOX(hbox),
                         (tmp2 = (H_("Start playback automatically"))));

  lives_free(tmp); lives_free(tmp2);

  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
  radiobutton = lives_standard_radio_button_new((tmp = (_("Manual"))),
                &radiobutton2_group, LIVES_BOX(hbox),
                (tmp2 = (H_("Wait for the user to start playback"))));

  lives_free(tmp); lives_free(tmp2);

  lives_layout_add_separator(LIVES_LAYOUT(layout), FALSE);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  alwindow->mute_button = lives_standard_check_button_new
                          ((tmp = (_("Mute internal audio during playback"))), FALSE, LIVES_BOX(hbox),
                           (tmp2 = (_("Mute the audio in LiVES during playback by setting the "
                                      "audio source to external."))));
  lives_free(tmp); lives_free(tmp2);

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(alwindow->mute_button), TRUE);

  lives_layout_add_separator(LIVES_LAYOUT(layout), FALSE);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  alwindow->debug_button = lives_standard_check_button_new
                           ((tmp = (_("Debug mode"))), FALSE, LIVES_BOX(hbox),
                            (tmp2 = (_("Show debug output on stderr."))));

  lives_free(tmp); lives_free(tmp2);

  lives_widget_show_all(alwindow->dialog);
  return alwindow;
}


static boolean special_cleanup_cb(LiVESWidget * widget, void *userdata) {
  // need to call special_cleanup(TRUE) before destroying the toplevel if you want to prompt
  // for filewrite overwrites
  special_cleanup(FALSE);
  return FALSE;
}


const lives_special_aspect_t *add_aspect_ratio_button(LiVESSpinButton * sp_width, LiVESSpinButton * sp_height, LiVESBox * box) {
  static lives_param_t aspect_width, aspect_height;

  init_special();

  aspect_width.widgets[0] = (LiVESWidget *)sp_width;
  aspect_height.widgets[0] = (LiVESWidget *)sp_height;

  set_aspect_ratio_widgets(&aspect_width, &aspect_height);

  check_for_special(NULL, &aspect_width, box);
  check_for_special(NULL, &aspect_height, box);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(sp_width), LIVES_WIDGET_DESTROY_SIGNAL,
                            LIVES_GUI_CALLBACK(special_cleanup_cb), NULL);

  return paramspecial_get_aspect();
}


LiVESWidget *add_list_expander(LiVESBox * box, const char *title, int width, int height, LiVESList * xlist) {
  // add widget to preview affected layouts

  LiVESWidget *expander;
  LiVESWidget *textview = lives_text_view_new();
  LiVESTextBuffer *textbuffer = lives_text_view_get_buffer(LIVES_TEXT_VIEW(textview));

  LiVESWidget *scrolledwindow =
    lives_standard_scrolled_window_new(width, height, LIVES_WIDGET(textview));

  lives_text_view_set_editable(LIVES_TEXT_VIEW(textview), FALSE);

  lives_widget_set_size_request(scrolledwindow, width, height);

  expander = lives_standard_expander_new(title, NULL, LIVES_BOX(box), scrolledwindow);

  if (palette->style & STYLE_1) {
    LiVESWidget *label = lives_expander_get_label_widget(LIVES_EXPANDER(expander));
    lives_widget_set_fg_color(label, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
    lives_widget_set_fg_color(label, LIVES_WIDGET_STATE_PRELIGHT, &palette->normal_fore);
    lives_widget_set_fg_color(expander, LIVES_WIDGET_STATE_PRELIGHT, &palette->normal_fore);
    lives_widget_set_bg_color(expander, LIVES_WIDGET_STATE_PRELIGHT, &palette->normal_back);

    lives_widget_set_base_color(textview, LIVES_WIDGET_STATE_NORMAL, &palette->info_base);
    lives_widget_set_text_color(textview, LIVES_WIDGET_STATE_NORMAL, &palette->info_text);
    lives_widget_set_base_color(scrolledwindow, LIVES_WIDGET_STATE_NORMAL, &palette->info_base);
    lives_widget_set_text_color(scrolledwindow, LIVES_WIDGET_STATE_NORMAL, &palette->info_text);
  }

  lives_text_buffer_insert_at_cursor(textbuffer, "\n", strlen("\n"));

  for (; xlist; xlist = xlist->next) {
    lives_text_buffer_insert_at_cursor(textbuffer, (const char *)xlist->data, strlen((char *)xlist->data));
    lives_text_buffer_insert_at_cursor(textbuffer, "\n", strlen("\n"));
  }
  return expander;
}


LIVES_GLOBAL_INLINE boolean do_utube_stream_warn(void) {
  return do_yesno_dialog(_("The selected clip appears to be a stream.\nThe output will be continuously saved "
                           "until either the Enough or Cancel button is clicked.\n\n"
                           "It is also likely that the file obtained will not be in a useable format\n"
                           "Do you wish to continue with the download anyway ?"));
}


#ifdef ALLOW_NONFREE_CODECS
static void on_freedom_toggled(LiVESToggleButton * togglebutton, livespointer user_data) {
  LiVESWidget *label = (LiVESWidget *)user_data;
  if (!lives_toggle_button_get_active(togglebutton)) lives_label_set_text(LIVES_LABEL(label), "." LIVES_FILE_EXT_WEBM);
  else lives_label_set_text(LIVES_LABEL(label), "." LIVES_FILE_EXT_MP4);
}
#endif

static LiVESWidget *spinbutton_width;
static LiVESWidget *spinbutton_height;
static LiVESWidget *px_label;
static LiVESWidget *memo_check;
static const lives_special_aspect_t *aspect;

static void utsense(LiVESToggleButton * togglebutton, livespointer user_data) {
  boolean sensitive = (boolean)LIVES_POINTER_TO_INT(user_data);
  if (!lives_toggle_button_get_active(togglebutton)) return;
  if (spinbutton_width)
    lives_widget_set_sensitive(spinbutton_width, sensitive);
  if (spinbutton_height)
    lives_widget_set_sensitive(spinbutton_height, sensitive);
  if (px_label)
    lives_widget_set_sensitive(px_label, sensitive);
  if (aspect) lives_widget_set_sensitive(aspect->lockbutton, sensitive);
}


LIVES_GLOBAL_INLINE int rbgroup_get_data(LiVESSList * rbgroup, const char *key, int def) {
  for (LiVESSList *list = rbgroup; list; list = list->next) {
    if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(list->data)))
      def =  GET_INT_DATA(list->data, key);
  }
  if (memo_check && lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(memo_check)))
    def = -def;

  return def;
}


LiVESSList *add_match_methods(LiVESLayout * layout, char *mopts, int height_step,
                              int width_step, boolean add_aspect) {
  LiVESSList *rbgroup = NULL;
  LiVESWidget *radiobutton;
  LiVESWidget *hbox;
  char *tmp, *tmp2;
  int n_added = 0;
  int woph = widget_opts.packing_height;
  widget_opts.packing_height >>= 1;

  if (mopts[LIVES_MATCH_NEAREST]) {
    hbox = lives_layout_row_new(layout);
    radiobutton = lives_standard_radio_button_new((tmp = (_("- Approximately:"))),
                  &rbgroup, LIVES_BOX(hbox),
                  (tmp2 = (_("Select the closest to this size"))));

    lives_free(tmp); lives_free(tmp2);

    lives_signal_sync_connect(LIVES_GUI_OBJECT(radiobutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                              LIVES_GUI_CALLBACK(utsense), LIVES_INT_TO_POINTER(TRUE));
    SET_INT_DATA(radiobutton, MATCHTYPE_KEY, LIVES_MATCH_NEAREST);

    if (mopts[LIVES_MATCH_NEAREST] == MATCH_TYPE_DEFAULT)
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(radiobutton), TRUE);

    n_added++;
  }

  if (mopts[LIVES_MATCH_AT_LEAST]) {
    if (n_added) hbox = lives_layout_hbox_new(layout);
    else hbox = lives_layout_row_new(layout);

    radiobutton = lives_standard_radio_button_new((tmp = (_("- At _least"))), &rbgroup,
                  LIVES_BOX(hbox),
                  (tmp2 = (_("Frame size should be at least this size"))));
    lives_free(tmp); lives_free(tmp2);

    lives_signal_sync_connect(LIVES_GUI_OBJECT(radiobutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                              LIVES_GUI_CALLBACK(utsense), LIVES_INT_TO_POINTER(TRUE));
    SET_INT_DATA(radiobutton, MATCHTYPE_KEY, LIVES_MATCH_AT_LEAST);

    if (mopts[LIVES_MATCH_AT_LEAST] == MATCH_TYPE_DEFAULT)
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(radiobutton), TRUE);

    n_added++;
  }

  if (mopts[LIVES_MATCH_AT_MOST]) {
    if (n_added) hbox = lives_layout_hbox_new(layout);
    else hbox = lives_layout_row_new(layout);

    radiobutton = lives_standard_radio_button_new((tmp = (_("- At _most:"))), &rbgroup,
                  LIVES_BOX(hbox),
                  (tmp2 = (_("Frame size should be at most this size"))));
    lives_free(tmp); lives_free(tmp2);

    lives_signal_sync_connect(LIVES_GUI_OBJECT(radiobutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                              LIVES_GUI_CALLBACK(utsense), LIVES_INT_TO_POINTER(TRUE));

    SET_INT_DATA(radiobutton, MATCHTYPE_KEY, LIVES_MATCH_AT_MOST);

    if (mopts[LIVES_MATCH_AT_MOST] == MATCH_TYPE_DEFAULT)
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(radiobutton), TRUE);

    n_added++;
  }

  if (n_added) {
    lives_layout_add_fill(layout, FALSE);
    hbox = lives_layout_row_new(layout);

    spinbutton_width = lives_standard_spin_button_new(_("_Width"),
                       CURRENT_CLIP_HAS_VIDEO ? cfile->hsize : DEF_GEN_WIDTH,
                       width_step, 100000., width_step, width_step, 0, LIVES_BOX(hbox), NULL);

    lives_spin_button_set_snap_to_multiples(LIVES_SPIN_BUTTON(spinbutton_width), width_step);
    lives_spin_button_update(LIVES_SPIN_BUTTON(spinbutton_width));
    lives_widget_nullify_with(LIVES_WIDGET(layout), (void **)&spinbutton_width);

    widget_opts.justify = LIVES_JUSTIFY_CENTER;
    hbox = lives_layout_hbox_new(layout);
    spinbutton_height = lives_standard_spin_button_new(_("X\t_Height"),
                        CURRENT_CLIP_HAS_VIDEO ? cfile->vsize : DEF_GEN_HEIGHT,
                        height_step, 100000., height_step, height_step, 0, LIVES_BOX(hbox), NULL);
    widget_opts.justify = LIVES_JUSTIFY_DEFAULT;

    lives_spin_button_set_snap_to_multiples(LIVES_SPIN_BUTTON(spinbutton_height), height_step);
    lives_spin_button_update(LIVES_SPIN_BUTTON(spinbutton_height));
    lives_widget_nullify_with(LIVES_WIDGET(layout), (void **)&spinbutton_height);

    widget_opts.expand = LIVES_EXPAND_DEFAULT_HEIGHT;
    px_label = lives_layout_add_label(layout, _("pixels"), TRUE);
    widget_opts.expand = LIVES_EXPAND_DEFAULT;
    lives_widget_nullify_with(LIVES_WIDGET(layout), (void **)&px_label);

    // add "aspectratio" widget
    if (add_aspect) {
      hbox = lives_layout_hbox_new(layout);
      aspect = add_aspect_ratio_button(LIVES_SPIN_BUTTON(spinbutton_width),
                                       LIVES_SPIN_BUTTON(spinbutton_height), LIVES_BOX(hbox));
      lives_widget_nullify_with(LIVES_WIDGET(layout), (void **)&aspect);
    } else aspect = NULL;

    lives_layout_add_fill(layout, FALSE);
    lives_layout_add_row(layout);

    lives_layout_add_label(layout, _(" OR:"), TRUE);
  } else {
    spinbutton_width = spinbutton_height = px_label = NULL;
    aspect = NULL;
  }

  if (mopts[LIVES_MATCH_LOWEST]) {
    if (n_added) hbox = lives_layout_hbox_new(layout);
    else hbox = lives_layout_row_new(layout);

    radiobutton = lives_standard_radio_button_new((tmp = (_("- Pick the _smallest"))),
                  &rbgroup, LIVES_BOX(hbox),
                  (tmp2 = (_("Select the lowest resolution available"))));

    lives_free(tmp); lives_free(tmp2);

    lives_signal_sync_connect(LIVES_GUI_OBJECT(radiobutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                              LIVES_GUI_CALLBACK(utsense), LIVES_INT_TO_POINTER(FALSE));

    SET_INT_DATA(radiobutton, MATCHTYPE_KEY, LIVES_MATCH_LOWEST);

    if (mopts[LIVES_MATCH_LOWEST] == MATCH_TYPE_DEFAULT)
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(radiobutton), TRUE);

    n_added++;
  }

  if (mopts[LIVES_MATCH_HIGHEST]) {
    if (n_added) hbox = lives_layout_hbox_new(layout);
    else hbox = lives_layout_row_new(layout);

    radiobutton = lives_standard_radio_button_new((tmp = (_("- Pick the _largest"))),
                  &rbgroup, LIVES_BOX(hbox),
                  (tmp2 = (_("Select the highest resolution available"))));

    lives_free(tmp); lives_free(tmp2);

    lives_signal_sync_connect(LIVES_GUI_OBJECT(radiobutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                              LIVES_GUI_CALLBACK(utsense), LIVES_INT_TO_POINTER(FALSE));

    SET_INT_DATA(radiobutton, MATCHTYPE_KEY, LIVES_MATCH_HIGHEST);

    if (mopts[LIVES_MATCH_HIGHEST] == MATCH_TYPE_DEFAULT)
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(radiobutton), TRUE);

    n_added++;
  }

  if (mopts[LIVES_MATCH_CHOICE]) {
    if (n_added) {
      lives_layout_add_fill(layout, FALSE);
      hbox = lives_layout_row_new(layout);
    } else hbox = lives_layout_row_new(layout);

    radiobutton = lives_standard_radio_button_new((tmp = (_("- Let me choose..."))),
                  &rbgroup, LIVES_BOX(hbox),
                  (tmp2 = (_("Choose the resolution from a list (opens in new window)"))));

    lives_free(tmp); lives_free(tmp2);

    lives_signal_sync_connect(LIVES_GUI_OBJECT(radiobutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                              LIVES_GUI_CALLBACK(utsense), LIVES_INT_TO_POINTER(FALSE));

    SET_INT_DATA(radiobutton, MATCHTYPE_KEY, LIVES_MATCH_CHOICE);

    if (mopts[LIVES_MATCH_CHOICE] == MATCH_TYPE_DEFAULT)
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(radiobutton), TRUE);

    n_added++;
  }

  if (n_added > 1) {
    lives_layout_add_fill(layout, FALSE);
    hbox = lives_layout_row_new(layout);
    memo_check = lives_standard_check_button_new(_("Remember my choice"),
                 FALSE, LIVES_BOX(hbox), NULL);
  } else memo_check = NULL;

  widget_opts.packing_height = woph;

  return rbgroup;
}


static void dl_url_changed(LiVESWidget * urlw, livespointer user_data) {
  LiVESWidget *namew = (LiVESWidget *)user_data;
  static size_t oldlen = 0;
  size_t ulen = lives_strlen(lives_entry_get_text(LIVES_ENTRY(urlw)));
  if (!(*(lives_entry_get_text(LIVES_ENTRY(namew))))) {
    if (ulen > oldlen + 1) lives_widget_grab_focus(namew);
  }
  oldlen = ulen;
}


static void on_utupinfo_clicked(LiVESWidget * b, livespointer data) {
  do_info_dialogf(_("LiVES will only update %s if you have a local user copy installed.\n"
                    "Otherwise you may need to update it manually when prompted\n\n"
                    "Checking the button for the first time will cause the program to be copied\n"
                    "to your home directory.\n"
                    "After this it can be updated without needing root privileges.\n"),
                  EXEC_YOUTUBE_DL);
}


#define EXAMPLE_DL_URL "http://www.youtube.com/watch?v=WCR6f6WzjP8"

// prompt for the following:
// - URL
// save dir
// format selection (free / nonfree)
// filename
// approx file size
// update youtube-dl
// advanced :: audio selection / save subs / sub language [TODO]

lives_remote_clip_request_t *run_youtube_dialog(lives_remote_clip_request_t *req) {
  static lives_remote_clip_request_t *def_req = NULL;
  LiVESWidget *dialog_vbox;
  LiVESWidget *label;
  LiVESWidget *ext_label;
  LiVESWidget *hbox;
  LiVESWidget *dialog;
  LiVESWidget *url_entry;
  LiVESWidget *name_entry;
  LiVESWidget *dir_entry;
  LiVESWidget *checkbutton_update;
  LiVESWidget *cb_debug = NULL;
#ifdef ALLOW_NONFREE_CODECS
  LiVESWidget *radiobutton_free;
  LiVESWidget *radiobutton_nonfree;
#endif
  LiVESWidget *button;
  LiVESWidget *layout;

  double width_step = 4.;
  double height_step = 4.;

  char *fname;
  char mopts[N_MATCH_TYPES];

#ifdef ALLOW_NONFREE_CODECS
  LiVESSList *radiobutton_group = NULL;
#endif
  LiVESSList *radiobutton_group2 = NULL;
  char *title, *tmp, *tmp2, *msg;
  char *dfile = NULL, *url = NULL;

  char dirname[PATH_MAX];
#ifdef YTDL_URL
  uint64_t gflags = 0;
#endif

  LiVESResponseType response;
  boolean only_free = TRUE;
  boolean debug = FALSE;
  static boolean trylocal = FALSE;
  static boolean firsttime = TRUE;

  if (!req && def_req) {
    only_free = !def_req->allownf;
  }

  if (!req || !req->do_update || trylocal) {
#ifdef YTDL_URL
    gflags |= INSTALL_CANLOCAL;
#endif
    if (!check_for_executable(&capable->has_youtube_dl, EXEC_YOUTUBE_DL) &&
        !check_for_executable(&capable->has_youtube_dlc, EXEC_YOUTUBE_DLC)) {
      firsttime = trylocal = TRUE;
      if (do_please_install(NULL, EXEC_YOUTUBE_DL, EXEC_YOUTUBE_DLC, INSTALL_CANLOCAL) == LIVES_RESPONSE_CANCEL) {
        capable->has_youtube_dl = capable->has_youtube_dlc = UNCHECKED;
        return NULL;
      }
      if (!check_for_executable(&capable->has_pip, EXEC_PIP)
          && !check_for_executable(&capable->has_pip, EXEC_PIP3)
         ) {
        /// check we can update locally
        char *msg = _("LiVES can try to install a local copy, however");
        do_please_install(msg, EXEC_PIP, NULL, 0);
        lives_free(msg);
        capable->has_pip = UNCHECKED;
        return NULL;
      }
    } else {
      if (capable->has_youtube_dl != LOCAL) {
        /// local version not found, so try first with system version
        firsttime = FALSE;
      }
    }
  }

  if (firsttime) {
    if (!check_for_executable(&capable->has_pip, EXEC_PIP) &&
        !check_for_executable(&capable->has_pip, EXEC_PIP3)) {
      /// requirement is missing, if the user does set it checked, we will warn
      if (!trylocal) firsttime = FALSE;
    }
  }

#ifdef ALLOW_NONFREE_CODECS
  if (req) only_free = !req->allownf;
#endif

  if (req) debug = req->debug;

  title = (_("Open Online Clip"));

  dialog = lives_standard_dialog_new(title, TRUE, -1, -1);
  lives_signal_handlers_disconnect_by_func(dialog, LIVES_GUI_CALLBACK(return_true), NULL);

  lives_free(title);

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(dialog));

  widget_opts.justify = LIVES_JUSTIFY_CENTER;
  msg = lives_strdup_printf(_("To open a clip from Youtube or another video site, LiVES will first download it with %s.\n"),
                            EXEC_YOUTUBE_DL);
  label = lives_standard_label_new(msg);
  lives_free(msg);

  lives_box_pack_start(LIVES_BOX(dialog_vbox), label, FALSE, FALSE, 0);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, TRUE, TRUE, widget_opts.packing_height * 2);

  add_spring_to_box(LIVES_BOX(hbox), 0);

  msg = lives_big_and_bold(_("<--- Install or Update local copy of %s ?"), EXEC_YOUTUBE_DL);
  widget_opts.use_markup = TRUE;
  checkbutton_update = lives_standard_check_button_new(msg, firsttime || (req && req->do_update),
                       LIVES_BOX(hbox),
                       H_("If checked then LiVES will attempt to update\n"
                          "it to the most recent version\n"
                          "before attempting the download."));
  widget_opts.use_markup = FALSE;
  lives_free(msg);

  button = lives_standard_button_new_from_stock_full(LIVES_STOCK_DIALOG_INFO, _("_Info"),
           DEF_BUTTON_WIDTH, DEF_BUTTON_HEIGHT, LIVES_BOX(hbox), TRUE, NULL);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(button), LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_utupinfo_clicked), NULL);

  add_spring_to_box(LIVES_BOX(hbox), 0);

  tmp = lives_strdup_printf(_("Enter the URL of the clip below.\nE.g. %s"),
                            EXAMPLE_DL_URL);
  label = lives_standard_label_new(tmp);
  lives_free(tmp);

  widget_opts.justify = LIVES_JUSTIFY_DEFAULT;

  lives_box_pack_start(LIVES_BOX(dialog_vbox), label, FALSE, FALSE, 0);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, TRUE, TRUE, widget_opts.packing_height);

  url_entry = lives_standard_entry_new(_("Clip URL : "), req ? req->URI : "",
                                       LONG_ENTRY_WIDTH, URL_MAX, LIVES_BOX(hbox), NULL);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, TRUE, FALSE, widget_opts.packing_height);

  if (only_free)
    ext_label = lives_standard_label_new("." LIVES_FILE_EXT_WEBM);
  else
    ext_label = lives_standard_label_new("." LIVES_FILE_EXT_MP4);

#ifdef ALLOW_NONFREE_CODECS
  label = lives_standard_label_new(_("Format selection:"));

  lives_box_pack_start(LIVES_BOX(hbox), label, FALSE, FALSE, widget_opts.packing_width);

  radiobutton_free =
    lives_standard_radio_button_new((tmp = (_("_Free (eg. vp9 / opus / webm)"))), &radiobutton_group, LIVES_BOX(hbox),
                                    (tmp2 = (_("Download clip using Free codecs and support the community"))));

  lives_free(tmp); lives_free(tmp2);

  add_fill_to_box(LIVES_BOX(hbox));

#endif
  name_entry = lives_standard_entry_new(_("_File Name : "), req ? req->fname : "",
                                        SHORT_ENTRY_WIDTH, PATH_MAX, LIVES_BOX(hbox), NULL);

  lives_box_pack_start(LIVES_BOX(hbox), ext_label, FALSE, FALSE, 0);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(url_entry), LIVES_WIDGET_CHANGED_SIGNAL,
                            LIVES_GUI_CALLBACK(dl_url_changed), name_entry);

#ifdef ALLOW_NONFREE_CODECS
  //
  hbox = lives_hbox_new(FALSE, 0);

  lives_widget_show_all(dialog);
  lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);
  lives_box_pack_start(LIVES_BOX(dialog_vbox), align_horizontal_with(hbox, radiobutton_free),
                       TRUE, FALSE, widget_opts.packing_height);

  radiobutton_nonfree = lives_standard_radio_button_new((tmp = (_("_Non-free (eg. h264 / aac / mp4)"))),
                        &radiobutton_group,  LIVES_BOX(hbox),
                        (tmp2 = (_("Download clip using non-free codecs and support commercial interests"))));

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(radiobutton_nonfree), !only_free);

  lives_free(tmp); lives_free(tmp2);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(radiobutton_nonfree), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_freedom_toggled), (livespointer)ext_label);

#endif

  toggle_toggles_var(LIVES_TOGGLE_BUTTON(radiobutton_free), &only_free, FALSE);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, TRUE, FALSE, widget_opts.packing_height * 3);

  dir_entry = lives_standard_direntry_new(_("_Directory to save to: "),
                                          req ? req->save_dir : mainw->vid_dl_dir,
                                          LONG_ENTRY_WIDTH, PATH_MAX, LIVES_BOX(hbox), NULL);
  lives_entry_set_editable(LIVES_ENTRY(dir_entry), TRUE);

  if (prefs->show_dev_opts) {
    cb_debug = lives_standard_check_button_new("Debug mode", debug, LIVES_BOX(hbox), NULL);
    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, TRUE, FALSE, widget_opts.packing_height);
    toggle_toggles_var(LIVES_TOGGLE_BUTTON(cb_debug), &debug, FALSE);
  }

  add_hsep_to_box(LIVES_BOX(dialog_vbox));

  layout = lives_layout_new(LIVES_BOX(dialog_vbox));

  lives_layout_add_row(LIVES_LAYOUT(layout));
  lives_layout_add_label(LIVES_LAYOUT(layout), _("Desired frame size:"), TRUE);

  lives_memset(mopts, MATCH_TYPE_ENABLED, N_MATCH_TYPES);
  if (def_req && def_req && mopts[def_req->matchsize])
    mopts[def_req->matchsize] = MATCH_TYPE_DEFAULT;
  else {
    if (mopts[prefs->dload_matmet]) mopts[prefs->dload_matmet] = MATCH_TYPE_DEFAULT;
    else mopts[LIVES_MATCH_CHOICE] = MATCH_TYPE_DEFAULT;
  }
  radiobutton_group2 = add_match_methods(LIVES_LAYOUT(layout), mopts,
                                         width_step, height_step, CURRENT_CLIP_HAS_VIDEO);

  ///////

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, TRUE, FALSE, widget_opts.packing_height * 2);

  add_spring_to_box(LIVES_BOX(hbox), 0);
  lives_standard_expander_new(_("Other _options (e.g audio, subtitles)..."),
                              _("Hide _options"), LIVES_BOX(hbox), NULL);
  add_spring_to_box(LIVES_BOX(hbox), 0);

  lives_widget_grab_focus(url_entry);

  // TODO - add other options

  while (1) {
    response = lives_dialog_run(LIVES_DIALOG(dialog));
    if (response == LIVES_RESPONSE_CANCEL) {
      mainw->cancelled = CANCEL_USER;
      return NULL;
    }

    ///////

    if (!*lives_entry_get_text(LIVES_ENTRY(name_entry))) {
      do_error_dialog(_("Please enter the name of the file to save the downloaded clip as.\n"));
      continue;
    }

    url = lives_strdup(lives_entry_get_text(LIVES_ENTRY(url_entry)));

    if (!(*url)) {
      lives_free(url);
      do_error_dialog(_("Please enter a valid URL for the download.\n"));
      continue;
    }

    fname = ensure_extension(lives_entry_get_text(LIVES_ENTRY(name_entry)), only_free ? LIVES_FILE_EXT_WEBM
                             : LIVES_FILE_EXT_MP4);
    lives_snprintf(dirname, PATH_MAX, "%s", lives_entry_get_text(LIVES_ENTRY(dir_entry)));
    ensure_isdir(dirname);
    dfile = lives_build_filename(dirname, fname, NULL);
    lives_free(fname);
    if (!check_file(dfile, TRUE)) {
      lives_free(dfile);
      lives_free(url);
      continue;
    }
    break;
  }

  lives_snprintf(mainw->vid_dl_dir, PATH_MAX, "%s", dirname);

  if (!req) {
    req = (lives_remote_clip_request_t *)lives_calloc(1, sizeof(lives_remote_clip_request_t));
    if (!req) {
      lives_widget_destroy(dialog);
      lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);
      lives_free(url); lives_free(dfile);
      LIVES_ERROR("Could not alloc memory for remote clip request");
      mainw->error = TRUE;
      return NULL;
    }
  }

  req->allownf = !only_free;
  req->debug = debug;

  mainw->error = FALSE;
  d_print(_("Downloading %s to %s..."), url, dfile);
  lives_free(dfile);

  lives_snprintf(req->URI, 8192, "%s", url);
  lives_free(url);
  lives_snprintf(req->save_dir, PATH_MAX, "%s", dirname);
  lives_snprintf(req->fname, PATH_MAX, "%s", lives_entry_get_text(LIVES_ENTRY(name_entry)));
#ifdef ALLOW_NONFREE_CODECS
  if (!req->allownf)
    lives_snprintf(req->format, 256, "%s", LIVES_FILE_EXT_WEBM);
  else
    lives_snprintf(req->format, 256, "%s", LIVES_FILE_EXT_MP4);
#else
  lives_snprintf(req->format, 256, "%s", LIVES_FILE_EXT_WEBM);
#endif
  req->desired_width = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(spinbutton_width));
  req->desired_height = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(spinbutton_height));
  req->desired_fps = 0.;

  req->matchsize = (lives_match_t)rbgroup_get_data(radiobutton_group2, MATCHTYPE_KEY, LIVES_MATCH_UNDEFINED);
  if (req->matchsize < 0) {
    req->matchsize = -req->matchsize;
    update_int_pref(PREF_DLOAD_MATMET, req->matchsize, TRUE);
  }
  if (!lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(checkbutton_update))) req->do_update = FALSE;
  else {
    req->do_update = TRUE;
  }
  *req->vidchoice = *req->audchoice = 0;

  lives_widget_destroy(dialog);
  lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);
  firsttime = FALSE;
  if (!def_req)
    def_req = (lives_remote_clip_request_t *)lives_calloc(1, sizeof(lives_remote_clip_request_t));
  if (def_req) {
    // keep these around for next time
    def_req->allownf = req->allownf;
    def_req->matchsize = req->matchsize;
  }
  return req;
}


static boolean on_ebox_click(LiVESWidget * widget, LiVESXEventButton * event, livespointer user_data) {
  // want to get doubleclick and then exit somehow
  int val = LIVES_POINTER_TO_INT(user_data);
  if (event->type != LIVES_BUTTON_PRESS) {
    lives_dialog_response(LIVES_DIALOG(lives_widget_get_toplevel(widget)), val);
    lives_widget_destroy(lives_widget_get_toplevel(LIVES_WIDGET(widget)));
  }
  return TRUE;
}


boolean youtube_select_format(lives_remote_clip_request_t *req) {
  // need to set req->vidchoice
  LiVESWidget *dialog, *dialog_vbox, *scrollw, *table;
  LiVESWidget *label, *eventbox, *cancelbutton;
  LiVESWidget *abox;

  LiVESList *allids = NULL;

  char **lines, **pieces;

  char *title, *txt;
  char *notes;

  size_t slen;

  int numlines, npieces;
  int width, height;
  int i, j, dbw, pdone;
  int scrw = GUI_SCREEN_WIDTH;
  int scrh = GUI_SCREEN_HEIGHT;
  int row = 1;
  int response;

  if (lives_strlen(mainw->msg) < 10) return FALSE;
  numlines = get_token_count(mainw->msg, '|');
  if (numlines < 4) return FALSE;
  lines = lives_strsplit(mainw->msg, "|", numlines);
  if (strcmp(lines[0], "completed")) {
    lives_strfreev(lines);
    return FALSE;
  }

  req->duration = lives_strtod(lines[1]);

  if (req->duration == 0.) {
    if (!do_utube_stream_warn()) return FALSE;
  }

  // create the dialog with a scrolledwindow
  width = scrw - SCR_WIDTH_SAFETY;
  height = (scrh - SCR_HEIGHT_SAFETY) / 2;

  title = (_("Select Video Format to Download"));
  dialog = lives_standard_dialog_new(title, FALSE, 8, 8);
  lives_free(title);

  abox = lives_dialog_get_action_area(LIVES_DIALOG(dialog));
  if (LIVES_IS_BUTTON_BOX(abox)) {
    label = lives_standard_label_new(_("Double click on a format to load it, or click Cancel to exit."));
    lives_box_pack_start(LIVES_BOX(abox), label, FALSE, TRUE, widget_opts.border_width);
    lives_button_box_make_first(LIVES_BUTTON_BOX(abox), label);

    add_fill_to_box(LIVES_BOX(abox));
  }

  cancelbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_CANCEL, NULL,
                 LIVES_RESPONSE_CANCEL);

  lives_window_add_escape(LIVES_WINDOW(dialog), cancelbutton);

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(dialog));

  table = lives_table_new(numlines, 5, FALSE);
  lives_table_set_row_spacings(LIVES_TABLE(table), widget_opts.packing_height * 2);

  dbw = widget_opts.border_width;

  // need to set a large enough default here
  scrollw = lives_standard_scrolled_window_new(width * .8, height * 1., table);
  widget_opts.border_width = dbw;

  lives_box_pack_start(LIVES_BOX(dialog_vbox), scrollw, FALSE, TRUE, 0);
  lives_container_set_border_width(LIVES_CONTAINER(table), 0);

  notes = lives_strdup("");

  // set the column headings
  label = lives_standard_label_new(_("ID"));
  lives_table_attach(LIVES_TABLE(table), label, 0, 1, 0, 1,
                     (LiVESAttachOptions)(LIVES_EXPAND | LIVES_FILL),
                     (LiVESAttachOptions)(0), 0, 0);
  lives_widget_set_halign(label, LIVES_ALIGN_CENTER);
  lives_widget_set_valign(label, LIVES_ALIGN_END);

  label = lives_standard_label_new(_("Format"));
  lives_table_attach(LIVES_TABLE(table), label, 1, 2, 0, 1,
                     (LiVESAttachOptions)(LIVES_EXPAND | LIVES_FILL),
                     (LiVESAttachOptions)(0), 0, 0);
  lives_widget_set_halign(label, LIVES_ALIGN_CENTER);
  lives_widget_set_valign(label, LIVES_ALIGN_END);

  label = lives_standard_label_new(_("Resolution"));
  lives_table_attach(LIVES_TABLE(table), label, 2, 3, 0, 1,
                     (LiVESAttachOptions)(LIVES_EXPAND | LIVES_FILL),
                     (LiVESAttachOptions)(0), 0, 0);
  lives_widget_set_halign(label, LIVES_ALIGN_CENTER);
  lives_widget_set_valign(label, LIVES_ALIGN_END);

  label = lives_standard_label_new(_("Notes"));
  lives_table_attach(LIVES_TABLE(table), label, 3, 4, 0, 1,
                     (LiVESAttachOptions)(LIVES_EXPAND | LIVES_FILL),
                     (LiVESAttachOptions)(0), 0, 0);
  lives_widget_set_halign(label, LIVES_ALIGN_CENTER);
  lives_widget_set_valign(label, LIVES_ALIGN_END);

  for (i = 2; i < numlines; i++) {
    if (!lines[i]) continue;

    npieces = get_token_count(lines[i], ' ');
    pieces = lives_strsplit(lines[i], " ", npieces);
    pdone = 0;

    for (j = 0; j < npieces; j++) {
      if (!pieces[j]) break;
      if (pdone < 3 && !*pieces[j]) continue;

      if (pdone == 0) {
        // id no
        txt = lives_strdup_printf("\n%s\n", pieces[j]);
        label = lives_standard_label_new(txt);
        lives_free(txt);
        lives_widget_apply_theme3(label, LIVES_WIDGET_STATE_NORMAL);
        eventbox = lives_event_box_new();
        lives_container_add(LIVES_CONTAINER(eventbox), label);
        lives_event_box_set_above_child(LIVES_EVENT_BOX(eventbox), TRUE);
        lives_signal_sync_connect(LIVES_GUI_OBJECT(eventbox), LIVES_WIDGET_BUTTON_PRESS_EVENT,
                                  LIVES_GUI_CALLBACK(on_ebox_click),
                                  LIVES_INT_TO_POINTER(row - 1));
        lives_table_attach(LIVES_TABLE(table), eventbox, 0, 1, row, row + 1,
                           (LiVESAttachOptions)(LIVES_EXPAND | LIVES_FILL),
                           (LiVESAttachOptions)(0), 0, 0);
        lives_widget_set_halign(label, LIVES_ALIGN_CENTER);
        allids = lives_list_append(allids, lives_strdup(pieces[j]));
        pdone = 1;
        continue;
      }

      if (pdone == 1) {
        // format
        txt = lives_strdup_printf("\n%s\n", pieces[j]);
        label = lives_standard_label_new(txt);
        lives_free(txt);
        lives_widget_apply_theme3(label, LIVES_WIDGET_STATE_NORMAL);
        eventbox = lives_event_box_new();
        lives_container_add(LIVES_CONTAINER(eventbox), label);
        lives_event_box_set_above_child(LIVES_EVENT_BOX(eventbox), TRUE);
        lives_signal_sync_connect(LIVES_GUI_OBJECT(eventbox), LIVES_WIDGET_BUTTON_PRESS_EVENT,
                                  LIVES_GUI_CALLBACK(on_ebox_click),
                                  LIVES_INT_TO_POINTER(row - 1));
        lives_table_attach(LIVES_TABLE(table), eventbox, 1, 2, row, row + 1,
                           (LiVESAttachOptions)(LIVES_EXPAND | LIVES_FILL),
                           (LiVESAttachOptions)(0), 0, 0);
        lives_widget_set_halign(label, LIVES_ALIGN_CENTER);
        pdone = 2;
        continue;
      }

      if (pdone == 2) {
        // res
        txt = lives_strdup_printf("\n%s\n", pieces[j]);
        label = lives_standard_label_new(txt);
        lives_free(txt);
        lives_widget_apply_theme3(label, LIVES_WIDGET_STATE_NORMAL);
        eventbox = lives_event_box_new();
        lives_container_add(LIVES_CONTAINER(eventbox), label);
        lives_event_box_set_above_child(LIVES_EVENT_BOX(eventbox), TRUE);
        lives_signal_sync_connect(LIVES_GUI_OBJECT(eventbox), LIVES_WIDGET_BUTTON_PRESS_EVENT,
                                  LIVES_GUI_CALLBACK(on_ebox_click),
                                  LIVES_INT_TO_POINTER(row - 1));
        lives_table_attach(LIVES_TABLE(table), eventbox, 2, 3, row, row + 1,
                           (LiVESAttachOptions)(LIVES_EXPAND | LIVES_FILL),
                           (LiVESAttachOptions)(0), 0, 0);
        lives_widget_set_halign(label, LIVES_ALIGN_CENTER);
        pdone = 3;
        continue;
      }
      notes = lives_strdup_printf("%s %s", notes, pieces[j]);
    }

    lives_strfreev(pieces);

    slen = lives_strlen(notes);
    // strip trailing newline
    if (slen > 0 && notes[slen - 1] == '\n') notes[slen - 1] = 0;

    txt = lives_strdup_printf("\n%s\n", notes);
    label = lives_standard_label_new(txt);
    lives_free(txt);
    lives_widget_apply_theme3(label, LIVES_WIDGET_STATE_NORMAL);
    eventbox = lives_event_box_new();
    lives_container_add(LIVES_CONTAINER(eventbox), label);
    lives_event_box_set_above_child(LIVES_EVENT_BOX(eventbox), TRUE);
    lives_signal_sync_connect(LIVES_GUI_OBJECT(eventbox), LIVES_WIDGET_BUTTON_PRESS_EVENT,
                              LIVES_GUI_CALLBACK(on_ebox_click),
                              LIVES_INT_TO_POINTER(row - 1));
    lives_table_attach(LIVES_TABLE(table), eventbox, 3, 4, row, row + 1,
                       (LiVESAttachOptions)(LIVES_EXPAND | LIVES_FILL),
                       (LiVESAttachOptions)(0), 0, 0);
    lives_free(notes);
    notes = lives_strdup("");
    row++;
  }

  lives_strfreev(lines);
  lives_free(notes);

  if (req->duration == 0.) {
    // stream doesnt care
    lives_snprintf(req->vidchoice, 512, "%s", (char *)lives_list_nth_data(allids, 0));
    lives_list_free_all(&allids);
    return TRUE;
  }

  response = lives_dialog_run(LIVES_DIALOG(dialog));

  if (response < 0) {
    // user cancelled
    lives_list_free_all(&allids);
    lives_widget_destroy(dialog);
    return FALSE;
  }

  // set req->vidchoice and return
  lives_snprintf(req->vidchoice, 512, "%s", (char *)lives_list_nth_data(allids, response));
  lives_list_free_all(&allids);
  return TRUE;
}


static boolean workdir_change_spacecheck(const char *new_dir, uint64_t *freeds) {
  int64_t dsval = -1;
  uint64_t dsfree;
  if (mainw->dsu_valid && capable->ds_used > -1) {
    dsval = capable->ds_used;
  } else if (prefs->disk_quota) {
    dsval = disk_monitor_check_result(prefs->workdir);
  }
  if (dsval == -1) {
    dsval = get_dir_size(prefs->workdir);
  }
  if (dsval >= 0) capable->ds_used = dsval;
  dsfree = get_ds_free(new_dir);

  if ((int64_t)dsfree < capable->ds_used) {
    *freeds = dsfree;
    return FALSE;
  }
  return TRUE;
}


static boolean workdir_check_levels(const char *fpmp, uint64_t freeds) {
  if (prefs->ds_crit_level <= 0 && prefs->ds_warn_level <= 0 && !prefs->disk_quota)
    return TRUE;
  LiVESWidget *dialog = lives_standard_dialog_new(_("Diskspace Options for New Location"),
                        FALSE, DEF_DIALOG_WIDTH, DEF_DIALOG_HEIGHT);
  LiVESWidget *dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(dialog));
  LiVESWidget *layout = lives_layout_new(LIVES_BOX(dialog_vbox));
  LiVESWidget *button, *hbox, *image;

  lives_layout_add_label(LIVES_LAYOUT(layout), _("Diskspace critical levels"), TRUE);

  lives_layout_add_label(LIVES_LAYOUT(layout), _("Diskspace warning levels"), TRUE);

  lives_layout_add_fill(LIVES_LAYOUT(layout), TRUE);
  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));

  button =
    lives_standard_button_new_full(_("Set quota for new volume"), -1, -1, LIVES_BOX(hbox), TRUE, NULL);

  image = lives_image_find_in_stock(LIVES_ICON_SIZE_BUTTON, "quota", "settings", "preferences", NULL);
  lives_standard_button_set_image(LIVES_BUTTON(button), image, FALSE);

  lives_dialog_run(LIVES_DIALOG(dialog));

  return FALSE;
}


boolean workdir_change_dialog(void) {
  // show a dialog with message and buttons, set the relevant prefs.
  // return FALSE on Cancel
  LiVESWidget *dialog = lives_standard_dialog_new(_("Options for Changing Working Directory"),
                        FALSE, DEF_DIALOG_WIDTH, DEF_DIALOG_HEIGHT);
  LiVESWidget *dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(dialog));
  LiVESWidget *label, *hbox;
  LiVESWidget *warn_label;
#if LIVES_HAS_SPINNER_WIDGET
  LiVESWidget *spinner;
#endif
  LiVESWidget *cancel_button;
  char *msg = workdir_ch_warning();
  boolean ret = TRUE;

  widget_opts.use_markup = TRUE;
  label = lives_standard_formatted_label_new(msg);
  lives_free(msg);
  lives_box_pack_start(LIVES_BOX(dialog_vbox), label, TRUE, TRUE, 0);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, FALSE, TRUE, widget_opts.packing_height);
  warn_label = lives_standard_label_new("");
  lives_box_pack_start(LIVES_BOX(hbox), warn_label, FALSE, TRUE, widget_opts.packing_width);

#if LIVES_HAS_SPINNER_WIDGET
  spinner = lives_standard_spinner_new(FALSE);
  lives_box_pack_start(LIVES_BOX(hbox), spinner, FALSE, TRUE, widget_opts.packing_width);
#endif

  //
  cancel_button = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_CANCEL, NULL,
                  LIVES_RESPONSE_CANCEL);
  widget_opts.use_markup = TRUE;
  lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_DELETE,
                                     _("<b>_Delete</b> the old directory\nand its contents"),
                                     LIVES_RESPONSE_NO);
  lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_KEEP,
                                     _("<b>_Leave</b> the contents,\nchange to a new directory"),
                                     LIVES_RESPONSE_ACCEPT);
  lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_GO_FORWARD,
                                     _("<b>_Move</b> everything\nto the new directory"),
                                     LIVES_RESPONSE_YES);
  widget_opts.use_markup = FALSE;

  lives_window_add_escape(LIVES_WINDOW(dialog), cancel_button);
  lives_window_block_delete(LIVES_WINDOW(dialog));

  while (1) {
    LiVESResponseType resp = lives_dialog_run_with_countdown(LIVES_DIALOG(dialog), LIVES_RESPONSE_NO, 3);
    if (resp == LIVES_RESPONSE_CANCEL) {
      lives_widget_destroy(dialog);
      return FALSE;
    }
    if (resp == LIVES_RESPONSE_NO) {
      lives_widget_hide(dialog);
      if (!check_del_workdir(prefs->workdir)) {
        lives_widget_show_all(dialog);
        continue;
      }
      prefs->workdir_tx_intent = LIVES_INTENTION_DELETE;
    }
    if (resp == LIVES_RESPONSE_ACCEPT) prefs->workdir_tx_intent = LIVES_INTENTION_LEAVE;
    if (resp == LIVES_RESPONSE_YES) {
      char *fpmp;
      if (!capable->mountpoint) capable->mountpoint = get_mountpoint_for(prefs->workdir);
      if (lives_strcmp((fpmp = get_mountpoint_for(future_prefs->workdir)), capable->mountpoint)) {
        // show spinner and label
        uint64_t fsp1;
        msg = lives_strdup_printf(_("Checking free space in %s"), future_prefs->workdir);
        lives_label_set_text(LIVES_LABEL(warn_label), msg);
        lives_free(msg);
#if LIVES_HAS_SPINNER_WIDGET
        lives_spinner_start(LIVES_SPINNER(spinner));
#endif
        lives_widget_context_update();
        if (!workdir_change_spacecheck(future_prefs->workdir, &fsp1)) {
          char *fsz1 = lives_format_storage_space_string(fsp1);
          char *fsz2 = lives_format_storage_space_string(capable->ds_free);
          // show error
          msg = lives_strdup_printf(_("Insuffient space in volume containing %s !\n"
                                      "(free space in %s = %s\n work directory size is %s)\n"
                                      "Please free up some disk space in %s or choose another action"),
                                    future_prefs->workdir, fpmp, fsz1, fsz2, fpmp);
          lives_label_set_text(LIVES_LABEL(warn_label), msg);
#if LIVES_HAS_SPINNER_WIDGET
          lives_spinner_stop(LIVES_SPINNER(spinner));
#endif
          lives_free(msg);
          lives_free(fpmp); lives_free(fsz1); lives_free(fsz2);
          continue;
        }
        // check if we are moving to a different device. If so first check if there is sufficient diskspace,
        // then prompt the user to set disk warn / critical and quota levels
        // then check the status post move. If we pass critical level, abandon the operation
        // if we overshoot disk warn or quota levels, warn the user and allow them to cancel, adjust or ignore.

        if (!workdir_check_levels(fpmp, fsp1)) {
          lives_free(fpmp);
          ret = FALSE;
          break;
        }
      }
      lives_free(fpmp);
      prefs->workdir_tx_intent = LIVES_INTENTION_MOVE;
    }
    break;
  }
  lives_widget_destroy(dialog);
  return ret;
}


/// disk quota window

static void lives_show_after(LiVESWidget * button, livespointer data) {
  LiVESWidget *showme = (LiVESWidget *)data;
  lives_general_button_clicked(LIVES_BUTTON(button), NULL);
  if (showme) lives_widget_show(showme);
}

static void workdir_query_cb(LiVESWidget * w, LiVESWidget * dlg) {
  // called from disk quota window to change the working directory
  lives_widget_hide(dlg);
  // prompt for new dir.
  if (do_workdir_query()) {
    if (lives_strcmp(prefs->workdir, future_prefs->workdir)) {
      // not Cancelled, and value was changed
      if (workdir_change_dialog()) {
        lives_widget_destroy(dlg);
        on_quit_activate(NULL, NULL);
      } else {
        do_info_dialog(_("\nDirectory was not changed\n"));
      }
    }
    *future_prefs->workdir = 0;
  }
  lives_widget_show(dlg);
}

static void cleards_cb(LiVESWidget * w, LiVESWidget * dlg) {
  lives_widget_hide(dlg);
  on_cleardisk_activate(NULL, NULL);
  lives_widget_show(dlg);

}

void run_diskspace_dialog_cb(LiVESWidget * w, livespointer data) {
  run_diskspace_dialog((const char *)data);
}

boolean run_diskspace_dialog_idle(livespointer data) {
  run_diskspace_dialog((const char *)data);
  return FALSE;
}

///////

static void manclips_del(LiVESWidget * button, _entryw * entryw) {
  boolean is_curset = FALSE;
  const char *setname = lives_entry_get_text(LIVES_ENTRY(entryw->entry));
  char *fsetname;
  if (!*setname) return;

retry1:
  if (mainw->was_set && !lives_strcmp(setname, mainw->set_name)) {
    is_curset = TRUE;
    fsetname = lives_strdup(_("current set"));
  } else
    fsetname = lives_strdup_printf(_("set %s"), setname);
  if (check_for_executable(&capable->has_gio, EXEC_GIO) == PRESENT) mainw->add_trash_rb = TRUE;
  if (do_warning_dialogf(_("The %s will be permanently deleted from the disk.\n"
                           "Are you sure ?"), fsetname)) {
    mainw->add_trash_rb = FALSE;
    lives_free(fsetname);
    if (is_curset) {
      del_current_set(FALSE);
    } else {
      char *setdir = lives_build_path(prefs->workdir, setname, NULL);
      if (mainw->add_trash_rb && prefs->pref_trash) {
        if (send_to_trash(setdir) == LIVES_RESPONSE_CANCEL) goto retry1;
      } else lives_rmdir(setdir, TRUE);
      mainw->num_sets--;
    }
    if (!mainw->num_sets && !mainw->clips_available) {
      do_info_dialog(_("All Sets have been erased from the disk"));
      lives_widget_destroy(entryw->dialog);
      lives_widget_show(entryw->parent);
      lives_free(entryw);
      entryw = NULL;
      lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);
      return;
    }
    // refresh entry
    lives_entry_set_text(LIVES_ENTRY(entryw->entry), "");
    lives_widget_queue_draw(entryw->dialog);
  }
  mainw->add_trash_rb = FALSE;
  lives_free(fsetname);
}

static void manclips_reload(LiVESWidget * button, _entryw * entryw) {
  char *setname = (char *)lives_entry_get_text(LIVES_ENTRY(entryw->entry));
  if (!*setname) return;
  setname = lives_strdup(lives_entry_get_text(LIVES_ENTRY(entryw->entry)));
  if (mainw->was_set && !lives_strcmp(setname, mainw->set_name)) {
    do_info_dialogf(_("The set %s is already loaded !"), setname);
    return;
  }
  if (mainw->cliplist) {
    do_info_dialog(_("The current clips must be saved before reloading another set"));
    mainw->only_close = TRUE;
    if (!on_save_set_activate(button, NULL)) return;
    mainw->only_close = FALSE;
  }

  do_info_dialog(_("After reloading the Set you can inspect it and use it as normal.\n"
                   "Should you decide to delete it or re-save it, click on\nFile | Close/Save all Clips "
                   "in the menu of the Clip Editor\n"
                   "You will then be returned to the Manage Sets dialog,\n"
                   "where you may choose to continue this process further\n"));
  lives_widget_destroy(entryw->dialog);
  lives_widget_destroy(entryw->parent);
  lives_free(entryw);
  reload_set(setname);
  if (mainw->num_sets > -1) mainw->num_sets--;
  if (mainw->clips_available) mainw->cs_manage = TRUE;
}

static void manclips_ok(LiVESWidget * button, LiVESWidget * dialog) {
  _entryw *entryw;

  lives_general_button_clicked(LIVES_BUTTON(button), NULL);

  //mainw->cs_manage = TRUE;
  /// show list of all sets, excluding current

  lives_widget_hide(dialog);

  entryw = create_entry_dialog(ENTRYW_RELOAD_SET);
  if (!entryw) return; ///< no sets available

  entryw->parent = dialog;

  /// show buttons "Cancel", "Delete", "Reload"

  lives_signal_sync_connect(LIVES_GUI_OBJECT(entryw->cancelbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(lives_show_after), dialog);

  lives_button_ungrab_default_special(entryw->okbutton);
  lives_widget_destroy(entryw->okbutton);

  button = lives_dialog_add_button_from_stock(LIVES_DIALOG(entryw->dialog), LIVES_STOCK_DELETE,
           NULL, LIVES_RESPONSE_RESET);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(button), LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(manclips_del), entryw);

  // reaload will exit dlg and set mainw->cs_managed, after close/save all we come back here
  button = lives_dialog_add_button_from_stock(LIVES_DIALOG(entryw->dialog), LIVES_STOCK_OPEN,
           _("_Reload"), LIVES_RESPONSE_YES);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(button), LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(manclips_reload), entryw);

  lives_widget_show_all(entryw->dialog);
}

static void manclips_cb(LiVESWidget * w, livespointer data) {
  LiVESWidget *parent = (LiVESWidget *)data;
  LiVESWidget *dialog;
  LiVESWidget *button;
  char *text, *extra;

  lives_widget_hide(parent);

  if (mainw->was_set) {
    extra = (_(" including the current set."));
  } else extra = lives_strdup("");

  text = lives_strdup_printf(_("<b>The current working directory contains %d Clip Sets%s</b>\n"
                               "You may be able to free up some disk space by deleting "
                               "unwanted ones.\n\n"
                               "After selecting an existing Set, "
                               "you will be presented with the options to "
                               "erase it from the disk\n"
                               "or to reload it first to inspect the contents\n\n"
                               "Please select an option below\n"), mainw->num_sets
                             + (mainw->was_set ? 1 : 0), extra);
  lives_free(extra);
  widget_opts.use_markup = TRUE;
  dialog = create_question_dialog(_("Manage Clipsets"), text);
  widget_opts.use_markup = FALSE;
  lives_free(text);

  button = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_GO_BACK,
           _("Go back"), LIVES_RESPONSE_CANCEL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(button), LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(lives_show_after), data);

  button = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_GO_FORWARD,
           _("Continue"), LIVES_RESPONSE_OK);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(button), LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(manclips_ok), parent);

  lives_widget_show_all(dialog);
}


static _dsquotaw *dsq = NULL;

void draw_dsu_widget(LiVESWidget * dsu_widget) {
  double scale, xw, offs_x = 0;
  lives_painter_t *cr;
  int width, height;

  if (!dsq->dsu_surface) return;
  cr = lives_painter_create_from_surface(dsq->dsu_surface);
  if (!cr) return;

  if (capable->ds_used == -1 || capable->ds_free == -1 || capable->ds_tot == -1) return;

  width = lives_widget_get_allocation_width(dsu_widget);
  height = lives_widget_get_allocation_height(dsu_widget);

  if (width <= 0 || height <= 0) return;

  scale = (double)capable->ds_tot / (double)width;

  /// paint bg
  lives_painter_set_source_rgb(cr, 1., 1., 1.);
  lives_painter_rectangle(cr, 0, 0, width, height);
  lives_painter_fill(cr);

  /// space used by other apps
  xw = (double)(capable->ds_tot - capable->ds_used - capable->ds_free) / scale;
  lives_painter_set_source_rgb(cr, 0., 0., 1.);
  lives_painter_rectangle(cr, 0, 0, xw, height);
  lives_painter_fill(cr);

  offs_x += xw;

  /// space used by lives
  xw = (double)capable->ds_used / scale;
  lives_painter_set_source_rgb(cr, 0., 1., 1.);
  lives_painter_rectangle(cr, offs_x, 0, offs_x + xw, height);
  lives_painter_fill(cr);

  offs_x += xw;

  /// draw quota (if set)
  if (future_prefs->disk_quota > capable->ds_used) {
    uint64_t qq = future_prefs->disk_quota - capable->ds_used;
    if (qq > capable->ds_free - prefs->ds_warn_level) qq = capable->ds_free - prefs->ds_warn_level;
    if (qq > 0) {
      xw = (double)qq / scale;
      if (xw > 0.) {
        lives_painter_set_source_rgb(cr, 1., 1., 0.);
        lives_painter_rectangle(cr, offs_x, 0, offs_x + xw, height);
        lives_painter_fill(cr);
        offs_x += xw;
      }
    }
  }

  /// draw ds_free
  xw = (double)(capable->ds_free) / scale;
  if (prefs->ds_warn_level > 0)
    xw -= (double)prefs->ds_warn_level / scale;
  if (future_prefs->disk_quota > capable->ds_used)
    xw -= (double)(future_prefs->disk_quota - capable->ds_used) / scale;

  if (xw > 0.) {
    lives_painter_set_source_rgb(cr, 0., 1., 0.);
    lives_painter_rectangle(cr, offs_x, 0, offs_x + xw, height);
    lives_painter_fill(cr);
    offs_x += xw;
  }

  /// ds warning level
  if (prefs->ds_warn_level > 0) {
    xw = (double)(prefs->ds_warn_level - prefs->ds_crit_level) / scale;
    if (xw > 0.) {
      lives_painter_set_source_rgb(cr, 1., .5, 0.);
      lives_painter_rectangle(cr, offs_x, 0, offs_x + xw, height);
      lives_painter_fill(cr);
      offs_x += xw;
    }
  }

  /// ds critical level
  if (prefs->ds_crit_level > 0) {
    xw = (double)prefs->ds_crit_level / scale;
    if (xw > 0.) {
      lives_painter_set_source_rgb(cr, 1., 0., 0.);
      lives_painter_rectangle(cr, offs_x, 0, offs_x + xw, height);
      lives_painter_fill(cr);
      offs_x += xw;
    }
  }
  lives_painter_destroy(cr);
  lives_widget_queue_draw(dsu_widget);
}


static void dsu_set_toplabel(void) {
  char *ltext = NULL, *dtxt, *dtxt2;
  widget_opts.text_size = LIVES_TEXT_SIZE_LARGE;

  if (mainw->dsu_valid && !dsq->scanning) {
    if (capable->ds_free < prefs->ds_crit_level) {
      if (!capable->mountpoint) capable->mountpoint = get_mountpoint_for(prefs->workdir);
      dtxt = lives_format_storage_space_string(prefs->ds_crit_level);
      dtxt2 = lives_markup_escape_text(capable->mountpoint, -1);
      ltext = lives_strdup_printf(_("<b>ALERT ! FREE SPACE IN %s IS BELOW THE CRITICAL LEVEL OF %s\n"
                                    "YOU SHOULD EXIT LIVES IMMEDIATELY TO AVOID POSSIBLE DATA LOSS</b>"),
                                  dtxt2, dtxt);
      lives_free(dtxt); lives_free(dtxt2);
      widget_opts.use_markup = TRUE;
      lives_label_set_text(LIVES_LABEL(dsq->top_label), ltext);
      widget_opts.use_markup = FALSE;
      widget_opts.text_size = LIVES_TEXT_SIZE_NORMAL;
      if (!dsq->crit_dism) {
        dsq->crit_dism = TRUE;
        lives_free(ltext);
        ltext = ds_critical_msg(prefs->workdir, &capable->mountpoint, capable->ds_free);
        widget_opts.use_markup = TRUE;
        do_abort_ok_dialog(ltext);
        widget_opts.use_markup = FALSE;
      }
      lives_free(ltext);
      return;
    }
    if (capable->ds_free < prefs->ds_warn_level) {
      if (!capable->mountpoint) capable->mountpoint = get_mountpoint_for(prefs->workdir);
      dtxt = lives_format_storage_space_string(prefs->ds_crit_level);
      ltext = lives_strdup_printf(_("WARNING ! Free space in %s is below the warning level of %s\n"
                                    "Action should be taken to remedy this"),
                                  capable->mountpoint, dtxt);
      lives_free(dtxt);
    } else if (prefs->disk_quota) {
      if (capable->ds_used > prefs->disk_quota) {
        uint64_t xs = capable->ds_used - prefs->disk_quota;
        dtxt = lives_format_storage_space_string(xs);
        ltext = lives_strdup_printf(_("WARNING ! LiVES has exceeded its quota by %s"), dtxt);
        lives_free(dtxt);
      } else if (capable->ds_used >= (int64_t)((double)prefs->disk_quota * prefs->quota_limit / 100.)) {
        double pcused = (double)capable->ds_used / (double)prefs->disk_quota * 100.;
        ltext = lives_strdup_printf(_("ATTENTION: LiVES is currently using over %d%% of its assigned quota"), (int)pcused);
      } else if (prefs->disk_quota - capable->ds_used + prefs->ds_warn_level > capable->ds_free) {
        ltext = lives_strdup(_("ATTENTION ! There is insufficient free space on the disk for LiVES' current quota"));
      }
    }
  }
  if (!ltext) {
    ltext = lives_strdup(_("LiVES can help limit the amount of diskspace used by projects (sets)."));
  }
  lives_label_set_text(LIVES_LABEL(dsq->top_label), ltext);
  lives_free(ltext);
  widget_opts.text_size = LIVES_TEXT_SIZE_NORMAL;
}

LIVES_LOCAL_INLINE char *dsu_label_notset(void) {return _("Value not set");}
LIVES_LOCAL_INLINE char *dsu_label_calculating(void) {return _("Calculating....");}

boolean update_dsu(livespointer data) {
  static boolean set_label = FALSE;
  int64_t dsu = -1;
  char *txt;
  char *xtarget = (char *)data;
  if (!xtarget) xtarget = prefs->workdir;
  if ((!dsq || dsq->scanning) && (dsu = disk_monitor_check_result(xtarget)) < 0) {
    if (!dsq || !dsq->visible) {
      return FALSE;
    }
    if (!set_label) {
      set_label = TRUE;
      lives_label_set_text(LIVES_LABEL(dsq->used_label), (txt = dsu_label_calculating()));
      lives_free(txt);
    }
  } else {
    if (mainw->dsu_valid) {
      if (dsu > -1) capable->ds_used = dsu;
      dsu = capable->ds_used;
      capable->ds_status = get_storage_status(xtarget, mainw->next_ds_warn_level, &dsu, 0);
      capable->ds_free = dsu;
      dsq->scanning = FALSE;
      if (mainw->dsu_widget) {
        txt = lives_format_storage_space_string(capable->ds_used);
        lives_label_set_text(LIVES_LABEL(dsq->used_label), txt);
        lives_free(txt);
        draw_dsu_widget(mainw->dsu_widget);
        dsu_set_toplabel();
        dsu_fill_details(NULL, NULL);
        qslider_changed(dsq->slider, dsq);
        if (capable->ds_free < prefs->ds_warn_level || mainw->has_session_workdir)
          lives_widget_set_sensitive(dsq->button, FALSE);
        if (capable->ds_free < prefs->ds_crit_level) {
          lives_widget_set_no_show_all(dsq->abort_button, FALSE);
          lives_widget_show_all(dsq->abort_button);
        }
      }
      set_label = FALSE;
      mainw->dsu_valid = TRUE;
      return FALSE;
    }
  }
  return TRUE;
}

static void qslider_changed(LiVESWidget * slid, livespointer data) {
  char *txt, *dtxt;
  if (mainw->dsu_valid && !dsq->scanning) {
    uint64_t min = capable->ds_used;
    uint64_t max = capable->ds_free + min - prefs->ds_warn_level;
    double value = 0.;
    if (dsq->setting) {
      value = lives_range_get_value(LIVES_RANGE(slid)) / 100.;
      lives_signal_handler_block(dsq->checkbutton, dsq->checkfunc);
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(dsq->checkbutton), FALSE);
      lives_signal_handler_unblock(dsq->checkbutton, dsq->checkfunc);
      future_prefs->disk_quota = (uint64_t)(min + value * (max - min) + .5);
      draw_dsu_widget(mainw->dsu_widget);
    } else {
      if (future_prefs->disk_quota > 0) {
        if (future_prefs->disk_quota >= capable->ds_used) {
          uint64_t dq = future_prefs->disk_quota;
          dq -= min;
          value = 100. * (double)dq / (double)(max - min);
          if (value > 100.) value = 100.;
        }
      }
      lives_signal_handler_block(dsq->slider, dsq->sliderfunc);
      lives_range_set_value(LIVES_RANGE(dsq->slider), value);
      lives_signal_handler_unblock(dsq->slider, dsq->sliderfunc);
    }
  }

  if (future_prefs->disk_quota > 0.) {
    lives_widget_set_opacity(dsq->noqlabel, 0.);
    txt = lives_format_storage_space_string(future_prefs->disk_quota);
    dtxt = lives_strdup_printf("<b>%s</b>", txt);
    widget_opts.use_markup = TRUE;
    lives_label_set_text(LIVES_LABEL(dsq->vlabel), dtxt);
    widget_opts.use_markup = FALSE;
    lives_free(txt); lives_free(dtxt);
    if (mainw->dsu_valid && !dsq->scanning) {
      double pcused = 100. * (double)capable->ds_used
                      / (double)future_prefs->disk_quota;

      if (pcused < 100.) txt = lives_strdup_printf(_("%.2f%% used"), pcused);
      else {
        txt = lives_strdup_printf(_("<b>%.2f%% used !!</b>"), pcused);
        widget_opts.use_markup = TRUE;
      }
      lives_label_set_text(LIVES_LABEL(dsq->pculabel), txt);
      widget_opts.use_markup = FALSE;
      lives_free(txt);
      if (pcused >= prefs->quota_limit) {
        txt = lives_strdup_printf(_("LiVES is currently using over %d%% of its available quota"), (int)prefs->quota_limit);
        show_warn_image(dsq->pculabel, txt);
        lives_free(txt);
      } else hide_warn_image(dsq->pculabel);
    } else {
      hide_warn_image(dsq->pculabel);
      lives_label_set_text(LIVES_LABEL(dsq->pculabel), _("Calculating %% used"));
    }
  } else {
    lives_widget_set_opacity(dsq->noqlabel, 1.);
    hide_warn_image(dsq->pculabel);
    lives_label_set_text(LIVES_LABEL(dsq->pculabel), NULL);
    txt = dsu_label_notset();
    dtxt = lives_strdup_printf("<b>%s</b>", txt);
    widget_opts.use_markup = TRUE;
    lives_label_set_text(LIVES_LABEL(dsq->vlabel), dtxt);
    widget_opts.use_markup = FALSE;
    lives_free(dtxt); lives_free(txt);
  }
}

static void dsq_check_toggled(LiVESWidget * cbutt, livespointer data) {
  if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(cbutt))) {
    future_prefs->disk_quota = 0;
  } else {
    lives_widget_set_sensitive(dsq->slider, TRUE);
    future_prefs->disk_quota = prefs->disk_quota;
  }
  dsq->setting = FALSE;
  qslider_changed(dsq->slider, NULL);
  dsq->setting = TRUE;
  draw_dsu_widget(mainw->dsu_widget);
}

static boolean mouse_on = FALSE;

static boolean dsu_widget_clicked(LiVESWidget * widget, LiVESXEventButton * event, livespointer is_clickp) {
  boolean is_click;
  is_click = LIVES_POINTER_TO_INT(is_clickp);
  if (is_click) mouse_on = TRUE;
  else if (!mouse_on) return TRUE;

  if (!mainw->dsu_valid || dsq->scanning) return TRUE;
  else {
    int width = lives_widget_get_allocation_width(widget);
    if (width <= 0) return TRUE;
    else {
      int x;
      lives_widget_get_pointer((LiVESXDevice *)mainw->mgeom[widget_opts.monitor].mouse_device,
                               widget, &x, NULL);
      if (x > 0) {
        uint64_t min = capable->ds_tot - capable->ds_free;
        uint64_t max = capable->ds_tot - prefs->ds_warn_level;
        double scale = (double)capable->ds_tot / (double)width;
        double value = (double)x * scale;
        value -= (double)min;
        value = 100. * value / (double)(max - min);
        if (value < 0.) value = 0.;
        if (value > 100.) value = 100.;
        lives_range_set_value(LIVES_RANGE(dsq->slider), value);
	// *INDENT-OFF*
      }}}
  // *INDENT-ON*

  return TRUE;
}

static boolean dsu_widget_released(LiVESWidget * widget, LiVESXEventButton * event, livespointer is_clickp) {
  mouse_on = FALSE;
  return TRUE;
}

static void dsu_ok_clicked(LiVESWidget * butt, LiVESWidget * toshow) {
  dsq->visible = FALSE;
  mainw->dsu_widget = NULL;
  lives_show_after(butt, toshow);
}

static void dsu_fill_details(LiVESWidget * widget, livespointer data) {
  LiVESWidget *layout2;
  int64_t ds_free, ds_tot;
  uint64_t dsval;
  char *txt;
  char *xtarget = (char *)data, *mountpoint;

  if (dsq->exp_layout) lives_widget_destroy(dsq->exp_layout);
  dsq->exp_layout = NULL;

  if (!lives_expander_get_expanded(LIVES_EXPANDER(dsq->expander))) return;

  ds_free = capable->ds_free;
  ds_tot = capable->ds_tot;

  if (!xtarget) xtarget = prefs->workdir;
  xtarget = "/data";

  layout2 = dsq->exp_layout = lives_layout_new(LIVES_BOX(dsq->exp_vbox));

  if (!data)
    lives_layout_add_label(LIVES_LAYOUT(layout2), _("Working directory"), TRUE);
  else
    lives_layout_add_label(LIVES_LAYOUT(layout2), _("New working directory"), TRUE);
  lives_layout_add_label(LIVES_LAYOUT(layout2), xtarget, TRUE);

  lives_layout_add_row(LIVES_LAYOUT(layout2));
  lives_layout_add_label(LIVES_LAYOUT(layout2), _("Mount point"), TRUE);
  mountpoint = get_mountpoint_for(xtarget);
  if (!capable->mountpoint && !data)
    capable->mountpoint = mountpoint;
  lives_layout_add_label(LIVES_LAYOUT(layout2), mountpoint, TRUE);

  lives_layout_add_row(LIVES_LAYOUT(layout2));
  lives_layout_add_label(LIVES_LAYOUT(layout2), _("Total size"), TRUE);
  if (!mainw->dsu_valid || dsq->scanning)
    txt = dsu_label_calculating();
  else
    txt = lives_format_storage_space_string(capable->ds_tot);
  lives_layout_add_label(LIVES_LAYOUT(layout2), txt, TRUE);
  lives_free(txt);

  lives_layout_add_label(LIVES_LAYOUT(layout2), _("Disk space free"), TRUE);
  if (!mainw->dsu_valid || dsq->scanning)
    txt = dsu_label_calculating();
  else {
    if (data)
      txt = lives_format_storage_space_string(capable->ds_free + capable->ds_used);
    else
      txt = lives_format_storage_space_string(capable->ds_free);
    lives_layout_add_label(LIVES_LAYOUT(layout2), txt, TRUE);
    lives_free(txt);

    if (data) {
      char *txt2;
      txt = lives_format_storage_space_string(capable->ds_free);
      txt2 = lives_strdup_printf(_("(After migrating working directory: %s)"), txt);
      lives_layout_add_label(LIVES_LAYOUT(layout2), txt2, TRUE);
      lives_free(txt); lives_free(txt2);
    }
  }

  if (mainw->dsu_valid && !dsq->scanning) {
    if (capable->ds_free <= prefs->ds_crit_level)
      show_warn_image(widget_opts.last_label, _("Free diskspace is below the critical level"));
    else if (capable->ds_free <= prefs->ds_warn_level)
      show_warn_image(widget_opts.last_label, _("Free diskspace is below the warning level"));
  }

  lives_layout_add_row(LIVES_LAYOUT(layout2));
  lives_layout_add_label(LIVES_LAYOUT(layout2), _("Used by other applications"), TRUE);
  if (!mainw->dsu_valid || dsq->scanning)
    txt = dsu_label_calculating();
  else {
    dsval = capable->ds_tot - capable->ds_free - capable->ds_used;
    txt = lives_format_storage_space_string(dsval);
  }
  lives_layout_add_label(LIVES_LAYOUT(layout2), txt, TRUE);
  lives_free(txt);

  //lives_layout_add_row(LIVES_LAYOUT(layout2));
  lives_layout_add_label(LIVES_LAYOUT(layout2), _("Used by LiVES"), TRUE);

  if (!mainw->dsu_valid || dsq->scanning)
    txt = dsu_label_calculating();
  else
    txt = lives_format_storage_space_string(capable->ds_used);
  lives_layout_add_label(LIVES_LAYOUT(layout2), txt, TRUE);
  lives_free(txt);

  if (!data) {
    lives_layout_add_row(LIVES_LAYOUT(layout2));
    lives_layout_add_label(LIVES_LAYOUT(layout2), _("Sets on disk"), TRUE);
    txt = lives_strdup_printf("%d", mainw->num_sets + mainw->was_set ? 1 : 0);
    lives_layout_add_label(LIVES_LAYOUT(layout2), txt, TRUE);
    lives_free(txt);

    //lives_layout_add_row(LIVES_LAYOUT(layout2));
    lives_layout_add_label(LIVES_LAYOUT(layout2), _("Currently opened clips"), TRUE);
    txt = lives_strdup_printf("%d", mainw->clips_available);
    lives_layout_add_label(LIVES_LAYOUT(layout2), txt, TRUE);
    lives_free(txt);
  }

  lives_layout_add_row(LIVES_LAYOUT(layout2));
  lives_layout_add_label(LIVES_LAYOUT(layout2), _("Disk quota"), TRUE);
  if (prefs->disk_quota)
    txt = lives_format_storage_space_string(prefs->disk_quota);
  else
    txt = dsu_label_notset();
  lives_layout_add_label(LIVES_LAYOUT(layout2), txt, TRUE);
  lives_free(txt);

  if (prefs->disk_quota) {
    double pcu = 0.;

    if (!mainw->dsu_valid || dsq->scanning)
      txt = dsu_label_calculating();
    else {
      uint64_t qq = prefs->disk_quota, over = 0;
      if (qq > capable->ds_used) {
        qq -= capable->ds_used;
        if (qq + prefs->ds_warn_level > capable->ds_free)
          over = qq + prefs->ds_warn_level - capable->ds_free;
        if (over) {
          char *txt2;
          txt = lives_format_storage_space_string(over);
          txt2 = lives_strdup_printf(_("Quota is reduced by %s due to free disk space limitations"), txt);
          show_warn_image(widget_opts.last_label, txt2);
          lives_free(txt); lives_free(txt2);
        }
        if (qq < over) qq = 0;
        else qq -= over;
        pcu = (double)qq / (double)prefs->disk_quota;
      }
      txt = lives_strdup_printf("%.2f%%", pcu * 100.);
    }

    lives_layout_add_label(LIVES_LAYOUT(layout2), _("Unused quota"), TRUE);
    lives_layout_add_label(LIVES_LAYOUT(layout2), txt, TRUE);
    lives_free(txt);
    pcu = 100. * (1. - pcu);
    if (pcu > prefs->quota_limit) {
      txt = lives_strdup_printf(_("LiVES is currently using over %d%% of its available quota"), (int)prefs->quota_limit);
      show_warn_image(widget_opts.last_label, txt);
      lives_free(txt);
    }
  }

  if (!lives_strcmp(xtarget, prefs->workdir)) {
    capable->ds_free = ds_free;
    capable->ds_tot = ds_tot;
  }

  lives_layout_add_row(LIVES_LAYOUT(layout2));
  lives_layout_add_label(LIVES_LAYOUT(layout2), _("Disk warning level"), TRUE);
  lives_widget_set_tooltip_text(widget_opts.last_label, H_("value can be set in Preferences . Warnings"));
  if (prefs->ds_warn_level)
    txt = lives_format_storage_space_string(prefs->ds_warn_level);
  else
    txt = dsu_label_notset();
  lives_layout_add_label(LIVES_LAYOUT(layout2), txt, TRUE);
  lives_free(txt);

  //lives_layout_add_row(LIVES_LAYOUT(layout2));
  lives_layout_add_label(LIVES_LAYOUT(layout2), _("Disk critical level"), TRUE);
  lives_widget_set_tooltip_text(widget_opts.last_label, H_("value can be set in Preferences . Warnings"));
  if (prefs->ds_crit_level)
    txt = lives_format_storage_space_string(prefs->ds_crit_level);
  else
    txt = dsu_label_notset();
  lives_layout_add_label(LIVES_LAYOUT(layout2), txt, TRUE);
  lives_free(txt);
  lives_widget_show_all(dsq->exp_layout);
}


static void changequota_cb(LiVESWidget * butt, livespointer data) {
  static char *otxt = NULL;

  if (dsq->scanning || !mainw->dsu_valid) {
    lives_label_set_text(LIVES_LABEL(dsq->inst_label), _("Still calculating...please wait and try again..."));
    return;
  }

  if (!dsq->setting) {
    otxt = lives_strdup(lives_standard_button_get_label(LIVES_BUTTON(butt)));
    widget_opts.use_markup = TRUE;
    lives_label_set_text(LIVES_LABEL(dsq->inst_label), _("<b>Change the quota by clicking in the free space area "
                         "in the disk map above,\n"
                         "or by dragging the slider below</b>"));
    widget_opts.use_markup = FALSE;
    lives_widget_set_sensitive(dsq->checkbutton, TRUE);
    lives_widget_set_sensitive(dsq->vvlabel, TRUE);
    lives_widget_set_sensitive(dsq->vlabel, TRUE);
    lives_widget_set_sensitive(dsq->slider, TRUE);
    lives_standard_button_set_label(LIVES_BUTTON(butt), _("APPLY _QUOTA"));
    lives_widget_hide(dsq->note_label);
    lives_widget_set_no_show_all(dsq->resbutton, FALSE);
    lives_widget_show_all(dsq->resbutton);
    dsq->setting = TRUE;
  } else {
    pref_factory_int64(PREF_DISK_QUOTA, future_prefs->disk_quota, TRUE);
    dsu_set_toplabel();
    widget_opts.use_markup = TRUE;
    lives_label_set_text(LIVES_LABEL(dsq->inst_label), _("<b>Updated !</b>"));
    widget_opts.use_markup = FALSE;
    lives_widget_set_frozen(dsq->checkbutton, TRUE, 0.);
    lives_widget_set_frozen(dsq->vvlabel, TRUE, 0.);
    lives_widget_set_frozen(dsq->vlabel, TRUE, 0.);
    lives_widget_set_frozen(dsq->slider, TRUE, 0.);
    lives_standard_button_set_label(LIVES_BUTTON(dsq->button), otxt);
    dsq->setting = FALSE;
    dsu_fill_details(NULL, NULL);
    qslider_changed(dsq->slider, dsq);
  }
}

static void resquota_cb(LiVESWidget * butt, livespointer data) {
  lives_widget_hide(butt);
  lives_widget_show(dsq->note_label);
  lives_label_set_text(LIVES_LABEL(dsq->inst_label), NULL);
  future_prefs->disk_quota = prefs->disk_quota;
  lives_signal_handler_block(dsq->checkbutton, dsq->checkfunc);
  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(dsq->checkbutton), !prefs->disk_quota);
  lives_signal_handler_unblock(dsq->checkbutton, dsq->checkfunc);

  dsq->setting = TRUE;
  changequota_cb(dsq->button, NULL);
  lives_label_set_text(LIVES_LABEL(dsq->inst_label), NULL);
  draw_dsu_widget(mainw->dsu_widget);
}

static void dsu_abort_clicked(LiVESWidget * butt, livespointer data) {
  if (do_abort_check()) lives_abort("User aborted within quota maintenance");
}

void run_diskspace_dialog(const char *target) {
  LiVESWidget *dialog, *dialog_vbox;
  LiVESWidget *layout;
  LiVESWidget *label;
  LiVESWidget *entry;
  LiVESWidget *button;
  LiVESWidget *hbox, *hbox2;
  LiVESWidget *cbut;
  LiVESWidget *okbutton;
  LiVESWidget *rembutton;

  LiVESBox *aar;

  LiVESWidgetColor colr;

  char *title, *tmp, *tmp2;
  char *xtarget = (char *)target;
  int wofl;

  /// kick off a bg process to get free ds and ds used

  if (!dsq) dsq = (_dsquotaw *)lives_calloc(1, sizeof(_dsquotaw));

  dsq->scanning = TRUE;
  if (!target) xtarget = prefs->workdir;
  xtarget = "/data";
  disk_monitor_start(xtarget);

  dsq->setting = FALSE;
  dsq->visible = TRUE;
  dsq->crit_dism = FALSE;

  dsq->exp_layout = NULL;

  if (dsq->dsu_surface) lives_painter_surface_destroy(dsq->dsu_surface);
  dsq->dsu_surface = NULL;

  if (prefsw) lives_widget_hide(prefsw->prefs_dialog);

  title = (_("Disk Space Quota"));

  dialog = lives_standard_dialog_new(title, FALSE, -1, -1);
  lives_signal_handlers_disconnect_by_func(dialog, LIVES_GUI_CALLBACK(return_true), NULL);

  lives_free(title);

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(dialog));
  layout = lives_layout_new(LIVES_BOX(dialog_vbox));

  widget_opts.text_size = LIVES_TEXT_SIZE_LARGE;
  widget_opts.justify = LIVES_JUSTIFY_CENTER;
  dsq->top_label = lives_layout_add_label(LIVES_LAYOUT(layout), NULL, FALSE);
  dsu_set_toplabel();

  widget_opts.justify = LIVES_JUSTIFY_DEFAULT;
  widget_opts.text_size = LIVES_TEXT_SIZE_NORMAL;

  if (prefs->startup_phase) add_fill_to_box(LIVES_BOX(dialog_vbox));
  else {
    lives_layout_add_fill(LIVES_LAYOUT(layout), FALSE);

    hbox = lives_layout_row_new(LIVES_LAYOUT(layout));

    entry = lives_standard_entry_new(_("Current working directory"), prefs->workdir, -1, PATH_MAX,
                                     LIVES_BOX(hbox),
                                     H_("The directory where LiVES will save projects (sets)"));

    lives_entry_set_editable(LIVES_ENTRY(entry), FALSE);

    hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));

    button = lives_standard_button_new_from_stock(LIVES_STOCK_PREFERENCES, _("Change Directory"),
             DEF_BUTTON_WIDTH, DEF_BUTTON_HEIGHT);
    lives_widget_set_focus_on_click(button, FALSE);

    lives_box_pack_start(LIVES_BOX(hbox), button, FALSE, TRUE, widget_opts.packing_width * 4);

    lives_signal_connect(LIVES_GUI_OBJECT(button), LIVES_WIDGET_CLICKED_SIGNAL,
                         LIVES_GUI_CALLBACK(workdir_query_cb), dialog);
  }

  lives_layout_add_row(LIVES_LAYOUT(layout));

  widget_opts.text_size = LIVES_TEXT_SIZE_LARGE;
  widget_opts.use_markup = TRUE;
  lives_layout_add_label(LIVES_LAYOUT(layout), (_("<b>Disk space used by LiVES:</b>")), TRUE);
  widget_opts.use_markup = FALSE;

  dsq->used_label = lives_layout_add_label(LIVES_LAYOUT(layout), NULL, TRUE);

  if (!capable->mountpoint) capable->mountpoint = get_mountpoint_for(prefs->workdir);
  if (capable->mountpoint) {
    char *txt = lives_strdup_printf(_("in %s"), capable->mountpoint);
    lives_layout_add_label(LIVES_LAYOUT(layout), txt, TRUE);
    lives_free(txt);
  }

  widget_opts.text_size = LIVES_TEXT_SIZE_NORMAL;

  add_hsep_to_box(LIVES_BOX(dialog_vbox));

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, FALSE, TRUE, widget_opts.packing_height >> 1);

  if (prefs->startup_phase) add_fill_to_box(LIVES_BOX(dialog_vbox));

  layout = lives_layout_new(LIVES_BOX(dialog_vbox));

  /// dsu widget
  mainw->dsu_widget = lives_standard_drawing_area_new(LIVES_GUI_CALLBACK(all_expose), &dsq->dsu_surface);
  lives_widget_add_events(mainw->dsu_widget, LIVES_BUTTON_PRESS_MASK | LIVES_BUTTON_RELEASE_MASK | LIVES_BUTTON1_MOTION_MASK);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->dsu_widget), LIVES_WIDGET_BUTTON_PRESS_EVENT,
                            LIVES_GUI_CALLBACK(dsu_widget_clicked), LIVES_INT_TO_POINTER(TRUE));

  lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->dsu_widget), LIVES_WIDGET_MOTION_NOTIFY_EVENT,
                            LIVES_GUI_CALLBACK(dsu_widget_clicked), LIVES_INT_TO_POINTER(FALSE));

  lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->dsu_widget), LIVES_WIDGET_BUTTON_RELEASE_EVENT,
                            LIVES_GUI_CALLBACK(dsu_widget_released), NULL);

  lives_box_pack_start(LIVES_BOX(hbox), mainw->dsu_widget, TRUE, TRUE, 0);

  lives_widget_set_size_request(mainw->dsu_widget, -1, widget_opts.css_min_height);

  hbox = lives_hbox_new(TRUE, 0);
  lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, FALSE, TRUE, 0);

  colr.alpha = 1.;

  hbox2 = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(hbox), hbox2, FALSE, TRUE, 0);
  colr.red = colr.green = 0.; colr.blue = 1.;
  cbut = lives_color_button_new_with_color(&colr);
  lives_widget_set_can_focus(cbut, FALSE);
  lives_box_pack_start(LIVES_BOX(hbox2), cbut, FALSE, FALSE, widget_opts.packing_width);
  label = lives_standard_label_new(_("Used by other apps"));
  lives_box_pack_start(LIVES_BOX(hbox2), label, FALSE, FALSE, widget_opts.packing_width);

  hbox2 = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(hbox), hbox2, FALSE, TRUE, 0);
  colr.red = 0.; colr.green = colr.blue = 1.;
  cbut = lives_color_button_new_with_color(&colr);
  lives_widget_set_can_focus(cbut, FALSE);
  lives_box_pack_start(LIVES_BOX(hbox2), cbut, FALSE, FALSE, widget_opts.packing_width);
  label = lives_standard_label_new(_("Used by LiVES"));
  lives_box_pack_start(LIVES_BOX(hbox2), label, FALSE, FALSE, widget_opts.packing_width);

  hbox2 = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(hbox), hbox2, FALSE, TRUE, 0);
  colr.red = colr.green = 1.; colr.blue = 0.;
  cbut = lives_color_button_new_with_color(&colr);
  lives_widget_set_can_focus(cbut, FALSE);
  lives_box_pack_start(LIVES_BOX(hbox2), cbut, FALSE, FALSE, widget_opts.packing_width);
  label = lives_standard_label_new(_("Quota"));
  lives_box_pack_start(LIVES_BOX(hbox2), label, FALSE, FALSE, widget_opts.packing_width);

  hbox2 = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(hbox), hbox2, FALSE, TRUE, 0);
  colr.red = colr.blue = 0.; colr.green = 1.;
  cbut = lives_color_button_new_with_color(&colr);
  lives_widget_set_can_focus(cbut, FALSE);
  lives_box_pack_start(LIVES_BOX(hbox2), cbut, FALSE, FALSE, widget_opts.packing_width);
  label = lives_standard_label_new(_("Free space"));
  lives_box_pack_start(LIVES_BOX(hbox2), label, FALSE, FALSE, widget_opts.packing_width);

  hbox2 = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(hbox), hbox2, FALSE, TRUE, 0);
  colr.red = 1.; colr.green = .5; colr.blue = 0.;
  cbut = lives_color_button_new_with_color(&colr);
  lives_widget_set_can_focus(cbut, FALSE);
  lives_box_pack_start(LIVES_BOX(hbox2), cbut, FALSE, FALSE, widget_opts.packing_width);
  label = lives_standard_label_new(_("Warn level"));
  lives_box_pack_start(LIVES_BOX(hbox2), label, FALSE, FALSE, widget_opts.packing_width);

  hbox2 = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(hbox), hbox2, FALSE, TRUE, 0);
  colr.red = 1.; colr.green = colr.blue = 0.;
  cbut = lives_color_button_new_with_color(&colr);
  lives_widget_set_can_focus(cbut, FALSE);
  lives_box_pack_start(LIVES_BOX(hbox2), cbut, FALSE, FALSE, widget_opts.packing_width);
  label = lives_standard_label_new(_("Critical level"));
  lives_box_pack_start(LIVES_BOX(hbox2), label, FALSE, FALSE, widget_opts.packing_width);

  //// expander section ////
  dsq->exp_vbox = lives_vbox_new(FALSE, 0);

  if (prefs->startup_phase) add_fill_to_box(LIVES_BOX(dialog_vbox));

  layout = lives_layout_new(LIVES_BOX(dialog_vbox));
  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
  dsq->expander = lives_standard_expander_new(tmp = _("<b>Click for Full _Details</b>"),
                  tmp2 = _("<b>Hide _Details</b>"), LIVES_BOX(hbox), dsq->exp_vbox);
  lives_free(tmp); lives_free(tmp2);
  lives_layout_expansion_row_new(LIVES_LAYOUT(layout), dsq->expander);

  lives_signal_sync_connect_after(LIVES_GUI_OBJECT(dsq->expander), LIVES_WIDGET_ACTIVATE_SIGNAL,
                                  LIVES_GUI_CALLBACK(dsu_fill_details), dsq);

  dsu_fill_details(dsq->expander, dsq);

  lives_layout_add_row(LIVES_LAYOUT(layout));

  wofl = widget_opts.filler_len;
  widget_opts.filler_len = def_widget_opts.filler_len * 6;
  lives_layout_add_fill(LIVES_LAYOUT(layout), TRUE);
  widget_opts.filler_len = wofl;

  if (prefs->startup_phase) lives_layout_add_fill(LIVES_LAYOUT(layout), FALSE);

  widget_opts.use_markup = TRUE;
  if (!mainw->has_session_workdir)
    dsq->note_label = lives_layout_add_label(LIVES_LAYOUT(layout), (_("Note: LiVES cannot <b>guarantee"
                      "</b> not to exceed its quota\n"
                      "but it can warn you if this is detected.")), TRUE);
  else
    dsq->note_label = lives_layout_add_label(LIVES_LAYOUT(layout), (_("<b>Quota checking is disabled when workdir\n"
                      "is set via commandline option.</b>")), TRUE);

  widget_opts.use_markup = FALSE;
  hbox = widget_opts.last_container;

  dsq->resbutton = lives_standard_button_new_from_stock(LIVES_STOCK_UNDO, _("_Reset"),
                   DEF_BUTTON_WIDTH, DEF_BUTTON_HEIGHT);
  lives_widget_set_focus_on_click(dsq->resbutton, FALSE);

  lives_box_pack_start(LIVES_BOX(hbox), dsq->resbutton, FALSE, FALSE, widget_opts.packing_width * 4);

  lives_signal_sync_connect_after(LIVES_GUI_OBJECT(dsq->resbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                                  LIVES_GUI_CALLBACK(resquota_cb), NULL);

  lives_widget_set_no_show_all(dsq->resbutton, TRUE);

  // quota button

  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));

  widget_opts.use_markup = TRUE;
  dsq->button = lives_standard_button_new_from_stock(LIVES_STOCK_PREFERENCES, _("<b>Change _Quota</b>"),
                DEF_BUTTON_WIDTH, DEF_BUTTON_HEIGHT);
  widget_opts.use_markup = FALSE;
  lives_widget_set_focus_on_click(dsq->button, FALSE);

  lives_box_pack_start(LIVES_BOX(hbox), dsq->button, FALSE, FALSE, widget_opts.packing_width * 4);

  lives_signal_sync_connect_after(LIVES_GUI_OBJECT(dsq->button), LIVES_WIDGET_CLICKED_SIGNAL,
                                  LIVES_GUI_CALLBACK(changequota_cb), NULL);

  dsq->inst_label = lives_layout_add_label(LIVES_LAYOUT(layout), NULL, TRUE);

  if (prefs->startup_phase) add_fill_to_box(LIVES_BOX(dialog_vbox));

  layout = lives_layout_new(LIVES_BOX(dialog_vbox));

  lives_layout_add_label(LIVES_LAYOUT(layout), (_("Quota:")), TRUE);

  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
  widget_opts.use_markup = TRUE;
  dsq->checkbutton =
    lives_standard_check_button_new((tmp = (_("<big><b>Unlimited</b></big>"))), future_prefs->disk_quota == 0,
                                    LIVES_BOX(hbox), NULL);
  widget_opts.use_markup = FALSE;

  lives_widget_set_frozen(dsq->checkbutton, TRUE, 0.);
  lives_widget_set_frozen(widget_opts.last_label, TRUE, 0.);

  dsq->checkfunc = lives_signal_sync_connect_after(LIVES_GUI_OBJECT(dsq->checkbutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                   LIVES_GUI_CALLBACK(dsq_check_toggled), NULL);

  dsq->vvlabel = lives_layout_add_label(LIVES_LAYOUT(layout), (_("Value:")), TRUE);
  lives_widget_set_frozen(dsq->vvlabel, TRUE, 0.);
  dsq->vlabel = label = lives_layout_add_label(LIVES_LAYOUT(layout), NULL, TRUE);
  lives_widget_set_frozen(dsq->vlabel, TRUE, 0.);
  lives_label_set_width_chars(LIVES_LABEL(dsq->vlabel), 12);

  add_fill_to_box(LIVES_BOX(lives_widget_get_parent(label)));

  dsq->slider = lives_standard_hscale_new(NULL);
  lives_widget_set_size_request(dsq->slider, DEF_SLIDER_WIDTH * 3, widget_opts.css_min_height);
  lives_widget_set_sensitive(dsq->slider, TRUE);
  lives_range_set_range(LIVES_RANGE(dsq->slider), 0., 100.);

  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
  lives_layout_pack(LIVES_HBOX(hbox), dsq->slider);

  lives_widget_set_sensitive(dsq->slider, FALSE);

  dsq->sliderfunc = lives_signal_sync_connect_after(LIVES_GUI_OBJECT(dsq->slider), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                    LIVES_GUI_CALLBACK(qslider_changed), NULL);

  dsq->pculabel = lives_layout_add_label(LIVES_LAYOUT(layout), NULL, TRUE);

  if (prefs->startup_phase) add_fill_to_box(LIVES_BOX(dialog_vbox));

  if (!prefs->startup_phase) {
    add_hsep_to_box(LIVES_BOX(dialog_vbox));

    layout = lives_layout_new(LIVES_BOX(dialog_vbox));

    lives_layout_add_label(LIVES_LAYOUT(layout), _("Diskspace Management Options"), FALSE);
    lives_layout_add_fill(LIVES_LAYOUT(layout), FALSE);

    widget_opts.expand = LIVES_EXPAND_EXTRA_WIDTH | LIVES_EXPAND_DEFAULT_HEIGHT;
    lives_layout_add_fill(LIVES_LAYOUT(layout), TRUE);
    widget_opts.expand = LIVES_EXPAND_DEFAULT;

    hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
    button = lives_standard_button_new_from_stock(LIVES_STOCK_PREFERENCES, _("Clean Up Diskspace"),
             DEF_BUTTON_WIDTH, DEF_BUTTON_HEIGHT);
    lives_widget_set_focus_on_click(button, FALSE);
    lives_box_pack_start(LIVES_BOX(hbox), button, FALSE, FALSE, widget_opts.packing_width * 4);

    lives_signal_connect(LIVES_GUI_OBJECT(button), LIVES_WIDGET_CLICKED_SIGNAL,
                         LIVES_GUI_CALLBACK(cleards_cb), dialog);

    lives_layout_add_fill(LIVES_LAYOUT(layout), TRUE);
    hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));

    button = lives_standard_button_new_from_stock(LIVES_STOCK_PREFERENCES, _("Manage Clip Sets"),
             DEF_BUTTON_WIDTH, DEF_BUTTON_HEIGHT);
    lives_widget_set_focus_on_click(button, FALSE);

    lives_box_pack_start(LIVES_BOX(hbox), button, FALSE, FALSE, widget_opts.packing_width * 4);

    lives_signal_sync_connect(LIVES_GUI_OBJECT(button), LIVES_WIDGET_CLICKED_SIGNAL,
                              LIVES_GUI_CALLBACK(manclips_cb),  dialog);

    if (mainw->num_sets == -1) {
      mainw->set_list = get_set_list(prefs->workdir, TRUE);
      if (mainw->set_list) {
        mainw->num_sets = lives_list_length(mainw->set_list);
        if (mainw->was_set) mainw->num_sets--;
      } else mainw->num_sets = 0;
    }

    if (mainw->num_sets <= 0) lives_widget_set_sensitive(button, FALSE);

    lives_layout_add_fill(LIVES_LAYOUT(layout), TRUE);
    hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));

    button = lives_standard_button_new_from_stock(LIVES_STOCK_CLOSE, _("Close Current Clips"),
             DEF_BUTTON_WIDTH, DEF_BUTTON_HEIGHT);
    lives_widget_set_focus_on_click(button, FALSE);

    //if (!mainw->clips_available) lives_widget_set_sensitive(button, FALSE);
    lives_widget_set_sensitive(button, FALSE); // TODO

    lives_box_pack_start(LIVES_BOX(hbox), button, FALSE, FALSE, widget_opts.packing_width * 4);

    widget_opts.expand = LIVES_EXPAND_EXTRA_WIDTH | LIVES_EXPAND_DEFAULT_HEIGHT;
    lives_layout_add_fill(LIVES_LAYOUT(layout), TRUE);
    widget_opts.expand = LIVES_EXPAND_DEFAULT;
  }
  ////

  add_fill_to_box(LIVES_BOX(dialog_vbox));

  aar = LIVES_BOX(lives_dialog_get_action_area(LIVES_DIALOG(dialog)));

  rembutton =
    lives_standard_check_button_new(_("Show this dialog on startup"), prefs->show_disk_quota, aar,
                                    (tmp = lives_strdup(H_("These settings can also be changed "
                                        "in Preferences / Warnings"))));

  lives_signal_sync_connect(LIVES_GUI_OBJECT(rembutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(toggle_sets_pref), PREF_SHOW_QUOTA);

  lives_button_box_make_first(LIVES_BUTTON_BOX(aar), widget_opts.last_container);

  widget_opts.use_markup = TRUE;
  dsq->noqlabel = lives_standard_label_new(_("<big><b>No quota has been set</b></big>"));
  widget_opts.use_markup = FALSE;
  lives_box_pack_start(LIVES_BOX(aar), dsq->noqlabel, FALSE, TRUE, widget_opts.packing_width);

  dsq->abort_button = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_QUIT,
                      _("Abort"), LIVES_RESPONSE_ABORT);

  lives_button_uncenter(dsq->abort_button, DLG_BUTTON_WIDTH * 2.);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(dsq->abort_button), LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(dsu_abort_clicked),
                            prefsw ? prefsw->prefs_dialog : NULL);
  lives_widget_set_no_show_all(dsq->abort_button, TRUE);

  if (prefs->startup_phase)
    okbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_OK,
               _("FINISH"), LIVES_RESPONSE_OK);
  else
    okbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_OK,
               _("Continue with current values"), LIVES_RESPONSE_OK);

  lives_button_uncenter(okbutton, DLG_BUTTON_WIDTH * 2.);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(okbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(dsu_ok_clicked),
                            prefsw ? prefsw->prefs_dialog : NULL);

  lives_button_grab_default_special(okbutton);
  lives_widget_grab_focus(okbutton);

  lives_widget_show_all(dialog);

  qslider_changed(dsq->slider, NULL);

  if (prefs->startup_phase) {
    char *wid = lives_strdup_printf("0x%08lx", (uint64_t)LIVES_XWINDOW_XID(lives_widget_get_xwindow(dialog)));
    if (!wid || !activate_x11_window(wid)) lives_window_set_keep_above(LIVES_WINDOW(dialog), TRUE);
    lives_dialog_run(LIVES_DIALOG(dialog));
  } else lives_idle_add_simple(update_dsu, NULL);
  if (mainw->cs_manage && mainw->num_sets) {
    mainw->cs_manage = FALSE;
    manclips_cb(NULL, dialog);
  } else mainw->cs_manage = FALSE;
}


rec_args *do_rec_desk_dlg(void) {
  _resaudw *resaudw;
  rec_args *recargs = NULL;
  LiVESWidget *dialog = lives_standard_dialog_new(_("Record Desktop (LiVES Main Window)"), TRUE, -1, -1);
  LiVESWidget *dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(dialog));
  LiVESWidget *layout = lives_layout_new(LIVES_BOX(dialog_vbox)), *hbox, *vbox;
  LiVESWidget *spinh, *spinm, *spins, *spinf, *checkbutton;
  LiVESWidget *spindh, *spindm, *spinds, *checkbuttond;
  LiVESWidget *cb_aud, *cb_vover, *rb_win;
  LiVESSList *rb_group = NULL;
  LiVESResponseType response;

  lives_layout_add_label(LIVES_LAYOUT(layout),
                         _("This tool may be used to record either the entire desktop or just the area covered "
                           "by the LiVES main window.\nIf Voiceover Mode is enabled, then "
                           "both external and internal audio sources may be recorded and mixed.\n"
                           "Note that recording is linked to the screen area, rather then the window itself - "
                           "thus anything placed over the window will also be recorded."), FALSE);

  lives_layout_add_row(LIVES_LAYOUT(layout));
  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  lives_layout_add_label(LIVES_LAYOUT(layout), _("Screen area to record:"), TRUE);

  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
  rb_win = lives_standard_radio_button_new(_("LiVES main _window"), &rb_group, LIVES_BOX(hbox), NULL);

  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
  lives_standard_radio_button_new(_("Entire _desktop"), &rb_group, LIVES_BOX(hbox), NULL);

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(rb_win), TRUE);

  lives_layout_add_label(LIVES_LAYOUT(layout), _("Max recording time (unless cancelled from the menu)"), FALSE);
  lives_layout_add_row(LIVES_LAYOUT(layout));
  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));

  checkbutton = lives_standard_check_button_new(_("Unlimited (until cancelled from the menu)"), TRUE, LIVES_BOX(hbox), NULL);
  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
  spinh = lives_standard_spin_button_new(_("_Hours"), 0., 0., 23., 1., 1., 0., LIVES_BOX(hbox), NULL);
  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
  spinm = lives_standard_spin_button_new(_("_Minutes"), 0., 0., 59., 1., 1., 0., LIVES_BOX(hbox), NULL);
  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
  spins = lives_standard_spin_button_new(_("_Seconds"), 10., 0., 59., 1., 1., 0., LIVES_BOX(hbox), NULL);

  toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(checkbutton), spinh, TRUE);
  toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(checkbutton), spinm, TRUE);
  toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(checkbutton), spins, TRUE);

  lives_layout_add_fill(LIVES_LAYOUT(layout), FALSE);

  lives_layout_add_label(LIVES_LAYOUT(layout), _("Delay before starting"), FALSE);
  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));

  checkbuttond = lives_standard_check_button_new(_("Immediate"), FALSE, LIVES_BOX(hbox), NULL);

  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
  spindh = lives_standard_spin_button_new(_("_Hours"), 0., 0., 23., 1., 1., 0., LIVES_BOX(hbox), NULL);
  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
  spindm = lives_standard_spin_button_new(_("_Minutes"), 0., 0., 59., 1., 1., 0., LIVES_BOX(hbox), NULL);
  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
  spinds = lives_standard_spin_button_new(_("_Seconds"), 10., 0., 59., 1., 1., 0., LIVES_BOX(hbox), NULL);

  toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(checkbuttond), spindh, TRUE);
  toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(checkbuttond), spindm, TRUE);
  toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(checkbuttond), spinds, TRUE);

  lives_layout_add_fill(LIVES_LAYOUT(layout), FALSE);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  spinf = lives_standard_spin_button_new(_("_FPS"), DEF_FPS, 1., FPS_MAX, 1., 1., 2., LIVES_BOX(hbox), NULL);

  lives_layout_add_fill(LIVES_LAYOUT(layout), FALSE);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  cb_aud = lives_standard_check_button_new(_("Record internal audio"), FALSE, LIVES_BOX(hbox), NULL);

  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
  cb_vover = lives_standard_check_button_new(_("Voiceover mode"), FALSE, LIVES_BOX(hbox),
             H_("Record external audio regardless of the "
                "audio source set in the player.\n"
                "If the player source is set to 'Internal', "
                "then both audio streams may be mixed together.\n"
                "If unset, only internal audio will be recorded."));
  mainw->fx1_val = DEFAULT_AUDIO_RATE;
  mainw->fx2_val = DEFAULT_AUDIO_CHANS;
  mainw->fx3_val = DEFAULT_AUDIO_SAMPS;
  mainw->fx4_val = mainw->endian;

  vbox = lives_vbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(dialog_vbox), vbox, FALSE, FALSE, widget_opts.packing_height);

  resaudw = create_resaudw(6, NULL, vbox);

  toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(cb_aud), vbox, FALSE);
  toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(cb_aud), cb_vover, FALSE);

  response = lives_dialog_run(LIVES_DIALOG(dialog));

  if (response == LIVES_RESPONSE_OK) {
    recargs = lives_calloc(sizeof(rec_args), 1);
    recargs->scale = 1.;
    if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(rb_win)))
      recargs->screen_area = SCREEN_AREA_FOREGROUND;
    else
      recargs->screen_area = SCREEN_AREA_BACKGROUND;

    recargs->fps = lives_spin_button_get_value(LIVES_SPIN_BUTTON(spinf));
    if (!lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(checkbuttond))) {
      recargs->delay_time = (lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(spindh)) * 60
                             + lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(spindm))) * 60
                            + lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(spinds));
    }
    if (!lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(checkbutton))) {
      recargs->rec_time = (lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(spinh)) * 60
                           + lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(spinm))) * 60
                          + lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(spins));
    }
    if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(cb_aud))) {
      recargs->arate = (int)atoi(lives_entry_get_text(LIVES_ENTRY(resaudw->entry_arate)));
      recargs->achans = (int)atoi(lives_entry_get_text(LIVES_ENTRY(resaudw->entry_achans)));
      recargs->asamps = (int)atoi(lives_entry_get_text(LIVES_ENTRY(resaudw->entry_asamps)));
      if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(resaudw->rb_unsigned))) {
        recargs->signed_endian = AFORM_UNSIGNED;
      } else recargs->signed_endian = AFORM_SIGNED;
      if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(resaudw->rb_bigend))) {
        recargs->signed_endian |= AFORM_BIG_ENDIAN;
      } else recargs->signed_endian |= AFORM_LITTLE_ENDIAN;
    }
    lives_free(resaudw);
    recargs->duplex = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(cb_vover));
    lives_widget_destroy(dialog);
  }
  return recargs;
}


//// message area functions
//#define DEBUG_OVERFLOW

static int vmin = -10000000;
static int hmin = -10000000;
static int reqheight = -1; // presumed height of msg_area
static int reqwidth = -1; // presumed width of msg_area


boolean get_screen_usable_size(int *w, int *h) {
  *w = GUI_SCREEN_WIDTH - ((hmin > 0) ? hmin : 0);
  *h = GUI_SCREEN_HEIGHT - ((vmin > 0) ? vmin : 0);
  if (vmin > 0 || hmin > 0) return TRUE;
  return FALSE;
}


static boolean msg_area_scroll_to(LiVESWidget * widget, int msgno, boolean recompute, LiVESAdjustment * adj) {
  // "scroll" the message area so that the last message appears at the bottom
  // widget is mainw->msg_area (or mt->msg_area)
  LingoLayout *layout;
  lives_colRGBA64_t fg, bg;

  int width;
  int height = -1, lh;
  int nlines;

  static int last_height = -1;

  if (!prefs->show_msg_area) return FALSE;
  if (mainw->n_messages <= 0) return FALSE;

  if (!LIVES_IS_WIDGET(widget)) return FALSE;

  height = lives_widget_get_allocation_height(LIVES_WIDGET(widget));
  //if (reqheight != -1) height = reqheight;
  width = lives_widget_get_allocation_width(LIVES_WIDGET(widget));
  if (reqwidth != -1) width = reqwidth;
  //g_print("GET  LINGO xx %d %d\n", width, height);

  layout = (LingoLayout *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(widget), "layout");
  if (layout) {
    if (LINGO_IS_LAYOUT(layout)) lingo_layout_set_text(layout, "", -1);
    lives_widget_object_unref(layout);
  }
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(widget), "layout", NULL);

  if (width < LAYOUT_SIZE_MIN || height < LAYOUT_SIZE_MIN) return FALSE;

  if (msgno < 0) msgno = 0;
  if (msgno >= mainw->n_messages) msgno = mainw->n_messages - 1;

  // redraw the layout ///////////////////////
  lives_widget_set_text_size(widget, LIVES_WIDGET_STATE_NORMAL, lives_textsize_to_string(prefs->msg_textsize));

  layout = layout_nth_message_at_bottom(msgno, width, height, LIVES_WIDGET(widget), &nlines);
  if (!LINGO_IS_LAYOUT(layout) || !layout) {
    return FALSE;
  }

  lingo_layout_get_size(layout, NULL, &lh);
  lh /= LINGO_SCALE;
  if (height != last_height) recompute = TRUE;
  last_height = height;

  if (recompute) {
    // redjust the page size
    if (nlines > 0) {
      double linesize = lh / nlines;
      double page_size = (double)((int)((double)height / linesize));
      //g_print("VALS3 lh = %d, nlines = %d, lsize = %f, height = %d, ps = %f\n", lh, nlines, linesize, height, page_size);
      if (mainw->multitrack)
        lives_signal_handler_block(mainw->multitrack->msg_adj, mainw->mt_msg_adj_func);
      else
        lives_signal_handler_block(mainw->msg_adj, mainw->msg_adj_func);
      lives_widget_object_freeze_notify(LIVES_WIDGET_OBJECT(adj));
      lives_adjustment_set_lower(adj, page_size);
      lives_adjustment_set_upper(adj, (double)(mainw->n_messages + page_size - 2));
      lives_adjustment_set_page_size(adj, page_size);
      lives_adjustment_set_value(adj, (double)msgno);
      lives_widget_object_thaw_notify(LIVES_WIDGET_OBJECT(adj));
      if (mainw->multitrack)
        lives_signal_handler_unblock(mainw->multitrack->msg_adj, mainw->mt_msg_adj_func);
      else
        lives_signal_handler_unblock(mainw->msg_adj, mainw->msg_adj_func);
      //g_print("PAGE SIZE is %f\n", page_size);
    }
  }

  widget_color_to_lives_rgba(&fg, &palette->info_text);
  widget_color_to_lives_rgba(&bg, &palette->info_base);

  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(widget), "layout", layout);
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(widget), "layout_height", LIVES_INT_TO_POINTER(lh + .5));
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(widget), "layout_lines", LIVES_INT_TO_POINTER(nlines));
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(widget), "layout_last", LIVES_INT_TO_POINTER(msgno));

  return TRUE;
}

//#define DEBUG_OVERFLOW
static int height, lheight;

boolean msg_area_config(LiVESWidget * widget) {
  // widget is mainw->msg_area :- the drawing area we write messages to
  static int wiggle_room = 0;
  static int last_height = -1;
  static int last_textsize = -1;

  static int old_scr_width = -1;
  static int old_scr_height = -1;

  static int last_overflowy = 10000000;
  static int last_overflowx = 10000000;

  static int gui_posx = 1000000;
  static int gui_posy = 1000000;

  static boolean norecurse = FALSE;

  LingoLayout *layout;
  lives_rect_t rect;

  boolean mustret = FALSE;

  int width;
  int lineheight, llines, llast;
  int scr_width = GUI_SCREEN_WIDTH;
  int scr_height = mainw->mgeom[0].phys_height;
  int bx, by, w = -1, h = -1, posx, posy;
  int overflowx = 0, overflowy = 0;
  int ww, hh, vvmin, hhmin;
  int paisize = 0, opaisize;

  if (norecurse) return FALSE;

  if (!mainw->is_ready) return FALSE;
  if (!prefs->show_msg_area) return FALSE;
  if (LIVES_IS_PLAYING && prefs->msgs_nopbdis) return FALSE;

  if (mainw->multitrack && lives_widget_get_allocation_height(mainw->multitrack->top_vbox) < 32)
    return FALSE;

  //lives_widget_set_vexpand(widget, TRUE);

  layout = (LingoLayout *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(widget), "layout");

  if (last_textsize == -1) last_textsize = prefs->msg_textsize;

  width = lives_widget_get_allocation_width(widget);
  height = lives_widget_get_allocation_height(widget);

  if (reqwidth != -1) width = reqwidth;
  reqwidth = -1;
  if (reqheight != -1) height = reqheight;
  reqheight = -1;

  llast = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(widget),
                               "layout_last"));

  if (mainw->is_ready && (scr_width != old_scr_width || scr_height != old_scr_height)) {
    vmin = -10000000;
    hmin = -10000000;
    reqheight = -1; // presumed height of msg_area
    reqwidth = -1; // presumed width of msg_area
    wiggle_room = 0;
    last_height = -1;
    last_textsize = -1;

    last_overflowy = 10000000;
    last_overflowx = 10000000;

    old_scr_height = scr_height;
    old_scr_width = scr_width;
    gui_posx = gui_posy = 1000000;
  }

  lives_xwindow_get_frame_extents(lives_widget_get_xwindow(LIVES_MAIN_WINDOW_WIDGET), &rect);

  get_border_size(LIVES_MAIN_WINDOW_WIDGET, &bx, &by);

  scr_height -= abs(by);
  scr_width -= abs(bx);

  ww = lives_widget_get_allocation_width(LIVES_MAIN_WINDOW_WIDGET);
  w = mainw->assumed_width;
  if (w == -1) w = ww;
  hh = lives_widget_get_allocation_height(LIVES_MAIN_WINDOW_WIDGET);
  h = mainw->assumed_height;
  if (h == -1) h = hh;

  if (!mainw->multitrack) {
    overflowx = ww - scr_width;
    overflowy = hh - scr_height;

#ifdef DEBUG_OVERFLOW
    g_print("ADJ A %d = %d - (%d - %d) + (%d - %d) %d %d\n", overflowy, h, scr_height, by, hh,
            mainw->assumed_height, ABS(overflowy), vmin);

    g_print("ADJ A2 %d = %d - (%d) + (%d - %d - %d) %d %d\n", overflowx, w, scr_width, ww,
            mainw->assumed_width, bx, ABS(overflowx), hmin);
#endif

    if (ABS(overflowx) <= hmin) overflowx = 0;
    if (ABS(overflowy) <= vmin) overflowy = 0;

#ifdef DEBUG_OVERFLOW
    g_print("overflow2 is %d : %d %d %d X %d : %d %d %d [%d %d %d]\n", overflowx, w, scr_width, bx, overflowy,
            h, scr_height, by, h,
            rect.height, lives_widget_get_allocation_height(LIVES_MAIN_WINDOW_WIDGET));
#endif

    if (overflowx != 0 && w <= scr_width && ww <= scr_width && overflowx == last_overflowx) {
      int xhmin = ABS(overflowx);
      if (xhmin < ABS(hmin)) {
        hmin = xhmin;
        mustret = TRUE;
      }
    }
    last_overflowx = overflowx;

#ifdef DEBUG_OVERFLOW
    g_print("NOW %d %d %d %d %d\n", overflowy, h, scr_height, hh, last_overflowy);
#endif
    if (overflowy != 0 && h <= scr_height && hh <= scr_height && overflowy == last_overflowy) {
      int xvmin = ABS(overflowy);
      if (xvmin < ABS(vmin)) {
        vmin = xvmin;
        mustret = TRUE;
      }
    }

    last_overflowy = overflowy;

    width = lives_widget_get_allocation_width(widget);
    height = lives_widget_get_allocation_height(widget);

    if (reqwidth != -1) width = reqwidth;
    if (reqheight != -1) height = reqheight;

#ifdef DEBUG_OVERFLOW
    g_print("WIDG SIZE %d X %d, %d,%d and %d %d %d\n", width, height, hmin, vmin, bx, by, mustret);
#endif

    vvmin = by - vmin;
    if (vvmin < by && by - vmin < vmin) vmin = by - vmin;

    hhmin = bx - hmin;
    if (hhmin < bx && bx - hmin < hmin) hmin = bx - hmin;

    if (mustret) {
      lives_widget_queue_draw(mainw->msg_area);
      return FALSE;
    }
  }

  if (overflowx != 0 || overflowy != 0) {
#ifdef DEBUG_OVERFLOW
    g_print("overflow is %d X %d : %d %d\n", overflowx, overflowy, width, height);
#endif
    width -= overflowx;
    height -= overflowy;

    if (!mainw->multitrack) {
      if (height <= MIN_MSGBAR_HEIGHT) {
        height = MIN_MSGBAR_HEIGHT;
        mainw->mbar_res = height;
        if (!LIVES_IS_PLAYING && CURRENT_CLIP_IS_VALID && !THREADVAR(fg_service)
            && !FG_THREADVAR(fg_service)) {
          norecurse = TRUE;
          redraw_timeline(mainw->current_file);
          norecurse = FALSE;
        }
        if (width < 0 || height < 0) return FALSE;
      }
    }

    w = ww - overflowx + abs(bx);
    h = hh - overflowy + abs(by);

    if (!prefs->open_maximised) {
      mainw->assumed_width = w;
      mainw->assumed_height = h;

#ifdef DEBUG_OVERFLOW
      g_print("resize mainwin to %d X %d\n", w, h);
#endif
      lives_window_resize(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET), w, h);

      lives_xwindow_get_origin(lives_widget_get_xwindow(LIVES_MAIN_WINDOW_WIDGET), &posx, &posy);

#ifdef DEBUG_OVERFLOW
      g_print("2MOVE to %d X %d\n", posx, posy);
#endif
    } else {
      mainw->assumed_width = w;//rect.width;// - overflowx;// - bx;
      mainw->assumed_height = h;//rect.height;// - overflowy;// - by;
    }

    if (!prefs->open_maximised) {
      /* if (overflowx > 0) posx -= overflowx; */
      /* else posx = -overflowx; */
      /* //if (posx < 0) posx = 0; */
      /* if (overflowy > 0) posy = overflowy - posy; */
      /* //if (posy < 0) posy = 0; */

      /* if (posx > gui_posx) posx = gui_posx; */
      /* if (posy > gui_posy) posy = gui_posy; */

      //get_border_size(LIVES_MAIN_WINDOW_WIDGET, &bx, &by);

#ifdef DEBUG_OVERFLOW
      g_print("MOVE to %d X %d : %d %d\n", posx, posy, bx, by);
#endif
      lives_window_move_resize(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET), posx - bx, posy - by, w, h);

      gui_posx = posx;
      gui_posy = posy;
    }

    if (height > 0 && width > 0) {
      if (mainw->multitrack) {
        if (height <= MIN_MSGBAR_HEIGHT) {
          int pos = lives_paned_get_position(LIVES_PANED(mainw->multitrack->top_vpaned));
          pos = pos + height - MIN_MSGBAR_HEIGHT;
          height = MIN_MSGBAR_HEIGHT;
          lives_container_child_set_shrinkable(LIVES_CONTAINER(mainw->multitrack->top_vpaned),
                                               mainw->multitrack->vpaned, FALSE);
          lives_paned_set_position(LIVES_PANED(mainw->multitrack->top_vpaned), pos);
        } else
          lives_container_child_set_shrinkable(LIVES_CONTAINER(mainw->multitrack->top_vpaned),
                                               mainw->multitrack->vpaned, TRUE);
      } else {
        if (mainw->mbar_res && height >= mainw->mbar_res * 2) {
          mainw->mbar_res = 0;
          height -= mainw->mbar_res;
        }
      }

#ifdef DEBUG_OVERFLOW
      g_print("SIZE REQ: %d X %d and %d X %d\n", width, height, w, h);
#endif

      // NECESSARY !
      lives_window_move_resize(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET), abs(bx), abs(by), w, h);

      if (width + overflowx > scr_width) {
        lives_widget_set_size_request(widget, width, height);
        //lives_widget_set_size_request(lives_widget_get_parent(widget), width, height);
        reqwidth = width;
      } else {
        lives_widget_set_size_request(widget, -1, height);
        //lives_widget_set_size_request(lives_widget_get_parent(widget), -1, height);
        reqwidth = width + overflowx;
      }
      reqheight = height;
    }

    if (prefs->show_msg_area) lives_widget_show_all(mainw->message_box);

    if (!prefs->open_maximised)
      lives_window_move(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET), posx, posy);
    else
      lives_window_maximize(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET));
  }

  opaisize = paisize;
  paisize = lives_widget_get_allocation_width(lives_widget_get_parent(widget));

  if (!layout || !LINGO_IS_LAYOUT(layout) || paisize != opaisize) {
    // this can happen e.g if we open the app. with no clips
    msg_area_scroll_to_end(widget, mainw->msg_adj);

    // reget this as it may have changed
    layout = (LingoLayout *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(widget), "layout");
    if (!layout || !LINGO_IS_LAYOUT(layout)) {
      return FALSE;
    }
  }

  if (!prefs->open_maximised && !mainw->multitrack && gui_posx < 1000000)
    lives_window_move(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET), gui_posx, gui_posy);

  gui_posx = gui_posy = 1000000;

  // check if we could request more
  lheight = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(widget), "layout_height"));
  if (lheight == 0) return FALSE;

  if (height != last_height) wiggle_room = 0;
  last_height = height;

#ifdef DEBUG_OVERFLOW
  g_print("VALS %d, %d %d\n", lheight, height, wiggle_room);
#endif

  llines = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(widget), "layout_lines"));
  lineheight = CEIL(lheight / llines, 1);

  if (height / lineheight < MIN_MSGBOX_LLINES) {
    /// try a smaller font size if we can
    if (prefs->msg_textsize > 1) prefs->msg_textsize--;
    else if (height < lineheight) return FALSE;
    mainw->max_textsize = prefs->msg_textsize;
  }

  if (lheight < height - wiggle_room || prefs->msg_textsize != last_textsize) {
#ifdef DEBUG_OVERFLOW
    g_print("VALS2 %d %d %d : %d %d\n", height / lineheight, llines + 1, llast, prefs->msg_textsize, last_textsize);
#endif
    if ((height / lineheight >= llines + 1 && llast > llines) || (prefs->msg_textsize != last_textsize)) {
#ifdef DEBUG_OVERFLOW
      g_print("VALS22 %d %d %d : %d %d\n", height / lineheight, llines + 1, llast, prefs->msg_textsize, last_textsize);
#endif
      // recompute if the window grew or the text size changed
      last_textsize = prefs->msg_textsize;
      msg_area_scroll_to(widget, llast, TRUE, mainw->msg_adj); // window grew, re-get layout
      layout = (LingoLayout *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(widget), "layout");
      if (!layout || !LINGO_IS_LAYOUT(layout)) {
        return FALSE;
      }
      lheight = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(widget),
                                     "layout_height"));
      wiggle_room = height - lheight;
    }
  }
  return FALSE;
}


boolean reshow_msg_area(LiVESWidget * widget, lives_painter_t *cr, livespointer psurf) {
  lives_painter_t *cr2;
  LingoLayout *layout;

  if (!prefs->show_msg_area) return TRUE;

  layout = (LingoLayout *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(widget), "layout");

  if (layout && LINGO_IS_LAYOUT(layout)) {
    lives_colRGBA64_t fg, bg;
    int rwidth = lives_widget_get_allocation_width(widget);
    int rheight = lives_widget_get_allocation_height(widget);

    widget_color_to_lives_rgba(&fg, &palette->info_text);
    widget_color_to_lives_rgba(&bg, &palette->info_base);

    cr2 = lives_painter_create_from_surface(mainw->msg_surface);
    lives_painter_render_background(widget, cr2, 0., 0., rwidth, rheight);

    height = lives_widget_get_allocation_height(widget);
    //bg.alpha = 0.5;
    layout_to_lives_painter(layout, cr2, LIVES_TEXT_MODE_FOREGROUND_AND_BACKGROUND, &fg, &bg, rwidth, rheight,
                            0., 0., 0., height - lheight - 4);
    lingo_painter_show_layout(cr2, layout);
    lives_painter_destroy(cr2);

  }
  lives_painter_set_source_surface(cr, mainw->msg_surface, 0., 0.);
  lives_painter_paint(cr);

  return FALSE;
}


LIVES_GLOBAL_INLINE void msg_area_scroll_to_end(LiVESWidget * widget, LiVESAdjustment * adj) {
  if (!prefs->show_msg_area) return;
  msg_area_scroll_to(widget, mainw->n_messages - 2, TRUE, adj);
}


void msg_area_scroll(LiVESAdjustment * adj, livespointer userdata) {
  // scrollbar callback
  LiVESWidget *widget = (LiVESWidget *)userdata;
  double val;
  if (!prefs->show_msg_area) return;
  if (!LIVES_IS_ADJUSTMENT(adj)) return;
  val = lives_adjustment_get_value(adj);
  //g_print("val is %f rnd %d\n", val, (int)(val + .5));
  if (msg_area_scroll_to(widget, (int)(val + .5), FALSE, adj))
    lives_widget_queue_draw(widget);
  //reshow_msg_area(widget);
}


boolean on_msg_area_scroll(LiVESWidget * widget, LiVESXEventScroll * event, livespointer user_data) {
  // mouse scroll callback
  LiVESAdjustment *adj = (LiVESAdjustment *)user_data;
  if (lives_get_scroll_direction(event) == LIVES_SCROLL_UP) lives_adjustment_set_value(adj, lives_adjustment_get_value(adj) - 1.);
  if (lives_get_scroll_direction(event) == LIVES_SCROLL_DOWN) lives_adjustment_set_value(adj,
        lives_adjustment_get_value(adj) + 1.);
  return FALSE;
}
