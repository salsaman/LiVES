
// multitrack-gui.c
// LiVES
// (c) G. Finch 2005 - 2020 <salsaman+lives@gmail.com>
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
// (see weed events extension in weed-docs directory)

// mainw->playarea is reparented:
// (mt->hbox -> preview_frame -> preview_eventbox -> (playarea -> plug -> play_image)
// for gtk+ 3.x the frame_pixbuf is drawn into play_image in expose_pb when not playing

// MAP:

// top_vbox
//         - menu_hbox
//                    - menubar
//                            - menuitems
//         - xtravbox
//                    - top_eventbox
//                            - hbox
//                                 - btoolbar2
//                    - hseparator
//                    - mt_hbox
//                             - preview_frame
//                                     - preview_eventbox
//                                              - mainw->playarea
//                                                      - plug
//                                                             - play_image
////                             - hpaned
//                                    - mt->nb (notebook)
//                                    - context_box
//                    - hseparator
//                    - sepimage
//                    - hbox
//                            - amixb_eventbox
//                                            - btoolbar
//                                                    - amixer_button
//
//         - hseparator2
//         - vpaned
//                    - mt->tlx_vbox
//                    - message_box

#include "main.h"
#include "cvirtual.h"
#include "resample.h"
#include "effects-weed.h"
#include "paramwindow.h"
#include "callbacks.h"
#include "multitrack-gui.h"

static int aofile;
static int afd;

static EXPOSE_FN_PROTOTYPE(mt_expose_audtrack_event)

LIVES_GLOBAL_INLINE double get_time_from_x(lives_mt *mt, int x) {
  double time = (double)x / (double)(lives_widget_get_allocation_width(mt->timeline) - 1) *
                (mt->tl_max - mt->tl_min) + mt->tl_min;
  if (time < 0.) time = 0.;
  else if (time > mt->end_secs + 1. / mt->fps) time = mt->end_secs + 1. / mt->fps;
  return q_dbl(time, mt->fps);
}

void mt_update_timecodes(lives_mt *mt, double dtime) {
  char *timestring = mt_time_to_string(QUANT_TIME(dtime));
  lives_snprintf(mt->timestring, TIMECODE_LENGTH, "%s", timestring);
  widget_opts.justify = LIVES_JUSTIFY_CENTER;
  lives_entry_set_text(LIVES_ENTRY(mt->timecode), timestring);
  widget_opts.justify = LIVES_JUSTIFY_DEFAULT;
  lives_free(timestring);
}


static void draw_soundwave(LiVESWidget *ebox, lives_painter_surface_t *surf, int chnum, lives_mt *mt) {
  weed_plant_t *event;
  weed_timecode_t tc;

  lives_painter_t *cr = lives_painter_create_from_surface(surf);

  LiVESWidget *eventbox = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(ebox), "owner");

  track_rect *block = (track_rect *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "blocks");

  char *filename;

  double offset_startd, offset_endd; // time values
  double tl_span = mt->tl_max - mt->tl_min;
  double secs;
  double ypos;
  double seek, vel;

  int offset_start, offset_end; // pixel values
  int fnum;
  int width = lives_widget_get_allocation_width(ebox);
  int track = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), LAYER_NUMBER_KEY));

  aofile = -1;
  afd = -1;

  THREADVAR(read_failed) = 0;

  while (block) {
    event = block->start_event;
    tc = get_event_timecode(event);

    offset_startd = tc / TICKS_PER_SECOND_DBL;
    if (offset_startd > mt->tl_max) {
      if (afd != -1) lives_close_buffered(afd);
      return;
    }

    offset_start = (int)((offset_startd - mt->tl_min) / tl_span * width + .5);
    if (offset_start < 0) offset_start = 0;

    offset_endd = get_event_timecode(block->end_event) / TICKS_PER_SECOND_DBL; //+1./cfile->fps;
    if (offset_endd < mt->tl_min) {
      block = block->next;
      continue;
    }
    if (offset_endd > mt->tl_max) offset_endd = mt->tl_max;
    offset_end = (offset_endd - mt->tl_min) / tl_span * width;

    fnum = get_audio_frame_clip(block->start_event, track);
    seek = get_audio_frame_seek(block->start_event, track);
    vel = get_audio_frame_vel(block->start_event, track);

    lives_painter_set_source_rgb(cr, 1., 1., 1.);
    lives_painter_rectangle(cr, offset_start, 0, offset_end - offset_start, lives_widget_get_allocation_height(ebox) - 1);
    lives_painter_fill(cr);

    lives_painter_set_source_rgb(cr, 0., 0., 0.);
    lives_painter_set_line_width(cr, 1.);
    lives_painter_rectangle(cr, offset_start, 0, offset_end - offset_start, lives_widget_get_allocation_height(ebox) - 1);
    lives_painter_stroke(cr);

    lives_painter_set_source_rgb(cr, 0.5, 0.5, 0.5);

    // open audio file here

    if (fnum != aofile) {
      if (afd != -1) close(afd);
      filename = lives_get_audio_file_name(fnum);
      afd = lives_open_buffered_rdonly(filename);
      lives_free(filename);
      aofile = fnum;
    }

    for (int i = offset_start; i <= offset_end; i++) {
      secs = (double)i / (double)width * tl_span + mt->tl_min;
      secs -= offset_startd;
      secs = secs * vel + seek;
      if (secs >= (chnum == 0 ? mainw->files[fnum]->laudio_time : mainw->files[fnum]->raudio_time)) break;

      // seek and read
      if (afd == -1) {
        THREADVAR(read_failed) = -2;
        return;
      }
      ypos = get_float_audio_val_at_time(fnum, afd, secs, chnum, cfile->achans) * .5;

      lives_painter_move_to(cr, i, (float)lives_widget_get_allocation_height(ebox) / 2.);
      lives_painter_line_to(cr, i, (.5 - ypos) * (float)lives_widget_get_allocation_height(ebox));
      lives_painter_stroke(cr);
    }
    block = block->next;
    if (lives_read_buffered_eof(afd)) THREADVAR(read_failed) = -1;

    if (THREADVAR(read_failed) == afd + 1) {
      filename = lives_get_audio_file_name(fnum);
      do_read_failed_error_s(filename, NULL);
      lives_free(filename);
    }
  }

  lives_painter_destroy(cr);

  if (afd != -1) lives_close_buffered(afd);
}


static EXPOSE_FN_DECL(mt_expose_audtrack_event, ebox, user_data) {
  lives_mt *mt = (lives_mt *)user_data;

  lives_painter_surface_t *bgimage;

  int startx, starty, width, height;
  int hidden;
  int channum;

  if (mt->no_expose) return TRUE;

  if (event) {
    if (event->count > 0) {
      return TRUE;
    }
    startx = event->area.x;
    starty = event->area.y;
    width = event->area.width;
    height = event->area.height;
  } else {
    startx = starty = 0;
    width = lives_widget_get_allocation_width(ebox);
    height = lives_widget_get_allocation_height(ebox);
  }

  if (width == 0) return FALSE;

  hidden = (int)LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(ebox), HIDDEN_KEY));
  if (hidden != 0) {
    return FALSE;
  }

  if (width > lives_widget_get_allocation_width(ebox) - startx) width = lives_widget_get_allocation_width(ebox) - startx;

#if !GTK_CHECK_VERSION(3, 22, 0)
  if (event) cairo = lives_painter_create_from_widget(ebox);
#endif

  bgimage = (lives_painter_surface_t *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(ebox), "bgimg");

  if (GET_INT_DATA(ebox, DRAWN_KEY)) {
    if (bgimage) {
      lives_painter_set_source_surface(cairo, bgimage, startx, starty);
      lives_painter_rectangle(cairo, startx, starty, width, height);
      lives_painter_fill(cairo);
      if (event) lives_painter_destroy(cairo);
      return TRUE;
    }
  }

  if (event) {
    lives_painter_destroy(cairo);
    width = lives_widget_get_allocation_width(ebox);
    height = lives_widget_get_allocation_height(ebox);
  }

  if (bgimage) lives_painter_surface_destroy(bgimage);

  bgimage = lives_widget_create_painter_surface(ebox);

  if (palette->style & STYLE_1) {
    lives_painter_t *crx = lives_painter_create_from_surface(bgimage);
    lives_painter_set_source_rgb_from_lives_rgba(crx, &palette->mt_evbox);
    lives_painter_rectangle(crx, 0., 0., width, height);
    lives_painter_fill(crx);
    lives_painter_paint(crx);
    lives_painter_destroy(crx);
  } else clear_widget_bg(ebox, bgimage);

  channum = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(ebox), "channel"));

  if (bgimage) {
    draw_soundwave(ebox, bgimage, channum, mt);
    SET_INT_DATA(ebox, DRAWN_KEY, TRUE);
    lives_widget_queue_draw(ebox);
  } else if (bgimage) {
    lives_painter_surface_destroy(bgimage);
    bgimage = NULL;
  }

  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(ebox), "bgimg", bgimage);

  return TRUE;
}
EXPOSE_FN_END


static boolean add_to_thumb_cache(int fnum, frames_t frame, frames_t range, int height,
                                  LiVESPixbuf *pixbuf) {
  LiVESList *list = mainw->files[fnum]->tcache, *xlist, *llist = NULL;
  lives_tcache_entry_t *tce;

  for (; list; list = list->next) {
    tce = (lives_tcache_entry_t *)list->data;
    llist = list;
    if (tce->frame <= frame - range) continue;
    if (tce->frame <= frame + range) return FALSE;
    break;
  }
  tce = (lives_tcache_entry_t *)lives_calloc(1, sizeof(lives_tcache_entry_t));
  tce->frame = frame;
  tce->pixbuf = pixbuf;
  xlist = lives_list_append(NULL, tce);
  xlist->data = (livespointer)tce;
  if (list) {
    xlist->prev = list->prev;
    if (list->prev) list->prev->next = xlist;
    else  mainw->files[fnum]->tcache = xlist;
    xlist->next = list;
    list->prev = xlist;
  } else {
    if (llist) {
      llist->next = xlist;
      xlist->prev = llist;
    } else {
      mainw->files[fnum]->tcache = xlist;
      mainw->files[fnum]->tcache_height = height;
    }
  }
  return TRUE;
}

static LiVESPixbuf *get_from_thumb_cache(int fnum, frames_t frame, frames_t range) {
  LiVESList *list = mainw->files[fnum]->tcache;
  for (; list; list = list->next) {
    lives_tcache_entry_t *tcentry = (lives_tcache_entry_t *)list->data;
    if (tcentry->frame <= frame - range) continue;
    if (tcentry->frame >= frame + range) return NULL;
    return tcentry->pixbuf;
  }
  return NULL;
}

void free_thumb_cache(int fnum, frames_t fromframe) {
  LiVESList *list = mainw->files[fnum]->tcache, *freestart = NULL;
  boolean has_some = FALSE;
  for (; list; list = list->next) {
    lives_tcache_entry_t *tcentry = (lives_tcache_entry_t *)list->data;
    if (tcentry) {
      if (tcentry->frame < fromframe) {
        has_some = TRUE;
        continue;
      }
      if (has_some) {
        list->prev->next = NULL;
        list->prev = NULL;
        freestart = list;
        has_some = FALSE;
      } else freestart = mainw->files[fnum]->tcache;
      lives_widget_object_unref(tcentry->pixbuf);
    }
  }
  if (freestart) lives_list_free(freestart);
  if (freestart == mainw->files[fnum]->tcache) {
    mainw->files[fnum]->tcache = NULL;
    mainw->files[fnum]->tcache_height = 0;
  }
  mainw->files[fnum]->tcache_dubious_from = 0;
}


LiVESPixbuf *mt_make_thumb(lives_mt *mt, int file, int width, int height, frames_t frame, LiVESInterpType interp,
                           boolean noblanks) {
  LiVESPixbuf *thumbnail = NULL, *pixbuf;
  LiVESError *error = NULL;

  lives_clip_t *sfile = mainw->files[file];

  boolean tried_all = FALSE;
  boolean needs_idlefunc = FALSE;

  boolean did_backup = FALSE;

  int nframe, oframe = frame;

  if (file < 1) {
    LIVES_WARN("Warning - make thumb for file -1");
    return NULL;
  }

  if (width < 4 || height < 4) return NULL;

  if (mt) did_backup = mt->did_backup;

  if (mt && mt->idlefunc > 0) {
    needs_idlefunc = TRUE;
    lives_source_remove(mt->idlefunc);
    mt->idlefunc = 0;
  }

  do {
    if (sfile->frames > 0) {
      weed_timecode_t tc = (frame - 1.) / sfile->fps * TICKS_PER_SECOND;
      if (sfile->frames > 0 && sfile->clip_type == CLIP_TYPE_FILE) {
        lives_clip_data_t *cdata = ((lives_decoder_t *)sfile->ext_src)->cdata;
        if (cdata && !(cdata->seek_flag & LIVES_SEEK_FAST) &&
            is_virtual_frame(file, frame)) {
          virtual_to_images(file, frame, frame, FALSE, NULL);
        }
      }
      thumbnail = pull_lives_pixbuf_at_size(file, frame, get_image_ext_for_type(sfile->img_type), tc,
                                            width, height, LIVES_INTERP_FAST, TRUE);
    } else {
      pixbuf = lives_pixbuf_new_from_stock_at_size(LIVES_LIVES_STOCK_AUDIO, LIVES_ICON_SIZE_CUSTOM, width);
      if (error || !pixbuf) {
        lives_error_free(error);
        if (mt && (needs_idlefunc || (!did_backup && mt->auto_changed))) {
          mt->idlefunc = mt_idle_add(mt);
        }
        return NULL;
      }

      if (lives_pixbuf_get_width(pixbuf) != width || lives_pixbuf_get_height(pixbuf) != height) {
        // ...at_scale is inaccurate
        thumbnail = lives_pixbuf_scale_simple(pixbuf, width, height, LIVES_INTERP_FAST);
        lives_widget_object_unref(pixbuf);
      } else thumbnail = pixbuf;
    }

    if (tried_all) noblanks = FALSE;

    if (noblanks && thumbnail && !lives_pixbuf_is_all_black(thumbnail, FALSE)) noblanks = FALSE;
    if (noblanks) {
      nframe = frame + sfile->frames / 10.;
      if (nframe == frame) nframe++;
      if (nframe > sfile->frames) {
        nframe = oframe;
        tried_all = TRUE;
      }
      frame = nframe;
      if (thumbnail) lives_widget_object_unref(thumbnail);
      thumbnail = NULL;
    }
  } while (noblanks);

  if (mt) {
    if (needs_idlefunc || (!did_backup && mt->auto_changed)) {
      mt->idlefunc = mt_idle_add(mt);
    }
  }

  return thumbnail;
}


LiVESPixbuf *make_thumb_fast_between(lives_mt *mt, int fileno, int width, int height, int tframe, int range) {
  int nvframe = -1;

  if (fileno < 1) {
    LIVES_WARN("Warning - make thumb for file -1");
    return NULL;
  }

  if (width < 2 || height < 2) return NULL;

  for (int i = 1; i <= range; i++) {
    if (tframe - i > 0 && !is_virtual_frame(fileno, tframe - i)) {
      nvframe = tframe - i;
      break;
    }
    if (tframe + i <= mainw->files[fileno]->frames && !is_virtual_frame(fileno, tframe + i)) {
      nvframe = tframe + i;
      break;
    }
  }

  if (nvframe != -1) {
    return mt_make_thumb(mt, fileno, width, height, nvframe, LIVES_INTERP_FAST, FALSE);
  }

  return NULL;
}


LIVES_GLOBAL_INLINE void reset_mt_play_sizes(lives_mt *mt) {
  lives_widget_set_size_request(mt->preview_eventbox, GUI_SCREEN_WIDTH / PEB_WRATIO,
                                GUI_SCREEN_HEIGHT / PEB_HRATIO);
  lives_widget_set_size_request(mainw->play_image, GUI_SCREEN_WIDTH / PEB_WRATIO,
                                GUI_SCREEN_HEIGHT / PEB_HRATIO);
}


void mt_set_cursor_style(lives_mt *mt, lives_cursor_t cstyle, int width, int height, int clip, int hsx, int hsy) {
  LiVESXCursor *cursor;
  LiVESXDisplay *disp;

  LiVESPixbuf *pixbuf = NULL;
  LiVESPixbuf *thumbnail = NULL;

  uint8_t *cpixels, *tpixels;

  lives_clip_t *sfile = mainw->files[clip];

  double frames_width;

  unsigned int cwidth, cheight;

  int twidth = 0, twidth3, twidth4, trow;
  int frame_start;

  int i, j, k;

  disp = lives_widget_get_display(LIVES_MAIN_WINDOW_WIDGET);

#ifdef GUI_GTK
  /* screen = gdk_display_get_default_screen (display); */
  /* window = gdk_screen_get_root_window (screen); */
  /* XQueryBestCursor (GDK_DISPLAY_XDISPLAY (display), */
  /* 		    GDK_WINDOW_XID (window), */
  /* 		    width, height, &cwidth, &cheight); */
  //
  // actually, the code for this function is rubbish, it just makes a guess:
  gdk_display_get_maximal_cursor_size(disp, &cwidth, &cheight);
#endif

  if (width > cwidth) width = cwidth;

  mt->cursor_style = cstyle;
  switch (cstyle) {
  case LIVES_CURSOR_BLOCK:
    if (sfile && sfile->frames > 0) {
      frame_start = mt->opts.ign_ins_sel ? 1 : sfile->start;
      frames_width = (double)(mt->opts.ign_ins_sel ? sfile->frames : sfile->end - sfile->start + 1.);

      pixbuf = lives_pixbuf_new(TRUE, width, height);

      for (i = 0; i < width; i += BLOCK_THUMB_WIDTH) {
        // create a small thumb
        twidth = BLOCK_THUMB_WIDTH;
        if ((i + twidth) > width) twidth = width - i;
        if (twidth >= 4) {
          thumbnail = mt_make_thumb(mt, clip, twidth, height, frame_start + (int)((double)i / (double)width * frames_width),
                                    LIVES_INTERP_NORMAL, FALSE);
          // render it in the cursor
          if (thumbnail) {
            trow = lives_pixbuf_get_rowstride(thumbnail);
            twidth = lives_pixbuf_get_width(thumbnail);
            cpixels = lives_pixbuf_get_pixels(pixbuf) + (i * 4);
            tpixels = lives_pixbuf_get_pixels(thumbnail);

            if (!lives_pixbuf_get_has_alpha(thumbnail)) {
              twidth3 = twidth * 3;
              for (j = 0; j < height; j++) {
                for (k = 0; k < twidth3; k += 3) {
                  lives_memcpy(cpixels, &tpixels[k], 3);
                  lives_memset(cpixels + 3, 0xFF, 1);
                  cpixels += 4;
                }
                tpixels += trow;
                cpixels += (width - twidth) << 2;
              }
            } else {
              twidth4 = twidth * 4;
              for (j = 0; j < height; j++) {
                lives_memcpy(cpixels, tpixels, twidth4);
                tpixels += trow;
                cpixels += width << 2;
		// *INDENT-OFF*
              }}
            lives_widget_object_unref(thumbnail);
          }}}
      // *INDENT-ON*
      break;
    }
  // fallthrough
  case LIVES_CURSOR_AUDIO_BLOCK:
    pixbuf = lives_pixbuf_new(TRUE, width, height);
    trow = lives_pixbuf_get_rowstride(pixbuf);
    cpixels = lives_pixbuf_get_pixels(pixbuf);
    for (j = 0; j < height; j++) {
      for (k = 0; k < width; k++) {
        cpixels[0] = palette->audcol.red >> 8;
        cpixels[1] = palette->audcol.green >> 8;
        cpixels[2] = palette->audcol.blue >> 8;
        cpixels[3] = palette->audcol.alpha >> 8;
        cpixels += 4;
      }
      cpixels += (trow - width * 4);
    }
    break;
  case LIVES_CURSOR_VIDEO_BLOCK:
    pixbuf = lives_pixbuf_new(TRUE, width, height);
    trow = lives_pixbuf_get_rowstride(pixbuf);
    cpixels = lives_pixbuf_get_pixels(pixbuf);
    for (j = 0; j < height; j++) {
      for (k = 0; k < width; k++) {
        cpixels[0] = palette->vidcol.red >> 8;
        cpixels[1] = palette->vidcol.green >> 8;
        cpixels[2] = palette->vidcol.blue >> 8;
        cpixels[3] = palette->vidcol.alpha >> 8;
        cpixels += 4;
      }
      cpixels += (trow - width * 4);
    }
    break;
  case LIVES_CURSOR_FX_BLOCK:
    pixbuf = lives_pixbuf_new(TRUE, width, height);
    trow = lives_pixbuf_get_rowstride(pixbuf);
    cpixels = lives_pixbuf_get_pixels(pixbuf);
    for (j = 0; j < height; j++) {
      for (k = 0; k < width; k++) {
        cpixels[0] = palette->fxcol.red >> 8;
        cpixels[1] = palette->fxcol.green >> 8;
        cpixels[2] = palette->fxcol.blue >> 8;
        cpixels[3] = palette->fxcol.alpha >> 8;
        cpixels += 4;
      }
      cpixels += (trow - width * 4);
    }
    break;
  default:
    return;
  }

  cursor = lives_cursor_new_from_pixbuf(disp, pixbuf, hsx, hsy);
  lives_xwindow_set_cursor(lives_widget_get_xwindow(LIVES_MAIN_WINDOW_WIDGET), cursor);

  if (pixbuf) lives_widget_object_unref(pixbuf);
  if (cursor) lives_cursor_unref(cursor);
}


void mt_draw_block(lives_mt * mt, lives_painter_t *cairo,
                   lives_painter_surface_t *surf, track_rect * block, int x1, int x2) {
  // x1 is start point of drawing area (in pixels), x2 is width of drawing area (in pixels)
  lives_painter_t *cr;
  weed_event_t *event = block->start_event, *nxevent = NULL;
  weed_timecode_t tc = get_event_timecode(event);
  LiVESWidget *eventbox = block->eventbox;
  LiVESPixbuf *thumbnail = NULL;

  double tl_span = mt->tl_max - mt->tl_min;
  double offset_startd = (double)tc / TICKS_PER_SECOND_DBL, offset_endd;

  frames_t framenum, last_framenum = -1, range = 0;

  boolean needs_text = TRUE;
  boolean is_audio = FALSE;

  int offset_start, offset_end;
  int filenum, track;
  int width = BLOCK_THUMB_WIDTH;
  int hidden = (int)LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), HIDDEN_KEY));

  int i;

  if (mt->no_expose) return;

  if (hidden) return;

  // block to right of screen
  if (offset_startd >= mt->tl_max) return;

  // block to right of drawing area
  offset_start = (int)((offset_startd - mt->tl_min) / tl_span * lives_widget_get_allocation_width(eventbox) + .5);
  if ((x1 > 0 || x2 > 0) && offset_start > (x1 + x2)) return;

  offset_endd = get_event_timecode(block->end_event) / TICKS_PER_SECOND_DBL + (!is_audio_eventbox(eventbox))
                / mainw->files[mt->render_file]->fps;
  offset_end = (offset_endd - mt->tl_min) / tl_span * lives_widget_get_allocation_width(eventbox);

  // end of block before drawing area
  if (offset_end < x1) return;

  if (!surf) cr = cairo;
  else cr = lives_painter_create_from_surface(surf);

  if (!cr) return;

  lives_painter_set_line_width(cr, 1.);

  track = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), LAYER_NUMBER_KEY));
  is_audio = is_audio_eventbox(eventbox);
  if (track < 0) is_audio = TRUE;

  if (!is_audio) filenum = get_frame_event_clip(block->start_event, track);
  else filenum = get_audio_frame_clip(block->start_event, track);
  if (!IS_VALID_CLIP(filenum)) return;

  switch (block->state) {
  case BLOCK_UNSELECTED:
    if (BLOCK_DRAW_TYPE == BLOCK_DRAW_SIMPLE) {
      lives_painter_set_source_rgb_from_lives_rgba(cr, &palette->vidcol);
      lives_painter_new_path(cr);
      lives_painter_rectangle(cr, offset_start, 0, offset_end - offset_start, lives_widget_get_allocation_height(eventbox));

      lives_painter_move_to(cr, offset_start, 0);
      lives_painter_line_to(cr, offset_end, lives_widget_get_allocation_height(eventbox));

      lives_painter_move_to(cr, offset_end, 0);
      lives_painter_line_to(cr, offset_start, lives_widget_get_allocation_height(eventbox));

      lives_painter_stroke(cr);
    } else {
      if (!is_audio && track > -1) {
        boolean in_cache = FALSE;
        int height = lives_widget_get_allocation_height(eventbox);
        for (i = offset_start; i < offset_end; i += BLOCK_THUMB_WIDTH) {
          if (i > x2 - x1) break;
          tc += tl_span / lives_widget_get_allocation_width(eventbox) * width * TICKS_PER_SECOND_DBL;
          if (i + BLOCK_THUMB_WIDTH < x1) continue;
          if (!nxevent) event = get_frame_event_at(mt->event_list, tc, event, FALSE);
          else {
            event = nxevent;
            nxevent = NULL;
          }
          if (last_framenum == -1) {
            frames_t xframenum;
            weed_timecode_t xtc = tc + tl_span / lives_widget_get_allocation_width(eventbox) * width
                                  * TICKS_PER_SECOND_DBL;
            framenum = get_frame_event_frame(event, track);
            if ((nxevent = get_frame_event_at(mt->event_list, xtc, event, FALSE))) {
              if ((xframenum = get_frame_event_frame(nxevent, track)) > framenum)
                range = (xframenum - framenum) / 2;
            } else {
              xtc = tc - tl_span / lives_widget_get_allocation_width(eventbox) * width
                    * TICKS_PER_SECOND_DBL;
              if (xtc >= 0) {
                if ((nxevent = get_frame_event_at(mt->event_list, xtc, NULL, FALSE))) {
                  if ((xframenum = get_frame_event_frame(nxevent, track)) < framenum)
                    range = (framenum - xframenum) / 2;
                  nxevent = NULL;
		  // *INDENT-OFF*
                }}}}
	  // *INDENT-ON*

          if (i + width >= 0) {
            // create a small thumb
            framenum = get_frame_event_frame(event, track);
            if (framenum < 1 || framenum > mainw->files[filenum]->frames) continue;

            if (!in_cache && thumbnail) lives_widget_object_unref(thumbnail);
            thumbnail = NULL;
            in_cache = FALSE;

            if (IS_VALID_CLIP(filenum) && filenum != mainw->scrap_file && framenum != last_framenum) {
              if (height != mainw->files[filenum]->tcache_height && mainw->files[filenum]->tcache) {
                free_thumb_cache(filenum, 0);
              }
              if (!(thumbnail = get_from_thumb_cache(filenum, framenum, range))) {
                if (mainw->files[filenum]->frames > 0 && mainw->files[filenum]->clip_type == CLIP_TYPE_FILE) {
                  lives_clip_data_t *cdata = ((lives_decoder_t *)mainw->files[filenum]->ext_src)->cdata;
                  if (cdata && !((cdata->seek_flag & LIVES_SEEK_FAST) &&
                                 is_virtual_frame(filenum, framenum))) {
                    thumbnail = make_thumb_fast_between(mt, filenum, width, height,
                                                        framenum, last_framenum == -1 ? 0
                                                        : framenum - last_framenum);
                  } else {
                    thumbnail = mt_make_thumb(mt, filenum, width, height, framenum, LIVES_INTERP_FAST, FALSE);
                  }
                } else {
                  thumbnail = mt_make_thumb(mt, filenum, width, height, framenum, LIVES_INTERP_FAST, FALSE);
                }
                in_cache = add_to_thumb_cache(filenum, framenum, range, height, thumbnail);
              } else {
                in_cache = TRUE;
              }
            }
            last_framenum = framenum;
            // render it in the eventbox
            if (thumbnail) {
              lives_painter_set_source_pixbuf(cr, thumbnail, i, 0);
              if (i + width > offset_end) {
                width = offset_end - i;
                // crop to width
                lives_painter_new_path(cr);
                lives_painter_rectangle(cr, i, 0, width, lives_widget_get_allocation_height(eventbox));
                lives_painter_clip(cr);
              }
              lives_painter_paint(cr);
            } else {
              if (i + width > offset_end) width = offset_end - i;
              lives_painter_set_source_rgb_from_lives_rgba(cr, &palette->vidcol);
              lives_painter_new_path(cr);
              lives_painter_rectangle(cr, i, 0, width, lives_widget_get_allocation_height(eventbox));
              lives_painter_fill(cr);
            }
            if (LIVES_IS_PLAYING) {
              mt->no_expose = TRUE;
              // expose is not re-entrant due to bgimg refs
              mt_unpaint_lines(mt);
              mt->no_expose = FALSE;
            }
          }
        }
        if (!in_cache && thumbnail) lives_widget_object_unref(thumbnail);
      } else {
        lives_painter_set_source_rgb_from_lives_rgba(cr, &palette->audcol);
        lives_painter_new_path(cr);
        lives_painter_rectangle(cr, offset_start, 0, offset_end - offset_start, lives_widget_get_allocation_height(eventbox));
        lives_painter_fill(cr);
      }
      lives_painter_set_source_rgb_from_lives_widget_color(cr, &palette->black);
      lives_painter_new_path(cr);
      lives_painter_rectangle(cr, offset_start, 0, offset_end - offset_start, lives_widget_get_allocation_height(eventbox));
      lives_painter_stroke(cr);

      if (needs_text) {
        const char *sfont = "Sans";
        char *fname = get_menu_name(mainw->files[filenum], FALSE);
        lives_colRGBA64_t col_white, col_black, *colr;
        LingoLayout *layout;
        lives_painter_surface_t *surface;
        double top = 0.2;
        int height = lives_widget_get_allocation_height(eventbox);
        int text_start = offset_start + 2, text_end = offset_end;

        if (text_start < 2) text_start = 2;

        surface = lives_painter_get_target(cr);
        lives_painter_surface_flush(surface);

        col_white.red = col_white.green = col_white.blue = col_white.alpha = col_black.alpha = 65535;
        col_black.red = col_black.green = col_black.blue = 0;
        colr = &col_white;

        if (is_audio) {
          double luma = get_luma16(palette->audcol.red, palette->audcol.green, palette->audcol.blue);
          if (luma > 0.2) colr = &col_black;
        }

        layout = render_text_to_cr(NULL, cr, fname, sfont, 10., LIVES_TEXT_MODE_FOREGROUND_ONLY, colr,
                                   colr, FALSE, FALSE, &top, &text_start, text_end - text_start, &height);

        lingo_painter_show_layout(cr, layout);

        if (layout) lives_widget_object_unref(layout);

        lives_free(fname);

        lives_painter_fill(cr);
      }

      if (LIVES_IS_PLAYING) {
        mt->no_expose = TRUE; // expose is not re-entrant due to bgimg refs.
        mt_unpaint_lines(mt);
        mt->no_expose = FALSE;
      }
    }
    break;
  case BLOCK_SELECTED:
    lives_painter_new_path(cr);

    lives_painter_set_source_rgba(cr, 0., 0., 0., SELBLOCK_ALPHA);

    lives_painter_rectangle(cr, offset_start, 0, offset_end - offset_start, lives_widget_get_allocation_height(eventbox));
    lives_painter_fill(cr);

    lives_painter_new_path(cr);
    lives_painter_rectangle(cr, offset_start, 0, offset_end - offset_start, lives_widget_get_allocation_height(eventbox));

    lives_painter_move_to(cr, offset_start, 0);
    lives_painter_line_to(cr, offset_end, lives_widget_get_allocation_height(eventbox));

    lives_painter_move_to(cr, offset_end, 0);
    lives_painter_line_to(cr, offset_start, lives_widget_get_allocation_height(eventbox));

    lives_painter_stroke(cr);

    break;
  }

  if (surf) lives_painter_destroy(cr);
}


void mt_draw_aparams(lives_mt * mt, LiVESWidget * eventbox, lives_painter_t *cr, LiVESList * param_list,
                     weed_event_t *init_event, int startx, int width) {
  // draw audio parameters : currently we overlay coloured lines on the audio track to show the level of
  // parameters in the audio_volume plugin
  // we only display whichever parameters the user has elected to show

  LiVESList *plist;

  weed_plant_t **in_params, *param, *ptmpl;
  weed_plant_t *filter, *inst, *deinit_event;

  weed_timecode_t tc, start_tc, end_tc;

  double tl_span = mt->tl_max - mt->tl_min;
  double dtime;
  double ratio;
  double vald, mind, maxd, *valds;

  double y;

  int vali, mini, maxi, *valis;
  int pnum, ptype;
  int offset_start, offset_end, startpos;
  int track;

  char *fhash;

  void **pchainx = NULL;

  fhash = weed_get_string_value(init_event, WEED_LEAF_FILTER, NULL);

  if (!fhash) return;

  filter = get_weed_filter(weed_get_idx_for_hashname(fhash, TRUE));
  lives_free(fhash);

  inst = weed_instance_from_filter(filter);

  in_params = weed_get_plantptr_array(inst, WEED_LEAF_IN_PARAMETERS, NULL);

  deinit_event = weed_get_plantptr_value(init_event, WEED_LEAF_DEINIT_EVENT, NULL);

  start_tc = get_event_timecode(init_event);
  end_tc = get_event_timecode(deinit_event);

  offset_start = (int)((start_tc / TICKS_PER_SECOND_DBL - mt->tl_min) / tl_span * lives_widget_get_allocation_width(
                         eventbox) + .5);
  offset_end = (int)((end_tc / TICKS_PER_SECOND_DBL - mt->tl_min + 1. / mt->fps) / tl_span * lives_widget_get_allocation_width(
                       eventbox) + .5);

  if (offset_end < 0 || offset_start > lives_widget_get_allocation_width(eventbox)) {
    lives_free(in_params);
    weed_instance_unref(inst);
    weed_instance_unref(inst);
    return;
  }

  if (offset_start > startx) startpos = offset_start;
  else startpos = startx;

  track = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox),
                               LAYER_NUMBER_KEY)) + mt->opts.back_audio_tracks;

  lives_painter_set_line_width(cr, 1.);
  lives_painter_set_source_rgb_from_lives_widget_color(cr, &palette->black);

  pchainx = weed_get_voidptr_array(init_event, WEED_LEAF_IN_PARAMETERS, NULL);

  //lives_painter_set_operator (cr, LIVES_PAINTER_OPERATOR_DEST_OVER);
  for (int i = startpos; i < startx + width; i++) {
    dtime = get_time_from_x(mt, i);
    tc = dtime * TICKS_PER_SECOND_DBL;
    if (tc >= end_tc) break;

    if (pchainx) interpolate_params(inst, pchainx, tc);

    plist = param_list;
    while (plist) {
      pnum = LIVES_POINTER_TO_INT(plist->data);
      param = in_params[pnum];
      ptmpl = weed_param_get_template(param);
      ptype = weed_paramtmpl_get_type(ptmpl);
      switch (ptype) {
      case WEED_PARAM_INTEGER:
        valis = weed_get_int_array(param, WEED_LEAF_VALUE, NULL);
        if (is_perchannel_multiw(in_params[pnum])) vali = valis[track];
        else vali = valis[0];
        mini = weed_get_int_value(ptmpl, WEED_LEAF_MIN, NULL);
        maxi = weed_get_int_value(ptmpl, WEED_LEAF_MAX, NULL);
        ratio = (double)(vali - mini) / (double)(maxi - mini);
        lives_free(valis);
        break;
      case WEED_PARAM_FLOAT:
        valds = weed_get_double_array(param, WEED_LEAF_VALUE, NULL);
        if (is_perchannel_multiw(in_params[pnum])) vald = valds[track];
        else vald = valds[0];
        mind = weed_get_double_value(ptmpl, WEED_LEAF_MIN, NULL);
        maxd = weed_get_double_value(ptmpl, WEED_LEAF_MAX, NULL);
        ratio = (vald - mind) / (maxd - mind);
        lives_free(valds);
        break;
      default:
        continue;
      }

      y = (1. - ratio) * (double)lives_widget_get_allocation_height(eventbox);

      lives_painter_move_to(cr, i - 1, y - 1);
      lives_painter_line_to(cr, i, y);

      plist = plist->next;
    }
  }

  weed_instance_unref(inst);
  weed_instance_unref(inst);

  lives_painter_stroke(cr);
  lives_painter_surface_flush(lives_painter_get_target(cr));

  lives_free(pchainx);
  lives_free(in_params);
}


void mt_redraw_eventbox(lives_mt * mt, LiVESWidget * eventbox) {
  if (!LIVES_IS_WIDGET_OBJECT(eventbox)) return;

  SET_INT_DATA(eventbox, DRAWN_KEY, FALSE);
  lives_widget_queue_draw(eventbox);  // redraw the track

  if (is_audio_eventbox(eventbox)) {
    // handle expanded audio
    LiVESWidget *xeventbox;
    if (mainw->files[mt->render_file]->achans > 0) {
      xeventbox = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "achan0");
      SET_INT_DATA(xeventbox, DRAWN_KEY, FALSE);
      lives_widget_queue_draw(xeventbox);  // redraw the track
      if (mainw->files[mt->render_file]->achans > 1) {
        xeventbox = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "achan1");
        SET_INT_DATA(xeventbox, DRAWN_KEY, FALSE);
        lives_widget_queue_draw(xeventbox);  // redraw the track
      }
    }
  }
}


static EXPOSE_FN_DECL(expose_track_event, eventbox, user_data) {
  lives_painter_t *cr;

  lives_mt *mt = (lives_mt *)user_data;

  track_rect *block;
  track_rect *sblock = NULL;

  ulong idlefunc;

  lives_painter_surface_t *bgimage;

  int startx, starty, width, height;
  int hidden;

  if (mt->no_expose) return TRUE;

  if (LIVES_IS_PLAYING && mainw->fs && mainw->play_window) return TRUE;

  if (event) {
    if (event->count > 0) {
      return TRUE;
    }
    startx = event->area.x;
    starty = event->area.y;
    width = event->area.width;
    height = event->area.height;
  } else {
    startx = starty = 0;
    width = lives_widget_get_allocation_width(eventbox);
    height = lives_widget_get_allocation_height(eventbox);
  }

  if (width == 0) return FALSE;

  hidden = (int)LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), HIDDEN_KEY));
  if (hidden != 0) {
    LiVESWidget *label = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "label");
    lives_widget_hide(eventbox);
    lives_widget_hide(lives_widget_get_parent(label));
    return FALSE;
  }

  idlefunc = mt->idlefunc;
  if (mt->idlefunc > 0) {
    mt->idlefunc = 0;
    lives_source_remove(idlefunc);
  }

  if (width > lives_widget_get_allocation_width(eventbox) - startx) width = lives_widget_get_allocation_width(eventbox) - startx;

  bgimage = (lives_painter_surface_t *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "bgimg");

  lives_set_cursor_style(LIVES_CURSOR_BUSY, NULL);
  lives_widget_context_update();

draw1:
#if !GTK_CHECK_VERSION(3, 22, 0)
  if (!cairo) cr = lives_painter_create_from_surface(bgimage);
  else cr = cairo;
#else
  cr = cairo;
#endif

  if (GET_INT_DATA(eventbox, DRAWN_KEY)) {
    if (bgimage) {
      lives_painter_set_source_surface(cr, bgimage, startx, starty);
      lives_painter_rectangle(cr, startx, starty, width, height);
      lives_painter_fill(cr);

      if (mt->block_selected && mt->block_selected->eventbox == eventbox) {
        mt_draw_block(mt, cr, NULL, mt->block_selected, -1, -1);
      }

      if (is_audio_eventbox(eventbox) && mt->avol_init_event && mt->opts.aparam_view_list)
        mt_draw_aparams(mt, eventbox, cr, mt->opts.aparam_view_list, mt->avol_init_event, startx, width);

      if (idlefunc > 0) {
        mt->idlefunc = mt_idle_add(mt);
      }
      if (!cairo) lives_painter_destroy(cr);
      lives_set_cursor_style(LIVES_CURSOR_NORMAL, NULL);

      return TRUE;
    }
  }

  width = lives_widget_get_allocation_width(eventbox);
  height = lives_widget_get_allocation_height(eventbox);
  if (!cairo) lives_painter_destroy(cr);

  if (bgimage) lives_painter_surface_destroy(bgimage);

  bgimage = lives_widget_create_painter_surface(eventbox);

  if (bgimage) {
    if (palette->style & STYLE_1) {
      lives_painter_t *crx = lives_painter_create_from_surface(bgimage);
      lives_painter_set_source_rgb_from_lives_rgba(crx, &palette->mt_evbox);
      lives_painter_rectangle(crx, 0., 0., width, height);
      lives_painter_fill(crx);
      lives_painter_paint(crx);
      lives_painter_destroy(crx);
    } else clear_widget_bg(eventbox, bgimage);

    if (mt->block_selected) {
      sblock = mt->block_selected;
      sblock->state = BLOCK_UNSELECTED;
    }

    block = (track_rect *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "blocks");

    while (block) {
      mt_draw_block(mt, NULL, bgimage, block, startx, width);
      block = block->next;
    }

    if (sblock) {
      mt->block_selected = sblock;
      sblock->state = BLOCK_SELECTED;
    }

  } else if (bgimage) {
    lives_painter_surface_destroy(bgimage);
    bgimage = NULL;
  }

  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(eventbox), "bgimg", (livespointer)bgimage);
  SET_INT_DATA(eventbox, DRAWN_KEY, bgimage != NULL);

  if (bgimage) goto draw1;

  if (idlefunc > 0) {
    mt->idlefunc = mt_idle_add(mt);
  }

  lives_set_cursor_style(LIVES_CURSOR_NORMAL, NULL);
  return TRUE;
}
EXPOSE_FN_END


static char *mt_params_label(lives_mt * mt) {
  char *fname = weed_filter_idx_get_name(mt->current_fx, FALSE, FALSE);
  char *layer_name;
  char *ltext, *tmp;

  if (has_perchannel_multiw(get_weed_filter(mt->current_fx))) {
    layer_name = get_track_name(mt, mt->current_track, mt->aud_track_selected);
    tmp = lives_strdup_printf(_("%s : parameters for %s"), fname, layer_name);
    ltext = lives_markup_escape_text(tmp, -1);
    lives_free(layer_name); lives_free(tmp);
  } else ltext = lives_strdup(fname);
  lives_free(fname);

  if (mt->framedraw) {
    char *someparms = lives_big_and_bold(_("<--- Some parameters can be altered by clicking / dragging in the Preview window"));
    char *tmp2 = lives_markup_escape_text(ltext, -1);
    tmp = lives_strdup_printf("%s\n%s",  tmp2, someparms);
    lives_free(ltext); lives_free(tmp2); lives_free(someparms);
    ltext = tmp;
  }

  return ltext;
}


LIVES_GLOBAL_INLINE double mt_get_effect_time(lives_mt * mt) {
  return QUANT_TIME(lives_spin_button_get_value(LIVES_SPIN_BUTTON(mt->node_spinbutton)));
}


boolean add_mt_param_box(lives_mt * mt) {
  // here we add a GUI box which will hold effect parameters

  // if we set keep_scale to TRUE, the current time slider is kept
  // this is necessary in case we need to update the parameters without resetting the current timeline value

  // returns TRUE if we have any parameters

  weed_plant_t *deinit_event;

  weed_timecode_t tc;

  double fx_start_time, fx_end_time;
  double cur_time = mt->ptr_time;

  char *ltext;

  boolean res = FALSE;
  int dph = widget_opts.packing_height;
  int dbw = widget_opts.border_width;

  tc = get_event_timecode((weed_plant_t *)mt->init_event);
  deinit_event = weed_get_plantptr_value(mt->init_event, WEED_LEAF_DEINIT_EVENT, NULL);

  fx_start_time = tc / TICKS_PER_SECOND_DBL;
  fx_end_time = get_event_timecode(deinit_event) / TICKS_PER_SECOND_DBL;

  if (mt->fx_box) {
    lives_widget_destroy(mt->fx_box);
  }

  mt->fx_box = lives_vbox_new(FALSE, 0);

  if (mt->fx_params_label) {
    lives_widget_destroy(mt->fx_params_label);
  }

  mt->fx_params_label = lives_label_new("");
  lives_widget_apply_theme2(mt->fx_params_label, LIVES_WIDGET_STATE_NORMAL, TRUE);
  lives_box_pack_start(LIVES_BOX(mt->fx_base_box), mt->fx_params_label, FALSE, TRUE, widget_opts.packing_height);

  lives_box_pack_end(LIVES_BOX(mt->fx_base_box), mt->fx_box, TRUE, TRUE, 0);

  lives_signal_handlers_block_by_func(mt->node_spinbutton, (livespointer)on_node_spin_value_changed, (livespointer)mt);
  lives_spin_button_configure(LIVES_SPIN_BUTTON(mt->node_spinbutton), cur_time - fx_start_time, 0.,
                              fx_end_time - fx_start_time, 1. / mt->fps, 10. / mt->fps);

  lives_signal_handlers_unblock_by_func(mt->node_spinbutton, (livespointer)on_node_spin_value_changed, (livespointer)mt);

  widget_opts.packing_height = 2. * widget_opts.scale;
  widget_opts.border_width = 2. * widget_opts.scale;
  res = make_param_box(LIVES_VBOX(mt->fx_box), mt->current_rfx);
  widget_opts.packing_height = dph;
  widget_opts.border_width = dbw;

  ltext = mt_params_label(mt);

  widget_opts.mnemonic_label = FALSE;
  widget_opts.justify = LIVES_JUSTIFY_CENTER;
  widget_opts.use_markup = TRUE;
  lives_label_set_text(LIVES_LABEL(mt->fx_params_label), ltext);
  widget_opts.use_markup = FALSE;
  widget_opts.justify = LIVES_JUSTIFY_DEFAULT;
  widget_opts.mnemonic_label = TRUE;

  lives_free(ltext);

  lives_widget_show_all(mt->fx_base_box);

  if (!res) {
    lives_widget_hide(mt->apply_fx_button);
    lives_widget_hide(mt->resetp_button);
    lives_widget_hide(mt->del_node_button);
    lives_widget_hide(mt->prev_node_button);
    lives_widget_hide(mt->next_node_button);
  }

  mt->prev_fx_time = mt_get_effect_time(mt);

  if (!mt->sel_locked) {
    lives_spin_button_set_value(LIVES_SPIN_BUTTON(mt->spinbutton_start), fx_start_time);
    lives_spin_button_set_value(LIVES_SPIN_BUTTON(mt->spinbutton_end), fx_end_time);
  }
  return res;
}


void on_seltrack_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  LiVESWidget *eventbox;
  LiVESWidget *checkbutton;

  boolean mi_state;

  lives_mt_poly_state_t statep;

  if (mt->current_track == -1) return;

  eventbox = (LiVESWidget *)lives_list_nth_data(mt->video_draws, mt->current_track);
  checkbutton = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "checkbutton");

  mi_state = lives_check_menu_item_get_active(LIVES_CHECK_MENU_ITEM(menuitem));

  if (mi_state) {
    // selected
    if (lives_list_index(mt->selected_tracks, LIVES_INT_TO_POINTER(mt->current_track)) == -1)
      mt->selected_tracks = lives_list_append(mt->selected_tracks, LIVES_INT_TO_POINTER(mt->current_track));
  } else {
    // unselected
    if (lives_list_index(mt->selected_tracks, LIVES_INT_TO_POINTER(mt->current_track)) != -1)
      mt->selected_tracks = lives_list_remove(mt->selected_tracks, LIVES_INT_TO_POINTER(mt->current_track));
  }

#ifdef ENABLE_GIW
  if (!prefs->lamp_buttons) {
#endif
    if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(checkbutton)) != mi_state) {
      lives_signal_handlers_block_by_func(checkbutton, (livespointer)on_seltrack_toggled, (livespointer)mt);
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(checkbutton), mi_state);
      lives_signal_handlers_unblock_by_func(checkbutton, (livespointer)on_seltrack_toggled, (livespointer)mt);
    }
#ifdef ENABLE_GIW
  } else {
    if (giw_led_get_mode(GIW_LED(checkbutton)) != mi_state) {
      lives_signal_handlers_block_by_func(checkbutton, (livespointer)on_seltrack_toggled, (livespointer)mt);
      giw_led_set_mode(GIW_LED(checkbutton), mi_state);
      lives_signal_handlers_unblock_by_func(checkbutton, (livespointer)on_seltrack_toggled, (livespointer)mt);
    }
  }
#endif
  do_sel_context(mt);

  lives_widget_set_sensitive(mt->ins_gap_sel, FALSE);
  lives_widget_set_sensitive(mt->remove_gaps, FALSE);
  lives_widget_set_sensitive(mt->remove_first_gaps, FALSE);
  lives_widget_set_sensitive(mt->split_sel, FALSE);
  lives_widget_set_sensitive(mt->fx_region, FALSE);

  if (mt->selected_tracks) {
    if (mt->event_list && get_first_event(mt->event_list)) {
      lives_widget_set_sensitive(mt->split_sel, mt_selblock(NULL, (livespointer)mt) != NULL);
    }

    if (mt->region_start != mt->region_end) {
      if (mt->event_list && get_first_event(mt->event_list)) {
        lives_widget_set_sensitive(mt->ins_gap_sel, TRUE);
        lives_widget_set_sensitive(mt->remove_gaps, TRUE);
        lives_widget_set_sensitive(mt->remove_first_gaps, TRUE);
      }
      lives_widget_set_sensitive(mt->fx_region, TRUE);
      switch (lives_list_length(mt->selected_tracks)) {
      case 1:
        if (cfile->achans == 0) lives_widget_set_sensitive(mt->fx_region_a, FALSE);
        lives_widget_set_sensitive(mt->fx_region_2a, FALSE);
        lives_widget_set_sensitive(mt->fx_region_2v, FALSE);
        lives_widget_set_sensitive(mt->fx_region_2av, FALSE);
        break;
      case 2:
        lives_widget_set_sensitive(mt->fx_region_a, FALSE);
        lives_widget_set_sensitive(mt->fx_region_v, FALSE);
        if (!mt->opts.pertrack_audio)
          lives_widget_set_sensitive(mt->fx_region_2a, FALSE);
        break;
      default:
        break;
      }
    }
  }

  // update labels
  statep = get_poly_state_from_page(mt);
  if (statep == POLY_TRANS || statep == POLY_COMP) {
    polymorph(mt, POLY_NONE);
    polymorph(mt, statep);
  }
}


void track_select(lives_mt * mt) {
  LiVESWidget *labelbox, *label, *hbox, *dummy, *ahbox, *arrow;
  LiVESWidget *eventbox, *oeventbox, *checkbutton = NULL;
  LiVESList *list;
  weed_timecode_t tc;
  int i, hidden = 0;

  if (!prefs->show_gui) return;

  if (mt->current_track < 0) {
    // back aud sel
    lives_widget_set_sensitive(mt->select_track, FALSE);
    lives_widget_set_sensitive(mt->rename_track, FALSE);
    lives_widget_set_sensitive(mt->insert, FALSE);

    lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mt->select_track), FALSE);

    lives_widget_set_sensitive(mt->cback_audio, FALSE);
    lives_widget_set_sensitive(mt->audio_insert, mt->file_selected > 0 &&
                               mainw->files[mt->file_selected]->achans > 0 &&
                               mainw->files[mt->file_selected]->laudio_time > 0.);
  } else {
    // vid sel
    lives_widget_set_sensitive(mt->select_track, TRUE);
    lives_widget_set_sensitive(mt->rename_track, TRUE);
    lives_widget_set_sensitive(mt->cback_audio, TRUE);

    lives_widget_set_sensitive(mt->insert, mt->file_selected > 0 && mainw->files[mt->file_selected]->frames > 0);
    lives_widget_set_sensitive(mt->adjust_start_end, mt->file_selected > 0);
    lives_widget_set_sensitive(mt->audio_insert, FALSE);
  }

  if (palette->style & STYLE_1) {
    if (CURRENT_CLIP_HAS_AUDIO) {
      i = 0;
      for (list = mt->audio_draws; list; list = list->next, i++) {
        eventbox = (LiVESWidget *)list->data;
        if ((oeventbox = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "owner")))
          hidden = !LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(oeventbox), "expanded"));
        if (hidden == 0) hidden = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), HIDDEN_KEY));
        if (hidden == 0) {
          labelbox = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "labelbox");
          label = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "label");
          dummy = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "dummy");
          ahbox = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "ahbox");
          hbox = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "hbox");
          arrow = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "arrow");
          if (mt->current_track == i - mt->opts.back_audio_tracks && (mt->current_track < 0 || mt->aud_track_selected)) {
            // audio track is selected
            if (labelbox) {
              lives_widget_set_bg_color(labelbox, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
              lives_widget_set_fg_color(labelbox, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars_fore);
            }
            if (ahbox) {
              lives_widget_set_bg_color(ahbox, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
              lives_widget_set_fg_color(ahbox, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars_fore);
            }
            lives_widget_set_bg_color(label, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
            lives_widget_set_fg_color(label, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars_fore);
            lives_widget_set_bg_color(dummy, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
            lives_widget_set_fg_color(dummy, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars_fore);
            lives_widget_set_bg_color(arrow, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
            lives_widget_set_fg_color(arrow, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars_fore);

            lives_widget_set_sensitive(mt->jumpback,
                                       lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "blocks") != NULL);
            lives_widget_set_sensitive(mt->jumpnext,
                                       lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "blocks") != NULL);
          } else {
            if (labelbox) {
              lives_widget_set_bg_color(labelbox, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
              lives_widget_set_fg_color(labelbox, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
            }
            if (ahbox) {
              lives_widget_set_bg_color(ahbox, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
              lives_widget_set_fg_color(ahbox, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
            }
            lives_widget_set_fg_color(label, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
            lives_widget_set_bg_color(label, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
            lives_widget_set_fg_color(dummy, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
            lives_widget_set_bg_color(dummy, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
            lives_widget_set_fg_color(arrow, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
            lives_widget_set_bg_color(arrow, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
	    // *INDENT-OFF*
          }}}}}
  // *INDENT-ON*
  i = 0;
  for (list = mt->video_draws; list; list = list->next, i++) {
    eventbox = (LiVESWidget *)list->data;
    hidden = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), HIDDEN_KEY));
    if (hidden == 0) {
      labelbox = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "labelbox");
      label = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "label");
      hbox = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "hbox");
      ahbox = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "ahbox");
      arrow = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "arrow");
      checkbutton = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "checkbutton");
      if (i == mt->current_track) {
        if (palette->style & STYLE_1) {
          if (!mt->aud_track_selected) {
            if (labelbox) {
              lives_widget_set_bg_color(labelbox, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
              lives_widget_set_fg_color(labelbox, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars_fore);
            }
            if (ahbox) {
              lives_widget_set_bg_color(ahbox, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
              lives_widget_set_fg_color(ahbox, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars_fore);
            }
            lives_widget_set_bg_color(label, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
            lives_widget_set_bg_color(arrow, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
            lives_widget_set_bg_color(checkbutton, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
            lives_widget_set_bg_color(hbox, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
            lives_widget_set_fg_color(label, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars_fore);
            lives_widget_set_fg_color(arrow, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars_fore);

            lives_widget_set_sensitive(mt->jumpback,
                                       lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "blocks") != NULL);
            lives_widget_set_sensitive(mt->jumpnext,
                                       lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "blocks") != NULL);
          } else {
            if (labelbox) {
              lives_widget_set_bg_color(labelbox, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
              lives_widget_set_fg_color(labelbox, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
            }
            if (ahbox) {
              lives_widget_set_bg_color(ahbox, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
              lives_widget_set_fg_color(ahbox, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
            }
            lives_widget_set_fg_color(label, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
            lives_widget_set_fg_color(arrow, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
            lives_widget_set_bg_color(label, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
            lives_widget_set_bg_color(arrow, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
            lives_widget_set_bg_color(hbox, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
            lives_widget_set_bg_color(checkbutton, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
          }
        }

#ifdef ENABLE_GIW
        if ((prefs->lamp_buttons
             && !giw_led_get_mode(GIW_LED(checkbutton)))
            || (!prefs->lamp_buttons &&
#else
        if (
#endif
                !lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(checkbutton)))
#ifdef ENABLE_GIW
           )
#endif
#if 0
        }
#endif
      {
        // set other widgets
        if (lives_check_menu_item_get_active(LIVES_CHECK_MENU_ITEM(mt->select_track))) {
          lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mt->select_track), FALSE);
        } else on_seltrack_activate(LIVES_MENU_ITEM(mt->select_track), mt);
      } else {
        if (!lives_check_menu_item_get_active(LIVES_CHECK_MENU_ITEM(mt->select_track)))
          lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mt->select_track), TRUE);
        else on_seltrack_activate(LIVES_MENU_ITEM(mt->select_track), mt);
      }
    } else {
      if (palette->style & STYLE_1) {
        if (labelbox) {
          lives_widget_set_bg_color(labelbox, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
          lives_widget_set_fg_color(labelbox, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
        }
        if (ahbox) {
          lives_widget_set_bg_color(ahbox, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
          lives_widget_set_fg_color(ahbox, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
        }
        lives_widget_set_fg_color(label, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
        lives_widget_set_fg_color(arrow, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
        lives_widget_set_bg_color(label, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
        lives_widget_set_bg_color(arrow, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
        lives_widget_set_bg_color(checkbutton, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
        lives_widget_set_bg_color(hbox, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
	  // *INDENT-OFF*
      }}}}
  // *INDENT-ON*

if (mt->poly_state == POLY_FX_STACK) polymorph(mt, POLY_FX_STACK);
else if (mt->current_rfx && mt->init_event && mt->poly_state == POLY_PARAMS &&
         weed_plant_has_leaf(mt->init_event, WEED_LEAF_IN_TRACKS)) {
  boolean xx;
  boolean interp = TRUE;
  weed_timecode_t init_tc = get_event_timecode(mt->init_event);
  tc = q_gint64(lives_spin_button_get_value(LIVES_SPIN_BUTTON(mt->node_spinbutton)) * TICKS_PER_SECOND_DBL + init_tc, mt->fps);

  // must be done in this order: interpolate, update, preview
  xx = get_track_index(mt, tc);
  if (mt->track_index != -1) {
    for (i = 0; i < mt->current_rfx->num_params; i++) {
      // if we are just switching tracks within the same effect, without changing the time,
      // and we have unapplied changes, we don't want to interpolate
      // otherwise we will lose those changes
      if (mt->current_rfx->params[i].flags & PARAM_FLAG_VALUE_SET) {
        interp = FALSE;
        break;
      }
    }
    if (mt->current_track >= 0) {
      // interpolate values ONLY if there are no unapplied changes (e.g. the time was altered)
      if (interp) interpolate_params((weed_plant_t *)mt->current_rfx->source, mt_get_pchain(), tc);
    }
    if (!xx) {
      // the param box was redrawn
      boolean aprev = mt->opts.fx_auto_preview;
      //mt->opts.fx_auto_preview = FALSE;
      mainw->block_param_updates = TRUE;
      mt->current_rfx->needs_reinit = FALSE;
      mt->current_rfx->flags |= RFX_FLAGS_NO_RESET;
      update_visual_params(mt->current_rfx, FALSE);
      mainw->block_param_updates = FALSE;
      if (mt->current_rfx->needs_reinit) {
        weed_reinit_effect(mt->current_rfx->source, TRUE);
        mt->current_rfx->needs_reinit = FALSE;
      }
      mt->current_rfx->flags &= ~RFX_FLAGS_NO_RESET;
      mt->opts.fx_auto_preview = aprev;
      activate_mt_preview(mt);
    } else mt_show_current_frame(mt, FALSE);
  } else polymorph(mt, POLY_FX_STACK);
}
}


static void show_track_info(lives_mt * mt, LiVESWidget * eventbox, int track, double timesecs) {
  char *tmp, *tmp1;
  track_rect *block = get_block_from_time(eventbox, timesecs, mt);
  int filenum;

  clear_context(mt);
  if (!is_audio_eventbox(eventbox)) add_context_label
    (mt, (tmp = lives_markup_printf_escaped
                (_("Current track: %s (layer %d)\n"),
                 lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox),
                     "track_name"), track)));
  else {
    if (track == -1) add_context_label(mt, (tmp = (_("Current track: Backing audio\n"))));
    else add_context_label(mt, (tmp =
                                    lives_markup_printf_escaped(_("Current track: Layer %d audio\n"), track)));
  }
  lives_free(tmp);
  add_context_label(mt, (tmp = lives_strdup_printf(_("%.2f sec.\n"), timesecs)));
  lives_free(tmp);
  if (block) {
    if (!is_audio_eventbox(eventbox)) filenum = get_frame_event_clip(block->start_event, track);
    else filenum = get_audio_frame_clip(block->start_event, track);
    add_context_label(mt, (tmp = lives_markup_printf_escaped(_("Source: %s"),
                                 (tmp1 = get_menu_name(mainw->files[filenum], FALSE)))));
    lives_free(tmp);
    lives_free(tmp1);
    add_context_label(mt, (_("Right click for context menu.\n")));
  }
  add_context_label(mt, (_("Double click on a block\nto select it.")));
}


static boolean atrack_ebox_pressed(LiVESWidget * labelbox, LiVESXEventButton * event, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  int current_track = mt->current_track;
  if (!LIVES_IS_INTERACTIVE) return FALSE;
  mt->current_track = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(labelbox), LAYER_NUMBER_KEY));
  if (current_track != mt->current_track) mt->fm_edit_event = NULL;
  mt->aud_track_selected = TRUE;
  track_select(mt);
  show_track_info(mt, (LiVESWidget *)lives_list_nth_data(mt->audio_draws, mt->current_track + mt->opts.back_audio_tracks),
                  mt->current_track, mt->ptr_time);
  return FALSE;
}


static boolean track_ebox_pressed(LiVESWidget * labelbox, LiVESXEventButton * event, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  int current_track = mt->current_track;
  if (!LIVES_IS_INTERACTIVE) return FALSE;
  mt->current_track = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(labelbox), LAYER_NUMBER_KEY));
  if (current_track != mt->current_track) mt->fm_edit_event = NULL;
  mt->aud_track_selected = FALSE;
  track_select(mt);
  show_track_info(mt, (LiVESWidget *)lives_list_nth_data(mt->video_draws, mt->current_track), mt->current_track, mt->ptr_time);
  return FALSE;
}


boolean on_mt_timeline_scroll(LiVESWidget * widget, LiVESXEventScroll * event, livespointer user_data) {
  // scroll timeline up/down with mouse wheel
  lives_mt *mt = (lives_mt *)user_data;

  int cval;

  //if (!lives_window_has_toplevel_focus(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET))) return FALSE;

  LiVESXModifierType kstate = (LiVESXModifierType)event->state;
  if ((kstate & LIVES_DEFAULT_MOD_MASK) == LIVES_CONTROL_MASK) return on_mouse_scroll(widget, event, user_data);

  cval = lives_adjustment_get_value(lives_range_get_adjustment(LIVES_RANGE(mt->scrollbar)));

  if (lives_get_scroll_direction(event) == LIVES_SCROLL_UP) {
    if (--cval < 0) return FALSE;
  } else if (lives_get_scroll_direction(event) == LIVES_SCROLL_DOWN) {
    if (++cval >= mt->num_video_tracks) return FALSE;
  }

  lives_range_set_value(LIVES_RANGE(mt->scrollbar), cval);

  return FALSE;
}


int get_top_track_for(lives_mt * mt, int track) {
  // find top track such that all of track fits at the bottom

  LiVESWidget *eventbox;
  LiVESList *vdraw;
  int extras = prefs->max_disp_vtracks - 1;
  int hidden, expanded;

  if (mt->opts.back_audio_tracks > 0 && !mt->audio_draws) mt->opts.back_audio_tracks = 0;
  if (mainw->files[mt->render_file]->achans > 0 && mt->opts.back_audio_tracks > 0) {
    eventbox = (LiVESWidget *)mt->audio_draws->data;
    hidden = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), HIDDEN_KEY));
    if (!hidden) {
      extras--;
      expanded = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "expanded"));
      if (expanded) {
        extras -= mainw->files[mt->render_file]->achans;
      }
    }
  }

  if (extras < 0) return track;

  vdraw = lives_list_nth(mt->video_draws, track);
  eventbox = (LiVESWidget *)vdraw->data;
  expanded = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "expanded"));
  if (expanded) {
    eventbox = (LiVESWidget *)(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "atrack"));
    extras--;
    expanded = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "expanded"));
    if (expanded) {
      extras -= mainw->files[mt->render_file]->achans;
    }
  }

  if (extras < 0) return track;

  vdraw = vdraw->prev;

  while (vdraw) {
    eventbox = (LiVESWidget *)vdraw->data;
    hidden = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), HIDDEN_KEY))&TRACK_I_HIDDEN_USER;
    expanded = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "expanded"));
    extras--;
    if (expanded) {
      eventbox = (LiVESWidget *)(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "atrack"));
      extras--;
      expanded = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "expanded"));
      if (expanded) {
        extras -= mainw->files[mt->render_file]->achans;
      }
    }
    if (extras < 0) break;
    vdraw = vdraw->prev;
    track--;
  }

  if (track < 0) track = 0;
  return track;
}


void mt_redraw_all_event_boxes(lives_mt * mt) {
  LiVESList *slist;

  slist = mt->audio_draws;
  while (slist) {
    mt_redraw_eventbox(mt, (LiVESWidget *)slist->data);
    slist = slist->next;
  }

  slist = mt->video_draws;
  while (slist) {
    mt_redraw_eventbox(mt, (LiVESWidget *)slist->data);
    slist = slist->next;
  }
  mt_paint_lines(mt, mt->ptr_time, TRUE, NULL);
}


static boolean expose_paintlines(LiVESWidget * widget, lives_painter_t *cr, livespointer data) {
  int offset = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(widget),
                                    "has_line"));

  if (lives_painter_set_operator(cr, LIVES_PAINTER_OPERATOR_DIFFERENCE))
    lives_painter_set_source_rgb(cr, 1., 1., 1.);
  else
    lives_painter_set_source_rgb(cr, 0., 0., 0.);

  lives_painter_set_line_width(cr, 1.);
  lives_painter_rectangle(cr, offset, 0., 1., lives_widget_get_allocation_height(widget));
  lives_painter_fill(cr);

  return FALSE;
}


void scroll_tracks(lives_mt * mt, int top_track, boolean set_value) {
  LiVESList *vdraws = mt->video_draws;
  LiVESList *table_children, *xlist;

  LiVESWidget *eventbox;
  LiVESWidget *label;
  LiVESWidget *dummy;
  LiVESWidget *arrow;
  LiVESWidget *checkbutton;
  LiVESWidget *labelbox;
  LiVESWidget *hbox;
  LiVESWidget *ahbox;
  LiVESWidget *xeventbox, *aeventbox;

  LiVESWidgetColor col;

  boolean expanded;

  int rows = 0;
  int aud_tracks = 0;
  int hidden;

  // only for gtk+ 2 (I think)
  lives_rgba_to_widget_color(&col, &palette->mt_evbox);

  lives_adjustment_set_page_size(LIVES_ADJUSTMENT(mt->vadjustment), (double)prefs->max_disp_vtracks);
  lives_adjustment_set_upper(LIVES_ADJUSTMENT(mt->vadjustment), (double)(mt->num_video_tracks * 2 - 1));

  if (set_value)
    lives_adjustment_set_value(LIVES_ADJUSTMENT(mt->vadjustment), (double)top_track);

  if (top_track < 0) top_track = 0;
  if (top_track >= mt->num_video_tracks) top_track = mt->num_video_tracks - 1;

  mt->top_track = top_track;

  // first set all tracks to hidden
  while (vdraws) {
    eventbox = (LiVESWidget *)vdraws->data;
    hidden = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), HIDDEN_KEY));
    hidden |= TRACK_I_HIDDEN_SCROLLED;
    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(eventbox), HIDDEN_KEY, LIVES_INT_TO_POINTER(hidden));

    aeventbox = LIVES_WIDGET(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "atrack"));

    if (aeventbox) {
      hidden = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(aeventbox), HIDDEN_KEY));
      hidden |= TRACK_I_HIDDEN_SCROLLED;
      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(aeventbox), HIDDEN_KEY, LIVES_INT_TO_POINTER(hidden));

      xeventbox = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(aeventbox), "achan0");

      if (xeventbox) {
        hidden = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(xeventbox), HIDDEN_KEY));
        hidden |= TRACK_I_HIDDEN_SCROLLED;
        lives_widget_object_set_data(LIVES_WIDGET_OBJECT(xeventbox), HIDDEN_KEY, LIVES_INT_TO_POINTER(hidden));
      }

      xeventbox = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(aeventbox), "achan1");

      if (xeventbox) {
        hidden = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(xeventbox), HIDDEN_KEY));
        hidden |= TRACK_I_HIDDEN_SCROLLED;
        lives_widget_object_set_data(LIVES_WIDGET_OBJECT(xeventbox), HIDDEN_KEY, LIVES_INT_TO_POINTER(hidden));
      }
    }

    vdraws = vdraws->next;
  }

  if (mt->timeline_table) {
    lives_widget_destroy(mt->timeline_table);
  }

  mt->timeline_table = lives_table_new(prefs->max_disp_vtracks, TIMELINE_TABLE_COLUMNS, TRUE);
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(mt->timeline_table), "has_line", LIVES_INT_TO_POINTER(-1));

  lives_container_add(LIVES_CONTAINER(mt->tl_eventbox), mt->timeline_table);

  lives_signal_sync_connect_after(LIVES_GUI_OBJECT(mt->timeline_table), LIVES_WIDGET_EXPOSE_EVENT,
                                  LIVES_GUI_CALLBACK(expose_paintlines),
                                  (livespointer)mt);

  lives_table_set_row_spacings(LIVES_TABLE(mt->timeline_table), widget_opts.packing_height * widget_opts.scale);
  lives_table_set_col_spacings(LIVES_TABLE(mt->timeline_table), 0);

  lives_widget_set_vexpand(mt->timeline_table, FALSE);

  if (mt->opts.back_audio_tracks > 0 && !mt->audio_draws) mt->opts.back_audio_tracks = 0;

  if (mainw->files[mt->render_file]->achans > 0 && mt->opts.back_audio_tracks > 0) {
    // show our float audio
    if (LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(mt->audio_draws->data), HIDDEN_KEY)) == 0) {
      aud_tracks++;

      expanded = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(mt->audio_draws->data), "expanded"));

      label = (LIVES_WIDGET(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(mt->audio_draws->data), "label")));
      dummy = (LIVES_WIDGET(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(mt->audio_draws->data), "dummy")));
      arrow = (LIVES_WIDGET(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(mt->audio_draws->data), "arrow")));

      labelbox = lives_event_box_new();
      hbox = lives_hbox_new(FALSE, widget_opts.packing_width);
      ahbox = lives_event_box_new();

      lives_container_add(LIVES_CONTAINER(labelbox), hbox);
      lives_box_pack_start(LIVES_BOX(hbox), label, TRUE, TRUE, 0);
      lives_container_add(LIVES_CONTAINER(ahbox), arrow);

      lives_table_attach(LIVES_TABLE(mt->timeline_table), dummy, 0, 2, 0, 1, LIVES_FILL, (LiVESAttachOptions)0, 0, 0);
      lives_table_attach(LIVES_TABLE(mt->timeline_table), labelbox, 2, 6, 0, 1, LIVES_FILL, (LiVESAttachOptions)0, 0, 0);
      lives_table_attach(LIVES_TABLE(mt->timeline_table), ahbox, 6, 7, 0, 1, LIVES_FILL, (LiVESAttachOptions)0, 0, 0);

      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(mt->audio_draws->data), "labelbox", labelbox);
      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(mt->audio_draws->data), "ahbox", ahbox);
      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(ahbox), "eventbox", (livespointer)mt->audio_draws->data);
      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(labelbox), LAYER_NUMBER_KEY, LIVES_INT_TO_POINTER(-1));

      lives_signal_connect(LIVES_GUI_OBJECT(labelbox), LIVES_WIDGET_BUTTON_PRESS_EVENT,
                           LIVES_GUI_CALLBACK(atrack_ebox_pressed), (livespointer)mt);

      lives_signal_connect(LIVES_GUI_OBJECT(ahbox), LIVES_WIDGET_BUTTON_PRESS_EVENT,
                           LIVES_GUI_CALLBACK(track_arrow_pressed), (livespointer)mt);

      lives_table_attach(LIVES_TABLE(mt->timeline_table), (LiVESWidget *)mt->audio_draws->data, 7, TIMELINE_TABLE_COLUMNS, 0, 1,
                         (LiVESAttachOptions)(LIVES_EXPAND | LIVES_FILL), (LiVESAttachOptions)(0), 0, 0);

      lives_widget_set_valign((LiVESWidget *)mt->audio_draws->data, LIVES_ALIGN_CENTER);

      lives_signal_connect(LIVES_GUI_OBJECT(mt->audio_draws->data), LIVES_WIDGET_BUTTON_PRESS_EVENT,
                           LIVES_GUI_CALLBACK(on_track_click), (livespointer)mt);
      lives_signal_connect(LIVES_GUI_OBJECT(mt->audio_draws->data), LIVES_WIDGET_BUTTON_RELEASE_EVENT,
                           LIVES_GUI_CALLBACK(on_track_release), (livespointer)mt);

      lives_widget_set_bg_color(LIVES_WIDGET(mt->audio_draws->data), LIVES_WIDGET_STATE_NORMAL, &col);
      lives_widget_set_app_paintable(LIVES_WIDGET(mt->audio_draws->data), TRUE);
      lives_signal_sync_connect(LIVES_GUI_OBJECT(mt->audio_draws->data), LIVES_WIDGET_EXPOSE_EVENT,
                                LIVES_GUI_CALLBACK(expose_track_event), (livespointer)mt);

      if (expanded) {
        xeventbox = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(mt->audio_draws->data), "achan0");

        label = (LIVES_WIDGET(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(xeventbox), "label")));
        labelbox = lives_event_box_new();
        hbox = lives_hbox_new(FALSE, widget_opts.packing_width);
        lives_container_add(LIVES_CONTAINER(labelbox), hbox);
        lives_box_pack_end(LIVES_BOX(hbox), label, TRUE, TRUE, 0);

        lives_table_attach(LIVES_TABLE(mt->timeline_table), labelbox, 2, 6, 1, 2, LIVES_FILL, (LiVESAttachOptions)0, 0, 0);
        lives_table_attach(LIVES_TABLE(mt->timeline_table), xeventbox, 7, TIMELINE_TABLE_COLUMNS, 1, 2,
                           (LiVESAttachOptions)(LIVES_EXPAND | LIVES_FILL), (LiVESAttachOptions)(0), 0, 0);

        lives_widget_set_valign(xeventbox, LIVES_ALIGN_CENTER);

        lives_widget_set_bg_color(xeventbox, LIVES_WIDGET_STATE_NORMAL, &col);
        lives_widget_set_app_paintable(xeventbox, TRUE);
        lives_signal_connect(LIVES_GUI_OBJECT(xeventbox), LIVES_WIDGET_EXPOSE_EVENT,
                             LIVES_GUI_CALLBACK(mt_expose_audtrack_event),
                             (livespointer)mt);

        if (mainw->files[mt->render_file]->achans > 1) {
          xeventbox = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(mt->audio_draws->data), "achan1");

          label = (LIVES_WIDGET(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(xeventbox), "label")));
          labelbox = lives_event_box_new();
          hbox = lives_hbox_new(FALSE, widget_opts.packing_width);
          lives_container_add(LIVES_CONTAINER(labelbox), hbox);
          lives_box_pack_end(LIVES_BOX(hbox), label, TRUE, TRUE, 0);

          lives_table_attach(LIVES_TABLE(mt->timeline_table), labelbox, 2, 6, 2, 3, LIVES_FILL, (LiVESAttachOptions)0, 0, 0);
          lives_table_attach(LIVES_TABLE(mt->timeline_table), xeventbox, 7, TIMELINE_TABLE_COLUMNS, 2, 3,
                             (LiVESAttachOptions)(LIVES_EXPAND | LIVES_FILL), (LiVESAttachOptions)(0), 0, 0);

          lives_widget_set_valign(xeventbox, LIVES_ALIGN_CENTER);

          lives_widget_set_bg_color(xeventbox, LIVES_WIDGET_STATE_NORMAL, &col);
          lives_widget_set_app_paintable(xeventbox, TRUE);
          lives_signal_connect(LIVES_GUI_OBJECT(xeventbox), LIVES_WIDGET_EXPOSE_EVENT,
                               LIVES_GUI_CALLBACK(mt_expose_audtrack_event),
                               (livespointer)mt);
	  // *INDENT-OFF*
        }
        aud_tracks += mainw->files[mt->render_file]->achans;
      }}}
  // *INDENT-ON*

  lives_adjustment_set_page_size(LIVES_ADJUSTMENT(mt->vadjustment),
                                 (double)((int)(lives_adjustment_get_page_size(LIVES_ADJUSTMENT(mt->vadjustment))) - aud_tracks));

  vdraws = lives_list_nth(mt->video_draws, top_track);

  rows += aud_tracks;

  while (vdraws && rows < prefs->max_disp_vtracks) {
    eventbox = (LiVESWidget *)vdraws->data;

    hidden = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), HIDDEN_KEY))&TRACK_I_HIDDEN_USER;
    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(eventbox), HIDDEN_KEY, LIVES_INT_TO_POINTER(hidden));

    if (hidden == 0) {
      label = (LIVES_WIDGET(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "label")));
      arrow = (LIVES_WIDGET(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "arrow")));
      checkbutton = (LIVES_WIDGET(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "checkbutton")));
      labelbox = lives_event_box_new();
      hbox = lives_hbox_new(FALSE, widget_opts.packing_width);
      ahbox = lives_event_box_new();

      lives_widget_set_bg_color(LIVES_WIDGET(eventbox), LIVES_WIDGET_STATE_NORMAL, &col);

#ifdef ENABLE_GIW
      if (prefs->lamp_buttons) {
#if GTK_CHECK_VERSION(3, 0, 0)
        giw_led_set_rgba(GIW_LED(checkbutton), palette->light_green, palette->dark_red);
#else
        giw_led_set_colors(GIW_LED(checkbutton), palette->light_green, palette->dark_red);
#endif
      }
#endif

      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(labelbox), LAYER_NUMBER_KEY,
                                   LIVES_INT_TO_POINTER(LIVES_POINTER_TO_INT
                                       (lives_widget_object_get_data
                                        (LIVES_WIDGET_OBJECT(eventbox), LAYER_NUMBER_KEY))));

      lives_container_add(LIVES_CONTAINER(labelbox), hbox);
      lives_box_pack_start(LIVES_BOX(hbox), checkbutton, FALSE, FALSE, widget_opts.border_width);
      lives_box_pack_start(LIVES_BOX(hbox), label, TRUE, TRUE, 0);
      lives_container_add(LIVES_CONTAINER(ahbox), arrow);

      lives_table_attach(LIVES_TABLE(mt->timeline_table), labelbox, 0, 6,
                         rows, rows + 1, LIVES_FILL, (LiVESAttachOptions)0, 0, 0);
      lives_table_attach(LIVES_TABLE(mt->timeline_table), ahbox, 6, 7, rows, rows + 1, LIVES_FILL, (LiVESAttachOptions)0, 0, 0);

      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(eventbox), "labelbox", labelbox);
      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(eventbox), "hbox", hbox);
      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(eventbox), "ahbox", ahbox);
      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(ahbox), "eventbox", eventbox);

      lives_table_attach(LIVES_TABLE(mt->timeline_table), eventbox, 7, TIMELINE_TABLE_COLUMNS, rows, rows + 1,
                         (LiVESAttachOptions)(LIVES_EXPAND | LIVES_FILL), (LiVESAttachOptions)(0), 0, 0);

      lives_widget_set_valign(eventbox, LIVES_ALIGN_CENTER);

      if (!prefs->lamp_buttons) {
        lives_signal_sync_connect_after(LIVES_GUI_OBJECT(checkbutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                                        LIVES_GUI_CALLBACK(on_seltrack_toggled), mt);
      } else {
        lives_signal_sync_connect_after(LIVES_GUI_OBJECT(checkbutton), LIVES_WIDGET_MODE_CHANGED_SIGNAL,
                                        LIVES_GUI_CALLBACK(on_seltrack_toggled), mt);
      }

      lives_signal_connect(LIVES_GUI_OBJECT(labelbox), LIVES_WIDGET_BUTTON_PRESS_EVENT,
                           LIVES_GUI_CALLBACK(track_ebox_pressed), (livespointer)mt);

      lives_widget_set_bg_color(eventbox, LIVES_WIDGET_STATE_NORMAL, &col);
      lives_widget_set_app_paintable(eventbox, TRUE);
      lives_signal_sync_connect(LIVES_GUI_OBJECT(eventbox), LIVES_WIDGET_EXPOSE_EVENT,
                                LIVES_GUI_CALLBACK(expose_track_event), (livespointer)mt);

      lives_signal_connect(LIVES_GUI_OBJECT(eventbox), LIVES_WIDGET_BUTTON_PRESS_EVENT,
                           LIVES_GUI_CALLBACK(on_track_click), (livespointer)mt);
      lives_signal_connect(LIVES_GUI_OBJECT(eventbox), LIVES_WIDGET_BUTTON_RELEASE_EVENT,
                           LIVES_GUI_CALLBACK(on_track_release), (livespointer)mt);

      lives_signal_connect(LIVES_GUI_OBJECT(ahbox), LIVES_WIDGET_BUTTON_PRESS_EVENT,
                           LIVES_GUI_CALLBACK(track_arrow_pressed), (livespointer)mt);
      rows++;

      if (rows == prefs->max_disp_vtracks) break;

      if (mt->opts.pertrack_audio && lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "expanded")) {

        aeventbox = LIVES_WIDGET(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "atrack"));

        hidden = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(aeventbox),
                                      HIDDEN_KEY))&TRACK_I_HIDDEN_USER;
        lives_widget_object_set_data(LIVES_WIDGET_OBJECT(aeventbox), HIDDEN_KEY, LIVES_INT_TO_POINTER(hidden));

        if (hidden == 0) {
          lives_adjustment_set_page_size(LIVES_ADJUSTMENT(mt->vadjustment),
                                         (double)((int)lives_adjustment_get_page_size(LIVES_ADJUSTMENT(mt->vadjustment)) - 1));
          expanded = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(aeventbox), "expanded"));

          label = (LIVES_WIDGET(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(aeventbox), "label")));
          dummy = (LIVES_WIDGET(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(aeventbox), "dummy")));
          arrow = (LIVES_WIDGET(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(aeventbox), "arrow")));

          labelbox = lives_event_box_new();
          hbox = lives_hbox_new(FALSE, widget_opts.packing_width);
          ahbox = lives_event_box_new();

          lives_widget_set_bg_color(LIVES_WIDGET(aeventbox), LIVES_WIDGET_STATE_NORMAL, &col);

          lives_container_add(LIVES_CONTAINER(labelbox), hbox);
          lives_box_pack_start(LIVES_BOX(hbox), label, TRUE, TRUE, 0);
          lives_container_add(LIVES_CONTAINER(ahbox), arrow);

          // for gtk+2.x have 0,2...5,7 ?
          lives_table_attach(LIVES_TABLE(mt->timeline_table), dummy, 0, 2, rows, rows + 1,
                             LIVES_FILL, (LiVESAttachOptions)0, 0, 0);
          lives_table_attach(LIVES_TABLE(mt->timeline_table), labelbox, 2, 6, rows, rows + 1,
                             LIVES_FILL, (LiVESAttachOptions)0, 0, 0);
          lives_table_attach(LIVES_TABLE(mt->timeline_table), ahbox, 6, 7, rows, rows + 1,
                             LIVES_FILL, (LiVESAttachOptions)0, 0, 0);

          lives_widget_object_set_data(LIVES_WIDGET_OBJECT(aeventbox), "labelbox", labelbox);
          lives_widget_object_set_data(LIVES_WIDGET_OBJECT(aeventbox), "hbox", hbox);
          lives_widget_object_set_data(LIVES_WIDGET_OBJECT(aeventbox), "ahbox", ahbox);
          lives_widget_object_set_data(LIVES_WIDGET_OBJECT(ahbox), "eventbox", aeventbox);
          lives_widget_object_set_data(LIVES_WIDGET_OBJECT(labelbox), LAYER_NUMBER_KEY,
                                       LIVES_INT_TO_POINTER(LIVES_POINTER_TO_INT
                                           (lives_widget_object_get_data
                                            (LIVES_WIDGET_OBJECT(eventbox), LAYER_NUMBER_KEY))));

          lives_signal_connect(LIVES_GUI_OBJECT(labelbox), LIVES_WIDGET_BUTTON_PRESS_EVENT,
                               LIVES_GUI_CALLBACK(atrack_ebox_pressed), (livespointer)mt);

          lives_signal_connect(LIVES_GUI_OBJECT(ahbox), LIVES_WIDGET_BUTTON_PRESS_EVENT,
                               LIVES_GUI_CALLBACK(track_arrow_pressed), (livespointer)mt);

          lives_table_attach(LIVES_TABLE(mt->timeline_table), aeventbox, 7, TIMELINE_TABLE_COLUMNS, rows, rows + 1,
                             (LiVESAttachOptions)(LIVES_EXPAND | LIVES_FILL), (LiVESAttachOptions)(0), 0, 0);

          lives_widget_set_valign(aeventbox, LIVES_ALIGN_CENTER);

          lives_signal_connect(LIVES_GUI_OBJECT(aeventbox), LIVES_WIDGET_BUTTON_PRESS_EVENT,
                               LIVES_GUI_CALLBACK(on_track_click), (livespointer)mt);
          lives_signal_connect(LIVES_GUI_OBJECT(aeventbox), LIVES_WIDGET_BUTTON_RELEASE_EVENT,
                               LIVES_GUI_CALLBACK(on_track_release), (livespointer)mt);

          lives_widget_set_app_paintable(aeventbox, TRUE);
          lives_signal_sync_connect(LIVES_GUI_OBJECT(aeventbox), LIVES_WIDGET_EXPOSE_EVENT,
                                    LIVES_GUI_CALLBACK(expose_track_event), (livespointer)mt);
          rows++;

          if (rows == prefs->max_disp_vtracks) break;

          if (expanded) {
            xeventbox = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(aeventbox), "achan0");
            hidden = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(xeventbox), HIDDEN_KEY))
                     & TRACK_I_HIDDEN_USER;
            lives_widget_object_set_data(LIVES_WIDGET_OBJECT(xeventbox), HIDDEN_KEY, LIVES_INT_TO_POINTER(hidden));

            if (hidden == 0) {
              label = (LIVES_WIDGET(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(xeventbox), "label")));
              labelbox = lives_event_box_new();
              hbox = lives_hbox_new(FALSE, widget_opts.packing_width);
              lives_widget_apply_theme(label, LIVES_WIDGET_STATE_NORMAL);
              lives_widget_apply_theme(labelbox, LIVES_WIDGET_STATE_NORMAL);
              lives_widget_apply_theme(hbox, LIVES_WIDGET_STATE_NORMAL);
              lives_container_add(LIVES_CONTAINER(labelbox), hbox);
              lives_box_pack_end(LIVES_BOX(hbox), label, TRUE, TRUE, 0);

              lives_adjustment_set_page_size(LIVES_ADJUSTMENT(mt->vadjustment),
                                             (double)((int)lives_adjustment_get_page_size(LIVES_ADJUSTMENT(mt->vadjustment))
                                                      - 1));

              lives_table_attach(LIVES_TABLE(mt->timeline_table),
                                 labelbox, 2, 6, rows, rows + 1, LIVES_FILL, (LiVESAttachOptions)0, 0, 0);
              lives_table_attach(LIVES_TABLE(mt->timeline_table), xeventbox, 7, TIMELINE_TABLE_COLUMNS, rows, rows + 1,
                                 (LiVESAttachOptions)(LIVES_EXPAND | LIVES_FILL), (LiVESAttachOptions)(LIVES_FILL), 0, 0);

              lives_widget_object_set_data(LIVES_WIDGET_OBJECT(xeventbox), "labelbox", labelbox);
              lives_widget_object_set_data(LIVES_WIDGET_OBJECT(xeventbox), "hbox", hbox);

              lives_widget_set_bg_color(xeventbox, LIVES_WIDGET_STATE_NORMAL, &col);
              lives_widget_set_app_paintable(xeventbox, TRUE);
              lives_signal_connect(LIVES_GUI_OBJECT(xeventbox), LIVES_WIDGET_EXPOSE_EVENT,
                                   LIVES_GUI_CALLBACK(mt_expose_audtrack_event), (livespointer)mt);

              rows++;
              if (rows == prefs->max_disp_vtracks) break;
            }

            if (mainw->files[mt->render_file]->achans > 1) {
              xeventbox = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(aeventbox), "achan1");
              hidden = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(xeventbox), HIDDEN_KEY))
                       & TRACK_I_HIDDEN_USER;
              lives_widget_object_set_data(LIVES_WIDGET_OBJECT(xeventbox), HIDDEN_KEY, LIVES_INT_TO_POINTER(hidden));

              if (hidden == 0) {
                label = (LIVES_WIDGET(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(xeventbox), "label")));
                labelbox = lives_event_box_new();
                hbox = lives_hbox_new(FALSE, widget_opts.packing_width);
                lives_widget_apply_theme(label, LIVES_WIDGET_STATE_NORMAL);
                lives_widget_apply_theme(labelbox, LIVES_WIDGET_STATE_NORMAL);
                lives_widget_apply_theme(hbox, LIVES_WIDGET_STATE_NORMAL);
                lives_container_add(LIVES_CONTAINER(labelbox), hbox);
                lives_box_pack_end(LIVES_BOX(hbox), label, TRUE, TRUE, 0);

                lives_adjustment_set_page_size(LIVES_ADJUSTMENT(mt->vadjustment),
                                               (double)((int)lives_adjustment_get_page_size(LIVES_ADJUSTMENT(mt->vadjustment))
                                                        - 1));

                lives_table_attach(LIVES_TABLE(mt->timeline_table), labelbox, 2, 6, rows, rows + 1,
                                   LIVES_FILL, (LiVESAttachOptions)0, 0, 0);
                lives_table_attach(LIVES_TABLE(mt->timeline_table), xeventbox, 7, TIMELINE_TABLE_COLUMNS, rows, rows + 1,
                                   (LiVESAttachOptions)(LIVES_EXPAND | LIVES_FILL), (LiVESAttachOptions)(0), 0, 0);

                lives_widget_set_valign(xeventbox, LIVES_ALIGN_CENTER);

                lives_widget_object_set_data(LIVES_WIDGET_OBJECT(xeventbox), "labelbox", labelbox);
                lives_widget_object_set_data(LIVES_WIDGET_OBJECT(xeventbox), "hbox", hbox);

                lives_widget_set_bg_color(xeventbox, LIVES_WIDGET_STATE_NORMAL, &col);
                lives_widget_set_app_paintable(xeventbox, TRUE);
                lives_signal_connect(LIVES_GUI_OBJECT(xeventbox), LIVES_WIDGET_EXPOSE_EVENT,
                                     LIVES_GUI_CALLBACK(mt_expose_audtrack_event), (livespointer)mt);

                rows++;
                if (rows == prefs->max_disp_vtracks) break;
		// *INDENT-OFF*
              }}}}}}
    // *INDENT-ON*

    vdraws = vdraws->next;
  }

  if (lives_adjustment_get_page_size(LIVES_ADJUSTMENT(mt->vadjustment)) < 1.)
    lives_adjustment_set_page_size(LIVES_ADJUSTMENT(mt->vadjustment), 1.);

  lives_adjustment_set_upper(LIVES_ADJUSTMENT(mt->vadjustment),
                             (double)(get_top_track_for(mt, mt->num_video_tracks - 1) +
                                      (int)lives_adjustment_get_page_size(LIVES_ADJUSTMENT(mt->vadjustment))));

  if (lives_adjustment_get_value(LIVES_ADJUSTMENT(mt->vadjustment)) + lives_adjustment_get_page_size(LIVES_ADJUSTMENT(
        mt->vadjustment)) > lives_adjustment_get_upper(LIVES_ADJUSTMENT(mt->vadjustment)))
    lives_adjustment_set_upper(LIVES_ADJUSTMENT(mt->vadjustment), lives_adjustment_get_value(LIVES_ADJUSTMENT(mt->vadjustment)) +
                               lives_adjustment_get_page_size(LIVES_ADJUSTMENT(mt->vadjustment)));

  xlist = table_children = lives_container_get_children(LIVES_CONTAINER(mt->timeline_table));

  while (table_children) {
    LiVESWidget *child = (LiVESWidget *)table_children->data;
    lives_widget_set_size_request(child, -1, MT_TRACK_HEIGHT);
    table_children = table_children->next;
  }

  if (xlist) lives_list_free(xlist);

  mt_paint_lines(mt, mt->ptr_time, TRUE, NULL);

  lives_widget_show_all(mt->timeline_table);

  if (mt->is_ready) {
    mt->no_expose = FALSE;
    //lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);
  }
}


boolean track_arrow_pressed(LiVESWidget * ebox, LiVESXEventButton * event, livespointer user_data) {
  LiVESWidget *eventbox = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(ebox), "eventbox");
  LiVESWidget *arrow = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "arrow"), *new_arrow;
  lives_mt *mt = (lives_mt *)user_data;
  boolean expanded = !(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "expanded"));

  if (!LIVES_IS_INTERACTIVE) return FALSE;

  if (!mt->audio_draws || (!mt->opts.pertrack_audio && (mt->opts.back_audio_tracks == 0 ||
                           eventbox != mt->audio_draws->data))) {
    track_ebox_pressed(eventbox, NULL, mt);
    return FALSE;
  }

  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(eventbox), "expanded", LIVES_INT_TO_POINTER(expanded));

  if (!expanded) {
    new_arrow = lives_arrow_new(LIVES_ARROW_RIGHT, LIVES_SHADOW_OUT);
  } else {
    new_arrow = lives_arrow_new(LIVES_ARROW_DOWN, LIVES_SHADOW_OUT);
  }

  lives_widget_object_ref(new_arrow);

  lives_widget_object_set_data_widget_object(LIVES_WIDGET_OBJECT(eventbox), "arrow", new_arrow);

  lives_tooltips_copy(new_arrow, arrow);

  // must do this after we update object data, to avoid a race condition
  lives_widget_destroy(arrow);

  scroll_tracks(mt, mt->top_track, FALSE);
  track_select(mt);
  return FALSE;
}


void mt_clip_select(lives_mt * mt, boolean scroll) {
  LiVESList *list = lives_container_get_children(LIVES_CONTAINER(mt->clip_inner_box));
  LiVESWidget *clipbox = NULL;
  boolean was_neg = FALSE;
  int len;

  mt->file_selected = -1;

  if (!list) return;

  if (mt->poly_state == POLY_FX_STACK && mt->event_list) {
    if (!mt->was_undo_redo) {
      polymorph(mt, POLY_FX_STACK);
    }
  } else polymorph(mt, POLY_CLIPS);
  if (mt->clip_selected < 0) {
    was_neg = TRUE;
    mt->clip_selected = -mt->clip_selected;
  }

  if (mt->clip_selected >= (len = lives_list_length(list)) && !was_neg) mt->clip_selected = 0;

  if (was_neg) mt->clip_selected--;

  if (mt->clip_selected < 0 || (was_neg && mt->clip_selected == 0)) mt->clip_selected = len - 1;

  mt->file_selected = mt_file_from_clip(mt, mt->clip_selected);

  if (!IS_VALID_CLIP(mt->file_selected)) {
    mt->file_selected = -1;
    lives_list_free(list);
    return;
  }

  if (scroll) {
    LiVESAdjustment *adj = lives_scrolled_window_get_hadjustment(LIVES_SCROLLED_WINDOW(mt->clip_scroll));
    if (adj) {
      double value = lives_adjustment_get_upper(adj) * (mt->clip_selected + .5) / (double)len;
      lives_adjustment_clamp_page(adj, value - lives_adjustment_get_page_size(adj) / 2.,
                                  value + lives_adjustment_get_page_size(adj) / 2.);
    }
  }

  for (int i = 0; i < len; i++) {
    clipbox = (LiVESWidget *)lives_list_nth_data(list, i);
    if (i == mt->clip_selected) {
      if (palette->style & STYLE_1) {
        lives_widget_set_bg_color(clipbox, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
        lives_widget_set_fg_color(clipbox, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars_fore);
        set_child_alt_colour(clipbox, TRUE);
      }

      lives_widget_set_sensitive(mt->adjust_start_end, mainw->files[mt->file_selected]->frames > 0);
      if (mt->current_track > -1) {
        lives_widget_set_sensitive(mt->insert, mainw->files[mt->file_selected]->frames > 0);
        lives_widget_set_sensitive(mt->audio_insert, FALSE);
      } else {
        lives_widget_set_sensitive(mt->audio_insert, mainw->files[mt->file_selected]->achans > 0);
        lives_widget_set_sensitive(mt->insert, FALSE);
      }
    } else {
      if (palette->style & STYLE_1) {
        lives_widget_set_bg_color(clipbox, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
        lives_widget_set_fg_color(clipbox, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
        set_child_colour(clipbox, TRUE);
      }
    }
  }
  lives_list_free(list);
}


static void set_time_scrollbar(lives_mt * mt) {
  double page = mt->tl_max - mt->tl_min;
  if (mt->end_secs == 0.) mt->end_secs = DEF_TIME;

  if (mt->tl_max > mt->end_secs) mt->end_secs = mt->tl_max;

  lives_widget_object_freeze_notify(LIVES_WIDGET_OBJECT(mt->hadjustment));
  lives_range_set_range(LIVES_RANGE(mt->time_scrollbar), 0., mt->end_secs);
  lives_range_set_increments(LIVES_RANGE(mt->time_scrollbar), page / 4., page);
  lives_adjustment_set_page_size(LIVES_ADJUSTMENT(mt->hadjustment), page);
  lives_adjustment_set_value(LIVES_ADJUSTMENT(mt->hadjustment), mt->tl_min);
  lives_widget_object_thaw_notify(LIVES_WIDGET_OBJECT(mt->hadjustment));
  lives_widget_queue_draw(mt->time_scrollbar);
}


void set_timeline_end_secs(lives_mt * mt, double secs) {
  double pos = mt->ptr_time;

  mt->end_secs = secs;

#ifdef ENABLE_GIW
  giw_timeline_set_max_size(GIW_TIMELINE(mt->timeline), mt->end_secs);
  lives_ruler_set_upper(LIVES_RULER(mt->timeline), mt->tl_max);
  lives_ruler_set_lower(LIVES_RULER(mt->timeline), mt->tl_min);
#endif

  lives_ruler_set_range(LIVES_RULER(mt->timeline), mt->tl_min, mt->tl_max, mt->tl_min, mt->end_secs + 1. / mt->fps);
  lives_widget_queue_draw(mt->timeline);
  lives_widget_queue_draw(mt->timeline_table);
  if (!mt->sel_locked || mt->region_end < mt->end_secs + 1. / mt->fps) {
    lives_spin_button_set_range(LIVES_SPIN_BUTTON(mt->spinbutton_start), 0., mt->end_secs);
    lives_spin_button_set_range(LIVES_SPIN_BUTTON(mt->spinbutton_end), 0., mt->end_secs + 1. / mt->fps);
  }
  set_time_scrollbar(mt);

  lives_ruler_set_value(LIVES_RULER(mt->timeline), pos);

  mt_redraw_all_event_boxes(mt);
}


weed_layer_t *mt_show_current_frame(lives_mt * mt, boolean return_layer) {
  // show preview of current frame in preview_eventbox and/or play_
  // or, if return_layer is TRUE, we just set mainw->frame_layer
  // (used when we want to save the frame, e.g right click context)

  /// NOTE: this will show the current frame WITHOUT any currently unapplied effects
  // to show WITH unapplied effects, call activate_mt_preview() instead

  weed_timecode_t curr_tc;

  double ptr_time = mt->ptr_time;

  weed_plant_t *frame_layer = mainw->frame_layer;

  int current_file;
  int actual_frame;

  boolean is_rendering = mainw->is_rendering;
  boolean internal_messaging = mainw->internal_messaging;
  boolean needs_idlefunc = FALSE;
  boolean did_backup = mt->did_backup;
  static boolean lastblank = TRUE;

  //if (mt->play_width == 0 || mt->play_height == 0) return;
  if (mt->no_frame_update) return NULL;

  if (mainw->frame_layer) {
    weed_layer_ref(mainw->frame_layer);
    weed_layer_ref(mainw->frame_layer);
    mainw->frame_layer = NULL;
  }

  set_mt_play_sizes_cfg(mt);

  if (mt->idlefunc > 0) {
    lives_source_remove(mt->idlefunc);
    mt->idlefunc = 0;
    needs_idlefunc = TRUE;
  }

  if (!return_layer) {
    // show frame image in window
    if (!mt->mt_frame_preview) {
      boolean sep_win = mainw->sep_win;
      mt->mt_frame_preview = TRUE;

      if (mainw->plug) {
        lives_container_remove(LIVES_CONTAINER(mainw->plug), mainw->play_image);
        lives_widget_destroy(mainw->plug);
        mainw->plug = NULL;
      }

      if (LIVES_IS_WIDGET(mainw->playarea)) lives_widget_destroy(mainw->playarea);

      mainw->playarea = lives_hbox_new(FALSE, 0);
      lives_container_add(LIVES_CONTAINER(mt->preview_eventbox), mainw->playarea);
      lives_widget_set_bg_color(mainw->playarea, LIVES_WIDGET_STATE_NORMAL, &palette->black);
      mainw->sep_win = FALSE;
      add_to_playframe();
      lives_widget_show_all(mainw->playarea);
      set_mt_play_sizes_cfg(mt);
      mainw->sep_win = sep_win;
    }
  }

  if (LIVES_IS_PLAYING) {
    if (mainw->play_window && LIVES_IS_XWINDOW(lives_widget_get_xwindow(mainw->play_window))) {
#if GTK_CHECK_VERSION(3, 0, 0)
      if (!mt->frame_pixbuf || mt->frame_pixbuf != mainw->imframe) {
        if (mt->frame_pixbuf) lives_widget_object_unref(mt->frame_pixbuf);
        // set frame_pixbuf, this gets painted in in expose_event
        mt->frame_pixbuf = mainw->imframe;
        set_drawing_area_from_pixbuf(mainw->play_image, mt->frame_pixbuf, mainw->play_surface);
      }
#else
      set_drawing_area_from_pixbuf(mainw->play_image, mainw->imframe, mainw->play_surface);
#endif
    } else {
#if GTK_CHECK_VERSION(3, 0, 0)
      if (mt->frame_pixbuf != mainw->imframe) {
        if (mt->frame_pixbuf) lives_widget_object_unref(mt->frame_pixbuf);
        mt->frame_pixbuf = NULL;
        set_drawing_area_from_pixbuf(mainw->play_image, mt->frame_pixbuf, mainw->play_surface);
      }
#else
      set_drawing_area_from_pixbuf(mainw->play_image, NULL, mainw->play_surface);
#endif
    }
    lives_widget_queue_draw(mt->preview_eventbox);
    if (needs_idlefunc || (!did_backup && mt->auto_changed)) {
      mt->idlefunc = mt_idle_add(mt);
    }
    weed_layer_free(mainw->frame_layer);
    if (frame_layer) {
      mainw->frame_layer = frame_layer;
      weed_layer_unref(mainw->frame_layer);
      weed_layer_unref(mainw->frame_layer);
    } else mainw->frame_layer = NULL;
    return NULL;
  }

  // start "playback" at mt->ptr_time; we just "render" one frame
  curr_tc = mt_set_play_position(mt);
  actual_frame = (int)((double)curr_tc / TICKS_PER_SECOND_DBL * mainw->files[mt->render_file]->fps + 1.4999);
  mainw->frame_layer = NULL;

  if (mt->is_rendering && actual_frame <= mainw->files[mt->render_file]->frames) {
    // get the actual frame if it has already been rendered
    mainw->frame_layer = lives_layer_new_for_frame(mainw->current_file, actual_frame);
    pull_frame(mainw->frame_layer, get_image_ext_for_type(mainw->files[mt->render_file]->img_type), curr_tc);
  } else {
    weed_plant_t *live_inst = NULL;
    mainw->is_rendering = TRUE;

    if (mt->event_list) {
      if (mt->pb_start_event) {
        cfile->next_event = mt->pb_start_event;
      } else {
        cfile->next_event = get_first_event(mt->event_list);
      }
      // "play" a single frame
      current_file = mainw->current_file;
      mainw->internal_messaging = TRUE; // stop load_frame from showing image
      mainw->files[mt->render_file]->next_event = mt->pb_start_event;
      if (is_rendering) {
        backup_weed_instances();
        backup_host_tags(mt->event_list, curr_tc);
      }

      // pass quickly through events_list, switching on and off effects and interpolating at current time
      get_audio_and_effects_state_at(mt->event_list, mt->pb_start_event, 0, LIVES_PREVIEW_TYPE_VIDEO_ONLY, mt->exact_preview,
                                     NULL);

      // if we are previewing a specific effect we also need to init it
      if (mt->current_rfx && mt->init_event) {
        if (mt->current_rfx->source_type == LIVES_RFX_SOURCE_WEED && mt->current_rfx->source) {
          live_inst = (weed_plant_t *)mt->current_rfx->source;

          // we want to get hold of the instance which the renderer will use, and then set the in_parameters
          // with the currently unapplied values from the live instance

          // the renderer's instance will be freed in deinit_render_effects(),
          // so we need to keep the live instance around for when we return

          if (weed_plant_has_leaf(mt->init_event, WEED_LEAF_HOST_TAG)) {
            char *keystr = weed_get_string_value(mt->init_event, WEED_LEAF_HOST_TAG, NULL);
            int key = atoi(keystr) + 1;
            lives_freep((void **)&keystr);

            // get the rendering version:
            mt->current_rfx->source = (void *)rte_keymode_get_instance(key, 0); // adds a ref

            // for the preview we will use a copy of the current in_params from the live instance
            // interpolation is OFF here so we will see exactly the current values
            if (mt->current_rfx->source) {
              int nparams;
              weed_plant_t **src_params = weed_instance_get_in_params(live_inst, &nparams);
              weed_plant_t **dst_params = weed_instance_get_in_params((weed_plant_t *)mt->current_rfx->source, NULL);
              for (int i = 0; i < nparams; i++) {
                weed_leaf_dup(dst_params[i], src_params[i], WEED_LEAF_VALUE);
              }
              if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(mt->solo_check)))
                mt->solo_inst = mt->current_rfx->source;
              else
                mt->solo_inst = NULL;

              /// we want to preview the (first) output layer
              mt->preview_layer = weed_get_int_value(mt->init_event, WEED_LEAF_OUT_TRACKS, NULL);

              /// release the ref from rte_keymode_get_instance()
              weed_instance_unref((weed_plant_t *)mt->current_rfx->source);
              lives_free(src_params);
              lives_free(dst_params);
	      // *INDENT-OFF*
            }}}}
      // *INDENT-ON*

      mainw->last_display_ticks = 0;

      // start decoder plugins, one per track
      init_track_decoders();

      // render one frame
      process_events(mt->pb_start_event, FALSE, 0);
      free_track_decoders();
      mt->preview_layer = -100000;
      mt->solo_inst = NULL;
      mainw->internal_messaging = internal_messaging;
      mainw->current_file = current_file;
      deinit_render_effects();

      // if we are previewing an effect we now need to restore the live inst
      if (live_inst) mt->current_rfx->source = (void *)live_inst;

      if (is_rendering) {
        restore_weed_instances();
        restore_host_tags(mt->event_list, curr_tc);
      }
      mainw->files[mt->render_file]->next_event = NULL;
      mainw->is_rendering = is_rendering;
    }
  }

  if (return_layer) {
    weed_layer_t *layer;
    if (needs_idlefunc || (!did_backup && mt->auto_changed)) {
      mt->idlefunc = mt_idle_add(mt);
    }
    layer = mainw->frame_layer;
    if (frame_layer) {
      mainw->frame_layer = frame_layer;
      weed_layer_unref(mainw->frame_layer);
      weed_layer_unref(mainw->frame_layer);
    } else mainw->frame_layer = NULL;
    return layer;
  }

#if GTK_CHECK_VERSION(3, 0, 0)
  if (mt->frame_pixbuf && mt->frame_pixbuf != mainw->imframe) {
    lives_widget_object_unref(mt->frame_pixbuf);
    mt->frame_pixbuf = NULL;
  }
#endif

  if (mt->frame_pixbuf && mt->frame_pixbuf == mainw->imframe) {
    // size_request, reset play frame size
    // try to expand / shrink
    lives_widget_set_size_request(mt->preview_eventbox, GUI_SCREEN_WIDTH / PEB_WRATIO,
                                  GUI_SCREEN_HEIGHT / PEB_HRATIO);
  }

  if (mainw->frame_layer) {
    // we called process_events(), so that should have created mainw->frame_layer
    LiVESPixbuf *pixbuf = NULL;
    int pwidth, pheight, lb_width, lb_height;
    int cpal = WEED_PALETTE_RGB24, layer_palette;
    boolean was_letterboxed = FALSE;

    check_layer_ready(mainw->frame_layer);
    layer_palette = weed_layer_get_palette(mainw->frame_layer);
    if (weed_palette_has_alpha(layer_palette)) cpal = WEED_PALETTE_RGBA32;

    pwidth = mt->play_width;
    pheight = mt->play_height;

    if (weed_get_boolean_value(mainw->frame_layer, "letterboxed", NULL) == WEED_FALSE) {

      if (prefs->letterbox_mt) {
        lb_width = weed_layer_get_width_pixels(mainw->frame_layer);
        lb_height = weed_layer_get_height(mainw->frame_layer);
        calc_maxspect(pwidth, pheight, &lb_width, &lb_height);
        pwidth = lb_width;
        pheight = lb_height;
        if (!letterbox_layer(mainw->frame_layer, pwidth, pheight, lb_width, lb_height,
                             LIVES_INTERP_BEST, cpal, 0)) {
          if (frame_layer) {
            if (mainw->frame_layer) weed_layer_free(mainw->frame_layer);
            mainw->frame_layer = frame_layer;
            weed_layer_unref(mainw->frame_layer);
            weed_layer_unref(mainw->frame_layer);
          }
          return NULL;
        }
        was_letterboxed = TRUE;
      }
    }

    if (!was_letterboxed) resize_layer(mainw->frame_layer, pwidth, pheight, LIVES_INTERP_BEST, cpal, 0);

    convert_layer_palette_full(mainw->frame_layer, cpal, 0, 0, 0, WEED_GAMMA_SRGB);

    if (prefs->use_screen_gamma)
      gamma_convert_layer(WEED_GAMMA_MONITOR, mainw->frame_layer);

    if (mt->framedraw) mt_framedraw(mt, mainw->frame_layer); // framedraw will free the frame_layer itself
    else {
      if (lastblank) {
        clear_widget_bg(mainw->play_image, mainw->play_surface);
        lastblank = FALSE;
      }
      if (!pixbuf) {
        pixbuf = layer_to_pixbuf(mainw->frame_layer, TRUE, TRUE);
      }
#if GTK_CHECK_VERSION(3, 0, 0)
      // set frame_pixbuf, this gets painted in in expose_event
      mt->frame_pixbuf = pixbuf;
      set_drawing_area_from_pixbuf(mainw->play_image, mt->frame_pixbuf, mainw->play_surface);
#endif
      lives_widget_queue_draw(mt->preview_eventbox);
      weed_plant_free(mainw->frame_layer);
      mainw->frame_layer = NULL;
    }
  } else {
    // no frame - show blank
    if (!lastblank) {
      clear_widget_bg(mainw->play_image, mainw->play_surface);
      lastblank = TRUE;
    }

#if GTK_CHECK_VERSION(3, 0, 0)
    // set frame_pixbuf, this gets painted in in expose_event
    mt->frame_pixbuf = mainw->imframe;
    set_drawing_area_from_pixbuf(mainw->play_image, mt->frame_pixbuf, mainw->play_surface);
#else
    set_drawing_area_from_pixbuf(mainw->play_image, mainw->imframe, mainw->play_surface);
#endif
    lives_widget_queue_draw(mt->preview_eventbox);
  }
  if (mt->frame_pixbuf && mt->frame_pixbuf != mainw->imframe) {
    lives_widget_object_unref(mt->frame_pixbuf);
  }
  mt->frame_pixbuf = NULL;

  /// restore original mainw->frame_layer
  if (frame_layer) {
    if (mainw->frame_layer) weed_layer_free(mainw->frame_layer);
    mainw->frame_layer = frame_layer;
    weed_layer_unref(mainw->frame_layer);
    weed_layer_unref(mainw->frame_layer);
  }

  lives_ruler_set_value(LIVES_RULER(mt->timeline), ptr_time);
  lives_widget_queue_draw(mt->timeline);

  if (needs_idlefunc || (!did_backup && mt->auto_changed)) {
    mt->idlefunc = mt_idle_add(mt);
  }

  return NULL;
}


boolean mt_idle_show_current_frame(livespointer data) {
  lives_mt *mt = (lives_mt *)data;
  if (!mainw->multitrack || lives_get_status() != LIVES_STATUS_IDLE) return FALSE;
  mt_tl_move(mt, mt->opts.ptr_time);
  lives_widget_queue_draw(mainw->play_image);
  return FALSE;
}


static void _mt_tl_move(lives_mt * mt, double pos) {
  pos = q_dbl(pos, mt->fps);
  if (pos < 0.) pos = 0.;

  // after this, we need to reference ONLY mt->ptr_time, since it may become outside the range of mt->timeline
  // thus we cannot rely on reading the value from mt->timeline
  mt->ptr_time = lives_ruler_set_value(LIVES_RULER(mt->timeline), pos);

  if (mt->is_ready) mt_unpaint_lines(mt);

  if (pos > 0.) {
    lives_widget_set_sensitive(mt->rewind, TRUE);
    lives_widget_set_sensitive(mainw->m_rewindbutton, TRUE);
  } else {
    lives_widget_set_sensitive(mt->rewind, FALSE);
    lives_widget_set_sensitive(mainw->m_rewindbutton, FALSE);
  }

  if (mt->is_paused) {
    mt->is_paused = FALSE;
    lives_widget_set_sensitive(mainw->stop, FALSE);
    lives_widget_set_sensitive(mainw->m_stopbutton, FALSE);
  }

  lives_widget_queue_draw(mt->timeline);
  if (mt->init_event && mt->poly_state == POLY_PARAMS && !mt->block_node_spin) {
    //mt->block_tl_move = TRUE;
    lives_spin_button_set_value(LIVES_SPIN_BUTTON(mt->node_spinbutton),
                                pos - get_event_timecode(mt->init_event) / TICKS_PER_SECOND_DBL);
    //mt->block_tl_move = FALSE;
  }

  mt_update_timecodes(mt, pos);

  if (pos > mt->region_end - 1. / mt->fps) lives_widget_set_sensitive(mt->tc_to_rs, FALSE);
  else lives_widget_set_sensitive(mt->tc_to_rs, TRUE);
  if (pos < mt->region_start + 1. / mt->fps) lives_widget_set_sensitive(mt->tc_to_re, FALSE);
  else lives_widget_set_sensitive(mt->tc_to_re, TRUE);

  mt->fx_order = FX_ORD_NONE;
  if (mt->init_event) {
    weed_timecode_t tc = q_gint64(pos * TICKS_PER_SECOND_DBL, mt->fps);
    weed_plant_t *deinit_event = weed_get_plantptr_value(mt->init_event, WEED_LEAF_DEINIT_EVENT, NULL);
    if (tc < get_event_timecode(mt->init_event) || tc > get_event_timecode(deinit_event)) {
      mt->init_event = NULL;
      mt->current_rfx = NULL;
      if (mt->poly_state == POLY_PARAMS) polymorph(mt, POLY_FX_STACK);
    }
  }

  if (mt->poly_state == POLY_FX_STACK) polymorph(mt, POLY_FX_STACK);
  if (mt->is_ready) {
    mt_show_current_frame(mt, FALSE);
    mt_paint_lines(mt, pos, TRUE, NULL);
  }
}


void mt_tl_move(lives_mt * mt, double pos) {
  if (LIVES_IS_PLAYING) return;
  main_thread_execute((lives_funcptr_t)_mt_tl_move, -1, NULL, "vd", mt, pos);
}


LIVES_GLOBAL_INLINE void mt_tl_move_relative(lives_mt * mt, double pos_rel) {
  mt_tl_move(mt, mt->ptr_time + pos_rel);
}


void mt_set_time_scrollbar(lives_mt * mt) {
  double page = mt->tl_max - mt->tl_min;
  if (mt->end_secs == 0.) mt->end_secs = DEF_TIME;

  if (mt->tl_max > mt->end_secs) mt->end_secs = mt->tl_max;

  lives_widget_object_freeze_notify(LIVES_WIDGET_OBJECT(mt->hadjustment));
  lives_range_set_range(LIVES_RANGE(mt->time_scrollbar), 0., mt->end_secs);
  lives_range_set_increments(LIVES_RANGE(mt->time_scrollbar), page / 4., page);
  lives_adjustment_set_page_size(LIVES_ADJUSTMENT(mt->hadjustment), page);
  lives_adjustment_set_value(LIVES_ADJUSTMENT(mt->hadjustment), mt->tl_min);
  lives_widget_object_thaw_notify(LIVES_WIDGET_OBJECT(mt->hadjustment));
  lives_widget_queue_draw(mt->time_scrollbar);
}


static void unpaint_line(lives_mt * mt, LiVESWidget * eventbox) {
  int xoffset;
  int ebwidth;

  if (mt->redraw_block) return; // don't update during expose event, otherwise we might leave lines
  if (!lives_widget_is_visible(eventbox)) return;
  if (!mt->is_ready) return;
  if ((xoffset = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "has_line"))) < 0) return;

  ebwidth = lives_widget_get_allocation_width(mt->timeline);

  if (xoffset < ebwidth) {
    lives_widget_queue_draw_area(eventbox, xoffset - 4, 0, 9, lives_widget_get_allocation_height(eventbox));
    // lives_widget_process_updates(eventbox);
  }

  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(eventbox), "has_line", LIVES_INT_TO_POINTER(-1));
}


void mt_unpaint_lines(lives_mt * mt) {
  if (!mt->is_ready) return;
  unpaint_line(mt, mt->timeline_table);
  return;
}


static void paint_line(lives_mt * mt, LiVESWidget * eventbox, int offset, double currtime,
                       lives_painter_t *cr) {
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(eventbox), "has_line", LIVES_INT_TO_POINTER(offset));
  lives_widget_queue_draw(eventbox);
}


void mt_paint_lines(lives_mt * mt, double currtime, boolean unpaint, lives_painter_t *cr) {
  int ebwidth;
  int offset, off_x;

  if (!mt->is_ready) return;

  ebwidth = lives_widget_get_allocation_width(mt->timeline);

  if (unpaint) unpaint_line(mt, mt->timeline_table);

  if (currtime < mt->tl_min || currtime > mt->tl_max) return;
  offset = (currtime - mt->tl_min) / (mt->tl_max - mt->tl_min) * (double)ebwidth;

  lives_widget_get_position(mt->timeline_eb, &off_x, NULL);
  offset += off_x;

  if (offset > off_x && offset < ebwidth + off_x) {
    paint_line(mt, mt->timeline_table, offset, currtime, cr);
  }
}


void mt_set_in_out_spin_ranges(lives_mt * mt, weed_timecode_t start_tc, weed_timecode_t end_tc) {
  track_rect *block = mt->block_selected;
  weed_timecode_t min_tc = 0, max_tc = -1;
  weed_timecode_t offset_start = get_event_timecode(block->start_event);
  int filenum;
  double in_val = start_tc / TICKS_PER_SECOND_DBL, out_val = end_tc / TICKS_PER_SECOND_DBL, in_start_range = 0.,
         out_start_range = in_val + 1. / mt->fps;
  double out_end_range, real_out_end_range;
  double in_end_range = out_val - 1. / mt->fps, real_in_start_range = in_start_range;
  double avel = 1.;

  int track = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(block->eventbox), LAYER_NUMBER_KEY));

  lives_signal_handler_block(mt->spinbutton_out, mt->spin_out_func);
  lives_signal_handler_block(mt->spinbutton_in, mt->spin_in_func);

  if (block->prev) min_tc = get_event_timecode(block->prev->end_event) + (double)(track >= 0) * TICKS_PER_SECOND_DBL /
                              mt->fps;
  if (block->next) max_tc = get_event_timecode(block->next->start_event) - (double)(
                                track >= 0) * TICKS_PER_SECOND_DBL / mt->fps;

  if (track >= 0) {
    filenum = get_frame_event_clip(block->start_event, track);
    if (!IS_VALID_CLIP(filenum)) return;
    // actually we should quantise this to the mt->fps, but we leave it in case clip has only
    // one frame -> otherwise we could quantise to zero frames
    out_end_range = count_resampled_frames(mainw->files[filenum]->frames, mainw->files[filenum]->fps, mt->fps) / mt->fps;
  } else {
    filenum = get_audio_frame_clip(block->start_event, track);
    if (!IS_VALID_CLIP(filenum)) return;
    out_end_range = q_gint64(mainw->files[filenum]->laudio_time * TICKS_PER_SECOND_DBL, mt->fps) / TICKS_PER_SECOND_DBL;
    avel = get_audio_frame_vel(block->start_event, track);
  }
  real_out_end_range = out_end_range;

  if (mt->opts.insert_mode != INSERT_MODE_OVERWRITE) {
    if (!block->end_anchored && max_tc > -1 &&
        (((max_tc - offset_start) / TICKS_PER_SECOND_DBL * ABS(avel) + in_val) < out_end_range))
      real_out_end_range = q_gint64((max_tc - offset_start) * ABS(avel) + in_val * TICKS_PER_SECOND_DBL,
                                    mt->fps) / TICKS_PER_SECOND_DBL;
    if (!block->start_anchored && min_tc > -1 &&
        (((min_tc - offset_start) / TICKS_PER_SECOND_DBL * ABS(avel) + in_val) > in_start_range))
      real_in_start_range = q_gint64((min_tc - offset_start) * ABS(avel) + in_val * TICKS_PER_SECOND_DBL,
                                     mt->fps) / TICKS_PER_SECOND_DBL;
    if (!block->start_anchored) out_end_range = real_out_end_range;
    if (!block->end_anchored) in_start_range = real_in_start_range;
  }

  if (block->end_anchored && (out_val - in_val > out_start_range)) out_start_range = in_start_range + out_val - in_val;
  if (block->start_anchored && (out_end_range - out_val + in_val) < in_end_range) in_end_range = out_end_range - out_val + in_val;

  in_end_range = lives_fix(in_end_range, 2);
  real_out_end_range = lives_fix(real_out_end_range, 2);

  out_start_range = lives_fix(out_start_range, 2);
  real_in_start_range = lives_fix(real_in_start_range, 2);

  if (avel > 0.) {
    lives_spin_button_set_range(LIVES_SPIN_BUTTON(mt->spinbutton_out), out_start_range, real_out_end_range);
    lives_spin_button_set_range(LIVES_SPIN_BUTTON(mt->spinbutton_in), real_in_start_range, in_end_range);
  } else {
    lives_spin_button_set_range(LIVES_SPIN_BUTTON(mt->spinbutton_in), out_start_range, real_out_end_range);
    lives_spin_button_set_range(LIVES_SPIN_BUTTON(mt->spinbutton_out), real_in_start_range, in_end_range);
  }

  lives_signal_handler_unblock(mt->spinbutton_out, mt->spin_out_func);
  lives_signal_handler_unblock(mt->spinbutton_in, mt->spin_in_func);
}


void mt_desensitise(lives_mt * mt) {
  double val;

  mainw->sense_state &= LIVES_SENSE_STATE_INTERACTIVE;
  mainw->sense_state |= LIVES_SENSE_STATE_INSENSITIZED;

  lives_widget_set_sensitive(mt->clipedit, FALSE);
  lives_widget_set_sensitive(mt->insert, FALSE);
  lives_widget_set_sensitive(mt->audio_insert, FALSE);
  lives_widget_set_sensitive(mt->playall, FALSE);
  lives_widget_set_sensitive(mt->playsel, FALSE);
  lives_widget_set_sensitive(mt->view_events, FALSE);
  lives_widget_set_sensitive(mt->view_sel_events, FALSE);
  lives_widget_set_sensitive(mt->render, FALSE);
  lives_widget_set_sensitive(mt->prerender_aud, FALSE);
  lives_widget_set_sensitive(mt->delblock, FALSE);
#ifdef LIBAV_TRANSCODE
  lives_widget_set_sensitive(mt->transcode, FALSE);
#endif
  lives_widget_set_sensitive(mt->save_event_list, FALSE);
  lives_widget_set_sensitive(mt->load_event_list, FALSE);
  lives_widget_set_sensitive(mt->clear_event_list, FALSE);
  lives_widget_set_sensitive(mt->remove_gaps, FALSE);
  lives_widget_set_sensitive(mt->remove_first_gaps, FALSE);
  lives_widget_set_sensitive(mt->undo, FALSE);
  lives_widget_set_sensitive(mt->redo, FALSE);
  lives_widget_set_sensitive(mt->show_quota, FALSE);
  lives_widget_set_sensitive(mt->jumpback, FALSE);
  lives_widget_set_sensitive(mt->jumpnext, FALSE);
  lives_widget_set_sensitive(mt->mark_jumpback, FALSE);
  lives_widget_set_sensitive(mt->mark_jumpnext, FALSE);
  lives_widget_set_sensitive(mt->fx_edit, FALSE);
  lives_widget_set_sensitive(mt->fx_delete, FALSE);
  lives_widget_set_sensitive(mt->checkbutton_avel_reverse, FALSE);
  lives_widget_set_sensitive(mt->spinbutton_avel, FALSE);
  lives_widget_set_sensitive(mt->avel_scale, FALSE);
  lives_widget_set_sensitive(mt->change_vals, FALSE);
  lives_widget_set_sensitive(mt->add_vid_behind, FALSE);
  lives_widget_set_sensitive(mt->add_vid_front, FALSE);
  lives_widget_set_sensitive(mt->quit, FALSE);
  lives_widget_set_sensitive(mt->clear_ds, FALSE);
  lives_widget_set_sensitive(mt->open_menu, FALSE);
#ifdef HAVE_WEBM
  lives_widget_set_sensitive(mt->open_loc_menu, FALSE);
#endif
#ifdef ENABLE_DVD_GRAB
  lives_widget_set_sensitive(mt->vcd_dvd_menu, FALSE);
#endif
#ifdef HAVE_LDVGRAB
  lives_widget_set_sensitive(mt->device_menu, FALSE);
#endif
  lives_widget_set_sensitive(mt->recent_menu, FALSE);
  lives_widget_set_sensitive(mt->load_set, FALSE);
  lives_widget_set_sensitive(mt->save_set, FALSE);
  lives_widget_set_sensitive(mt->close, FALSE);
  lives_widget_set_sensitive(mt->capture, FALSE);
  lives_widget_set_sensitive(mt->gens_submenu, FALSE);
  lives_widget_set_sensitive(mt->troubleshoot, FALSE);
  lives_widget_set_sensitive(mt->expl_missing, FALSE);

  lives_widget_set_sensitive(mt->fx_region, FALSE);
  lives_widget_set_sensitive(mt->ins_gap_sel, FALSE);
  lives_widget_set_sensitive(mt->ins_gap_cur, FALSE);

  lives_widget_set_sensitive(mt->addback_audio, FALSE);

  if (mt->poly_state == POLY_IN_OUT) {
    if (mt->block_selected) {
      val = lives_spin_button_get_value(LIVES_SPIN_BUTTON(mt->spinbutton_in));
      lives_spin_button_set_range(LIVES_SPIN_BUTTON(mt->spinbutton_in), val, val);

      val = lives_spin_button_get_value(LIVES_SPIN_BUTTON(mt->spinbutton_out));
      lives_spin_button_set_range(LIVES_SPIN_BUTTON(mt->spinbutton_out), val, val);
    }
  }
}


void mt_sensitise(lives_mt * mt) {
  LiVESWidget *eventbox = NULL;

  if (mt->in_sensitise) return; // prevent infinite loops
  mt->in_sensitise = TRUE;

  mainw->sense_state &= LIVES_SENSE_STATE_INTERACTIVE;
  mainw->sense_state |= LIVES_SENSE_STATE_SENSITIZED;

  if (mt->event_list && get_first_event(mt->event_list)) {
    lives_widget_set_sensitive(mt->playall, TRUE);
    lives_widget_set_sensitive(mainw->m_playbutton, TRUE);
    lives_widget_set_sensitive(mt->view_events, TRUE);
    lives_widget_set_sensitive(mt->view_sel_events, mt->region_start != mt->region_end);
    lives_widget_set_sensitive(mt->render, TRUE);
#ifdef LIBAV_TRANSCODE
    lives_widget_set_sensitive(mt->transcode, TRUE);
#endif
    if (mt->avol_init_event && mt->opts.pertrack_audio && mainw->files[mt->render_file]->achans > 0)
      lives_widget_set_sensitive(mt->prerender_aud, TRUE);
    lives_widget_set_sensitive(mt->save_event_list, !mainw->recording_recovered);
  } else {
    lives_widget_set_sensitive(mt->playall, FALSE);
    lives_widget_set_sensitive(mt->playsel, FALSE);
    lives_widget_set_sensitive(mainw->m_playbutton, FALSE);
    lives_widget_set_sensitive(mt->view_events, FALSE);
    lives_widget_set_sensitive(mt->view_sel_events, FALSE);
    lives_widget_set_sensitive(mt->render, FALSE);
    lives_widget_set_sensitive(mt->save_event_list, FALSE);
#ifdef LIBAV_TRANSCODE
    lives_widget_set_sensitive(mt->transcode, FALSE);
#endif
  }

  if (mt->event_list) lives_widget_set_sensitive(mt->clear_event_list, TRUE);

  lives_widget_set_sensitive(mt->add_vid_behind, TRUE);
  lives_widget_set_sensitive(mt->add_vid_front, TRUE);
  lives_widget_set_sensitive(mt->quit, TRUE);
  lives_widget_set_sensitive(mt->clear_ds, TRUE);
  lives_widget_set_sensitive(mt->open_menu, TRUE);
  lives_widget_set_sensitive(mt->show_quota, TRUE);
#ifdef HAVE_WEBM
  lives_widget_set_sensitive(mt->open_loc_menu, TRUE);
#endif
#ifdef ENABLE_DVD_GRAB
  lives_widget_set_sensitive(mt->vcd_dvd_menu, TRUE);
#endif
#ifdef HAVE_LDVGRAB
  lives_widget_set_sensitive(mt->device_menu, TRUE);
#endif
  lives_widget_set_sensitive(mt->recent_menu, TRUE);
  lives_widget_set_sensitive(mt->capture, TRUE);
  lives_widget_set_sensitive(mt->gens_submenu, TRUE);
  lives_widget_set_sensitive(mt->troubleshoot, TRUE);
  lives_widget_set_sensitive(mt->expl_missing, TRUE);

  lives_widget_set_sensitive(mainw->m_mutebutton, TRUE);

  lives_widget_set_sensitive(mt->load_set, !mainw->was_set);

  if (mt->undoable) lives_widget_set_sensitive(mt->undo, TRUE);
  if (mt->redoable) lives_widget_set_sensitive(mt->redo, TRUE);
  if (mt->selected_init_event) {
    lives_widget_set_sensitive(mt->fx_edit, TRUE);
    lives_widget_set_sensitive(mt->fx_delete, TRUE);
  }

  if (mt->checkbutton_avel_reverse) {
    lives_widget_set_sensitive(mt->checkbutton_avel_reverse, TRUE);

    if (mt->block_selected && (!mt->block_selected->start_anchored ||
                               !mt->block_selected->end_anchored) && !lives_toggle_button_get_active
        (LIVES_TOGGLE_BUTTON(mt->checkbutton_avel_reverse))) {
      lives_widget_set_sensitive(mt->spinbutton_avel, TRUE);
      lives_widget_set_sensitive(mt->avel_scale, TRUE);
    }
  }

  if (!mt->opts.back_audio_tracks && mainw->files[mt->render_file]->achans)
    lives_widget_set_sensitive(mt->addback_audio, TRUE);

  lives_widget_set_sensitive(mt->load_event_list, *mainw->set_name != 0);
  lives_widget_set_sensitive(mt->clipedit, TRUE);
  if (mt->file_selected > -1) {
    if (mainw->files[mt->file_selected]->frames > 0) lives_widget_set_sensitive(mt->insert, TRUE);
    if (mainw->files[mt->file_selected]->achans > 0 && mainw->files[mt->file_selected]->laudio_time > 0.)
      lives_widget_set_sensitive(mt->audio_insert, TRUE);
    lives_widget_set_sensitive(mt->save_set, !mainw->recording_recovered);
    lives_widget_set_sensitive(mt->close, TRUE);
    lives_widget_set_sensitive(mt->adjust_start_end, TRUE);
  }

  if (mt->video_draws && mt->current_track > -1)
    eventbox = (LiVESWidget *)lives_list_nth_data(mt->video_draws, mt->current_track);
  else if (mt->audio_draws) eventbox = (LiVESWidget *)mt->audio_draws->data;

  if (eventbox) {
    lives_widget_set_sensitive(mt->jumpback, lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "blocks") != NULL);
    lives_widget_set_sensitive(mt->jumpnext, lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "blocks") != NULL);
  }

  if (mt->tl_marks) {
    lives_widget_set_sensitive(mt->mark_jumpback, TRUE);
    lives_widget_set_sensitive(mt->mark_jumpnext, TRUE);
  }

  lives_widget_set_sensitive(mt->change_vals, TRUE);

  if (mt->block_selected) {
    lives_widget_set_sensitive(mt->delblock, TRUE);
    if (mt->poly_state == POLY_IN_OUT && mt->block_selected->ordered) {
      weed_timecode_t offset_end = mt->block_selected->offset_start + (weed_timecode_t)(TICKS_PER_SECOND_DBL / mt->fps)
                                   + (get_event_timecode(mt->block_selected->end_event) - get_event_timecode(mt->block_selected->start_event));
      mt_set_in_out_spin_ranges(mt, mt->block_selected->offset_start, offset_end);
    }
  } else if (mt->poly_state == POLY_IN_OUT) {
    int filenum = mt_file_from_clip(mt, mt->clip_selected);
    lives_signal_handler_block(mt->spinbutton_in, mt->spin_in_func);
    lives_signal_handler_block(mt->spinbutton_out, mt->spin_out_func);
    lives_spin_button_set_range(LIVES_SPIN_BUTTON(mt->spinbutton_in), 1., mainw->files[filenum]->frames);
    lives_spin_button_set_range(LIVES_SPIN_BUTTON(mt->spinbutton_out), 1., mainw->files[filenum]->frames);

    lives_signal_handler_unblock(mt->spinbutton_in, mt->spin_in_func);
    lives_signal_handler_unblock(mt->spinbutton_out, mt->spin_out_func);
  }

  if (mt->region_end > mt->region_start && mt->event_list && get_first_event(mt->event_list)) {
    if (mt->selected_tracks) {
      lives_widget_set_sensitive(mt->fx_region, TRUE);
      lives_widget_set_sensitive(mt->ins_gap_sel, TRUE);
      lives_widget_set_sensitive(mt->remove_gaps, TRUE);
      lives_widget_set_sensitive(mt->remove_first_gaps, TRUE);
      lives_widget_set_sensitive(mt->fx_region, TRUE);
      if (mt->selected_tracks && mt->region_end != mt->region_start) {
        switch (lives_list_length(mt->selected_tracks)) {
        case 1:
          lives_widget_set_sensitive(mt->fx_region_v, TRUE);
          if (cfile->achans == 0) lives_widget_set_sensitive(mt->fx_region_a, FALSE);
          break;
        case 2:
          lives_widget_set_sensitive(mt->fx_region_v, FALSE);
          lives_widget_set_sensitive(mt->fx_region_a, FALSE);
          if (!mt->opts.pertrack_audio)
            lives_widget_set_sensitive(mt->fx_region_2a, FALSE);
          break;
        default:
          break;
        }
      }
    }
    lives_widget_set_sensitive(mt->playsel, TRUE);
    lives_widget_set_sensitive(mt->ins_gap_cur, TRUE);
    lives_widget_set_sensitive(mt->view_sel_events, TRUE);
  }

  track_select(mt);

  mt->in_sensitise = FALSE;
}


void mt_swap_play_pause(lives_mt * mt, boolean put_pause) {
  LiVESWidget *tmp_img = NULL;
  static LiVESWidgetClosure *freeze_closure = NULL;

  if (!freeze_closure) freeze_closure = lives_cclosure_new(LIVES_GUI_CALLBACK(freeze_callback), NULL, NULL);

  if (put_pause) {
#if GTK_CHECK_VERSION(2, 6, 0)
    tmp_img =
      lives_image_new_from_stock
      (LIVES_STOCK_MEDIA_PAUSE, lives_toolbar_get_icon_size(LIVES_TOOLBAR(mt->btoolbar2)));
#endif
    lives_menu_item_set_text(mt->playall, _("_Pause"), TRUE);
    lives_widget_set_tooltip_text(mainw->m_playbutton, _("Pause (p)"));
    lives_widget_set_sensitive(mt->playall, TRUE);
    lives_widget_set_sensitive(mainw->m_playbutton, TRUE);
    lives_signal_handlers_disconnect_by_func(mt->playall, LIVES_GUI_CALLBACK(on_playall_activate), NULL);
    lives_signal_sync_connect(LIVES_GUI_OBJECT(mt->playall), LIVES_WIDGET_ACTIVATE_SIGNAL,
                              LIVES_GUI_CALLBACK(on_playall_activate), NULL);
    lives_signal_handlers_disconnect_by_func(mainw->m_playbutton, LIVES_GUI_CALLBACK(on_playall_activate), NULL);
    lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->m_playbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                              LIVES_GUI_CALLBACK(on_playall_activate), NULL);
    lives_accel_group_connect(LIVES_ACCEL_GROUP(mt->accel_group), LIVES_KEY_BackSpace,
                              (LiVESXModifierType)LIVES_CONTROL_MASK, (LiVESAccelFlags)0, freeze_closure);
  } else {
    tmp_img = lives_image_new_from_stock(LIVES_STOCK_MEDIA_PLAY, lives_toolbar_get_icon_size(LIVES_TOOLBAR(mt->btoolbar2)));
    lives_menu_item_set_text(mt->playall, _("_Play from Timeline Position"), TRUE);
    lives_widget_set_tooltip_text(mainw->m_playbutton, _("Play all (p)"));
    lives_signal_handlers_disconnect_by_func(mt->playall, LIVES_GUI_CALLBACK(on_playall_activate), NULL);
    lives_signal_connect(LIVES_GUI_OBJECT(mt->playall), LIVES_WIDGET_ACTIVATE_SIGNAL,
                         LIVES_GUI_CALLBACK(on_playall_activate), NULL);
    lives_signal_handlers_disconnect_by_func(mainw->m_playbutton, LIVES_GUI_CALLBACK(on_playall_activate), NULL);
    lives_signal_connect(LIVES_GUI_OBJECT(mainw->m_playbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                         LIVES_GUI_CALLBACK(on_playall_activate), NULL);
    lives_accel_group_disconnect(LIVES_ACCEL_GROUP(mt->accel_group), freeze_closure);
    freeze_closure = NULL;
  }

  if (tmp_img) lives_widget_show(tmp_img);
  lives_tool_button_set_icon_widget(LIVES_TOOL_BUTTON(mainw->m_playbutton), tmp_img);
}


// AUDIO MIXER FUNCTIONS ////

void on_amixer_close_clicked(LiVESButton * button, lives_mt * mt) {
  lives_amixer_t *amixer = mt->amixer;
  double val;

  if (!LIVES_IS_INTERACTIVE) return;

  mt->opts.gang_audio = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(amixer->gang_checkbutton));

  // set vols from slider vals

  for (int i = 0; i < amixer->nchans; i++) {
#if ENABLE_GIW
    if (prefs->lamp_buttons) {
      val = giw_vslider_get_value(GIW_VSLIDER(amixer->ch_sliders[i]));
    } else {
#endif
      val = lives_range_get_value(LIVES_RANGE(amixer->ch_sliders[i]));
#if ENABLE_GIW
    }
#endif
    if (0)
      val = lives_vol_from_linear(val);
    set_mixer_track_vol(mt, i, val);
  }

  lives_widget_destroy(amixer->window);
  lives_free(amixer->ch_sliders);
  lives_free(amixer->ch_slider_fns);
  lives_free(amixer);
  mt->amixer = NULL;
  if (mt->audio_vols_back) lives_list_free(mt->audio_vols_back);
  //lives_widget_set_sensitive(mt->prerender_aud,TRUE);
}


static void on_amixer_reset_clicked(LiVESButton * button, lives_mt * mt) {
  lives_amixer_t *amixer = mt->amixer;
  int i;

  if (!LIVES_IS_INTERACTIVE) return;

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(amixer->inv_checkbutton), FALSE);
  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(amixer->gang_checkbutton), mt->opts.gang_audio);

  // copy vols to slider vals

  for (i = 0; i < amixer->nchans; i++) {
    float val = (float)LIVES_POINTER_TO_INT(lives_list_nth_data(mt->audio_vols_back, i)) / LIVES_AVOL_SCALE;
    // set linear slider values - we will convert to non linear when applying
#if ENABLE_GIW
    if (prefs->lamp_buttons) {
      lives_signal_handler_block(giw_vslider_get_adjustment(GIW_VSLIDER(amixer->ch_sliders[i])), amixer->ch_slider_fns[i]);
      giw_vslider_set_value(GIW_VSLIDER(amixer->ch_sliders[i]), val);
      lives_signal_handler_unblock(giw_vslider_get_adjustment(GIW_VSLIDER(amixer->ch_sliders[i])), amixer->ch_slider_fns[i]);
    } else {
#endif
      lives_signal_handler_block(lives_range_get_adjustment(LIVES_RANGE(amixer->ch_sliders[i])), amixer->ch_slider_fns[i]);
      lives_range_set_value(LIVES_RANGE(amixer->ch_sliders[i]), val);
      lives_signal_handler_unblock(lives_range_get_adjustment(LIVES_RANGE(amixer->ch_sliders[i])), amixer->ch_slider_fns[i]);
#if ENABLE_GIW
    }
#endif
  }
}


static void after_amixer_gang_toggled(LiVESToggleButton * toggle, lives_amixer_t *amixer) {
  lives_widget_set_sensitive(amixer->inv_checkbutton, (lives_toggle_button_get_active(toggle)));
}


void on_amixer_slider_changed(LiVESAdjustment * adj, lives_mt * mt) {
  lives_amixer_t *amixer = mt->amixer;
  int layer = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(adj), "layer"));
  boolean gang = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(amixer->gang_checkbutton));
  boolean inv = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(amixer->inv_checkbutton));
  double val;
  int i;

#if ENABLE_GIW
  if (prefs->lamp_buttons) {
    GiwVSlider *slider = GIW_VSLIDER(amixer->ch_sliders[layer]);
    val = giw_vslider_get_value(slider);
  } else {
#endif
    if (TRUE) {
      LiVESRange *range = LIVES_RANGE(amixer->ch_sliders[layer]);
      val = lives_range_get_value(range);
    }
#if ENABLE_GIW
  }
#endif

  if (gang) {
    if (layer > 0) {
      for (i = mt->opts.back_audio_tracks; i < amixer->nchans; i++) {
#if ENABLE_GIW
        if (prefs->lamp_buttons) {
          lives_signal_handler_block(giw_vslider_get_adjustment(GIW_VSLIDER(amixer->ch_sliders[i])), amixer->ch_slider_fns[i]);
          giw_vslider_set_value(GIW_VSLIDER(amixer->ch_sliders[i]), val);
          lives_signal_handler_unblock(giw_vslider_get_adjustment(GIW_VSLIDER(amixer->ch_sliders[i])), amixer->ch_slider_fns[i]);
        } else {
#endif
          lives_signal_handler_block(lives_range_get_adjustment(LIVES_RANGE(amixer->ch_sliders[i])), amixer->ch_slider_fns[i]);
          lives_range_set_value(LIVES_RANGE(amixer->ch_sliders[i]), val);
          lives_signal_handler_unblock(lives_range_get_adjustment(LIVES_RANGE(amixer->ch_sliders[i])), amixer->ch_slider_fns[i]);
#if ENABLE_GIW
        }
#endif
      }
      if (inv && mt->opts.back_audio_tracks > 0) {
#if ENABLE_GIW
        if (prefs->lamp_buttons) {
          lives_signal_handler_block(giw_vslider_get_adjustment(GIW_VSLIDER(amixer->ch_sliders[0])), amixer->ch_slider_fns[0]);
          giw_vslider_set_value(GIW_VSLIDER(amixer->ch_sliders[0]), 1. - val < 0. ? 0. : 1. - val);
          lives_signal_handler_unblock(giw_vslider_get_adjustment(GIW_VSLIDER(amixer->ch_sliders[0])), amixer->ch_slider_fns[0]);
        } else {
#endif
          lives_signal_handler_block(lives_range_get_adjustment(LIVES_RANGE(amixer->ch_sliders[0])), amixer->ch_slider_fns[0]);
          lives_range_set_value(LIVES_RANGE(amixer->ch_sliders[0]), 1. - val);
          lives_signal_handler_unblock(lives_range_get_adjustment(LIVES_RANGE(amixer->ch_sliders[0])), amixer->ch_slider_fns[0]);
#if ENABLE_GIW
        }
#endif
      }
    } else {
      if (inv) {
        for (i = 1; i < amixer->nchans; i++) {
#if ENABLE_GIW
          if (prefs->lamp_buttons) {
            lives_signal_handler_block(giw_vslider_get_adjustment(GIW_VSLIDER(amixer->ch_sliders[i])), amixer->ch_slider_fns[i]);
            giw_vslider_set_value(GIW_VSLIDER(amixer->ch_sliders[i]), 1. - val < 0. ? 0. : 1. - val);
            lives_signal_handler_unblock(giw_vslider_get_adjustment(GIW_VSLIDER(amixer->ch_sliders[i])),
                                         amixer->ch_slider_fns[i]);
          } else {
#endif
            lives_signal_handler_block(lives_range_get_adjustment(LIVES_RANGE(amixer->ch_sliders[i])), amixer->ch_slider_fns[i]);
            lives_range_set_value(LIVES_RANGE(amixer->ch_sliders[i]), 1. - val);
            lives_signal_handler_unblock(lives_range_get_adjustment(LIVES_RANGE(amixer->ch_sliders[i])),
                                         amixer->ch_slider_fns[i]);
#if ENABLE_GIW
          }
#endif
	  // *INDENT-OFF*
        }}}}
  // *INDENT-ON*

  if (!mt->is_rendering) {
    if (0)
      val = lives_vol_from_linear(val);
    set_mixer_track_vol(mt, layer, val);
  }
}


LiVESWidget *amixer_add_channel_slider(lives_mt * mt, int i) {
  // add a slider to audio mixer for layer i; i<0 are backing audio tracks
  // automatically sets the track name and layer number

  LiVESWidgetObject *adj;
  LiVESWidget *spinbutton;
  LiVESWidget *label;
  LiVESWidget *vbox;
  lives_amixer_t *amixer = mt->amixer;
  char *tname;

  i += mt->opts.back_audio_tracks;

  adj = (LiVESWidgetObject *)lives_adjustment_new(0.5, 0., 4., 0.01, 0.01, 0.);

#if ENABLE_GIW
  if (prefs->lamp_buttons) {
    amixer->ch_sliders[i] = giw_vslider_new(LIVES_ADJUSTMENT(adj));
    giw_vslider_set_legends_digits(GIW_VSLIDER(amixer->ch_sliders[i]), 1);
    giw_vslider_set_major_ticks_number(GIW_VSLIDER(amixer->ch_sliders[i]), 5);
    giw_vslider_set_minor_ticks_number(GIW_VSLIDER(amixer->ch_sliders[i]), 4);
    if (palette->style & STYLE_1) {
      lives_widget_set_bg_color(amixer->ch_sliders[i], LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
    }
  } else {
#endif
    amixer->ch_sliders[i] = lives_vscale_new(LIVES_ADJUSTMENT(adj));
    lives_range_set_inverted(LIVES_RANGE(amixer->ch_sliders[i]), TRUE);
    lives_scale_set_digits(LIVES_SCALE(amixer->ch_sliders[i]), 2);
    lives_scale_set_value_pos(LIVES_SCALE(amixer->ch_sliders[i]), LIVES_POS_BOTTOM);
#if ENABLE_GIW
  }
#endif

  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(amixer->ch_sliders[i]), "adj", adj);
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(adj), "layer", LIVES_INT_TO_POINTER(i));

  amixer->ch_slider_fns[i] = lives_signal_sync_connect_after(LIVES_GUI_OBJECT(adj), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                             LIVES_GUI_CALLBACK(on_amixer_slider_changed), (livespointer)mt);

  if (palette->style & STYLE_1) {
    lives_widget_set_fg_color(amixer->ch_sliders[i], LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
  }

  tname = get_track_name(mt, i - mt->opts.back_audio_tracks, TRUE);
  label = lives_standard_label_new(tname);
  lives_free(tname);

  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(amixer->ch_sliders[i]), "label", label);

  vbox = lives_vbox_new(FALSE, widget_opts.packing_height * 1.5);
  lives_box_pack_start(LIVES_BOX(vbox), label, FALSE, FALSE, widget_opts.packing_height);
  lives_box_pack_start(LIVES_BOX(vbox), amixer->ch_sliders[i], TRUE, TRUE, widget_opts.packing_height * 5);

  spinbutton = lives_standard_spin_button_new(NULL, 0.5, 0., 4., 0.01, 0.01, 3, LIVES_BOX(vbox), NULL);
  lives_spin_button_set_adjustment(LIVES_SPIN_BUTTON(spinbutton), LIVES_ADJUSTMENT(adj));

  amixer->nchans++;

  return vbox;
}


void amixer_show(LiVESButton * button, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  LiVESWidget *amixerw;
  LiVESWidget *top_vbox;
  LiVESWidget *vbox;
  LiVESWidget *vbox2;
  LiVESWidget *hbox;
  LiVESWidget *hbuttonbox;
  LiVESWidget *scrolledwindow;
  LiVESWidget *label;
  LiVESWidget *filler;
  LiVESWidget *eventbox;
  LiVESWidget *close_button;
  LiVESWidget *reset_button;
  LiVESAccelGroup *accel_group = LIVES_ACCEL_GROUP(lives_accel_group_new());

  lives_amixer_t *amixer;

  int nachans = lives_list_length(mt->audio_draws);

  int winsize_h = GUI_SCREEN_WIDTH * AMIXER_WRATIO;
  int winsize_v = GUI_SCREEN_HEIGHT * AMIXER_HRATIO;

  if (!LIVES_IS_INTERACTIVE) return;

  if (nachans == 0) return;

  if (mt->amixer) {
    on_amixer_close_clicked(NULL, mt);
    return;
  }

  mt->audio_vols_back = lives_list_copy(mt->audio_vols);

  amixer = mt->amixer = (lives_amixer_t *)lives_malloc(sizeof(lives_amixer_t));
  amixer->nchans = 0;

  amixer->ch_sliders = (LiVESWidget **)lives_malloc(nachans * sizeof(LiVESWidget *));
  amixer->ch_slider_fns = (ulong *)lives_malloc(nachans * sizeof(ulong));

  amixer->window = amixerw = lives_window_new(LIVES_WINDOW_TOPLEVEL);
  if (palette->style & STYLE_1) {
    lives_widget_set_bg_color(amixerw, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
    lives_widget_set_fg_color(amixerw, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars_fore);
  }

  lives_window_set_title(LIVES_WINDOW(amixerw), _("Multitrack Audio Mixer"));

  top_vbox = lives_vbox_new(FALSE, 0);

  amixer->main_hbox = lives_hbox_new(FALSE, widget_opts.packing_width * 2);

  scrolledwindow = lives_standard_scrolled_window_new(winsize_h, winsize_v, amixer->main_hbox);

  if (prefs->gui_monitor != 0) {
    lives_window_center(LIVES_WINDOW(amixerw));
  }

  lives_window_set_transient_for(LIVES_WINDOW(amixerw), LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET));

  lives_box_pack_start(LIVES_BOX(top_vbox), scrolledwindow, TRUE, TRUE, widget_opts.packing_height);
  lives_container_add(LIVES_CONTAINER(amixerw), top_vbox);

  hbuttonbox = lives_hbutton_box_new();
  lives_box_pack_start(LIVES_BOX(top_vbox), hbuttonbox, FALSE, TRUE, widget_opts.packing_height * 2);

  lives_button_box_set_layout(LIVES_BUTTON_BOX(hbuttonbox), LIVES_BUTTONBOX_SPREAD);

  filler = add_fill_to_box(LIVES_BOX(hbuttonbox));
  lives_widget_apply_theme2(filler, LIVES_WIDGET_STATE_NORMAL, TRUE);

  reset_button = lives_dialog_add_button_from_stock(NULL, NULL, _("_Reset values"),
                 LIVES_RESPONSE_RESET);

  lives_container_add(LIVES_CONTAINER(hbuttonbox), reset_button);

  close_button = lives_dialog_add_button_from_stock(NULL, NULL, _("_Close mixer"),
                 LIVES_RESPONSE_OK);

  lives_container_add(LIVES_CONTAINER(hbuttonbox), close_button);

  filler = add_fill_to_box(LIVES_BOX(hbuttonbox));
  lives_widget_apply_theme2(filler, LIVES_WIDGET_STATE_NORMAL, TRUE);

  lives_widget_add_accelerator(close_button, LIVES_WIDGET_CLICKED_SIGNAL, accel_group,
                               LIVES_KEY_m, LIVES_CONTROL_MASK, (LiVESAccelFlags)0);

  lives_window_add_accel_group(LIVES_WINDOW(amixerw), accel_group);

  if (mt->opts.back_audio_tracks > 0) {
    vbox = amixer_add_channel_slider(mt, -1);
    lives_box_pack_start(LIVES_BOX(amixer->main_hbox), vbox, FALSE, FALSE, widget_opts.packing_width);
  }

  vbox2 = lives_vbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(amixer->main_hbox), vbox2, FALSE, FALSE, widget_opts.packing_width);

  add_fill_to_box(LIVES_BOX(vbox2));

  vbox = lives_vbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox2), vbox, TRUE, TRUE, widget_opts.packing_height);

  if (prefs->lamp_buttons) {
    amixer->inv_checkbutton = lives_check_button_new_with_label(" ");
    lives_toggle_button_set_mode(LIVES_TOGGLE_BUTTON(amixer->inv_checkbutton), FALSE);
#if GTK_CHECK_VERSION(3, 0, 0)
    lives_signal_sync_connect(LIVES_GUI_OBJECT(amixer->inv_checkbutton), LIVES_WIDGET_EXPOSE_EVENT,
                              LIVES_GUI_CALLBACK(draw_cool_toggle), NULL);
#endif
    lives_widget_set_bg_color(amixer->inv_checkbutton, LIVES_WIDGET_STATE_ACTIVE, &palette->light_green);
    lives_widget_set_bg_color(amixer->inv_checkbutton, LIVES_WIDGET_STATE_NORMAL, &palette->dark_red);

    lives_signal_sync_connect_after(LIVES_GUI_OBJECT(amixer->inv_checkbutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                                    LIVES_GUI_CALLBACK(lives_cool_toggled), NULL);

    lives_cool_toggled(amixer->inv_checkbutton, NULL);

  } else amixer->inv_checkbutton = lives_check_button_new();

  if (mt->opts.back_audio_tracks > 0 && mt->opts.pertrack_audio) {
    label = lives_standard_label_new_with_mnemonic_widget(_("_Invert backing audio\nand layer volumes"), amixer->inv_checkbutton);

    lives_widget_set_tooltip_text(amixer->inv_checkbutton, _("Adjust backing and layer audio values so that they sum to 1.0"));
    eventbox = lives_event_box_new();
    lives_tooltips_copy(eventbox, amixer->inv_checkbutton);
    lives_label_set_mnemonic_widget(LIVES_LABEL(label), amixer->inv_checkbutton);

    lives_container_add(LIVES_CONTAINER(eventbox), label);
    lives_signal_sync_connect(LIVES_GUI_OBJECT(eventbox), LIVES_WIDGET_BUTTON_PRESS_EVENT,
                              LIVES_GUI_CALLBACK(label_act_toggle), amixer->inv_checkbutton);

    if (palette->style & STYLE_1) {
      lives_widget_apply_theme(eventbox, LIVES_WIDGET_STATE_NORMAL);
    }

    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);
    lives_box_pack_start(LIVES_BOX(hbox), eventbox, FALSE, FALSE, widget_opts.packing_width);
    lives_box_pack_start(LIVES_BOX(hbox), amixer->inv_checkbutton, FALSE, FALSE, 0);
    lives_widget_set_can_focus_and_default(amixer->inv_checkbutton);
  }

  if (prefs->lamp_buttons) {
    amixer->gang_checkbutton = lives_check_button_new_with_label(" ");
    lives_toggle_button_set_mode(LIVES_TOGGLE_BUTTON(amixer->gang_checkbutton), FALSE);
#if GTK_CHECK_VERSION(3, 0, 0)
    lives_signal_sync_connect(LIVES_GUI_OBJECT(amixer->gang_checkbutton), LIVES_WIDGET_EXPOSE_EVENT,
                              LIVES_GUI_CALLBACK(draw_cool_toggle), NULL);
#endif
    lives_widget_set_bg_color(amixer->gang_checkbutton, LIVES_WIDGET_STATE_ACTIVE, &palette->light_green);
    lives_widget_set_bg_color(amixer->gang_checkbutton, LIVES_WIDGET_STATE_NORMAL, &palette->dark_red);
  } else amixer->gang_checkbutton = lives_check_button_new();

  if (mt->opts.pertrack_audio) {
    label = lives_standard_label_new_with_mnemonic_widget(_("_Gang layer audio"), amixer->gang_checkbutton);

    lives_widget_set_tooltip_text(amixer->gang_checkbutton, _("Adjust all layer audio values to the same value"));
    eventbox = lives_event_box_new();
    lives_tooltips_copy(eventbox, amixer->gang_checkbutton);

    lives_container_add(LIVES_CONTAINER(eventbox), label);
    lives_signal_sync_connect(LIVES_GUI_OBJECT(eventbox), LIVES_WIDGET_BUTTON_PRESS_EVENT,
                              LIVES_GUI_CALLBACK(label_act_toggle), amixer->gang_checkbutton);

    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(amixer->gang_checkbutton), mt->opts.gang_audio);

    if (palette->style & STYLE_1) {
      lives_widget_apply_theme(eventbox, LIVES_WIDGET_STATE_NORMAL);
    }

    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_end(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);
    lives_box_pack_start(LIVES_BOX(hbox), eventbox, FALSE, FALSE, widget_opts.packing_width);
    lives_box_pack_start(LIVES_BOX(hbox), amixer->gang_checkbutton, FALSE, FALSE, widget_opts.packing_width);
    lives_widget_set_can_focus_and_default(amixer->gang_checkbutton);
  }

  add_fill_to_box(LIVES_BOX(vbox2));

  for (int i = 0; i < nachans - mt->opts.back_audio_tracks; i++) {
    vbox = amixer_add_channel_slider(mt, i);
    lives_box_pack_start(LIVES_BOX(amixer->main_hbox), vbox, FALSE, FALSE, widget_opts.packing_width);
  }

  lives_signal_sync_connect(LIVES_GUI_OBJECT(close_button), LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_amixer_close_clicked), (livespointer)mt);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(reset_button), LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_amixer_reset_clicked), (livespointer)mt);

  lives_widget_add_accelerator(close_button, LIVES_WIDGET_CLICKED_SIGNAL, accel_group,
                               LIVES_KEY_Escape, (LiVESXModifierType)0, (LiVESAccelFlags)0);

  lives_signal_sync_connect_after(LIVES_GUI_OBJECT(amixer->gang_checkbutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                                  LIVES_GUI_CALLBACK(after_amixer_gang_toggled), (livespointer)amixer);

  lives_signal_sync_connect_after(LIVES_GUI_OBJECT(amixer->gang_checkbutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                                  LIVES_GUI_CALLBACK(lives_cool_toggled), NULL);

  lives_widget_add_accelerator(close_button, LIVES_WIDGET_CLICKED_SIGNAL, accel_group,
                               LIVES_KEY_Escape, (LiVESXModifierType)0, (LiVESAccelFlags)0);

  lives_cool_toggled(amixer->gang_checkbutton, NULL);
  after_amixer_gang_toggled(LIVES_TOGGLE_BUTTON(amixer->gang_checkbutton), amixer);

  on_amixer_reset_clicked(NULL, mt);

  lives_widget_show_all(amixerw);
  lives_widget_grab_focus(amixer->window);
  lives_window_present(LIVES_WINDOW(amixer->window));
  lives_widget_grab_focus(close_button);
}
