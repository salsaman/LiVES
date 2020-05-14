// multitrack.c
// LiVES
// (c) G. Finch 2005 - 2019 <salsaman+lives@gmail.com>
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

// future plans include timeline plugins, which would generate event lists
// or adjust the currently playing one
// and it would be nice to be able to read/write event lists in other formats than the default


// mainw->playarea is reparented:
// (mt->hbox -> preview_frame -> preview_eventbox -> (playarea -> plug -> play_image)
// for gtk+ 3.x the frame_pixbuf is drawn into play_image in expose_pb when not playing

// TODO: see if we can simplify, e.g
// hbox -> preview_eventbox -> play_image
// (removing play_box, playarea and plug)


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
//                                     - play_box
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
//                    - tl_vbox
//                    - message_box

//#define DEBUG_TTABLE

#include "main.h"
#include "events.h"
#include "callbacks.h"
#include "effects.h"
#include "resample.h"
#include "paramwindow.h"
#include "interface.h"
#include "audio.h"
#include "startup.h"
#include "framedraw.h"
#include "cvirtual.h"
#include "pangotext.h"
#include "rte_window.h"

#ifdef ENABLE_GIW
#include "giw/giwvslider.h"
#include "giw/giwled.h"
#endif

#ifdef ENABLE_GIW_3
#include "giw/giwtimeline.h"
#endif

#ifdef HAVE_LDVGRAB
#include "ldvgrab.h"
#endif

#ifndef WEED_AUDIO_LITTLE_ENDIAN
#define WEED_AUDIO_LITTLE_ENDIAN 0
#define WEED_AUDIO_BIG_ENDIAN 1
#endif

static EXPOSE_FN_PROTOTYPE(expose_timeline_reg_event)
static EXPOSE_FN_PROTOTYPE(mt_expose_audtrack_event)

static boolean mt_add_block_effect_idle(livespointer mt);
static boolean mt_add_region_effect_idle(livespointer mt);
static boolean mt_fx_edit_idle(livespointer mt);

static void paint_lines(lives_mt *mt, double currtime, boolean unpaint);

static int *update_layout_map(weed_plant_t *event_list);
static double *update_layout_map_audio(weed_plant_t *event_list);

static boolean check_can_resetp(lives_mt *mt);

/// used to match clips from the event recorder with renumbered clips (without gaps)
static int renumbered_clips[MAX_FILES + 1];

static double lfps[MAX_FILES + 1]; ///< table of layout fps
static void **pchain; ///< param chain for currently being edited filter

static int xachans, xarate, xasamps, xse;
static boolean ptaud;
static int btaud;

static int aofile;
static int afd;

static int dclick_time = 0;

static boolean force_pertrack_audio;
static int force_backing_tracks;

static int clips_to_files[MAX_FILES];

static boolean pb_audio_needs_prerender;
static weed_plant_t *pb_loop_event, *pb_filter_map, *pb_afilter_map;

static boolean nb_ignore = FALSE;

static LiVESWidget *dummy_menuitem;

static boolean doubleclick = FALSE;

static uint32_t last_press_time = 0;

static boolean needs_clear;

static LiVESList *pkg_list;

static LiVESWidget *mainw_message_box;
static LiVESWidget *mainw_msg_area;
static LiVESWidget *mainw_msg_scrollbar;
static LiVESAdjustment *mainw_msg_adj;
static ulong mainw_sw_func;

////////////////////////////

// menuitem callbacks
static void multitrack_adj_start_end(LiVESMenuItem *, livespointer mt);
boolean multitrack_audio_insert(LiVESMenuItem *, livespointer mt);
static void multitrack_view_events(LiVESMenuItem *, livespointer mt);
static void multitrack_view_sel_events(LiVESMenuItem *, livespointer mt);
static void on_prerender_aud_activate(LiVESMenuItem *, livespointer mt);
static void on_jumpnext_activate(LiVESMenuItem *, livespointer mt);
static void on_jumpback_activate(LiVESMenuItem *, livespointer mt);
static void on_jumpnext_mark_activate(LiVESMenuItem *, livespointer mt);
static void on_jumpback_mark_activate(LiVESMenuItem *, livespointer mt);
static void on_delblock_activate(LiVESMenuItem *, livespointer mt);
static void on_seltrack_activate(LiVESMenuItem *, livespointer mt);
static void multitrack_view_details(LiVESMenuItem *, livespointer mt);
static void mt_add_region_effect(LiVESMenuItem *, livespointer mt);
static void mt_add_block_effect(LiVESMenuItem *, livespointer mt);
static void on_clear_event_list_activate(LiVESMenuItem *, livespointer mt);
static void show_frame_events_activate(LiVESMenuItem *, livespointer);
static void mt_load_vals_toggled(LiVESMenuItem *, livespointer mt);
static void mt_load_vals_toggled(LiVESMenuItem *, livespointer mt);
static void mt_render_vid_toggled(LiVESMenuItem *, livespointer mt);
static void mt_render_aud_toggled(LiVESMenuItem *, livespointer mt);
static void mt_norm_aud_toggled(LiVESMenuItem *, livespointer mt);
static void mt_fplay_toggled(LiVESMenuItem *, livespointer mt);
static void mt_change_vals_activate(LiVESMenuItem *, livespointer mt);
static void on_set_pvals_clicked(LiVESWidget *button, livespointer mt);
static void on_move_fx_changed(LiVESMenuItem *, livespointer mt);
static void select_all_time(LiVESMenuItem *, livespointer mt);
static void select_from_zero_time(LiVESMenuItem *, livespointer mt);
static void select_to_end_time(LiVESMenuItem *, livespointer mt);
static void select_all_vid(LiVESMenuItem *, livespointer mt);
static void select_no_vid(LiVESMenuItem *, livespointer mt);
static void on_split_sel_activate(LiVESMenuItem *, livespointer mt);
static void on_split_curr_activate(LiVESMenuItem *, livespointer mt);
static void multitrack_undo(LiVESMenuItem *, livespointer mt);
static void multitrack_redo(LiVESMenuItem *, livespointer mt);
static void on_mt_showkeys_activate(LiVESMenuItem *, livespointer);
static void on_mt_list_fx_activate(LiVESMenuItem *, livespointer mt);
static void on_mt_delfx_activate(LiVESMenuItem *, livespointer mt);
static void on_mt_fx_edit_activate(LiVESMenuItem *, livespointer mt);
static void mt_view_audio_toggled(LiVESMenuItem *, livespointer mt);
static void mt_ign_ins_sel_toggled(LiVESMenuItem *, livespointer mt);
static void mt_change_max_disp_tracks(LiVESMenuItem *, livespointer mt);

static void mt_ac_audio_toggled(LiVESMenuItem *, livespointer mt);

///////////////////////////////////////////////////////////////////

#define LIVES_AVOL_SCALE ((double)1000000.)


LIVES_INLINE int mt_file_from_clip(lives_mt *mt, int clip) {
  return clips_to_files[clip];
}


LIVES_INLINE int mt_clip_from_file(lives_mt *mt, int file) {
  register int i;
  for (i = 0; i < MAX_FILES; i++) {
    if (clips_to_files[i] == file) return i;
  }
  return -1;
}


/// return track number for a given block
int get_track_for_block(track_rect *block) {
  return LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(block->eventbox), "layer_number"));
}


LIVES_INLINE boolean is_empty_track(LiVESWidgetObject *track) {
  return (lives_widget_object_get_data(track, "blocks") == NULL);
}

double get_mixer_track_vol(lives_mt *mt, int trackno) {
  double vol = (double)(LIVES_POINTER_TO_INT(lives_list_nth_data(mt->audio_vols, trackno)));
  return vol / LIVES_AVOL_SCALE;
}


void set_mixer_track_vol(lives_mt *mt, int trackno, double vol) {
  int x = vol * LIVES_AVOL_SCALE;
  lives_list_nth(mt->audio_vols, trackno)->data = LIVES_INT_TO_POINTER(x);
}


boolean save_event_list_inner(lives_mt *mt, int fd, weed_plant_t *event_list, unsigned char **mem) {
  weed_plant_t *event;

  void **ievs = NULL;
  void *next, *prev;

  int64_t *uievs;
  int64_t iev = 0l;

  struct timeval otv;

  char *cversion;
  char *cdate;

  int count = 0;
  int nivs = 0;

  int error;

  register int i;

  if (event_list == NULL) return TRUE;

  gettimeofday(&otv, NULL);
  cdate = lives_datetime(&otv);

  event = get_first_event(event_list);

  threaded_dialog_spin(0.);

  weed_set_int_value(event_list, WEED_LEAF_WIDTH, cfile->hsize);
  weed_set_int_value(event_list, WEED_LEAF_HEIGHT, cfile->vsize);
  weed_set_int_value(event_list, WEED_LEAF_AUDIO_CHANNELS, cfile->achans);
  weed_set_int_value(event_list, WEED_LEAF_AUDIO_RATE, cfile->arate);
  weed_set_int_value(event_list, WEED_LEAF_AUDIO_SAMPLE_SIZE, cfile->asampsize);

  weed_set_int_value(event_list, WEED_LEAF_WEED_EVENT_API_VERSION, WEED_EVENT_API_VERSION);
  weed_set_int_value(event_list, WEED_LEAF_WEED_API_VERSION, WEED_API_VERSION);
  weed_set_int_value(event_list, WEED_LEAF_FILTER_API_VERSION, WEED_FILTER_API_VERSION);

  cversion = lives_strdup_printf("LiVES version %s", LiVES_VERSION);
  if (!weed_plant_has_leaf(event_list, WEED_LEAF_LIVES_CREATED_VERSION)) {
    weed_set_string_value(event_list, WEED_LEAF_LIVES_CREATED_VERSION, cversion);
    weed_set_string_value(event_list, WEED_LEAF_CREATED_DATE, cdate);
  }
  weed_set_string_value(event_list, WEED_LEAF_LIVES_EDITED_VERSION, cversion);
  weed_set_string_value(event_list, WEED_LEAF_EDITED_DATE, cdate);
  lives_free(cversion);
  lives_free(cdate);

  if (cfile->signed_endian & AFORM_UNSIGNED) weed_set_boolean_value(event_list, WEED_LEAF_AUDIO_SIGNED, WEED_FALSE);
  else weed_set_boolean_value(event_list, WEED_LEAF_AUDIO_SIGNED, WEED_TRUE);

  if (cfile->signed_endian & AFORM_BIG_ENDIAN) weed_set_int_value(event_list, WEED_LEAF_AUDIO_ENDIAN, WEED_AUDIO_BIG_ENDIAN);
  else weed_set_int_value(event_list, WEED_LEAF_AUDIO_ENDIAN, WEED_AUDIO_LITTLE_ENDIAN);

  if (mt != NULL && mt->audio_vols != NULL && mt->audio_draws != NULL) {
    int natracks = lives_list_length(mt->audio_draws);

    int *atracks = (int *)lives_malloc(natracks * sizint);
    double *avols;

    int navols;

    for (i = 0; i < natracks; i++) {
      atracks[i] = i - mt->opts.back_audio_tracks;
    }
    weed_set_int_array(event_list, WEED_LEAF_AUDIO_VOLUME_TRACKS, natracks, atracks);
    lives_free(atracks);

    if (mt->opts.gang_audio) navols = 1 + mt->opts.back_audio_tracks;
    else navols = natracks;

    avols = (double *)lives_malloc(navols * sizeof(double));
    for (i = 0; i < navols; i++) {
      avols[i] = get_mixer_track_vol(mt, i);
    }
    weed_set_double_array(event_list, WEED_LEAF_AUDIO_VOLUME_VALUES, navols, avols);
    lives_free(avols);
  }

  if (mt != NULL) {
    int nvtracks = lives_list_length(mt->video_draws);

    int *vtracks = (int *)lives_malloc(nvtracks * sizint);
    char **const labels = (char **)lives_malloc(nvtracks * sizeof(char *));

    for (i = 0; i < nvtracks; i++) {
      LiVESWidget *ebox = get_eventbox_for_track(mt, i);
      const char *tname = (const char *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(ebox), "track_name");
      labels[i] = (char *)tname;
      vtracks[i] = i;
    }

    weed_set_int_array(event_list, WEED_LEAF_TRACK_LABEL_TRACKS, nvtracks, vtracks);
    lives_free(vtracks);

    weed_set_string_array(event_list, WEED_LEAF_TRACK_LABEL_VALUES, nvtracks, labels);

    lives_free(labels);
  }

  if (mem == NULL && fd < 0) return TRUE;

  threaded_dialog_spin(0.);

  mainw->write_failed = FALSE;
  weed_plant_serialise(fd, event_list, mem);

  while (!mainw->write_failed && event != NULL) {
    next = weed_get_voidptr_value(event, WEED_LEAF_NEXT, &error);
    weed_leaf_delete(event, WEED_LEAF_NEXT);

    prev = weed_get_voidptr_value(event, WEED_LEAF_PREVIOUS, &error);
    weed_leaf_delete(event, WEED_LEAF_PREVIOUS);

    if (WEED_EVENT_IS_FILTER_INIT(event)) {
      weed_leaf_delete(event, WEED_LEAF_EVENT_ID);
      weed_set_int64_value(event, WEED_LEAF_EVENT_ID, (int64_t)(uint64_t)((void *)event));
    } else if (WEED_EVENT_IS_FILTER_DEINIT(event) || WEED_EVENT_IS_PARAM_CHANGE(event)) {
      iev = (int64_t)(uint64_t)weed_get_voidptr_value(event, WEED_LEAF_INIT_EVENT, &error);
      weed_leaf_delete(event, WEED_LEAF_INIT_EVENT);
      weed_set_int64_value(event, WEED_LEAF_INIT_EVENT, iev);
    } else if (WEED_EVENT_IS_FILTER_MAP(event)) {
      ievs = weed_get_voidptr_array_counted(event, WEED_LEAF_INIT_EVENTS, &nivs);
      uievs = (int64_t *)lives_malloc(nivs * 8);
      for (i = 0; i < nivs; i++) {
        uievs[i] = (int64_t)(uint64_t)ievs[i];
      }
      weed_leaf_delete(event, WEED_LEAF_INIT_EVENTS);
      weed_set_int64_array(event, WEED_LEAF_INIT_EVENTS, nivs, uievs);
      lives_free(uievs);
    }

    weed_plant_serialise(fd, event, mem);

    if (WEED_EVENT_IS_FILTER_INIT(event)) {
      weed_leaf_delete(event, WEED_LEAF_EVENT_ID);
    }
    if (WEED_EVENT_IS_FILTER_DEINIT(event) || WEED_EVENT_IS_PARAM_CHANGE(event)) {
      weed_leaf_delete(event, WEED_LEAF_INIT_EVENT);
      weed_set_voidptr_value(event, WEED_LEAF_INIT_EVENT, (void *)iev);
    } else if (WEED_EVENT_IS_FILTER_MAP(event)) {
      weed_leaf_delete(event, WEED_LEAF_INIT_EVENTS);
      weed_set_voidptr_array(event, WEED_LEAF_INIT_EVENTS, nivs, ievs);
      lives_free(ievs);
    }

    weed_set_voidptr_value(event, WEED_LEAF_NEXT, next);
    weed_set_voidptr_value(event, WEED_LEAF_PREVIOUS, prev);

    event = get_next_event(event);
    if (++count == 100) {
      count = 0;
      threaded_dialog_spin(0.);
    }
  }
  if (mainw->write_failed) return FALSE;
  return TRUE;
}


LiVESPixbuf *make_thumb(lives_mt *mt, int file, int width, int height, frames_t frame, LiVESInterpType interp,
                        boolean noblanks) {
  LiVESPixbuf *thumbnail = NULL, *pixbuf;
  LiVESError *error = NULL;

  lives_clip_t *sfile = mainw->files[file];

  boolean tried_all = FALSE;
  boolean needs_idlefunc = FALSE;
  boolean did_backup = mt->did_backup;

  int nframe, oframe = frame;

  if (file < 1) {
    LIVES_WARN("Warning - make thumb for file -1");
    return NULL;
  }

  if (width < 4 || height < 4) return NULL;

  if (mt != NULL && mt->idlefunc > 0) {
    needs_idlefunc = TRUE;
    lives_source_remove(mt->idlefunc);
    mt->idlefunc = 0;
  }

  do {
    if (sfile->frames > 0) {
      weed_timecode_t tc = (frame - 1.) / sfile->fps * TICKS_PER_SECOND;
      if (sfile->frames > 0 && sfile->clip_type == CLIP_TYPE_FILE) {
        lives_clip_data_t *cdata = ((lives_decoder_t *)sfile->ext_src)->cdata;
        if (cdata != NULL && !(cdata->seek_flag & LIVES_SEEK_FAST) &&
            is_virtual_frame(file, frame)) {
          virtual_to_images(file, frame, frame, FALSE, NULL);
        }
      }
      thumbnail = pull_lives_pixbuf_at_size(file, frame, get_image_ext_for_type(sfile->img_type), tc,
                                            width, height, LIVES_INTERP_FAST, TRUE);
    } else {
      pixbuf = lives_pixbuf_new_from_stock_at_size(LIVES_LIVES_STOCK_AUDIO, LIVES_ICON_SIZE_CUSTOM, width, height);
      if (error != NULL || pixbuf == NULL) {
        lives_error_free(error);
        if (mt != NULL && (needs_idlefunc || (!did_backup && mt->auto_changed))) {
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

    if (noblanks && thumbnail != NULL && !lives_pixbuf_is_all_black(thumbnail)) noblanks = FALSE;
    if (noblanks) {
      nframe = frame + sfile->frames / 10.;
      if (nframe == frame) nframe++;
      if (nframe > sfile->frames) {
        nframe = oframe;
        tried_all = TRUE;
      }
      frame = nframe;
      if (thumbnail != NULL) lives_widget_object_unref(thumbnail);
      thumbnail = NULL;
    }
  } while (noblanks);

  if (mt != NULL) {
    if (needs_idlefunc || (!did_backup && mt->auto_changed)) {
      mt->idlefunc = mt_idle_add(mt);
    }
  }

  return thumbnail;
}


LiVESPixbuf *make_thumb_fast_between(lives_mt *mt, int fileno, int width, int height, int tframe, int range) {
  int nvframe = -1;
  register int i;

  if (fileno < 1) {
    LIVES_WARN("Warning - make thumb for file -1");
    return NULL;
  }

  if (width < 2 || height < 2) return NULL;

  for (i = 1; i <= range; i++) {
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
    return make_thumb(mt, fileno, width, height, nvframe, LIVES_INTERP_FAST, FALSE);
  }

  return NULL;
}


static void mt_set_cursor_style(lives_mt *mt, lives_cursor_t cstyle, int width, int height, int clip, int hsx, int hsy) {
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

  register int i, j, k;

  disp = lives_widget_get_display(LIVES_MAIN_WINDOW_WIDGET);

#ifdef GUI_GTK
  gdk_display_get_maximal_cursor_size(disp, &cwidth, &cheight);
#endif
#ifdef GUI_QT
  cwidth = MAX_CURSOR_WIDTH;
#endif

  if (width > cwidth) width = cwidth;

  mt->cursor_style = cstyle;
  switch (cstyle) {
  case LIVES_CURSOR_BLOCK:
    if (sfile != NULL && sfile->frames > 0) {
      frame_start = mt->opts.ign_ins_sel ? 1 : sfile->start;
      frames_width = (double)(mt->opts.ign_ins_sel ? sfile->frames : sfile->end - sfile->start + 1.);

      pixbuf = lives_pixbuf_new(TRUE, width, height);

      for (i = 0; i < width; i += BLOCK_THUMB_WIDTH) {
        // create a small thumb
        twidth = BLOCK_THUMB_WIDTH;
        if ((i + twidth) > width) twidth = width - i;
        if (twidth >= 4) {
          thumbnail = make_thumb(mt, clip, twidth, height, frame_start + (int)((double)i / (double)width * frames_width),
                                 LIVES_INTERP_NORMAL, FALSE);
          // render it in the cursor
          if (thumbnail != NULL) {
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
              }
            }
            lives_widget_object_unref(thumbnail);
          }
        }
      }
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

  if (pixbuf != NULL) lives_widget_object_unref(pixbuf);
  if (cursor != NULL) lives_cursor_unref(cursor);
}


boolean write_backup_layout_numbering(lives_mt *mt) {
  // link clip numbers in the auto save event_list to actual clip numbers
  lives_clip_t *sfile;
  double vald;
  int fd, i, vali, hdlsize;
  char *asave_file = lives_strdup_printf("%s/%s.%d.%d.%d", prefs->workdir, LAYOUT_NUMBERING_FILENAME, lives_getuid(),
                                         lives_getgid(),
                                         capable->mainpid);
  LiVESList *clist = mainw->cliplist;

  fd = lives_create_buffered(asave_file, DEF_FILE_PERMS);
  lives_free(asave_file);

  mainw->write_failed = FALSE;

  if (fd != -1) {
    for (clist = mainw->cliplist; !mainw->write_failed && clist != NULL; clist = clist->next) {
      i = LIVES_POINTER_TO_INT(clist->data);
      if (i < 1 || !IS_NORMAL_CLIP(i)) continue;
      sfile = mainw->files[i];
      if (mt != NULL) vali = i;
      else vali = sfile->stored_layout_idx;
      lives_write_le_buffered(fd, &vali, 4, TRUE);
      vald = sfile->fps;
      lives_write_le_buffered(fd, &vald, 8, TRUE);
      hdlsize = strlen(sfile->handle);
      lives_write_le_buffered(fd, &hdlsize, 4, TRUE);
      lives_write_buffered(fd, sfile->handle, hdlsize, TRUE);
    }
    lives_close_buffered(fd);
  }

  if (mainw->write_failed) return FALSE;
  return TRUE;
}


static void upd_layout_maps(weed_plant_t *event_list) {
  int *layout_map;
  double *layout_map_audio;

  // update layout maps for files from global layout map
  layout_map = update_layout_map(event_list);
  layout_map_audio = update_layout_map_audio(event_list);

  save_layout_map(layout_map, layout_map_audio, NULL, NULL);

  lives_freep((void **)&layout_map);
  lives_freep((void **)&layout_map_audio);
}


static void renumber_from_backup_layout_numbering(lives_mt *mt) {
  // this is used only for crash recovery

  // layout_numbering simply maps our clip handle to clip numbers in the current layout
  // (it would have been better to use the unique_id, but for backwards compatibility that is not possible)
  // we assume the order hasnt changed (it cant), but there may be gaps in the numbering
  // the numbering may have changed (for example we started last time in mt mode, this time in ce mode)

  double vard;
  char buf[512];
  char *aload_file;
  boolean gotvalid = FALSE;
  int fd, vari, clipn, offs, restart = 0;

  if (mt != NULL) {
    aload_file = lives_strdup_printf("%s/%s.%d.%d.%d", prefs->workdir, LAYOUT_NUMBERING_FILENAME,
                                     lives_getuid(), lives_getgid(),
                                     capable->mainpid);
    // ensure file layouts are updated
    upd_layout_maps(NULL);
  } else {
    aload_file = lives_strdup_printf("%s/recorded-%s.%d.%d.%d", prefs->workdir, LAYOUT_NUMBERING_FILENAME,
                                     lives_getuid(), lives_getgid(),
                                     capable->mainpid);
  }

  fd = lives_open_buffered_rdonly(aload_file);
  if (fd != -1) {
    while (1) {
      offs = restart;
      if (lives_read_le_buffered(fd, &clipn, 4, TRUE) < 4) break;
      if (lives_read_le_buffered(fd, &vard, 8, TRUE) < 8) break;
      if (lives_read_le_buffered(fd, &vari, 4, TRUE) < 4) break;
      if (vari > 511) vari = 511;
      if (lives_read_buffered(fd, buf, vari, TRUE) < vari) break;
      lives_memset(buf + vari, 0, 1);
      if (clipn < 0 || clipn > MAX_FILES) continue;
      gotvalid = FALSE;
      while (++offs < MAX_FILES) {
        if (!IS_VALID_CLIP(offs)) {
          if (!gotvalid) restart = offs;
          continue;
        }
        gotvalid = TRUE;

        // dont increase restart, in case we cant match this clip
        if (strcmp(mainw->files[offs]->handle, buf)) continue;

        // got a match - index the current clip order -> clip order in layout
        renumbered_clips[clipn] = offs;
        // lfps contains the fps at the time of the crash
        lfps[offs] = vard;
        restart = offs;
        break;
      }
    }
    lives_close_buffered(fd);
  }
  lives_free(aload_file);
}


static void save_mt_autoback(lives_mt *mt) {
  // auto backup of the current layout

  // this is called from an idle funtion - if the specified amount of time has passed and
  // the clip has been altered

  struct timeval otv;

  char *asave_file = lives_strdup_printf("%s/%s.%d.%d.%d.%s", prefs->workdir, LAYOUT_FILENAME, lives_getuid(), lives_getgid(),
                                         capable->mainpid, LIVES_FILE_EXT_LAYOUT);
  char *tmp;

  boolean retval = TRUE;
  int retval2;
  int fd;

  mt->auto_changed = FALSE;
  lives_widget_set_sensitive(mt->backup, FALSE);
  mt_desensitise(mt);

  // flush any pending events
  lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET, TRUE);

  do {
    retval2 = 0;
    mainw->write_failed = FALSE;

    fd = lives_create_buffered(asave_file, DEF_FILE_PERMS);
    if (fd >= 0) {
      add_markers(mt, mt->event_list, FALSE);
      do_threaded_dialog(_("Auto backup"), FALSE);

      set_signal_handlers((SignalHandlerPointer)defer_sigint);

      retval = save_event_list_inner(mt, fd, mt->event_list, NULL);
      if (retval) retval = write_backup_layout_numbering(mt);

      if (mainw->signal_caught) catch_sigint(mainw->signal_caught);

      set_signal_handlers((SignalHandlerPointer)catch_sigint);

      end_threaded_dialog();
      paint_lines(mt, mt->ptr_time, FALSE);

      remove_markers(mt->event_list);
      lives_close_buffered(fd);
    } else mainw->write_failed = TRUE;

    mt_sensitise(mt);

    if (!retval || mainw->write_failed) {
      mainw->write_failed = FALSE;
      retval2 = do_write_failed_error_s_with_retry(asave_file, NULL, NULL);
    }
  } while (retval2 == LIVES_RESPONSE_RETRY);

  lives_free(asave_file);

  mt->auto_back_time = lives_get_current_ticks();

  gettimeofday(&otv, NULL);
  tmp = lives_datetime(&otv);
  d_print("Auto backup of timeline at %s\n", tmp);
  lives_free(tmp);
}


static void on_mt_backup_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  if (!mt->auto_changed) return;
  if (mt->idlefunc > 0) {
    lives_source_remove(mt->idlefunc);
    mt->idlefunc = 0;
  }
  save_mt_autoback(mt);
}


boolean mt_auto_backup(livespointer user_data) {
  ticks_t stime, diff;

  lives_mt *mt = (lives_mt *)user_data;

  if (mainw->multitrack == NULL) return FALSE;

  if (prefs->mt_auto_back == 0) mt->auto_changed = TRUE;

  if (!mt->auto_changed && mt->did_backup) {
    LIVES_WARN("Error in mt backup");
    return TRUE;
  }

  mt->idlefunc = 0;

  if (!mt->auto_changed || mt->event_list == NULL || prefs->mt_auto_back < 0) {
    return FALSE;
  }

  stime = lives_get_current_ticks();
  if (mt->auto_back_time == 0) mt->auto_back_time = stime;

  diff = stime - mt->auto_back_time;
  if (diff >= prefs->mt_auto_back * TICKS_PER_SECOND) {
    // time to back up the event_list
    // resets mt->auto_changed
    save_mt_autoback(mt);
    return FALSE;
  }

  // re-add the idlefunc (in case we came here via a timer)
  mt->idlefunc = mt_idle_add(mt);
  return FALSE;
}


uint32_t mt_idle_add(lives_mt *mt) {
  uint32_t retval;

  if (LIVES_IS_PLAYING || mt->is_rendering) return 0;

  if (prefs->mt_auto_back <= 0) return 0;

  if (mt->idlefunc > 0) return mt->idlefunc;

  set_signal_handlers((SignalHandlerPointer)defer_sigint);

  // TODO: last param is a destroy notify, so we can check if something re-adds it or removes it when it shouldnt
  retval = lives_timer_add(1001, mt_auto_backup, mt);

  if (mainw->signal_caught) catch_sigint(mainw->signal_caught);

  set_signal_handlers((SignalHandlerPointer)catch_sigint);

  return retval;
}


void recover_layout_cancelled(boolean is_startup) {
  char *eload_file = lives_strdup_printf("%s/%s.%d.%d.%d.%s", prefs->workdir, LAYOUT_FILENAME, lives_getuid(), lives_getgid(),
                                         capable->mainpid, LIVES_FILE_EXT_LAYOUT);

  if (is_startup) mainw->recoverable_layout = FALSE;

  lives_rm(eload_file);
  lives_free(eload_file);

  eload_file = lives_strdup_printf("%s/%s.%d.%d.%d", prefs->workdir, LAYOUT_NUMBERING_FILENAME, lives_getuid(), lives_getgid(),
                                   capable->mainpid);
  lives_rm(eload_file);
  lives_free(eload_file);

  if (is_startup) do_after_crash_warning();
}


boolean mt_load_recovery_layout(lives_mt *mt) {
  boolean recovered = TRUE;
  char *aload_file = NULL, *eload_file;

  if (mt != NULL) {
    aload_file = lives_strdup_printf("%s/%s.%d.%d.%d", prefs->workdir, LAYOUT_NUMBERING_FILENAME, lives_getuid(), lives_getgid(),
                                     capable->mainpid);
    eload_file = lives_strdup_printf("%s/%s.%d.%d.%d.%s", prefs->workdir, LAYOUT_FILENAME, lives_getuid(), lives_getgid(),
                                     capable->mainpid, LIVES_FILE_EXT_LAYOUT);
    mt->auto_reloading = TRUE;
    mt->event_list = mainw->event_list = load_event_list(mt, eload_file);
    mt->auto_reloading = FALSE;
    if (mt->event_list != NULL) {
      lives_rm(eload_file);
      lives_rm(aload_file);
      mt_init_tracks(mt, TRUE);
      remove_markers(mt->event_list);
      save_mt_autoback(mt);
    }
  } else {
    // recover recording
    int fd;
    aload_file = lives_strdup_printf("%s/recorded-%s.%d.%d.%d", prefs->workdir, LAYOUT_NUMBERING_FILENAME, lives_getuid(),
                                     lives_getgid(),
                                     capable->mainpid);
    eload_file = lives_strdup_printf("%s/recorded-%s.%d.%d.%d.%s", prefs->workdir, LAYOUT_FILENAME, lives_getuid(), lives_getgid(),
                                     capable->mainpid, LIVES_FILE_EXT_LAYOUT);

    if ((fd = lives_open_buffered_rdonly(eload_file)) < 0) {
      lives_close_buffered(fd);
    } else {
      mainw->event_list = load_event_list(NULL, eload_file);
      if (mainw->event_list != NULL) {
        lives_rm(eload_file);
        lives_rm(aload_file);
      }
    }
  }

  if (mainw->event_list == NULL) {
    // failed to load
    // keep the faulty layout for forensic purposes
    char *uldir = lives_build_filename(prefs->workdir, "unrecoverable_layouts", LIVES_DIR_SEP, NULL);
    lives_mkdir_with_parents(uldir, capable->umask);
    if (lives_file_test(eload_file, LIVES_FILE_TEST_EXISTS)) {
      lives_mv(eload_file, uldir);
    }
    if (lives_file_test(aload_file, LIVES_FILE_TEST_EXISTS)) {
      lives_mv(aload_file, uldir);
    }
    if (!capable->has_gzip) capable->has_gzip = has_executable(EXEC_GZIP);
    if (capable->has_gzip) {
      char *cwd = lives_get_current_dir();
      mainw->chdir_failed = FALSE;
      lives_chdir(uldir, TRUE);
      if (mainw->chdir_failed) mainw->chdir_failed = FALSE;
      else {
        char *com = lives_strdup_printf("%s * 2>%s", EXEC_GZIP, LIVES_DEVNULL);
        lives_system(com, TRUE);
        lives_free(com);
        lives_chdir(cwd, FALSE);
      }
      lives_free(cwd);
    }

    if (mt != NULL) {
      mt->fps = prefs->mt_def_fps;
    }
    lives_free(uldir);
    recovered = FALSE;
  }

  lives_free(eload_file);
  lives_freep((void **)&aload_file);
  return recovered;
}


boolean recover_layout(void) {
  boolean loaded = TRUE;

  if (prefs->startup_interface == STARTUP_CE) {
    if (!on_multitrack_activate(NULL, NULL)) {
      loaded = FALSE;
      multitrack_delete(mainw->multitrack, FALSE);
      do_bad_layout_error();
    }
  } else {
    mainw->multitrack->auto_reloading = TRUE;
    set_string_pref(PREF_AR_LAYOUT, ""); // in case we crash...
    loaded = mt_load_recovery_layout(mainw->multitrack);
    mainw->multitrack->auto_reloading = FALSE;
    mt_sensitise(mainw->multitrack);
    set_mt_play_sizes(mainw->multitrack, cfile->hsize, cfile->vsize, TRUE);
  }
  mainw->recoverable_layout = FALSE;
  do_after_crash_warning();
  return loaded;
}


void **mt_get_pchain(void) {
  return pchain;
}


char *get_track_name(lives_mt *mt, int track_num, boolean is_audio) {
  // not const return because of translation issues
  LiVESWidget *xeventbox;
  if (track_num < 0) return lives_strdup(_("Backing audio"));
  if (!is_audio) xeventbox = (LiVESWidget *)lives_list_nth_data(mt->video_draws, track_num);
  else xeventbox = (LiVESWidget *)lives_list_nth_data(mt->audio_draws, track_num + mt->opts.back_audio_tracks);
  return lives_strdup((char *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(xeventbox), "track_name"));
}


LIVES_INLINE double get_time_from_x(lives_mt *mt, int x) {
  double time = (double)x / (double)(lives_widget_get_allocation_width(mt->timeline) - 1) *
                (mt->tl_max - mt->tl_min) + mt->tl_min;
  if (time < 0.) time = 0.;
  else if (time > mt->end_secs + 1. / mt->fps) time = mt->end_secs + 1. / mt->fps;
  return q_dbl(time, mt->fps) / TICKS_PER_SECOND_DBL;
}


LIVES_INLINE void set_params_unchanged(lives_mt *mt, lives_rfx_t *rfx) {
  weed_plant_t *wparam;
  weed_plant_t *inst = rfx->source;
  int i, j;

  for (i = 0; i < rfx->num_params; i++) {
    rfx->params[i].changed = FALSE;
    if ((wparam = weed_inst_in_param(inst, i, FALSE, FALSE)) != NULL) {
      if (is_perchannel_multiw(wparam)) {
        if (weed_plant_has_leaf(wparam, WEED_LEAF_IGNORE)) {
          int num_vals;
          int *ign = weed_get_boolean_array_counted(wparam, WEED_LEAF_IGNORE, &num_vals);
          for (j = 0; j < num_vals; j++) {
            if (ign[j] == WEED_FALSE) ign[j] = WEED_TRUE;
          }
          weed_set_boolean_array(wparam, WEED_LEAF_IGNORE, num_vals, ign);
          lives_free(ign);
	  // *INDENT-OFF*
        }}}}
  // *INDENT-ON*

  lives_widget_set_sensitive(mt->apply_fx_button, FALSE);
  lives_widget_set_sensitive(mt->resetp_button, check_can_resetp(mt));
}


static int get_track_height(lives_mt * mt) {
  LiVESWidget *eventbox;
  LiVESList *list = mt->video_draws;

  while (list != NULL) {
    eventbox = (LiVESWidget *)list->data;
    if (LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "hidden")) == 0)
      return lives_widget_get_allocation_height(eventbox);
    list = list->next;
  }

  return 0;
}


static boolean is_audio_eventbox(LiVESWidget * ebox) {
  return LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(ebox), "is_audio"));
}


static void draw_block(lives_mt * mt, lives_painter_t *cairo,
                       lives_painter_surface_t *surf, track_rect * block, int x1, int x2) {
  // x1 is start point of drawing area (in pixels), x2 is width of drawing area (in pixels)
  lives_painter_t *cr;

  weed_plant_t *event = block->start_event;

  weed_timecode_t tc = get_event_timecode(event);

  LiVESWidget *eventbox = block->eventbox;

  LiVESPixbuf *thumbnail = NULL;

  double tl_span = mt->tl_max - mt->tl_min;
  double offset_startd = tc / TICKS_PER_SECOND_DBL;
  double offset_endd;

  boolean needs_text = TRUE;
  boolean is_audio = FALSE;

  int offset_start;
  int offset_end;
  int filenum, track;

  int framenum, last_framenum;
  int width = BLOCK_THUMB_WIDTH;

  int hidden = (int)LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "hidden"));

  register int i;

  if (mt->no_expose) return;

  if (hidden) return;

  // block to right of screen
  if (offset_startd >= mt->tl_max) return;

  // block to right of drawing area
  offset_start = (int)((offset_startd - mt->tl_min) / tl_span * lives_widget_get_allocation_width(eventbox) + .5);
  if ((x1 > 0 || x2 > 0) && offset_start > (x1 + x2)) return;

  offset_endd = get_event_timecode(block->end_event) / TICKS_PER_SECOND_DBL + (!is_audio_eventbox(eventbox)) / cfile->fps;
  offset_end = (offset_endd - mt->tl_min) / tl_span * lives_widget_get_allocation_width(eventbox);

  //if (offset_end+offset_start>eventbox->allocation.width) offset_end=eventbox->allocation.width-offset_start;

  // end of block before drawing area
  if (offset_end < x1) return;

  if (surf == NULL) cr = cairo;
  else cr = lives_painter_create_from_surface(surf);

  if (cr == NULL) return;

  lives_painter_set_line_width(cr, 1.);

  track = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "layer_number"));
  is_audio = is_audio_eventbox(eventbox);
  if (track < 0) is_audio = TRUE;

  if (!is_audio) filenum = get_frame_event_clip(block->start_event, track);
  else filenum = get_audio_frame_clip(block->start_event, track);

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
        last_framenum = -1;
        for (i = offset_start; i < offset_end; i += BLOCK_THUMB_WIDTH) {
          if (i > x2 - x1) break;
          tc += tl_span / lives_widget_get_allocation_width(eventbox) * width * TICKS_PER_SECOND_DBL;
          if (i + BLOCK_THUMB_WIDTH < x1) continue;
          event = get_frame_event_at(mt->event_list, tc, event, FALSE);
          if (i + width >= 0) {
            // create a small thumb
            framenum = get_frame_event_frame(event, track);
            if (framenum < 1 || framenum > mainw->files[filenum]->frames) continue;
            if (thumbnail != NULL) lives_widget_object_unref(thumbnail);
            thumbnail = NULL;

            if (IS_VALID_CLIP(filenum) && filenum != mainw->scrap_file && framenum != last_framenum) {
              if (mainw->files[filenum]->frames > 0 && mainw->files[filenum]->clip_type == CLIP_TYPE_FILE) {
                lives_clip_data_t *cdata = ((lives_decoder_t *)mainw->files[filenum]->ext_src)->cdata;
                if (cdata != NULL && !(cdata->seek_flag & LIVES_SEEK_FAST) &&
                    is_virtual_frame(filenum, framenum)) {
                  thumbnail = make_thumb_fast_between(mt, filenum, width,
                                                      lives_widget_get_allocation_height(eventbox),
                                                      framenum, last_framenum == -1 ? 0 : framenum - last_framenum);
                } else {
                  thumbnail = make_thumb(mt, filenum, width,
                                         lives_widget_get_allocation_height(eventbox),
                                         framenum, LIVES_INTERP_FAST, FALSE);
                }
              } else {
                thumbnail = make_thumb(mt, filenum, width,
                                       lives_widget_get_allocation_height(eventbox),
                                       framenum, LIVES_INTERP_FAST, FALSE);
              }
            }
            last_framenum = framenum;
            // render it in the eventbox
            if (thumbnail != NULL) {
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
              unpaint_lines(mt);
              mt->no_expose = FALSE;
            }
            mt->redraw_block = TRUE; // stop drawing cursor during playback
            if (LIVES_IS_PLAYING && mainw->cancelled == CANCEL_NONE) {
              // expose is not re-entrant
              mt->no_expose = TRUE;
              process_one(FALSE);
              mt->no_expose = FALSE;
            }
            mt->redraw_block = FALSE;
          }
        }
        if (thumbnail != NULL) lives_widget_object_unref(thumbnail);
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
        lives_colRGBA64_t col_white, col_black;
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

        layout = render_text_to_cr(NULL, cr, fname, sfont, 10.,
                                   LIVES_TEXT_MODE_FOREGROUND_ONLY, &col_white, &col_white, FALSE, FALSE, &top, &text_start,
                                   text_end - text_start, &height);

        lingo_painter_show_layout(cr, layout);

        if (layout) lives_widget_object_unref(layout);

        lives_free(fname);

        lives_painter_fill(cr);
      }

      if (LIVES_IS_PLAYING) {
        mt->no_expose = TRUE; // expose is not re-entrant due to bgimg refs.
        unpaint_lines(mt);
        mt->no_expose = FALSE;
      }
      mt->redraw_block = TRUE; // stop drawing cursor during playback
      if (LIVES_IS_PLAYING && mainw->cancelled == CANCEL_NONE) {
        mt->no_expose = TRUE;
        process_one(FALSE);
        mt->no_expose = FALSE;
      }
      mt->redraw_block = FALSE;
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

  if (surf != NULL) lives_painter_destroy(cr);

}


static void draw_aparams(lives_mt * mt, LiVESWidget * eventbox, lives_painter_t *cr, LiVESList * param_list,
                         weed_plant_t *init_event,
                         int startx, int width) {
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
  int i, error, pnum;
  int hint;

  int offset_start, offset_end, startpos;
  int track;

  char *fhash;

  void **pchainx = NULL;

  fhash = weed_get_string_value(init_event, WEED_LEAF_FILTER, &error);

  if (fhash == NULL) {
    return;
  }

  filter = get_weed_filter(weed_get_idx_for_hashname(fhash, TRUE));
  lives_free(fhash);

  inst = weed_instance_from_filter(filter);
  in_params = weed_get_plantptr_array(inst, WEED_LEAF_IN_PARAMETERS, &error);

  deinit_event = weed_get_plantptr_value(init_event, WEED_LEAF_DEINIT_EVENT, &error);

  start_tc = get_event_timecode(init_event);
  end_tc = get_event_timecode(deinit_event);

  offset_start = (int)((start_tc / TICKS_PER_SECOND_DBL - mt->tl_min) / tl_span * lives_widget_get_allocation_width(
                         eventbox) + .5);
  offset_end = (int)((end_tc / TICKS_PER_SECOND_DBL - mt->tl_min + 1. / mt->fps) / tl_span * lives_widget_get_allocation_width(
                       eventbox) + .5);

  if (offset_end < 0 || offset_start > lives_widget_get_allocation_width(eventbox)) {
    lives_free(in_params);
    weed_instance_unref(inst);
    return;
  }

  if (offset_start > startx) startpos = offset_start;
  else startpos = startx;

  track = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox),
                               "layer_number")) + mt->opts.back_audio_tracks;

  lives_painter_set_line_width(cr, 1.);
  lives_painter_set_source_rgb_from_lives_widget_color(cr, &palette->black);

  pchainx = weed_get_voidptr_array(init_event, WEED_LEAF_IN_PARAMETERS, &error);

  //lives_painter_set_operator (cr, LIVES_PAINTER_OPERATOR_DEST_OVER);
  for (i = startpos; i < startx + width; i++) {
    dtime = get_time_from_x(mt, i);
    tc = dtime * TICKS_PER_SECOND_DBL;
    if (tc >= end_tc) break;

    if (pchainx != NULL) interpolate_params(inst, pchainx, tc);

    plist = param_list;
    while (plist != NULL) {
      pnum = LIVES_POINTER_TO_INT(plist->data);
      param = in_params[pnum];
      ptmpl = weed_get_plantptr_value(param, WEED_LEAF_TEMPLATE, &error);
      hint = weed_get_int_value(ptmpl, WEED_LEAF_HINT, &error);
      switch (hint) {
      case WEED_HINT_INTEGER:
        valis = weed_get_int_array(param, WEED_LEAF_VALUE, &error);
        if (is_perchannel_multiw(in_params[pnum])) vali = valis[track];
        else vali = valis[0];
        mini = weed_get_int_value(ptmpl, WEED_LEAF_MIN, &error);
        maxi = weed_get_int_value(ptmpl, WEED_LEAF_MAX, &error);
        ratio = (double)(vali - mini) / (double)(maxi - mini);
        lives_free(valis);
        break;
      case WEED_HINT_FLOAT:
        valds = weed_get_double_array(param, WEED_LEAF_VALUE, &error);
        if (is_perchannel_multiw(in_params[pnum])) vald = valds[track];
        else vald = valds[0];
        mind = weed_get_double_value(ptmpl, WEED_LEAF_MIN, &error);
        maxd = weed_get_double_value(ptmpl, WEED_LEAF_MAX, &error);
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


static void redraw_eventbox(lives_mt * mt, LiVESWidget * eventbox) {
  if (!LIVES_IS_WIDGET_OBJECT(eventbox)) return;

  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(eventbox), "drawn", LIVES_INT_TO_POINTER(FALSE));
  lives_widget_queue_draw(eventbox);  // redraw the track

  if (is_audio_eventbox(eventbox)) {
    // handle expanded audio
    LiVESWidget *xeventbox;
    if (cfile->achans > 0) {
      xeventbox = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "achan0");
      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(xeventbox), "drawn", LIVES_INT_TO_POINTER(FALSE));
      lives_widget_queue_draw(xeventbox);  // redraw the track
      if (cfile->achans > 1) {
        xeventbox = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "achan1");
        lives_widget_object_set_data(LIVES_WIDGET_OBJECT(xeventbox), "drawn", LIVES_INT_TO_POINTER(FALSE));
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

  if (event != NULL) {
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

  hidden = (int)LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "hidden"));
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

draw1:

  if (cairo == NULL) cr = lives_painter_create_from_widget(eventbox);
  else cr = cairo;

  if (LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "drawn"))) {
    if (bgimage != NULL && lives_painter_image_surface_get_width(bgimage) > 0) {
      lives_painter_set_source_surface(cr, bgimage, startx, starty);
      lives_painter_rectangle(cr, startx, starty, width, height);
      lives_painter_fill(cr);

      if (mt->block_selected != NULL && mt->block_selected->eventbox == eventbox) {
        draw_block(mt, cr, NULL, mt->block_selected, -1, -1);
      }

      if (is_audio_eventbox(eventbox) && mt->avol_init_event != NULL && mt->opts.aparam_view_list != NULL)
        draw_aparams(mt, eventbox, cr, mt->opts.aparam_view_list, mt->avol_init_event, startx, width);

      if (idlefunc > 0) {
        mt->idlefunc = mt_idle_add(mt);
      }
      if (cairo == NULL) lives_painter_destroy(cr);

      return TRUE;
    }
  }

  width = lives_widget_get_allocation_width(eventbox);
  height = lives_widget_get_allocation_height(eventbox);
  if (cairo == NULL) lives_painter_destroy(cr);

  if (bgimage != NULL) lives_painter_surface_destroy(bgimage);

  bgimage = lives_painter_image_surface_create(LIVES_PAINTER_FORMAT_ARGB32,
            width,
            height);

  if (bgimage != NULL && lives_painter_image_surface_get_width(bgimage) > 0) {
    lives_set_cursor_style(LIVES_CURSOR_BUSY, NULL);

    if (palette->style & STYLE_1) {
      lives_painter_t *crx = lives_painter_create_from_surface(bgimage);
      lives_painter_set_source_rgb_from_lives_rgba(crx, &palette->mt_evbox);
      lives_painter_rectangle(crx, 0., 0., width, height);
      lives_painter_fill(crx);
      lives_painter_paint(crx);
      lives_painter_destroy(crx);
    }

    if (mt->block_selected != NULL) {
      sblock = mt->block_selected;
      sblock->state = BLOCK_UNSELECTED;
    }

    block = (track_rect *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "blocks");

    while (block != NULL) {
      draw_block(mt, NULL, bgimage, block, startx, width);
      block = block->next;
      mt->redraw_block = TRUE; // stop drawing cursor during playback
      if (LIVES_IS_PLAYING && mainw->cancelled == CANCEL_NONE) {
        mt->no_expose = TRUE;
        process_one(FALSE);
        mt->no_expose = FALSE;
      }
      mt->redraw_block = FALSE;
    }

    if (sblock != NULL) {
      mt->block_selected = sblock;
      sblock->state = BLOCK_SELECTED;
    }

    lives_set_cursor_style(LIVES_CURSOR_NORMAL, NULL);
  } else if (bgimage != NULL) {
    lives_painter_surface_destroy(bgimage);
    bgimage = NULL;
  }

  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(eventbox), "bgimg", (livespointer)bgimage);
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(eventbox), "drawn", LIVES_INT_TO_POINTER(bgimage != NULL));

  if (bgimage != NULL && lives_painter_image_surface_get_width(bgimage) > 0) goto draw1;

  if (idlefunc > 0) {
    mt->idlefunc = mt_idle_add(mt);
  }

  return TRUE;
}
EXPOSE_FN_END


static char *mt_params_label(lives_mt * mt) {
  char *fname = weed_filter_idx_get_name(mt->current_fx, FALSE, FALSE, FALSE);
  char *layer_name;
  char *ltext;

  if (has_perchannel_multiw(get_weed_filter(mt->current_fx))) {
    layer_name = get_track_name(mt, mt->current_track, mt->aud_track_selected);
    ltext = lives_strdup_printf(_("%s : parameters for %s"), fname, layer_name);
    lives_free(layer_name);
  } else ltext = lives_strdup(fname);
  lives_free(fname);

  if (mt->framedraw != NULL) {
    char *tmp = lives_strdup_printf("%s\n%s",  ltext,
                                    _("<--- Some parameters can be altered by clicking / dragging in the Preview window"));
    lives_free(ltext);
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

  if (mt->fx_box != NULL) {
    lives_widget_destroy(mt->fx_box);
  }

  mt->fx_box = lives_vbox_new(FALSE, 0);

  if (mt->fx_params_label != NULL) {
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
  lives_label_set_text(LIVES_LABEL(mt->fx_params_label), ltext);
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


static track_rect *get_block_from_time(LiVESWidget * eventbox, double time, lives_mt * mt) {
  // return block (track_rect) at seconds time in eventbox
  weed_timecode_t tc = time * TICKS_PER_SECOND;
  track_rect *block;

  block = (track_rect *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "blocks");
  tc = q_gint64(tc, mt->fps);

  while (block != NULL) {
    if (get_event_timecode(block->start_event) > tc) return NULL;
    if (q_gint64(get_event_timecode(block->end_event) + (!is_audio_eventbox(eventbox))*TICKS_PER_SECOND_DBL / mt->fps,
                 mt->fps) > tc) break;
    block = block->next;
  }
  return block;
}


static int track_to_channel(weed_plant_t *ievent, int track) {
  // given an init_event and a track, we check to see which (if any) channel the track is mapped to

  // if track is not mapped, we return -1

  // note that a track could be mapped to multiple channels; we return only the first instance we find

  int *in_tracks;
  int *carray = NULL;
  int nc = 0, rpts, ntracks;

  register int i;

  if (ntracks == 0) return -1;

  in_tracks = weed_get_int_array_counted(ievent, WEED_LEAF_IN_TRACKS, &ntracks);
  carray = weed_get_int_array_counted(ievent, WEED_LEAF_IN_COUNT, &nc);

  for (i = 0; i < ntracks; i++) {
    rpts = nc < i ? 1 : carray[i];
    if (in_tracks[i] + rpts > track) {
      lives_free(in_tracks);
      lives_freep((void **)&carray);
      return i;
    }
    track -= rpts - 1;
  }
  lives_free(in_tracks);
  lives_freep((void **)&carray);
  return -1;
}


static boolean get_track_index(lives_mt * mt, weed_timecode_t tc) {
  // set mt->track_index to the in_channel index of mt->current_track in WEED_LEAF_IN_TRACKS in mt->init_event
  // set -1 if there is no frame for that in_channel, or if mt->current_track lies outside the WEED_LEAF_IN_TRACKS of mt->init_event

  // return TRUE if mt->fx_box is redrawn

  int *clips, *in_tracks, numtracks;
  weed_plant_t *event = get_frame_event_at(mt->event_list, tc, NULL, TRUE);

  boolean retval = FALSE;

  int num_in_tracks;
  int opwidth, opheight;

  int track_index = mt->track_index;

  int chindx;

  register int i;

  mt->track_index = -1;

  if (event == NULL || mt->play_width == 0 || mt->play_height == 0) return retval;

  opwidth = cfile->hsize;
  opheight = cfile->vsize;
  calc_maxspect(mt->play_width, mt->play_height, &opwidth, &opheight);

  clips = weed_get_int_array_counted(event, WEED_LEAF_CLIPS, &numtracks);

  chindx = track_to_channel(mt->init_event, mt->current_track);

  if (mt->current_track < numtracks && clips[mt->current_track] < 1 &&
      (mt->current_rfx == NULL || mt->init_event == NULL || mt->current_rfx->source == NULL || chindx == -1 ||
       !is_audio_channel_in((weed_plant_t *)mt->current_rfx->source, chindx))) {
    if (track_index != -1 && mt->fx_box != NULL) {
      add_mt_param_box(mt);
      retval = TRUE;
    }
    lives_free(clips);
    return retval;
  }

  in_tracks = weed_get_int_array_counted(mt->init_event, WEED_LEAF_IN_TRACKS, &num_in_tracks);
  if (num_in_tracks > 0) {
    for (i = 0; i < num_in_tracks; i++) {
      if (in_tracks[i] == mt->current_track) {
        mt->track_index = i;
        if (track_index == -1 && mt->fx_box != NULL) {
          add_mt_param_box(mt);
          retval = TRUE;
        }
        break;
      }
    }
    lives_free(in_tracks);
  }
  lives_free(clips);
  if (track_index != -1 && mt->track_index == -1 && mt->fx_box != NULL) {
    add_mt_param_box(mt);
    retval = TRUE;
  }
  return retval;
}


void track_select(lives_mt * mt) {
  LiVESWidget *labelbox, *label, *hbox, *dummy, *ahbox, *arrow, *eventbox, *oeventbox, *checkbutton = NULL;
  LiVESList *list;
  weed_timecode_t tc;
  int hidden = 0;
  register int i;

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
    if (cfile->achans > 0) {
      i = 0;
      for (list = mt->audio_draws; list != NULL; list = list->next, i++) {
        eventbox = (LiVESWidget *)list->data;
        if ((oeventbox = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "owner")) != NULL)
          hidden = !LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(oeventbox), "expanded"));
        if (hidden == 0) hidden = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "hidden"));
        if (hidden == 0) {
          labelbox = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "labelbox");
          label = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "label");
          dummy = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "dummy");
          ahbox = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "ahbox");
          hbox = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "hbox");
          arrow = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "arrow");
          if (mt->current_track == i - mt->opts.back_audio_tracks && (mt->current_track < 0 || mt->aud_track_selected)) {
            // audio track is selected
            if (labelbox != NULL) {
              lives_widget_set_bg_color(labelbox, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
              lives_widget_set_fg_color(labelbox, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars_fore);
            }
            if (ahbox != NULL) {
              lives_widget_set_bg_color(ahbox, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
              lives_widget_set_fg_color(ahbox, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars_fore);
            }
            lives_widget_set_bg_color(label, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
            lives_widget_set_fg_color(label, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars_fore);
            lives_widget_set_bg_color(dummy, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
            lives_widget_set_fg_color(dummy, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars_fore);
            lives_widget_set_bg_color(arrow, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
            lives_widget_set_fg_color(arrow, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars_fore);

            lives_widget_set_sensitive(mt->jumpback, lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "blocks") != NULL);
            lives_widget_set_sensitive(mt->jumpnext, lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "blocks") != NULL);
          } else {
            if (labelbox != NULL) {
              lives_widget_set_bg_color(labelbox, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
              lives_widget_set_fg_color(labelbox, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
            }
            if (ahbox != NULL) {
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
  for (list = mt->video_draws; list != NULL; list = list->next, i++) {
    eventbox = (LiVESWidget *)list->data;
    hidden = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "hidden"));
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
            if (labelbox != NULL) {
              lives_widget_set_bg_color(labelbox, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
              lives_widget_set_fg_color(labelbox, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars_fore);
            }
            if (ahbox != NULL) {
              lives_widget_set_bg_color(ahbox, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
              lives_widget_set_fg_color(ahbox, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars_fore);
            }
            lives_widget_set_bg_color(label, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
            lives_widget_set_bg_color(arrow, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
            lives_widget_set_bg_color(checkbutton, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
            lives_widget_set_bg_color(hbox, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
            lives_widget_set_fg_color(label, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars_fore);
            lives_widget_set_fg_color(arrow, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars_fore);

            lives_widget_set_sensitive(mt->jumpback, lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "blocks") != NULL);
            lives_widget_set_sensitive(mt->jumpnext, lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "blocks") != NULL);
          } else {
            if (labelbox != NULL) {
              lives_widget_set_bg_color(labelbox, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
              lives_widget_set_fg_color(labelbox, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
            }
            if (ahbox != NULL) {
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
        if ((prefs->lamp_buttons && !giw_led_get_mode(GIW_LED(checkbutton))) || (!prefs->lamp_buttons &&
#else
        if (
#endif
            !lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(checkbutton)))
#ifdef ENABLE_GIW
           )
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
          if (labelbox != NULL) {
            lives_widget_set_bg_color(labelbox, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
            lives_widget_set_fg_color(labelbox, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
          }
          if (ahbox != NULL) {
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
  else if (mt->current_rfx != NULL && mt->init_event != NULL && mt->poly_state == POLY_PARAMS &&
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
        if (mt->current_rfx->params[i].changed) {
          interp = FALSE;
          break;
        }
      }
      if (mt->current_track >= 0) {
        // interpolate values ONLY if there are no unapplied changes (e.g. the time was altered)
        if (interp) interpolate_params((weed_plant_t *)mt->current_rfx->source, pchain, tc);
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
        mt->current_rfx->flags ^= RFX_FLAGS_NO_RESET;
        mt->opts.fx_auto_preview = aprev;
        activate_mt_preview(mt);
      } else
        mt_show_current_frame(mt, FALSE);
    } else polymorph(mt, POLY_FX_STACK);
  }
}


static void show_track_info(lives_mt * mt, LiVESWidget * eventbox, int track, double timesecs) {
  char *tmp, *tmp1;
  track_rect *block = get_block_from_time(eventbox, timesecs, mt);
  int filenum;

  clear_context(mt);
  if (!is_audio_eventbox(eventbox)) add_context_label
    (mt, (tmp = lives_strdup_printf
                (_("Current track: %s (layer %d)\n"),
                 lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox),
                     "track_name"), track)));
  else {
    if (track == -1) add_context_label(mt, (tmp = lives_strdup(_("Current track: Backing audio\n"))));
    else add_context_label(mt, (tmp = lives_strdup_printf(_("Current track: Layer %d audio\n"), track)));
  }
  lives_free(tmp);
  add_context_label(mt, (tmp = lives_strdup_printf(_("%.2f sec.\n"), timesecs)));
  lives_free(tmp);
  if (block != NULL) {
    if (!is_audio_eventbox(eventbox)) filenum = get_frame_event_clip(block->start_event, track);
    else filenum = get_audio_frame_clip(block->start_event, track);
    add_context_label(mt, (tmp = lives_strdup_printf(_("Source: %s"), (tmp1 = get_menu_name(mainw->files[filenum], FALSE)))));
    lives_free(tmp);
    lives_free(tmp1);
    add_context_label(mt, (_("Right click for context menu.\n")));
  }
  add_context_label(mt, (_("Double click on a block\nto select it.")));
}


static boolean atrack_ebox_pressed(LiVESWidget * labelbox, LiVESXEventButton * event, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  int current_track = mt->current_track;
  if (!mainw->interactive) return FALSE;
  mt->current_track = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(labelbox), "layer_number"));
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
  if (!mainw->interactive) return FALSE;
  mt->current_track = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(labelbox), "layer_number"));
  if (current_track != mt->current_track) mt->fm_edit_event = NULL;
  mt->aud_track_selected = FALSE;
  track_select(mt);
  show_track_info(mt, (LiVESWidget *)lives_list_nth_data(mt->video_draws, mt->current_track), mt->current_track, mt->ptr_time);
  return FALSE;
}


static boolean on_mt_timeline_scroll(LiVESWidget * widget, LiVESXEventScroll * event, livespointer user_data) {
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


static int get_top_track_for(lives_mt * mt, int track) {
  // find top track such that all of track fits at the bottom

  LiVESWidget *eventbox;
  LiVESList *vdraw;
  int extras = prefs->max_disp_vtracks - 1;
  int hidden, expanded;

  if (mt->opts.back_audio_tracks > 0 && mt->audio_draws == NULL) mt->opts.back_audio_tracks = 0;
  if (cfile->achans > 0 && mt->opts.back_audio_tracks > 0) {
    eventbox = (LiVESWidget *)mt->audio_draws->data;
    hidden = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "hidden"));
    if (!hidden) {
      extras--;
      expanded = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "expanded"));
      if (expanded) {
        extras -= cfile->achans;
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
      extras -= cfile->achans;
    }
  }

  if (extras < 0) return track;

  vdraw = vdraw->prev;

  while (vdraw != NULL) {
    eventbox = (LiVESWidget *)vdraw->data;
    hidden = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "hidden"))&TRACK_I_HIDDEN_USER;
    expanded = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "expanded"));
    extras--;
    if (expanded) {
      eventbox = (LiVESWidget *)(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "atrack"));
      extras--;
      expanded = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "expanded"));
      if (expanded) {
        extras -= cfile->achans;
      }
    }
    if (extras < 0) break;
    vdraw = vdraw->prev;
    track--;
  }

  if (track < 0) track = 0;
  return track;
}


static void redraw_all_event_boxes(lives_mt * mt) {
  LiVESList *slist;

  slist = mt->audio_draws;
  while (slist != NULL) {
    redraw_eventbox(mt, (LiVESWidget *)slist->data);
    slist = slist->next;
  }

  slist = mt->video_draws;
  while (slist != NULL) {
    redraw_eventbox(mt, (LiVESWidget *)slist->data);
    slist = slist->next;
  }
  paint_lines(mt, mt->ptr_time, TRUE);
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
  while (vdraws != NULL) {
    eventbox = (LiVESWidget *)vdraws->data;
    hidden = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "hidden"));
    hidden |= TRACK_I_HIDDEN_SCROLLED;
    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(eventbox), "hidden", LIVES_INT_TO_POINTER(hidden));

    aeventbox = LIVES_WIDGET(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "atrack"));

    if (aeventbox != NULL) {
      hidden = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(aeventbox), "hidden"));
      hidden |= TRACK_I_HIDDEN_SCROLLED;
      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(aeventbox), "hidden", LIVES_INT_TO_POINTER(hidden));

      xeventbox = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(aeventbox), "achan0");

      if (xeventbox != NULL) {
        hidden = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(xeventbox), "hidden"));
        hidden |= TRACK_I_HIDDEN_SCROLLED;
        lives_widget_object_set_data(LIVES_WIDGET_OBJECT(xeventbox), "hidden", LIVES_INT_TO_POINTER(hidden));
      }

      xeventbox = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(aeventbox), "achan1");

      if (xeventbox != NULL) {
        hidden = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(xeventbox), "hidden"));
        hidden |= TRACK_I_HIDDEN_SCROLLED;
        lives_widget_object_set_data(LIVES_WIDGET_OBJECT(xeventbox), "hidden", LIVES_INT_TO_POINTER(hidden));
      }
    }

    vdraws = vdraws->next;
  }

  if (mt->timeline_table != NULL) {
    lives_widget_destroy(mt->timeline_table);
  }

  mt->timeline_table = lives_table_new(prefs->max_disp_vtracks, TIMELINE_TABLE_COLUMNS, TRUE);
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(mt->timeline_table), "has_line", LIVES_INT_TO_POINTER(-1));

  if (palette->style & STYLE_1) {
    lives_widget_set_bg_color(LIVES_WIDGET(mt->timeline_table), LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
  }

  lives_container_add(LIVES_CONTAINER(mt->tl_eventbox), mt->timeline_table);

  lives_table_set_row_spacings(LIVES_TABLE(mt->timeline_table), widget_opts.packing_height * widget_opts.scale);
  lives_table_set_col_spacings(LIVES_TABLE(mt->timeline_table), 0);

  lives_widget_set_vexpand(mt->timeline_table, FALSE);

  if (mt->opts.back_audio_tracks > 0 && mt->audio_draws == NULL) mt->opts.back_audio_tracks = 0;

  if (cfile->achans > 0 && mt->opts.back_audio_tracks > 0) {
    // show our float audio
    if (LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(mt->audio_draws->data), "hidden")) == 0) {
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
      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(labelbox), "layer_number", LIVES_INT_TO_POINTER(-1));

      lives_signal_connect(LIVES_GUI_OBJECT(labelbox), LIVES_WIDGET_BUTTON_PRESS_EVENT,
                           LIVES_GUI_CALLBACK(atrack_ebox_pressed),
                           (livespointer)mt);

      lives_signal_connect(LIVES_GUI_OBJECT(ahbox), LIVES_WIDGET_BUTTON_PRESS_EVENT,
                           LIVES_GUI_CALLBACK(track_arrow_pressed),
                           (livespointer)mt);

      lives_table_attach(LIVES_TABLE(mt->timeline_table), (LiVESWidget *)mt->audio_draws->data, 7, TIMELINE_TABLE_COLUMNS, 0, 1,
                         (LiVESAttachOptions)(LIVES_EXPAND | LIVES_FILL),
                         (LiVESAttachOptions)(0), 0, 0);

      lives_widget_set_valign((LiVESWidget *)mt->audio_draws->data, LIVES_ALIGN_CENTER);

      lives_signal_connect(LIVES_GUI_OBJECT(mt->audio_draws->data), LIVES_WIDGET_BUTTON_PRESS_EVENT,
                           LIVES_GUI_CALLBACK(on_track_click),
                           (livespointer)mt);
      lives_signal_connect(LIVES_GUI_OBJECT(mt->audio_draws->data), LIVES_WIDGET_BUTTON_RELEASE_EVENT,
                           LIVES_GUI_CALLBACK(on_track_release),
                           (livespointer)mt);

      lives_widget_set_bg_color(LIVES_WIDGET(mt->audio_draws->data), LIVES_WIDGET_STATE_NORMAL, &col);
      lives_widget_set_app_paintable(LIVES_WIDGET(mt->audio_draws->data), TRUE);
      lives_signal_connect(LIVES_GUI_OBJECT(mt->audio_draws->data), LIVES_WIDGET_EXPOSE_EVENT,
                           LIVES_GUI_CALLBACK(expose_track_event),
                           (livespointer)mt);

      if (expanded) {
        xeventbox = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(mt->audio_draws->data), "achan0");

        label = (LIVES_WIDGET(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(xeventbox), "label")));
        labelbox = lives_event_box_new();
        hbox = lives_hbox_new(FALSE, widget_opts.packing_width);
        lives_container_add(LIVES_CONTAINER(labelbox), hbox);
        lives_box_pack_end(LIVES_BOX(hbox), label, TRUE, TRUE, 0);

        lives_table_attach(LIVES_TABLE(mt->timeline_table), labelbox, 2, 6, 1, 2, LIVES_FILL, (LiVESAttachOptions)0, 0, 0);
        lives_table_attach(LIVES_TABLE(mt->timeline_table), xeventbox, 7, TIMELINE_TABLE_COLUMNS, 1, 2,
                           (LiVESAttachOptions)(LIVES_EXPAND | LIVES_FILL),
                           (LiVESAttachOptions)(0), 0, 0);

        lives_widget_set_valign(xeventbox, LIVES_ALIGN_CENTER);

        lives_widget_set_bg_color(xeventbox, LIVES_WIDGET_STATE_NORMAL, &col);
        lives_widget_set_app_paintable(xeventbox, TRUE);
        lives_signal_connect(LIVES_GUI_OBJECT(xeventbox), LIVES_WIDGET_EXPOSE_EVENT,
                             LIVES_GUI_CALLBACK(mt_expose_audtrack_event),
                             (livespointer)mt);

        if (cfile->achans > 1) {
          xeventbox = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(mt->audio_draws->data), "achan1");

          label = (LIVES_WIDGET(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(xeventbox), "label")));
          labelbox = lives_event_box_new();
          hbox = lives_hbox_new(FALSE, widget_opts.packing_width);
          lives_container_add(LIVES_CONTAINER(labelbox), hbox);
          lives_box_pack_end(LIVES_BOX(hbox), label, TRUE, TRUE, 0);

          lives_table_attach(LIVES_TABLE(mt->timeline_table), labelbox, 2, 6, 2, 3, LIVES_FILL, (LiVESAttachOptions)0, 0, 0);
          lives_table_attach(LIVES_TABLE(mt->timeline_table), xeventbox, 7, TIMELINE_TABLE_COLUMNS, 2, 3,
                             (LiVESAttachOptions)(LIVES_EXPAND | LIVES_FILL),
                             (LiVESAttachOptions)(0), 0, 0);

          lives_widget_set_valign(xeventbox, LIVES_ALIGN_CENTER);

          lives_widget_set_bg_color(xeventbox, LIVES_WIDGET_STATE_NORMAL, &col);
          lives_widget_set_app_paintable(xeventbox, TRUE);
          lives_signal_connect(LIVES_GUI_OBJECT(xeventbox), LIVES_WIDGET_EXPOSE_EVENT,
                               LIVES_GUI_CALLBACK(mt_expose_audtrack_event),
                               (livespointer)mt);

        }
        aud_tracks += cfile->achans;
      }
    }
  }

  lives_adjustment_set_page_size(LIVES_ADJUSTMENT(mt->vadjustment),
                                 (double)((int)(lives_adjustment_get_page_size(LIVES_ADJUSTMENT(mt->vadjustment))) - aud_tracks));

  vdraws = lives_list_nth(mt->video_draws, top_track);

  rows += aud_tracks;

  while (vdraws != NULL && rows < prefs->max_disp_vtracks) {
    eventbox = (LiVESWidget *)vdraws->data;

    hidden = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "hidden"))&TRACK_I_HIDDEN_USER;
    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(eventbox), "hidden", LIVES_INT_TO_POINTER(hidden));

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

      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(labelbox), "layer_number",
                                   LIVES_INT_TO_POINTER(LIVES_POINTER_TO_INT
                                       (lives_widget_object_get_data
                                        (LIVES_WIDGET_OBJECT(eventbox), "layer_number"))));

      lives_container_add(LIVES_CONTAINER(labelbox), hbox);
      lives_box_pack_start(LIVES_BOX(hbox), checkbutton, FALSE, FALSE, widget_opts.border_width);
      lives_box_pack_start(LIVES_BOX(hbox), label, TRUE, TRUE, 0);
      lives_container_add(LIVES_CONTAINER(ahbox), arrow);

      lives_table_attach(LIVES_TABLE(mt->timeline_table), labelbox, 0, 6, rows, rows + 1, LIVES_FILL, (LiVESAttachOptions)0, 0, 0);
      lives_table_attach(LIVES_TABLE(mt->timeline_table), ahbox, 6, 7, rows, rows + 1, LIVES_FILL, (LiVESAttachOptions)0, 0, 0);

      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(eventbox), "labelbox", labelbox);
      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(eventbox), "hbox", hbox);
      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(eventbox), "ahbox", ahbox);
      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(ahbox), "eventbox", eventbox);

      lives_table_attach(LIVES_TABLE(mt->timeline_table), eventbox, 7, TIMELINE_TABLE_COLUMNS, rows, rows + 1,
                         (LiVESAttachOptions)(LIVES_EXPAND | LIVES_FILL),
                         (LiVESAttachOptions)(0), 0, 0);

      lives_widget_set_valign(eventbox, LIVES_ALIGN_CENTER);

      if (!prefs->lamp_buttons) {
        lives_signal_connect_after(LIVES_GUI_OBJECT(checkbutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                                   LIVES_GUI_CALLBACK(on_seltrack_toggled),
                                   mt);
      } else {
        lives_signal_connect_after(LIVES_GUI_OBJECT(checkbutton), LIVES_WIDGET_MODE_CHANGED_SIGNAL,
                                   LIVES_GUI_CALLBACK(on_seltrack_toggled),
                                   mt);
      }

      lives_signal_connect(LIVES_GUI_OBJECT(labelbox), LIVES_WIDGET_BUTTON_PRESS_EVENT,
                           LIVES_GUI_CALLBACK(track_ebox_pressed),
                           (livespointer)mt);

      lives_widget_set_bg_color(eventbox, LIVES_WIDGET_STATE_NORMAL, &col);
      lives_widget_set_app_paintable(eventbox, TRUE);
      lives_signal_connect(LIVES_GUI_OBJECT(eventbox), LIVES_WIDGET_EXPOSE_EVENT,
                           LIVES_GUI_CALLBACK(expose_track_event),
                           (livespointer)mt);

      lives_signal_connect(LIVES_GUI_OBJECT(eventbox), LIVES_WIDGET_BUTTON_PRESS_EVENT,
                           LIVES_GUI_CALLBACK(on_track_click),
                           (livespointer)mt);
      lives_signal_connect(LIVES_GUI_OBJECT(eventbox), LIVES_WIDGET_BUTTON_RELEASE_EVENT,
                           LIVES_GUI_CALLBACK(on_track_release),
                           (livespointer)mt);

      lives_signal_connect(LIVES_GUI_OBJECT(ahbox), LIVES_WIDGET_BUTTON_PRESS_EVENT,
                           LIVES_GUI_CALLBACK(track_arrow_pressed),
                           (livespointer)mt);
      rows++;

      if (rows == prefs->max_disp_vtracks) break;

      if (mt->opts.pertrack_audio && lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "expanded")) {

        aeventbox = LIVES_WIDGET(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "atrack"));

        hidden = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(aeventbox), "hidden"))&TRACK_I_HIDDEN_USER;
        lives_widget_object_set_data(LIVES_WIDGET_OBJECT(aeventbox), "hidden", LIVES_INT_TO_POINTER(hidden));

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
          lives_table_attach(LIVES_TABLE(mt->timeline_table), dummy, 0, 2, rows, rows + 1, LIVES_FILL, (LiVESAttachOptions)0, 0, 0);
          lives_table_attach(LIVES_TABLE(mt->timeline_table), labelbox, 2, 6, rows, rows + 1, LIVES_FILL, (LiVESAttachOptions)0, 0, 0);
          lives_table_attach(LIVES_TABLE(mt->timeline_table), ahbox, 6, 7, rows, rows + 1, LIVES_FILL, (LiVESAttachOptions)0, 0, 0);

          lives_widget_object_set_data(LIVES_WIDGET_OBJECT(aeventbox), "labelbox", labelbox);
          lives_widget_object_set_data(LIVES_WIDGET_OBJECT(aeventbox), "hbox", hbox);
          lives_widget_object_set_data(LIVES_WIDGET_OBJECT(aeventbox), "ahbox", ahbox);
          lives_widget_object_set_data(LIVES_WIDGET_OBJECT(ahbox), "eventbox", aeventbox);
          lives_widget_object_set_data(LIVES_WIDGET_OBJECT(labelbox), "layer_number",
                                       LIVES_INT_TO_POINTER(LIVES_POINTER_TO_INT
                                           (lives_widget_object_get_data
                                            (LIVES_WIDGET_OBJECT(eventbox), "layer_number"))));

          lives_signal_connect(LIVES_GUI_OBJECT(labelbox), LIVES_WIDGET_BUTTON_PRESS_EVENT,
                               LIVES_GUI_CALLBACK(atrack_ebox_pressed),
                               (livespointer)mt);

          lives_signal_connect(LIVES_GUI_OBJECT(ahbox), LIVES_WIDGET_BUTTON_PRESS_EVENT,
                               LIVES_GUI_CALLBACK(track_arrow_pressed),
                               (livespointer)mt);

          lives_table_attach(LIVES_TABLE(mt->timeline_table), aeventbox, 7, TIMELINE_TABLE_COLUMNS, rows, rows + 1,
                             (LiVESAttachOptions)(LIVES_EXPAND | LIVES_FILL),
                             (LiVESAttachOptions)(0), 0, 0);

          lives_widget_set_valign(aeventbox, LIVES_ALIGN_CENTER);

          lives_signal_connect(LIVES_GUI_OBJECT(aeventbox), LIVES_WIDGET_BUTTON_PRESS_EVENT,
                               LIVES_GUI_CALLBACK(on_track_click),
                               (livespointer)mt);
          lives_signal_connect(LIVES_GUI_OBJECT(aeventbox), LIVES_WIDGET_BUTTON_RELEASE_EVENT,
                               LIVES_GUI_CALLBACK(on_track_release),
                               (livespointer)mt);

          lives_widget_set_app_paintable(aeventbox, TRUE);
          lives_signal_connect(LIVES_GUI_OBJECT(aeventbox), LIVES_WIDGET_EXPOSE_EVENT,
                               LIVES_GUI_CALLBACK(expose_track_event),
                               (livespointer)mt);

          rows++;

          if (rows == prefs->max_disp_vtracks) break;

          if (expanded) {
            xeventbox = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(aeventbox), "achan0");
            hidden = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(xeventbox), "hidden")) & TRACK_I_HIDDEN_USER;
            lives_widget_object_set_data(LIVES_WIDGET_OBJECT(xeventbox), "hidden", LIVES_INT_TO_POINTER(hidden));

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
                                             (double)((int)lives_adjustment_get_page_size(LIVES_ADJUSTMENT(mt->vadjustment)) - 1));

              lives_table_attach(LIVES_TABLE(mt->timeline_table), labelbox, 2, 6, rows, rows + 1, LIVES_FILL, (LiVESAttachOptions)0, 0, 0);
              lives_table_attach(LIVES_TABLE(mt->timeline_table), xeventbox, 7, TIMELINE_TABLE_COLUMNS, rows, rows + 1,
                                 (LiVESAttachOptions)(LIVES_EXPAND | LIVES_FILL),
                                 (LiVESAttachOptions)(LIVES_FILL), 0, 0);

              lives_widget_object_set_data(LIVES_WIDGET_OBJECT(xeventbox), "labelbox", labelbox);
              lives_widget_object_set_data(LIVES_WIDGET_OBJECT(xeventbox), "hbox", hbox);

              lives_widget_set_bg_color(xeventbox, LIVES_WIDGET_STATE_NORMAL, &col);
              lives_widget_set_app_paintable(xeventbox, TRUE);
              lives_signal_connect(LIVES_GUI_OBJECT(xeventbox), LIVES_WIDGET_EXPOSE_EVENT,
                                   LIVES_GUI_CALLBACK(mt_expose_audtrack_event),
                                   (livespointer)mt);

              rows++;
              if (rows == prefs->max_disp_vtracks) break;
            }

            if (cfile->achans > 1) {
              xeventbox = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(aeventbox), "achan1");
              hidden = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(xeventbox), "hidden")) & TRACK_I_HIDDEN_USER;
              lives_widget_object_set_data(LIVES_WIDGET_OBJECT(xeventbox), "hidden", LIVES_INT_TO_POINTER(hidden));

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
                                               (double)((int)lives_adjustment_get_page_size(LIVES_ADJUSTMENT(mt->vadjustment)) - 1));

                lives_table_attach(LIVES_TABLE(mt->timeline_table), labelbox, 2, 6, rows, rows + 1, LIVES_FILL, (LiVESAttachOptions)0, 0, 0);
                lives_table_attach(LIVES_TABLE(mt->timeline_table), xeventbox, 7, TIMELINE_TABLE_COLUMNS, rows, rows + 1,
                                   (LiVESAttachOptions)(LIVES_EXPAND | LIVES_FILL),
                                   (LiVESAttachOptions)(0), 0, 0);

                lives_widget_set_valign(xeventbox, LIVES_ALIGN_CENTER);

                lives_widget_object_set_data(LIVES_WIDGET_OBJECT(xeventbox), "labelbox", labelbox);
                lives_widget_object_set_data(LIVES_WIDGET_OBJECT(xeventbox), "hbox", hbox);

                lives_widget_set_bg_color(xeventbox, LIVES_WIDGET_STATE_NORMAL, &col);
                lives_widget_set_app_paintable(xeventbox, TRUE);
                lives_signal_connect(LIVES_GUI_OBJECT(xeventbox), LIVES_WIDGET_EXPOSE_EVENT,
                                     LIVES_GUI_CALLBACK(mt_expose_audtrack_event),
                                     (livespointer)mt);

                rows++;
                if (rows == prefs->max_disp_vtracks) break;
              }
            }
          }
        }
      }
    }
    vdraws = vdraws->next;
  }

  if (lives_adjustment_get_page_size(LIVES_ADJUSTMENT(mt->vadjustment)) < 1.)
    lives_adjustment_set_page_size(LIVES_ADJUSTMENT(mt->vadjustment), 1.);

  lives_adjustment_set_upper(LIVES_ADJUSTMENT(mt->vadjustment),
                             (double)(get_top_track_for(mt, mt->num_video_tracks - 1) +
                                      (int)lives_adjustment_get_page_size(LIVES_ADJUSTMENT(mt->vadjustment))));

  if (lives_adjustment_get_value(LIVES_ADJUSTMENT(mt->vadjustment)) + lives_adjustment_get_page_size(LIVES_ADJUSTMENT(
        mt->vadjustment)) >
      lives_adjustment_get_upper(LIVES_ADJUSTMENT(mt->vadjustment)))
    lives_adjustment_set_upper(LIVES_ADJUSTMENT(mt->vadjustment), lives_adjustment_get_value(LIVES_ADJUSTMENT(mt->vadjustment)) +
                               lives_adjustment_get_page_size(LIVES_ADJUSTMENT(mt->vadjustment)));

  xlist = table_children = lives_container_get_children(LIVES_CONTAINER(mt->timeline_table));

  while (table_children != NULL) {
    LiVESWidget *child = (LiVESWidget *)table_children->data;
    lives_widget_set_size_request(child, -1, MT_TRACK_HEIGHT);
    table_children = table_children->next;
  }

  if (xlist != NULL) lives_list_free(xlist);

  lives_paned_set_position(LIVES_PANED(mt->vpaned), 0.);

  lives_widget_show_all(mt->timeline_table);
  lives_widget_queue_draw(mt->vpaned);

  if (mt->is_ready) {
    mt->no_expose = FALSE;
    lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET, TRUE);
  }
}


boolean track_arrow_pressed(LiVESWidget * ebox, LiVESXEventButton * event, livespointer user_data) {
  LiVESWidget *eventbox = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(ebox), "eventbox");
  LiVESWidget *arrow = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "arrow"), *new_arrow;
  lives_mt *mt = (lives_mt *)user_data;
  boolean expanded = !(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "expanded"));

  if (!mainw->interactive) return FALSE;

  if (mt->audio_draws == NULL || (!mt->opts.pertrack_audio && (mt->opts.back_audio_tracks == 0 ||
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


void multitrack_view_clips(LiVESMenuItem * menuitem, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  polymorph(mt, POLY_CLIPS);
}


void multitrack_view_in_out(LiVESMenuItem * menuitem, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  if (mt->block_selected == NULL) return;
  if (!nb_ignore) {
    // workaround for possible UI issues
    polymorph(mt, POLY_IN_OUT);
    polymorph(mt, POLY_CLIPS);
    polymorph(mt, POLY_IN_OUT);
  }
}


static char *time_to_string(double secs) {
  int hours, mins, rest;
  char timestring[TIMECODE_LENGTH];

  hours = secs / 3600;
  secs -= hours * 3600.;
  mins = secs / 60;
  secs -= mins * 60.;
  rest = (secs - ((int)secs) * 1.) * 100. + .5;
  secs = (int)secs * 1.;
  lives_snprintf(timestring, TIMECODE_LENGTH, "%02d:%02d:%02d.%02d", hours, mins, (int)secs, rest);
  return lives_strdup(timestring);
}


static void update_timecodes(lives_mt * mt, double dtime) {
  char *timestring = time_to_string(QUANT_TIME(dtime));
  lives_snprintf(mt->timestring, TIMECODE_LENGTH, "%s", timestring);
  lives_entry_set_text(LIVES_ENTRY(mt->timecode), timestring);
  lives_free(timestring);
}


static void set_fxlist_label(lives_mt * mt) {
  char *tname = get_track_name(mt, mt->current_track, mt->aud_track_selected);
  char *text = lives_strdup_printf(_("\nEffects stack for %s at time %s\n"), tname, mt->timestring);
  lives_label_set_text(LIVES_LABEL(mt->fx_list_label), text);
  lives_free(tname);
  lives_free(text);
}


static void renumber_clips(void) {
  // remove gaps in our mainw->files array - caused when clips are closed
  // we also ensure each clip has a (non-zero) 64 bit unique_id to help with later id of the clips

  // called once when we enter multitrack mode

  int cclip;
  int i = 1, j;

  LiVESList *clist;

  boolean bad_header = FALSE;

  renumbered_clips[0] = 0;

  // walk through files mainw->files[cclip]
  // mainw->files[i] points to next non-NULL clip

  // if we find a gap we move i to cclip

  for (cclip = 1; i <= MAX_FILES; cclip++) {
    if (mainw->files[cclip] == NULL) {
      if (i != cclip) {
        mainw->files[cclip] = mainw->files[i];

        for (j = 0; j < FN_KEYS - 1; j++) {
          if (mainw->clipstore[j][0] == i) mainw->clipstore[j][0] = cclip;
        }

        // we need to change the entries in mainw->cliplist
        clist = mainw->cliplist;
        while (clist != NULL) {
          if (LIVES_POINTER_TO_INT(clist->data) == i) {
            clist->data = LIVES_INT_TO_POINTER(cclip);
            break;
          }
          clist = clist->next;
        }

        mainw->files[i] = NULL;

        if (mainw->scrap_file == i) mainw->scrap_file = cclip;
        if (mainw->ascrap_file == i) mainw->ascrap_file = cclip;
        if (mainw->current_file == i) mainw->current_file = cclip;

        if (mainw->first_free_file == cclip) mainw->first_free_file++;

        renumbered_clips[i] = cclip;
      }
      // process this clip again
      else cclip--;
    } else {
      renumbered_clips[cclip] = cclip;
      if (i == cclip) i++;
    }

    if (mainw->files[cclip] != NULL && (cclip == mainw->scrap_file || cclip == mainw->ascrap_file ||
                                        IS_NORMAL_CLIP(cclip)) && mainw->files[cclip]->unique_id == 0l) {
      mainw->files[cclip]->unique_id = lives_random();
      save_clip_value(cclip, CLIP_DETAILS_UNIQUE_ID, &mainw->files[cclip]->unique_id);
      if (mainw->com_failed || mainw->write_failed) bad_header = TRUE;

      if (bad_header) do_header_write_error(cclip);
    }

    for (; i <= MAX_FILES; i++) {
      if (mainw->files[i] != NULL) break;
    }
  }
}


static void rerenumber_clips(const char *lfile, weed_plant_t *event_list) {
  // we loaded an event_list, now we match clip numbers in event_list with our current clips, using the layout map file
  // the renumbering is used for translations in event_list_rectify
  // in mt_init_tracks we alter the clip numbers in the event_list

  // this means if we save again, the clip numbers in the disk event list (*.lay file) may be updated
  // however, since we also have a layout map file (*.map) for the set, this should not be too big an issue

  LiVESList *lmap;
  char **array;
  int rnc;
  register int i;

  // ensure file layouts are updated
  upd_layout_maps(event_list);

  renumbered_clips[0] = 0;

  for (i = 1; i <= MAX_FILES && mainw->files[i] != NULL; i++) {
    renumbered_clips[i] = 0;
    if (mainw->files[i] != NULL) lfps[i] = mainw->files[i]->fps;
    else lfps[i] = cfile->fps;
  }

  if (lfile != NULL) {
    // lfile is supplied layout file name
    for (i = 1; i <= MAX_FILES && mainw->files[i] != NULL; i++) {
      for (lmap = mainw->files[i]->layout_map; lmap != NULL; lmap = lmap->next) {
        // lmap->data starts with layout name
        if (!lives_strncmp((char *)lmap->data, lfile, strlen(lfile))) {
          threaded_dialog_spin(0.);
          array = lives_strsplit((char *)lmap->data, "|", -1);
          threaded_dialog_spin(0.);

          // piece 2 is the clip number
          rnc = atoi(array[1]);
          if (rnc < 0 || rnc > MAX_FILES) continue;
          renumbered_clips[rnc] = i;

          // original fps
          lfps[i] = strtod(array[3], NULL);
          threaded_dialog_spin(0.);
          lives_strfreev(array);
          threaded_dialog_spin(0.);
        }
      }
    }
  } else {
    // current event_list
    for (i = 1; i <= MAX_FILES && mainw->files[i] != NULL; i++) {
      if (mainw->files[i]->stored_layout_idx != -1) {
        renumbered_clips[mainw->files[i]->stored_layout_idx] = i;
      }
      lfps[i] = mainw->files[i]->stored_layout_fps;
    }
  }
}


void mt_clip_select(lives_mt * mt, boolean scroll) {
  LiVESList *list = lives_container_get_children(LIVES_CONTAINER(mt->clip_inner_box));
  LiVESWidget *clipbox = NULL;
  boolean was_neg = FALSE;
  int len;

  mt->file_selected = -1;

  if (list == NULL) return;

  if (mt->poly_state == POLY_FX_STACK && mt->event_list != NULL) {
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

  if (mt->clip_selected < 0) {
    mt->file_selected = -1;
    lives_list_free(list);
    return;
  }

  mt->file_selected = mt_file_from_clip(mt, mt->clip_selected);

  if (scroll) {
    LiVESAdjustment *adj = lives_scrolled_window_get_hadjustment(LIVES_SCROLLED_WINDOW(mt->clip_scroll));
    if (adj != NULL) {
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


boolean mt_prevclip(LiVESAccelGroup * group, LiVESWidgetObject * obj, uint32_t keyval, LiVESXModifierType mod,
                    livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  if (!mainw->interactive) return TRUE;
  mt->clip_selected--;
  polymorph(mt, POLY_CLIPS);
  mt_clip_select(mt, TRUE);
  return TRUE;
}


boolean mt_nextclip(LiVESAccelGroup * group, LiVESWidgetObject * obj, uint32_t keyval, LiVESXModifierType mod,
                    livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  if (!mainw->interactive) return TRUE;
  mt->clip_selected++;
  polymorph(mt, POLY_CLIPS);
  mt_clip_select(mt, TRUE);
  return TRUE;
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

  lives_ruler_set_range(LIVES_RULER(mt->timeline), mt->tl_min, mt->tl_max, mt->tl_min, mt->end_secs + 1. / mt->fps);
  lives_widget_queue_draw(mt->timeline);
  lives_widget_queue_draw(mt->timeline_table);
  if (!mt->sel_locked || mt->region_end < mt->end_secs + 1. / mt->fps) {
    lives_spin_button_set_range(LIVES_SPIN_BUTTON(mt->spinbutton_start), 0., mt->end_secs);
    lives_spin_button_set_range(LIVES_SPIN_BUTTON(mt->spinbutton_end), 0., mt->end_secs + 1. / mt->fps);
  }
  set_time_scrollbar(mt);

  lives_ruler_set_value(LIVES_RULER(mt->timeline), pos);

  redraw_all_event_boxes(mt);
}


static weed_timecode_t set_play_position(lives_mt * mt) {
  // get start event
  boolean has_pb_loop_event = FALSE;
  weed_timecode_t tc;
#ifdef ENABLE_JACK_TRANSPORT
  weed_timecode_t end_tc = event_list_get_end_tc(mt->event_list);
#endif

  mainw->cancelled = CANCEL_NONE;

#ifdef ENABLE_JACK_TRANSPORT
  // if we have jack transport enabled, we get our playback start time from there

  if (mainw->jack_can_stop && (prefs->jack_opts & JACK_OPTS_TIMEBASE_START) && (prefs->jack_opts & JACK_OPTS_TRANSPORT_CLIENT)) {
    mt->pb_loop_event = get_first_frame_event(mt->event_list);
    has_pb_loop_event = TRUE;
    tc = q_gint64(TICKS_PER_SECOND_DBL * jack_transport_get_time(), cfile->fps);
    if (!mainw->loop_cont) {
      if (tc > end_tc) {
        mainw->cancelled = CANCEL_VID_END;
        return 0;
      }
    }
    tc %= end_tc;
    mt->is_paused = FALSE;
  } else {
#endif
    //////////////////////////////////////////

    // set actual playback start time, from mt->ptr_time
    tc = q_gint64(mt->ptr_time * TICKS_PER_SECOND_DBL, cfile->fps);

    if (!mt->is_paused)
      mt->pb_unpaused_start_time = mt->ptr_time;

    mt->pb_start_time = mt->ptr_time;

    //////////////////////////////////
#ifdef ENABLE_JACK_TRANSPORT
  }
#endif
  // get the start event to play from
  if (tc > event_list_get_end_tc(mt->event_list) || tc == 0) mt->pb_start_event = get_first_frame_event(mt->event_list);
  else {
    mt->pb_start_event = get_frame_event_at(mt->event_list, tc, NULL, TRUE);
  }

  if (!has_pb_loop_event) mt->pb_loop_event = mt->pb_start_event;

  // return timecode of start event
  return get_event_timecode(mt->pb_start_event);
}


void mt_show_current_frame(lives_mt * mt, boolean return_layer) {
  // show preview of current frame in play_box and/or play_
  // or, if return_layer is TRUE, we just set mainw->frame_layer (used when we want to save the frame, e.g right click context)

  weed_timecode_t curr_tc;

  double ptr_time = mt->ptr_time;

  weed_plant_t *frame_layer = mainw->frame_layer;

  int current_file;
  int actual_frame;
  int error;

  boolean is_rendering = mainw->is_rendering;
  boolean internal_messaging = mainw->internal_messaging;
  boolean needs_idlefunc = FALSE;
  boolean did_backup = mt->did_backup;

  if (mt->play_width == 0 || mt->play_height == 0) return;
  if (mt->no_frame_update) return;

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

      if (lives_widget_get_parent(mt->play_blank) != NULL) {
        lives_widget_object_ref(mt->play_blank);
        lives_container_remove(LIVES_CONTAINER(mt->play_box), mt->play_blank);
      }

      if (mainw->plug != NULL) {
        lives_container_remove(LIVES_CONTAINER(mainw->plug), mainw->play_image);
        lives_widget_destroy(mainw->plug);
        mainw->plug = NULL;
      }

      if (LIVES_IS_WIDGET(mainw->playarea)) lives_widget_destroy(mainw->playarea);

      mainw->playarea = lives_hbox_new(FALSE, 0);
      lives_container_add(LIVES_CONTAINER(mt->play_box), mainw->playarea);
      mainw->sep_win = FALSE;
      add_to_playframe();
      set_mt_play_sizes(mt, cfile->hsize, cfile->vsize, TRUE);
      lives_widget_show_all(mainw->playarea);
      mainw->sep_win = sep_win;
    }
  }

  if (LIVES_IS_PLAYING) {
    if (mainw->play_window != NULL && LIVES_IS_XWINDOW(lives_widget_get_xwindow(mainw->play_window))) {
#if GTK_CHECK_VERSION(3, 0, 0)
      if (mt->frame_pixbuf == NULL || mt->frame_pixbuf != mainw->imframe) {
        if (mt->frame_pixbuf != NULL) lives_widget_object_unref(mt->frame_pixbuf);
        // set frame_pixbuf, this gets painted in in expose_event
        mt->frame_pixbuf = mainw->imframe;
      }
#else
      set_ce_frame_from_pixbuf(LIVES_IMAGE(mainw->play_image), mainw->imframe, NULL);
#endif
    } else {
#if GTK_CHECK_VERSION(3, 0, 0)
      if (mt->frame_pixbuf != mainw->imframe) {
        if (mt->frame_pixbuf != NULL) lives_widget_object_unref(mt->frame_pixbuf);
        mt->frame_pixbuf = NULL;
      }
#else
      set_ce_frame_from_pixbuf(LIVES_IMAGE(mainw->play_image), NULL, NULL);
#endif
    }
    lives_widget_queue_draw(mt->play_box);
    if (needs_idlefunc || (!did_backup && mt->auto_changed)) {
      mt->idlefunc = mt_idle_add(mt);
    }
    return;
  }

  // start "playback" at mt->ptr_time; we just "render" one frame
  curr_tc = set_play_position(mt);
  actual_frame = (int)((double)curr_tc / TICKS_PER_SECOND_DBL * cfile->fps + 1.4999);
  mainw->frame_layer = NULL;

  if (mt->is_rendering && actual_frame <= cfile->frames) {
    // get the actual frame if it has already been rendered
    mainw->frame_layer = lives_layer_new_for_frame(mainw->current_file, actual_frame);
    pull_frame(mainw->frame_layer, get_image_ext_for_type(cfile->img_type), curr_tc);
  } else {
    weed_plant_t *live_inst = NULL;
    mainw->is_rendering = TRUE;

    if (mt->event_list != NULL) {
      if (mt->pb_start_event != NULL) {
        cfile->next_event = mt->pb_start_event;
      } else {
        cfile->next_event = get_first_event(mt->event_list);
      }
      // "play" a single frame
      current_file = mainw->current_file;
      mainw->internal_messaging = TRUE; // stop load_frame from showing image
      cfile->next_event = mt->pb_start_event;
      if (is_rendering) {
        backup_weed_instances();
        backup_host_tags(mt->event_list, curr_tc);
      }

      // pass quickly through events_list, switching on and off effects and interpolating at current time
      get_audio_and_effects_state_at(mt->event_list, mt->pb_start_event, 0, LIVES_PREVIEW_TYPE_VIDEO_ONLY, mt->exact_preview);

      // if we are previewing a specific effect we also need to init it
      if (mt->current_rfx != NULL && mt->init_event != NULL) {
        if (mt->current_rfx->source_type == LIVES_RFX_SOURCE_WEED && mt->current_rfx->source != NULL) {
          live_inst = (weed_plant_t *)mt->current_rfx->source;

          // we want to get hold of the instance which the renderer will use, and then set the in_parameters
          // with the currently unapplied values from the live instance

          // the renderer's instance will be freed in deinit_render_effects(),
          // so we need to keep the live instance around for when we return

          if (weed_plant_has_leaf(mt->init_event, WEED_LEAF_HOST_TAG)) {
            char *keystr = weed_get_string_value(mt->init_event, WEED_LEAF_HOST_TAG, &error);
            int key = atoi(keystr) + 1;
            lives_freep((void **)&keystr);

            // get the rendering version:
            mt->current_rfx->source = (void *)rte_keymode_get_instance(key, 0); // adds a ref

            // for the preview we will use a copy of the current in_params from the live instance
            // interpolation is OFF here so we will see exactly the current values
            if (mt->current_rfx->source != NULL) {
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
      if (live_inst != NULL) mt->current_rfx->source = (void *)live_inst;

      if (is_rendering) {
        restore_weed_instances();
        restore_host_tags(mt->event_list, curr_tc);
      }
      cfile->next_event = NULL;
      mainw->is_rendering = is_rendering;
    }
  }

  if (return_layer) {
    if (needs_idlefunc || (!did_backup && mt->auto_changed)) {
      mt->idlefunc = mt_idle_add(mt);
    }
    return;
  }

#if GTK_CHECK_VERSION(3, 0, 0)
  if (mt->frame_pixbuf != NULL && mt->frame_pixbuf != mainw->imframe) {
    lives_widget_object_unref(mt->frame_pixbuf);
    mt->frame_pixbuf = NULL;
  }
#endif

  if (mt->frame_pixbuf != NULL && mt->frame_pixbuf == mainw->imframe) {
    // size_request, reset play frame size
    // try to expand / shrink
    lives_widget_set_size_request(mt->preview_eventbox, GUI_SCREEN_WIDTH / 3, GUI_SCREEN_HEIGHT / 3);
  }

  if (mainw->frame_layer != NULL) {
    LiVESPixbuf *pixbuf = NULL;
    int pwidth, pheight, lb_width, lb_height;
    int cpal = WEED_PALETTE_RGB24, layer_palette = weed_layer_get_palette(mainw->frame_layer);
    if (weed_palette_has_alpha(layer_palette)) cpal = WEED_PALETTE_RGBA32;

    pwidth = cfile->hsize;
    pheight = cfile->vsize;
    calc_maxspect(mt->play_width, mt->play_height, &pwidth, &pheight);
    lb_width = pwidth;
    lb_height = pheight;

    letterbox_layer(mainw->frame_layer, mt->play_width, mt->play_height, lb_width, lb_height, LIVES_INTERP_BEST,
                    cpal, 0);

    resize_layer(mainw->frame_layer, mt->play_width, mt->play_height, LIVES_INTERP_BEST,
                 cpal, 0);

    convert_layer_palette_full(mainw->frame_layer, cpal, 0, 0, 0,
                               prefs->gamma_srgb ? WEED_GAMMA_SRGB : WEED_GAMMA_MONITOR);

    gamma_convert_layer(prefs->gamma_srgb ? WEED_GAMMA_SRGB : WEED_GAMMA_MONITOR, mainw->frame_layer);

    if (mt->framedraw != NULL) mt_framedraw(mt, mainw->frame_layer); // framedraw will free the frame_layer itself
    else {
      if (pixbuf == NULL) {
        pixbuf = layer_to_pixbuf(mainw->frame_layer, TRUE, TRUE);
      }
#if GTK_CHECK_VERSION(3, 0, 0)
      // set frame_pixbuf, this gets painted in in expose_event
      mt->frame_pixbuf = pixbuf;
#else
      set_ce_frame_from_pixbuf(LIVES_IMAGE(mainw->play_image), pixbuf, NULL);
#endif
      lives_widget_queue_draw(mt->play_box);
      weed_plant_free(mainw->frame_layer);
    }
  } else {
    // no frame - show blank
#if GTK_CHECK_VERSION(3, 0, 0)
    // set frame_pixbuf, this gets painted in in expose_event
    mt->frame_pixbuf = mainw->imframe;
#else
    set_ce_frame_from_pixbuf(LIVES_IMAGE(mainw->play_image), mainw->imframe, NULL);
#endif
    lives_widget_queue_draw(mt->play_box);
  }

  /// restore original mainw->frame_layer
  mainw->frame_layer = frame_layer;

  lives_ruler_set_value(LIVES_RULER(mt->timeline), ptr_time);
  lives_widget_queue_draw(mt->timeline);

  if (needs_idlefunc || (!did_backup && mt->auto_changed)) {
    mt->idlefunc = mt_idle_add(mt);
  }
}


static void tc_to_rs(LiVESMenuItem * menuitem, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  mt->region_start = mt->ptr_time;
  on_timeline_release(mt->timeline_reg, NULL, mt);
}


static void tc_to_re(LiVESMenuItem * menuitem, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  mt->region_end = mt->ptr_time;
  on_timeline_release(mt->timeline_reg, NULL, mt);
}


static void rs_to_tc(LiVESMenuItem * menuitem, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  mt_tl_move(mt, mt->region_start);
}


static void re_to_tc(LiVESMenuItem * menuitem, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  mt_tl_move(mt, mt->region_end);
}


//////////////////////////////////////////////////

void mt_tl_move(lives_mt * mt, double pos) {
  if (LIVES_IS_PLAYING) return;

  pos = q_dbl(pos, mt->fps) / TICKS_PER_SECOND_DBL;
  if (pos < 0.) pos = 0.;

  // after this, we need to reference ONLY mt->ptr_time, since it may become outside the range of mt->timeline
  // thus we cannot rely on reading the value from mt->timeline
  mt->ptr_time = lives_ruler_set_value(LIVES_RULER(mt->timeline), pos);

  if (mt->is_ready) unpaint_lines(mt);

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
  if (mt->init_event != NULL && mt->poly_state == POLY_PARAMS && !mt->block_node_spin) {
    mt->block_tl_move = TRUE;
    lives_spin_button_set_value(LIVES_SPIN_BUTTON(mt->node_spinbutton),
                                pos - get_event_timecode(mt->init_event) / TICKS_PER_SECOND_DBL);
    mt->block_tl_move = FALSE;
  }

  update_timecodes(mt, pos);

  if (pos > mt->region_end - 1. / mt->fps) lives_widget_set_sensitive(mt->tc_to_rs, FALSE);
  else lives_widget_set_sensitive(mt->tc_to_rs, TRUE);
  if (pos < mt->region_start + 1. / mt->fps) lives_widget_set_sensitive(mt->tc_to_re, FALSE);
  else lives_widget_set_sensitive(mt->tc_to_re, TRUE);

  mt->fx_order = FX_ORD_NONE;
  if (mt->init_event != NULL) {
    int error;
    weed_timecode_t tc = q_gint64(pos * TICKS_PER_SECOND_DBL, mt->fps);
    weed_plant_t *deinit_event = weed_get_plantptr_value(mt->init_event, WEED_LEAF_DEINIT_EVENT, &error);
    if (tc < get_event_timecode(mt->init_event) || tc > get_event_timecode(deinit_event)) {
      mt->init_event = NULL;
      if (mt->poly_state == POLY_PARAMS) polymorph(mt, POLY_FX_STACK);
    }
  }

  if (mt->poly_state == POLY_FX_STACK) polymorph(mt, POLY_FX_STACK);
  if (mt->is_ready) {
    mt_show_current_frame(mt, FALSE);
    paint_lines(mt, pos, TRUE);
  }
}


LIVES_INLINE void mt_tl_move_relative(lives_mt * mt, double pos_rel) {
  mt_tl_move(mt, mt->ptr_time + pos_rel);
}


boolean mt_tlfor(LiVESAccelGroup * group, LiVESWidgetObject * obj, uint32_t keyval, LiVESXModifierType mod,
                 livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  if (!mainw->interactive) return TRUE;
  mt->fm_edit_event = NULL;
  mt_tl_move_relative(mt, 1.);
  return TRUE;
}


boolean mt_tlfor_frame(LiVESAccelGroup * group, LiVESWidgetObject * obj, uint32_t keyval, LiVESXModifierType mod,
                       livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  if (!mainw->interactive) return TRUE;
  mt->fm_edit_event = NULL;
  mt_tl_move_relative(mt, 1. / mt->fps);
  return TRUE;
}


boolean mt_tlback(LiVESAccelGroup * group, LiVESWidgetObject * obj, uint32_t keyval, LiVESXModifierType mod,
                  livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  if (!mainw->interactive) return TRUE;
  mt->fm_edit_event = NULL;
  mt_tl_move_relative(mt, -1.);
  return TRUE;
}

boolean mt_tlback_frame(LiVESAccelGroup * group, LiVESWidgetObject * obj, uint32_t keyval, LiVESXModifierType mod,
                        livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  if (!mainw->interactive) return TRUE;
  mt->fm_edit_event = NULL;
  mt_tl_move_relative(mt, -1. / mt->fps);
  return TRUE;
}


static void scroll_track_on_screen(lives_mt * mt, int track) {
  if (track > mt->top_track) track = get_top_track_for(mt, track);
  scroll_tracks(mt, track, track != mt->top_track);
}


void scroll_track_by_scrollbar(LiVESScrollbar * sbar, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  scroll_tracks(mt, lives_adjustment_get_value(lives_range_get_adjustment(LIVES_RANGE(sbar))), FALSE);
  track_select(mt);
}


static void mt_zoom(lives_mt * mt, double scale) {
  // ABS(scale) < 1.0 == zoom in

  // scale < 0.0 = center on screen middle
  // scale > 0.0 = center on cursor

  double tl_span = (mt->tl_max - mt->tl_min) / 2.;
  double tl_cur;

  if (scale > 0.) {
    tl_cur = mt->ptr_time;
  } else {
    tl_cur = mt->tl_min + tl_span; // center on middle of screen
    scale = -scale;
  }

  mt->tl_min = tl_cur - tl_span * scale; // new min
  mt->tl_max = tl_cur + tl_span * scale; // new max

  if (mt->tl_min < 0.) {
    mt->tl_max -= mt->tl_min;
    mt->tl_min = 0.;
  }

  mt->tl_min = q_gint64(mt->tl_min * TICKS_PER_SECOND_DBL, mt->fps) / TICKS_PER_SECOND_DBL;
  mt->tl_max = q_gint64(mt->tl_max * TICKS_PER_SECOND_DBL, mt->fps) / TICKS_PER_SECOND_DBL;

  if (mt->tl_min == mt->tl_max) mt->tl_max = mt->tl_min + 1. / mt->fps;

  lives_ruler_set_upper(LIVES_RULER(mt->timeline), mt->tl_max);
  lives_ruler_set_lower(LIVES_RULER(mt->timeline), mt->tl_min);

  set_time_scrollbar(mt);

  lives_widget_queue_draw(mt->vpaned);
  lives_widget_queue_draw(mt->timeline);

  redraw_all_event_boxes(mt);
}


static void scroll_time_by_scrollbar(LiVESHScrollbar * sbar, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  mt->tl_min = lives_adjustment_get_value(lives_range_get_adjustment(LIVES_RANGE(sbar)));
  mt->tl_max = lives_adjustment_get_value(lives_range_get_adjustment(LIVES_RANGE(sbar)))
               + lives_adjustment_get_page_size(lives_range_get_adjustment(LIVES_RANGE(sbar)));
  mt_zoom(mt, -1.);
  paint_lines(mt, mt->ptr_time, TRUE);
}


boolean mt_trdown(LiVESAccelGroup * group, LiVESWidgetObject * obj, uint32_t keyval, LiVESXModifierType mod,
                  livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  if (!mainw->interactive) return TRUE;

  if (mt->current_track >= 0 && mt->opts.pertrack_audio && !mt->aud_track_selected) {
    LiVESWidget *eventbox = (LiVESWidget *)lives_list_nth_data(mt->video_draws, mt->current_track);
    mt->aud_track_selected = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "expanded"));
    if (!mt->aud_track_selected && mt->current_track == mt->num_video_tracks - 1) return TRUE;
  } else {
    if (mt->current_track == mt->num_video_tracks - 1) return TRUE;
    mt->aud_track_selected = FALSE;
  }

  if (!mt->aud_track_selected || mt->current_track == -1) {
    if (mt->current_track > -1) mt->current_track++;
    else {
      int i = 0;
      LiVESList *llist = mt->video_draws;
      while (llist != NULL) {
        if (LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(llist->data), "hidden")) == 0) {
          mt->current_track = i;
          break;
        }
        llist = llist->next;
        i++;
      }
      mt->current_track = i;
    }
  }
  mt->selected_init_event = NULL;
  scroll_track_on_screen(mt, mt->current_track);
  track_select(mt);

  return TRUE;
}


boolean mt_trup(LiVESAccelGroup * group, LiVESWidgetObject * obj, uint32_t keyval, LiVESXModifierType mod,
                livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  if (mt->current_track == -1 || (mt->current_track == 0 && !mt->aud_track_selected && !mt->opts.show_audio)) return TRUE;
  if (!mainw->interactive) return TRUE;

  if (mt->aud_track_selected) mt->aud_track_selected = FALSE;
  else {
    mt->current_track--;
    if (mt->current_track >= 0 && mt->opts.pertrack_audio) {
      LiVESWidget *eventbox = (LiVESWidget *)lives_list_nth_data(mt->video_draws, mt->current_track);
      mt->aud_track_selected = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "expanded"));
    }
  }
  mt->selected_init_event = NULL;
  if (mt->current_track != -1) scroll_track_on_screen(mt, mt->current_track);
  track_select(mt);

  return TRUE;
}


LIVES_INLINE int poly_page_to_tab(uint32_t page) {
  return ++page;
}


LIVES_INLINE int poly_tab_to_page(uint32_t tab) {
  return --tab;
}


LIVES_INLINE lives_mt_poly_state_t get_poly_state_from_page(lives_mt * mt) {
  return (lives_mt_poly_state_t)poly_page_to_tab(lives_notebook_get_current_page(LIVES_NOTEBOOK(mt->nb)));
}


static void notebook_error(LiVESNotebook * nb, uint32_t tab, lives_mt_nb_error_t err, lives_mt * mt) {
  uint32_t page = poly_tab_to_page(tab);

  if (mt->nb_label != NULL) lives_widget_destroy(mt->nb_label);
  mt->nb_label = NULL;

  widget_opts.justify = LIVES_JUSTIFY_CENTER;

  switch (err) {
  case NB_ERROR_SEL:
    mt->nb_label = lives_standard_label_new(_("\n\nPlease select a block\nin the timeline by\nright or double clicking on it.\n"));
    break;
  case NB_ERROR_NOEFFECT:
    mt->nb_label = lives_standard_label_new(
                     _("\n\nNo effect selected.\nSelect an effect in FX stack first to view its parameters.\n"));
    break;
  case NB_ERROR_NOCLIP:
    mt->nb_label = lives_standard_label_new(_("\n\nNo clips loaded.\n"));
    break;
  case NB_ERROR_NOTRANS:
    mt->nb_label = lives_standard_label_new(
                     _("You must select two video tracks\nand a time region\nto apply transitions.\n\n"
                       "Alternately, you can enable Autotransitions from the Effects menu\nbefore inserting clips into the timeline."));
    break;
  case NB_ERROR_NOCOMP:
    mt->nb_label = lives_standard_label_new(
                     _("\n\nYou must select at least one video track\nand a time region\nto apply compositors.\n"));
    break;
  }

  widget_opts.justify = widget_opts.default_justify;

  lives_widget_set_hexpand(mt->nb_label, TRUE);

  // add label to notebook page
  lives_container_add(LIVES_CONTAINER(lives_notebook_get_nth_page(LIVES_NOTEBOOK(nb), page)), mt->nb_label);
  lives_container_set_border_width(LIVES_CONTAINER(lives_notebook_get_nth_page(LIVES_NOTEBOOK(nb), page)),
                                   widget_opts.border_width);
  lives_widget_show(mt->nb_label);

  // hide the poly box
  lives_widget_hide(mt->poly_box);

  lives_widget_queue_resize(mt->nb_label);
}


static void fubar(lives_mt * mt) {
  int npch, i, error;
  int num_in_tracks;
  int *in_tracks;
  void **pchainx;
  char *fhash;

  mt->init_event = mt->selected_init_event;

  mt->track_index = -1;

  in_tracks = weed_get_int_array_counted(mt->init_event, WEED_LEAF_IN_TRACKS, &num_in_tracks);
  if (num_in_tracks > 0) {
    // set track_index (for special widgets)
    for (i = 0; i < num_in_tracks; i++) {
      if (mt->current_track == in_tracks[i]) {
        mt->track_index = i;
        break;
      }
    }
    lives_free(in_tracks);
  }

  fhash = weed_get_string_value(mt->init_event, WEED_LEAF_FILTER, &error);
  mt->current_fx = weed_get_idx_for_hashname(fhash, TRUE);
  lives_free(fhash);

  pchainx = weed_get_voidptr_array_counted(mt->init_event, WEED_LEAF_IN_PARAMETERS, &npch);
  if (npch > 0) {
    pchain = (void **)lives_malloc((npch + 1) * sizeof(void *));
    for (i = 0; i < npch; i++) pchain[i] = pchainx[i];
    pchain[i] = NULL;
    lives_free(pchainx);
  }
}


static boolean notebook_page(LiVESWidget * nb, LiVESWidget * nbp, uint32_t tab, livespointer user_data) {
  // this is called once or twice: - once when the user clicks on a tab, and a second time from polymorph
  // (via set_poly_tab(), lives_notebook_set_current_page() )

  uint32_t page;
  lives_mt *mt = (lives_mt *)user_data;

  if (nbp != NULL) {
    page = tab;
    tab = poly_page_to_tab(page);
  } else {
    // should never be NULL i think
    page = poly_tab_to_page(tab);
  }

  // destroy the label that was in the page
  if (mt->nb_label != NULL) lives_widget_destroy(mt->nb_label);
  mt->nb_label = NULL;

  lives_widget_show(mt->poly_box);
  lives_widget_show(lives_notebook_get_nth_page(LIVES_NOTEBOOK(mt->nb), page));

  // we reparent the poly_box in the current tab

  switch (tab) {
  case POLY_CLIPS:
    if (mt->clip_labels == NULL) {
      notebook_error(LIVES_NOTEBOOK(mt->nb), tab, NB_ERROR_NOCLIP, mt);
      return FALSE;
    }
    if (mt->poly_state != POLY_CLIPS && nb != NULL) polymorph(mt, POLY_CLIPS);
    else lives_widget_reparent(mt->poly_box, lives_notebook_get_nth_page(LIVES_NOTEBOOK(mt->nb), page));
    break;
  case POLY_IN_OUT:
    if (mt->block_selected == NULL && mt->poly_state != POLY_IN_OUT) {
      notebook_error(LIVES_NOTEBOOK(mt->nb), tab, NB_ERROR_SEL, mt);
      return FALSE;
    }
    if (mt->poly_state != POLY_IN_OUT) polymorph(mt, POLY_IN_OUT);
    else lives_widget_reparent(mt->poly_box, lives_notebook_get_nth_page(LIVES_NOTEBOOK(mt->nb), page));
    break;
  case POLY_FX_STACK:
    if (mt->poly_state != POLY_FX_STACK) polymorph(mt, POLY_FX_STACK);
    else lives_widget_reparent(mt->poly_box, lives_notebook_get_nth_page(LIVES_NOTEBOOK(mt->nb), page));
    break;
  case POLY_EFFECTS:
    if (mt->block_selected == NULL && mt->poly_state != POLY_EFFECTS) {
      notebook_error(LIVES_NOTEBOOK(mt->nb), tab, NB_ERROR_SEL, mt);
      return FALSE;
    }
    if (mt->poly_state != POLY_EFFECTS) polymorph(mt, POLY_EFFECTS);
    else lives_widget_reparent(mt->poly_box, lives_notebook_get_nth_page(LIVES_NOTEBOOK(mt->nb), page));
    break;
  case POLY_TRANS:
    if (lives_list_length(mt->selected_tracks) != 2 || mt->region_start == mt->region_end) {
      notebook_error(LIVES_NOTEBOOK(mt->nb), tab, NB_ERROR_NOTRANS, mt);
      return FALSE;
    }
    if (mt->poly_state != POLY_TRANS) polymorph(mt, POLY_TRANS);
    else {
      lives_widget_reparent(mt->poly_box, lives_notebook_get_nth_page(LIVES_NOTEBOOK(mt->nb), page));
    }
    break;
  case POLY_COMP:
    if (mt->selected_tracks == NULL || mt->region_start == mt->region_end) {
      notebook_error(LIVES_NOTEBOOK(mt->nb), tab, NB_ERROR_NOCOMP, mt);
      return FALSE;
    }
    if (mt->poly_state != POLY_COMP) polymorph(mt, POLY_COMP);
    else lives_widget_reparent(mt->poly_box, lives_notebook_get_nth_page(LIVES_NOTEBOOK(mt->nb), page));
    break;
  case POLY_PARAMS:
    if (mt->poly_state != POLY_PARAMS && mt->selected_init_event == NULL) {
      notebook_error(LIVES_NOTEBOOK(mt->nb), tab, NB_ERROR_NOEFFECT, mt);
      return FALSE;
    }
    lives_widget_reparent(mt->poly_box, lives_notebook_get_nth_page(LIVES_NOTEBOOK(mt->nb), page));
    if (mt->selected_init_event != NULL && mt->poly_state != POLY_PARAMS) {
      fubar(mt);
      polymorph(mt, POLY_PARAMS);
    }
    break;
  }

  return TRUE;
}


void set_poly_tab(lives_mt * mt, uint32_t tab) {
  int page = poly_tab_to_page(tab);
  lives_widget_show(lives_notebook_get_nth_page(LIVES_NOTEBOOK(mt->nb), page));
  if (page != lives_notebook_get_current_page(LIVES_NOTEBOOK(mt->nb))) {
    lives_notebook_set_current_page(LIVES_NOTEBOOK(mt->nb), page);
  } else {
    notebook_page(mt->nb, lives_notebook_get_nth_page(LIVES_NOTEBOOK(mt->nb), page), page, mt);
  }
}


static void select_block(lives_mt * mt) {
  track_rect *block = mt->putative_block;
  int track;
  int filenum;
  char *tmp, *tmp2;

  if (block != NULL) {
    LiVESWidget *eventbox = block->eventbox;

    if (cfile->achans == 0 || mt->audio_draws == NULL || (mt->opts.back_audio_tracks == 0 || eventbox != mt->audio_draws->data))
      track = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "layer_number"));
    else track = -1;

    if (!is_audio_eventbox(eventbox)) filenum = get_frame_event_clip(block->start_event, track);
    else filenum = get_audio_frame_clip(block->start_event, track);
    block->state = BLOCK_SELECTED;
    mt->block_selected = block;

    clear_context(mt);

    if (cfile->achans == 0 || mt->audio_draws == NULL || (mt->opts.back_audio_tracks == 0 || eventbox != mt->audio_draws->data))
      add_context_label(mt, (tmp2 = lives_strdup_printf(_("Current track: %s (layer %d)\n"),
                                    lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "track_name"), track)));
    else add_context_label(mt, (tmp2 = lives_strdup(_("Current track: Backing audio\n"))));
    lives_free(tmp2);

    add_context_label(mt, (tmp2 = lives_strdup_printf(_("%.2f sec. to %.2f sec.\n"),
                                  get_event_timecode(block->start_event) / TICKS_PER_SECOND_DBL,
                                  get_event_timecode(block->end_event) / TICKS_PER_SECOND_DBL + 1. / mt->fps)));
    lives_free(tmp2);
    add_context_label(mt, (tmp2 = lives_strdup_printf(_("Source: %s"), (tmp = get_menu_name(mainw->files[filenum], FALSE)))));
    lives_free(tmp);
    lives_free(tmp2);
    add_context_label(mt, (_("Right click for context menu.\n")));
    add_context_label(mt, (_("Single click on timeline\nto select a frame.\n")));

    lives_widget_set_sensitive(mt->view_in_out, TRUE);
    lives_widget_set_sensitive(mt->fx_block, TRUE);

    lives_widget_set_sensitive(mt->fx_blockv, TRUE);
    if (cfile->achans > 0) lives_widget_set_sensitive(mt->fx_blocka, TRUE);

    redraw_eventbox(mt, eventbox);

    multitrack_view_in_out(NULL, mt);
    paint_lines(mt, mt->ptr_time, TRUE);
  }

  mt->context_time = -1.;
}


LIVES_LOCAL_INLINE int pkg_in_list(char *pkgstring) {
  return lives_list_strcmp_index(pkg_list, pkgstring, FALSE) + 1;
}


LIVES_LOCAL_INLINE int add_to_pkg_list(char *pkgstring) {
  pkg_list = lives_list_append(pkg_list, pkgstring);
  return lives_list_length(pkg_list);
}

LIVES_LOCAL_INLINE char *get_pkg_name(int pkgnum) {
  return strdup(lives_list_nth_data(pkg_list, pkgnum - 1));
}


LIVES_LOCAL_INLINE void free_pkg_list(void) {
  lives_list_free_all((LiVESList **)&pkg_list);
}

static void populate_filter_box(int ninchans, lives_mt * mt, int pkgnum);

static boolean filter_ebox_pressed(LiVESWidget * eventbox, LiVESXEventButton * event, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;

  int pkgnum;

  if (mt->is_rendering) return FALSE;

  if ((pkgnum = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "pkgnum"))) != 0) {
    populate_filter_box(0, mt, pkgnum);
    return FALSE;
  }

  mt->selected_filter = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "fxid"));

  if (event->type != LIVES_BUTTON_PRESS) {
    // double click
    return FALSE;
  }

  if (!LIVES_IS_PLAYING) {
    // change cursor to mini block
    if (mt->video_draws == NULL && mt->audio_draws == NULL) {
      return FALSE;
    } else {
      mt_set_cursor_style(mt, LIVES_CURSOR_FX_BLOCK, FX_BLOCK_WIDTH, FX_BLOCK_HEIGHT, 0, 0, FX_BLOCK_HEIGHT / 2);
      mt->hotspot_x = mt->hotspot_y = 0;
    }
  }

  return FALSE;
}


static boolean on_drag_filter_end(LiVESWidget * widget, LiVESXEventButton * event, livespointer user_data) {
  LiVESXWindow *window;
  LiVESWidget *eventbox = NULL, *oeventbox;
  LiVESWidget *labelbox;
  LiVESWidget *ahbox;
  LiVESList *list;
  lives_mt *mt = (lives_mt *)user_data;
  double timesecs = 0.;
  int win_x, win_y, nins;
  int tchan = 0;
  boolean ok = FALSE;

  if (mt->cursor_style != LIVES_CURSOR_FX_BLOCK) {
    mt->selected_filter = -1;
    return FALSE;
  }

  lives_set_cursor_style(LIVES_CURSOR_NORMAL, NULL);

  if (mt->is_rendering || LIVES_IS_PLAYING || mt->selected_filter == -1) {
    mt->selected_filter = -1;
    return FALSE;
  }

  window = lives_display_get_window_at_pointer
           ((LiVESXDevice *)mainw->mgeom[widget_opts.monitor].mouse_device,
            mt->display, &win_x, &win_y);

  if (cfile->achans > 0 && enabled_in_channels(get_weed_filter(mt->selected_filter), TRUE) == 1) {
    for (list = mt->audio_draws; list != NULL; list = list->next) {
      eventbox = (LiVESWidget *)list->data;
      if (LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "hidden")) != 0) continue;
      labelbox = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "labelbox");
      if (lives_widget_get_xwindow(eventbox) == window || lives_widget_get_xwindow(labelbox) == window) {
        if (lives_widget_get_xwindow(labelbox) != window) {
          lives_widget_get_pointer((LiVESXDevice *)mainw->mgeom[prefs->gui_monitor > 0 ? prefs->gui_monitor - 1 : 0].mouse_device,
                                   mt->timeline, &mt->sel_x, &mt->sel_y);
          timesecs = get_time_from_x(mt, mt->sel_x);
        }
        tchan = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "layer_number"));
        if ((oeventbox = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "owner")) != NULL) {
          eventbox = oeventbox;
        }
        ok = TRUE;
        break;
      }
    }
  }

  if (!ok) {
    for (list = mt->video_draws; list != NULL; list = list->next) {
      eventbox = (LiVESWidget *)list->data;
      if (LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "hidden")) != 0) continue;
      labelbox = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "labelbox");
      ahbox = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "ahbox");
      if (lives_widget_get_xwindow(eventbox) == window || lives_widget_get_xwindow(labelbox) == window ||
          lives_widget_get_xwindow(ahbox) == window) {
        if (lives_widget_get_xwindow(labelbox) != window) {
          lives_widget_get_pointer((LiVESXDevice *)mainw->mgeom[prefs->gui_monitor > 0 ? prefs->gui_monitor - 1 : 0].mouse_device,
                                   mt->timeline, &mt->sel_x, &mt->sel_y);
          timesecs = get_time_from_x(mt, mt->sel_x);
        }
        tchan = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "layer_number"));
        ok = TRUE;
        break;
      }
    }
  }

  if (!ok) {
    mt->selected_filter = -1;
    return FALSE;
  }

  mt->current_fx = mt->selected_filter;
  mt->selected_filter = -1;

  // create dummy menuitem, needed in order to pass idx value
  dummy_menuitem = lives_standard_menu_item_new();
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(dummy_menuitem), "idx", LIVES_INT_TO_POINTER(mt->current_fx));
  lives_widget_object_ref_sink(dummy_menuitem);

  nins = enabled_in_channels(get_weed_filter(mt->current_fx), TRUE);
  if (nins == 1) {
    /*    // filter - either we drop on a region or on a block
      if (lives_list_length(mt->selected_tracks)==1&&mt->region_start!=mt->region_end) {
      // apply to region
      mt_add_region_effect(LIVES_MENU_ITEM(menuitem),mt);
      }
      else {*/
    // always apply to block
    track_rect *block;

    if (tchan == -1 && !is_pure_audio(get_weed_filter(mt->current_fx), FALSE)) {
      // can only apply audio filters to backing audio
      lives_widget_object_unref(dummy_menuitem);
      return FALSE;
    }

    block = get_block_from_time(eventbox, timesecs, mt);
    if (block == NULL) {
      lives_widget_object_unref(dummy_menuitem);
      return FALSE;
    }
    nb_ignore = TRUE;
    unselect_all(mt);
    mt->putative_block = block;

    mt->current_track = get_track_for_block(mt->putative_block);
    if (mt->current_track < 0) mt->aud_track_selected = TRUE;
    else mt->aud_track_selected = FALSE;
    track_select(mt);

    select_block(mt);
    nb_ignore = FALSE;
    // apply to block
    mt->putative_block = NULL;
    lives_timer_add(0, mt_add_block_effect_idle, mt); // work around issue in gtk+
  } else if (nins == 2) {
    // transition
    if (lives_list_length(mt->selected_tracks) == 2 && mt->region_start != mt->region_end) {
      // apply to region
      lives_timer_add(0, mt_add_region_effect_idle, mt);
    }
  } else if (nins >= 1000000) {
    // compositor
    if (mt->selected_tracks != NULL && mt->region_start != mt->region_end) {
      // apply to region
      lives_timer_add(0, mt_add_region_effect_idle, mt);
    }
  }

  //lives_widget_destroy(menuitem);
  return FALSE;
}


static void add_to_listbox(lives_mt * mt, LiVESWidget * xeventbox, char *fname, boolean add_top) {
  LiVESWidget *label;
  int offswidth = lives_widget_get_allocation_width(mt->nb) / 3.;

  lives_widget_add_events(xeventbox, LIVES_BUTTON_RELEASE_MASK | LIVES_BUTTON_PRESS_MASK);
  if (palette->style & STYLE_1) {
    lives_widget_set_bg_color(xeventbox, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
  }

  lives_container_set_border_width(LIVES_CONTAINER(xeventbox), widget_opts.border_width >> 1);
  widget_opts.mnemonic_label = FALSE;
  label = lives_standard_label_new(fname);
  widget_opts.mnemonic_label = TRUE;

  if (palette->style & STYLE_1) {
    lives_widget_set_fg_color(label, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
    lives_widget_set_fg_color(xeventbox, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
    lives_widget_set_can_focus(xeventbox, TRUE);
  }

  lives_container_add(LIVES_CONTAINER(xeventbox), label);
  lives_widget_set_margin_left(label, offswidth);

  // pack pkgs and a/v transitions first

  if (add_top) lives_box_pack_top(LIVES_BOX(mt->fx_list_vbox), xeventbox, FALSE, FALSE, 0);
  else lives_box_pack_start(LIVES_BOX(mt->fx_list_vbox), xeventbox, TRUE, FALSE, 0);

  lives_signal_connect(LIVES_GUI_OBJECT(xeventbox), LIVES_WIDGET_BUTTON_PRESS_EVENT,
                       LIVES_GUI_CALLBACK(filter_ebox_pressed),
                       (livespointer)mt);
  lives_signal_connect(LIVES_GUI_OBJECT(xeventbox), LIVES_WIDGET_BUTTON_RELEASE_EVENT,
                       LIVES_GUI_CALLBACK(on_drag_filter_end),
                       (livespointer)mt);
}


static void populate_filter_box(int ninchans, lives_mt * mt, int pkgnum) {
  static int oxninchans = 0;

  LiVESWidget *eventbox = NULL, *xeventbox;
  char *fname, *pkgstring = NULL, *pkg_name = NULL, *catstring;

  int nfilts = rte_get_numfilters();
  int nins;

  register int i;

  if (mt->fx_list_scroll != NULL) lives_widget_destroy(mt->fx_list_scroll);
  mt->fx_list_scroll = lives_scrolled_window_new(NULL, NULL);
  lives_scrolled_window_set_policy(LIVES_SCROLLED_WINDOW(mt->fx_list_scroll), LIVES_POLICY_AUTOMATIC, LIVES_POLICY_AUTOMATIC);
  lives_box_pack_start(LIVES_BOX(mt->fx_list_box), mt->fx_list_scroll, TRUE, TRUE, 0);

  mt->fx_list_vbox = lives_vbox_new(FALSE, widget_opts.packing_height);
  lives_container_set_border_width(LIVES_CONTAINER(mt->fx_list_vbox), widget_opts.border_width);
  lives_scrolled_window_add_with_viewport(LIVES_SCROLLED_WINDOW(mt->fx_list_scroll), mt->fx_list_vbox);
  lives_widget_set_bg_color(lives_bin_get_child(LIVES_BIN(mt->fx_list_scroll)), LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
  lives_widget_set_fg_color(mt->fx_list_vbox, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
  lives_widget_show_all(mt->fx_list_scroll);

  if (mt->block_selected == NULL && ninchans == 1) return;

  if (mt->block_selected != NULL) eventbox = mt->block_selected->eventbox;

  if (pkgnum != 0) {
    ninchans = oxninchans;
    if (pkgnum == -1) pkgnum = 0;
    else pkg_name = get_pkg_name(pkgnum);
  }

  if (pkgnum == 0) free_pkg_list();

  oxninchans = ninchans;

  catstring = (ninchans == 1 ? lives_fx_cat_to_text(LIVES_FX_CAT_EFFECT, TRUE) :
               ninchans == 2 ? lives_fx_cat_to_text(LIVES_FX_CAT_TRANSITION, TRUE) :
               lives_fx_cat_to_text(LIVES_FX_CAT_COMPOSITOR, TRUE));

  for (i = 0; i < nfilts; i++) {
    int sorted = weed_get_sorted_filter(i);
    weed_plant_t *filter = get_weed_filter(sorted);
    if (filter != NULL && !weed_plant_has_leaf(filter, WEED_LEAF_HOST_MENU_HIDE)) {

      if ((is_pure_audio(filter, FALSE) && (eventbox == NULL || !is_audio_eventbox(eventbox))) ||
          (!is_pure_audio(filter, FALSE) && eventbox != NULL && is_audio_eventbox(eventbox))) continue;

      if (weed_filter_hints_unstable(filter) && !prefs->unstable_fx) continue;

      nins = enabled_in_channels(filter, TRUE);

      if ((nins == ninchans || (ninchans == 1000000 && nins >= ninchans)) && enabled_out_channels(filter, FALSE) == 1) {
        pkgnum = 0;
        fname = weed_filter_idx_get_name(sorted, TRUE, TRUE, TRUE);

        if ((pkgstring = weed_filter_idx_get_package_name(sorted)) != NULL) {
          // filter is in package
          if (pkg_name != NULL && strcmp(pkgstring, pkg_name)) {
            // but wrong one
            lives_free(fname);
            lives_freep((void **)&pkgstring);
            continue;
          }
          if (pkg_name == NULL || !strlen(pkg_name)) {
            // no pkg requested
            if (!(pkgnum = pkg_in_list(pkgstring))) {
              // if this is the first for this package, add to list and show it
              lives_free(fname);
              fname = lives_strdup_printf(_("%s from %s package (CLICK TO SHOW)     ---->"), catstring, pkgstring);
              pkgnum = add_to_pkg_list(pkgstring); // list will free the string later
              pkgstring = NULL;
            } else {
              // pkg already in list, skip
              lives_free(fname);
              lives_freep((void **)&pkgstring);
              continue;
            }
          }
          // pkg matched
        }

        if (!pkgnum) {
          // filter is in no package
          if (pkg_name != NULL && strlen(pkg_name) && pkgstring == NULL) {
            // skip if pkg was requested
            lives_free(fname);
            continue;
          }
          fname = weed_filter_idx_get_name(sorted, TRUE, TRUE, TRUE);
        }

        xeventbox = lives_event_box_new();
        lives_widget_object_set_data(LIVES_WIDGET_OBJECT(xeventbox), "fxid", LIVES_INT_TO_POINTER(sorted));
        lives_widget_object_set_data(LIVES_WIDGET_OBJECT(xeventbox), "pkgnum", LIVES_INT_TO_POINTER(pkgnum));

        //add_to_listbox(mt, xeventbox, fname, (pkgnum != 0 || get_transition_param(filter, FALSE) == -1 || !has_video_chans_in(filter, FALSE)));
        add_to_listbox(mt, xeventbox, fname, FALSE);
        lives_free(fname);
      }
    }
  }
  if (pkg_name != NULL) {
    char *tmp;
    xeventbox = lives_event_box_new();
    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(xeventbox), "fxid", LIVES_INT_TO_POINTER(0));
    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(xeventbox), "pkgnum", LIVES_INT_TO_POINTER(-1));
    fname = lives_strdup_printf(_("<----     SHOW ALL %s"), (tmp = lives_utf8_strup(catstring, -1)));
    lives_free(tmp);
    add_to_listbox(mt, xeventbox, fname, TRUE);
  }
  lives_free(catstring);
  lives_freep((void **)&pkg_name);
  lives_widget_show_all(mt->fx_list_box);
}


static track_rect *mt_selblock(LiVESMenuItem * menuitem, livespointer user_data) {
  // ctrl-Enter - select block at current time/track
  lives_mt *mt = (lives_mt *)user_data;
  LiVESWidget *eventbox;
  double timesecs = mt->ptr_time;
  boolean desel = TRUE;

  if (mt->current_track == -1 || mt->aud_track_selected)
    eventbox = (LiVESWidget *)lives_list_nth_data(mt->audio_draws, mt->current_track + mt->opts.back_audio_tracks);
  else eventbox = (LiVESWidget *)lives_list_nth_data(mt->video_draws, mt->current_track);

  if (menuitem == NULL) return get_block_from_time(eventbox, timesecs, mt);

  mt->putative_block = get_block_from_time(eventbox, timesecs, mt);

  if (mt->putative_block != NULL && mt->putative_block->state == BLOCK_UNSELECTED) desel = FALSE;

  unselect_all(mt);
  if (!desel) select_block((lives_mt *)user_data);

  return mt->putative_block;
}


void mt_center_on_cursor(LiVESMenuItem * menuitem, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  mt_zoom(mt, 1.);
  paint_lines(mt, mt->ptr_time, TRUE);
}


void mt_zoom_in(LiVESMenuItem * menuitem, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  mt_zoom(mt, 0.5);
  if (LIVES_IS_PLAYING && mt->opts.follow_playback) {
    mt_zoom(mt, 1.);
  }
  paint_lines(mt, mt->ptr_time, TRUE);
}


void mt_zoom_out(LiVESMenuItem * menuitem, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  mt_zoom(mt, 2.);
  if (LIVES_IS_PLAYING && mt->opts.follow_playback) {
    mt_zoom(mt, 1.);
  }
  paint_lines(mt, mt->ptr_time, TRUE);
}


void paned_position(LiVESWidgetObject * object, livespointer pspec, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  mt->opts.vpaned_pos = lives_paned_get_position(LIVES_PANED(object));
}


static void hpaned_position(LiVESWidgetObject * object, livespointer pspec, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  mt->opts.hpaned_pos = lives_paned_get_position(LIVES_PANED(object));
}


static void no_time_selected(lives_mt * mt) {
  clear_context(mt);
  add_context_label(mt, _("You can click and drag\nbelow the timeline"));
  add_context_label(mt, _("to select a time region.\n"));
}


void mt_spin_start_value_changed(LiVESSpinButton * spinbutton, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  boolean has_region = (mt->region_start != mt->region_end);

  if (!mainw->interactive) return;

  lives_signal_handler_block(mt->spinbutton_start, mt->spin_start_func);
  mt->region_start = q_dbl(lives_spin_button_get_value(spinbutton), mt->fps) / TICKS_PER_SECOND_DBL;
  lives_spin_button_set_value(spinbutton, mt->region_start);
  lives_spin_button_set_range(LIVES_SPIN_BUTTON(mt->spinbutton_end), mt->region_start, mt->end_secs);
  lives_widget_queue_draw(mt->timeline_reg);
  lives_widget_process_updates(mt->timeline_reg, FALSE);
  draw_region(mt);
  do_sel_context(mt);

  if ((((mt->region_start != mt->region_end && !has_region) || (mt->region_start == mt->region_end && has_region))) &&
      mt->event_list != NULL && get_first_event(mt->event_list) != NULL) {
    lives_mt_poly_state_t statep = get_poly_state_from_page(mt);
    if (mt->selected_tracks != NULL) {
      lives_widget_set_sensitive(mt->split_sel, mt_selblock(NULL, (livespointer)mt) != NULL);
      if (mt->region_start != mt->region_end) {
        lives_widget_set_sensitive(mt->playsel, TRUE);
        lives_widget_set_sensitive(mt->ins_gap_sel, TRUE);
        lives_widget_set_sensitive(mt->remove_first_gaps, TRUE);
        lives_widget_set_sensitive(mt->fx_region, TRUE);
        switch (lives_list_length(mt->selected_tracks)) {
        case 1:
          lives_widget_set_sensitive(mt->fx_region_2av, FALSE);
          lives_widget_set_sensitive(mt->fx_region_2v, FALSE);
          lives_widget_set_sensitive(mt->fx_region_2a, FALSE);
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
      // update labels
      if (statep == POLY_TRANS || statep == POLY_COMP) {
        polymorph(mt, POLY_NONE);
        polymorph(mt, statep);
      }
    }
  }

  if (mt->region_start == mt->region_end) no_time_selected(mt);

  lives_signal_handler_unblock(mt->spinbutton_start, mt->spin_start_func);
}


void mt_spin_end_value_changed(LiVESSpinButton * spinbutton, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  boolean has_region = (mt->region_start != mt->region_end);

  if (!mainw->interactive) return;

  lives_signal_handler_block(mt->spinbutton_end, mt->spin_end_func);
  mt->region_end = q_dbl(lives_spin_button_get_value(spinbutton), mt->fps) / TICKS_PER_SECOND_DBL;
  lives_spin_button_set_value(spinbutton, mt->region_end);
  lives_spin_button_set_range(LIVES_SPIN_BUTTON(mt->spinbutton_start), 0., mt->region_end);
  lives_widget_queue_draw(mt->timeline_reg);
  lives_widget_process_updates(mt->timeline_reg, FALSE);
  draw_region(mt);
  do_sel_context(mt);

  if ((((mt->region_start != mt->region_end && !has_region) || (mt->region_start == mt->region_end && has_region))) &&
      mt->event_list != NULL && get_first_event(mt->event_list) != NULL) {
    lives_mt_poly_state_t statep = get_poly_state_from_page(mt);
    if (mt->selected_tracks != NULL) {
      lives_widget_set_sensitive(mt->split_sel, TRUE);
      if (mt->region_start != mt->region_end) {
        lives_widget_set_sensitive(mt->playsel, TRUE);
        lives_widget_set_sensitive(mt->ins_gap_sel, TRUE);
        lives_widget_set_sensitive(mt->remove_gaps, TRUE);
        lives_widget_set_sensitive(mt->remove_first_gaps, TRUE);
        lives_widget_set_sensitive(mt->fx_region, TRUE);
        switch (lives_list_length(mt->selected_tracks)) {
        case 1:
          if (cfile->achans == 0) lives_widget_set_sensitive(mt->fx_region_a, FALSE);
          lives_widget_set_sensitive(mt->fx_region_2a, FALSE);
          lives_widget_set_sensitive(mt->fx_region_2v, FALSE);
          lives_widget_set_sensitive(mt->fx_region_2av, FALSE);
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
      // update labels
      if (statep == POLY_TRANS || statep == POLY_COMP) {
        polymorph(mt, POLY_NONE);
        polymorph(mt, statep);
      }
    }
  }

  if (mt->region_start == mt->region_end) no_time_selected(mt);

  lives_signal_handler_unblock(mt->spinbutton_end, mt->spin_end_func);
}


static boolean in_out_ebox_pressed(LiVESWidget * eventbox, LiVESXEventButton * event, livespointer user_data) {
  int height;
  double width;
  int ebwidth;
  lives_clip_t *sfile;
  int file;
  lives_mt *mt = (lives_mt *)user_data;

  if (!mainw->interactive) return FALSE;

  if (mt->block_selected != NULL) return FALSE;

  ebwidth = lives_widget_get_allocation_width(mt->timeline);
  file = mt_file_from_clip(mt, mt->clip_selected);
  sfile = mainw->files[file];

  // change cursor to block
  if (mt->video_draws == NULL && mt->audio_draws == NULL) {
    return FALSE;
  } else {
    if (sfile->frames > 0) {
      if (!mt->opts.ign_ins_sel) {
        width = (sfile->end - sfile->start + 1.) / sfile->fps;
      } else {
        width = sfile->frames / sfile->fps;
      }
    } else width = sfile->laudio_time;
    if (width == 0) return FALSE;
    width = width / (mt->tl_max - mt->tl_min) * (double)ebwidth;
    if (width > ebwidth) width = ebwidth;
    if (width < 2) width = 2;
    height = get_track_height(mt);

    lives_set_cursor_style(LIVES_CURSOR_NORMAL, eventbox);
    mt_set_cursor_style(mt, LIVES_CURSOR_BLOCK, width, height, file, 0, height / 2);

    mt->hotspot_x = mt->hotspot_y = 0;
  }

  return FALSE;
}


static void do_clip_context(lives_mt * mt, LiVESXEventButton * event, lives_clip_t *sfile) {
  // pop up a context menu when clip is right clicked on

  LiVESWidget *edit_start_end, *edit_clipedit, *close_clip, *show_clipinfo;
  LiVESWidget *menu = lives_menu_new();

  if (!mainw->interactive) return;

  lives_menu_set_title(LIVES_MENU(menu), _("Selected Clip"));

  if (sfile->frames > 0) {
    edit_start_end = lives_standard_menu_item_new_with_label(_("_Adjust Start and End Points"));
    lives_signal_connect(LIVES_GUI_OBJECT(edit_start_end), LIVES_WIDGET_ACTIVATE_SIGNAL,
                         LIVES_GUI_CALLBACK(edit_start_end_cb),
                         (livespointer)mt);

    lives_container_add(LIVES_CONTAINER(menu), edit_start_end);
  }

  edit_clipedit = lives_standard_menu_item_new_with_label(_("_Edit/Encode in Clip Editor"));
  lives_signal_connect(LIVES_GUI_OBJECT(edit_clipedit), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(multitrack_end_cb),
                       (livespointer)mt);

  lives_container_add(LIVES_CONTAINER(menu), edit_clipedit);

  show_clipinfo = lives_standard_menu_item_new_with_label(_("_Show Clip Information"));
  lives_signal_connect(LIVES_GUI_OBJECT(show_clipinfo), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(show_clipinfo_cb),
                       (livespointer)mt);

  lives_container_add(LIVES_CONTAINER(menu), show_clipinfo);

  close_clip = lives_standard_menu_item_new_with_label(_("_Close this Clip"));
  lives_signal_connect(LIVES_GUI_OBJECT(close_clip), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(close_clip_cb),
                       (livespointer)mt);

  lives_container_add(LIVES_CONTAINER(menu), close_clip);

  if (palette->style & STYLE_1) {
    set_child_alt_colour(menu, TRUE);
    lives_widget_set_bg_color(menu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
    lives_widget_set_fg_color(menu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars_fore);
  }

  lives_widget_show_all(menu);
  lives_menu_popup(LIVES_MENU(menu), event);
}


static boolean clip_ebox_pressed(LiVESWidget * eventbox, LiVESXEventButton * event, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  lives_clip_t *sfile;
  double width;
  int height;
  int ebwidth;
  int file;

  if (!mt->is_ready) return FALSE;

  if (!mainw->interactive) return FALSE;

  if (event->type != LIVES_BUTTON_PRESS && !mt->is_rendering) {
    lives_set_cursor_style(LIVES_CURSOR_NORMAL, NULL);
    // double click, open up in clip editor
    if (!LIVES_IS_PLAYING) multitrack_delete(mt, !(prefs->warning_mask & WARN_MASK_EXIT_MT));
    return FALSE;
  }

  mt->clip_selected = get_box_child_index(LIVES_BOX(mt->clip_inner_box), eventbox);
  mt_clip_select(mt, FALSE);

  ebwidth = lives_widget_get_allocation_width(mt->timeline);
  file = mt_file_from_clip(mt, mt->clip_selected);
  sfile = mainw->files[file];

  if (event->button == 3) {
    double timesecs;
    lives_widget_get_pointer((LiVESXDevice *)mainw->mgeom[prefs->gui_monitor > 0 ? prefs->gui_monitor - 1 : 0].mouse_device,
                             mt->timeline, &mt->sel_x, &mt->sel_y);
    timesecs = get_time_from_x(mt, mt->sel_x);
    lives_ruler_set_value(LIVES_RULER(mt->timeline), timesecs);
    lives_widget_queue_draw(mt->timeline);
    do_clip_context(mt, event, sfile);
    return FALSE;
  }

  // change cursor to block
  if (mt->video_draws == NULL && mt->audio_draws == NULL) {
    return FALSE;
  } else {
    if (sfile->frames > 0) {
      if (!mt->opts.ign_ins_sel) {
        width = (sfile->end - sfile->start + 1.) / sfile->fps;
      } else {
        width = sfile->frames / sfile->fps;
      }
    } else width = sfile->laudio_time;
    if (width == 0) return FALSE;
    width = width / (mt->tl_max - mt->tl_min) * (double)ebwidth;
    if (width > ebwidth) width = ebwidth;
    if (width < 2) width = 2;
    height = get_track_height(mt);
    lives_set_cursor_style(LIVES_CURSOR_NORMAL, eventbox);
    mt_set_cursor_style(mt, LIVES_CURSOR_BLOCK, width, height, file, 0, height / 2);
    mt->hotspot_x = mt->hotspot_y = 0;
  }

  return FALSE;
}


static boolean on_drag_clip_end(LiVESWidget * widget, LiVESXEventButton * event, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;

  LiVESXWindow *window;
  LiVESWidget *eventbox;
  LiVESWidget *labelbox;
  LiVESWidget *ahbox;

  double timesecs, osecs;

  int win_x, win_y;

  if (!mainw->interactive) return FALSE;

  if (mt->is_rendering) return FALSE;

  if (mt->cursor_style != LIVES_CURSOR_BLOCK) return FALSE;

  osecs = mt->ptr_time;

  lives_set_cursor_style(LIVES_CURSOR_BUSY, NULL);

  window = lives_display_get_window_at_pointer
           ((LiVESXDevice *)mainw->mgeom[widget_opts.monitor].mouse_device,
            mt->display, &win_x, &win_y);

  if (cfile->achans > 0 && mt->opts.back_audio_tracks > 0 &&
      LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(mt->audio_draws->data), "hidden")) == 0) {
    labelbox = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(mt->audio_draws->data), "labelbox");
    ahbox = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(mt->audio_draws->data), "ahbox");

    if (lives_widget_get_xwindow(LIVES_WIDGET(mt->audio_draws->data)) == window ||
        lives_widget_get_xwindow(labelbox) == window || lives_widget_get_xwindow(ahbox) == window) {

      // insert in backing audio
      if (lives_widget_get_xwindow(labelbox) == window || lives_widget_get_xwindow(ahbox) == window) timesecs = 0.;
      else {
        lives_widget_get_pointer((LiVESXDevice *)mainw->mgeom[prefs->gui_monitor > 0 ? prefs->gui_monitor - 1 : 0].mouse_device,
                                 mt->timeline, &mt->sel_x, &mt->sel_y);
        timesecs = get_time_from_x(mt, mt->sel_x);
      }
      mt->current_track = -1;
      track_select(mt);

      if (!LIVES_IS_PLAYING) {
        mt->ptr_time = lives_ruler_set_value(LIVES_RULER(mt->timeline), timesecs);
        if (!mt->is_paused) {
          if (mt->poly_state == POLY_FX_STACK) {
            polymorph(mt, POLY_FX_STACK);
          }
          mt_show_current_frame(mt, FALSE);
          if (timesecs > 0.) {
            lives_widget_set_sensitive(mt->rewind, TRUE);
            lives_widget_set_sensitive(mainw->m_rewindbutton, TRUE);
          }
        }
        lives_widget_queue_draw(mt->timeline);
      }

      if (!LIVES_IS_PLAYING && (mainw->files[mt->file_selected]->laudio_time >
                                ((mainw->files[mt->file_selected]->start - 1.) / mainw->files[mt->file_selected]->fps) ||
                                (mainw->files[mt->file_selected]->laudio_time > 0. && mt->opts.ign_ins_sel)))
        insert_audio_here_cb(NULL, (livespointer)mt);
      lives_set_cursor_style(LIVES_CURSOR_NORMAL, NULL);
      if (mt->is_paused) mt->ptr_time = lives_ruler_set_value(LIVES_RULER(mt->timeline), osecs);
      return FALSE;
    }
  }

  for (LiVESList *list = mt->video_draws; list != NULL; list = list->next) {
    eventbox = (LiVESWidget *)list->data;
    if (LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "hidden")) != 0) continue;
    labelbox = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "labelbox");
    ahbox = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "ahbox");
    if (lives_widget_get_xwindow(eventbox) == window || lives_widget_get_xwindow(labelbox) == window ||
        lives_widget_get_xwindow(ahbox) == window) {
      if (lives_widget_get_xwindow(labelbox) == window || lives_widget_get_xwindow(ahbox) == window) timesecs = 0.;
      else {
        lives_widget_get_pointer((LiVESXDevice *)mainw->mgeom[prefs->gui_monitor > 0 ? prefs->gui_monitor - 1 : 0].mouse_device,
                                 mt->timeline, &mt->sel_x, &mt->sel_y);
        timesecs = get_time_from_x(mt, mt->sel_x);
      }
      mt->current_track = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "layer_number"));
      mt->aud_track_selected = FALSE;

      track_select(mt);

      if (!LIVES_IS_PLAYING) {
        mt->ptr_time = lives_ruler_set_value(LIVES_RULER(mt->timeline), timesecs);
        if (!mt->is_paused) {
          mt_show_current_frame(mt, FALSE);
          if (timesecs > 0.) {
            lives_widget_set_sensitive(mt->rewind, TRUE);
            lives_widget_set_sensitive(mainw->m_rewindbutton, TRUE);
          }
        }
        lives_widget_queue_draw(mt->timeline);
      }
      if (!LIVES_IS_PLAYING && mainw->files[mt->file_selected]->frames > 0) insert_here_cb(NULL, mt);
      break;
    }
  }

  if (mt->is_paused) mt->ptr_time = lives_ruler_set_value(LIVES_RULER(mt->timeline), osecs);

  lives_set_cursor_style(LIVES_CURSOR_NORMAL, NULL);

  return FALSE;
}


static boolean on_clipbox_enter(LiVESWidget * widget, LiVESXEventCrossing * event, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  if (mt->cursor_style != LIVES_CURSOR_NORMAL) return FALSE;
  lives_set_cursor_style(LIVES_CURSOR_HAND2, widget);
  return FALSE;
}


void mt_init_start_end_spins(lives_mt * mt) {
  LiVESWidget *hbox;

  int dpw = widget_opts.packing_width;

  hbox = lives_hbox_new(FALSE, 0);
  lives_widget_apply_theme(hbox, LIVES_WIDGET_STATE_NORMAL);

  lives_box_pack_start(LIVES_BOX(mt->xtravbox), hbox, FALSE, FALSE, 6);

  mt->amixb_eventbox = lives_event_box_new();
  lives_box_pack_start(LIVES_BOX(hbox), mt->amixb_eventbox, FALSE, FALSE, widget_opts.packing_width * 2);

  mt->btoolbar = lives_toolbar_new();
  lives_widget_apply_theme(mt->btoolbar, LIVES_WIDGET_STATE_NORMAL);
  lives_container_add(LIVES_CONTAINER(mt->amixb_eventbox), mt->btoolbar);

  lives_toolbar_set_show_arrow(LIVES_TOOLBAR(mt->btoolbar), FALSE);

  lives_toolbar_set_style(LIVES_TOOLBAR(mt->btoolbar), LIVES_TOOLBAR_TEXT);

  mt->amixer_button = lives_standard_tool_button_new(LIVES_TOOLBAR(mt->btoolbar), NULL, _("  Audio Mixer (ctrl-m)  "), NULL);

  mt->amix_label = widget_opts.last_label;

  lives_widget_add_accelerator(mt->amixer_button, LIVES_WIDGET_CLICKED_SIGNAL, mt->accel_group,
                               LIVES_KEY_m, LIVES_CONTROL_MASK,
                               LIVES_ACCEL_VISIBLE);

  if (cfile->achans == 0 || !mt->opts.pertrack_audio) lives_widget_set_sensitive(mt->amixer_button, FALSE);

  lives_signal_connect(LIVES_GUI_OBJECT(mt->amixer_button), LIVES_WIDGET_CLICKED_SIGNAL,
                       LIVES_GUI_CALLBACK(amixer_show),
                       (livespointer)mt);

  widget_opts.packing_width = MAIN_SPIN_SPACER;
  mt->spinbutton_start = lives_standard_spin_button_new(NULL, 0., 0., 10000000., 1. / mt->fps, 1. / mt->fps, 3,
                         NULL, NULL);
  lives_spin_button_set_snap_to_ticks(LIVES_SPIN_BUTTON(mt->spinbutton_start), TRUE);
  widget_opts.packing_width = dpw;
  lives_widget_set_valign(mt->spinbutton_start, LIVES_ALIGN_CENTER);

  lives_box_pack_start(LIVES_BOX(hbox), mt->spinbutton_start, TRUE, FALSE, MAIN_SPIN_SPACER);

  mt->l_sel_arrow = lives_arrow_new(LIVES_ARROW_LEFT, LIVES_SHADOW_OUT);
  lives_box_pack_start(LIVES_BOX(hbox), mt->l_sel_arrow, FALSE, FALSE, 0);

  lives_entry_set_width_chars(LIVES_ENTRY(mt->spinbutton_start), COMBOWIDTHCHARS);
  mt->sel_label = lives_standard_label_new(NULL);

  set_sel_label(mt->sel_label);
  lives_box_pack_start(LIVES_BOX(hbox), mt->sel_label, FALSE, FALSE, 0);

  mt->r_sel_arrow = lives_arrow_new(LIVES_ARROW_RIGHT, LIVES_SHADOW_OUT);
  lives_box_pack_start(LIVES_BOX(hbox), mt->r_sel_arrow, FALSE, FALSE, 3);

  widget_opts.packing_width = MAIN_SPIN_SPACER;
  mt->spinbutton_end = lives_standard_spin_button_new(NULL, 0., 0., 10000000., 1. / mt->fps, 1. / mt->fps, 3,
                       NULL, NULL);
  lives_spin_button_set_snap_to_ticks(LIVES_SPIN_BUTTON(mt->spinbutton_end), TRUE);
  lives_widget_set_valign(mt->spinbutton_end, LIVES_ALIGN_CENTER);

  widget_opts.packing_width = dpw;

  lives_entry_set_width_chars(LIVES_ENTRY(mt->spinbutton_end), COMBOWIDTHCHARS);

  lives_box_pack_start(LIVES_BOX(hbox), mt->spinbutton_end, TRUE, FALSE, MAIN_SPIN_SPACER);

  mt->spin_start_func = lives_signal_connect_after(LIVES_GUI_OBJECT(mt->spinbutton_start), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                        LIVES_GUI_CALLBACK(mt_spin_start_value_changed),
                        (livespointer)mt);

  mt->spin_end_func = lives_signal_connect_after(LIVES_GUI_OBJECT(mt->spinbutton_end), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                      LIVES_GUI_CALLBACK(mt_spin_end_value_changed),
                      (livespointer)mt);
}


void mouse_mode_context(lives_mt * mt) {
  char *fname, *text;
  clear_context(mt);

  if (mt->opts.mouse_mode == MOUSE_MODE_MOVE) {
    add_context_label(mt, (_("Single click on timeline")));
    add_context_label(mt, (_("to select a frame.")));
    add_context_label(mt, (_("Double click or right click on timeline")));
    add_context_label(mt, (_("to select a block.")));
    add_context_label(mt, (_("\nClips can be dragged")));
    add_context_label(mt, (_("onto the timeline.")));

    add_context_label(mt, (_("Mouse mode is: Move")));
    add_context_label(mt, (_("clips can be moved around.")));
  } else if (mt->opts.mouse_mode == MOUSE_MODE_SELECT) {
    add_context_label(mt, (_("Mouse mode is: Select.")));
    add_context_label(mt, (_("Drag with mouse on timeline")));
    add_context_label(mt, (_("to select tracks and time.")));
  }
  if (prefs->atrans_fx != -1) fname = weed_filter_idx_get_name(prefs->atrans_fx, FALSE, FALSE, FALSE);
  else fname = lives_strdup(mainw->string_constants[LIVES_STRING_CONSTANT_NONE]);
  add_context_label(mt, (_("\nAutotransition effect is:")));
  text = lives_strdup_printf("<b>%s</b>", fname);
  add_context_label(mt, (text));
  lives_free(text);
  lives_free(fname);
}


void update_insert_mode(lives_mt * mt) {
  const char *mtext = NULL;

  if (mt->opts.insert_mode == INSERT_MODE_NORMAL) {
    mtext = lives_menu_item_get_text(mt->ins_normal);
  }

  if (mt->ins_label == NULL) {
    lives_menu_item_set_text(mt->ins_menuitem, mtext, TRUE);
  } else {
    widget_opts.mnemonic_label = FALSE;
    lives_label_set_text(LIVES_LABEL(mt->ins_label), mtext);
    widget_opts.mnemonic_label = TRUE;
  }

  lives_signal_handler_block(mt->ins_normal, mt->ins_normal_func);
  lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mt->ins_normal), mt->opts.insert_mode == INSERT_MODE_NORMAL);
  lives_signal_handler_unblock(mt->ins_normal, mt->ins_normal_func);
}


static void on_insert_mode_changed(LiVESMenuItem * menuitem, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;

  if (!mainw->interactive) return;

  if (menuitem == (LiVESMenuItem *)mt->ins_normal) {
    mt->opts.insert_mode = INSERT_MODE_NORMAL;
  }
  update_insert_mode(mt);
}


static void on_mouse_mode_changed(LiVESMenuItem * menuitem, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  const char *text;

  if (!mainw->interactive) return;

  if (menuitem == (LiVESMenuItem *)mt->mm_move) {
    mt->opts.mouse_mode = MOUSE_MODE_MOVE;
  } else if (menuitem == (LiVESMenuItem *)mt->mm_select) {
    mt->opts.mouse_mode = MOUSE_MODE_SELECT;
  }

  text = lives_menu_item_get_text(LIVES_WIDGET(menuitem));

  if (mt->ins_label == NULL) {
    lives_menu_item_set_text(mt->mm_menuitem, text, TRUE);
  } else {
    widget_opts.mnemonic_label = FALSE;
    lives_label_set_text(LIVES_LABEL(mt->mm_label), text);
    widget_opts.mnemonic_label = TRUE;
  }

  mouse_mode_context(mt);

  lives_signal_handler_block(mt->mm_move, mt->mm_move_func);
  lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mt->mm_move), mt->opts.mouse_mode == MOUSE_MODE_MOVE);
  lives_signal_handler_unblock(mt->mm_move, mt->mm_move_func);

  lives_signal_handler_block(mt->mm_select, mt->mm_select_func);
  lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mt->mm_select), mt->opts.mouse_mode == MOUSE_MODE_SELECT);
  lives_signal_handler_unblock(mt->mm_select, mt->mm_select_func);
}


void update_grav_mode(lives_mt * mt) {
  // update GUI after grav mode change
  const char *mtext = NULL;

  if (mt->opts.grav_mode == GRAV_MODE_NORMAL) {
    mtext = lives_menu_item_get_text(mt->grav_normal);
  } else if (mt->opts.grav_mode == GRAV_MODE_LEFT) {
    mtext = lives_menu_item_get_text(mt->grav_left);
  }

  if (mt->opts.grav_mode == GRAV_MODE_RIGHT) {
    mtext = lives_menu_item_get_text(mt->grav_right);
    lives_menu_item_set_text(mt->remove_first_gaps, _("Close _last gap(s) in selected tracks/time"), TRUE);
  } else {
    lives_menu_item_set_text(mt->remove_first_gaps, _("Close _first gap(s) in selected tracks/time"), TRUE);
  }

  widget_opts.mnemonic_label = FALSE;
  lives_label_set_text(LIVES_LABEL(mt->grav_label), mtext);
  widget_opts.mnemonic_label = TRUE;

  lives_signal_handler_block(mt->grav_normal, mt->grav_normal_func);
  lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mt->grav_normal), mt->opts.grav_mode == GRAV_MODE_NORMAL);
  lives_signal_handler_unblock(mt->grav_normal, mt->grav_normal_func);

  lives_signal_handler_block(mt->grav_left, mt->grav_left_func);
  lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mt->grav_left), mt->opts.grav_mode == GRAV_MODE_LEFT);
  lives_signal_handler_unblock(mt->grav_left, mt->grav_left_func);

  lives_signal_handler_block(mt->grav_right, mt->grav_right_func);
  lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mt->grav_right), mt->opts.grav_mode == GRAV_MODE_RIGHT);
  lives_signal_handler_unblock(mt->grav_right, mt->grav_right_func);
}


static void on_grav_mode_changed(LiVESMenuItem * menuitem, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;

  if (!mainw->interactive) return;

  if (menuitem == (LiVESMenuItem *)mt->grav_normal) {
    mt->opts.grav_mode = GRAV_MODE_NORMAL;
  } else if (menuitem == (LiVESMenuItem *)mt->grav_left) {
    mt->opts.grav_mode = GRAV_MODE_LEFT;
  } else if (menuitem == (LiVESMenuItem *)mt->grav_right) {
    mt->opts.grav_mode = GRAV_MODE_RIGHT;
  }
  update_grav_mode(mt);
}


static size_t estimate_space(lives_mt * mt, int undo_type) {
  size_t needed = sizeof(mt_undo);

  switch (undo_type) {
  case MT_UNDO_NONE:
    break;
  default:
    needed += event_list_get_byte_size(mt, mt->event_list, NULL);
    break;
  }
  return needed;
}


static char *get_undo_text(int action, void *extra) {
  char *filtname, *ret;
  int error;

  switch (action) {
  case MT_UNDO_REMOVE_GAPS:
    return lives_strdup(_("Close gaps"));
  case MT_UNDO_MOVE_BLOCK:
    return lives_strdup(_("Move block"));
  case MT_UNDO_MOVE_AUDIO_BLOCK:
    return lives_strdup(_("Move audio block"));
  case MT_UNDO_DELETE_BLOCK:
    return lives_strdup(_("Delete block"));
  case MT_UNDO_DELETE_AUDIO_BLOCK:
    return lives_strdup(_("Delete audio block"));
  case MT_UNDO_SPLIT_MULTI:
    return lives_strdup(_("Split tracks"));
  case MT_UNDO_SPLIT:
    return lives_strdup(_("Split block"));
  case MT_UNDO_APPLY_FILTER:
    filtname = weed_get_string_value((weed_plant_t *)extra, WEED_LEAF_NAME, &error);
    ret = lives_strdup_printf(_("Apply %s"), filtname);
    lives_free(filtname);
    return ret;
  case MT_UNDO_DELETE_FILTER:
    filtname = weed_get_string_value((weed_plant_t *)extra, WEED_LEAF_NAME, &error);
    ret = lives_strdup_printf(_("Delete %s"), filtname);
    lives_free(filtname);
    return ret;
  case MT_UNDO_INSERT_BLOCK:
    return lives_strdup(_("Insert block"));
  case MT_UNDO_INSERT_GAP:
    return lives_strdup(_("Insert gap"));
  case MT_UNDO_INSERT_AUDIO_BLOCK:
    return lives_strdup(_("Insert audio block"));
  case MT_UNDO_FILTER_MAP_CHANGE:
    return lives_strdup(_("Effect order change"));
  }
  return lives_strdup("");
}


static void mt_set_undoable(lives_mt * mt, int what, void *extra, boolean sensitive) {
  mt->undoable = sensitive;
  if (what != MT_UNDO_NONE) {
    char *what_safe;
    char *text = get_undo_text(what, extra);
    what_safe = lives_strdelimit(lives_strdup(text), "_", ' ');
    lives_snprintf(mt->undo_text, 32, _("_Undo %s"), what_safe);

    lives_free(what_safe);
    lives_free(text);
  } else {
    mt->undoable = FALSE;
    lives_snprintf(mt->undo_text, 32, "%s", _("_Undo"));
  }
  lives_menu_item_set_text(mt->undo, mt->undo_text, TRUE);

  lives_widget_set_sensitive(mt->undo, sensitive);
}


static void mt_set_redoable(lives_mt * mt, int what, void *extra, boolean sensitive) {
  mt->redoable = sensitive;
  if (what != MT_UNDO_NONE) {
    char *what_safe;
    char *text = get_undo_text(what, extra);
    what_safe = lives_strdelimit(lives_strdup(text), "_", ' ');
    lives_snprintf(mt->redo_text, 32, _("_Redo %s"), what_safe);
    lives_free(what_safe);
    lives_free(text);
  } else {
    mt->redoable = FALSE;
    lives_snprintf(mt->redo_text, 32, "%s", _("_Redo"));
  }
  lives_menu_item_set_text(mt->redo, mt->redo_text, TRUE);

  lives_widget_set_sensitive(mt->redo, sensitive);
}


boolean make_backup_space(lives_mt * mt, size_t space_needed) {
  // read thru mt->undos and eliminate that space until we have space_needed
  size_t space_avail = (size_t)(prefs->mt_undo_buf * 1024 * 1024) - mt->undo_buffer_used;
  size_t space_freed = 0;
  size_t len;
  LiVESList *xundo = mt->undos, *ulist;
  int count = 0;
  mt_undo *undo;

  while (xundo != NULL) {
    count++;
    undo = (mt_undo *)(xundo->data);
    space_freed += undo->data_len;
    if ((space_avail + space_freed) >= space_needed) {
      lives_memmove(mt->undo_mem, mt->undo_mem + space_freed, mt->undo_buffer_used - space_freed);
      ulist = lives_list_copy(lives_list_nth(mt->undos, count));
      if (ulist != NULL) ulist->prev = NULL;
      lives_list_free(mt->undos);
      mt->undos = ulist;
      while (ulist != NULL) {
        ulist->data = (unsigned char *)(ulist->data) - space_freed;
        ulist = ulist->next;
      }
      mt->undo_buffer_used -= space_freed;
      if (mt->undo_offset > (len = lives_list_length(mt->undos))) {
        mt->undo_offset = len;
        mt_set_undoable(mt, MT_UNDO_NONE, NULL, FALSE);
        mt_set_redoable(mt, ((mt_undo *)(mt->undos->data))->action, NULL, TRUE);
      }
      return TRUE;
    }
    xundo = xundo->next;
  }
  mt->undo_buffer_used = 0;
  lives_list_free(mt->undos);
  mt->undos = NULL;
  mt->undo_offset = 0;
  mt_set_undoable(mt, MT_UNDO_NONE, NULL, FALSE);
  mt_set_redoable(mt, MT_UNDO_NONE, NULL, FALSE);
  return FALSE;
}


void mt_backup(lives_mt * mt, int undo_type, weed_timecode_t tc) {
  // backup an operation in the undo/redo list

  size_t space_needed = 0;
  mt_undo *undo;
  mt_undo *last_valid_undo;

  unsigned char *memblock;

  if (mt->did_backup) return;

  // top level caller MUST reset this to FALSE !
  mt->did_backup = mt->changed = TRUE;
  lives_widget_set_sensitive(mt->backup, TRUE);

  // ask caller to add the idle func
  if (prefs->mt_auto_back > 0) mt->auto_changed = TRUE;

  if (mt->undo_mem == NULL) return;

  if (mt->undos != NULL && mt->undo_offset != 0) {
    // invalidate redo's - we are backing up, so we can't redo any more
    // invalidate from lives_list_length-undo_offset onwards
    if ((lives_list_length(mt->undos)) == mt->undo_offset) {
      mt->undos = NULL;
      mt->undo_buffer_used = 0;
    } else {
      int i = 0;
      LiVESList *ulist = mt->undos;
      for (i = (int)lives_list_length(mt->undos) - mt->undo_offset; --i > 0; ulist = ulist->next);
      if (ulist != NULL) {
        memblock = (unsigned char *)ulist->data;
        last_valid_undo = (mt_undo *)memblock;
        memblock += last_valid_undo->data_len;
        mt->undo_buffer_used = memblock - mt->undo_mem;
        if (ulist->next != NULL) {
          ulist = ulist->next;
          ulist->prev->next = NULL;
          lives_list_free(ulist);
        }
      }
    }
    mt_set_redoable(mt, MT_UNDO_NONE, NULL, FALSE);
    mt->undo_offset = 0;
  }

  undo = (mt_undo *)lives_calloc(1, sizeof(mt_undo));
  undo->action = (lives_mt_undo_t)undo_type;
  undo->extra = NULL;

  switch (undo_type) {
  case MT_UNDO_NONE:
    break;
  case MT_UNDO_APPLY_FILTER:
  case MT_UNDO_DELETE_FILTER:
    undo->extra = get_weed_filter(mt->current_fx);
    break;
  case MT_UNDO_FILTER_MAP_CHANGE:
    undo->tc = tc;
    break;
  default:
    break;
  }

  add_markers(mt, mt->event_list, TRUE);
  if ((space_needed = estimate_space(mt, undo_type) + sizeof(mt_undo)) >
      ((size_t)(prefs->mt_undo_buf * 1024 * 1024) - mt->undo_buffer_used)) {
    if (!make_backup_space(mt, space_needed)) {
      remove_markers(mt->event_list);
      do_mt_backup_space_error(mt, (int)((space_needed * 3) >> 20));
      return;
    }
    memblock = (unsigned char *)(mt->undo_mem + mt->undo_buffer_used + sizeof(mt_undo));
  }

  undo->data_len = space_needed;
  memblock = (unsigned char *)(mt->undo_mem + mt->undo_buffer_used + sizeof(mt_undo));
  save_event_list_inner(NULL, 0, mt->event_list, &memblock);
  remove_markers(mt->event_list);

  lives_memcpy(mt->undo_mem + mt->undo_buffer_used, undo, sizeof(mt_undo));
  mt->undos = lives_list_append(mt->undos, mt->undo_mem + mt->undo_buffer_used);
  mt->undo_buffer_used += space_needed;
  mt_set_undoable(mt, undo->action, undo->extra, TRUE);
  lives_free(undo);
}


void mt_aparam_view_toggled(LiVESMenuItem * menuitem, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  LiVESList *list;
  int which = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(menuitem), "pnum"));

  if (lives_check_menu_item_get_active(LIVES_CHECK_MENU_ITEM(menuitem)))
    mt->opts.aparam_view_list = lives_list_append(mt->opts.aparam_view_list, LIVES_INT_TO_POINTER(which));
  else mt->opts.aparam_view_list = lives_list_remove(mt->opts.aparam_view_list, LIVES_INT_TO_POINTER(which));
  for (list = mt->audio_draws; list != NULL; list = list->next) {
    lives_widget_queue_draw((LiVESWidget *)list->data);
  }
}


static void destroy_widget(LiVESWidget * widget, livespointer user_data) {
  lives_widget_destroy(widget);
}


void add_aparam_menuitems(lives_mt * mt) {
  // add menuitems for avol_fx to the View/Audio parameters submenu
  LiVESWidget *menuitem;
  weed_plant_t *filter;
  lives_rfx_t *rfx;
  register int i;

  lives_container_foreach(LIVES_CONTAINER(mt->aparam_submenu), destroy_widget, NULL);

  if (mt->avol_fx == -1 || mt->audio_draws == NULL) {
    lives_widget_hide(mt->insa_checkbutton);
    lives_widget_hide(mt->aparam_separator);
    lives_widget_hide(mt->aparam_menuitem);
    lives_widget_hide(mt->aparam_submenu);

    lives_widget_hide(mt->render_aud);
    lives_widget_hide(mt->normalise_aud);
    lives_widget_hide(mt->render_vid);
    lives_widget_hide(mt->render_sep);

    if (mt->opts.aparam_view_list != NULL && mainw->multi_opts.aparam_view_list == NULL) {
      lives_list_free(mt->opts.aparam_view_list);
      mt->opts.aparam_view_list = NULL;
    }
    return;
  }
  if (mt->opts.pertrack_audio) {
    lives_widget_show(mt->insa_checkbutton);
  }

  lives_widget_show(mt->render_aud);
  lives_widget_show(mt->normalise_aud);
  lives_widget_show(mt->render_vid);
  lives_widget_show(mt->render_sep);

  //  lives_widget_show(mt->aparam_separator);
  lives_widget_show(mt->aparam_menuitem);
  lives_widget_show(mt->aparam_submenu);

  filter = get_weed_filter(mt->avol_fx);
  rfx = weed_to_rfx(filter, FALSE);
  widget_opts.mnemonic_label = FALSE;

  for (i = 0; i < rfx->num_params; i++) {
    // TODO - check rfx->params[i].multi
    if ((rfx->params[i].hidden | HIDDEN_MULTI) == HIDDEN_MULTI && rfx->params[i].type == LIVES_PARAM_NUM) {
      menuitem = lives_standard_check_menu_item_new_with_label(rfx->params[i].name,
                 mt->opts.aparam_view_list != NULL && lives_list_find(mt->opts.aparam_view_list, LIVES_INT_TO_POINTER(i)));
      lives_container_add(LIVES_CONTAINER(mt->aparam_submenu), menuitem);
      lives_widget_show(menuitem);
      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(menuitem), "pnum", LIVES_INT_TO_POINTER(i));
      lives_signal_connect(LIVES_GUI_OBJECT(menuitem), LIVES_WIDGET_ACTIVATE_SIGNAL,
                           LIVES_GUI_CALLBACK(mt_aparam_view_toggled),
                           (livespointer)mt);
    }
  }
  widget_opts.mnemonic_label = TRUE;
  rfx_free(rfx);
  lives_free(rfx);
}


static void apply_avol_filter(lives_mt * mt) {
  // apply audio volume effect from 0 to last frame event
  // since audio off occurs on a frame event this should cover the whole timeline

  weed_plant_t *init_event = mt->avol_init_event, *new_end_event;
  weed_plant_t *deinit_event;
  weed_timecode_t new_tc;
  LiVESList *list;

  register int i;

  if (mt->opts.back_audio_tracks == 0 && !mt->opts.pertrack_audio) return;

  new_end_event = get_last_frame_event(mt->event_list);

  if (new_end_event == NULL) {
    if (init_event != NULL) {
      remove_filter_from_event_list(mt->event_list, init_event);
      if (mt->opts.aparam_view_list != NULL) {
        for (list = mt->audio_draws; list != NULL; list = list->next) {
          lives_widget_queue_draw((LiVESWidget *)list->data);
        }
      }
    }
    return;
  }

  if (mt->opts.pertrack_audio) lives_widget_set_sensitive(mt->prerender_aud, TRUE);

  if (init_event == NULL) {
    LiVESList *slist = lives_list_copy(mt->selected_tracks);

    weed_plant_t *old_mt_init = mt->init_event;

    double region_start = mt->region_start;
    double region_end = mt->region_end;

    boolean did_backup = mt->did_backup;
    int current_fx = mt->current_fx;

    mt->region_start = 0.;
    mt->region_end = (get_event_timecode(new_end_event) + TICKS_PER_SECOND_DBL / mt->fps) / TICKS_PER_SECOND_DBL;
    if (mt->selected_tracks != NULL) {
      lives_list_free(mt->selected_tracks);
      mt->selected_tracks = NULL;
    }
    if (mt->opts.back_audio_tracks > 0) mt->selected_tracks = lives_list_append(mt->selected_tracks, LIVES_INT_TO_POINTER(-1));
    if (mt->opts.pertrack_audio) {
      for (i = 0; i < mt->num_video_tracks; i++) {
        mt->selected_tracks = lives_list_append(mt->selected_tracks, LIVES_INT_TO_POINTER(i));
      }
    }
    mt->current_fx = mt->avol_fx;

    mt->did_backup = TRUE; // this is a special internal event that we don't want to backup
    mt_add_region_effect(NULL, mt);
    mt->did_backup = did_backup;
    mt->avol_init_event = mt->init_event;

    mt->region_start = region_start;
    mt->region_end = region_end;
    lives_list_free(mt->selected_tracks);
    mt->selected_tracks = lives_list_copy(slist);
    if (slist != NULL) lives_list_free(slist);
    mt->current_fx = current_fx;
    mt->init_event = old_mt_init;

    if (mt->opts.aparam_view_list != NULL) {
      for (list = mt->audio_draws; list != NULL; list = list->next) {
        lives_widget_queue_draw((LiVESWidget *)list->data);
      }
    }
    return;
  }

  // init event is already there - we will move the deinit event to tc==new_end event
  deinit_event = weed_get_plantptr_value(init_event, WEED_LEAF_DEINIT_EVENT, NULL);
  new_tc = get_event_timecode(new_end_event);

  move_filter_deinit_event(mt->event_list, new_tc, deinit_event, mt->fps, FALSE);

  if (mt->opts.aparam_view_list != NULL) {
    for (list = mt->audio_draws; list != NULL; list = list->next) {
      lives_widget_queue_draw((LiVESWidget *)list->data);
    }
  }
}


static void set_audio_filter_channel_values(lives_mt * mt) {
  // audio values may have changed
  // we need to reinit the filters if they are being edited
  // for now we just have avol_fx

  // TODO - in future we may have more audio filters

  weed_plant_t *inst;
  weed_plant_t **in_channels, **out_channels;

  int num_in, num_out;
  int i;

  add_aparam_menuitems(mt);

  if (mt->current_rfx == NULL || mt->current_fx == -1 || mt->current_fx != mt->avol_fx) return;

  inst = (weed_plant_t *)mt->current_rfx->source;
  in_channels = weed_get_plantptr_array_counted(inst, WEED_LEAF_IN_CHANNELS, &num_in);
  if (num_in > 0) {
    for (i = 0; i < num_in; i++) {
      weed_set_int_value(in_channels[i], WEED_LEAF_AUDIO_CHANNELS, cfile->achans);
      weed_set_int_value(in_channels[i], WEED_LEAF_AUDIO_RATE, cfile->arate);
    }
    lives_free(in_channels);
  }
  out_channels = weed_get_plantptr_array_counted(inst, WEED_LEAF_OUT_CHANNELS, &num_out);
  if (num_out > 0) {
    for (i = 0; i < num_out; i++) {
      weed_set_int_value(out_channels[i], WEED_LEAF_AUDIO_CHANNELS, cfile->achans);
      weed_set_int_value(out_channels[i], WEED_LEAF_AUDIO_RATE, cfile->arate);
    }
    lives_free(out_channels);
  }

  mt->changed = TRUE;

  weed_reinit_effect(inst, TRUE);
  polymorph(mt, POLY_PARAMS);
}


static char *mt_set_vals_string(void) {
  char sendian[128];

  if (cfile->signed_endian & AFORM_UNSIGNED) lives_snprintf(sendian, 128, "%s", _("unsigned "));
  else lives_snprintf(sendian, 128, "%s", _("signed "));

  if (cfile->signed_endian & AFORM_BIG_ENDIAN) lives_strappend(sendian, 128, _("big endian"));
  else lives_strappend(sendian, 128, _("little endian"));

  return lives_strdup_printf(
           _("Multitrack values set to %.3f fps, frame size %d x %d, audio channels %d, audio rate %d, audio sample size %d, %s.\n"),
           cfile->fps,
           cfile->hsize, cfile->vsize, cfile->achans, cfile->arate, cfile->asampsize, sendian);
}


void set_mt_play_sizes(lives_mt * mt, int width, int height, boolean reset) {
  int rwidth, rheight;

  if (reset) {
    boolean needs_idlefunc = FALSE;
    boolean did_backup = mt->did_backup;

    if (mt->idlefunc > 0) {
      lives_source_remove(mt->idlefunc);
      needs_idlefunc = TRUE;
      mt->idlefunc = 0;
    }

    lives_widget_set_size_request(mainw->play_image, GUI_SCREEN_WIDTH / 3., GUI_SCREEN_HEIGHT / 3.);
    lives_widget_set_size_request(mt->preview_eventbox, GUI_SCREEN_WIDTH / 3., GUI_SCREEN_HEIGHT / 3.);
    lives_widget_set_size_request(mt->play_box, GUI_SCREEN_WIDTH / 3., GUI_SCREEN_HEIGHT / 3.);

    if (!LIVES_IS_PLAYING) {
      lives_widget_context_update();
      if (needs_idlefunc || (!did_backup && mt->auto_changed)) mt->idlefunc = mt_idle_add(mt);
    }
  }

  rwidth = lives_widget_get_allocation_width(mainw->play_image) - widget_opts.border_width * 2;
  rheight = lives_widget_get_allocation_height(mainw->play_image) - widget_opts.border_width * 2;

  if (rwidth * rheight < 64) {
    rwidth = GUI_SCREEN_WIDTH / 3. - widget_opts.border_width * 2;
    rheight = GUI_SCREEN_HEIGHT / 3. - widget_opts.border_width * 2;
  }

  calc_maxspect(rwidth, rheight, &width, &height);

  mt->play_width = width;
  mt->play_height = height;
}


static weed_plant_t *load_event_list_inner(lives_mt * mt, int fd, boolean show_errors, int *num_events,
    unsigned char **mem, unsigned char *mem_end) {
  weed_plant_t *event, *eventprev = NULL;
  weed_plant_t *event_list;

  double fps = -1.;

  char *msg;
  char *err;

  int error;

  if (fd > 0 || mem != NULL) event_list = weed_plant_deserialise(fd, mem, NULL);
  else event_list = mainw->stored_event_list;

  if (mt != NULL) mt->layout_set_properties = FALSE;

  if (event_list == NULL || !WEED_PLANT_IS_EVENT_LIST(event_list)) {
    if (show_errors) d_print(_("invalid event list. Failed.\n"));
    return NULL;
  }

  if (show_errors && (!weed_plant_has_leaf(event_list, WEED_LEAF_FPS) ||
                      (fps = weed_get_double_value(event_list, WEED_LEAF_FPS, &error)) < 1. ||
                      fps > FPS_MAX)) {
    d_print(_("event list has invalid fps. Failed.\n"));
    return NULL;
  }

  if (weed_plant_has_leaf(event_list, WEED_LEAF_NEEDS_SET)) {
    if (show_errors) {
      char *set_needed = weed_get_string_value(event_list, WEED_LEAF_NEEDS_SET, &error);
      char *tmp = NULL;
      if (!mainw->was_set || strcmp((tmp = U82F(set_needed)), mainw->set_name)) {
        if (tmp != NULL) lives_free(tmp);
        err = lives_strdup_printf(
                _("\nThis layout requires the set \"%s\"\nIn order to load it you must return to the Clip Editor, \n"
                  "close the current set,\nthen load in the new set from the File menu.\n"),
                set_needed);
        d_print(err);
        do_error_dialog_with_check_transient(err, TRUE, 0, LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET));
        lives_free(err);
        lives_free(set_needed);
        return NULL;
      }
      if (tmp != NULL) lives_free(tmp);
      lives_free(set_needed);
    }
  } else if (mt != NULL && !show_errors && mem == NULL) return NULL; // no change needed

  if (mt != NULL) {
    if (event_list == mainw->stored_event_list || (mt != NULL && !mt->ignore_load_vals)) {
      if (fps > -1) {
        cfile->fps = cfile->pb_fps = fps;
        if (mt != NULL) mt->fps = cfile->fps;
        cfile->ratio_fps = check_for_ratio_fps(cfile->fps);
      }

      // check for optional leaves
      if (weed_plant_has_leaf(event_list, WEED_LEAF_WIDTH)) {
        int width = weed_get_int_value(event_list, WEED_LEAF_WIDTH, &error);
        if (width > 0) {
          cfile->hsize = width;
          if (mt != NULL) mt->layout_set_properties = TRUE;
        }
      }

      if (weed_plant_has_leaf(event_list, WEED_LEAF_HEIGHT)) {
        int height = weed_get_int_value(event_list, WEED_LEAF_HEIGHT, &error);
        if (height > 0) {
          cfile->vsize = height;
          if (mt != NULL) mt->layout_set_properties = TRUE;
        }
      }

      if (weed_plant_has_leaf(event_list, WEED_LEAF_AUDIO_CHANNELS)) {
        int achans = weed_get_int_value(event_list, WEED_LEAF_AUDIO_CHANNELS, &error);
        if (achans >= 0 && mt != NULL) {
          if (achans > 2) {
            char *err = lives_strdup_printf(
                          _("\nThis layout has an invalid number of audio channels (%d) for LiVES.\nIt cannot be loaded.\n"), achans);
            d_print(err);
            do_error_dialog_with_check_transient(err, TRUE, 0, LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET));
            lives_free(err);
            return NULL;
          }
          cfile->achans = achans;
          if (mt != NULL) mt->layout_set_properties = TRUE;
        }
      }

      if (weed_plant_has_leaf(event_list, WEED_LEAF_AUDIO_RATE)) {
        int arate = weed_get_int_value(event_list, WEED_LEAF_AUDIO_RATE, &error);
        if (arate > 0) {
          cfile->arate = cfile->arps = arate;
          if (mt != NULL) mt->layout_set_properties = TRUE;
        }
      }

      if (weed_plant_has_leaf(event_list, WEED_LEAF_AUDIO_SAMPLE_SIZE)) {
        int asamps = weed_get_int_value(event_list, WEED_LEAF_AUDIO_SAMPLE_SIZE, &error);
        if (asamps == 8 || asamps == 16) {
          cfile->asampsize = asamps;
          if (mt != NULL) mt->layout_set_properties = TRUE;
        } else if (cfile->achans > 0) {
          msg = lives_strdup_printf("Layout has invalid sample size %d\n", asamps);
          LIVES_ERROR(msg);
          lives_free(msg);
        }
      }

      if (weed_plant_has_leaf(event_list, WEED_LEAF_AUDIO_SIGNED)) {
        int asigned = weed_get_boolean_value(event_list, WEED_LEAF_AUDIO_SIGNED, &error);
        if (asigned == WEED_TRUE) {
          if (cfile->signed_endian & AFORM_UNSIGNED) cfile->signed_endian ^= AFORM_UNSIGNED;
        } else {
          if (!(cfile->signed_endian & AFORM_UNSIGNED)) cfile->signed_endian |= AFORM_UNSIGNED;
        }
        if (mt != NULL) mt->layout_set_properties = TRUE;
      }

      if (weed_plant_has_leaf(event_list, WEED_LEAF_AUDIO_ENDIAN)) {
        int aendian = weed_get_int_value(event_list, WEED_LEAF_AUDIO_ENDIAN, &error);
        if (aendian == WEED_AUDIO_LITTLE_ENDIAN) {
          if (cfile->signed_endian & AFORM_BIG_ENDIAN) cfile->signed_endian ^= AFORM_BIG_ENDIAN;
        } else {
          if (!(cfile->signed_endian & AFORM_BIG_ENDIAN)) cfile->signed_endian |= AFORM_BIG_ENDIAN;
        }
        if (mt != NULL) mt->layout_set_properties = TRUE;
      }
    } else {
      if (mt != NULL) {
        msg = set_values_from_defs(mt, FALSE);
        if (msg != NULL) {
          if (mt != NULL) mt->layout_set_properties = TRUE;
          lives_free(msg);
        }
        cfile->fps = cfile->pb_fps;
        if (mt != NULL) mt->fps = cfile->fps;
        cfile->ratio_fps = check_for_ratio_fps(cfile->fps);
      }
    }

    if (event_list == mainw->stored_event_list) return event_list;
    // force 64 bit ptrs when reading layouts (for compatibility)
    prefs->force64bit = FALSE;

    if (weed_plant_has_leaf(event_list, WEED_LEAF_WEED_EVENT_API_VERSION)) {
      if (weed_get_int_value(event_list, WEED_LEAF_WEED_EVENT_API_VERSION, &error) >= 110) prefs->force64bit = TRUE;
    } else {
      if (weed_plant_has_leaf(event_list, WEED_LEAF_PTRSIZE)) {
        if (weed_get_int_value(event_list, WEED_LEAF_PTRSIZE, &error) == 8) prefs->force64bit = TRUE;
      }
    }
  }

  if (weed_plant_has_leaf(event_list, WEED_LEAF_FIRST)) weed_leaf_delete(event_list, WEED_LEAF_FIRST);
  if (weed_plant_has_leaf(event_list, WEED_LEAF_LAST)) weed_leaf_delete(event_list, WEED_LEAF_LAST);

  weed_set_voidptr_value(event_list, WEED_LEAF_FIRST, NULL);
  weed_set_voidptr_value(event_list, WEED_LEAF_LAST, NULL);

  do {
    if (mem != NULL && (*mem) >= mem_end) break;
    event = weed_plant_deserialise(fd, mem, NULL);
    if (event != NULL) {
#ifdef DEBUG_TTABLE
      uint64_t event_id;
      if (weed_plant_has_leaf(event, WEED_LEAF_INIT_EVENT)) {
        if (weed_leaf_seed_type(event, WEED_LEAF_INIT_EVENT) == WEED_SEED_INT64)
          event_id = (uint64_t)(weed_get_int64_value(event, WEED_LEAF_INIT_EVENT, &error));
        else
          event_id = (uint64_t)((weed_plant_t *)weed_get_voidptr_value(event, WEED_LEAF_INIT_EVENT, &error));
      }
#endif

      if (weed_plant_has_leaf(event, WEED_LEAF_PREVIOUS)) weed_leaf_delete(event, WEED_LEAF_PREVIOUS);
      if (weed_plant_has_leaf(event, WEED_LEAF_NEXT)) weed_leaf_delete(event, WEED_LEAF_NEXT);
      if (eventprev != NULL) weed_set_voidptr_value(eventprev, WEED_LEAF_NEXT, event);
      weed_set_voidptr_value(event, WEED_LEAF_PREVIOUS, eventprev);
      weed_set_voidptr_value(event, WEED_LEAF_NEXT, NULL);
      if (get_first_event(event_list) == NULL) {
        weed_set_voidptr_value(event_list, WEED_LEAF_FIRST, event);
      }
      weed_set_voidptr_value(event_list, WEED_LEAF_LAST, event);
      //weed_add_plant_flags(event, WEED_LEAF_READONLY_PLUGIN);
      eventprev = event;
      if (num_events != NULL)(*num_events)++;
    }
  } while (event != NULL);

  if (mt == NULL) return event_list;

  //weed_add_plant_flags(event_list, WEED_LEAF_READONLY_PLUGIN);

  if (weed_plant_has_leaf(event_list, WEED_LEAF_GAMMA_ENABLED)) {
    err = NULL;
    if (weed_get_boolean_value(event_list, WEED_LEAF_GAMMA_ENABLED, &error) == WEED_TRUE) {
      if (!prefs->apply_gamma) {
        err = lives_strdup(_("This layout was created using automatic gamma correction.\nFor compatibility, you may wish to"
                             "enable this feature in Tools -> Preferences -> Effects, whilst editing and rendering the layout."));
      }
    } else if (prefs->apply_gamma) {
      err = lives_strdup(_("This layout was created without automatic gamma correction.\nFor compatibility, you may wish to"
                           "disable this feature in Tools -> Preferences -> Effects, whilst editing and rendering the layout."));
    }
    if (err != NULL) {
      do_error_dialog_with_check_transient(err, TRUE, WARN_MASK_LAYOUT_GAMMA, LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET));
      lives_free(err);
    }
  }
  return event_list;
}


char *set_values_from_defs(lives_mt * mt, boolean from_prefs) {
  // set various multitrack state flags from either defaults or user preferences

  char *retval = NULL;

  int hsize = cfile->hsize;
  int vsize = cfile->vsize;
  int arate = cfile->arate;
  int achans = cfile->achans;
  int asamps = cfile->asampsize;
  int ase = cfile->signed_endian;

  if (mainw->stored_event_list != NULL) {
    load_event_list_inner(mt, -1, TRUE, NULL, NULL, NULL);
    mt->user_width = cfile->hsize;
    mt->user_height = cfile->vsize;
    cfile->pb_fps = mt->fps = mt->user_fps = cfile->fps;
    cfile->arps = mt->user_arate = cfile->arate;
    mt->user_achans = cfile->achans;
    mt->user_asamps = cfile->asampsize;
    mt->user_signed_endian = cfile->signed_endian;
  } else {
    if (!from_prefs) {
      cfile->hsize = mt->user_width;
      cfile->vsize = mt->user_height;
      cfile->pb_fps = cfile->fps = mt->fps = mt->user_fps;
      cfile->arps = cfile->arate = mt->user_arate;
      cfile->achans = mt->user_achans;
      cfile->asampsize = mt->user_asamps;
      cfile->signed_endian = mt->user_signed_endian;
    } else {
      mt->user_width = cfile->hsize = prefs->mt_def_width;
      mt->user_height = cfile->vsize = prefs->mt_def_height;
      mt->user_fps = cfile->pb_fps = cfile->fps = mt->fps = prefs->mt_def_fps;
      mt->user_arate = cfile->arate = cfile->arps = prefs->mt_def_arate;
      mt->user_achans = cfile->achans = prefs->mt_def_achans;
      mt->user_asamps = cfile->asampsize = prefs->mt_def_asamps;
      mt->user_signed_endian = cfile->signed_endian = prefs->mt_def_signed_endian;
    }
  }
  cfile->ratio_fps = check_for_ratio_fps(cfile->fps);

  if (cfile->hsize != hsize || cfile->vsize != vsize || cfile->arate != arate || cfile->achans != achans ||
      cfile->asampsize != asamps || cfile->signed_endian != ase) {
    retval = mt_set_vals_string();
  }

  if (mt->is_ready) scroll_tracks(mt, 0, TRUE);

  if (cfile->achans == 0) {
    mt->avol_fx = -1;
    mt->avol_init_event = NULL;
  } else set_audio_filter_channel_values(mt);

  return retval;
}


void event_list_free_undos(lives_mt * mt) {
  if (mt == NULL) return;

  if (mt->undos != NULL) lives_list_free(mt->undos);
  mt->undos = NULL;
  mt->undo_buffer_used = 0;
  mt->undo_offset = 0;

  if (mainw->is_exiting) return;

  mt_set_undoable(mt, MT_UNDO_NONE, NULL, FALSE);
  mt_set_redoable(mt, MT_UNDO_NONE, NULL, FALSE);
}


void stored_event_list_free_undos(void) {
  if (mainw->stored_layout_undos != NULL) lives_list_free(mainw->stored_layout_undos);
  mainw->stored_layout_undos = NULL;
  lives_freep((void **)&mainw->sl_undo_mem);
  mainw->sl_undo_buffer_used = 0;
  mainw->sl_undo_offset = 0;
}


void remove_current_from_affected_layouts(lives_mt * mt) {
  // remove from affected layouts map
  if (mainw->affected_layouts_map != NULL) {
    LiVESList *found = lives_list_find_custom(mainw->affected_layouts_map, mainw->string_constants[LIVES_STRING_CONSTANT_CL],
                       (LiVESCompareFunc)strcmp);
    if (found != NULL) {
      lives_free((livespointer)found->data);
      mainw->affected_layouts_map = lives_list_delete_link(mainw->affected_layouts_map, found);
    }
  }

  if (mainw->affected_layouts_map == NULL) {
    lives_widget_set_sensitive(mainw->show_layout_errors, FALSE);
    if (mt != NULL) lives_widget_set_sensitive(mt->show_layout_errors, FALSE);
  }

  recover_layout_cancelled(FALSE);

  if (mt != NULL) {
    if (mt->event_list != NULL) {
      event_list_free(mt->event_list);
      mt->event_list = NULL;
    }
    mt_clear_timeline(mt);
  }

  // remove some text

  if (mainw->layout_textbuffer != NULL) {
    LiVESTextIter iter1, iter2;
    LiVESList *markmap = mainw->affected_layout_marks;
    while (markmap != NULL) {
      lives_text_buffer_get_iter_at_mark(LIVES_TEXT_BUFFER(mainw->layout_textbuffer), &iter1, (LiVESTextMark *)markmap->data);
      lives_text_buffer_get_iter_at_mark(LIVES_TEXT_BUFFER(mainw->layout_textbuffer), &iter2, (LiVESTextMark *)markmap->next->data);
      lives_text_buffer_delete(LIVES_TEXT_BUFFER(mainw->layout_textbuffer), &iter1, &iter2);

      lives_text_buffer_delete_mark(LIVES_TEXT_BUFFER(mainw->layout_textbuffer), (LiVESTextMark *)markmap->data);
      lives_text_buffer_delete_mark(LIVES_TEXT_BUFFER(mainw->layout_textbuffer), (LiVESTextMark *)markmap->next->data);
      markmap = markmap->next->next;
    }
    mainw->affected_layout_marks = NULL;
  }
}


void stored_event_list_free_all(boolean wiped) {
  register int i;

  for (i = 0; i < MAX_FILES; i++) {
    if (mainw->files[i] != NULL) {
      mainw->files[i]->stored_layout_frame = 0;
      mainw->files[i]->stored_layout_audio = 0.;
      mainw->files[i]->stored_layout_fps = 0.;
      mainw->files[i]->stored_layout_idx = -1;
    }
  }

  stored_event_list_free_undos();

  if (mainw->stored_event_list != NULL) event_list_free(mainw->stored_event_list);
  mainw->stored_event_list = NULL;

  if (wiped) {
    remove_current_from_affected_layouts(NULL);
    mainw->stored_event_list_changed = FALSE;
  }
}


LIVES_INLINE void print_layout_wiped(void) {
  d_print(_("Layout was wiped.\n"));
}


boolean check_for_layout_del(lives_mt * mt, boolean exiting) {
  // save or wipe event_list
  // returns FALSE if cancelled
  int resp = 2;

  if ((mt == NULL || mt->event_list == NULL || get_first_event(mt->event_list) == NULL) &&
      (mainw->stored_event_list == NULL || get_first_event(mainw->stored_event_list) == NULL)) return TRUE;

  if (((mt != NULL && (mt->changed || mainw->scrap_file != -1 || mainw->ascrap_file != -1)) ||
       (mainw->stored_event_list != NULL &&
        mainw->stored_event_list_changed))) {
    int type = ((mainw->scrap_file == -1 && mainw->ascrap_file == -1) || mt == NULL) ? 3 * (!exiting) : 4;
    _entryw *cdsw = create_cds_dialog(type);

    do {
      resp = lives_dialog_run(LIVES_DIALOG(cdsw->dialog));
      if (resp == 2) {
        // save
        mainw->cancelled = CANCEL_NONE;
        on_save_event_list_activate(NULL, mt);
        if (mainw->cancelled == CANCEL_NONE) {
          break;
        } else mainw->cancelled = CANCEL_NONE;
      }
    } while (resp == 2);

    lives_widget_destroy(cdsw->dialog);
    lives_free(cdsw);

    if (resp == LIVES_RESPONSE_CANCEL) {
      // cancel
      return FALSE;
    }

    recover_layout_cancelled(FALSE);

    if (resp == 1 && !exiting) {
      // wipe
      prefs->ar_layout = FALSE;
      set_string_pref(PREF_AR_LAYOUT, "");
      lives_memset(prefs->ar_layout_name, 0, 1);
    }
  }

  if (mainw->stored_event_list != NULL || mainw->sl_undo_mem != NULL) {
    stored_event_list_free_all(TRUE);
    print_layout_wiped();
  } else if (mt != NULL && mt->event_list != NULL && (exiting || resp == 1)) {
    event_list_free(mt->event_list);
    event_list_free_undos(mt);
    mt->event_list = NULL;
    mt_clear_timeline(mt);
    close_scrap_file(TRUE);
    close_ascrap_file(TRUE);
    mainw->recording_recovered = FALSE;
    print_layout_wiped();
  }

  return TRUE;
}


void delete_audio_tracks(lives_mt * mt, LiVESList * list, boolean full) {
  LiVESList *slist = list;
  while (slist != NULL) {
    delete_audio_track(mt, (LiVESWidget *)slist->data, full);
    slist = slist->next;
  }
  lives_list_free(list);
}


void mt_quit_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;

  if (!check_for_layout_del(mt, FALSE)) return;

  if (mt->idlefunc > 0) lives_source_remove(mt->idlefunc);
  mt->idlefunc = 0;

  on_quit_activate(menuitem, NULL);
}


static void set_mt_title(lives_mt * mt) {
  char *wtxt = lives_strdup_printf(_("Multitrack %dx%d : %d bpp %.3f fps"), cfile->hsize, cfile->vsize, cfile->bpp,
                                   cfile->fps);
  lives_window_set_title(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET), wtxt);
  lives_free(wtxt);
}


static boolean timecode_string_validate(LiVESEntry * entry, lives_mt * mt) {
  const char *etext = lives_entry_get_text(entry);
  char **array;

  double secs;
  double tl_range, pos;

  int hrs, mins;

  if (get_token_count((char *)etext, ':') != 3) return FALSE;

  array = lives_strsplit(etext, ":", 3);

  if (get_token_count(array[2], '.') != 2) {
    lives_strfreev(array);
    return FALSE;
  }

  hrs = atoi(array[0]);
  mins = atoi(array[1]);
  if (mins > 59) mins = 59;
  secs = lives_strtod(array[2], NULL);

  lives_strfreev(array);

  secs = secs + mins * 60. + hrs * 3600.;

  if (secs > mt->end_secs) {
    tl_range = mt->tl_max - mt->tl_min;
    set_timeline_end_secs(mt, secs);

    mt->tl_min = secs - tl_range / 2;
    mt->tl_max = secs + tl_range / 2;

    if (mt->tl_max > mt->end_secs) {
      mt->tl_min -= (mt->tl_max - mt->end_secs);
      mt->tl_max = mt->end_secs;
    }
  }

  mt_tl_move(mt, secs);

  lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET, TRUE);

  pos = mt->ptr_time;

  pos = q_dbl(pos, mt->fps) / TICKS_PER_SECOND_DBL;
  if (pos < 0.) pos = 0.;

  update_timecodes(mt, pos);

  return TRUE;
}


static void cmi_set_inactive(LiVESWidget * widget, livespointer data) {
  if (widget == data) return;
  lives_widget_object_freeze_notify(LIVES_WIDGET_OBJECT(widget));
  lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(widget), FALSE);
  lives_widget_object_thaw_notify(LIVES_WIDGET_OBJECT(widget));
}


void mt_set_autotrans(int idx) {
  prefs->atrans_fx = idx;
  if (idx == -1) set_string_pref(PREF_ACTIVE_AUTOTRANS, "none");
  else {
    char *atrans_hash = make_weed_hashname(prefs->atrans_fx, TRUE, FALSE, '|', FALSE);
    set_string_pref(PREF_ACTIVE_AUTOTRANS, atrans_hash);
    lives_free(atrans_hash);
  }
  mouse_mode_context(mainw->multitrack);
}


static void mt_set_atrans_effect(LiVESMenuItem * menuitem, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;

  if (!lives_check_menu_item_get_active(LIVES_CHECK_MENU_ITEM(menuitem))) return;
  lives_container_foreach(LIVES_CONTAINER(mt->submenu_atransfx), cmi_set_inactive, menuitem);

  mt_set_autotrans(LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(menuitem), "idx")));
}


static void after_timecode_changed(LiVESWidget * entry, LiVESXEventFocus * dir, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  double pos;

  if (!timecode_string_validate(LIVES_ENTRY(entry), mt)) {
    pos = mt->ptr_time;
    pos = q_dbl(pos, mt->fps) / TICKS_PER_SECOND_DBL;
    if (pos < 0.) pos = 0.;
    update_timecodes(mt, pos);
  }
}

#if GTK_CHECK_VERSION(3, 0, 0)
static boolean expose_pb(LiVESWidget * widget, lives_painter_t *cr, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  if (mt->no_expose || mt->no_expose_frame) return TRUE;
  if (LIVES_IS_PLAYING) return TRUE;
  //lives_widget_set_size_request(mainw->play_image, GUI_SCREEN_WIDTH / 3., GUI_SCREEN_HEIGHT / 3.);
  set_ce_frame_from_pixbuf(LIVES_IMAGE(mainw->play_image), mt->frame_pixbuf, cr);
  return TRUE;
}
#endif


static char *get_tab_name(uint32_t tab) {
  switch (tab) {
  case POLY_CLIPS:
    return lives_strdup(_("Clips"));
  case POLY_IN_OUT:
    return lives_strdup(_("In/out"));
  case POLY_FX_STACK:
    return lives_strdup(_("FX stack"));
  case POLY_EFFECTS:
    return lives_fx_cat_to_text(LIVES_FX_CAT_EFFECT, TRUE); // effects
  case POLY_TRANS:
    return lives_fx_cat_to_text(LIVES_FX_CAT_TRANSITION, TRUE); // transitions
  case POLY_COMP:
    return lives_fx_cat_to_text(LIVES_FX_CAT_COMPOSITOR, TRUE); // compositors
  case POLY_PARAMS:
    return lives_strdup(_("Params."));
  default:
    break;
  }
  return lives_strdup("");
}


void set_mt_colours(lives_mt * mt) {
  lives_widget_set_bg_color(mt->timecode, LIVES_WIDGET_STATE_NORMAL, &palette->mt_timecode_bg);
  lives_widget_set_base_color(mt->timecode, LIVES_WIDGET_STATE_NORMAL, &palette->mt_timecode_bg);
  lives_widget_set_text_color(mt->timecode, LIVES_WIDGET_STATE_NORMAL, &palette->mt_timecode_fg);

  lives_widget_set_bg_color(mt->timecode, LIVES_WIDGET_STATE_INSENSITIVE, &palette->mt_timecode_bg);
  lives_widget_set_base_color(mt->timecode, LIVES_WIDGET_STATE_INSENSITIVE, &palette->mt_timecode_bg);
  lives_widget_set_text_color(mt->timecode, LIVES_WIDGET_STATE_INSENSITIVE, &palette->mt_timecode_fg);

#ifdef ENABLE_GIW_3
  // need to set this even if theme is none
  if (mt->timeline != NULL) {
    boolean woat = widget_opts.apply_theme;
    widget_opts.apply_theme = TRUE;
    lives_widget_apply_theme(mt->timeline, LIVES_WIDGET_STATE_NORMAL);
    widget_opts.apply_theme = woat;
  }
#endif

  mt_clip_select(mt, FALSE);

  lives_widget_apply_theme(mt->top_vbox, LIVES_WIDGET_STATE_NORMAL);
  lives_widget_apply_theme(mt->tl_hbox, LIVES_WIDGET_STATE_NORMAL);

  lives_widget_apply_theme(mt->clip_inner_box, LIVES_WIDGET_STATE_NORMAL);

  lives_widget_apply_theme2(mt->menubar, LIVES_WIDGET_STATE_NORMAL, TRUE);
  lives_widget_apply_theme2(mt->menu_hbox, LIVES_WIDGET_STATE_NORMAL, TRUE);

  lives_widget_apply_theme(mt->eventbox, LIVES_WIDGET_STATE_NORMAL);
  lives_widget_apply_theme(mt->scroll_label, LIVES_WIDGET_STATE_NORMAL);

  if (mt->dumlabel1 != NULL)
    lives_widget_set_bg_color(mt->dumlabel1, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
  if (mt->dumlabel2 != NULL)
    lives_widget_set_bg_color(mt->dumlabel2, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);

  lives_widget_apply_theme(mt->preview_frame, LIVES_WIDGET_STATE_NORMAL);
  lives_widget_set_fg_color(lives_frame_get_label_widget(LIVES_FRAME(mt->preview_frame)), LIVES_WIDGET_STATE_NORMAL,
                            &palette->normal_fore);

  lives_widget_apply_theme2(mt->top_eventbox, LIVES_WIDGET_STATE_NORMAL, TRUE);

  set_submenu_colours(LIVES_MENU(mt->files_menu), &palette->menu_and_bars_fore, &palette->menu_and_bars);
  set_submenu_colours(LIVES_MENU(mt->edit_menu), &palette->menu_and_bars_fore, &palette->menu_and_bars);
  set_submenu_colours(LIVES_MENU(mt->play_menu), &palette->menu_and_bars_fore, &palette->menu_and_bars);
  set_submenu_colours(LIVES_MENU(mt->effects_menu), &palette->menu_and_bars_fore, &palette->menu_and_bars);
  set_submenu_colours(LIVES_MENU(mt->tracks_menu), &palette->menu_and_bars_fore, &palette->menu_and_bars);
  set_submenu_colours(LIVES_MENU(mt->selection_menu), &palette->menu_and_bars_fore, &palette->menu_and_bars);
  set_submenu_colours(LIVES_MENU(mt->tools_menu), &palette->menu_and_bars_fore, &palette->menu_and_bars);
  set_submenu_colours(LIVES_MENU(mt->render_menu), &palette->menu_and_bars_fore, &palette->menu_and_bars);
  set_submenu_colours(LIVES_MENU(mt->view_menu), &palette->menu_and_bars_fore, &palette->menu_and_bars);
  set_submenu_colours(LIVES_MENU(mt->help_menu), &palette->menu_and_bars_fore, &palette->menu_and_bars);

  lives_widget_set_bg_color(mt->grav_submenu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
#if GTK_CHECK_VERSION(3, 0, 0)
  lives_widget_set_bg_color(mt->grav_submenu, LIVES_WIDGET_STATE_BACKDROP, &palette->menu_and_bars);
#endif
  lives_widget_set_fg_color(mt->grav_submenu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars_fore);

  lives_tool_button_set_border_colour(mt->grav_menuitem, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
  set_child_alt_colour(mt->grav_menuitem, TRUE);

  lives_widget_set_bg_color(mt->mm_submenu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
#if GTK_CHECK_VERSION(3, 0, 0)
  lives_widget_set_bg_color(mt->mm_submenu, LIVES_WIDGET_STATE_BACKDROP, &palette->menu_and_bars);
#endif
  lives_widget_set_fg_color(mt->mm_submenu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars_fore);
  lives_tool_button_set_border_colour(mt->mm_menuitem, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
  set_child_alt_colour(mt->mm_menuitem, TRUE);

  lives_widget_set_bg_color(mt->ins_submenu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
#if GTK_CHECK_VERSION(3, 0, 0)
  lives_widget_set_bg_color(mt->ins_submenu, LIVES_WIDGET_STATE_BACKDROP, &palette->menu_and_bars);
#endif
  lives_widget_set_fg_color(mt->ins_submenu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars_fore);
  lives_tool_button_set_border_colour(mt->ins_menuitem, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
  set_child_alt_colour(mt->ins_menuitem, TRUE);

  lives_widget_set_fg_color(mt->l_sel_arrow, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
  lives_widget_set_fg_color(mt->r_sel_arrow, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);

  lives_widget_apply_theme2(mt->amixer_button, LIVES_WIDGET_STATE_NORMAL, TRUE);
  set_child_alt_colour(mt->amixer_button, TRUE);

  lives_widget_apply_theme2(mainw->vol_toolitem, LIVES_WIDGET_STATE_NORMAL, FALSE);
  if (mainw->vol_label != NULL) lives_widget_apply_theme2(mainw->vol_label, LIVES_WIDGET_STATE_NORMAL, FALSE);
  lives_widget_apply_theme2(mainw->volume_scale, LIVES_WIDGET_STATE_NORMAL, FALSE);

  lives_widget_apply_theme(mt->in_out_box, LIVES_WIDGET_STATE_NORMAL);
  set_child_colour(mt->in_out_box, TRUE);

  if (palette->style & STYLE_4) {
    lives_widget_show(mt->hseparator);
    lives_widget_apply_theme2(mt->hseparator, LIVES_WIDGET_STATE_NORMAL, TRUE);
    if (mt->hseparator2 != NULL) {
      lives_widget_show(mt->hseparator2);
      lives_widget_apply_theme2(mt->hseparator2, LIVES_WIDGET_STATE_NORMAL, TRUE);
    }
  } else {
    lives_widget_hide(mt->hseparator);
    if (mt->hseparator2 != NULL) {
      lives_widget_hide(mt->hseparator2);
    }
  }

  lives_widget_set_bg_color(mt->in_image, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
  lives_widget_set_bg_color(mt->out_image, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);

  lives_widget_apply_theme2(mt->btoolbarx, LIVES_WIDGET_STATE_NORMAL, TRUE);

  lives_widget_apply_theme2(mt->btoolbary, LIVES_WIDGET_STATE_NORMAL, TRUE);
  set_child_alt_colour(mt->btoolbary, TRUE);

  lives_widget_set_fg_color(mt->tlx_eventbox, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
  lives_widget_set_bg_color(mt->tlx_eventbox, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);

  if (mt->fx_params_label != NULL) lives_widget_apply_theme2(mt->fx_params_label, LIVES_WIDGET_STATE_NORMAL, TRUE);
  lives_widget_apply_theme(mt->time_label, LIVES_WIDGET_STATE_NORMAL);

  if (mt->tl_label != NULL)
    lives_widget_set_fg_color(mt->tl_label, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);

  // needed for gtk+ 2.x
  lives_widget_apply_theme2(lives_widget_get_parent(mt->insa_label), LIVES_WIDGET_STATE_NORMAL, TRUE);
  lives_widget_apply_theme2(mt->insa_label, LIVES_WIDGET_STATE_NORMAL, TRUE);

  lives_widget_apply_theme2(lives_widget_get_parent(mt->overlap_label), LIVES_WIDGET_STATE_NORMAL, TRUE);
  lives_widget_apply_theme2(mt->overlap_label, LIVES_WIDGET_STATE_NORMAL, TRUE);

  lives_widget_set_bg_color(mt->preview_eventbox, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);

  lives_widget_apply_theme2(mt->btoolbar2, LIVES_WIDGET_STATE_NORMAL, TRUE);
  lives_widget_apply_theme2(mt->btoolbar3, LIVES_WIDGET_STATE_NORMAL, TRUE);

  lives_widget_apply_theme2(lives_widget_get_parent(mt->grav_label), LIVES_WIDGET_STATE_NORMAL, TRUE);
  lives_widget_apply_theme2(mt->grav_label, LIVES_WIDGET_STATE_NORMAL, TRUE);

  if (mt->ins_label != NULL) {
    lives_widget_apply_theme2(lives_widget_get_parent(mt->ins_label), LIVES_WIDGET_STATE_NORMAL, TRUE);
    lives_widget_apply_theme2(mt->ins_label, LIVES_WIDGET_STATE_NORMAL, TRUE);
  }

  if (mt->mm_label != NULL) {
    lives_widget_apply_theme2(lives_widget_get_parent(mt->mm_label), LIVES_WIDGET_STATE_NORMAL, TRUE);
    lives_widget_apply_theme2(mt->mm_label, LIVES_WIDGET_STATE_NORMAL, TRUE);
  }

  lives_widget_apply_theme2(lives_bin_get_child(LIVES_BIN(mt->grav_normal)), LIVES_WIDGET_STATE_NORMAL, TRUE);
  lives_widget_apply_theme2(lives_bin_get_child(LIVES_BIN(mt->grav_left)), LIVES_WIDGET_STATE_NORMAL, TRUE);
  lives_widget_apply_theme2(lives_bin_get_child(LIVES_BIN(mt->grav_right)), LIVES_WIDGET_STATE_NORMAL, TRUE);

  lives_widget_set_bg_color(mt->hpaned, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);

  lives_widget_set_bg_color(mt->nb, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
  lives_widget_set_base_color(mt->nb, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);

  lives_widget_set_bg_color(mt->nb, LIVES_WIDGET_STATE_ACTIVE, &palette->menu_and_bars);
  lives_widget_set_base_color(mt->nb, LIVES_WIDGET_STATE_ACTIVE, &palette->menu_and_bars);

  lives_widget_set_fg_color(mt->nb, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
  lives_widget_set_text_color(mt->nb, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);

  lives_widget_set_bg_color(lives_bin_get_child(LIVES_BIN(mt->clip_scroll)), LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);

  lives_widget_apply_theme2(mt->fx_base_box, LIVES_WIDGET_STATE_NORMAL, TRUE);

  lives_widget_apply_theme(mt->context_frame, LIVES_WIDGET_STATE_NORMAL);

  lives_widget_set_fg_color(lives_frame_get_label_widget(LIVES_FRAME(mt->context_frame)), LIVES_WIDGET_STATE_NORMAL,
                            &palette->normal_fore);

  // gtk+ 2.x
  if ((mt->poly_state == POLY_FX_STACK || mt->poly_state == POLY_EFFECTS || mt->poly_state == POLY_TRANS ||
       mt->poly_state == POLY_COMP) \
      && LIVES_IS_BIN(mt->fx_list_scroll) && lives_bin_get_child(LIVES_BIN(mt->fx_list_scroll)) != NULL)
    lives_widget_set_bg_color(lives_bin_get_child(LIVES_BIN(mt->fx_list_scroll)), LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);

  lives_widget_apply_theme(lives_bin_get_child(LIVES_BIN(mt->context_scroll)), LIVES_WIDGET_STATE_NORMAL);

  lives_widget_apply_theme(mt->context_box, LIVES_WIDGET_STATE_NORMAL);
  set_child_colour(mt->context_box, TRUE);

  lives_widget_apply_theme(mt->vpaned, LIVES_WIDGET_STATE_NORMAL);

  lives_widget_set_bg_color(mt->tl_eventbox, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);

  set_child_alt_colour(mt->btoolbar2, TRUE);

  lives_widget_apply_theme(LIVES_WIDGET(mt->timeline_table_header), LIVES_WIDGET_STATE_NORMAL);

  if (mt->timeline != NULL) {
    lives_widget_apply_theme(mt->timeline, LIVES_WIDGET_STATE_NORMAL);
  }

  if (palette->style & STYLE_3) {
    if (mt->timeline_eb != NULL) {
      lives_widget_apply_theme2(mt->timeline_eb, LIVES_WIDGET_STATE_NORMAL, TRUE);
    }
    if (mt->timeline_reg != NULL) {
      lives_widget_apply_theme2(mt->timeline_reg, LIVES_WIDGET_STATE_NORMAL, TRUE);
    }
  } else {
    if (mt->timeline_reg != NULL) {
      lives_widget_set_bg_color(mt->timeline_reg, LIVES_WIDGET_STATE_NORMAL, &palette->white);
      lives_widget_set_fg_color(mt->timeline_reg, LIVES_WIDGET_STATE_NORMAL, &palette->black);
    }
  }

  // BG color is set by eventbox (gtk+ 2.x), this is for gtk+3.x (?)
  lives_widget_apply_theme(mt->amix_label, LIVES_WIDGET_STATE_NORMAL);

  // for gtk+2.x (At least) this sets the amixer button (?)
  lives_widget_apply_theme2(mt->amixb_eventbox, LIVES_WIDGET_STATE_NORMAL, TRUE);

  lives_widget_apply_theme2(mt->amixer_button, LIVES_WIDGET_STATE_NORMAL, TRUE);

  lives_widget_apply_theme2(mt->btoolbar, LIVES_WIDGET_STATE_NORMAL, TRUE);

  if (palette->style & STYLE_2) {
#if !GTK_CHECK_VERSION(3, 0, 0)
    lives_widget_set_base_color(mt->spinbutton_start, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
    lives_widget_set_base_color(mt->spinbutton_start, LIVES_WIDGET_STATE_INSENSITIVE, &palette->normal_back);
    lives_widget_set_base_color(mt->spinbutton_end, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
    lives_widget_set_base_color(mt->spinbutton_end, LIVES_WIDGET_STATE_INSENSITIVE, &palette->normal_back);
    lives_widget_set_text_color(mt->spinbutton_start, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
    lives_widget_set_text_color(mt->spinbutton_start, LIVES_WIDGET_STATE_INSENSITIVE, &palette->normal_fore);
    lives_widget_set_text_color(mt->spinbutton_end, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
    lives_widget_set_text_color(mt->spinbutton_end, LIVES_WIDGET_STATE_INSENSITIVE, &palette->normal_fore);
#endif
  }

  lives_widget_apply_theme(mt->sel_label, LIVES_WIDGET_STATE_NORMAL);

  lives_widget_apply_theme2(mt->nb_label1, LIVES_WIDGET_STATE_NORMAL, TRUE);
  lives_widget_apply_theme2(mt->nb_label1, LIVES_WIDGET_STATE_ACTIVE, TRUE);
  lives_widget_apply_theme2(mt->nb_label2, LIVES_WIDGET_STATE_NORMAL, TRUE);
  lives_widget_apply_theme2(mt->nb_label2, LIVES_WIDGET_STATE_ACTIVE, TRUE);
  lives_widget_apply_theme2(mt->nb_label3, LIVES_WIDGET_STATE_NORMAL, TRUE);
  lives_widget_apply_theme2(mt->nb_label3, LIVES_WIDGET_STATE_ACTIVE, TRUE);
  lives_widget_apply_theme2(mt->nb_label4, LIVES_WIDGET_STATE_NORMAL, TRUE);
  lives_widget_apply_theme2(mt->nb_label4, LIVES_WIDGET_STATE_ACTIVE, TRUE);
  lives_widget_apply_theme2(mt->nb_label5, LIVES_WIDGET_STATE_NORMAL, TRUE);
  lives_widget_apply_theme2(mt->nb_label5, LIVES_WIDGET_STATE_ACTIVE, TRUE);
  lives_widget_apply_theme2(mt->nb_label6, LIVES_WIDGET_STATE_NORMAL, TRUE);
  lives_widget_apply_theme2(mt->nb_label6, LIVES_WIDGET_STATE_ACTIVE, TRUE);
  lives_widget_apply_theme2(mt->nb_label7, LIVES_WIDGET_STATE_NORMAL, TRUE);
  lives_widget_apply_theme2(mt->nb_label7, LIVES_WIDGET_STATE_ACTIVE, TRUE);

  redraw_all_event_boxes(mt);
}


static void on_solo_check_toggled(LiVESWidget * widg, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  mt_show_current_frame(mt, FALSE);
}


static void multitrack_clear_marks(LiVESMenuItem * menuitem, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  lives_list_free(mt->tl_marks);
  mt->tl_marks = NULL;
  lives_widget_set_sensitive(mt->clear_marks, FALSE);
  lives_widget_set_sensitive(mt->mark_jumpback, FALSE);
  lives_widget_set_sensitive(mt->mark_jumpnext, FALSE);
  lives_widget_queue_draw(mt->timeline_reg);
}


lives_mt *multitrack(weed_plant_t *event_list, int orig_file, double fps) {
  LiVESWidget *hseparator;
  LiVESWidget *menuitem;
  LiVESWidget *menuitem2;
  LiVESWidget *menuitemsep;
  LiVESWidget *menuitem_menu;
  LiVESWidget *selcopy_menu;
#if LIVES_HAS_IMAGE_MENU_ITEM
  LiVESWidget *image;
#endif
  LiVESWidget *full_screen;
  LiVESWidget *about;
  LiVESWidget *show_mt_keys;
  LiVESWidget *view_mt_details;
  LiVESWidget *zoom_in;
  LiVESWidget *zoom_out;
  LiVESWidget *show_messages;
  LiVESWidget *tl_vbox;
  LiVESWidget *scrollbar;
  LiVESWidget *hbox;
  LiVESWidget *vbox;
  LiVESWidget *ign_ins_sel;
  LiVESWidget *recent_submenu;
#ifdef ENABLE_DVD_GRAB
  LiVESWidget *vcd_dvd_submenu;
#endif
#ifdef HAVE_LDVGRAB
  LiVESWidget *device_submenu;
#endif
#ifdef HAVE_WEBM
  LiVESWidget *open_loc_submenu;
#endif
  LiVESWidget *submenu;
  LiVESWidget *submenu2;

  // block fx submenus
  LiVESWidget *submenu_menu;
  LiVESWidget *submenu_menuv;
  LiVESWidget *submenu_menua;
  LiVESWidget *submenu_menuvp = NULL;
  LiVESWidget *submenu_menuap = NULL;

  // region fx submenus
  LiVESWidget *submenu_menurv;
  LiVESWidget *submenu_menura;
  LiVESWidget *submenu_menurvp = NULL;
  LiVESWidget *submenu_menurap = NULL;
  LiVESWidget *submenu_menu2av;
  LiVESWidget *submenu_menu2v;
  LiVESWidget *submenu_menu2a;
  LiVESWidget *submenu_menu3;

  LiVESWidget *show_frame_events;
  LiVESWidget *ccursor;
  LiVESWidget *sep;
  LiVESWidget *show_manual;
  LiVESWidget *donate;
  LiVESWidget *email_author;
  LiVESWidget *report_bug;
  LiVESWidget *suggest_feature;
  LiVESWidget *help_translate;

  LiVESWidgetObject *vadjustment;
  LiVESAdjustment *spinbutton_adj;

  char buff[32768];
  const char *mtext;

  boolean in_menubar = TRUE;
  boolean woat = widget_opts.apply_theme;

  char *cname, *tname, *msg;
  char *tmp, *tmp2;
  char *pkg, *xpkgv = NULL, *xpkga = NULL;

  int dph;
  int num_filters;
  int scr_width = lives_widget_get_allocation_width(LIVES_MAIN_WINDOW_WIDGET);
  int scr_height = GUI_SCREEN_HEIGHT;

  register int i;

  lives_mt *mt = (lives_mt *)lives_malloc(sizeof(lives_mt));

  if (rte_window != NULL) {
    on_rtew_delete_event(NULL, NULL, NULL);
    lives_widget_destroy(rte_window);
    rte_window = NULL;
  }

  mainw->multitrack = mt;

  mt->frame_pixbuf = NULL;

  mt->is_ready = FALSE;
  mt->tl_marks = NULL;

  mt->timestring[0] = 0;

  mt->idlefunc = 0; // idle function for auto backup
  mt->auto_back_time = 0;

  mt->playing_sel = FALSE;

  mt->in_sensitise = FALSE;

  mt->render_file = mainw->current_file;

  if (mainw->sl_undo_mem == NULL) {
    mt->undo_mem = (uint8_t *)lives_malloc(prefs->mt_undo_buf * 1024 * 1024);
    if (mt->undo_mem == NULL) {
      do_mt_undo_mem_error();
    }
    mt->undo_buffer_used = 0;
    mt->undos = NULL;
    mt->undo_offset = 0;
  } else {
    mt->undo_mem = mainw->sl_undo_mem;
    mt->undo_buffer_used = mainw->sl_undo_buffer_used;
    mt->undos = mainw->stored_layout_undos;
    mt->undo_offset = mainw->sl_undo_offset;
  }

  mt->preview_layer = -1000000;

  mt->solo_inst = NULL;

  mt->apply_fx_button = NULL;
  mt->resetp_button = NULL;

  mt->cursor_style = LIVES_CURSOR_NORMAL;

  mt->file_selected = orig_file;

  mt->auto_changed = mt->changed = FALSE;

  mt->was_undo_redo = FALSE;

  mt->tl_mouse = FALSE;

  mt->sel_locked = FALSE;

  mt->clip_labels = NULL;

  mt->force_load_name = NULL;

  mt->dumlabel1 = mt->dumlabel2 = mt->tl_label = mt->timeline = mt->timeline_eb = mt->timeline_reg = NULL;

  mt->play_width = mt->play_height = 0;

  if (mainw->multi_opts.set) {
    lives_memcpy(&mt->opts, &mainw->multi_opts, sizeof(mt->opts));
    mt->opts.aparam_view_list = lives_list_copy(mainw->multi_opts.aparam_view_list);
  } else {
    mt->opts.move_effects = TRUE;
    mt->opts.fx_auto_preview = TRUE;
    mt->opts.snap_over = FALSE;
    mt->opts.mouse_mode = MOUSE_MODE_MOVE;
    mt->opts.show_audio = TRUE;
    mt->opts.show_ctx = TRUE;
    mt->opts.ign_ins_sel = FALSE;
    mt->opts.follow_playback = TRUE;
    mt->opts.grav_mode = GRAV_MODE_NORMAL;
    mt->opts.insert_mode = INSERT_MODE_NORMAL;
    mt->opts.autocross_audio = TRUE;
    mt->opts.render_vidp = TRUE;
    mt->opts.render_audp = TRUE;
    mt->opts.normalise_audp = TRUE;
    mt->opts.aparam_view_list = NULL;
    mt->opts.hpaned_pos = mt->opts.vpaned_pos = -1;
    mt->opts.overlay_timecode = TRUE;
  }

  mt->opts.insert_audio = TRUE;

  mt->opts.pertrack_audio = prefs->mt_pertrack_audio;
  mt->opts.audio_bleedthru = FALSE;
  mt->opts.gang_audio = TRUE;
  mt->opts.back_audio_tracks = 1;

  if (force_pertrack_audio) mt->opts.pertrack_audio = TRUE;
  force_pertrack_audio = FALSE;

  mt->tl_fixed_length = 0.;
  mt->ptr_time = 0.;
  mt->video_draws = NULL;
  mt->block_selected = NULL;
  mt->event_list = event_list;
  mt->accel_group = LIVES_ACCEL_GROUP(lives_accel_group_new());
  lives_window_remove_accel_group(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET), mainw->accel_group);
  lives_window_add_accel_group(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET), mt->accel_group);

  mt->fps = fps;
  mt->hotspot_x = mt->hotspot_y = 0;
  mt->redraw_block = FALSE;

  mt->region_start = mt->region_end = 0.;
  mt->region_updating = FALSE;
  mt->is_rendering = FALSE;
  mt->pr_audio = FALSE;
  mt->selected_tracks = NULL;
  mt->mt_frame_preview = FALSE;
  mt->current_rfx = NULL;
  mt->current_fx = -1;
  mt->putative_block = NULL;
  mt->specific_event = NULL;

  mt->block_tl_move = FALSE;
  mt->block_node_spin = FALSE;

  mt->is_atrans = FALSE;

  mt->last_fx_type = MT_LAST_FX_NONE;

  mt->display = mainw->mgeom[widget_opts.monitor].disp;

  mt->moving_block = FALSE;

  mt->insert_start = mt->insert_end = -1;
  mt->insert_avel = 1.;

  mt->selected_init_event = mt->init_event = NULL;

  mt->auto_reloading = FALSE;
  mt->fm_edit_event = NULL;

  mt->nb_label = NULL;
  mt->fx_list_box = NULL;
  mt->fx_list_scroll = NULL;
  mt->fx_list_label = NULL;

  mt->moving_fx = NULL;
  mt->fx_order = FX_ORD_NONE;

  lives_memset(mt->layout_name, 0, 1);

  mt->did_backup = FALSE;
  mt->framedraw = NULL;

  mt->audio_draws = NULL;
  mt->audio_vols = mt->audio_vols_back = NULL;
  mt->amixer = NULL;
  mt->ignore_load_vals = FALSE;

  mt->exact_preview = 0;

  mt->context_time = -1.;
  mt->use_context = FALSE;

  mt->no_expose = mt->no_expose_frame = TRUE;

  mt->is_paused = FALSE;

  mt->pb_start_event = NULL;

  mt->aud_track_selected = FALSE;

  mt->has_audio_file = FALSE;

  mt->fx_params_label = NULL;
  mt->fx_box = NULL;

  mt->selected_filter = -1;

  mt->top_track = 0;

  mt->cb_list = NULL;

  mt->no_frame_update = FALSE;

  if (mainw->fx_candidates[FX_CANDIDATE_AUDIO_VOL].delegate != -1) {
    // user (or system) has delegated an audio volume filter from the candidates
    mt->avol_fx = LIVES_POINTER_TO_INT(lives_list_nth_data(mainw->fx_candidates[FX_CANDIDATE_AUDIO_VOL].list,
                                       mainw->fx_candidates[FX_CANDIDATE_AUDIO_VOL].delegate));
  } else mt->avol_fx = -1;
  mt->avol_init_event = NULL;

  if (prefs->mt_enter_prompt && rdet != NULL) {
    mt->user_width = rdet->width;
    mt->user_height = rdet->height;
    mt->user_fps = rdet->fps;
    mt->user_arate = xarate;
    mt->user_achans = xachans;
    mt->user_asamps = xasamps;
    mt->user_signed_endian = xse;
    mt->opts.pertrack_audio = ptaud;
    mt->opts.back_audio_tracks = btaud;
  }

  if (rdet != NULL) {
    lives_free(rdet->encoder_name);
    lives_freep((void **)&rdet);
    lives_freep((void **)&resaudw);
  }

  if (force_backing_tracks > mt->opts.back_audio_tracks) mt->opts.back_audio_tracks = force_backing_tracks;
  force_backing_tracks = 0;

  mt->top_vbox = lives_vbox_new(FALSE, 0);

  mt->xtravbox = lives_vbox_new(FALSE, 0);

  mt->menu_hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(mt->top_vbox), mt->menu_hbox, FALSE, FALSE, 0);

  lives_box_pack_start(LIVES_BOX(mt->top_vbox), mt->xtravbox, TRUE, TRUE, 6);

  mt->menubar = lives_menu_bar_new();
  lives_box_pack_start(LIVES_BOX(mt->menu_hbox), mt->menubar, FALSE, FALSE, 0);

  // File
  menuitem = lives_standard_menu_item_new_with_label(_("_File"));
  lives_container_add(LIVES_CONTAINER(mt->menubar), menuitem);

  mt->files_menu = lives_menu_new();
  lives_menu_item_set_submenu(LIVES_MENU_ITEM(menuitem), mt->files_menu);

  mt->open_menu = lives_standard_menu_item_new_with_label(_("_Open..."));
  lives_container_add(LIVES_CONTAINER(mt->files_menu), mt->open_menu);

  menuitem_menu = lives_menu_new();
  lives_menu_item_set_submenu(LIVES_MENU_ITEM(mt->open_menu), menuitem_menu);

  menuitem = lives_standard_menu_item_new_with_label(_("_Open File/Directory"));
  lives_container_add(LIVES_CONTAINER(menuitem_menu), menuitem);

  lives_signal_connect(LIVES_GUI_OBJECT(menuitem), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_open_activate),
                       NULL);

  menuitem = lives_standard_menu_item_new_with_label(_("O_pen File Selection..."));
  lives_container_add(LIVES_CONTAINER(menuitem_menu), menuitem);

  lives_signal_connect(LIVES_GUI_OBJECT(menuitem), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_open_sel_activate),
                       NULL);

  // TODO, show these options but show error if no mplayer / mplayer2

#ifdef HAVE_WEBM
  mt->open_loc_menu = lives_standard_menu_item_new_with_label(_("Open _Location/Stream..."));
  lives_container_add(LIVES_CONTAINER(menuitem_menu), mt->open_loc_menu);

  open_loc_submenu = lives_menu_new();
  lives_menu_item_set_submenu(LIVES_MENU_ITEM(mt->open_loc_menu), open_loc_submenu);

  menuitem = lives_standard_menu_item_new_with_label(_("Open _Youtube Clip..."));
  lives_container_add(LIVES_CONTAINER(open_loc_submenu), menuitem);

  lives_signal_connect(LIVES_GUI_OBJECT(menuitem), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_open_utube_activate),
                       NULL);

  menuitem = lives_standard_menu_item_new_with_label(_("Open _Location/Stream..."));
  lives_container_add(LIVES_CONTAINER(open_loc_submenu), menuitem);

#else

  menuitem = lives_standard_menu_item_new_with_label(_("Open _Location/Stream..."));
  lives_container_add(LIVES_CONTAINER(menuitem_menu), menuitem);

#endif

  lives_signal_connect(LIVES_GUI_OBJECT(menuitem), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_open_loc_activate),
                       NULL);

#ifdef ENABLE_DVD_GRAB
  mt->vcd_dvd_menu = lives_standard_menu_item_new_with_label(_("Import Selection from _dvd/vcd..."));
  lives_container_add(LIVES_CONTAINER(menuitem_menu), mt->vcd_dvd_menu);
  vcd_dvd_submenu = lives_menu_new();
  lives_menu_item_set_submenu(LIVES_MENU_ITEM(mt->vcd_dvd_menu), vcd_dvd_submenu);

  menuitem = lives_standard_menu_item_new_with_label(_("Import Selection from _dvd"));
  lives_container_add(LIVES_CONTAINER(vcd_dvd_submenu), menuitem);

  lives_signal_connect(LIVES_GUI_OBJECT(menuitem), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_open_vcd_activate),
                       LIVES_INT_TO_POINTER(1));

# endif

  menuitem = lives_standard_menu_item_new_with_label(_("Import Selection from _vcd"));

#ifdef ENABLE_DVD_GRAB
  lives_container_add(LIVES_CONTAINER(vcd_dvd_submenu), menuitem);
#else
  lives_container_add(LIVES_CONTAINER(menuitem_menu), menuitem);
#endif

  lives_signal_connect(LIVES_GUI_OBJECT(menuitem), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_open_vcd_activate),
                       LIVES_INT_TO_POINTER(2));

#ifdef HAVE_LDVGRAB
  mt->device_menu = lives_standard_menu_item_new_with_label(_("_Import from Device"));
  lives_container_add(LIVES_CONTAINER(menuitem_menu), mt->device_menu);
  device_submenu = lives_menu_new();

  lives_menu_item_set_submenu(LIVES_MENU_ITEM(mt->device_menu), device_submenu);

  if (capable->has_mplayer || capable->has_mplayer2) {

    menuitem = lives_standard_menu_item_new_with_label(_("Import from _Firewire Device (dv)"));
    lives_container_add(LIVES_CONTAINER(device_submenu), menuitem);

    lives_signal_connect(LIVES_GUI_OBJECT(menuitem), LIVES_WIDGET_ACTIVATE_SIGNAL,
                         LIVES_GUI_CALLBACK(on_open_fw_activate),
                         LIVES_INT_TO_POINTER(CAM_FORMAT_DV));

    menuitem = lives_standard_menu_item_new_with_label(_("Import from _Firewire Device (hdv)"));
    lives_container_add(LIVES_CONTAINER(device_submenu), menuitem);

    lives_signal_connect(LIVES_GUI_OBJECT(menuitem), LIVES_WIDGET_ACTIVATE_SIGNAL,
                         LIVES_GUI_CALLBACK(on_open_fw_activate),
                         LIVES_INT_TO_POINTER(CAM_FORMAT_HDV));
  }
#endif

  mt->close = lives_standard_menu_item_new_with_label(_("_Close the Selected Clip"));
  lives_container_add(LIVES_CONTAINER(mt->files_menu), mt->close);

  lives_signal_connect(LIVES_GUI_OBJECT(mt->close), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_close_activate),
                       NULL);

  lives_widget_set_sensitive(mt->close, FALSE);

  lives_widget_add_accelerator(mt->close, LIVES_WIDGET_ACTIVATE_SIGNAL, mt->accel_group,
                               LIVES_KEY_w, LIVES_CONTROL_MASK,
                               LIVES_ACCEL_VISIBLE);

  mt->recent_menu = lives_standard_menu_item_new_with_label(_("_Recent Files..."));
  lives_container_add(LIVES_CONTAINER(mt->files_menu), mt->recent_menu);
  recent_submenu = lives_menu_new();
  lives_menu_item_set_submenu(LIVES_MENU_ITEM(mt->recent_menu), recent_submenu);

  lives_memset(buff, 0, 1);
  widget_opts.mnemonic_label = FALSE;

  for (i = 0; i < N_RECENT_FILES; i++) {
    char *prefname = lives_strdup_printf("%s%d", PREF_RECENT, i + 1);
    get_utf8_pref(prefname, buff, PATH_MAX * 2);
    lives_free(prefname);
    mt->recent[i] = lives_standard_menu_item_new_with_label(buff);
    lives_container_add(LIVES_CONTAINER(mainw->recent_submenu), mainw->recent[i]);
  }

  widget_opts.mnemonic_label = TRUE;

  lives_container_add(LIVES_CONTAINER(recent_submenu), mt->recent[0]);
  lives_container_add(LIVES_CONTAINER(recent_submenu), mt->recent[1]);
  lives_container_add(LIVES_CONTAINER(recent_submenu), mt->recent[2]);
  lives_container_add(LIVES_CONTAINER(recent_submenu), mt->recent[3]);

  lives_signal_connect(LIVES_GUI_OBJECT(mt->recent[0]), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_recent_activate),
                       LIVES_INT_TO_POINTER(1));
  lives_signal_connect(LIVES_GUI_OBJECT(mt->recent[1]), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_recent_activate),
                       LIVES_INT_TO_POINTER(2));
  lives_signal_connect(LIVES_GUI_OBJECT(mt->recent[2]), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_recent_activate),
                       LIVES_INT_TO_POINTER(3));
  lives_signal_connect(LIVES_GUI_OBJECT(mt->recent[3]), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_recent_activate),
                       LIVES_INT_TO_POINTER(4));

  lives_menu_add_separator(LIVES_MENU(mt->files_menu));

  mt->load_set = lives_standard_menu_item_new_with_label(_("_Reload Clip Set..."));
  lives_container_add(LIVES_CONTAINER(mt->files_menu), mt->load_set);

  lives_signal_connect(LIVES_GUI_OBJECT(mt->load_set), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_load_set_activate),
                       NULL);

  mt->save_set = lives_standard_menu_item_new_with_label(_("Close/Sa_ve All Clips"));
  lives_widget_set_sensitive(mt->save_set, FALSE);
  lives_container_add(LIVES_CONTAINER(mt->files_menu), mt->save_set);

  lives_signal_connect(LIVES_GUI_OBJECT(mt->save_set), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_quit_activate),
                       LIVES_INT_TO_POINTER(1));

  lives_menu_add_separator(LIVES_MENU(mt->files_menu));

  mt->save_event_list = lives_standard_image_menu_item_new_with_label(_("_Save Layout as..."));
  lives_container_add(LIVES_CONTAINER(mt->files_menu), mt->save_event_list);
  lives_widget_set_sensitive(mt->save_event_list, FALSE);

  lives_widget_add_accelerator(mt->save_event_list, LIVES_WIDGET_ACTIVATE_SIGNAL, mt->accel_group,
                               LIVES_KEY_s, LIVES_CONTROL_MASK,
                               LIVES_ACCEL_VISIBLE);

  mt->load_event_list = lives_standard_image_menu_item_new_with_label(_("_Load Layout..."));
  lives_container_add(LIVES_CONTAINER(mt->files_menu), mt->load_event_list);
  lives_widget_set_sensitive(mt->load_event_list, strlen(mainw->set_name) > 0);

  mt->clear_event_list = lives_standard_image_menu_item_new_with_label(_("_Wipe/Delete Layout..."));
  lives_container_add(LIVES_CONTAINER(mt->files_menu), mt->clear_event_list);

  lives_widget_add_accelerator(mt->clear_event_list, LIVES_WIDGET_ACTIVATE_SIGNAL, mt->accel_group,
                               LIVES_KEY_d, LIVES_CONTROL_MASK,
                               LIVES_ACCEL_VISIBLE);

  lives_widget_set_sensitive(mt->clear_event_list, mt->event_list != NULL);

  lives_menu_add_separator(LIVES_MENU(mt->files_menu));

  mt->clear_ds = lives_standard_menu_item_new_with_label(_("Clean _up Diskspace"));
  lives_container_add(LIVES_CONTAINER(mt->files_menu), mt->clear_ds);

  lives_signal_connect(LIVES_GUI_OBJECT(mt->clear_ds), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_cleardisk_activate),
                       NULL);

  lives_menu_add_separator(LIVES_MENU(mt->files_menu));

  mt->load_vals = lives_standard_check_menu_item_new_with_label(_("_Ignore Width, Height and Audio Values from Loaded Layouts"),
                  mt->ignore_load_vals);
  lives_container_add(LIVES_CONTAINER(mt->files_menu), mt->load_vals);

  mt->aload_subs = lives_standard_check_menu_item_new_with_label(_("Auto Load _Subtitles with Clips"), prefs->autoload_subs);
  lives_container_add(LIVES_CONTAINER(mt->files_menu), mt->aload_subs);

  lives_menu_add_separator(LIVES_MENU(mt->files_menu));

  mt->quit = lives_standard_image_menu_item_new_from_stock(LIVES_STOCK_LABEL_QUIT, mt->accel_group);
  lives_container_add(LIVES_CONTAINER(mt->files_menu), mt->quit);

  lives_widget_add_accelerator(mt->quit, LIVES_WIDGET_ACTIVATE_SIGNAL, mt->accel_group,
                               LIVES_KEY_q, LIVES_CONTROL_MASK,
                               LIVES_ACCEL_VISIBLE);

  // Edit

  menuitem = lives_standard_menu_item_new_with_label(_("_Edit"));
  lives_container_add(LIVES_CONTAINER(mt->menubar), menuitem);

  mt->edit_menu = lives_menu_new();
  lives_menu_item_set_submenu(LIVES_MENU_ITEM(menuitem), mt->edit_menu);

  mt->undo = lives_standard_image_menu_item_new_with_label(_("_Undo"));
  lives_container_add(LIVES_CONTAINER(mt->edit_menu), mt->undo);
  lives_widget_set_sensitive(mt->undo, FALSE);

  lives_widget_add_accelerator(mt->undo, LIVES_WIDGET_ACTIVATE_SIGNAL, mt->accel_group,
                               LIVES_KEY_u, LIVES_CONTROL_MASK,
                               LIVES_ACCEL_VISIBLE);

#if LIVES_HAS_IMAGE_MENU_ITEM
  image = lives_image_new_from_stock(LIVES_STOCK_UNDO, LIVES_ICON_SIZE_MENU);
  lives_image_menu_item_set_image(LIVES_IMAGE_MENU_ITEM(mt->undo), image);
#endif

  if (mt->undo_offset == lives_list_length(mt->undos)) mt_set_undoable(mt, MT_UNDO_NONE, NULL, FALSE);
  else {
    mt_undo *undo = (mt_undo *)(lives_list_nth_data(mt->undos, lives_list_length(mt->undos) - mt->undo_offset - 1));
    mt_set_undoable(mt, undo->action, undo->extra, TRUE);
  }

  lives_signal_connect(LIVES_GUI_OBJECT(mt->undo), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(multitrack_undo),
                       (livespointer)mt);

  mt->redo = lives_standard_image_menu_item_new_with_label(_("_Redo"));
  lives_container_add(LIVES_CONTAINER(mt->edit_menu), mt->redo);
  lives_widget_set_sensitive(mt->redo, FALSE);

  lives_widget_add_accelerator(mt->redo, LIVES_WIDGET_ACTIVATE_SIGNAL, mt->accel_group,
                               LIVES_KEY_z, LIVES_CONTROL_MASK,
                               LIVES_ACCEL_VISIBLE);

#if LIVES_HAS_IMAGE_MENU_ITEM
  image = lives_image_new_from_stock(LIVES_STOCK_REDO, LIVES_ICON_SIZE_MENU);
  lives_image_menu_item_set_image(LIVES_IMAGE_MENU_ITEM(mt->redo), image);
#endif

  if (mt->undo_offset <= 1) mt_set_redoable(mt, MT_UNDO_NONE, NULL, FALSE);
  else {
    mt_undo *redo = (mt_undo *)(lives_list_nth_data(mt->undos, lives_list_length(mt->undos) - mt->undo_offset));
    mt_set_redoable(mt, redo->action, redo->extra, TRUE);
  }

  lives_signal_connect(LIVES_GUI_OBJECT(mt->redo), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(multitrack_redo),
                       (livespointer)mt);

  lives_menu_add_separator(LIVES_MENU(mt->edit_menu));

  mt->clipedit = lives_standard_image_menu_item_new_with_label(_("_CLIP EDITOR"));
  lives_container_add(LIVES_CONTAINER(mt->edit_menu), mt->clipedit);

  lives_widget_add_accelerator(mt->clipedit, LIVES_WIDGET_ACTIVATE_SIGNAL, mt->accel_group,
                               LIVES_KEY_e, LIVES_CONTROL_MASK,
                               LIVES_ACCEL_VISIBLE);

  lives_menu_add_separator(LIVES_MENU(mt->edit_menu));

  mt->adjust_start_end = lives_standard_image_menu_item_new_with_label(_("_Adjust Selected Clip Start/End Points"));
  lives_container_add(LIVES_CONTAINER(mt->edit_menu), mt->adjust_start_end);

  lives_widget_add_accelerator(mt->adjust_start_end, LIVES_WIDGET_ACTIVATE_SIGNAL, mt->accel_group,
                               LIVES_KEY_x, LIVES_CONTROL_MASK,
                               LIVES_ACCEL_VISIBLE);
  lives_widget_set_sensitive(mt->adjust_start_end, FALSE);

  mt->insert = lives_standard_image_menu_item_new_with_label(_("_Insert selected clip"));
  lives_container_add(LIVES_CONTAINER(mt->edit_menu), mt->insert);

  lives_widget_add_accelerator(mt->insert, LIVES_WIDGET_ACTIVATE_SIGNAL, mt->accel_group,
                               LIVES_KEY_i, (LiVESXModifierType)0,
                               LIVES_ACCEL_VISIBLE);
  lives_widget_add_accelerator(mt->insert, LIVES_WIDGET_ACTIVATE_SIGNAL, mt->accel_group,
                               LIVES_KEY_i, LIVES_CONTROL_MASK,
                               (LiVESAccelFlags)0);
  lives_widget_set_sensitive(mt->insert, FALSE);

  mt->audio_insert = lives_standard_image_menu_item_new_with_label(_("_Insert Selected Clip Audio"));
  lives_container_add(LIVES_CONTAINER(mt->edit_menu), mt->audio_insert);

  lives_widget_add_accelerator(mt->audio_insert, LIVES_WIDGET_ACTIVATE_SIGNAL, mt->accel_group,
                               LIVES_KEY_i, LIVES_CONTROL_MASK,
                               LIVES_ACCEL_VISIBLE);
  lives_widget_set_sensitive(mt->audio_insert, FALSE);

  mt->delblock = lives_standard_image_menu_item_new_with_label(_("_Delete Selected Block"));
  lives_container_add(LIVES_CONTAINER(mt->edit_menu), mt->delblock);
  lives_widget_set_sensitive(mt->delblock, FALSE);

  // TODO
  /*
    lives_widget_add_accelerator (mt->delblock, LIVES_WIDGET_ACTIVATE_SIGNAL, mt->accel_group,
                              LIVES_KEY_d, LIVES_CONTROL_MASK,
                              LIVES_ACCEL_VISIBLE);
  */

  mt->jumpback = lives_standard_image_menu_item_new_with_label(_("_Jump to Previous Block Boundary"));
  lives_container_add(LIVES_CONTAINER(mt->edit_menu), mt->jumpback);

  lives_widget_add_accelerator(mt->jumpback, LIVES_WIDGET_ACTIVATE_SIGNAL, mt->accel_group,
                               LIVES_KEY_j, LIVES_CONTROL_MASK,
                               LIVES_ACCEL_VISIBLE);

  lives_widget_set_sensitive(mt->jumpback, FALSE);

  mt->jumpnext = lives_standard_image_menu_item_new_with_label(_("_Jump to Next Block Boundary"));
  lives_container_add(LIVES_CONTAINER(mt->edit_menu), mt->jumpnext);

  lives_widget_add_accelerator(mt->jumpnext, LIVES_WIDGET_ACTIVATE_SIGNAL, mt->accel_group,
                               LIVES_KEY_l, LIVES_CONTROL_MASK,
                               LIVES_ACCEL_VISIBLE);

  lives_widget_set_sensitive(mt->jumpnext, FALSE);

  lives_menu_add_separator(LIVES_MENU(mt->edit_menu));

  mt->mark_jumpback = lives_standard_image_menu_item_new_with_label(_("_Jump to Previous Timeline Mark"));
  lives_container_add(LIVES_CONTAINER(mt->edit_menu), mt->mark_jumpback);

  lives_widget_set_sensitive(mt->mark_jumpback, FALSE);

  lives_widget_add_accelerator(mt->mark_jumpback, LIVES_WIDGET_ACTIVATE_SIGNAL, mt->accel_group,
                               LIVES_KEY_j, (LIVES_CONTROL_MASK | LIVES_SHIFT_MASK),
                               LIVES_ACCEL_VISIBLE);

  mt->mark_jumpnext = lives_standard_image_menu_item_new_with_label(_("_Jump to Next Timeline Mark"));
  lives_container_add(LIVES_CONTAINER(mt->edit_menu), mt->mark_jumpnext);

  lives_widget_set_sensitive(mt->mark_jumpnext, FALSE);

  lives_widget_add_accelerator(mt->mark_jumpnext, LIVES_WIDGET_ACTIVATE_SIGNAL, mt->accel_group,
                               LIVES_KEY_l, (LIVES_CONTROL_MASK | LIVES_SHIFT_MASK),
                               LIVES_ACCEL_VISIBLE);


  mt->clear_marks = lives_standard_image_menu_item_new_with_label(_("Clear _Marks from Timeline"));
  lives_container_add(LIVES_CONTAINER(mt->edit_menu), mt->clear_marks);
  lives_widget_set_sensitive(mt->clear_marks, FALSE);

  lives_menu_add_separator(LIVES_MENU(mt->edit_menu));

  ign_ins_sel = lives_standard_check_menu_item_new_with_label(_("Ignore Selection Limits when Inserting"), mt->opts.ign_ins_sel);
  lives_container_add(LIVES_CONTAINER(mt->edit_menu), ign_ins_sel);

  // Play

  menuitem = lives_standard_menu_item_new_with_label(_("_Play"));
  lives_container_add(LIVES_CONTAINER(mt->menubar), menuitem);

  mt->play_menu = lives_menu_new();
  lives_menu_item_set_submenu(LIVES_MENU_ITEM(menuitem), mt->play_menu);

  mt->playall = lives_standard_image_menu_item_new_with_label(_("_Play from Timeline Position"));
  lives_widget_add_accelerator(mt->playall, LIVES_WIDGET_ACTIVATE_SIGNAL, mt->accel_group,
                               LIVES_KEY_p, (LiVESXModifierType)0,
                               LIVES_ACCEL_VISIBLE);
  lives_widget_set_sensitive(mt->playall, FALSE);

  lives_container_add(LIVES_CONTAINER(mt->play_menu), mt->playall);

#if LIVES_HAS_IMAGE_MENU_ITEM
  image = lives_image_new_from_stock(LIVES_STOCK_REFRESH, LIVES_ICON_SIZE_MENU);
  lives_image_menu_item_set_image(LIVES_IMAGE_MENU_ITEM(mt->playall), image);
#endif

  mt->playsel = lives_standard_image_menu_item_new_with_label(_("Pla_y Selected Time Only"));
  lives_widget_add_accelerator(mt->playsel, LIVES_WIDGET_ACTIVATE_SIGNAL, mt->accel_group,
                               LIVES_KEY_y, (LiVESXModifierType)0,
                               LIVES_ACCEL_VISIBLE);
  lives_container_add(LIVES_CONTAINER(mt->play_menu), mt->playsel);
  lives_widget_set_sensitive(mt->playsel, FALSE);

  mt->stop = lives_standard_image_menu_item_new_with_label(_("_Stop"));
  lives_container_add(LIVES_CONTAINER(mt->play_menu), mt->stop);
  lives_widget_set_sensitive(mt->stop, FALSE);
  lives_widget_add_accelerator(mt->stop, LIVES_WIDGET_ACTIVATE_SIGNAL, mt->accel_group,
                               LIVES_KEY_q, (LiVESXModifierType)0,
                               LIVES_ACCEL_VISIBLE);

#if LIVES_HAS_IMAGE_MENU_ITEM
  image = lives_image_new_from_stock(LIVES_STOCK_MEDIA_STOP, LIVES_ICON_SIZE_MENU);
  lives_image_menu_item_set_image(LIVES_IMAGE_MENU_ITEM(mt->stop), image);
#endif

  mt->rewind = lives_standard_image_menu_item_new_with_label(_("Re_wind"));
  lives_container_add(LIVES_CONTAINER(mt->play_menu), mt->rewind);

#if LIVES_HAS_IMAGE_MENU_ITEM
  image = lives_image_new_from_stock(LIVES_STOCK_MEDIA_REWIND, LIVES_ICON_SIZE_MENU);
  lives_image_menu_item_set_image(LIVES_IMAGE_MENU_ITEM(mt->rewind), image);
#endif

  lives_widget_set_sensitive(mt->rewind, FALSE);

  lives_widget_add_accelerator(mt->rewind, LIVES_WIDGET_ACTIVATE_SIGNAL, mt->accel_group,
                               LIVES_KEY_w, (LiVESXModifierType)0,
                               LIVES_ACCEL_VISIBLE);

  lives_menu_add_separator(LIVES_MENU(mt->play_menu));

  full_screen = lives_standard_check_menu_item_new_with_label(_("_Full Screen"), mainw->fs);
  lives_container_add(LIVES_CONTAINER(mt->play_menu), full_screen);

  lives_widget_add_accelerator(full_screen, LIVES_WIDGET_ACTIVATE_SIGNAL, mt->accel_group,
                               LIVES_KEY_f, (LiVESXModifierType)0,
                               LIVES_ACCEL_VISIBLE);

  mt->sepwin = lives_standard_check_menu_item_new_with_label(_("Play in _Separate Window"), mainw->sep_win);
  lives_container_add(LIVES_CONTAINER(mt->play_menu), mt->sepwin);

  lives_widget_add_accelerator(mt->sepwin, LIVES_WIDGET_ACTIVATE_SIGNAL, mt->accel_group,
                               LIVES_KEY_s, (LiVESXModifierType)0,
                               LIVES_ACCEL_VISIBLE);

  mt->loop_continue = lives_standard_check_menu_item_new_with_label(_("L_oop Continuously"), mainw->loop_cont);
  lives_container_add(LIVES_CONTAINER(mt->play_menu), mt->loop_continue);

  lives_widget_add_accelerator(mt->loop_continue, LIVES_WIDGET_ACTIVATE_SIGNAL, mt->accel_group,
                               LIVES_KEY_o, (LiVESXModifierType)0,
                               LIVES_ACCEL_VISIBLE);

  mt->mute_audio = lives_standard_check_menu_item_new_with_label(_("_Mute"), mainw->mute);
  lives_container_add(LIVES_CONTAINER(mt->play_menu), mt->mute_audio);

  lives_widget_add_accelerator(mt->mute_audio, LIVES_WIDGET_ACTIVATE_SIGNAL, mt->accel_group,
                               LIVES_KEY_z, (LiVESXModifierType)0,
                               LIVES_ACCEL_VISIBLE);

  // Effects

  menuitem = lives_standard_menu_item_new_with_label(_("Effect_s"));
  lives_container_add(LIVES_CONTAINER(mt->menubar), menuitem);

  mt->effects_menu = lives_menu_new();
  lives_menu_item_set_submenu(LIVES_MENU_ITEM(menuitem), mt->effects_menu);

  mt->move_fx = lives_standard_check_menu_item_new_with_label(_("_Move Effects with Blocks"), mt->opts.move_effects);
  lives_container_add(LIVES_CONTAINER(mt->effects_menu), mt->move_fx);

  lives_signal_connect_after(LIVES_GUI_OBJECT(mt->move_fx), LIVES_WIDGET_TOGGLED_SIGNAL,
                             LIVES_GUI_CALLBACK(on_move_fx_changed),
                             (livespointer)mt);

  lives_menu_add_separator(LIVES_MENU(mt->effects_menu));

  mt->atrans_menuitem = lives_standard_menu_item_new_with_label(_("Select _Autotransition Effect..."));
  lives_container_add(LIVES_CONTAINER(mt->effects_menu), mt->atrans_menuitem);

  mt->submenu_atransfx = lives_menu_new();
  lives_menu_item_set_submenu(LIVES_MENU_ITEM(mt->atrans_menuitem), mt->submenu_atransfx);

  mt->ac_audio_check = lives_standard_check_menu_item_new_with_label(_("Crossfade Audio with Autotransition"),
                       mt->opts.autocross_audio);
  lives_container_add(LIVES_CONTAINER(mt->effects_menu), mt->ac_audio_check);

  lives_menu_add_separator(LIVES_MENU(mt->effects_menu));

  mt->fx_edit = lives_standard_menu_item_new_with_label(_("View/_Edit Selected Effect"));
  lives_container_add(LIVES_CONTAINER(mt->effects_menu), mt->fx_edit);
  lives_widget_set_sensitive(mt->fx_edit, FALSE);

  mt->fx_delete = lives_standard_menu_item_new_with_label(_("_Delete Selected Effect"));
  lives_container_add(LIVES_CONTAINER(mt->effects_menu), mt->fx_delete);
  lives_widget_set_sensitive(mt->fx_delete, FALSE);

  lives_menu_add_separator(LIVES_MENU(mt->effects_menu));

  //// block effects (video / audio) /////

  mt->fx_block = lives_standard_menu_item_new_with_label(_("Apply Effect to _Block..."));
  lives_container_add(LIVES_CONTAINER(mt->effects_menu), mt->fx_block);

  submenu_menu = lives_menu_new();
  lives_menu_item_set_submenu(LIVES_MENU_ITEM(mt->fx_block), submenu_menu);

  tname = lives_fx_cat_to_text(LIVES_FX_CAT_VIDEO_EFFECT, TRUE); // video effects
  cname = lives_strdup_printf("_%s...", tname);
  lives_free(tname);

  mt->fx_blockv = lives_standard_menu_item_new_with_label(cname);
  mt->fx_region_v = lives_standard_menu_item_new_with_label(cname);

  lives_free(cname);

  lives_container_add(LIVES_CONTAINER(submenu_menu), mt->fx_blockv);

  submenu_menuv = lives_menu_new();
  lives_menu_item_set_submenu(LIVES_MENU_ITEM(mt->fx_blockv), submenu_menuv);

  tname = lives_fx_cat_to_text(LIVES_FX_CAT_AUDIO_EFFECT, TRUE); // audio effects
  cname = lives_strdup_printf("_%s...", tname);
  lives_free(tname);

  mt->fx_blocka = lives_standard_menu_item_new_with_label(cname);
  mt->fx_region_a = lives_standard_menu_item_new_with_label(cname);

  lives_free(cname);

  lives_container_add(LIVES_CONTAINER(submenu_menu), mt->fx_blocka);

  submenu_menua = lives_menu_new();
  lives_menu_item_set_submenu(LIVES_MENU_ITEM(mt->fx_blocka), submenu_menua);

  lives_widget_set_sensitive(mt->fx_blockv, FALSE);
  lives_widget_set_sensitive(mt->fx_blocka, FALSE);
  lives_widget_set_sensitive(mt->fx_block, FALSE);

  /////// region effects (video, audio, va trans, v transitions, a transitions, c)

  mt->fx_region = lives_standard_menu_item_new_with_label(_("Apply Effect to _Region..."));
  lives_container_add(LIVES_CONTAINER(mt->effects_menu), mt->fx_region);

  submenu_menu = lives_menu_new();
  lives_menu_item_set_submenu(LIVES_MENU_ITEM(mt->fx_region), submenu_menu);
  lives_container_add(LIVES_CONTAINER(submenu_menu), mt->fx_region_v);
  lives_container_add(LIVES_CONTAINER(submenu_menu), mt->fx_region_a);

  submenu_menurv = lives_menu_new();
  lives_menu_item_set_submenu(LIVES_MENU_ITEM(mt->fx_region_v), submenu_menurv);

  submenu_menura = lives_menu_new();
  lives_menu_item_set_submenu(LIVES_MENU_ITEM(mt->fx_region_a), submenu_menura);

  tname = lives_fx_cat_to_text(LIVES_FX_CAT_AV_TRANSITION, TRUE); //audio/video transitions
  cname = lives_strdup_printf("_%s...", tname);
  lives_free(tname);

  mt->fx_region_2av = lives_standard_menu_item_new_with_label(cname);
  lives_free(cname);
  lives_container_add(LIVES_CONTAINER(submenu_menu), mt->fx_region_2av);

  submenu_menu2av = lives_menu_new();
  lives_menu_item_set_submenu(LIVES_MENU_ITEM(mt->fx_region_2av), submenu_menu2av);

  tname = lives_fx_cat_to_text(LIVES_FX_CAT_VIDEO_TRANSITION, TRUE); //video only transitions
  cname = lives_strdup_printf("_%s...", tname);
  lives_free(tname);

  mt->fx_region_2v = lives_standard_menu_item_new_with_label(cname);
  lives_free(cname);
  lives_container_add(LIVES_CONTAINER(submenu_menu), mt->fx_region_2v);

  submenu_menu2v = lives_menu_new();
  lives_menu_item_set_submenu(LIVES_MENU_ITEM(mt->fx_region_2v), submenu_menu2v);

  tname = lives_fx_cat_to_text(LIVES_FX_CAT_AUDIO_TRANSITION, TRUE); //audio only transitions
  cname = lives_strdup_printf("_%s...", tname);
  lives_free(tname);

  mt->fx_region_2a = lives_standard_menu_item_new_with_label(cname);
  lives_free(cname);
  lives_container_add(LIVES_CONTAINER(submenu_menu), mt->fx_region_2a);

  submenu_menu2a = lives_menu_new();
  lives_menu_item_set_submenu(LIVES_MENU_ITEM(mt->fx_region_2a), submenu_menu2a);

  tname = lives_fx_cat_to_text(LIVES_FX_CAT_COMPOSITOR, TRUE); // compositors
  cname = lives_strdup_printf("_%s...", tname);
  lives_free(tname);

  mt->fx_region_3 = lives_standard_menu_item_new_with_label(cname);
  lives_free(cname);
  lives_container_add(LIVES_CONTAINER(submenu_menu), mt->fx_region_3);

  submenu_menu3 = lives_menu_new();
  lives_menu_item_set_submenu(LIVES_MENU_ITEM(mt->fx_region_3), submenu_menu3);

  num_filters = rte_get_numfilters();
  widget_opts.mnemonic_label = FALSE;
  for (i = 0; i < num_filters; i++) {
    int sorted = weed_get_sorted_filter(i);
    weed_plant_t *filter = get_weed_filter(sorted);
    if (filter != NULL && !weed_plant_has_leaf(filter, WEED_LEAF_HOST_MENU_HIDE)) {
      LiVESWidget *menuitem;
      char *fxname = NULL;

      if (weed_filter_hints_unstable(filter) && !prefs->unstable_fx) continue;
      pkg = weed_get_package_name(filter);

      if (enabled_in_channels(filter, TRUE) >= 1000000 && enabled_out_channels(filter, FALSE) == 1) {
        fxname = weed_filter_idx_get_name(sorted, TRUE, TRUE, TRUE);
        menuitem = lives_standard_image_menu_item_new_with_label(fxname);
        lives_container_add(LIVES_CONTAINER(submenu_menu3), menuitem);
        lives_widget_object_set_data(LIVES_WIDGET_OBJECT(menuitem), "idx", LIVES_INT_TO_POINTER(sorted));
        lives_signal_connect(LIVES_GUI_OBJECT(menuitem), LIVES_WIDGET_ACTIVATE_SIGNAL,
                             LIVES_GUI_CALLBACK(mt_add_region_effect),
                             (livespointer)mt);
      } else if (enabled_in_channels(filter, FALSE) == 1 && enabled_out_channels(filter, FALSE) == 1) {
        fxname = weed_filter_idx_get_name(sorted, FALSE, FALSE, TRUE);
        // add all filter effects to submenus
        if (!is_pure_audio(filter, FALSE)) {
          if (pkg != NULL) {
            if (xpkgv == NULL || strcmp(pkg, xpkgv)) {
              // create new submenu for pkg
              tname = lives_fx_cat_to_text(LIVES_FX_CAT_VIDEO_EFFECT, TRUE); // video effects
              cname = lives_strdup_printf("%s from package %s...", tname, pkg);
              lives_free(tname);

              menuitem2 = lives_standard_menu_item_new_with_label(cname);

              lives_container_add(LIVES_CONTAINER(submenu_menuv), menuitem2);

              submenu_menuvp = lives_menu_new();
              lives_menu_item_set_submenu(LIVES_MENU_ITEM(menuitem2), submenu_menuvp);

              menuitem2 = lives_standard_menu_item_new_with_label(cname);
              lives_free(cname);

              lives_container_add(LIVES_CONTAINER(submenu_menurv), menuitem2);

              submenu_menurvp = lives_menu_new();
              lives_menu_item_set_submenu(LIVES_MENU_ITEM(menuitem2), submenu_menurvp);
            }
            submenu = submenu_menuvp;
            submenu2 = submenu_menurvp;
            lives_freep((void **)&xpkgv);
            xpkgv = lives_strdup(pkg);
          } else {
            submenu = submenu_menuv;
            submenu2 = submenu_menurv;
          }

          menuitem = lives_standard_image_menu_item_new_with_label(fxname);
          lives_container_add(LIVES_CONTAINER(submenu), menuitem);
          lives_widget_object_set_data(LIVES_WIDGET_OBJECT(menuitem), "idx", LIVES_INT_TO_POINTER(sorted));
          lives_signal_connect(LIVES_GUI_OBJECT(menuitem), LIVES_WIDGET_ACTIVATE_SIGNAL,
                               LIVES_GUI_CALLBACK(mt_add_block_effect),
                               (livespointer)mt);

          menuitem = lives_standard_image_menu_item_new_with_label(fxname);
          lives_container_add(LIVES_CONTAINER(submenu2), menuitem);
          lives_widget_object_set_data(LIVES_WIDGET_OBJECT(menuitem), "idx", LIVES_INT_TO_POINTER(sorted));
          lives_signal_connect(LIVES_GUI_OBJECT(menuitem), LIVES_WIDGET_ACTIVATE_SIGNAL,
                               LIVES_GUI_CALLBACK(mt_add_region_effect),
                               (livespointer)mt);
        } else {
          if (pkg != NULL) {
            if (xpkga == NULL || strcmp(pkg, xpkga)) {
              // create new submenu for pkg
              tname = lives_fx_cat_to_text(LIVES_FX_CAT_AUDIO_EFFECT, TRUE); // audio effects
              cname = lives_strdup_printf("%s from package %s...", tname, pkg);
              lives_free(tname);

              menuitem2 = lives_standard_menu_item_new_with_label(cname);

              lives_container_add(LIVES_CONTAINER(submenu_menua), menuitem2);

              submenu_menuap = lives_menu_new();
              lives_menu_item_set_submenu(LIVES_MENU_ITEM(menuitem2), submenu_menuap);

              menuitem2 = lives_standard_menu_item_new_with_label(cname);
              lives_free(cname);

              lives_container_add(LIVES_CONTAINER(submenu_menura), menuitem2);

              submenu_menurap = lives_menu_new();
              lives_menu_item_set_submenu(LIVES_MENU_ITEM(menuitem2), submenu_menurap);
            }
            submenu = submenu_menuap;
            submenu2 = submenu_menurap;
            lives_freep((void **)&xpkga);
            xpkga = lives_strdup(pkg);
          } else {
            submenu = submenu_menua;
            submenu2 = submenu_menura;
          }

          menuitem = lives_standard_image_menu_item_new_with_label(fxname);
          lives_container_add(LIVES_CONTAINER(submenu), menuitem);
          lives_widget_object_set_data(LIVES_WIDGET_OBJECT(menuitem), "idx", LIVES_INT_TO_POINTER(sorted));
          lives_signal_connect(LIVES_GUI_OBJECT(menuitem), LIVES_WIDGET_ACTIVATE_SIGNAL,
                               LIVES_GUI_CALLBACK(mt_add_block_effect),
                               (livespointer)mt);

          menuitem = lives_standard_image_menu_item_new_with_label(fxname);
          lives_container_add(LIVES_CONTAINER(submenu2), menuitem);
          lives_widget_object_set_data(LIVES_WIDGET_OBJECT(menuitem), "idx", LIVES_INT_TO_POINTER(sorted));
          lives_signal_connect(LIVES_GUI_OBJECT(menuitem), LIVES_WIDGET_ACTIVATE_SIGNAL,
                               LIVES_GUI_CALLBACK(mt_add_region_effect),
                               (livespointer)mt);
        }
      } else if (enabled_in_channels(filter, FALSE) == 2 && enabled_out_channels(filter, FALSE) == 1) {
        fxname = weed_filter_idx_get_name(sorted, FALSE, TRUE, TRUE);
        // add all transitions to submenus
        menuitem = lives_standard_image_menu_item_new_with_label(fxname);
        lives_widget_object_set_data(LIVES_WIDGET_OBJECT(menuitem), "idx", LIVES_INT_TO_POINTER(sorted));
        if (get_transition_param(filter, FALSE) == -1) lives_container_add(LIVES_CONTAINER(submenu_menu2v), menuitem);
        else {
          if (has_video_chans_in(filter, FALSE)) {
            /// the autotransitions menu
            menuitem2 = lives_standard_check_menu_item_new_with_label(fxname, prefs->atrans_fx == sorted);
            lives_widget_object_set_data(LIVES_WIDGET_OBJECT(menuitem2), "idx", LIVES_INT_TO_POINTER(sorted));

            lives_signal_connect(LIVES_GUI_OBJECT(menuitem2), LIVES_WIDGET_ACTIVATE_SIGNAL,
                                 LIVES_GUI_CALLBACK(mt_set_atrans_effect),
                                 (livespointer)mt);
            if (sorted == mainw->def_trans_idx) {
              lives_menu_shell_prepend(LIVES_MENU_SHELL(mt->submenu_atransfx), menuitem2);
            } else lives_menu_shell_append(LIVES_MENU_SHELL(mt->submenu_atransfx), menuitem2);
            /// apply block effect menu
            lives_container_add(LIVES_CONTAINER(submenu_menu2av), menuitem);
          } else lives_container_add(LIVES_CONTAINER(submenu_menu2a), menuitem);
        }
        lives_signal_connect(LIVES_GUI_OBJECT(menuitem), LIVES_WIDGET_ACTIVATE_SIGNAL,
                             LIVES_GUI_CALLBACK(mt_add_region_effect),
                             (livespointer)mt);
      }

      if (pkg != NULL) lives_free(pkg);
      if (fxname != NULL) lives_free(fxname);
    }
  }

  lives_freep((void **)&xpkgv);
  lives_freep((void **)&xpkga);
  widget_opts.mnemonic_label = TRUE;

  /// None autotransition
  menuitem2 = lives_standard_check_menu_item_new_with_label(mainw->string_constants[LIVES_STRING_CONSTANT_NONE],
              prefs->atrans_fx == -1);
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(menuitem2), "idx", LIVES_INT_TO_POINTER(-1));
  lives_menu_shell_prepend(LIVES_MENU_SHELL(mt->submenu_atransfx), menuitem2);

  lives_signal_connect(LIVES_GUI_OBJECT(menuitem2), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(mt_set_atrans_effect),
                       (livespointer)mt);

  lives_widget_set_sensitive(mt->fx_block, FALSE);
  lives_widget_set_sensitive(mt->fx_region, FALSE);

  // Tracks

  menuitem = lives_standard_menu_item_new_with_label(_("_Tracks"));
  lives_container_add(LIVES_CONTAINER(mt->menubar), menuitem);

  mt->tracks_menu = lives_menu_new();
  lives_menu_item_set_submenu(LIVES_MENU_ITEM(menuitem), mt->tracks_menu);

  mt->rename_track = lives_standard_image_menu_item_new_with_label(_("Rename Current Track"));
  lives_container_add(LIVES_CONTAINER(mt->tracks_menu), mt->rename_track);

  lives_menu_add_separator(LIVES_MENU(mt->tracks_menu));

  mt->cback_audio = lives_standard_image_menu_item_new_with_label(_("Make _Backing Audio Current Track"));
  lives_container_add(LIVES_CONTAINER(mt->tracks_menu), mt->cback_audio);

  lives_widget_add_accelerator(mt->cback_audio, LIVES_WIDGET_ACTIVATE_SIGNAL, mt->accel_group,
                               LIVES_KEY_b, LIVES_CONTROL_MASK,
                               LIVES_ACCEL_VISIBLE);

  lives_menu_add_separator(LIVES_MENU(mt->tracks_menu));

  mt->add_vid_behind = lives_standard_image_menu_item_new_with_label(_("Add Video Track at _Rear"));
  lives_container_add(LIVES_CONTAINER(mt->tracks_menu), mt->add_vid_behind);

  lives_widget_add_accelerator(mt->add_vid_behind, LIVES_WIDGET_ACTIVATE_SIGNAL, mt->accel_group,
                               LIVES_KEY_t, LIVES_CONTROL_MASK,
                               LIVES_ACCEL_VISIBLE);

  mt->add_vid_front = lives_standard_image_menu_item_new_with_label(_("Add Video Track at _Front"));
  lives_container_add(LIVES_CONTAINER(mt->tracks_menu), mt->add_vid_front);

  lives_widget_add_accelerator(mt->add_vid_front, LIVES_WIDGET_ACTIVATE_SIGNAL, mt->accel_group,
                               LIVES_KEY_t, (LiVESXModifierType)(LIVES_CONTROL_MASK | LIVES_SHIFT_MASK),
                               LIVES_ACCEL_VISIBLE);

  lives_menu_add_separator(LIVES_MENU(mt->tracks_menu));

  menuitem = lives_standard_menu_item_new_with_label(_("_Split Current Track at Cursor"));
  lives_container_add(LIVES_CONTAINER(mt->tracks_menu), menuitem);

  lives_signal_connect(LIVES_GUI_OBJECT(menuitem), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_split_curr_activate),
                       (livespointer)mt);

  lives_widget_add_accelerator(menuitem, LIVES_WIDGET_ACTIVATE_SIGNAL, mt->accel_group,
                               LIVES_KEY_s, (LiVESXModifierType)LIVES_CONTROL_MASK,
                               LIVES_ACCEL_VISIBLE);

  mt->split_sel = lives_standard_menu_item_new_with_label(_("_Split Selected Video Tracks"));
  lives_container_add(LIVES_CONTAINER(mt->tracks_menu), mt->split_sel);
  lives_widget_set_sensitive(mt->split_sel, FALSE);

  lives_signal_connect(LIVES_GUI_OBJECT(mt->split_sel), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_split_sel_activate),
                       (livespointer)mt);

  lives_menu_add_separator(LIVES_MENU(mt->tracks_menu));

  mt->ins_gap_sel = lives_standard_image_menu_item_new_with_label(_("Insert Gap in Selected Tracks/Time"));
  lives_container_add(LIVES_CONTAINER(mt->tracks_menu), mt->ins_gap_sel);
  lives_widget_set_sensitive(mt->ins_gap_sel, FALSE);

  lives_signal_connect(LIVES_GUI_OBJECT(mt->ins_gap_sel), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_insgap_sel_activate),
                       (livespointer)mt);

  mt->ins_gap_cur = lives_standard_image_menu_item_new_with_label(_("Insert Gap in Current Track/Selected Time"));
  lives_container_add(LIVES_CONTAINER(mt->tracks_menu), mt->ins_gap_cur);
  lives_widget_set_sensitive(mt->ins_gap_cur, FALSE);

  lives_signal_connect(LIVES_GUI_OBJECT(mt->ins_gap_cur), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_insgap_cur_activate),
                       (livespointer)mt);

  lives_menu_add_separator(LIVES_MENU(mt->tracks_menu));

  mt->remove_gaps = lives_standard_menu_item_new_with_label(_("Close All _Gaps in Selected Tracks/Time"));
  lives_container_add(LIVES_CONTAINER(mt->tracks_menu), mt->remove_gaps);

  lives_signal_connect(LIVES_GUI_OBJECT(mt->remove_gaps), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(remove_gaps),
                       (livespointer)mt);

  lives_widget_add_accelerator(mt->remove_gaps, LIVES_WIDGET_ACTIVATE_SIGNAL, mt->accel_group,
                               LIVES_KEY_g, LIVES_CONTROL_MASK,
                               LIVES_ACCEL_VISIBLE);

  mt->remove_first_gaps = lives_standard_menu_item_new_with_label("");
  lives_container_add(LIVES_CONTAINER(mt->tracks_menu), mt->remove_first_gaps);

  lives_signal_connect(LIVES_GUI_OBJECT(mt->remove_first_gaps), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(remove_first_gaps),
                       (livespointer)mt);

  lives_widget_add_accelerator(mt->remove_first_gaps, LIVES_WIDGET_ACTIVATE_SIGNAL, mt->accel_group,
                               LIVES_KEY_f, LIVES_CONTROL_MASK,
                               LIVES_ACCEL_VISIBLE);

  // Selection

  menuitem = lives_standard_menu_item_new_with_label(_("Se_lection"));
  lives_container_add(LIVES_CONTAINER(mt->menubar), menuitem);

  mt->selection_menu = lives_menu_new();
  lives_menu_item_set_submenu(LIVES_MENU_ITEM(menuitem), mt->selection_menu);

  menuitem = lives_standard_check_menu_item_new_with_label(_("_Lock Time Selection"), mt->sel_locked);
  lives_container_add(LIVES_CONTAINER(mt->selection_menu), menuitem);

  lives_signal_connect(LIVES_GUI_OBJECT(menuitem), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(mt_selection_lock),
                       (livespointer)mt);

  mt->select_track = lives_standard_check_menu_item_new_with_label(_("_Select Current Track"), FALSE);
  lives_container_add(LIVES_CONTAINER(mt->selection_menu), mt->select_track);

  lives_widget_add_accelerator(mt->select_track, LIVES_WIDGET_ACTIVATE_SIGNAL, mt->accel_group,
                               LIVES_KEY_Space, LIVES_CONTROL_MASK,
                               LIVES_ACCEL_VISIBLE);

  menuitem = lives_standard_menu_item_new_with_label(_("Select _All Video Tracks"));
  lives_container_add(LIVES_CONTAINER(mt->selection_menu), menuitem);

  lives_signal_connect(LIVES_GUI_OBJECT(menuitem), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(select_all_vid),
                       (livespointer)mt);

  menuitem = lives_standard_menu_item_new_with_label(_("Select _No Video Tracks"));
  lives_container_add(LIVES_CONTAINER(mt->selection_menu), menuitem);

  lives_signal_connect(LIVES_GUI_OBJECT(menuitem), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(select_no_vid),
                       (livespointer)mt);

  menuitem = lives_standard_menu_item_new_with_label(_("Select All _Time"));
  lives_container_add(LIVES_CONTAINER(mt->selection_menu), menuitem);

  lives_signal_connect(LIVES_GUI_OBJECT(menuitem), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(select_all_time),
                       (livespointer)mt);

  lives_widget_add_accelerator(menuitem, LIVES_WIDGET_ACTIVATE_SIGNAL, mt->accel_group,
                               LIVES_KEY_a, LIVES_CONTROL_MASK,
                               LIVES_ACCEL_VISIBLE);

  menuitem = lives_standard_menu_item_new_with_label(_("Select from _Zero Time"));
  lives_container_add(LIVES_CONTAINER(mt->selection_menu), menuitem);

  lives_signal_connect(LIVES_GUI_OBJECT(menuitem), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(select_from_zero_time),
                       (livespointer)mt);

  menuitem = lives_standard_menu_item_new_with_label(_("Select to _End Time"));
  lives_container_add(LIVES_CONTAINER(mt->selection_menu), menuitem);

  lives_signal_connect(LIVES_GUI_OBJECT(menuitem), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(select_to_end_time),
                       (livespointer)mt);

  menuitem = lives_standard_menu_item_new_with_label(_("_Copy..."));
  lives_container_add(LIVES_CONTAINER(mt->selection_menu), menuitem);

  selcopy_menu = lives_menu_new();
  lives_menu_item_set_submenu(LIVES_MENU_ITEM(menuitem), selcopy_menu);

  mt->tc_to_rs = lives_standard_menu_item_new_with_label(_("_Timecode to Region Start"));
  lives_container_add(LIVES_CONTAINER(selcopy_menu), mt->tc_to_rs);

  lives_signal_connect(LIVES_GUI_OBJECT(mt->tc_to_rs), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(tc_to_rs),
                       (livespointer)mt);

  mt->tc_to_re = lives_standard_menu_item_new_with_label(_("_Timecode to Region End"));
  lives_container_add(LIVES_CONTAINER(selcopy_menu), mt->tc_to_re);

  lives_signal_connect(LIVES_GUI_OBJECT(mt->tc_to_re), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(tc_to_re),
                       (livespointer)mt);

  mt->rs_to_tc = lives_standard_menu_item_new_with_label(_("_Region Start to Timecode"));
  lives_container_add(LIVES_CONTAINER(selcopy_menu), mt->rs_to_tc);

  lives_signal_connect(LIVES_GUI_OBJECT(mt->rs_to_tc), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(rs_to_tc),
                       (livespointer)mt);

  mt->re_to_tc = lives_standard_menu_item_new_with_label(_("_Region End to Timecode"));
  lives_container_add(LIVES_CONTAINER(selcopy_menu), mt->re_to_tc);

  lives_signal_connect(LIVES_GUI_OBJECT(mt->re_to_tc), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(re_to_tc),
                       (livespointer)mt);

  lives_widget_set_sensitive(mt->rs_to_tc, FALSE);
  lives_widget_set_sensitive(mt->re_to_tc, FALSE);

  lives_menu_add_separator(LIVES_MENU(mt->selection_menu));

  mt->seldesel_menuitem = lives_standard_menu_item_new_with_label(_("Select/Deselect Block at Current Track/Time"));
  lives_container_add(LIVES_CONTAINER(mt->selection_menu), mt->seldesel_menuitem);

  lives_signal_connect(LIVES_GUI_OBJECT(mt->seldesel_menuitem), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(mt_selblock),
                       (livespointer)mt);

  lives_widget_add_accelerator(mt->seldesel_menuitem, LIVES_WIDGET_ACTIVATE_SIGNAL, mt->accel_group,
                               LIVES_KEY_Return, LIVES_CONTROL_MASK,
                               LIVES_ACCEL_VISIBLE);

  // Tools

  menuitem = lives_standard_menu_item_new_with_label(_("_Tools"));
  lives_container_add(LIVES_CONTAINER(mt->menubar), menuitem);

  mt->tools_menu = lives_menu_new();
  lives_menu_item_set_submenu(LIVES_MENU_ITEM(menuitem), mt->tools_menu);

  mt->change_vals = lives_standard_image_menu_item_new_with_label(_("_Change Width, Height and Audio Values..."));
  lives_container_add(LIVES_CONTAINER(mt->tools_menu), mt->change_vals);

  lives_signal_connect(LIVES_GUI_OBJECT(mt->change_vals), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(mt_change_vals_activate),
                       (livespointer)mt);

  lives_menu_add_separator(LIVES_MENU(mt->tools_menu));

  mt->gens_submenu = lives_standard_menu_item_new_with_label(_("_Generate"));
  lives_container_add(LIVES_CONTAINER(mt->tools_menu), mt->gens_submenu);

  if (mainw->gens_menu) {
    lives_widget_object_ref(mainw->gens_menu);
    lives_menu_detach(LIVES_MENU(mainw->gens_menu));
    lives_menu_item_set_submenu(LIVES_MENU_ITEM(mt->gens_submenu), mainw->gens_menu);
  }

  mt->capture = lives_standard_menu_item_new_with_label(_("Capture _External Window... "));
  lives_container_add(LIVES_CONTAINER(mt->tools_menu), mt->capture);

  lives_signal_connect(LIVES_GUI_OBJECT(mt->capture), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_capture_activate),
                       NULL);

  lives_menu_add_separator(LIVES_MENU(mt->tools_menu));

  mt->backup = lives_standard_image_menu_item_new_with_label(_("_Backup timeline now"));
  lives_container_add(LIVES_CONTAINER(mt->tools_menu), mt->backup);
  lives_widget_set_sensitive(mt->backup, FALSE);

  lives_signal_connect(LIVES_GUI_OBJECT(mt->backup), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_mt_backup_activate),
                       (livespointer)mt);

  lives_menu_add_separator(LIVES_MENU(mt->tools_menu));

  menuitem = lives_standard_image_menu_item_new_with_label(_("_Preferences..."));
  lives_container_add(LIVES_CONTAINER(mt->tools_menu), menuitem);
  lives_widget_add_accelerator(menuitem, LIVES_WIDGET_ACTIVATE_SIGNAL, mt->accel_group,
                               LIVES_KEY_p, LIVES_CONTROL_MASK,
                               LIVES_ACCEL_VISIBLE);

#if LIVES_HAS_IMAGE_MENU_ITEM
  image = lives_image_new_from_stock(LIVES_STOCK_PREFERENCES, LIVES_ICON_SIZE_MENU);
  lives_image_menu_item_set_image(LIVES_IMAGE_MENU_ITEM(menuitem), image);
#endif

  lives_signal_connect(LIVES_GUI_OBJECT(menuitem), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_preferences_activate),
                       NULL);

  // Render

  menuitem = lives_standard_menu_item_new_with_label(_("_Render"));
  lives_container_add(LIVES_CONTAINER(mt->menubar), menuitem);

  mt->render_menu = lives_menu_new();
  lives_menu_item_set_submenu(LIVES_MENU_ITEM(menuitem), mt->render_menu);

  mt->render = lives_standard_image_menu_item_new_with_label(_("_Render All to New Clip"));
  lives_container_add(LIVES_CONTAINER(mt->render_menu), mt->render);
  lives_widget_set_sensitive(mt->render, FALSE);

  lives_widget_add_accelerator(mt->render, LIVES_WIDGET_ACTIVATE_SIGNAL, mt->accel_group,
                               LIVES_KEY_r, LIVES_CONTROL_MASK,
                               LIVES_ACCEL_VISIBLE);

  // TODO - render selected time

  mt->render_sep = lives_standard_menu_item_new();
  lives_container_add(LIVES_CONTAINER(mt->render_menu), mt->render_sep);
  lives_widget_set_sensitive(mt->render_sep, FALSE);

  mt->render_vid = lives_standard_check_menu_item_new_with_label(_("Render _Video"), mt->opts.render_vidp);
  lives_widget_set_sensitive(mt->render_vid, mt->opts.render_audp);

  lives_container_add(LIVES_CONTAINER(mt->render_menu), mt->render_vid);

  mt->render_aud = lives_standard_check_menu_item_new_with_label(_("Render _Audio"), mt->opts.render_audp);

  lives_container_add(LIVES_CONTAINER(mt->render_menu), mt->render_aud);

  sep = lives_standard_menu_item_new();

  lives_container_add(LIVES_CONTAINER(mt->render_menu), sep);
  lives_widget_set_sensitive(sep, FALSE);

  mt->normalise_aud = lives_standard_check_menu_item_new_with_label(_("_Normalise Rendered Audio"), mt->opts.normalise_audp);
  lives_widget_set_sensitive(mt->normalise_aud, mt->opts.render_audp);

  lives_container_add(LIVES_CONTAINER(mt->render_menu), mt->normalise_aud);

  mt->prerender_aud = lives_standard_menu_item_new_with_label(_("_Pre-render Audio"));
  lives_widget_set_sensitive(mt->prerender_aud, FALSE);

  //lives_container_add (LIVES_CONTAINER (mt->render_menu), mt->prerender_aud);

  // View

  menuitem = lives_standard_menu_item_new_with_label(_("_View"));
  lives_container_add(LIVES_CONTAINER(mt->menubar), menuitem);

  mt->view_menu = lives_menu_new();
  lives_menu_item_set_submenu(LIVES_MENU_ITEM(menuitem), mt->view_menu);

  mt->view_clips = lives_standard_menu_item_new_with_label(_("_Clips"));
  lives_container_add(LIVES_CONTAINER(mt->view_menu), mt->view_clips);

  lives_widget_add_accelerator(mt->view_clips, LIVES_WIDGET_ACTIVATE_SIGNAL, mt->accel_group,
                               LIVES_KEY_c, (LiVESXModifierType)0,
                               LIVES_ACCEL_VISIBLE);

  mt->view_in_out = lives_standard_menu_item_new_with_label(_("Block _In/Out Points"));
  lives_container_add(LIVES_CONTAINER(mt->view_menu), mt->view_in_out);

  lives_widget_add_accelerator(mt->view_in_out, LIVES_WIDGET_ACTIVATE_SIGNAL, mt->accel_group,
                               LIVES_KEY_n, (LiVESXModifierType)0,
                               LIVES_ACCEL_VISIBLE);

  lives_widget_set_sensitive(mt->view_in_out, FALSE);

  mt->view_effects = lives_standard_menu_item_new_with_label(_("_Effects at Current"));
  lives_container_add(LIVES_CONTAINER(mt->view_menu), mt->view_effects);

  lives_widget_add_accelerator(mt->view_effects, LIVES_WIDGET_ACTIVATE_SIGNAL, mt->accel_group,
                               LIVES_KEY_e, (LiVESXModifierType)0,
                               LIVES_ACCEL_VISIBLE);

  show_messages = lives_standard_image_menu_item_new_with_label(_("Show _Messages"));
  lives_container_add(LIVES_CONTAINER(mt->view_menu), show_messages);

  mt->aparam_separator = lives_standard_menu_item_new();
  lives_container_add(LIVES_CONTAINER(mt->view_menu), mt->aparam_separator);
  lives_widget_set_sensitive(mt->aparam_separator, FALSE);

  mt->aparam_menuitem = lives_standard_menu_item_new_with_label(_("Audio Parameters"));
  lives_container_add(LIVES_CONTAINER(mt->view_menu), mt->aparam_menuitem);

  mt->aparam_submenu = lives_menu_new();
  lives_menu_item_set_submenu(LIVES_MENU_ITEM(mt->aparam_menuitem), mt->aparam_submenu);

  mt->view_audio = lives_standard_check_menu_item_new_with_label(_("Show Backing _Audio Track"), mt->opts.show_audio);
  lives_container_add(LIVES_CONTAINER(mt->view_menu), mt->view_audio);

  mt->change_max_disp = lives_standard_menu_item_new_with_label(_("Maximum Tracks to Display..."));
  lives_container_add(LIVES_CONTAINER(mt->view_menu), mt->change_max_disp);

  lives_menu_add_separator(LIVES_MENU(mt->view_menu));

  mt->follow_play = lives_standard_check_menu_item_new_with_label(_("Scroll to Follow Playback"), mt->opts.follow_playback);
  lives_container_add(LIVES_CONTAINER(mt->view_menu), mt->follow_play);

  ccursor = lives_standard_menu_item_new_with_label(_("_Center on Cursor"));
  lives_container_add(LIVES_CONTAINER(mt->view_menu), ccursor);

  lives_widget_add_accelerator(ccursor, LIVES_WIDGET_ACTIVATE_SIGNAL, mt->accel_group,
                               LIVES_KEY_c, (LiVESXModifierType)LIVES_CONTROL_MASK,
                               LIVES_ACCEL_VISIBLE);

  zoom_in = lives_standard_menu_item_new_with_label(_("_Zoom In"));
  lives_container_add(LIVES_CONTAINER(mt->view_menu), zoom_in);

  lives_widget_add_accelerator(zoom_in, LIVES_WIDGET_ACTIVATE_SIGNAL, mt->accel_group,
                               LIVES_KEY_Plus, (LiVESXModifierType)LIVES_CONTROL_MASK,
                               LIVES_ACCEL_VISIBLE);

  lives_widget_add_accelerator(zoom_in, LIVES_WIDGET_ACTIVATE_SIGNAL, mt->accel_group,
                               LIVES_KEY_Equal, (LiVESXModifierType)LIVES_CONTROL_MASK,
                               (LiVESAccelFlags)0);

  zoom_out = lives_standard_menu_item_new_with_label(_("_Zoom Out"));
  lives_container_add(LIVES_CONTAINER(mt->view_menu), zoom_out);

  lives_widget_add_accelerator(zoom_out, LIVES_WIDGET_ACTIVATE_SIGNAL, mt->accel_group,
                               LIVES_KEY_Minus, (LiVESXModifierType)LIVES_CONTROL_MASK,
                               LIVES_ACCEL_VISIBLE);

  lives_menu_add_separator(LIVES_MENU(mt->view_menu));

  view_mt_details = lives_standard_menu_item_new_with_label(_("Multitrack _Details"));
  lives_container_add(LIVES_CONTAINER(mt->view_menu), view_mt_details);

  mt->show_layout_errors = lives_standard_image_menu_item_new_with_label(_("Show _Layout Errors"));
  lives_container_add(LIVES_CONTAINER(mt->view_menu), mt->show_layout_errors);
  lives_widget_set_sensitive(mt->show_layout_errors, mainw->affected_layouts_map != NULL);

  lives_menu_add_separator(LIVES_MENU(mt->view_menu));

  mt->view_events = lives_standard_image_menu_item_new_with_label(_("_Event Window"));
  lives_container_add(LIVES_CONTAINER(mt->view_menu), mt->view_events);
  lives_widget_set_sensitive(mt->view_events, FALSE);

  mt->view_sel_events = lives_standard_image_menu_item_new_with_label(_("_Event Window (selected time only)"));
  lives_container_add(LIVES_CONTAINER(mt->view_menu), mt->view_sel_events);
  lives_widget_set_sensitive(mt->view_sel_events, FALSE);

  show_frame_events = lives_standard_check_menu_item_new_with_label(_("_Show FRAME Events in Event Window"),
                      prefs->event_window_show_frame_events);
  lives_container_add(LIVES_CONTAINER(mt->view_menu), show_frame_events);

  // help
  menuitem = lives_standard_menu_item_new_with_label(_("_Help"));
  lives_container_add(LIVES_CONTAINER(mt->menubar), menuitem);

  mt->help_menu = lives_menu_new();
  lives_menu_item_set_submenu(LIVES_MENU_ITEM(menuitem), mt->help_menu);

  show_mt_keys = lives_standard_menu_item_new_with_label(_("_Show Multitrack Keys"));
  lives_container_add(LIVES_CONTAINER(mt->help_menu), show_mt_keys);

  lives_menu_add_separator(LIVES_MENU(mt->help_menu));

  show_manual = lives_standard_menu_item_new_with_label(_("_Manual (opens in browser)"));
  lives_container_add(LIVES_CONTAINER(mt->help_menu), show_manual);

  lives_menu_add_separator(LIVES_MENU(mt->help_menu));

  donate = lives_standard_menu_item_new_with_label(_("_Donate to the Project !"));
  lives_container_add(LIVES_CONTAINER(mt->help_menu), donate);

  email_author = lives_standard_menu_item_new_with_label(_("_Email the Author"));
  lives_container_add(LIVES_CONTAINER(mt->help_menu), email_author);

  report_bug = lives_standard_menu_item_new_with_label(_("Report a _bug"));
  lives_container_add(LIVES_CONTAINER(mt->help_menu), report_bug);

  suggest_feature = lives_standard_menu_item_new_with_label(_("Suggest a _Feature"));
  lives_container_add(LIVES_CONTAINER(mt->help_menu), suggest_feature);

  help_translate = lives_standard_menu_item_new_with_label(_("Assist with _Translating"));
  lives_container_add(LIVES_CONTAINER(mt->help_menu), help_translate);

  lives_menu_add_separator(LIVES_MENU(mt->help_menu));

  mt->show_devopts = lives_standard_check_menu_item_new_with_label(_("Enable Developer Options"), prefs->show_dev_opts);
  lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mt->show_devopts),  prefs->show_dev_opts);

  lives_container_add(LIVES_CONTAINER(mt->help_menu), mt->show_devopts);
  lives_signal_connect(LIVES_GUI_OBJECT(mt->show_devopts), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(toggle_sets_pref),
                       (livespointer)PREF_SHOW_DEVOPTS);

  lives_menu_add_separator(LIVES_MENU(mt->help_menu));

  mt->troubleshoot = lives_standard_menu_item_new_with_label(_("_Troubleshoot"));
  lives_container_add(LIVES_CONTAINER(mt->help_menu), mt->troubleshoot);

  mt->expl_missing = lives_standard_menu_item_new_with_label(_("Check for Missing Features"));
  if (!prefs->vj_mode) lives_container_add(LIVES_CONTAINER(mainw->help_menu), mt->expl_missing);

  lives_menu_add_separator(LIVES_MENU(mt->help_menu));

  about = lives_standard_menu_item_new_with_label(_("_About"));
  lives_container_add(LIVES_CONTAINER(mt->help_menu), about);

  lives_signal_connect(LIVES_GUI_OBJECT(mt->quit), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(mt_quit_activate),
                       (livespointer)mt);
  lives_signal_connect(LIVES_GUI_OBJECT(mt->load_vals), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(mt_load_vals_toggled),
                       (livespointer)mt);
  lives_signal_connect(LIVES_GUI_OBJECT(mt->ac_audio_check), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(mt_ac_audio_toggled),
                       (livespointer)mt);
  lives_signal_connect(LIVES_GUI_OBJECT(mt->aload_subs), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_boolean_toggled),
                       &prefs->autoload_subs);
  lives_signal_connect(LIVES_GUI_OBJECT(mt->clipedit), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(multitrack_end_cb),
                       (livespointer)mt);
  lives_signal_connect(LIVES_GUI_OBJECT(mt->playall), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_playall_activate),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mt->playsel), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(multitrack_play_sel),
                       (livespointer)mt);
  lives_signal_connect(LIVES_GUI_OBJECT(mt->insert), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(multitrack_insert),
                       (livespointer)mt);
  lives_signal_connect(LIVES_GUI_OBJECT(mt->audio_insert), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(multitrack_audio_insert),
                       (livespointer)mt);
  lives_signal_connect(LIVES_GUI_OBJECT(mt->adjust_start_end), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(multitrack_adj_start_end),
                       (livespointer)mt);
  lives_signal_connect(LIVES_GUI_OBJECT(mt->view_events), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(multitrack_view_events),
                       (livespointer)mt);
  lives_signal_connect(LIVES_GUI_OBJECT(mt->view_sel_events), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(multitrack_view_sel_events),
                       (livespointer)mt);
  lives_signal_connect(LIVES_GUI_OBJECT(mt->clear_marks), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(multitrack_clear_marks),
                       (livespointer)mt);
  lives_signal_connect(LIVES_GUI_OBJECT(view_mt_details), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(multitrack_view_details),
                       (livespointer)mt);
  lives_signal_connect(LIVES_GUI_OBJECT(mt->show_layout_errors), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(popup_lmap_errors),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mt->view_clips), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(multitrack_view_clips),
                       (livespointer)mt);
  lives_signal_connect(LIVES_GUI_OBJECT(mt->view_in_out), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(multitrack_view_in_out),
                       (livespointer)mt);
  lives_signal_connect(LIVES_GUI_OBJECT(show_messages), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_show_messages_activate),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mt->stop), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_stop_activate),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mt->rewind), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_rewind_activate),
                       NULL);
  mt->sepwin_func = lives_signal_connect(LIVES_GUI_OBJECT(mt->sepwin), LIVES_WIDGET_ACTIVATE_SIGNAL,
                                         LIVES_GUI_CALLBACK(on_sepwin_activate),
                                         NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(full_screen), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_full_screen_activate),
                       NULL);
  mt->loop_cont_func = lives_signal_connect(LIVES_GUI_OBJECT(mt->loop_continue), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_loop_cont_activate),
                       NULL);
  mt->mute_audio_func = lives_signal_connect(LIVES_GUI_OBJECT(mt->mute_audio), LIVES_WIDGET_ACTIVATE_SIGNAL,
                        LIVES_GUI_CALLBACK(on_mute_activate),
                        NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mt->rename_track), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_rename_track_activate),
                       (livespointer)mt);
  lives_signal_connect(LIVES_GUI_OBJECT(mt->cback_audio), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_cback_audio_activate),
                       (livespointer)mt);
  lives_signal_connect(LIVES_GUI_OBJECT(mt->add_vid_behind), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(add_video_track_behind),
                       (livespointer)mt);
  lives_signal_connect(LIVES_GUI_OBJECT(mt->add_vid_front), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(add_video_track_front),
                       (livespointer)mt);
  lives_signal_connect(LIVES_GUI_OBJECT(mt->render), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_render_activate),
                       (livespointer)mt);
  lives_signal_connect(LIVES_GUI_OBJECT(mt->prerender_aud), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_prerender_aud_activate),
                       (livespointer)mt);
  lives_signal_connect(LIVES_GUI_OBJECT(mt->jumpback), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_jumpback_activate),
                       (livespointer)mt);
  lives_signal_connect(LIVES_GUI_OBJECT(mt->jumpnext), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_jumpnext_activate),
                       (livespointer)mt);
  lives_signal_connect(LIVES_GUI_OBJECT(mt->mark_jumpback), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_jumpback_mark_activate),
                       (livespointer)mt);
  lives_signal_connect(LIVES_GUI_OBJECT(mt->mark_jumpnext), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_jumpnext_mark_activate),
                       (livespointer)mt);
  lives_signal_connect(LIVES_GUI_OBJECT(mt->delblock), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_delblock_activate),
                       (livespointer)mt);
  lives_signal_connect(LIVES_GUI_OBJECT(mt->save_event_list), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_save_event_list_activate),
                       (livespointer)mt);
  lives_signal_connect(LIVES_GUI_OBJECT(mt->load_event_list), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_load_event_list_activate),
                       (livespointer)mt);
  lives_signal_connect(LIVES_GUI_OBJECT(mt->clear_event_list), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_clear_event_list_activate),
                       (livespointer)mt);
  lives_signal_connect(LIVES_GUI_OBJECT(mt->view_audio), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(mt_view_audio_toggled),
                       (livespointer)mt);
  lives_signal_connect(LIVES_GUI_OBJECT(mt->change_max_disp), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(mt_change_max_disp_tracks),
                       (livespointer)mt);
  lives_signal_connect(LIVES_GUI_OBJECT(mt->render_vid), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(mt_render_vid_toggled),
                       (livespointer)mt);
  lives_signal_connect(LIVES_GUI_OBJECT(mt->render_aud), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(mt_render_aud_toggled),
                       (livespointer)mt);
  lives_signal_connect(LIVES_GUI_OBJECT(mt->normalise_aud), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(mt_norm_aud_toggled),
                       (livespointer)mt);
  lives_signal_connect(LIVES_GUI_OBJECT(ign_ins_sel), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(mt_ign_ins_sel_toggled),
                       (livespointer)mt);
  lives_signal_connect(LIVES_GUI_OBJECT(show_frame_events), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(show_frame_events_activate),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(ccursor), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(mt_center_on_cursor),
                       (livespointer)mt);
  lives_signal_connect(LIVES_GUI_OBJECT(mt->follow_play), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(mt_fplay_toggled),
                       (livespointer)mt);
  lives_signal_connect(LIVES_GUI_OBJECT(zoom_in), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(mt_zoom_in),
                       (livespointer)mt);
  lives_signal_connect(LIVES_GUI_OBJECT(zoom_out), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(mt_zoom_out),
                       (livespointer)mt);
  mt->seltrack_func = lives_signal_connect(LIVES_GUI_OBJECT(mt->select_track), LIVES_WIDGET_ACTIVATE_SIGNAL,
                      LIVES_GUI_CALLBACK(on_seltrack_activate),
                      (livespointer)mt);

  lives_signal_connect(LIVES_GUI_OBJECT(show_manual), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(show_manual_activate),
                       NULL);

  lives_signal_connect(LIVES_GUI_OBJECT(email_author), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(email_author_activate),
                       NULL);

  lives_signal_connect(LIVES_GUI_OBJECT(donate), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(donate_activate),
                       NULL);

  lives_signal_connect(LIVES_GUI_OBJECT(report_bug), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(report_bug_activate),
                       NULL);

  lives_signal_connect(LIVES_GUI_OBJECT(suggest_feature), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(suggest_feature_activate),
                       NULL);

  lives_signal_connect(LIVES_GUI_OBJECT(help_translate), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(help_translate_activate),
                       NULL);

  lives_signal_connect(LIVES_GUI_OBJECT(about), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_about_activate),
                       NULL);

  lives_signal_connect(LIVES_GUI_OBJECT(mt->troubleshoot), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_troubleshoot_activate),
                       NULL);

  lives_signal_connect(LIVES_GUI_OBJECT(mt->expl_missing), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(explain_missing_activate),
                       NULL);

  lives_signal_connect(LIVES_GUI_OBJECT(show_mt_keys), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_mt_showkeys_activate),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mt->fx_delete), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_mt_delfx_activate),
                       (livespointer)mt);
  lives_signal_connect(LIVES_GUI_OBJECT(mt->fx_edit), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_mt_fx_edit_activate),
                       (livespointer)mt);
  lives_signal_connect(LIVES_GUI_OBJECT(mt->view_effects), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_mt_list_fx_activate),
                       (livespointer)mt);

  lives_accel_group_connect(LIVES_ACCEL_GROUP(mt->accel_group), LIVES_KEY_m, (LiVESXModifierType)0, (LiVESAccelFlags)0,
                            lives_cclosure_new(LIVES_GUI_CALLBACK(mt_mark_callback), (livespointer)mt, NULL));

  lives_accel_group_connect(LIVES_ACCEL_GROUP(mt->accel_group), LIVES_KEY_t, (LiVESXModifierType)0, (LiVESAccelFlags)0,
                            lives_cclosure_new(LIVES_GUI_CALLBACK(mt_tcoverlay_callback), (livespointer)mt, NULL));

  mt->top_eventbox = lives_event_box_new();
  lives_box_pack_start(LIVES_BOX(mt->xtravbox), mt->top_eventbox, FALSE, FALSE, 0);

  lives_widget_add_events(mt->top_eventbox, LIVES_BUTTON1_MOTION_MASK | LIVES_BUTTON_RELEASE_MASK | LIVES_BUTTON_PRESS_MASK |
                          LIVES_SCROLL_MASK | LIVES_SMOOTH_SCROLL_MASK);

  lives_signal_connect(LIVES_GUI_OBJECT(mt->top_eventbox), LIVES_WIDGET_SCROLL_EVENT,
                       LIVES_GUI_CALLBACK(on_mouse_scroll),
                       mt);

  hbox = lives_hbox_new(FALSE, 0);
  lives_widget_set_bg_color(hbox, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
  lives_widget_set_fg_color(hbox, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars_fore);

  lives_container_add(LIVES_CONTAINER(mt->top_eventbox), hbox);

  mt->btoolbar2 = lives_toolbar_new();
  lives_box_pack_start(LIVES_BOX(hbox), mt->btoolbar2, FALSE, FALSE, 0);

  // play buttons

  lives_toolbar_set_show_arrow(LIVES_TOOLBAR(mt->btoolbar2), FALSE);

  lives_toolbar_set_style(LIVES_TOOLBAR(mt->btoolbar2), LIVES_TOOLBAR_ICONS);
  lives_toolbar_set_icon_size(LIVES_TOOLBAR(mt->btoolbar2), LIVES_ICON_SIZE_LARGE_TOOLBAR);

  lives_widget_object_ref(mainw->m_sepwinbutton);
  lives_widget_unparent(mainw->m_sepwinbutton);
  lives_toolbar_insert(LIVES_TOOLBAR(mt->btoolbar2), LIVES_TOOL_ITEM(mainw->m_sepwinbutton), -1);
  lives_widget_object_unref(mainw->m_sepwinbutton);

  lives_widget_object_ref(mainw->m_rewindbutton);
  lives_widget_unparent(mainw->m_rewindbutton);
  lives_toolbar_insert(LIVES_TOOLBAR(mt->btoolbar2), LIVES_TOOL_ITEM(mainw->m_rewindbutton), -1);
  lives_widget_object_unref(mainw->m_rewindbutton);
  lives_widget_set_sensitive(mainw->m_rewindbutton, (mt->event_list != NULL && get_first_event(mt->event_list) != NULL));

  lives_widget_object_ref(mainw->m_playbutton);
  lives_widget_unparent(mainw->m_playbutton);
  lives_toolbar_insert(LIVES_TOOLBAR(mt->btoolbar2), LIVES_TOOL_ITEM(mainw->m_playbutton), -1);
  lives_widget_object_unref(mainw->m_playbutton);
  lives_widget_set_sensitive(mainw->m_playbutton, (mt->event_list != NULL && get_first_event(mt->event_list) != NULL));

  lives_widget_object_ref(mainw->m_stopbutton);
  lives_widget_unparent(mainw->m_stopbutton);
  lives_toolbar_insert(LIVES_TOOLBAR(mt->btoolbar2), LIVES_TOOL_ITEM(mainw->m_stopbutton), -1);
  lives_widget_object_unref(mainw->m_stopbutton);
  lives_widget_set_sensitive(mainw->m_stopbutton, FALSE);

  lives_widget_object_ref(mainw->m_loopbutton);
  lives_widget_unparent(mainw->m_loopbutton);
  lives_toolbar_insert(LIVES_TOOLBAR(mt->btoolbar2), LIVES_TOOL_ITEM(mainw->m_loopbutton), -1);
  lives_widget_object_unref(mainw->m_loopbutton);

  widget_opts.expand = LIVES_EXPAND_EXTRA_HEIGHT | LIVES_EXPAND_DEFAULT_WIDTH;
  widget_opts.justify = LIVES_JUSTIFY_CENTER;
  widget_opts.apply_theme = FALSE;
  widget_opts.font_size = LIVES_FONT_SIZE_LARGE;
  mt->timecode = lives_standard_entry_new(NULL, NULL, TIMECODE_LENGTH, TIMECODE_LENGTH, LIVES_BOX(hbox), NULL);
  widget_opts.font_size = LIVES_FONT_SIZE_MEDIUM;
  lives_widget_set_valign(mt->timecode, LIVES_ALIGN_CENTER);
  widget_opts.apply_theme = woat;
  widget_opts.expand = LIVES_EXPAND_DEFAULT;
  widget_opts.justify = LIVES_JUSTIFY_DEFAULT;

  update_timecodes(mt, 0.);

  lives_widget_add_events(mt->timecode, LIVES_FOCUS_CHANGE_MASK);
  lives_widget_set_sensitive(mt->timecode, FALSE);

  mt->tc_func = lives_signal_connect_after(LIVES_WIDGET_OBJECT(mt->timecode), LIVES_WIDGET_FOCUS_OUT_EVENT,
                LIVES_GUI_CALLBACK(after_timecode_changed), (livespointer) mt);

  add_fill_to_box(LIVES_BOX(hbox));

  widget_opts.expand = LIVES_EXPAND_DEFAULT_HEIGHT;

  mt->insa_checkbutton = lives_glowing_check_button_new((tmp = lives_strdup(_("  Insert with _Audio  "))), LIVES_BOX(hbox),
                         (tmp2 = lives_strdup(_("Select whether video clips are inserted and moved with their audio or not"))),
                         &mt->opts.insert_audio);
  lives_free(tmp);
  lives_free(tmp2);

  widget_opts.expand = LIVES_EXPAND_DEFAULT;

  mt->insa_label = widget_opts.last_label;

  add_fill_to_box(LIVES_BOX(hbox));

  widget_opts.expand = LIVES_EXPAND_DEFAULT_HEIGHT;

  mt->snapo_checkbutton = lives_glowing_check_button_new((tmp = lives_strdup(_("  Select _Overlap  "))), LIVES_BOX(hbox),
                          (tmp2 = lives_strdup(_("Select whether timeline selection snaps to overlap between selected tracks or not"))),
                          &mt->opts.snap_over);
  lives_free(tmp);
  lives_free(tmp2);

  widget_opts.expand = LIVES_EXPAND_DEFAULT;

  mt->overlap_label = widget_opts.last_label;

  // TODO - add a vbox with two hboxes
  // in each hbox we have 16 images
  // light for audio - in animate_multitrack
  // divide by out volume - then we have a volume gauge

  // add toolbar

  /*  volind=LIVES_WIDGET(gtk_tool_item_new());
    mainw->volind_hbox=lives_hbox_new(TRUE,0);
    lives_container_add(LIVES_CONTAINER(volind),mainw->volind_hbox);
    lives_toolbar_insert(LIVES_TOOLBAR(mainw->btoolbar),LIVES_TOOL_ITEM(mainw->vol_label),7);
  */

  mt->btoolbarx = lives_toolbar_new();
  lives_box_pack_start(LIVES_BOX(hbox), mt->btoolbarx, FALSE, FALSE, widget_opts.packing_width * 2);

  lives_toolbar_set_show_arrow(LIVES_TOOLBAR(mt->btoolbarx), FALSE);

  lives_toolbar_set_style(LIVES_TOOLBAR(mt->btoolbarx), LIVES_TOOLBAR_TEXT);

  mt->btoolbar3 = lives_toolbar_new();
  lives_box_pack_start(LIVES_BOX(hbox), mt->btoolbar3, FALSE, FALSE, 0);

  lives_toolbar_set_show_arrow(LIVES_TOOLBAR(mt->btoolbar3), FALSE);

  lives_toolbar_set_style(LIVES_TOOLBAR(mt->btoolbar3), LIVES_TOOLBAR_TEXT);

  //lives_separator_tool_item_new();
  //lives_toolbar_insert(LIVES_TOOLBAR(mt->btoolbar3), mt->sep1, -1);

  lives_toolbar_insert_space(LIVES_TOOLBAR(mt->btoolbar3));

  mt->grav_menuitem = LIVES_WIDGET(lives_standard_menu_tool_button_new(NULL, NULL));
  lives_tool_button_set_use_underline(LIVES_TOOL_BUTTON(mt->grav_menuitem), TRUE);

  mt->grav_normal = lives_standard_check_menu_item_new_with_label(_("Gravity: _Normal"),
                    mt->opts.grav_mode == GRAV_MODE_NORMAL);
  mtext = lives_menu_item_get_text(mt->grav_normal);

  mt->grav_label = lives_label_new(mtext);
  lives_tool_button_set_label_widget(LIVES_TOOL_BUTTON(mt->grav_menuitem), mt->grav_label);

  lives_toolbar_insert(LIVES_TOOLBAR(mt->btoolbar3), LIVES_TOOL_ITEM(mt->grav_menuitem), -1);

  mt->grav_submenu = lives_menu_new();

  lives_menu_tool_button_set_menu(LIVES_MENU_TOOL_BUTTON(mt->grav_menuitem), mt->grav_submenu);

  lives_container_add(LIVES_CONTAINER(mt->grav_submenu), mt->grav_normal);

  mt->grav_normal_func = lives_signal_connect(LIVES_GUI_OBJECT(mt->grav_normal), LIVES_WIDGET_TOGGLED_SIGNAL,
                         LIVES_GUI_CALLBACK(on_grav_mode_changed),
                         (livespointer)mt);

  mt->grav_left = lives_standard_check_menu_item_new_with_label(_("Gravity: _Left"), mt->opts.grav_mode == GRAV_MODE_LEFT);
  lives_container_add(LIVES_CONTAINER(mt->grav_submenu), mt->grav_left);

  mt->grav_left_func = lives_signal_connect(LIVES_GUI_OBJECT(mt->grav_left), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(on_grav_mode_changed),
                       (livespointer)mt);

  mt->grav_right = lives_standard_check_menu_item_new_with_label(_("Gravity: _Right"), mt->opts.grav_mode == GRAV_MODE_RIGHT);
  lives_container_add(LIVES_CONTAINER(mt->grav_submenu), mt->grav_right);

  mt->grav_right_func = lives_signal_connect(LIVES_GUI_OBJECT(mt->grav_right), LIVES_WIDGET_TOGGLED_SIGNAL,
                        LIVES_GUI_CALLBACK(on_grav_mode_changed),
                        (livespointer)mt);

  lives_widget_show_all(mt->grav_submenu); // needed

  if (mainw->mgeom[prefs->gui_monitor > 0 ? prefs->gui_monitor - 1 : 0].width > MENUBAR_MIN) in_menubar = FALSE;

  if (in_menubar) {
    menuitemsep = lives_standard_menu_item_new_with_label("|");
    lives_widget_set_sensitive(menuitemsep, FALSE);
    lives_container_add(LIVES_CONTAINER(mt->menubar), menuitemsep);
  } else {
    lives_toolbar_insert_space(LIVES_TOOLBAR(mt->btoolbar3));
  }

  mt->mm_submenu = lives_menu_new();
  mt->mm_move = lives_standard_check_menu_item_new_with_label(_("Mouse Mode: _Move"), mt->opts.mouse_mode == MOUSE_MODE_MOVE);
  mt->mm_label = NULL;

  if (in_menubar) {
    mt->mm_menuitem = lives_standard_menu_item_new_with_label("");
    lives_menu_item_set_submenu(LIVES_MENU_ITEM(mt->mm_menuitem), mt->mm_submenu);
    lives_container_add(LIVES_CONTAINER(mt->menubar), mt->mm_menuitem);
  } else {
    mt->mm_menuitem = LIVES_WIDGET(lives_standard_menu_tool_button_new(NULL, NULL));
    lives_tool_button_set_use_underline(LIVES_TOOL_BUTTON(mt->mm_menuitem), TRUE);

    mtext = lives_menu_item_get_text(mt->mm_move);
    mt->mm_label = lives_label_new(mtext);
    lives_tool_button_set_label_widget(LIVES_TOOL_BUTTON(mt->mm_menuitem), mt->mm_label);
    lives_toolbar_insert(LIVES_TOOLBAR(mt->btoolbar3), LIVES_TOOL_ITEM(mt->mm_menuitem), -1);
    lives_menu_tool_button_set_menu(LIVES_MENU_TOOL_BUTTON(mt->mm_menuitem), mt->mm_submenu);
  }

  lives_container_add(LIVES_CONTAINER(mt->mm_submenu), mt->mm_move);

  mt->mm_move_func = lives_signal_connect(LIVES_GUI_OBJECT(mt->mm_move), LIVES_WIDGET_TOGGLED_SIGNAL,
                                          LIVES_GUI_CALLBACK(on_mouse_mode_changed),
                                          (livespointer)mt);

  mt->mm_select = lives_standard_check_menu_item_new_with_label(_("Mouse Mode: _Select"),
                  mt->opts.mouse_mode == MOUSE_MODE_SELECT);
  lives_container_add(LIVES_CONTAINER(mt->mm_submenu), mt->mm_select);

  mt->mm_select_func = lives_signal_connect(LIVES_GUI_OBJECT(mt->mm_select), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(on_mouse_mode_changed),
                       (livespointer)mt);

  lives_widget_show_all(mt->mm_submenu); // needed

  if (in_menubar) {
    menuitemsep = lives_standard_menu_item_new_with_label("|");
    lives_widget_set_sensitive(menuitemsep, FALSE);
    lives_container_add(LIVES_CONTAINER(mt->menubar), menuitemsep);
  } else {
    lives_toolbar_insert_space(LIVES_TOOLBAR(mt->btoolbar3));
  }

  mt->ins_submenu = lives_menu_new();
  mt->ins_normal = lives_standard_check_menu_item_new_with_label(_("Insert Mode: _Normal"),
                   mt->opts.insert_mode == INSERT_MODE_NORMAL);

  if (in_menubar) {
    mt->ins_menuitem = lives_standard_menu_item_new_with_label("");
    mt->ins_label = NULL;
    lives_menu_item_set_submenu(LIVES_MENU_ITEM(mt->ins_menuitem), mt->ins_submenu);
    lives_container_add(LIVES_CONTAINER(mt->menubar), mt->ins_menuitem);
  } else {
    mt->ins_menuitem = LIVES_WIDGET(lives_standard_menu_tool_button_new(NULL, NULL));
    lives_tool_button_set_use_underline(LIVES_TOOL_BUTTON(mt->ins_menuitem), TRUE);
    mtext = lives_menu_item_get_text(mt->ins_normal);
    mt->ins_label = lives_label_new(mtext);
    lives_tool_button_set_label_widget(LIVES_TOOL_BUTTON(mt->ins_menuitem), mt->ins_label);
    lives_toolbar_insert(LIVES_TOOLBAR(mt->btoolbar3), LIVES_TOOL_ITEM(mt->ins_menuitem), -1);
    lives_menu_tool_button_set_menu(LIVES_MENU_TOOL_BUTTON(mt->ins_menuitem), mt->ins_submenu);
  }

  lives_container_add(LIVES_CONTAINER(mt->ins_submenu), mt->ins_normal);

  mt->ins_normal_func = lives_signal_connect(LIVES_GUI_OBJECT(mt->ins_normal), LIVES_WIDGET_TOGGLED_SIGNAL,
                        LIVES_GUI_CALLBACK(on_insert_mode_changed),
                        (livespointer)mt);

  lives_widget_show_all(mt->ins_submenu); // needed

  if (!in_menubar) {
    mt->sep4 = lives_toolbar_insert_space(LIVES_TOOLBAR(mt->btoolbar3));
  } else mt->sep4 = NULL;

  mt->btoolbary = lives_toolbar_new();
  lives_box_pack_start(LIVES_BOX(hbox), mt->btoolbary, TRUE, TRUE, 0);

  lives_toolbar_set_show_arrow(LIVES_TOOLBAR(mt->btoolbary), FALSE);

  lives_toolbar_set_style(LIVES_TOOLBAR(mt->btoolbary), LIVES_TOOLBAR_ICONS);
  lives_toolbar_set_icon_size(LIVES_TOOLBAR(mt->btoolbary), LIVES_ICON_SIZE_SMALL_TOOLBAR);

  lives_widget_object_ref(mainw->m_mutebutton);
  lives_widget_unparent(mainw->m_mutebutton);
  lives_toolbar_insert(LIVES_TOOLBAR(mt->btoolbary), LIVES_TOOL_ITEM(mainw->m_mutebutton), -1);
  lives_widget_object_unref(mainw->m_mutebutton);

  if (!lives_scale_button_set_orientation(LIVES_SCALE_BUTTON(mainw->volume_scale), LIVES_ORIENTATION_HORIZONTAL)) {
    if (mainw->vol_label != NULL) {
      lives_widget_object_ref(mainw->vol_label);
      lives_widget_unparent(mainw->vol_label);
      lives_toolbar_insert(LIVES_TOOLBAR(mt->btoolbary), LIVES_TOOL_ITEM(mainw->vol_label), -1);
      lives_widget_object_unref(mainw->vol_label);
    }
  }

  lives_widget_object_ref(mainw->vol_toolitem);
  lives_widget_unparent(mainw->vol_toolitem);
  lives_toolbar_insert(LIVES_TOOLBAR(mt->btoolbary), LIVES_TOOL_ITEM(mainw->vol_toolitem), -1);
  lives_widget_object_unref(mainw->vol_toolitem);

  lives_widget_apply_theme2(mainw->vol_toolitem, LIVES_WIDGET_STATE_NORMAL, FALSE);
  if (mainw->vol_label != NULL) lives_widget_apply_theme2(mainw->vol_label, LIVES_WIDGET_STATE_NORMAL, FALSE);
  lives_widget_apply_theme2(mainw->volume_scale, LIVES_WIDGET_STATE_NORMAL, FALSE);

  hseparator = lives_hseparator_new();
  lives_box_pack_start(LIVES_BOX(mt->xtravbox), hseparator, FALSE, FALSE, 0);

  mt->hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(mt->xtravbox), mt->hbox, TRUE, TRUE, 0);

  mt->play_blank = lives_image_new_from_pixbuf(mainw->imframe);
  mt->preview_frame = lives_standard_frame_new(_("Preview"), 0.5, FALSE);
  //lives_container_set_border_width(LIVES_CONTAINER(mt->preview_frame), widget_opts.border_width);
  lives_box_pack_start(LIVES_BOX(mt->hbox), mt->preview_frame, FALSE, FALSE, 0);
  mt->fd_frame = mt->preview_frame;

  mt->preview_eventbox = lives_event_box_new();
  lives_signal_connect_after(LIVES_GUI_OBJECT(mt->preview_eventbox), LIVES_WIDGET_CONFIGURE_EVENT,
                             LIVES_GUI_CALLBACK(config_event),
                             NULL);
  lives_widget_queue_resize(mt->preview_eventbox);

  lives_widget_set_size_request(mt->preview_eventbox, scr_width / 3.5, scr_height / 3.);
  mt->play_box = lives_vbox_new(FALSE, 0);
  lives_widget_set_app_paintable(mt->preview_eventbox, TRUE);

  lives_widget_set_hexpand(mt->preview_frame, FALSE);
  lives_widget_set_vexpand(mt->preview_frame, FALSE);

  lives_widget_set_hexpand(mt->preview_eventbox, TRUE);
  lives_widget_set_vexpand(mt->preview_eventbox, TRUE);

  // does nothing
  //lives_widget_set_valign(mt->preview_eventbox, LIVES_ALIGN_CENTER);
  //lives_widget_set_halign(mt->preview_eventbox, LIVES_ALIGN_CENTER);

  // must do this here to set cfile->hsize, cfile->vsize; and we must have created aparam_submenu and insa_eventbox and insa_checkbutton
  msg = set_values_from_defs(mt, !prefs->mt_enter_prompt || (mainw->recoverable_layout &&
                             prefs->startup_interface == STARTUP_CE));
  lives_freep((void **)&msg);

#if GTK_CHECK_VERSION(3, 0, 0)
  lives_signal_connect(LIVES_GUI_OBJECT(mt->play_box), LIVES_WIDGET_EXPOSE_EVENT,
                       LIVES_GUI_CALLBACK(expose_pb),
                       (livespointer)mt);
#endif

  lives_container_add(LIVES_CONTAINER(mt->preview_frame), mt->preview_eventbox);
  lives_container_add(LIVES_CONTAINER(mt->preview_eventbox), mt->play_box);

  lives_container_add(LIVES_CONTAINER(mt->play_box), mt->play_blank);

  lives_widget_add_events(mt->preview_eventbox, LIVES_BUTTON1_MOTION_MASK | LIVES_BUTTON_RELEASE_MASK | LIVES_BUTTON_PRESS_MASK |
                          LIVES_ENTER_NOTIFY_MASK);

  lives_signal_connect(LIVES_GUI_OBJECT(mt->preview_eventbox), LIVES_WIDGET_MOTION_NOTIFY_EVENT,
                       LIVES_GUI_CALLBACK(on_framedraw_mouse_update),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mt->preview_eventbox), LIVES_WIDGET_BUTTON_RELEASE_EVENT,
                       LIVES_GUI_CALLBACK(on_framedraw_mouse_reset),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mt->preview_eventbox), LIVES_WIDGET_BUTTON_PRESS_EVENT,
                       LIVES_GUI_CALLBACK(on_framedraw_mouse_start),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mt->preview_eventbox), LIVES_WIDGET_ENTER_EVENT, LIVES_GUI_CALLBACK(on_framedraw_enter),
                       NULL);

  mt->hpaned = lives_hpaned_new();
  lives_box_pack_start(LIVES_BOX(mt->hbox), mt->hpaned, TRUE, TRUE, 0);

  mt->nb = lives_standard_notebook_new(&palette->normal_back, &palette->menu_and_bars);
  lives_container_set_border_width(LIVES_CONTAINER(mt->nb), widget_opts.border_width);

  hbox = lives_hbox_new(FALSE, 0);

  // add a page
  lives_container_add(LIVES_CONTAINER(mt->nb), hbox);

  tname = get_tab_name(POLY_CLIPS);
  mt->nb_label1 = lives_standard_label_new(tname);
  lives_free(tname);
  lives_widget_set_hexpand(mt->nb_label1, TRUE);
  lives_widget_set_halign(mt->nb_label1, LIVES_ALIGN_CENTER);
  lives_widget_apply_theme(mt->nb_label1, LIVES_WIDGET_STATE_NORMAL);

  // prepare polymorph box
  mt->poly_box = lives_vbox_new(FALSE, 0);

  lives_widget_set_vexpand(mt->poly_box, FALSE);
  lives_widget_set_hexpand(mt->poly_box, TRUE);

  lives_container_add(LIVES_CONTAINER(hbox), mt->poly_box);

  lives_notebook_set_tab_label(LIVES_NOTEBOOK(mt->nb), lives_notebook_get_nth_page(LIVES_NOTEBOOK(mt->nb), 0), mt->nb_label1);

  // poly box is first page in notebook

  // notebook goes in paned: so we have paned -> nb-> poly_box
  lives_paned_pack(1, LIVES_PANED(mt->hpaned), mt->nb, TRUE, FALSE);

  // poly clip scroll
  mt->clip_scroll = lives_scrolled_window_new(NULL, NULL);
  lives_widget_object_ref(mt->clip_scroll);
  lives_widget_set_events(mt->clip_scroll, LIVES_SCROLL_MASK | LIVES_SMOOTH_SCROLL_MASK);
  lives_signal_connect(LIVES_GUI_OBJECT(mt->clip_scroll), LIVES_WIDGET_SCROLL_EVENT,
                       LIVES_GUI_CALLBACK(on_mouse_scroll),
                       mt);

  lives_scrolled_window_set_policy(LIVES_SCROLLED_WINDOW(mt->clip_scroll), LIVES_POLICY_AUTOMATIC, LIVES_POLICY_NEVER);
  lives_widget_set_hexpand(mt->clip_scroll, TRUE);

  mt->clip_inner_box = lives_hbox_new(FALSE, widget_opts.packing_width);

  lives_scrolled_window_add_with_viewport(LIVES_SCROLLED_WINDOW(mt->clip_scroll), mt->clip_inner_box);

  // add a dummy hbox to nb (adds a tab with a label)

  tname = get_tab_name(POLY_IN_OUT);
  mt->nb_label2 = lives_label_new(tname);
  lives_free(tname);
  lives_widget_set_hexpand(mt->nb_label2, TRUE);
  lives_widget_set_halign(mt->nb_label2, LIVES_ALIGN_CENTER);
  lives_widget_apply_theme(mt->nb_label2, LIVES_WIDGET_STATE_NORMAL);

  hbox = lives_hbox_new(FALSE, 0);

  lives_container_add(LIVES_CONTAINER(mt->nb), hbox);
  lives_notebook_set_tab_label(LIVES_NOTEBOOK(mt->nb), lives_notebook_get_nth_page(LIVES_NOTEBOOK(mt->nb), 1), mt->nb_label2);

  // add a dummy hbox to nb (adds a tab with label)

  tname = get_tab_name(POLY_FX_STACK);
  mt->nb_label3 = lives_label_new(tname);
  lives_free(tname);
  lives_widget_set_hexpand(mt->nb_label3, TRUE);
  lives_widget_set_halign(mt->nb_label3, LIVES_ALIGN_CENTER);
  lives_widget_apply_theme(mt->nb_label3, LIVES_WIDGET_STATE_NORMAL);

  hbox = lives_hbox_new(FALSE, 0);

  lives_container_add(LIVES_CONTAINER(mt->nb), hbox);
  lives_notebook_set_tab_label(LIVES_NOTEBOOK(mt->nb), lives_notebook_get_nth_page(LIVES_NOTEBOOK(mt->nb), 2), mt->nb_label3);

  // add a dummy hbox to nb

  tname = get_tab_name(POLY_PARAMS);
  mt->nb_label7 = lives_label_new(tname);
  lives_free(tname);

  hbox = lives_hbox_new(FALSE, 0);

  lives_container_add(LIVES_CONTAINER(mt->nb), hbox);
  lives_notebook_set_tab_label(LIVES_NOTEBOOK(mt->nb), lives_notebook_get_nth_page(LIVES_NOTEBOOK(mt->nb), 3), mt->nb_label7);

  // params contents

  mt->fx_base_box = lives_vbox_new(FALSE, 0);
  lives_widget_object_ref(mt->fx_base_box);

  lives_widget_set_bg_color(mt->fx_base_box, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
  lives_widget_set_fg_color(mt->fx_base_box, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);

  mt->fx_contents_box = lives_vbox_new(FALSE, 2);

  lives_widget_set_bg_color(mt->fx_contents_box, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
  lives_widget_set_fg_color(mt->fx_contents_box, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);

  dph = widget_opts.packing_height;
  widget_opts.packing_height >>= 1;
  add_hsep_to_box(LIVES_BOX(mt->fx_contents_box));
  widget_opts.packing_height = dph;

  lives_box_pack_end(LIVES_BOX(mt->fx_base_box), mt->fx_contents_box, FALSE, FALSE, 0);

  hbox = lives_hbox_new(FALSE, widget_opts.packing_width);
  lives_box_pack_end(LIVES_BOX(mt->fx_contents_box), hbox, FALSE, FALSE, 0);

  lives_widget_set_bg_color(hbox, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
  lives_widget_set_fg_color(hbox, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);

  widget_opts.expand = LIVES_EXPAND_DEFAULT_WIDTH;
  mt->apply_fx_button = lives_standard_button_new_with_label(_("_Apply"));
  widget_opts.expand = LIVES_EXPAND_DEFAULT;
  lives_box_pack_start(LIVES_BOX(hbox), mt->apply_fx_button, FALSE, FALSE, 0);

  lives_signal_connect(LIVES_GUI_OBJECT(mt->apply_fx_button), LIVES_WIDGET_CLICKED_SIGNAL,
                       LIVES_GUI_CALLBACK(on_set_pvals_clicked),
                       (livespointer)mt);

  mt->node_adj = (LiVESWidgetObject *)lives_adjustment_new(0., 0., 0., 1. / mt->fps, 10. / mt->fps, 0.);

  mt->node_scale = lives_standard_hscale_new(LIVES_ADJUSTMENT(mt->node_adj));

  mt->node_spinbutton = lives_standard_spin_button_new(NULL, 0., 0., 0., 1. / mt->fps, 1. / mt->fps,
                        3, LIVES_BOX(hbox), NULL);

  lives_spin_button_set_adjustment(LIVES_SPIN_BUTTON(mt->node_spinbutton), LIVES_ADJUSTMENT(mt->node_adj));

  lives_signal_connect_after(LIVES_GUI_OBJECT(mt->node_spinbutton), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                             LIVES_GUI_CALLBACK(on_node_spin_value_changed),
                             (livespointer)mt);

  mt->time_label = lives_standard_label_new(_("Time"));
  lives_box_pack_start(LIVES_BOX(hbox), mt->time_label, FALSE, TRUE, 0);

  lives_box_pack_start(LIVES_BOX(hbox), mt->node_scale, TRUE, TRUE, widget_opts.packing_width);

  hbox = lives_hbox_new(FALSE, widget_opts.packing_width);
  lives_box_pack_end(LIVES_BOX(mt->fx_contents_box), hbox, FALSE, FALSE, 0);

  mt->solo_check = lives_standard_check_button_new(_("Preview _Solo"), TRUE, LIVES_BOX(hbox),
                   (tmp = lives_strdup(_("Preview only the selected effect"))));
  lives_free(tmp);

  lives_signal_connect_after(LIVES_GUI_OBJECT(mt->solo_check), LIVES_WIDGET_TOGGLED_SIGNAL,
                             LIVES_GUI_CALLBACK(on_solo_check_toggled),
                             (livespointer)mt);

  widget_opts.expand = LIVES_EXPAND_DEFAULT_WIDTH;
  mt->resetp_button = lives_standard_button_new_with_label(_("_Reset  To Defaults"));
  widget_opts.expand = LIVES_EXPAND_DEFAULT;
  lives_box_pack_start(LIVES_BOX(hbox), mt->resetp_button, FALSE, FALSE, 0);

  lives_signal_connect(LIVES_GUI_OBJECT(mt->resetp_button), LIVES_WIDGET_CLICKED_SIGNAL,
                       LIVES_GUI_CALLBACK(on_resetp_clicked),
                       (livespointer)mt);

  widget_opts.expand = LIVES_EXPAND_DEFAULT_WIDTH;
  mt->del_node_button = lives_standard_button_new_with_label(_("_Del. node"));
  widget_opts.expand = LIVES_EXPAND_DEFAULT;
  lives_box_pack_end(LIVES_BOX(hbox), mt->del_node_button, FALSE, FALSE, 0);
  lives_widget_set_sensitive(mt->del_node_button, FALSE);

  lives_signal_connect(LIVES_GUI_OBJECT(mt->del_node_button), LIVES_WIDGET_CLICKED_SIGNAL,
                       LIVES_GUI_CALLBACK(on_del_node_clicked),
                       (livespointer)mt);

  widget_opts.expand = LIVES_EXPAND_DEFAULT_WIDTH;
  mt->next_node_button = lives_standard_button_new_with_label(_("_Next node"));
  widget_opts.expand = LIVES_EXPAND_DEFAULT;
  lives_box_pack_end(LIVES_BOX(hbox), mt->next_node_button, FALSE, FALSE, 0);
  lives_widget_set_sensitive(mt->next_node_button, FALSE);

  lives_signal_connect(LIVES_GUI_OBJECT(mt->next_node_button), LIVES_WIDGET_CLICKED_SIGNAL,
                       LIVES_GUI_CALLBACK(on_next_node_clicked),
                       (livespointer)mt);

  widget_opts.expand = LIVES_EXPAND_DEFAULT_WIDTH;
  mt->prev_node_button = lives_standard_button_new_with_label(_("_Prev node"));
  widget_opts.expand = LIVES_EXPAND_DEFAULT;
  lives_box_pack_end(LIVES_BOX(hbox), mt->prev_node_button, FALSE, FALSE, 0);
  lives_widget_set_sensitive(mt->prev_node_button, FALSE);

  lives_signal_connect(LIVES_GUI_OBJECT(mt->prev_node_button), LIVES_WIDGET_CLICKED_SIGNAL,
                       LIVES_GUI_CALLBACK(on_prev_node_clicked),
                       (livespointer)mt);

  widget_opts.justify = LIVES_JUSTIFY_CENTER;
  mt->fx_label = lives_standard_label_new("");
  lives_box_pack_end(LIVES_BOX(hbox), mt->fx_label, FALSE, FALSE, widget_opts.packing_width * 2);
  widget_opts.justify = LIVES_JUSTIFY_DEFAULT;

  // add a dummy hbox to nb

  tname = get_tab_name(POLY_EFFECTS);
  mt->nb_label4 = lives_label_new(tname);
  lives_free(tname);
  lives_widget_set_hexpand(mt->nb_label4, TRUE);
  lives_widget_set_halign(mt->nb_label4, LIVES_ALIGN_CENTER);
  lives_widget_apply_theme(mt->nb_label4, LIVES_WIDGET_STATE_NORMAL);

  hbox = lives_hbox_new(FALSE, 0);

  lives_container_add(LIVES_CONTAINER(mt->nb), hbox);
  lives_notebook_set_tab_label(LIVES_NOTEBOOK(mt->nb), lives_notebook_get_nth_page(LIVES_NOTEBOOK(mt->nb), 4), mt->nb_label4);

  // add a dummy hbox to nb

  tname = get_tab_name(POLY_TRANS);
  mt->nb_label5 = lives_label_new(tname);
  lives_free(tname);
  lives_widget_set_hexpand(mt->nb_label5, TRUE);
  lives_widget_set_halign(mt->nb_label5, LIVES_ALIGN_CENTER);
  lives_widget_apply_theme(mt->nb_label5, LIVES_WIDGET_STATE_NORMAL);

  hbox = lives_hbox_new(FALSE, 0);

  lives_container_add(LIVES_CONTAINER(mt->nb), hbox);
  lives_notebook_set_tab_label(LIVES_NOTEBOOK(mt->nb), lives_notebook_get_nth_page(LIVES_NOTEBOOK(mt->nb), 5), mt->nb_label5);

  // add a dummy hbox to nb

  tname = get_tab_name(POLY_COMP);
  mt->nb_label6 = lives_label_new(tname);
  lives_free(tname);
  lives_widget_set_hexpand(mt->nb_label6, TRUE);
  lives_widget_set_halign(mt->nb_label6, LIVES_ALIGN_CENTER);
  lives_widget_apply_theme(mt->nb_label6, LIVES_WIDGET_STATE_NORMAL);

  hbox = lives_hbox_new(FALSE, 0);

  lives_container_add(LIVES_CONTAINER(mt->nb), hbox);
  lives_notebook_set_tab_label(LIVES_NOTEBOOK(mt->nb), lives_notebook_get_nth_page(LIVES_NOTEBOOK(mt->nb), 6), mt->nb_label6);

  set_mt_title(mt);

  mt_init_clips(mt, orig_file, FALSE);

  // poly audio velocity
  mt->avel_box = lives_vbox_new(FALSE, 0);
  lives_widget_object_ref(mt->avel_box);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(mt->avel_box), hbox, FALSE, FALSE, widget_opts.packing_height >> 1);

  mt->checkbutton_avel_reverse = lives_standard_check_button_new(_("_Reverse playback  "), FALSE, LIVES_BOX(hbox), NULL);

  if (palette->style & STYLE_1) {
    lives_widget_set_bg_color(hbox, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
    lives_widget_set_fg_color(hbox, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
  }

  mt->check_avel_rev_func = lives_signal_connect_after(LIVES_GUI_OBJECT(mt->checkbutton_avel_reverse),
                            LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(avel_reverse_toggled),
                            mt);

  hbox = lives_hbox_new(FALSE, 8);
  lives_box_pack_start(LIVES_BOX(mt->avel_box), hbox, FALSE, FALSE, widget_opts.packing_height);

  mt->spinbutton_avel = lives_standard_spin_button_new(_("_Velocity  "), 1., 0.5, 2., .1, 1., 2,
                        LIVES_BOX(hbox), NULL);

  mt->spin_avel_func = lives_signal_connect_after(LIVES_GUI_OBJECT(mt->spinbutton_avel), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(avel_spin_changed),
                       mt);

  spinbutton_adj = lives_spin_button_get_adjustment(LIVES_SPIN_BUTTON(mt->spinbutton_avel));

  mt->avel_scale = lives_standard_hscale_new(LIVES_ADJUSTMENT(spinbutton_adj));
  lives_box_pack_start(LIVES_BOX(hbox), mt->avel_scale, TRUE, TRUE, widget_opts.packing_width);

  // poly in_out_box
  mt->in_out_box = lives_hbox_new(TRUE, 0);
  lives_container_set_border_width(LIVES_CONTAINER(mt->in_out_box), 2);
  lives_widget_object_ref(mt->in_out_box);
  lives_widget_apply_theme(mt->in_out_box, LIVES_WIDGET_STATE_NORMAL);

  vbox = lives_vbox_new(FALSE, widget_opts.packing_height);
  lives_box_pack_start(LIVES_BOX(mt->in_out_box), vbox, TRUE, TRUE, widget_opts.packing_width);

  mt->in_image = lives_image_new();
  mt->in_frame = lives_frame_new(NULL);
  lives_container_set_border_width(LIVES_CONTAINER(mt->in_frame), 0);

  lives_container_add(LIVES_CONTAINER(mt->in_frame), mt->in_image);
  lives_box_pack_start(LIVES_BOX(vbox), mt->in_frame, FALSE, TRUE, 0);

  lives_signal_connect(LIVES_GUI_OBJECT(mt->in_frame), LIVES_WIDGET_BUTTON_PRESS_EVENT,
                       LIVES_GUI_CALLBACK(in_out_ebox_pressed),
                       (livespointer)mt);
  lives_signal_connect(LIVES_GUI_OBJECT(mt->in_frame), LIVES_WIDGET_BUTTON_RELEASE_EVENT,
                       LIVES_GUI_CALLBACK(on_drag_clip_end),
                       (livespointer)mt);

  if (palette->style & STYLE_1) {
    lives_widget_set_fg_color(mt->in_frame, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
    lives_widget_set_bg_color(mt->in_frame, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
  }

  mt->in_hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), mt->in_hbox, FALSE, FALSE, 0);

  add_spring_to_box(LIVES_BOX(mt->in_hbox), 0);

  mt->spinbutton_in = lives_standard_spin_button_new(NULL, 0., 0., 1000000., 1. / mt->fps, 1., 2,
                      LIVES_BOX(mt->in_hbox), NULL);
  lives_spin_button_set_snap_to_ticks(LIVES_SPIN_BUTTON(mt->spinbutton_in), TRUE);

  mt->checkbutton_start_anchored = lives_standard_check_button_new((tmp = lives_strdup(_("Anchor _start"))), FALSE,
                                   LIVES_BOX(mt->in_hbox),
                                   (tmp2 = lives_strdup(_("Anchor the start point to the timeline"))));

  lives_free(tmp);
  lives_free(tmp2);

  add_spring_to_box(LIVES_BOX(mt->in_hbox), 0);

  mt->spin_in_func = lives_signal_connect_after(LIVES_GUI_OBJECT(mt->spinbutton_in), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                     LIVES_GUI_CALLBACK(in_out_start_changed),
                     mt);

  mt->check_start_func = lives_signal_connect_after(LIVES_GUI_OBJECT(mt->checkbutton_start_anchored),
                         LIVES_WIDGET_TOGGLED_SIGNAL,
                         LIVES_GUI_CALLBACK(in_anchor_toggled),
                         mt);

  vbox = lives_vbox_new(FALSE, widget_opts.packing_height);

  lives_box_pack_end(LIVES_BOX(mt->in_out_box), vbox, TRUE, TRUE, widget_opts.packing_width);

  mt->out_image = lives_image_new();
  mt->out_frame = lives_frame_new(NULL);
  lives_container_set_border_width(LIVES_CONTAINER(mt->out_frame), 0);

  lives_container_add(LIVES_CONTAINER(mt->out_frame), mt->out_image);
  lives_box_pack_start(LIVES_BOX(vbox), mt->out_frame, FALSE, TRUE, 0);

  lives_signal_connect(LIVES_GUI_OBJECT(mt->out_frame), LIVES_WIDGET_BUTTON_PRESS_EVENT,
                       LIVES_GUI_CALLBACK(in_out_ebox_pressed),
                       (livespointer)mt);
  lives_signal_connect(LIVES_GUI_OBJECT(mt->out_frame), LIVES_WIDGET_BUTTON_RELEASE_EVENT,
                       LIVES_GUI_CALLBACK(on_drag_clip_end),
                       (livespointer)mt);

  if (palette->style & STYLE_1) {
    lives_widget_set_fg_color(mt->out_frame, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
    lives_widget_set_bg_color(mt->out_frame, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
  }

  mt->out_hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), mt->out_hbox, FALSE, FALSE, 0);

  add_spring_to_box(LIVES_BOX(mt->out_hbox), 0);

  mt->spinbutton_out = lives_standard_spin_button_new(NULL, 0., 0., 1000000., 1. / mt->fps, 1., 2,
                       LIVES_BOX(mt->out_hbox), NULL);
  lives_spin_button_set_snap_to_ticks(LIVES_SPIN_BUTTON(mt->spinbutton_out), TRUE);

  mt->checkbutton_end_anchored = lives_standard_check_button_new((tmp = lives_strdup(_("Anchor _end"))), FALSE,
                                 LIVES_BOX(mt->out_hbox),
                                 (tmp2 = lives_strdup(_("Anchor the end point to the timeline"))));


  add_spring_to_box(LIVES_BOX(mt->out_hbox), 0);

  lives_free(tmp);
  lives_free(tmp2);

  mt->spin_out_func = lives_signal_connect_after(LIVES_GUI_OBJECT(mt->spinbutton_out), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                      LIVES_GUI_CALLBACK(in_out_end_changed),
                      mt);

  mt->check_end_func = lives_signal_connect_after(LIVES_GUI_OBJECT(mt->checkbutton_end_anchored),
                       LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(out_anchor_toggled),
                       mt);

  lives_signal_handler_block(mt->spinbutton_in, mt->spin_in_func);
  lives_signal_handler_block(mt->spinbutton_out, mt->spin_out_func);

  lives_signal_connect(LIVES_GUI_OBJECT(mt->nb), LIVES_WIDGET_SWITCH_PAGE_SIGNAL,
                       LIVES_GUI_CALLBACK(notebook_page),
                       (livespointer)mt);

  ///////////////////////////////////////////////

  mt->poly_state = POLY_NONE;
  polymorph(mt, POLY_CLIPS);

  mt->context_frame = lives_standard_frame_new(_("Info"), 0.5, FALSE);

  lives_paned_pack(2, LIVES_PANED(mt->hpaned), mt->context_frame, TRUE, FALSE);

  mt->context_scroll = NULL;

  clear_context(mt);

  ////

  mt->hseparator = lives_hseparator_new();

  if (mainw->imsep == NULL) {
    lives_box_pack_start(LIVES_BOX(mt->xtravbox), mt->hseparator, FALSE, FALSE, widget_opts.packing_height);
    mt->sep_image = NULL;
    mt->hseparator2 = NULL;
  } else {
    lives_box_pack_start(LIVES_BOX(mt->xtravbox), mt->hseparator, FALSE, FALSE, 0);
    mt->sep_image = lives_image_new_from_pixbuf(mainw->imsep);
    lives_box_pack_start(LIVES_BOX(mt->xtravbox), mt->sep_image, FALSE, FALSE, 0);
    mt->hseparator2 = lives_hseparator_new();
    lives_box_pack_start(LIVES_BOX(mt->xtravbox), mt->hseparator2, FALSE, FALSE, 0);
  }

  mt_init_start_end_spins(mt);

  mt->vpaned = lives_vpaned_new();
  lives_box_pack_start(LIVES_BOX(mt->top_vbox), mt->vpaned, TRUE, TRUE, 0);
  lives_widget_set_vexpand(mt->vpaned, FALSE);
  lives_widget_set_hexpand(mt->vpaned, FALSE);

  tl_vbox = lives_vbox_new(FALSE, 0);
  lives_container_set_border_width(LIVES_CONTAINER(tl_vbox), 0);

  lives_paned_pack(1, LIVES_PANED(mt->vpaned), tl_vbox, TRUE, FALSE);

  mt->timeline_table_header = lives_table_new(2, TIMELINE_TABLE_COLUMNS, TRUE);
  lives_table_set_row_spacings(LIVES_TABLE(mt->timeline_table_header), 0);

  mt->tlx_eventbox = lives_event_box_new();
  lives_box_pack_start(LIVES_BOX(tl_vbox), mt->tlx_eventbox, FALSE, FALSE, 0);

  lives_signal_connect(LIVES_GUI_OBJECT(mt->tlx_eventbox), LIVES_WIDGET_BUTTON_PRESS_EVENT,
                       LIVES_GUI_CALLBACK(on_track_header_click),
                       (livespointer)mt);

  lives_signal_connect(LIVES_GUI_OBJECT(mt->tlx_eventbox), LIVES_WIDGET_BUTTON_RELEASE_EVENT,
                       LIVES_GUI_CALLBACK(on_track_header_release),
                       (livespointer)mt);

  lives_widget_add_events(mt->tlx_eventbox, LIVES_BUTTON1_MOTION_MASK | LIVES_BUTTON_RELEASE_MASK | LIVES_BUTTON_PRESS_MASK);
  mt->mouse_mot1 = lives_signal_connect(LIVES_GUI_OBJECT(mt->tlx_eventbox), LIVES_WIDGET_MOTION_NOTIFY_EVENT,
                                        LIVES_GUI_CALLBACK(on_track_header_move),
                                        (livespointer)mt);
  lives_signal_handler_block(mt->tlx_eventbox, mt->mouse_mot1);

  hbox = lives_hbox_new(FALSE, 0);
  lives_container_add(LIVES_CONTAINER(mt->tlx_eventbox), hbox);

  vadjustment = (LiVESWidgetObject *)lives_adjustment_new(1.0, 1.0, 1.0, 1.0, 1.0, 1.0);
  scrollbar = lives_vscrollbar_new(LIVES_ADJUSTMENT(vadjustment));
  lives_widget_set_sensitive(scrollbar, FALSE);
  lives_box_pack_start(LIVES_BOX(hbox), mt->timeline_table_header, TRUE, TRUE, 0);
  lives_box_pack_end(LIVES_BOX(hbox), scrollbar, FALSE, FALSE, widget_opts.packing_width);
#if GTK_CHECK_VERSION(3, 8, 0)
  gtk_widget_set_opacity(scrollbar, 0.);
#endif

  mt->tl_hbox = lives_hbox_new(FALSE, 0);
  lives_container_set_border_width(LIVES_CONTAINER(mt->tl_hbox), 0);

  lives_box_pack_start(LIVES_BOX(tl_vbox), mt->tl_hbox, TRUE, TRUE, widget_opts.packing_height >> 1);

  mt->vadjustment = (LiVESWidgetObject *)lives_adjustment_new(0.0, 0.0, 1.0, 1.0, prefs->max_disp_vtracks, 1.0);
  mt->scrollbar = lives_vscrollbar_new(LIVES_ADJUSTMENT(mt->vadjustment));

  lives_signal_connect_after(LIVES_GUI_OBJECT(mt->scrollbar), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                             LIVES_GUI_CALLBACK(scroll_track_by_scrollbar),
                             (livespointer)mt);

  mt->tl_eventbox = lives_event_box_new();
  lives_box_pack_start(LIVES_BOX(mt->tl_hbox), mt->tl_eventbox, TRUE, TRUE, 0);

  lives_signal_connect(LIVES_GUI_OBJECT(mt->tl_eventbox), LIVES_WIDGET_BUTTON_PRESS_EVENT,
                       LIVES_GUI_CALLBACK(on_track_between_click),
                       (livespointer)mt);

  lives_signal_connect(LIVES_GUI_OBJECT(mt->tl_eventbox), LIVES_WIDGET_BUTTON_RELEASE_EVENT,
                       LIVES_GUI_CALLBACK(on_track_between_release),
                       (livespointer)mt);

  lives_widget_add_events(mt->tl_eventbox, LIVES_BUTTON1_MOTION_MASK | LIVES_BUTTON_RELEASE_MASK | LIVES_BUTTON_PRESS_MASK |
                          LIVES_SCROLL_MASK | LIVES_SMOOTH_SCROLL_MASK);
  mt->mouse_mot2 = lives_signal_connect(LIVES_GUI_OBJECT(mt->tl_eventbox), LIVES_WIDGET_MOTION_NOTIFY_EVENT,
                                        LIVES_GUI_CALLBACK(on_track_move),
                                        (livespointer)mt);

  lives_signal_handler_block(mt->tl_eventbox, mt->mouse_mot2);

  lives_signal_connect(LIVES_GUI_OBJECT(mt->tl_eventbox), LIVES_WIDGET_SCROLL_EVENT,
                       LIVES_GUI_CALLBACK(on_mt_timeline_scroll),
                       (livespointer)mt);

  lives_box_pack_end(LIVES_BOX(mt->tl_hbox), mt->scrollbar, FALSE, FALSE, widget_opts.packing_width);

  mt->eventbox = lives_event_box_new();
  hbox = lives_hbox_new(FALSE, 0);

  lives_box_pack_start(LIVES_BOX(tl_vbox), mt->eventbox, FALSE, FALSE, 4.*widget_opts.scale);
  lives_container_add(LIVES_CONTAINER(mt->eventbox), hbox);

  mt->scroll_label = lives_standard_label_new(_("Scroll"));

  lives_box_pack_start(LIVES_BOX(hbox), mt->scroll_label, FALSE, FALSE, widget_opts.packing_width);

  mt->hadjustment = (LiVESWidgetObject *)lives_adjustment_new(0.0, 0.0, 1., 0.25, 1., 1.);
  mt->time_scrollbar = lives_hscrollbar_new(LIVES_ADJUSTMENT(mt->hadjustment));

  lives_box_pack_start(LIVES_BOX(hbox), mt->time_scrollbar, TRUE, TRUE, widget_opts.packing_width);

  lives_signal_connect(LIVES_GUI_OBJECT(mt->time_scrollbar), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(scroll_time_by_scrollbar),
                       (livespointer)mt);

  mt->num_video_tracks = 0;

  mt->timeline_table = NULL;
  mt->timeline_eb = NULL;

  if (prefs->ar_layout && mt->event_list == NULL && !mainw->recoverable_layout) {
    char *eload_file = lives_build_filename(prefs->workdir, mainw->set_name, LAYOUTS_DIRNAME, prefs->ar_layout_name, NULL);
    mt->auto_reloading = TRUE;
    set_string_pref(PREF_AR_LAYOUT, ""); // in case we crash...
    mainw->event_list = mt->event_list = load_event_list(mt, eload_file);
    mt->auto_reloading = FALSE;
    lives_free(eload_file);
    if (mt->event_list != NULL) {
      mt_init_tracks(mt, TRUE);
      remove_markers(mt->event_list);
      set_string_pref(PREF_AR_LAYOUT, prefs->ar_layout_name);
    } else {
      prefs->ar_layout = FALSE;
      lives_memset(prefs->ar_layout_name, 0, 1);
      mt_init_tracks(mt, TRUE);
      mainw->unordered_blocks = FALSE;
    }
  } else if (mainw->recoverable_layout) {
    mt_load_recovery_layout(mt);
  } else {
    mt_init_tracks(mt, TRUE);
    mainw->unordered_blocks = FALSE;
  }

  mainw_message_box = mainw->message_box;
  mainw_msg_area = mainw->msg_area;
  mainw_msg_adj = mainw->msg_adj;
  mainw_msg_scrollbar = mainw->msg_scrollbar;
  mainw_sw_func = mainw->sw_func;

  mt->message_box = mainw->message_box = lives_hbox_new(FALSE, 0);

  // msg_area NOT showing for gtk+2.x
  if (prefs->show_msg_area) {
    mt->msg_area = mainw->msg_area = lives_standard_drawing_area_new(LIVES_GUI_CALLBACK(expose_msg_area), &mt->sw_func);
    mainw->sw_func = mt->sw_func;
    lives_signal_handler_block(mt->msg_area, mt->sw_func);

    lives_widget_set_events(mt->msg_area, LIVES_SMOOTH_SCROLL_MASK | LIVES_SCROLL_MASK);

    lives_widget_set_app_paintable(mt->msg_area, TRUE);
    lives_container_set_border_width(LIVES_CONTAINER(mt->message_box), 0);
    lives_box_pack_start(LIVES_BOX(mt->message_box), mt->msg_area, TRUE, TRUE, 0);
    mt->msg_scrollbar = mainw->msg_scrollbar = lives_vscrollbar_new(NULL);

    lives_box_pack_start(LIVES_BOX(mt->message_box), mt->msg_scrollbar, FALSE, TRUE, 0);
    mt->msg_adj = mainw->msg_adj = lives_range_get_adjustment(LIVES_RANGE(mt->msg_scrollbar));

    lives_signal_connect_after(LIVES_GUI_OBJECT(mt->msg_adj),
                               LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                               LIVES_GUI_CALLBACK(msg_area_scroll),
                               (livespointer)mt->msg_area);

    lives_signal_connect(LIVES_GUI_OBJECT(mt->msg_area), LIVES_WIDGET_SCROLL_EVENT,
                         LIVES_GUI_CALLBACK(on_msg_area_scroll),
                         (livespointer)mt->msg_adj);

  } else {
    mt->msg_area = mt->msg_scrollbar = NULL;
    mt->msg_adj = NULL;
  }

  lives_paned_pack(2, LIVES_PANED(mt->vpaned), mt->message_box, TRUE, TRUE);

  lives_accel_group_connect(LIVES_ACCEL_GROUP(mt->accel_group), LIVES_KEY_Page_Up, LIVES_CONTROL_MASK, (LiVESAccelFlags)0,
                            lives_cclosure_new(LIVES_GUI_CALLBACK(mt_prevclip), mt, NULL));
  lives_accel_group_connect(LIVES_ACCEL_GROUP(mt->accel_group), LIVES_KEY_Page_Down, LIVES_CONTROL_MASK, (LiVESAccelFlags)0,
                            lives_cclosure_new(LIVES_GUI_CALLBACK(mt_nextclip), mt, NULL));

  lives_accel_group_connect(LIVES_ACCEL_GROUP(mt->accel_group), LIVES_KEY_Left, LIVES_CONTROL_MASK, (LiVESAccelFlags)0,
                            lives_cclosure_new(LIVES_GUI_CALLBACK(mt_tlback), mt, NULL));
  lives_accel_group_connect(LIVES_ACCEL_GROUP(mt->accel_group), LIVES_KEY_Right, LIVES_CONTROL_MASK, (LiVESAccelFlags)0,
                            lives_cclosure_new(LIVES_GUI_CALLBACK(mt_tlfor), mt, NULL));

  lives_accel_group_connect(LIVES_ACCEL_GROUP(mt->accel_group), LIVES_KEY_Left, LIVES_SHIFT_MASK, (LiVESAccelFlags)0,
                            lives_cclosure_new(LIVES_GUI_CALLBACK(mt_tlback_frame), mt, NULL));
  lives_accel_group_connect(LIVES_ACCEL_GROUP(mt->accel_group), LIVES_KEY_Right, LIVES_SHIFT_MASK, (LiVESAccelFlags)0,
                            lives_cclosure_new(LIVES_GUI_CALLBACK(mt_tlfor_frame), mt, NULL));

  lives_accel_group_connect(LIVES_ACCEL_GROUP(mt->accel_group), LIVES_KEY_Up, LIVES_CONTROL_MASK, (LiVESAccelFlags)0,
                            lives_cclosure_new(LIVES_GUI_CALLBACK(mt_trup), mt, NULL));
  lives_accel_group_connect(LIVES_ACCEL_GROUP(mt->accel_group), LIVES_KEY_Down, LIVES_CONTROL_MASK, (LiVESAccelFlags)0,
                            lives_cclosure_new(LIVES_GUI_CALLBACK(mt_trdown), mt, NULL));

  mt->last_direction = LIVES_DIRECTION_FORWARD;

  // set check menuitems
  if (mt->opts.mouse_mode == MOUSE_MODE_MOVE) on_mouse_mode_changed(LIVES_MENU_ITEM(mt->mm_move), (livespointer)mt);
  else if (mt->opts.mouse_mode == MOUSE_MODE_SELECT) on_mouse_mode_changed(LIVES_MENU_ITEM(mt->mm_select), (livespointer)mt);

  if (mt->opts.insert_mode == INSERT_MODE_NORMAL) on_insert_mode_changed(LIVES_MENU_ITEM(mt->ins_normal), (livespointer)mt);

  if (mt->opts.grav_mode == GRAV_MODE_NORMAL) on_grav_mode_changed(LIVES_MENU_ITEM(mt->grav_normal), (livespointer)mt);
  else if (mt->opts.grav_mode == GRAV_MODE_LEFT) on_grav_mode_changed(LIVES_MENU_ITEM(mt->grav_left), (livespointer)mt);
  else if (mt->opts.grav_mode == GRAV_MODE_RIGHT) on_grav_mode_changed(LIVES_MENU_ITEM(mt->grav_right), (livespointer)mt);

  set_mt_colours(mt);

  mt_sensitise(mt);
  mt->is_ready = TRUE;

  lives_widget_set_can_focus(mt->message_box, TRUE);
  lives_widget_grab_focus(mt->message_box);

  return mt;
}


void delete_audio_track(lives_mt * mt, LiVESWidget * eventbox, boolean full) {
  // WARNING - does not yet delete events from event_list
  // only deletes visually

  track_rect *block = (track_rect *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "blocks"), *blocknext;

  LiVESWidget *label, *labelbox, *arrow, *ahbox, *xeventbox;
  lives_painter_surface_t *bgimg;

  while (block != NULL) {
    blocknext = block->next;
    if (mt->block_selected == block) mt->block_selected = NULL;
    lives_free(block);
    block = blocknext;
  }

  if ((bgimg = (lives_painter_surface_t *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "bgimg")) != NULL) {
    lives_painter_surface_destroy(bgimg);
  }

  label = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "label");
  arrow = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "arrow");
  lives_widget_destroy(label);
  lives_widget_destroy(arrow);
  if (LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "hidden")) == 0) {
    labelbox = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "labelbox");
    ahbox = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "ahbox");
    if (labelbox != NULL) lives_widget_destroy(labelbox);
    if (ahbox != NULL) lives_widget_destroy(ahbox);
  }

  xeventbox = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "achan0");
  if (xeventbox != NULL) {
    label = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(xeventbox), "label");
    lives_widget_destroy(label);
    if (LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(xeventbox), "hidden")) == 0) {
      labelbox = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(xeventbox), "labelbox");
      if (labelbox != NULL) lives_widget_destroy(labelbox);
    }
    if ((bgimg = (lives_painter_surface_t *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(xeventbox), "bgimg")) != NULL) {
      lives_painter_surface_destroy(bgimg);
    }

    lives_widget_destroy(xeventbox);
  }
  if (cfile->achans > 1) {
    xeventbox = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "achan1");
    if (xeventbox != NULL) {
      label = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(xeventbox), "label");
      lives_widget_destroy(label);
      if (LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(xeventbox), "hidden")) == 0) {
        labelbox = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(xeventbox), "labelbox");
        if (labelbox != NULL) lives_widget_destroy(labelbox);
      }
      if ((bgimg = (lives_painter_surface_t *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(xeventbox), "bgimg")) != NULL) {
        lives_painter_surface_destroy(bgimg);
      }

      lives_widget_destroy(xeventbox);
    }
  }

  lives_free(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "track_name"));
  lives_widget_destroy(eventbox);
}


static int *update_layout_map(weed_plant_t *event_list) {
  // update our current layout map with the current layout
  // returns an int * of maximum frame used for each clip that exists, 0 means unused
  int *used_clips;
  weed_plant_t *event;
  int i;

  used_clips = (int *)lives_malloc((MAX_FILES + 1) * sizint);
  for (i = 1; i <= MAX_FILES; i++) used_clips[i] = 0;

  if (event_list == NULL) return used_clips;

  event = get_first_event(event_list);

  while (event != NULL) {
    if (WEED_EVENT_IS_FRAME(event)) {
      int numtracks;
      int *clip_index = weed_get_int_array_counted(event, WEED_LEAF_CLIPS, &numtracks);
      if (numtracks > 0) {
        int64_t *frame_index = weed_get_int64_array(event, WEED_LEAF_FRAMES, NULL);
        for (int i = 0; i < numtracks; i++) {
          if (clip_index[i] > 0 && (frame_index[i] > used_clips[clip_index[i]])) used_clips[clip_index[i]] = (int)frame_index[i];
        }
        lives_free(clip_index);
        lives_free(frame_index);
      }
    }
    event = get_next_event(event);
  }
  return used_clips;
}


static double *update_layout_map_audio(weed_plant_t *event_list) {
  // update our current layout map with the current layout
  // returns a double * of maximum audio in seconds used for each clip that exists, 0. means unused
  double *used_clips;
  weed_plant_t *event;
  int i;

  // TODO - use linked lists
  double aseek[MAX_AUDIO_TRACKS];
  double avel[MAX_AUDIO_TRACKS];
  weed_timecode_t atc[MAX_AUDIO_TRACKS];
  int last_aclips[MAX_AUDIO_TRACKS];

  double neg_aseek[MAX_AUDIO_TRACKS];
  double neg_avel[MAX_AUDIO_TRACKS];
  weed_timecode_t neg_atc[MAX_AUDIO_TRACKS];
  int neg_last_aclips[MAX_AUDIO_TRACKS];

  int atrack;
  double aval;
  weed_timecode_t tc;
  int last_aclip;

  used_clips = (double *)lives_calloc((MAX_FILES + 1), sizdbl);
  if (event_list == NULL) return used_clips;

  event = get_first_event(event_list);

  for (i = 0; i < MAX_AUDIO_TRACKS; i++) {
    avel[i] = neg_avel[i] = 0.;
  }

  while (event != NULL) {
    if (WEED_EVENT_IS_FRAME(event)) {
      if (WEED_EVENT_IS_AUDIO_FRAME(event)) {
        int numatracks;
        int *aclip_index;
        double *aseek_index;
        register int i;
        numatracks = weed_frame_event_get_audio_tracks(event, &aclip_index, &aseek_index);
        for (i = 0; i < numatracks; i += 2) {
          if (aclip_index[i + 1] > 0) {
            atrack = aclip_index[i];
            tc = get_event_timecode(event);
            if (atrack >= 0) {
              if (atrack >= MAX_AUDIO_TRACKS) {
                LIVES_ERROR("invalid atrack");
              } else {
                if (avel[atrack] != 0.) {
                  aval = aseek[atrack] + (tc - atc[atrack]) / TICKS_PER_SECOND_DBL * avel[atrack];
                  last_aclip = last_aclips[atrack];
                  if (aval > used_clips[last_aclip]) used_clips[last_aclip] = aval;
                }
                aseek[atrack] = aseek_index[i];
                avel[atrack] = aseek_index[i + 1];
                atc[atrack] = tc;
                last_aclips[atrack] = aclip_index[i + 1];
              }
            } else {
              atrack = -atrack;
              if (atrack > MAX_AUDIO_TRACKS) {
                LIVES_ERROR("invalid back atrack");
              } else {
                if (neg_avel[atrack] != 0.) {
                  aval = neg_aseek[atrack] + (tc - neg_atc[atrack]) / TICKS_PER_SECOND_DBL * neg_avel[atrack];
                  last_aclip = neg_last_aclips[atrack];
                  if (aval > used_clips[last_aclip]) used_clips[last_aclip] = aval;
                }
                neg_aseek[atrack] = aseek_index[i];
                neg_avel[atrack] = aseek_index[i + 1];
                neg_atc[atrack] = tc;
                neg_last_aclips[atrack] = aclip_index[i + 1];
              }
            }
          }
        }

        lives_free(aclip_index);
        lives_free(aseek_index);
      }
    }
    event = get_next_event(event);
  }

  return used_clips;
}


boolean used_in_current_layout(lives_mt * mt, int file) {
  // see if <file> is used in current layout
  int *layout_map;
  double *layout_map_audio;
  boolean retval = FALSE;

  if (mainw->stored_event_list != NULL) {
    return (mainw->files[file]->stored_layout_frame > 0 || mainw->files[file]->stored_layout_audio > 0.);
  }

  if (mt != NULL && mt->event_list != NULL) {
    layout_map = update_layout_map(mt->event_list);
    layout_map_audio = update_layout_map_audio(mt->event_list);

    if (layout_map[file] > 0 || layout_map_audio[file] > 0.) retval = TRUE;

    lives_freep((void **)&layout_map);
    lives_freep((void **)&layout_map_audio);
  }

  return retval;
}


boolean multitrack_delete(lives_mt * mt, boolean save_layout) {
  // free lives_mt struct
  int *layout_map;
  double *layout_map_audio = NULL;

  boolean needs_idlefunc = FALSE;
  boolean did_backup = mt->did_backup;

  register int i;

  mainw->cancelled = CANCEL_NONE;

  if (mt->idlefunc > 0) {
    needs_idlefunc = TRUE;
    lives_source_remove(mt->idlefunc);
    mt->idlefunc = 0;
  }

  if (mt->frame_pixbuf != NULL && mt->frame_pixbuf != mainw->imframe) {
    lives_widget_object_unref(mt->frame_pixbuf);
    mt->frame_pixbuf = NULL;
  }

  if (save_layout || mainw->scrap_file != -1 || mainw->ascrap_file != -1) {
    if (!mainw->recording_recovered) {
      int file_selected = mt->file_selected;
      if (!check_for_layout_del(mt, TRUE)) {
        if (needs_idlefunc || (!did_backup && mt->auto_changed)) mt->idlefunc = mt_idle_add(mt);
        return FALSE;
      }
      mt->file_selected = file_selected; // because init_clips will reset this
    } else mainw->recording_recovered = FALSE;
  } else {
    if (mt->event_list != NULL) {
      save_event_list_inner(mt, -1, mt->event_list, NULL); // set width, height, fps etc.
      add_markers(mt, mt->event_list, FALSE);

      mainw->stored_event_list = mt->event_list;

#ifdef DEBUG_TTABLE
      int error;
      weed_plant_t *tevent = get_first_event(mt->event_list);
      tevent = get_next_event(tevent);
      tevent = get_next_event(tevent);
      g_print("VALXX is %p\n", weed_get_voidptr_value(tevent, WEED_LEAF_INIT_EVENT, &error));
#endif

      mt->event_list = NULL;
      mainw->stored_event_list_changed = mt->changed;
      mainw->stored_event_list_auto_changed = mt->auto_changed;
      lives_snprintf(mainw->stored_layout_name, 256, "%s", mt->layout_name);
      mainw->stored_layout_undos = mt->undos;
      mainw->sl_undo_mem = mt->undo_mem;
      mainw->sl_undo_buffer_used = mt->undo_buffer_used;
      mainw->sl_undo_offset = mt->undo_offset;
      mt->undos = NULL;
      mt->undo_mem = NULL;

      // update layout maps (kind of) with the stored_event_list

      layout_map = update_layout_map(mainw->stored_event_list);
      layout_map_audio = update_layout_map_audio(mainw->stored_event_list);

      for (i = 1; i < MAX_FILES; i++) {
        if (mainw->files[i] != NULL && (layout_map[i] != 0 || (layout_map_audio != NULL && layout_map_audio[i] != 0.))) {
          mainw->files[i]->stored_layout_frame = layout_map[i];
          if (layout_map_audio != NULL)
            mainw->files[i]->stored_layout_audio = layout_map_audio[i];
          else
            mainw->files[i]->stored_layout_audio = 0.;
          mainw->files[i]->stored_layout_fps = mainw->files[i]->fps;
          mainw->files[i]->stored_layout_idx = i;
        }
      }

      lives_freep((void **)&layout_map);
      lives_freep((void **)&layout_map_audio);
    }
  }

  if (mt->amixer != NULL) on_amixer_close_clicked(NULL, mt);

  mt->no_expose = mt->no_expose_frame = TRUE;
  mt->is_ready = FALSE;

  lives_memcpy(&mainw->multi_opts, &mt->opts, sizeof(mainw->multi_opts));
  mainw->multi_opts.aparam_view_list = lives_list_copy(mt->opts.aparam_view_list);

  mainw->multi_opts.set = TRUE;

  if (mt->poly_state == POLY_PARAMS) polymorph(mt, POLY_CLIPS);

  lives_freep((void **)&mt->undo_mem);

  if (mt->undos != NULL) lives_list_free(mt->undos);

  if (mt->selected_tracks != NULL) lives_list_free(mt->selected_tracks);

  if (mainw->event_list == mt->event_list) mainw->event_list = NULL;
  if (mt->event_list != NULL) event_list_free(mt->event_list);
  mt->event_list = NULL;

  if (mt->clip_selected >= 0 && mainw->files[mt_file_from_clip(mt, mt->clip_selected)] != NULL)
    mt_file_from_clip(mt, mt->clip_selected);

  if (mt->clip_labels != NULL) lives_list_free(mt->clip_labels);

  lives_signal_handler_block(mainw->full_screen, mainw->fullscreen_cb_func);
  lives_signal_handler_block(mainw->sepwin, mainw->sepwin_cb_func);
  lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->full_screen), mainw->fs);
  lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->sepwin), mainw->sep_win);
  lives_signal_handler_unblock(mainw->full_screen, mainw->fullscreen_cb_func);
  lives_signal_handler_unblock(mainw->sepwin, mainw->sepwin_cb_func);

  if (mainw->play_window != NULL) {
    lives_window_remove_accel_group(LIVES_WINDOW(mainw->play_window), mt->accel_group);
    lives_window_add_accel_group(LIVES_WINDOW(mainw->play_window), mainw->accel_group);
  }

  // put buttons back in mainw->menubar
  mt_swap_play_pause(mt, FALSE);

  lives_signal_handler_block(mainw->loop_continue, mainw->loop_cont_func);
  lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->loop_continue), mainw->loop_cont);
  lives_signal_handler_unblock(mainw->loop_continue, mainw->loop_cont_func);

  lives_signal_handler_block(mainw->mute_audio, mainw->mute_audio_func);
  lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->mute_audio), mainw->mute);
  lives_signal_handler_unblock(mainw->mute_audio, mainw->mute_audio_func);

  lives_widget_object_ref(mainw->m_sepwinbutton);
  lives_widget_unparent(mainw->m_sepwinbutton);
  lives_toolbar_insert(LIVES_TOOLBAR(mainw->btoolbar), LIVES_TOOL_ITEM(mainw->m_sepwinbutton), 0);
  lives_widget_object_unref(mainw->m_sepwinbutton);

  lives_widget_object_ref(mainw->m_rewindbutton);
  lives_widget_unparent(mainw->m_rewindbutton);
  lives_toolbar_insert(LIVES_TOOLBAR(mainw->btoolbar), LIVES_TOOL_ITEM(mainw->m_rewindbutton), 1);
  lives_widget_object_unref(mainw->m_rewindbutton);

  lives_widget_object_ref(mainw->m_playbutton);
  lives_widget_unparent(mainw->m_playbutton);
  lives_toolbar_insert(LIVES_TOOLBAR(mainw->btoolbar), LIVES_TOOL_ITEM(mainw->m_playbutton), 2);
  lives_widget_object_unref(mainw->m_playbutton);

  lives_widget_object_ref(mainw->m_stopbutton);
  lives_widget_unparent(mainw->m_stopbutton);
  lives_toolbar_insert(LIVES_TOOLBAR(mainw->btoolbar), LIVES_TOOL_ITEM(mainw->m_stopbutton), 3);
  lives_widget_object_unref(mainw->m_stopbutton);

  /*  lives_widget_object_ref(mainw->m_playselbutton);
    lives_widget_unparent(mainw->m_playselbutton);
    lives_toolbar_insert(LIVES_TOOLBAR(mainw->btoolbar),LIVES_TOOL_ITEM(mainw->m_playselbutton),4);
    lives_widget_object_unref(mainw->m_playselbutton);*/

  lives_widget_object_ref(mainw->m_loopbutton);
  lives_widget_unparent(mainw->m_loopbutton);
  lives_toolbar_insert(LIVES_TOOLBAR(mainw->btoolbar), LIVES_TOOL_ITEM(mainw->m_loopbutton), 5);
  lives_widget_object_unref(mainw->m_loopbutton);

  lives_widget_object_ref(mainw->m_mutebutton);
  lives_widget_unparent(mainw->m_mutebutton);
  lives_toolbar_insert(LIVES_TOOLBAR(mainw->btoolbar), LIVES_TOOL_ITEM(mainw->m_mutebutton), 6);
  lives_widget_object_unref(mainw->m_mutebutton);

  if (!lives_scale_button_set_orientation(LIVES_SCALE_BUTTON(mainw->volume_scale), LIVES_ORIENTATION_HORIZONTAL)) {
    if (mainw->vol_label != NULL) {
      lives_widget_object_ref(mainw->vol_label);
      lives_widget_unparent(mainw->vol_label);
      lives_toolbar_insert(LIVES_TOOLBAR(mainw->btoolbar), LIVES_TOOL_ITEM(mainw->vol_label), 7);
      lives_widget_object_unref(mainw->vol_label);
    }
  }

  lives_widget_object_ref(mainw->vol_toolitem);
  lives_widget_unparent(mainw->vol_toolitem);
  lives_toolbar_insert(LIVES_TOOLBAR(mainw->btoolbar), LIVES_TOOL_ITEM(mainw->vol_toolitem), -1);
  lives_widget_object_unref(mainw->vol_toolitem);

  if (mainw->gens_menu) {
    lives_widget_object_ref(mainw->gens_menu);
    lives_menu_detach(LIVES_MENU(mainw->gens_menu));
    lives_menu_item_set_submenu(LIVES_MENU_ITEM(mainw->gens_submenu), mainw->gens_menu);
  }

  if (mt->mt_frame_preview) {
    if (mainw->plug != NULL) {
      lives_container_remove(LIVES_CONTAINER(mainw->plug), mainw->play_image);
      lives_widget_destroy(mainw->plug);
      mainw->plug = NULL;
    }

    mainw->playarea = lives_hbox_new(FALSE, 0);

    lives_container_add(LIVES_CONTAINER(mainw->pl_eventbox), mainw->playarea);
    lives_widget_set_app_paintable(mainw->playarea, TRUE);
    lives_widget_set_app_paintable(mainw->pl_eventbox, TRUE);

    if (palette->style & STYLE_1) {
      lives_widget_set_bg_color(mainw->playframe, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
    }
    lives_widget_show(mainw->playarea);
  }

  // free our track_rects
  if (cfile->achans > 0) {
    delete_audio_tracks(mt, mt->audio_draws, FALSE);
    if (mt->audio_vols != NULL) lives_list_free(mt->audio_vols);
  }

  if (CURRENT_CLIP_IS_VALID && CLIP_TOTAL_TIME(mainw->current_file) == 0.) close_current_file(mt->file_selected);

  if (mt->video_draws != NULL) {
    for (i = 0; i < mt->num_video_tracks; i++) {
      delete_video_track(mt, i, FALSE);
    }
    lives_list_free(mt->video_draws);
  }

  lives_widget_destroy(mt->in_out_box);
  lives_widget_destroy(mt->clip_scroll);
  lives_widget_destroy(mt->fx_base_box);

  lives_list_free(mt->tl_marks);

  mainw->multitrack = NULL;

  mainw->event_list = NULL;

  lives_window_remove_accel_group(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET), mt->accel_group);
  lives_window_add_accel_group(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET), mainw->accel_group);

  for (i = 1; i < MAX_FILES; i++) {
    if (mainw->files[i] != NULL) {
      if (mainw->files[i]->event_list != NULL) {
        event_list_free(mainw->files[i]->event_list);
      }
      mainw->files[i]->event_list = NULL;
    }
  }

  if (prefs->show_gui) {
    if (lives_widget_get_parent(mt->top_vbox) != NULL) {
      lives_widget_object_ref(mt->top_vbox);
      lives_widget_unparent(mt->top_vbox);
      lives_container_add(LIVES_CONTAINER(LIVES_MAIN_WINDOW_WIDGET), mainw->top_vbox);
      lives_widget_object_unref(mainw->top_vbox);
      mainw->sw_func = mainw_sw_func;
      mainw->message_box = mainw_message_box;
      mainw->msg_area = mainw_msg_area;
      mainw->msg_adj = mainw_msg_adj;
      mainw->msg_scrollbar = mainw_msg_scrollbar;
      if (mainw->reconfig) return TRUE;
      show_lives();
      unblock_expose();
      resize(1.);
    }
  }

  if (mainw->reconfig) return TRUE;

  if (future_prefs->audio_src == AUDIO_SRC_EXT) {
    // switch back to external audio
    pref_factory_bool(PREF_REC_EXT_AUDIO, TRUE, FALSE);
  }

  lives_widget_hide(mainw->playframe);

  mainw->is_rendering = FALSE;

  reset_clipmenu();
  mainw->last_dprint_file = -1;

  if (prefs->show_gui && prefs->open_maximised) {
    lives_window_maximize(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET));
  }

  desensitize();

  // force the message area to get its correct space, in case we started in STARTUP_MT and haven't show it yet
  lives_widget_context_update();

  if (mt->file_selected != -1) {
    switch_to_file((mainw->current_file = 0), mt->file_selected);
  } else {
    resize(1);
    lives_widget_hide(mainw->playframe);
    lives_widget_context_update();
    load_start_image(0);
    load_end_image(0);
    if (prefs->show_msg_area) {
      if (mainw->idlemax == 0)
        lives_idle_add(resize_message_area, NULL);
      mainw->idlemax = DEF_IDLE_MAX;
    }
  }

  pref_factory_int(PREF_SEPWIN_TYPE, future_prefs->sepwin_type, FALSE);

  lives_free(mt);

  if (prefs->sepwin_type == SEPWIN_TYPE_STICKY && mainw->sep_win) {
    make_play_window();
  }

  if (!mainw->recoverable_layout) sensitize();

  d_print(_("====== Switched to Clip Edit mode ======\n"));

  lives_notify_int(LIVES_OSC_NOTIFY_MODE_CHANGED, STARTUP_CE);

  return TRUE;
}


static void locate_avol_init_event(lives_mt * mt, weed_plant_t *event_list, int avol_fx) {
  // once we have detected or assigned our audio volume effect, we search for a FILTER_INIT event for it
  // this becomes our mt->avol_init_event
  int error;
  char *filter_hash;
  weed_plant_t *event = get_first_event(event_list);

  while (event != NULL) {
    if (WEED_EVENT_IS_FILTER_INIT(event)) {
      filter_hash = weed_get_string_value(event, WEED_LEAF_FILTER, &error);
      if (avol_fx == weed_get_idx_for_hashname(filter_hash, TRUE)) {
        lives_free(filter_hash);
        mt->avol_init_event = event;
        return;
      }
      lives_free(filter_hash);
    }
    event = get_next_event(event);
  }
}


static track_rect *add_block_start_point(LiVESWidget * eventbox, weed_timecode_t tc, int filenum,
    weed_timecode_t offset_start, weed_plant_t *event, boolean ordered) {
  // each mt->video_draw (eventbox) has a ulong data which points to a linked list of track_rect
  // here we create a new linked list item and set the start timecode in the timeline,
  // offset in the source file, and start event
  // then append it to our list

  // "block_last" points to the last block added - not the last block in the track !!

  // note: filenum is unused and may be removed in future

  track_rect *block = (track_rect *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "blocks");
  track_rect *new_block = (track_rect *)lives_malloc(sizeof(track_rect));

  new_block->uid = lives_random();
  new_block->next = new_block->prev = NULL;
  new_block->state = BLOCK_UNSELECTED;
  new_block->start_anchored = new_block->end_anchored = FALSE;
  new_block->start_event = event;
  new_block->ordered = ordered;
  new_block->eventbox = eventbox;
  new_block->offset_start = offset_start;

  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(eventbox), "block_last", (livespointer)new_block);

  while (block != NULL) {
    if (get_event_timecode(block->start_event) > tc) {
      // found a block after insertion point
      if (block->prev != NULL) {
        block->prev->next = new_block;
        new_block->prev = block->prev;
      }
      // add as first block
      else lives_widget_object_set_data(LIVES_WIDGET_OBJECT(eventbox), "blocks", (livespointer)new_block);
      new_block->next = block;
      block->prev = new_block;
      break;
    }
    if (block->next == NULL) {
      // add as last block
      block->next = new_block;
      new_block->prev = block;
      break;
    }
    block = block->next;
  }

  // there were no blocks there
  if (block == NULL) lives_widget_object_set_data(LIVES_WIDGET_OBJECT(eventbox), "blocks", (livespointer)new_block);

  return new_block;
}


static track_rect *add_block_end_point(LiVESWidget * eventbox, weed_plant_t *event) {
  // here we add the end point to our last track_rect
  track_rect *block = (track_rect *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "block_last");
  if (block != NULL) block->end_event = event;
  return block;
}

static boolean on_tlreg_enter(LiVESWidget * widget, LiVESXEventCrossing * event, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  if (mt->cursor_style != LIVES_CURSOR_NORMAL) return FALSE;
  lives_set_cursor_style(LIVES_CURSOR_SB_H_DOUBLE_ARROW, widget);
  return FALSE;
}


static boolean on_tleb_enter(LiVESWidget * widget, LiVESXEventCrossing * event, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  if (mt->cursor_style != LIVES_CURSOR_NORMAL) return FALSE;
  lives_set_cursor_style(LIVES_CURSOR_CENTER_PTR, widget);
  return FALSE;
}


void reset_renumbering(void) {
  for (int i = 1; i <= MAX_FILES; i++) {
    if (mainw->files[i] != NULL) {
      renumbered_clips[i] = i;
    } else renumbered_clips[i] = 0;
  }
}


static void set_track_labels(lives_mt * mt) {
  register int i;

  if (weed_plant_has_leaf(mt->event_list, WEED_LEAF_TRACK_LABEL_TRACKS)) {
    int navs;
    int *navals = weed_get_int_array_counted(mt->event_list, WEED_LEAF_TRACK_LABEL_TRACKS, &navs);

    int nlabs;
    char **labs = weed_get_string_array_counted(mt->event_list, WEED_LEAF_TRACK_LABEL_VALUES, &nlabs);

    if (nlabs < navs) navs = nlabs;

    for (i = 0; i < navs; i++) {
      int nt = navals[i];
      if (nt < mt->num_video_tracks) {
        set_track_label_string(mt, nt, labs[i]);
      }
    }
    lives_free(labs);
    lives_free(navals);
  }
}


void mt_init_tracks(lives_mt * mt, boolean set_min_max) {
  LiVESWidget *label;
  LiVESList *tlist;

  mt->avol_init_event = NULL;

  tlist = mt->audio_draws;

  while (mt->audio_draws != NULL) {
    if (mt->audio_draws->data != NULL) lives_widget_destroy((LiVESWidget *)mt->audio_draws->data);
    mt->audio_draws = mt->audio_draws->next;
  }

  lives_list_free(tlist);

  tlist = mt->video_draws;

  while (mt->video_draws != NULL) {
    if (mt->video_draws->data != NULL) lives_widget_destroy((LiVESWidget *)mt->video_draws->data);
    mt->video_draws = mt->video_draws->next;
  }

  lives_list_free(tlist);
  mt->num_video_tracks = 0;

  mt->tl_label = NULL;

#ifndef ENABLE_GIW_3
  if (mt->timeline_table == NULL) {
    mt->tl_label = lives_standard_label_new(_("Timeline (seconds)"));
    lives_table_attach(LIVES_TABLE(mt->timeline_table_header), mt->tl_label, 0, 7, 0, 2, LIVES_FILL, (LiVESAttachOptions)0, 0, 0);
  }
#endif

  mt->current_track = 0;

  mt->clip_selected = mt_clip_from_file(mt, mt->file_selected);
  mt_clip_select(mt, TRUE);

  if (cfile->achans > 0 && mt->opts.back_audio_tracks > 0) {
    // start with 1 audio track
    add_audio_track(mt, -1, FALSE);
  }

  // start with 2 video tracks
  add_video_track_behind(NULL, mt);
  add_video_track_behind(NULL, mt);

  mt->current_track = 0;
  mt->block_selected = NULL;

  if (mt->timeline_eb == NULL) {

#ifdef ENABLE_GIW_3
    mt->timeline = giw_timeline_new_with_adjustment(LIVES_ORIENTATION_HORIZONTAL, 0., 0., 1000000., 1000000.);
    giw_timeline_set_unit(GIW_TIMELINE(mt->timeline), GIW_TIME_UNIT_SMH);
    giw_timeline_set_mouse_policy(GIW_TIMELINE(mt->timeline), GIW_TIMELINE_MOUSE_DISABLED);
#else
    mt->timeline = lives_standard_hruler_new();
#endif
    mt->timeline_reg = lives_event_box_new();
    label = lives_standard_label_new(""); // dummy label
    lives_container_add(LIVES_CONTAINER(mt->timeline_reg), label);

    mt->timeline_eb = lives_event_box_new();

    lives_widget_add_events(mt->timeline_eb, LIVES_POINTER_MOTION_MASK | LIVES_BUTTON1_MOTION_MASK |
                            LIVES_BUTTON_RELEASE_MASK | LIVES_BUTTON_PRESS_MASK | LIVES_ENTER_NOTIFY_MASK);
    lives_widget_add_events(mt->timeline, LIVES_BUTTON_RELEASE_MASK | LIVES_BUTTON_PRESS_MASK);
    lives_widget_add_events(mt->timeline_reg, LIVES_POINTER_MOTION_MASK | LIVES_BUTTON1_MOTION_MASK |
                            LIVES_BUTTON_RELEASE_MASK | LIVES_BUTTON_PRESS_MASK | LIVES_ENTER_NOTIFY_MASK);
    lives_signal_connect(LIVES_GUI_OBJECT(mt->timeline_eb), LIVES_WIDGET_ENTER_EVENT, LIVES_GUI_CALLBACK(on_tleb_enter),
                         (livespointer)mt);
    lives_signal_connect(LIVES_GUI_OBJECT(mt->timeline_reg), LIVES_WIDGET_ENTER_EVENT, LIVES_GUI_CALLBACK(on_tlreg_enter),
                         (livespointer)mt);

    lives_signal_connect(LIVES_GUI_OBJECT(mt->timeline), LIVES_WIDGET_MOTION_NOTIFY_EVENT,
                         LIVES_GUI_CALLBACK(return_true),
                         NULL);

    lives_signal_connect(LIVES_GUI_OBJECT(mt->timeline_eb), LIVES_WIDGET_MOTION_NOTIFY_EVENT,
                         LIVES_GUI_CALLBACK(on_timeline_update),
                         (livespointer)mt);

    lives_signal_connect(LIVES_GUI_OBJECT(mt->timeline_eb), LIVES_WIDGET_BUTTON_RELEASE_EVENT,
                         LIVES_GUI_CALLBACK(on_timeline_release),
                         (livespointer)mt);

    lives_signal_connect(LIVES_GUI_OBJECT(mt->timeline_eb), LIVES_WIDGET_BUTTON_PRESS_EVENT,
                         LIVES_GUI_CALLBACK(on_timeline_press),
                         (livespointer)mt);

    lives_signal_connect(LIVES_GUI_OBJECT(mt->timeline), LIVES_WIDGET_BUTTON_PRESS_EVENT,
                         LIVES_GUI_CALLBACK(on_timeline_press),
                         (livespointer)mt);

    lives_signal_connect(LIVES_GUI_OBJECT(mt->timeline), LIVES_WIDGET_BUTTON_RELEASE_EVENT,
                         LIVES_GUI_CALLBACK(on_timeline_release),
                         (livespointer)mt);

    lives_signal_connect(LIVES_GUI_OBJECT(mt->timeline_reg), LIVES_WIDGET_MOTION_NOTIFY_EVENT,
                         LIVES_GUI_CALLBACK(on_timeline_update),
                         (livespointer)mt);

    lives_signal_connect(LIVES_GUI_OBJECT(mt->timeline_reg), LIVES_WIDGET_BUTTON_RELEASE_EVENT,
                         LIVES_GUI_CALLBACK(on_timeline_release),
                         (livespointer)mt);

    lives_signal_connect(LIVES_GUI_OBJECT(mt->timeline_reg), LIVES_WIDGET_BUTTON_PRESS_EVENT,
                         LIVES_GUI_CALLBACK(on_timeline_press),
                         (livespointer)mt);

    lives_signal_connect(LIVES_GUI_OBJECT(mt->timeline_reg), LIVES_WIDGET_EXPOSE_EVENT,
                         LIVES_GUI_CALLBACK(expose_timeline_reg_event),
                         (livespointer)mt);

    lives_container_add(LIVES_CONTAINER(mt->timeline_eb), mt->timeline);

    mt->dumlabel1 = lives_standard_label_new(""); // dummy label

    lives_table_attach(LIVES_TABLE(mt->timeline_table_header), mt->dumlabel1, 0, 7, 0, 1,
                       (LiVESAttachOptions)(LIVES_EXPAND | LIVES_FILL),
                       (LiVESAttachOptions)(LIVES_FILL), 0, 0);

    lives_table_attach(LIVES_TABLE(mt->timeline_table_header), mt->timeline_eb, 7, TIMELINE_TABLE_COLUMNS, 0, 1,
                       (LiVESAttachOptions)(LIVES_EXPAND | LIVES_FILL),
                       (LiVESAttachOptions)(LIVES_FILL), 0, 0);

    mt->dumlabel2 = lives_standard_label_new(""); // dummy label

    lives_table_attach(LIVES_TABLE(mt->timeline_table_header), mt->dumlabel2, 0, 7, 1, 2,
                       (LiVESAttachOptions)(LIVES_EXPAND | LIVES_FILL),
                       (LiVESAttachOptions)(LIVES_FILL), 0, 0);

    lives_table_attach(LIVES_TABLE(mt->timeline_table_header), mt->timeline_reg, 7, TIMELINE_TABLE_COLUMNS, 1, 2,
                       (LiVESAttachOptions)(LIVES_EXPAND | LIVES_FILL),
                       (LiVESAttachOptions)(LIVES_FILL), 0, 0);
  }

  if (mt->event_list != NULL) {
    LiVESWidget *audio_draw;
    LiVESList *slist;
    track_rect *block;
    weed_plant_t *event, *last_event = NULL, *next_frame_event;
    weed_timecode_t tc, last_tc;
    weed_timecode_t offset_start;
    weed_timecode_t block_marker_tc = -1;
    weed_timecode_t block_marker_uo_tc = -1;
    double *aseeks;
    double avels[MAX_AUDIO_TRACKS];
    int64_t *frame_index, *new_frame_index;
    int *clip_index, *new_clip_index;
    int *block_marker_uo_tracks = NULL;
    int *aclips, *navals;
    int *block_marker_tracks = NULL;
    int tracks[MAX_VIDEO_TRACKS]; // TODO - use linked list

    boolean forced_end;
    boolean ordered;
    boolean shown_audio_warn = FALSE;

    int block_marker_uo_num_tracks = 0;
    int num_aclips, i;
    int navs, maxval;
    int last_valid_frame;
    int block_marker_num_tracks = 0;
    int last_tracks = 1; // number of video tracks on timeline
    int num_tracks;
    int j;

    if (mainw->reconfig) mt->was_undo_redo = TRUE;

    for (j = 0; j < MAX_TRACKS; j++) {
      tracks[j] = 0;
      avels[j] = 0.;
    }

    navals = weed_get_int_array_counted(mt->event_list, WEED_LEAF_AUDIO_VOLUME_TRACKS, &navs);
    if (navs > 0) {
      maxval = mt->num_video_tracks - 1;

      for (j = 0; j < navs; j++) {
        if (navals[j] > maxval) maxval = navals[j];
      }
      lives_free(navals);
      num_tracks = maxval + 1;

      if (num_tracks > mt->num_video_tracks) {
        for (j = mt->num_video_tracks; j < num_tracks; j++) {
          add_video_track_behind(NULL, mt);
        }
      }
    }

    // draw coloured blocks to represent the FRAME events
    event = get_first_event(mt->event_list);
    while (event != NULL) {
      if (WEED_EVENT_IS_MARKER(event)) {
        if (weed_get_int_value(event, WEED_LEAF_LIVES_TYPE, NULL) == EVENT_MARKER_BLOCK_START) {
          block_marker_tc = get_event_timecode(event);
          lives_freep((void **)&block_marker_tracks);
          block_marker_tracks = weed_get_int_array(event, WEED_LEAF_TRACKS, NULL);
        } else if (weed_get_int_value(event, WEED_LEAF_LIVES_TYPE, NULL) == EVENT_MARKER_BLOCK_UNORDERED) {
          block_marker_uo_tc = get_event_timecode(event);
          lives_freep((void **)&block_marker_uo_tracks);
          block_marker_uo_tracks = weed_get_int_array_counted(event, WEED_LEAF_TRACKS, &block_marker_uo_num_tracks);
        }
      } else if (WEED_EVENT_IS_FILTER_INIT(event)) {
        if (weed_plant_has_leaf(event, WEED_LEAF_IN_TRACKS)) {
          navals = weed_get_int_array_counted(event, WEED_LEAF_IN_TRACKS, &navs);
          maxval = mt->num_video_tracks - 1;

          for (j = 0; j < navs; j++) {
            if (navals[j] > maxval) maxval = navals[j];
          }
          lives_free(navals);
          num_tracks = maxval + 1;

          if (num_tracks > mt->num_video_tracks) {
            for (j = mt->num_video_tracks; j < num_tracks; j++) {
              add_video_track_behind(NULL, mt);
            }
          }
        }
      } else if (WEED_EVENT_IS_FRAME(event)) {
        tc = get_event_timecode(event);
        clip_index = weed_get_int_array_counted(event, WEED_LEAF_CLIPS, &num_tracks);
        frame_index = weed_get_int64_array(event, WEED_LEAF_FRAMES, NULL);

        if (num_tracks < last_tracks) {
          for (j = num_tracks; j < last_tracks; j++) {
            // TODO - tracks should be linked list
            if (tracks[j] > 0) {
              add_block_end_point(LIVES_WIDGET(lives_list_nth_data(mt->video_draws, j)), last_event); // end of previous rectangle
              tracks[j] = 0;
            }
          }
        }

        if (num_tracks > mt->num_video_tracks) {
          for (j = mt->num_video_tracks; j < num_tracks; j++) {
            add_video_track_behind(NULL, mt);
          }
        }

        last_tracks = num_tracks;
        new_clip_index = (int *)lives_malloc(num_tracks * sizint);
        new_frame_index = (int64_t *)lives_malloc(num_tracks * 8);
        last_valid_frame = 0;

        for (j = 0; j < num_tracks; j++) {
          // TODO - tracks should be linked list
          if (clip_index[j] > 0 && frame_index[j] > -1 && clip_index[j] <= MAX_FILES &&
              renumbered_clips[clip_index[j]] > 0 && (frame_index[j] <=
                  (int64_t)mainw->files[renumbered_clips[clip_index[j]]]->frames
                  || renumbered_clips[clip_index[j]] == mainw->scrap_file)) {
            forced_end = FALSE;
            if (tc == block_marker_tc && int_array_contains_value(block_marker_tracks, block_marker_num_tracks, j))
              forced_end = TRUE;
            if ((tracks[j] != renumbered_clips[clip_index[j]]) || forced_end) {
              // handling for block end or split blocks
              if (tracks[j] > 0) {
                add_block_end_point(LIVES_WIDGET(lives_list_nth_data(mt->video_draws, j)), last_event); // end of previous rectangle
              }
              if (clip_index[j] > 0) {
                ordered = !mainw->unordered_blocks;
                if (tc == block_marker_uo_tc && int_array_contains_value(block_marker_uo_tracks, block_marker_uo_num_tracks, j))
                  ordered = FALSE;
                // start a new rectangle
                offset_start = calc_time_from_frame(renumbered_clips[clip_index[j]], (int)frame_index[j]) * TICKS_PER_SECOND_DBL;
                add_block_start_point(LIVES_WIDGET(lives_list_nth_data(mt->video_draws, j)), tc,
                                      renumbered_clips[clip_index[j]], offset_start, event, ordered);
              }
              tracks[j] = renumbered_clips[clip_index[j]];
            }
            new_clip_index[j] = renumbered_clips[clip_index[j]];
            new_frame_index[j] = frame_index[j];
            last_valid_frame = j + 1;
          } else {
            // clip has probably been closed, so we remove its frames

            // TODO - do similar check for audio
            new_clip_index[j] = -1;
            new_frame_index[j] = 0;
            if (tracks[j] > 0) {
              add_block_end_point(LIVES_WIDGET(lives_list_nth_data(mt->video_draws, j)), last_event); // end of previous rectangle
              tracks[j] = 0;
            }
          }
        }

        if (last_valid_frame == 0) {
          lives_free(new_clip_index);
          lives_free(new_frame_index);
          new_clip_index = (int *)lives_malloc(sizint);
          new_frame_index = (int64_t *)lives_malloc(8);
          *new_clip_index = -1;
          *new_frame_index = 0;
          num_tracks = 1;
        } else {
          if (last_valid_frame < num_tracks) {
            lives_free(new_clip_index);
            lives_free(new_frame_index);
            new_clip_index = (int *)lives_malloc(last_valid_frame * sizint);
            new_frame_index = (int64_t *)lives_malloc(last_valid_frame * 8);
            for (j = 0; j < last_valid_frame; j++) {
              new_clip_index[j] = clip_index[j];
              new_frame_index[j] = frame_index[j];
            }
            num_tracks = last_valid_frame;
          }
        }

        weed_set_int_array(event, WEED_LEAF_CLIPS, num_tracks, new_clip_index);
        weed_set_int64_array(event, WEED_LEAF_FRAMES, num_tracks, new_frame_index);

        lives_free(clip_index);
        lives_free(new_clip_index);
        lives_free(frame_index);
        lives_free(new_frame_index);

        if (WEED_EVENT_IS_AUDIO_FRAME(event)) {
          // audio starts or stops here
          aclips = weed_get_int_array_counted(event, WEED_LEAF_AUDIO_CLIPS, &num_aclips);
          aseeks = weed_get_double_array(event, WEED_LEAF_AUDIO_SEEKS, NULL);
          for (i = 0; i < num_aclips; i += 2) {
            if (aclips[i + 1] > 0) {
              if (cfile->achans == 0) {
                if (!shown_audio_warn) {
                  shown_audio_warn = TRUE;
                  do_mt_audchan_error(WARN_MASK_MT_ACHANS);
                }
              } else {
                if (aclips[i] == -1) audio_draw = (LiVESWidget *)mt->audio_draws->data;
                else audio_draw = (LiVESWidget *)lives_list_nth_data(mt->audio_draws, aclips[i] + mt->opts.back_audio_tracks);
                if (avels[aclips[i] + 1] != 0.) {
                  add_block_end_point(audio_draw, event);
                }
                //if (renumbered_clips[clip_index[aclips[i+1]]]>0) {
                avels[aclips[i] + 1] = aseeks[i + 1];
                //}
                if (avels[aclips[i] + 1] != 0.) {
                  add_block_start_point(audio_draw, tc, renumbered_clips[aclips[i + 1]], aseeks[i]*TICKS_PER_SECOND_DBL, event, TRUE);
                }
              }
            }
            if (aclips[i + 1] > 0) aclips[i + 1] = renumbered_clips[aclips[i + 1]];
          }
          weed_set_int_array(event, WEED_LEAF_AUDIO_CLIPS, num_aclips, aclips);
          lives_free(aseeks);
          lives_free(aclips);
        }

        slist = mt->audio_draws;
        for (i = mt->opts.back_audio_tracks + 1; --i > 0; slist = slist->next);
        for (; slist != NULL; slist = slist->next, i++) {
          // handling for split blocks
          if (tc == block_marker_tc && int_array_contains_value(block_marker_tracks, block_marker_num_tracks, -i)) {
            audio_draw = (LiVESWidget *)slist->data;
            if (avels[i] != 0.) {
              // end the current block and add a new one
              // note we only add markers here, when drawing the block audio events will be added
              block = add_block_end_point(audio_draw, event);
              if (block != NULL) {
                last_tc = get_event_timecode(block->start_event);
                offset_start = block->offset_start + (weed_timecode_t)((double)(tc - last_tc) * avels[i] + .5);
                add_block_start_point(audio_draw, tc, -1, offset_start, event, TRUE);
		// *INDENT-OFF*
              }}}}
	  // *INDENT-ON*

        next_frame_event = get_next_frame_event(event);

        if (next_frame_event == NULL) {
          // this is the last FRAME event, so close all our rectangles
          j = 0;
          for (slist = mt->video_draws; slist != NULL; slist = slist->next) {
            if (tracks[j++] > 0) {
              add_block_end_point(LIVES_WIDGET(slist->data), event);
            }
          }
          j = 0;
          for (slist = mt->audio_draws; slist != NULL; slist = slist->next) {
            if (cfile->achans > 0 && avels[j++] != 0.) add_block_end_point((LiVESWidget *)slist->data, event);
          }
        }
        last_event = event;
      }
      event = get_next_event(event);
    }
    if (!mt->was_undo_redo) remove_end_blank_frames(mt->event_list, TRUE);
    lives_freep((void **)&block_marker_tracks);
    lives_freep((void **)&block_marker_uo_tracks);

    if (cfile->achans > 0 && mt->opts.back_audio_tracks > 0) lives_widget_show(mt->view_audio);

    if (mt->avol_fx != -1) locate_avol_init_event(mt, mt->event_list, mt->avol_fx);

    if (!mt->was_undo_redo && mt->avol_fx != -1 && mt->audio_draws != NULL) {
      apply_avol_filter(mt);
    }
  }

  mt->end_secs = 0.;
  if (mt->event_list != NULL) {
    mt->end_secs = event_list_get_end_secs(mt->event_list) * 2.;
    if (mt->end_secs == 0.) LIVES_WARN("got zero length event_list");
  }
  if (mt->end_secs == 0.) mt->end_secs = DEF_TIME;

  if (set_min_max) {
    mt->tl_min = 0.;
    mt->tl_max = mt->end_secs;
  }

  set_timeline_end_secs(mt, mt->end_secs);

  if (!mt->was_undo_redo) {
    set_track_labels(mt);

    if (mt->is_ready) {
      if (mt->current_track != 0) {
        mt->current_track = 0;
        track_select(mt);
      }
      if (mt->region_start != mt->region_end || mt->region_start != 0.) {
        mt->region_start = mt->region_end = 0.;
        draw_region(mt);
      }
    }
    mt_tl_move(mt, 0.);
  } else mt->was_undo_redo = FALSE;

  reset_renumbering();
}


void delete_video_track(lives_mt * mt, int layer, boolean full) {
  // WARNING - does not yet delete events from event_list
  // only deletes visually

  LiVESWidget *eventbox = (LiVESWidget *)lives_list_nth_data(mt->video_draws, layer);
  track_rect *block = (track_rect *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "blocks"), *blocknext;

  LiVESWidget *checkbutton;
  LiVESWidget *label, *labelbox, *ahbox, *arrow;
  lives_painter_surface_t *bgimg;

  while (block != NULL) {
    blocknext = block->next;
    if (mt->block_selected == block) mt->block_selected = NULL;
    lives_free(block);
    block = blocknext;
  }

  if ((bgimg = (lives_painter_surface_t *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "bgimg")) != NULL) {
    lives_painter_surface_destroy(bgimg);
  }

  checkbutton = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "checkbutton");
  label = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "label");
  arrow = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "arrow");

  mt->cb_list = lives_list_remove(mt->cb_list, (livespointer)checkbutton);

  lives_widget_destroy(checkbutton);
  lives_widget_destroy(label);
  lives_widget_destroy(arrow);
  if (LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "hidden")) == 0) {
    labelbox = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "labelbox");
    ahbox = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "ahbox");
    if (labelbox != NULL) lives_widget_destroy(labelbox);
    if (ahbox != NULL) lives_widget_destroy(ahbox);
  }
  lives_free(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "track_name"));

  // corresponding audio track will be deleted in delete_audio_track(s)

  lives_widget_destroy(eventbox);
}


LiVESWidget *add_audio_track(lives_mt * mt, int track, boolean behind) {
  // add float or pertrack audio track to our timeline_table
  LiVESWidgetObject *adj;
  LiVESWidget *label, *dummy;
  LiVESWidget *arrow;
  LiVESWidget *eventbox;
  LiVESWidget *vbox;
  LiVESWidget *audio_draw = lives_event_box_new();
  char *pname, *tname;
  int max_disp_vtracks = prefs->max_disp_vtracks - 1;
  int llen, vol = 0;
  int nachans = 0;
  int i;

  lives_widget_object_ref(audio_draw);

  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(audio_draw), "blocks", (livespointer)NULL);
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(audio_draw), "block_last", (livespointer)NULL);
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(audio_draw), "hidden", LIVES_INT_TO_POINTER(0));
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(audio_draw), "expanded", LIVES_INT_TO_POINTER(FALSE));
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(audio_draw), "bgimg", NULL);
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(audio_draw), "is_audio", LIVES_INT_TO_POINTER(TRUE));

  lives_adjustment_set_upper(LIVES_ADJUSTMENT(mt->vadjustment), (double)mt->num_video_tracks);
  lives_adjustment_set_page_size(LIVES_ADJUSTMENT(mt->vadjustment), (double)(max_disp_vtracks > mt->num_video_tracks ?
                                 mt->num_video_tracks : max_disp_vtracks));

  dummy = lives_event_box_new();
  lives_widget_object_ref(dummy);

  widget_opts.justify = LIVES_JUSTIFY_LEFT;
  if (track == -1) {
    label = lives_label_new(_(" Backing audio"));
  } else {
    char *tmp = lives_strdup_printf(_(" Layer %d audio"), track);
    label = lives_label_new(tmp);
    lives_free(tmp);
  }

  lives_widget_set_halign(label, LIVES_ALIGN_START);

  widget_opts.justify = LIVES_JUSTIFY_DEFAULT;
  lives_widget_object_ref(label);

  arrow = lives_arrow_new(LIVES_ARROW_RIGHT, LIVES_SHADOW_OUT);
  lives_widget_set_tooltip_text(arrow, _("Show/hide audio details"));
  lives_widget_object_ref(arrow);

  lives_widget_object_set_data_widget_object(LIVES_WIDGET_OBJECT(audio_draw), "label", label);
  lives_widget_object_set_data_widget_object(LIVES_WIDGET_OBJECT(audio_draw), "dummy", dummy);
  lives_widget_object_set_data_widget_object(LIVES_WIDGET_OBJECT(audio_draw), "arrow", arrow);

  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(arrow), "layer_number", LIVES_INT_TO_POINTER(track));
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(audio_draw), "layer_number", LIVES_INT_TO_POINTER(track));
  lives_widget_object_set_data_auto(LIVES_WIDGET_OBJECT(audio_draw), "track_name", lives_strdup_printf(_("Layer %d audio"),
                                    track));

  // add channel subtracks
  for (i = 0; i < cfile->achans; i++) {
    eventbox = lives_event_box_new();
    lives_widget_object_ref(eventbox);
    pname = lives_strdup_printf("achan%d", i);
    lives_widget_object_set_data_widget_object(LIVES_WIDGET_OBJECT(audio_draw), pname, eventbox);
    lives_free(pname);

    widget_opts.justify = LIVES_JUSTIFY_RIGHT;
    tname = get_achannel_name(cfile->achans, i);
    label = lives_label_new(tname);
    lives_free(tname);

    lives_widget_set_halign(label, LIVES_ALIGN_END);

    widget_opts.justify = LIVES_JUSTIFY_DEFAULT;
    lives_widget_object_ref(label);

    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(eventbox), "owner", audio_draw);
    lives_widget_object_set_data_widget_object(LIVES_WIDGET_OBJECT(eventbox), "label", label);
    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(eventbox), "hidden", LIVES_INT_TO_POINTER(0));
    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(eventbox), "bgimg", NULL);
    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(eventbox), "channel", LIVES_INT_TO_POINTER(i));
  }

  lives_widget_queue_draw(mt->vpaned);

  if (!mt->was_undo_redo) {
    // calc layer volume value
    llen = lives_list_length(mt->audio_draws);
    if (llen == 0) {
      // set vol to 1.0
      vol = LIVES_AVOL_SCALE;
    } else if (llen == 1) {
      if (mt->opts.back_audio_tracks > 0) {
        vol = LIVES_AVOL_SCALE / 2.;
        mt->audio_vols->data = LIVES_INT_TO_POINTER(vol);
      } else {
        if (mt->opts.gang_audio) {
          vol = LIVES_POINTER_TO_INT(lives_list_nth_data(mt->audio_vols, 0));
        } else vol = LIVES_AVOL_SCALE;
      }
    } else {
      if (mt->opts.gang_audio) {
        vol = LIVES_POINTER_TO_INT(lives_list_nth_data(mt->audio_vols, mt->opts.back_audio_tracks));
      } else {
        if (mt->opts.back_audio_tracks > 0) {
          vol = LIVES_AVOL_SCALE / 2.;
        } else {
          vol = LIVES_AVOL_SCALE;
        }
      }
    }
  }

  if (!mt->was_undo_redo && mt->amixer != NULL && track >= 0) {
    // if mixer is open add space for another slider
    LiVESWidget **ch_sliders;
    ulong *ch_slider_fns;

    int j = 0;

    nachans = lives_list_length(mt->audio_vols) + 1;

    ch_sliders = (LiVESWidget **)lives_malloc(nachans * sizeof(LiVESWidget *));
    ch_slider_fns = (ulong *)lives_malloc(nachans * sizeof(ulong));

    // make a gap
    for (i = 0; i < nachans - 1; i++) {
      if (!behind && i == mt->opts.back_audio_tracks) j++;
      ch_sliders[j] = mt->amixer->ch_sliders[i];
      ch_slider_fns[j] = mt->amixer->ch_slider_fns[i];
      j++;
    }

    lives_free(mt->amixer->ch_sliders);
    lives_free(mt->amixer->ch_slider_fns);

    mt->amixer->ch_sliders = ch_sliders;
    mt->amixer->ch_slider_fns = ch_slider_fns;
  }

  if (track == -1) {
    mt->audio_draws = lives_list_prepend(mt->audio_draws, (livespointer)audio_draw);
    if (!mt->was_undo_redo) mt->audio_vols = lives_list_prepend(mt->audio_vols, LIVES_INT_TO_POINTER(vol));
  } else if (behind) {
    mt->audio_draws = lives_list_append(mt->audio_draws, (livespointer)audio_draw);

    if (!mt->was_undo_redo) {
      if (mt->amixer != NULL) {
        // if mixer is open add a new track at end
        vbox = amixer_add_channel_slider(mt, nachans - 1 - mt->opts.back_audio_tracks);
        lives_box_pack_start(LIVES_BOX(mt->amixer->main_hbox), vbox, FALSE, FALSE, widget_opts.packing_width);
        lives_widget_show_all(vbox);
      }

      mt->audio_vols = lives_list_append(mt->audio_vols, LIVES_INT_TO_POINTER(vol));
    }
  } else {
    mt->audio_draws = lives_list_insert(mt->audio_draws, (livespointer)audio_draw, mt->opts.back_audio_tracks);
    if (!mt->was_undo_redo) {
      mt->audio_vols = lives_list_insert(mt->audio_vols, LIVES_INT_TO_POINTER(vol), mt->opts.back_audio_tracks);

      if (mt->amixer != NULL) {
        // if mixer is open add a new track at posn 0 and update all labels and layer numbers
        vbox = amixer_add_channel_slider(mt, 0);

        // pack at posn 2
        lives_box_pack_start(LIVES_BOX(mt->amixer->main_hbox), vbox, FALSE, FALSE, widget_opts.packing_width);
        lives_box_reorder_child(LIVES_BOX(mt->amixer->main_hbox), vbox, 2);
        lives_widget_show_all(vbox);

        // update labels and layer numbers

        for (i = mt->opts.back_audio_tracks + 1; i < nachans; i++) {
          label = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(mt->amixer->ch_sliders[i]), "label");
          tname = get_track_name(mt, i - mt->opts.back_audio_tracks, TRUE);
          widget_opts.mnemonic_label = FALSE;
          lives_label_set_text(LIVES_LABEL(label), tname);
          widget_opts.mnemonic_label = TRUE;
          lives_free(tname);

          adj = (LiVESWidgetObject *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(mt->amixer->ch_sliders[i]), "adj");
          lives_widget_object_set_data(LIVES_WIDGET_OBJECT(adj), "layer", LIVES_INT_TO_POINTER(i));
	  // *INDENT-OFF*
        }}}}
  // *INDENT-ON*

  return audio_draw;
}


static void set_track_label(LiVESEventBox * xeventbox, int tnum) {
  LiVESWidget *label = LIVES_WIDGET(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(xeventbox), "label"));
  const char *tname = (const char *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(xeventbox), "track_name");
  char *newtext = lives_strdup_printf(_("%s (layer %d)"), tname, tnum);
  widget_opts.mnemonic_label = FALSE;
  lives_label_set_text(LIVES_LABEL(label), newtext);
  widget_opts.mnemonic_label = TRUE;
  lives_free(newtext);
}


void set_track_label_string(lives_mt * mt, int track, const char *label) {
  LiVESWidget *ebox = get_eventbox_for_track(mt, track);
  if (ebox == NULL) return;
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(ebox), "track_name", (livespointer)label);
  set_track_label(LIVES_EVENT_BOX(ebox), track);
}


static int add_video_track(lives_mt * mt, boolean behind) {
  // add another video track to our timeline_table
  LiVESWidget *label;
  LiVESWidget *checkbutton;
  LiVESWidget *arrow;
  LiVESWidget *eventbox;  // each track has an eventbox, which we store in LiVESList *video_draws
  LiVESWidget *aeventbox; // each track has optionally an associated audio track, which we store in LiVESList *audio_draws
  LiVESList *liste;
  int max_disp_vtracks = prefs->max_disp_vtracks;
  int i;
  char *tmp;

  if (mt->audio_draws != NULL && mt->audio_draws->data != NULL && mt->opts.back_audio_tracks > 0 &&
      LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(mt->audio_draws->data), "hidden")) == 0) {
    max_disp_vtracks--;
    if (LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(mt->audio_draws->data),
                             "expanded"))) max_disp_vtracks -= cfile->achans;
  }

  mt->num_video_tracks++;

#ifdef ENABLE_GIW
  if (!prefs->lamp_buttons) {
#endif
    checkbutton = lives_check_button_new();
#ifdef ENABLE_GIW
  } else {
    checkbutton = giw_led_new();
    giw_led_enable_mouse(GIW_LED(checkbutton), TRUE);
  }
#endif
  lives_widget_object_ref(checkbutton);

  mt->cb_list = lives_list_append(mt->cb_list, checkbutton);

  if (LIVES_IS_WIDGET(checkbutton)) {
    lives_widget_set_tooltip_text(checkbutton, _("Select track"));
  }

  arrow = lives_arrow_new(LIVES_ARROW_RIGHT, LIVES_SHADOW_OUT);
  lives_widget_set_tooltip_text(arrow, _("Show/hide audio"));
  lives_widget_object_ref(arrow);

  eventbox = lives_event_box_new();
  lives_widget_object_ref(eventbox);

  lives_widget_object_set_data_auto(LIVES_WIDGET_OBJECT(eventbox), "track_name", lives_strdup_printf(_("Video %d"),
                                    mt->num_video_tracks));
  lives_widget_object_set_data_widget_object(LIVES_WIDGET_OBJECT(eventbox), "checkbutton", (livespointer)checkbutton);
  lives_widget_object_set_data_widget_object(LIVES_WIDGET_OBJECT(eventbox), "arrow", (livespointer)arrow);
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(eventbox), "expanded", LIVES_INT_TO_POINTER(FALSE));

  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(eventbox), "blocks", (livespointer)NULL);
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(eventbox), "block_last", (livespointer)NULL);
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(eventbox), "hidden", LIVES_INT_TO_POINTER(0));
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(eventbox), "bgimg", NULL);

  lives_adjustment_set_page_size(LIVES_ADJUSTMENT(mt->vadjustment), (double)(max_disp_vtracks > mt->num_video_tracks ?
                                 mt->num_video_tracks : max_disp_vtracks));
  lives_adjustment_set_upper(LIVES_ADJUSTMENT(mt->vadjustment), (double)(mt->num_video_tracks +
                             (int)lives_adjustment_get_page_size(LIVES_ADJUSTMENT(mt->vadjustment))));

  if (!behind) {
    // track in front of (above) stack
    // shift all rows down
    // increment "layer_number"s, change labels
    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(eventbox), "layer_number", LIVES_INT_TO_POINTER(0));
    for (i = 0; i < mt->num_video_tracks - 1; i++) {
      char *newtext;
      LiVESWidget *xeventbox = (LiVESWidget *)lives_list_nth_data(mt->video_draws, i);
      LiVESWidget *xcheckbutton = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(xeventbox), "checkbutton");
      LiVESWidget *xarrow = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(xeventbox), "arrow");
      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(xeventbox), "layer_number", LIVES_INT_TO_POINTER(i + 1));
      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(xcheckbutton), "layer_number", LIVES_INT_TO_POINTER(i + 1));
      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(xarrow), "layer_number", LIVES_INT_TO_POINTER(i + 1));
      lives_widget_object_set_data(LIVES_WIDGET_OBJECT(xeventbox), "is_audio", LIVES_INT_TO_POINTER(FALSE));

      set_track_label(LIVES_EVENT_BOX(xeventbox), i + 1);

      if (mt->opts.pertrack_audio) {
        LiVESWidget *aeventbox = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(xeventbox), "atrack");
        lives_widget_object_set_data(LIVES_WIDGET_OBJECT(aeventbox), "layer_number", LIVES_INT_TO_POINTER(i + 1));
        xarrow = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(aeventbox), "arrow");
        lives_widget_object_set_data(LIVES_WIDGET_OBJECT(xarrow), "layer_number", LIVES_INT_TO_POINTER(i + 1));
        label = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(aeventbox), "label");
        lives_widget_object_set_data_auto(LIVES_WIDGET_OBJECT(aeventbox), "track_name", lives_strdup_printf(_("Layer %d audio"),
                                          i + 1));
        newtext = lives_strdup_printf(_(" %s"), lives_widget_object_get_data(LIVES_WIDGET_OBJECT(aeventbox), "track_name"));
        widget_opts.mnemonic_label = FALSE;
        lives_label_set_text(LIVES_LABEL(label), newtext);
        widget_opts.mnemonic_label = TRUE;
        lives_widget_object_set_data(LIVES_WIDGET_OBJECT(aeventbox), "is_audio", LIVES_INT_TO_POINTER(TRUE));
      }
    }
    // add a -1,0 in all frame events
    // renumber "in_tracks", "out_tracks" in effect_init events
    event_list_add_track(mt->event_list, 0);

    mt->video_draws = lives_list_prepend(mt->video_draws, (livespointer)eventbox);
    mt->current_track = 0;

    //renumber all tracks in mt->selected_tracks
    liste = mt->selected_tracks;
    while (liste != NULL) {
      liste->data = LIVES_INT_TO_POINTER(LIVES_POINTER_TO_INT(liste->data) + 1);
      liste = liste->next;
    }
  } else {
    // add track behind (below) stack
    mt->video_draws = lives_list_append(mt->video_draws, (livespointer)eventbox);
    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(eventbox), "layer_number", LIVES_INT_TO_POINTER(mt->num_video_tracks - 1));
    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(checkbutton), "layer_number", LIVES_INT_TO_POINTER(mt->num_video_tracks - 1));
    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(arrow), "layer_number", LIVES_INT_TO_POINTER(mt->num_video_tracks - 1));
    mt->current_track = mt->num_video_tracks - 1;
  }

  widget_opts.justify = LIVES_JUSTIFY_LEFT;
  label = lives_label_new((tmp = lives_strdup_printf(_("%s (layer %d)"),
                                 lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "track_name"),
                                 LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox),
                                     "layer_number")))));
  widget_opts.justify = widget_opts.default_justify;

  lives_free(tmp);
  lives_widget_object_ref(label);

  lives_widget_object_set_data_widget_object(LIVES_WIDGET_OBJECT(eventbox), "label", label);

  if (mt->opts.pertrack_audio) {
    aeventbox = add_audio_track(mt, mt->current_track, behind);
    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(aeventbox), "owner", eventbox);
    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(eventbox), "atrack", aeventbox);

    if (mt->avol_init_event != NULL) {
      weed_plant_t *filter = get_weed_filter(mt->avol_fx);
      add_track_to_avol_init(filter, mt->avol_init_event, mt->opts.back_audio_tracks, behind);
    }
  }
  if (!behind) scroll_track_on_screen(mt, 0);
  else scroll_track_on_screen(mt, mt->num_video_tracks - 1);

  lives_widget_queue_draw(mt->vpaned);

  if (!behind) return 0;
  else return mt->num_video_tracks - 1;
}


int add_video_track_behind(LiVESMenuItem * menuitem, livespointer user_data) {
  int tnum;
  lives_mt *mt = (lives_mt *)user_data;
  if (menuitem != NULL) mt_desensitise(mt);
  tnum = add_video_track(mt, TRUE);
  if (menuitem != NULL) mt_sensitise(mt);
  return tnum;
}


int add_video_track_front(LiVESMenuItem * menuitem, livespointer user_data) {
  int tnum;
  lives_mt *mt = (lives_mt *)user_data;
  if (menuitem != NULL) mt_desensitise(mt);
  tnum = add_video_track(mt, FALSE);
  if (menuitem != NULL) mt_sensitise(mt);
  return tnum;
}


void on_mt_fx_edit_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  if (mt->selected_init_event == NULL) return;
  fubar(mt);
  polymorph(mt, POLY_PARAMS);
  mt_show_current_frame(mt, FALSE);
  lives_widget_set_sensitive(mt->apply_fx_button, FALSE);
  lives_widget_set_sensitive(mt->resetp_button, check_can_resetp(mt));
}


static boolean mt_fx_edit_idle(livespointer user_data) {
  on_mt_fx_edit_activate(NULL, user_data);
  return FALSE;
}


static void mt_avol_quick(LiVESMenuItem * menuitem, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  mt->selected_init_event = mt->avol_init_event;
  on_mt_fx_edit_activate(menuitem, user_data);
}


static void rdrw_cb(LiVESMenuItem * menuitem, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  redraw_all_event_boxes(mt);
}


void do_effect_context(lives_mt * mt, LiVESXEventButton * event) {
  // pop up a context menu when a selected block is right clicked on

  LiVESWidget *edit_effect;
  LiVESWidget *delete_effect;
  LiVESWidget *menu = lives_menu_new();

  weed_plant_t *filter;
  char *fhash;

  lives_menu_set_title(LIVES_MENU(menu), _("Selected Effect"));

  fhash = weed_get_string_value(mt->selected_init_event, WEED_LEAF_FILTER, NULL);
  filter = get_weed_filter(weed_get_idx_for_hashname(fhash, TRUE));
  lives_free(fhash);

  if (num_in_params(filter, TRUE, TRUE) > 0) {
    edit_effect = lives_standard_menu_item_new_with_label(_("_View/Edit this Effect"));
  } else {
    edit_effect = lives_standard_menu_item_new_with_label(_("_View this Effect"));
  }
  lives_container_add(LIVES_CONTAINER(menu), edit_effect);

  lives_signal_connect(LIVES_GUI_OBJECT(edit_effect), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_mt_fx_edit_activate),
                       (livespointer)mt);

  delete_effect = lives_standard_menu_item_new_with_label(_("_Delete this Effect"));
  if (mt->selected_init_event != mt->avol_init_event) {
    lives_container_add(LIVES_CONTAINER(menu), delete_effect);

    lives_signal_connect(LIVES_GUI_OBJECT(delete_effect), LIVES_WIDGET_ACTIVATE_SIGNAL,
                         LIVES_GUI_CALLBACK(on_mt_delfx_activate),
                         (livespointer)mt);
  }

  if (palette->style & STYLE_1) {
    set_child_alt_colour(menu, TRUE);
    lives_widget_apply_theme2(menu, LIVES_WIDGET_STATE_NORMAL, TRUE);
  }

  lives_widget_show_all(menu);
  lives_menu_popup(LIVES_MENU(menu), event);
}


static boolean fx_ebox_pressed(LiVESWidget * eventbox, LiVESXEventButton * event, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  LiVESList *children, *xlist;
  weed_plant_t *osel = mt->selected_init_event;

  if (mt->is_rendering) return FALSE;

  mt->selected_init_event = (weed_plant_t *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "init_event");

  if (event->type == LIVES_BUTTON2_PRESS) {
    // double click
    mt->moving_fx = NULL;
    if (!LIVES_IS_PLAYING) {
      lives_timer_add(0, mt_fx_edit_idle, mt); // work around issue in gtk+
    }
    return FALSE;
  }

  if (!LIVES_IS_PLAYING) {
    if (mt->fx_order != FX_ORD_NONE) {
      if (osel != mt->selected_init_event && osel != mt->avol_init_event) {
        switch (mt->fx_order) {
        case FX_ORD_BEFORE:
          /// backup events and note timecode of filter map
          mt_backup(mt, MT_UNDO_FILTER_MAP_CHANGE, get_event_timecode(mt->fm_edit_event));

          /// move the init event in the filter map
          move_init_in_filter_map(mt, mt->event_list, mt->fm_edit_event, osel, mt->selected_init_event,
                                  mt->current_track, FALSE);
          break;
        case FX_ORD_AFTER:
          if (init_event_is_process_last(mt->selected_init_event)) {
            // cannot insert after a process_last effect
            clear_context(mt);
            add_context_label(mt, _("Cannot insert after this effect"));
            mt->selected_init_event = osel;
            mt->fx_order = FX_ORD_NONE;
            return FALSE;
          }

          /// backup events and note timecode of filter map
          mt_backup(mt, MT_UNDO_FILTER_MAP_CHANGE, get_event_timecode(mt->fm_edit_event));

          /// move the init event in the filter map
          move_init_in_filter_map(mt, mt->event_list, mt->fm_edit_event, osel, mt->selected_init_event,
                                  mt->current_track, TRUE);
          break;

        default:
          break;
        }
      }

      mt->did_backup = FALSE;
      mt->selected_init_event = osel;
      mt->fx_order = FX_ORD_NONE;
      mt->selected_init_event = NULL;
      polymorph(mt, POLY_FX_STACK);
      mt_show_current_frame(mt, FALSE); ///< show updated preview
      return FALSE;
    }
    if (mt->fx_order == FX_ORD_NONE && WEED_EVENT_IS_FILTER_MAP(mt->fm_edit_event)) {
      if (init_event_is_process_last(mt->selected_init_event)) {
        clear_context(mt);
        add_context_label(mt, _("This effect cannot be moved"));
        if (mt->fx_ibefore_button != NULL) lives_widget_set_sensitive(mt->fx_ibefore_button, FALSE);
        if (mt->fx_iafter_button != NULL) lives_widget_set_sensitive(mt->fx_iafter_button, FALSE);
      } else {
        do_fx_move_context(mt);
        if (mt->fx_ibefore_button != NULL) lives_widget_set_sensitive(mt->fx_ibefore_button, TRUE);
        if (mt->fx_iafter_button != NULL) lives_widget_set_sensitive(mt->fx_iafter_button, TRUE);
      }
    }
    lives_widget_set_sensitive(mt->fx_edit, TRUE);
    if (mt->selected_init_event != mt->avol_init_event) lives_widget_set_sensitive(mt->fx_delete, TRUE);
  }

  if (widget_opts.apply_theme) {
    // set clicked-on widget to selected state and reset all others
    xlist = children = lives_container_get_children(LIVES_CONTAINER(mt->fx_list_vbox));
    while (children != NULL) {
      LiVESWidget *child = (LiVESWidget *)children->data;
      if (child != eventbox) {
        lives_widget_apply_theme(child, LIVES_WIDGET_STATE_NORMAL);
        set_child_colour(child, TRUE);
      } else {
        lives_widget_apply_theme2(child, LIVES_WIDGET_STATE_NORMAL, TRUE);
        set_child_alt_colour(child, TRUE);
      }
      children = children->next;
    }
    if (xlist != NULL) lives_list_free(xlist);
  }
  if (event->button == 3 && !LIVES_IS_PLAYING) {
    do_effect_context(mt, event);
  }

  return FALSE;
}


static void set_clip_labels_variable(lives_mt * mt, int i) {
  char *tmp;
  LiVESLabel *label1, *label2;
  lives_clip_t *sfile = mainw->files[i];

  if (mt->clip_labels == NULL) return;

  i = mt_clip_from_file(mt, i);
  i *= 2;

  label1 = (LiVESLabel *)lives_list_nth_data(mt->clip_labels, i);
  label2 = (LiVESLabel *)lives_list_nth_data(mt->clip_labels, ++i);

  // sets the width of the box
  lives_label_set_text(label1, (tmp = lives_strdup_printf(_("%d to %d selected"), sfile->start, sfile->end)));
  lives_free(tmp);

  lives_label_set_text(label2, (tmp = lives_strdup_printf(_("%.2f sec."), (sfile->end - sfile->start + 1.) / sfile->fps)));
  lives_free(tmp);
}


void mt_clear_timeline(lives_mt * mt) {
  int i;
  char *msg;

  for (i = 0; i < mt->num_video_tracks; i++) {
    delete_video_track(mt, i, FALSE);
  }
  lives_list_free(mt->video_draws);
  mt->video_draws = NULL;
  mt->num_video_tracks = 0;
  mainw->event_list = mt->event_list = NULL;

  if (mt->selected_tracks != NULL) lives_list_free(mt->selected_tracks);
  mt->selected_tracks = NULL;

  if (mt->amixer != NULL) on_amixer_close_clicked(NULL, mt);

  delete_audio_tracks(mt, mt->audio_draws, FALSE);
  mt->audio_draws = NULL;
  if (mt->audio_vols != NULL) lives_list_free(mt->audio_vols);
  mt->audio_vols = NULL;

  mt_init_tracks(mt, TRUE);

  unselect_all(mt);
  mt->changed = FALSE;

  msg = set_values_from_defs(mt, FALSE);

  if (cfile->achans == 0) mt->opts.pertrack_audio = FALSE;

  if (msg != NULL) {
    d_print(msg);
    lives_free(msg);
    set_mt_title(mt);
  }

  // reset avol_fx
  if (cfile->achans > 0 && mainw->fx_candidates[FX_CANDIDATE_AUDIO_VOL].delegate != -1) {
    // user (or system) has delegated an audio volume filter from the candidates
    mt->avol_fx = LIVES_POINTER_TO_INT(lives_list_nth_data(mainw->fx_candidates[FX_CANDIDATE_AUDIO_VOL].list,
                                       mainw->fx_candidates[FX_CANDIDATE_AUDIO_VOL].delegate));
  } else mt->avol_fx = -1;
  mt->avol_init_event = NULL;

  add_aparam_menuitems(mt);

  lives_memset(mt->layout_name, 0, 1);

  mt_show_current_frame(mt, FALSE);
}


void mt_delete_clips(lives_mt * mt, int file) {
  // close eventbox(es) for a given file
  LiVESList *list = lives_container_get_children(LIVES_CONTAINER(mt->clip_inner_box)), *list_next, *olist = list;
  LiVESList *clist = mt->clip_labels, *clist_nnext;

  boolean removed = FALSE;
  int neg = 0, i = 0;

  for (; list != NULL; i++) {
    list_next = list->next;
    clist_nnext = clist->next->next;
    if (clips_to_files[i] == file) {
      removed = TRUE;

      lives_widget_destroy((LiVESWidget *)list->data);

      if (clist_nnext->next != NULL) clist_nnext->next->prev = clist->prev;
      if (clist->prev != NULL) clist->prev->next = clist_nnext->next;
      else mt->clip_labels = clist_nnext->next;
      clist->prev = clist_nnext->next = NULL;
      lives_list_free(clist);

      lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET, TRUE);

      neg++;
    }
    clips_to_files[i] = clips_to_files[i + neg];
    list = list_next;
    clist = clist_nnext;
  }

  if (olist != NULL) lives_list_free(olist);

  if (mt->event_list != NULL && removed && used_in_current_layout(mt, file)) {
    int current_file = mainw->current_file;

    if (!event_list_rectify(mt, mt->event_list) || get_first_event(mt->event_list) == NULL) {
      // delete the current layout
      mainw->current_file = mt->render_file;
      remove_current_from_affected_layouts(mt);
      mainw->current_file = current_file;
    } else {
      mainw->current_file = mt->render_file;
      mt_init_tracks(mt, FALSE);
      mainw->current_file = current_file;
    }
  }

  if (CURRENT_CLIP_IS_VALID) {
    lives_widget_set_sensitive(mt->adjust_start_end, FALSE);
  }
}


void mt_init_clips(lives_mt * mt, int orig_file, boolean add) {
  // setup clip boxes in the poly window. if add is TRUE then we are just adding a new clip
  // orig_file is the file we want to select

  // mt_clip_select() should be called after this

  LiVESWidget *thumb_image = NULL;
  LiVESWidget *vbox, *hbox, *label;
  LiVESWidget *eventbox;

  LiVESPixbuf *thumbnail;

  LiVESList *cliplist = mainw->cliplist;

  char filename[PATH_MAX];
  char clip_name[CLIP_LABEL_LENGTH];
  char *tmp;

  int i = 1;
  int width = CLIP_THUMB_WIDTH, height = CLIP_THUMB_HEIGHT;
  int count = lives_list_length(mt->clip_labels) / 2;

  mt->clip_selected = -1;

  if (add) i = orig_file;

  while (add || cliplist != NULL) {
    if (add) i = orig_file;
    else i = LIVES_POINTER_TO_INT(cliplist->data);
    if (!IS_NORMAL_CLIP(i)) {
      cliplist = cliplist->next;
      continue;
    }
    if (i != mainw->scrap_file && i != mainw->ascrap_file) {
      if (i == orig_file || (mt->clip_selected == -1 && i == mainw->pre_src_file)) {
        if (!add) mt->clip_selected = mt_clip_from_file(mt, i);
        else {
          mt->file_selected = i;
          mt->clip_selected = count;
          renumbered_clips[i] = i;
        }
      }
      // remove the "no clips" label
      if (mt->nb_label != NULL) lives_widget_destroy(mt->nb_label);
      mt->nb_label = NULL;

      // make a small thumbnail, add it to the clips box
      thumbnail = make_thumb(mt, i, width, height, mainw->files[i]->start, LIVES_INTERP_BEST, TRUE);

      eventbox = lives_event_box_new();
      lives_widget_add_events(eventbox, LIVES_BUTTON_RELEASE_MASK | LIVES_BUTTON_PRESS_MASK | LIVES_ENTER_NOTIFY_MASK);
      lives_signal_connect(LIVES_GUI_OBJECT(eventbox), LIVES_WIDGET_ENTER_EVENT, LIVES_GUI_CALLBACK(on_clipbox_enter),
                           (livespointer)mt);

      clips_to_files[count] = i;

      vbox = lives_vbox_new(FALSE, widget_opts.packing_height);

      thumb_image = lives_image_new();
      lives_image_set_from_pixbuf(LIVES_IMAGE(thumb_image), thumbnail);
      if (thumbnail != NULL) lives_widget_object_unref(thumbnail);
      lives_container_add(LIVES_CONTAINER(eventbox), vbox);
      lives_box_pack_start(LIVES_BOX(mt->clip_inner_box), eventbox, FALSE, FALSE, 0);

      lives_snprintf(filename, PATH_MAX, "%s", (tmp = get_menu_name(mainw->files[i], FALSE)));
      lives_free(tmp);
      get_basename(filename);
      lives_snprintf(clip_name, CLIP_LABEL_LENGTH, "%s", filename);

      widget_opts.justify = LIVES_JUSTIFY_CENTER;
      label = lives_standard_label_new(clip_name);
      lives_box_pack_start(LIVES_BOX(vbox), label, FALSE, FALSE, widget_opts.packing_height);
      lives_box_pack_start(LIVES_BOX(vbox), thumb_image, FALSE, FALSE, widget_opts.packing_height);

      if (mainw->files[i]->frames > 0) {
        label = lives_standard_label_new((tmp = lives_strdup_printf(_("%d frames"), mainw->files[i]->frames)));
        lives_box_pack_start(LIVES_BOX(vbox), label, FALSE, FALSE, widget_opts.packing_height);

        label = lives_standard_label_new("");
        mt->clip_labels = lives_list_append(mt->clip_labels, label);

        hbox = lives_hbox_new(FALSE, 0);
        lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, 0);
        lives_box_pack_start(LIVES_BOX(hbox), label, FALSE, TRUE, widget_opts.border_width);

        label = lives_standard_label_new("");
        mt->clip_labels = lives_list_append(mt->clip_labels, label);

        lives_box_pack_start(LIVES_BOX(vbox), label, FALSE, FALSE, widget_opts.packing_height);

        set_clip_labels_variable(mt, i);
      } else {
        label = lives_standard_label_new(_("audio only"));
        mt->clip_labels = lives_list_append(mt->clip_labels, label);

        hbox = lives_hbox_new(FALSE, 0);
        lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);
        lives_box_pack_start(LIVES_BOX(hbox), label, FALSE, TRUE, widget_opts.border_width);

        label = lives_standard_label_new((tmp = lives_strdup_printf(_("%.2f sec."), mainw->files[i]->laudio_time)));
        lives_box_pack_start(LIVES_BOX(vbox), label, FALSE, FALSE, widget_opts.packing_height);
        mt->clip_labels = lives_list_append(mt->clip_labels, label);
      }
      lives_free(tmp);
      widget_opts.justify = LIVES_JUSTIFY_DEFAULT;

      count++;

      lives_signal_connect(LIVES_GUI_OBJECT(eventbox), LIVES_WIDGET_BUTTON_PRESS_EVENT,
                           LIVES_GUI_CALLBACK(clip_ebox_pressed),
                           (livespointer)mt);
      lives_signal_connect(LIVES_GUI_OBJECT(eventbox), LIVES_WIDGET_BUTTON_RELEASE_EVENT,
                           LIVES_GUI_CALLBACK(on_drag_clip_end),
                           (livespointer)mt);

      if (add) {
        lives_widget_show_all(eventbox);
        break;
      }
    }
    cliplist = cliplist->next;
  }
}


static void set_audio_mixer_vols(lives_mt * mt, weed_plant_t *elist) {
  int *atracks;
  double *avols;
  weed_error_t error;
  int catracks;
  int i, xtrack, xavol;
  int natracks, navols;

  if (!weed_plant_has_leaf(elist, WEED_LEAF_AUDIO_VOLUME_TRACKS) ||
      !weed_plant_has_leaf(elist, WEED_LEAF_AUDIO_VOLUME_VALUES)) return;

  natracks = weed_leaf_num_elements(elist, WEED_LEAF_AUDIO_VOLUME_TRACKS);
  navols = weed_leaf_num_elements(elist, WEED_LEAF_AUDIO_VOLUME_VALUES);

  atracks = weed_get_int_array(elist, WEED_LEAF_AUDIO_VOLUME_TRACKS, &error);
  if (error != WEED_SUCCESS) return;

  avols = weed_get_double_array(elist, WEED_LEAF_AUDIO_VOLUME_VALUES, &error);
  if (error != WEED_SUCCESS) {
    lives_free(atracks);
    return;
  }

  catracks = lives_list_length(mt->audio_vols);
  for (i = 0; i < natracks; i++) {
    xtrack = atracks[i];
    if (xtrack < -mt->opts.back_audio_tracks) continue;
    if (xtrack >= catracks - mt->opts.back_audio_tracks) continue;

    xavol = i;
    if (xavol >= navols) {
      mt->opts.gang_audio = TRUE;
      xavol = navols - 1;
    }
    set_mixer_track_vol(mt, xtrack + mt->opts.back_audio_tracks, avols[xavol]);
  }

  lives_free(atracks);
  lives_free(avols);
}


boolean mt_idle_show_current_frame(livespointer mt) {
  mt_show_current_frame((lives_mt *)mt, FALSE);
  return FALSE;
}


boolean on_multitrack_activate(LiVESMenuItem * menuitem, weed_plant_t *event_list) {
  //returns TRUE if we go into mt mode
  lives_mt *multi;

  const char *mtext;

  boolean response;

  int orig_file;

  xachans = xarate = xasamps = xse = 0;
  ptaud = prefs->mt_pertrack_audio;
  btaud = prefs->mt_backaudio;

  if (mainw->frame_layer != NULL) weed_layer_free(mainw->frame_layer);
  mainw->frame_layer = NULL;

  if (prefs->mt_enter_prompt && mainw->stored_event_list == NULL && prefs->show_gui && !(mainw->recoverable_layout &&
      prefs->startup_interface == STARTUP_CE)) {
    // WARNING:
    rdet = create_render_details(3); // WARNING !! - rdet is global in events.h
    rdet->enc_changed = FALSE;
    do {
      rdet->suggestion_followed = FALSE;
      if ((response = lives_dialog_run(LIVES_DIALOG(rdet->dialog))) == LIVES_RESPONSE_OK) {
        if (rdet->enc_changed) {
          check_encoder_restrictions(FALSE, lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(resaudw->aud_checkbutton)), TRUE);
        }
      }
    } while (rdet->suggestion_followed || response == LIVES_RESPONSE_RESET);

    if (response == LIVES_RESPONSE_CANCEL) {
      lives_widget_destroy(rdet->dialog);
      lives_free(rdet->encoder_name);
      lives_freep((void **)&rdet);
      lives_freep((void **)&resaudw);
      return FALSE;
    }

    if (resaudw != NULL) {
      xarate = (int)atoi(lives_entry_get_text(LIVES_ENTRY(resaudw->entry_arate)));
      xachans = (int)atoi(lives_entry_get_text(LIVES_ENTRY(resaudw->entry_achans)));
      xasamps = (int)atoi(lives_entry_get_text(LIVES_ENTRY(resaudw->entry_asamps)));

      if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(resaudw->rb_unsigned))) {
        xse = AFORM_UNSIGNED;
      }

      if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(resaudw->rb_bigend))) {
        xse |= AFORM_BIG_ENDIAN;
      }

      if (!lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(resaudw->aud_checkbutton))) {
        xachans = 0;
      }

      ptaud = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(rdet->pertrack_checkbutton));
      btaud = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(rdet->backaudio_checkbutton));
    } else {
      xarate = xachans = xasamps = 0;
      xse = cfile->signed_endian;
    }

    if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(rdet->always_checkbutton))) {
      prefs->mt_enter_prompt = FALSE;
      set_boolean_pref(PREF_MT_ENTER_PROMPT, prefs->mt_enter_prompt);
      prefs->mt_def_width = rdet->width;
      set_int_pref(PREF_MT_DEF_WIDTH, prefs->mt_def_width);
      prefs->mt_def_height = rdet->height;
      set_int_pref(PREF_MT_DEF_HEIGHT, prefs->mt_def_height);
      prefs->mt_def_fps = rdet->fps;
      set_double_pref(PREF_MT_DEF_FPS, prefs->mt_def_fps);
      prefs->mt_def_arate = xarate;
      set_int_pref(PREF_MT_DEF_ARATE, prefs->mt_def_arate);
      prefs->mt_def_achans = xachans;
      set_int_pref(PREF_MT_DEF_ACHANS, prefs->mt_def_achans);
      prefs->mt_def_asamps = xasamps;
      set_int_pref(PREF_MT_DEF_ASAMPS, prefs->mt_def_asamps);
      prefs->mt_def_signed_endian = xse;
      set_int_pref(PREF_MT_DEF_SIGNED_ENDIAN, prefs->mt_def_signed_endian);
      prefs->mt_pertrack_audio = ptaud;
      set_boolean_pref(PREF_MT_PERTRACK_AUDIO, prefs->mt_pertrack_audio);
      prefs->mt_backaudio = btaud;
      set_int_pref(PREF_MT_BACKAUDIO, prefs->mt_backaudio);
    } else {
      if (!prefs->mt_enter_prompt) {
        prefs->mt_enter_prompt = TRUE;
        set_boolean_pref(PREF_MT_ENTER_PROMPT, prefs->mt_enter_prompt);
      }
    }

    lives_widget_destroy(rdet->dialog);
  }

  if (mainw->current_file > -1 && cfile != NULL && cfile->clip_type == CLIP_TYPE_GENERATOR) {
    // shouldn't be playing, so OK to just call this
    weed_generator_end((weed_plant_t *)cfile->ext_src);
  }

  if (prefs->show_gui) {
    lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET, TRUE); // force showing of transient window
  }

  // create new file for rendering to
  renumber_clips();
  orig_file = mainw->current_file;
  mainw->current_file = mainw->first_free_file;

  if (!get_new_handle(mainw->current_file, NULL)) {
    mainw->current_file = orig_file;
    if (rdet != NULL) {
      lives_free(rdet->encoder_name);
      lives_freep((void **)&rdet);
      lives_freep((void **)&resaudw);
    }
    return FALSE; // show dialog again
  }

  cfile->img_type = IMG_TYPE_BEST; // override the pref

  cfile->bpp = cfile->img_type == IMG_TYPE_JPEG ? 24 : 32;
  cfile->changed = TRUE;
  cfile->is_loaded = TRUE;

  cfile->old_frames = cfile->frames;

  force_pertrack_audio = FALSE;
  force_backing_tracks = 0;

  if (mainw->stored_event_list != NULL) {
    event_list = mainw->stored_event_list;
    rerenumber_clips(NULL, event_list);
  }

  // if we have an existing event list, we will quantise it to the selected fps
  if (event_list != NULL) {
    weed_plant_t *qevent_list = quantise_events(event_list, cfile->fps, FALSE);
    if (qevent_list == NULL) {
      if (rdet != NULL) {
        lives_free(rdet->encoder_name);
        lives_freep((void **)&rdet);
        lives_freep((void **)&resaudw);
      }
      return FALSE; // memory error
    }
    event_list_replace_events(event_list, qevent_list);
    weed_set_double_value(event_list, WEED_LEAF_FPS, cfile->fps);
    event_list_rectify(NULL, event_list);
  }

  if (prefs->show_gui) block_expose();

  ///////// CREATE MULTITRACK CONTENTS ////////////
  multi = multitrack(event_list, orig_file, cfile->fps); // also frees rdet
  ////////////////////////////////////////////////

  pref_factory_int(PREF_SEPWIN_TYPE, SEPWIN_TYPE_NON_STICKY, FALSE);

  if (mainw->stored_event_list != NULL) {
    mainw->stored_event_list = NULL;
    mainw->stored_layout_undos = NULL;
    mainw->sl_undo_mem = NULL;
    stored_event_list_free_all(FALSE);
    if (multi->event_list == NULL) {
      multi->clip_selected = mt_clip_from_file(multi, orig_file);
      multi->file_selected = orig_file;
      mainw->sw_func = mainw_sw_func;
      mainw->message_box = mainw_message_box;
      mainw->msg_area = mainw_msg_area;
      mainw->msg_adj = mainw_msg_adj;
      mainw->msg_scrollbar = mainw_msg_scrollbar;
      if (prefs->show_gui) unblock_expose();
      return FALSE;
    }
    remove_markers(multi->event_list);
    set_audio_mixer_vols(multi, multi->event_list);
    lives_snprintf(multi->layout_name, 256, "%s", mainw->stored_layout_name);
    multi->changed = mainw->stored_event_list_changed;
    multi->auto_changed = mainw->stored_event_list_auto_changed;
    if (multi->auto_changed) lives_widget_set_sensitive(multi->backup, TRUE);
  }

  if (mainw->recoverable_layout && multi->event_list == NULL && prefs->startup_interface == STARTUP_CE) {
    // failed to load recovery layout
    multi->clip_selected = mt_clip_from_file(multi, orig_file);
    multi->file_selected = orig_file;
    mainw->sw_func = mainw_sw_func;
    mainw->message_box = mainw_message_box;
    mainw->msg_area = mainw_msg_area;
    mainw->msg_adj = mainw_msg_adj;
    mainw->msg_scrollbar = mainw->msg_scrollbar;
    if (prefs->show_gui) unblock_expose();
    return FALSE;
  }

  if (prefs->show_gui) {
    lives_widget_object_ref(mainw->top_vbox);
    lives_widget_unparent(mainw->top_vbox);
  }

  lives_container_add(LIVES_CONTAINER(LIVES_MAIN_WINDOW_WIDGET), multi->top_vbox);

  if (prefs->show_gui) {
    lives_widget_show_all(multi->top_vbox);
    show_lives();
    scroll_track_on_screen(multi, 0);
    if (multi->nb_label != NULL) {
      lives_widget_hide(multi->poly_box);
      lives_widget_queue_resize(multi->nb_label);
    }

    mtext = lives_menu_item_get_text(multi->recent[0]);
    if (!strlen(mtext)) lives_widget_hide(multi->recent[0]);
    mtext = lives_menu_item_get_text(multi->recent[1]);
    if (!strlen(mtext)) lives_widget_hide(multi->recent[1]);
    mtext = lives_menu_item_get_text(multi->recent[2]);
    if (!strlen(mtext)) lives_widget_hide(multi->recent[2]);
    mtext = lives_menu_item_get_text(multi->recent[3]);
    if (!strlen(mtext)) lives_widget_hide(multi->recent[3]);
  }

  if (cfile->achans == 0) {
    multi->opts.pertrack_audio = FALSE;
  }

  if (!is_realtime_aplayer(prefs->audio_player)) {
    lives_widget_hide(mainw->vol_toolitem);
    if (mainw->vol_label != NULL) lives_widget_hide(mainw->vol_label);
  }

  if (!multi->opts.show_ctx) {
    lives_widget_hide(multi->context_frame);
    lives_widget_hide(multi->scrolledwindow);
    lives_widget_hide(multi->sep_image);
  }

  if (!(palette->style & STYLE_4)) {
    lives_widget_hide(multi->hseparator);
    if (multi->hseparator2 != NULL) {
      lives_widget_hide(multi->hseparator2);
    }
  }

  if (!multi->opts.pertrack_audio) {
    lives_widget_hide(multi->insa_checkbutton);
  }

  track_select(multi);

  if (mainw->preview_box != NULL && lives_widget_get_parent(mainw->preview_box) != NULL) {
    lives_widget_object_unref(mainw->preview_box);
    lives_container_remove(LIVES_CONTAINER(mainw->play_window), mainw->preview_box);
    mainw->preview_box = NULL;
  }

  if (mainw->play_window != NULL) {
    lives_window_remove_accel_group(LIVES_WINDOW(mainw->play_window), mainw->accel_group);
    lives_window_add_accel_group(LIVES_WINDOW(mainw->play_window), multi->accel_group);
    resize_play_window();
  }

  if (cfile->achans > 0 && !is_realtime_aplayer(prefs->audio_player)) {
    do_mt_no_jack_error(WARN_MASK_MT_NO_JACK);
  }

  mainw->is_ready = TRUE;

  if (prefs->show_gui && prefs->open_maximised) {
    lives_window_maximize(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET));
  }

  // calls widget_context_update() ///////////////////////
  set_mt_play_sizes(multi, cfile->hsize, cfile->vsize, TRUE);
  ////////////////////////////////////////////////////////

  redraw_all_event_boxes(multi);

  if (multi->opts.pertrack_audio) {
    lives_toggle_button_toggle(LIVES_TOGGLE_BUTTON(multi->insa_checkbutton));
  }
  lives_toggle_button_toggle(LIVES_TOGGLE_BUTTON(multi->snapo_checkbutton));

  if (multi->msg_area != NULL) lives_signal_handler_unblock(multi->msg_area, multi->sw_func);

  lives_widget_context_update();
  lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET, TRUE);
  mt_clip_select(multi, TRUE); // call this again to scroll clip on screen

  lives_toggle_button_toggle(LIVES_TOGGLE_BUTTON(multi->insa_checkbutton));
  lives_toggle_button_toggle(LIVES_TOGGLE_BUTTON(multi->snapo_checkbutton));

  if (multi->opts.hpaned_pos != -1)
    lives_paned_set_position(LIVES_PANED(multi->hpaned), multi->opts.hpaned_pos);
  else
    lives_paned_set_position(LIVES_PANED(multi->hpaned), (float)lives_widget_get_allocation_width(multi->hpaned) * .8); // 0 == top

  lives_paned_set_position(LIVES_PANED(multi->vpaned), (float)lives_widget_get_allocation_height(multi->vpaned) * .0); // 0 == top

  multi->no_expose = multi->no_expose_frame = FALSE;

  lives_container_child_set_shrinkable(LIVES_CONTAINER(multi->hpaned), multi->context_frame, TRUE);

  if (prefs->audio_src == AUDIO_SRC_EXT) {
    // switch to internal audio for multitrack
    pref_factory_bool(PREF_REC_EXT_AUDIO, FALSE, FALSE);
  }

  lives_signal_connect(LIVES_GUI_OBJECT(multi->hpaned), LIVES_WIDGET_NOTIFY_SIGNAL "position",
                       LIVES_GUI_CALLBACK(hpaned_position),
                       (livespointer)multi);

  lives_signal_connect(LIVES_GUI_OBJECT(multi->vpaned), LIVES_WIDGET_NOTIFY_SIGNAL "position",
                       LIVES_GUI_CALLBACK(paned_position),
                       (livespointer)multi);

  if (prefs->startup_interface == STARTUP_MT && mainw->go_away) {
    return FALSE;
  }

  /// call again to get allocated sizes
  set_mt_play_sizes(multi, cfile->hsize, cfile->vsize, FALSE);
  lives_idle_add(mt_idle_show_current_frame, (livespointer)multi);

  set_interactive(mainw->interactive);
  if (mainw->reconfig) return FALSE;

  d_print(_("====== Switched to Multitrack mode ======\n"));

  if (prefs->show_msg_area) {
    if (mainw->idlemax == 0)
      lives_idle_add(resize_message_area, NULL);
    mainw->idlemax = DEF_IDLE_MAX;
  }

  mt_show_current_frame(multi, FALSE);

  lives_notify_int(LIVES_OSC_NOTIFY_MODE_CHANGED, STARTUP_MT);

  return TRUE;
}


boolean block_overlap(LiVESWidget * eventbox, double time_start, double time_end) {
  weed_timecode_t tc_start = time_start * TICKS_PER_SECOND;
  weed_timecode_t tc_end = time_end * TICKS_PER_SECOND;
  track_rect *block = (track_rect *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "blocks");

  while (block != NULL) {
    if (get_event_timecode(block->start_event) > tc_end) return FALSE;
    if (get_event_timecode(block->end_event) >= tc_start) return TRUE;
    block = block->next;
  }
  return FALSE;
}


static track_rect *get_block_before(LiVESWidget * eventbox, double time, boolean allow_cur) {
  // get the last block which ends before or at time
  // if allow_cur is TRUE, we may count blocks whose end is after "time" but whose start is
  // before or at time

  weed_timecode_t tc = time * TICKS_PER_SECOND;
  track_rect *block = (track_rect *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "blocks"), *last_block = NULL;

  while (block != NULL) {
    if ((allow_cur && get_event_timecode(block->start_event) >= tc) || (!allow_cur &&
        get_event_timecode(block->end_event) >= tc)) break;
    last_block = block;
    block = block->next;
  }
  return last_block;
}


static track_rect *get_block_after(LiVESWidget * eventbox, double time, boolean allow_cur) {
  // return the first block which starts at or after time
  // if allow_cur is TRUE, we may count blocks whose end is after "time" but whose start is
  // before or at time

  weed_timecode_t tc = time * TICKS_PER_SECOND;
  track_rect *block = (track_rect *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "blocks");

  while (block != NULL) {
    if (get_event_timecode(block->start_event) >= tc || (allow_cur && get_event_timecode(block->end_event) >= tc)) break;
    block = block->next;
  }
  return block;
}


track_rect *move_block(lives_mt * mt, track_rect * block, double timesecs, int old_track, int new_track) {
  weed_timecode_t new_start_tc, end_tc;
  weed_timecode_t start_tc = get_event_timecode(block->start_event);

  ulong uid = block->uid;

  LiVESWidget *eventbox, *oeventbox;

  int clip, current_track = -1;

  boolean did_backup = mt->did_backup;
  boolean needs_idlefunc = FALSE;

  if (mt->idlefunc > 0) {
    needs_idlefunc = TRUE;
    lives_source_remove(mt->idlefunc);
    mt->idlefunc = 0;
  }

  if (is_audio_eventbox(block->eventbox) && (oeventbox =
        (LiVESWidget *)lives_widget_object_get_data
        (LIVES_WIDGET_OBJECT(block->eventbox), "owner")) != NULL) {
    // if moving an audio block we move the associated video block first
    block = get_block_from_time(oeventbox, start_tc / TICKS_PER_SECOND_DBL, mt);
  }

  mt->block_selected = block;
  end_tc = get_event_timecode(block->end_event);

  if (mt->opts.insert_mode == INSERT_MODE_NORMAL) {
    // first check if there is space to move the block to, otherwise we will abort the move
    weed_plant_t *event = NULL;
    weed_timecode_t tc = 0, tcnow;
    weed_timecode_t tclen = end_tc - start_tc;
    while (tc <= tclen) {
      tcnow = q_gint64(tc + timesecs * TICKS_PER_SECOND_DBL, mt->fps);
      tc += TICKS_PER_SECOND_DBL / mt->fps;
      if (old_track == new_track && tcnow >= start_tc && tcnow <= end_tc) continue; // ignore ourself !
      event = get_frame_event_at(mt->event_list, tcnow, event, TRUE);
      if (event == NULL) break; // must be end of timeline
      if (new_track >= 0) {
        // is video track, if we have a non-blank frame, abort
        if (get_frame_event_clip(event, new_track) >= 0) {
          if (!did_backup) {
            if (needs_idlefunc || (!did_backup && mt->auto_changed)) mt->idlefunc = mt_idle_add(mt);
          }
          return NULL;
        }
      } else {
        // is audio track, see if we are in an audio block
        // or if one starts here
        if ((tc == start_tc && get_audio_block_start(mt->event_list, new_track, tcnow, TRUE) != NULL) ||
            (get_audio_block_start(mt->event_list, new_track, tcnow, FALSE) != NULL)) {
          if (!did_backup) {
            if (needs_idlefunc || (!did_backup && mt->auto_changed)) mt->idlefunc = mt_idle_add(mt);
          }
          return NULL;
        }
      }
    }
  }

  if (old_track < 0) mt_backup(mt, MT_UNDO_MOVE_AUDIO_BLOCK, 0);
  else mt_backup(mt, MT_UNDO_MOVE_BLOCK, 0);

  mt->specific_event = get_prev_event(block->start_event);
  while (mt->specific_event != NULL && get_event_timecode(mt->specific_event) == start_tc) {
    mt->specific_event = get_prev_event(mt->specific_event);
  }

  if (old_track > -1) {
    clip = get_frame_event_clip(block->start_event, old_track);
    mt->insert_start = block->offset_start;
    mt->insert_end = block->offset_start + end_tc - start_tc + q_gint64(TICKS_PER_SECOND_DBL / mt->fps, mt->fps);
  } else {
    clip = get_audio_frame_clip(block->start_event, old_track);
    mt->insert_avel = get_audio_frame_vel(block->start_event, old_track);
    mt->insert_start = q_gint64(get_audio_frame_seek(block->start_event, old_track) * TICKS_PER_SECOND_DBL, mt->fps);
    mt->insert_end = q_gint64(mt->insert_start + (end_tc - start_tc), mt->fps);
  }

  mt->moving_block = TRUE;
  mt->current_track = old_track;
  delete_block_cb(NULL, (livespointer)mt);
  mt->block_selected = NULL;
  mt->current_track = new_track;
  track_select(mt);
  mt->clip_selected = mt_clip_from_file(mt, clip);
  mt_clip_select(mt, TRUE);
  mt_tl_move(mt, timesecs);

  if (new_track != -1) insert_here_cb(NULL, (livespointer)mt);

  else {
    insert_audio_here_cb(NULL, (livespointer)mt);
    mt->insert_avel = 1.;
  }

  mt->insert_start = mt->insert_end = -1;

  new_start_tc = q_gint64(timesecs * TICKS_PER_SECOND_DBL, mt->fps);

  remove_end_blank_frames(mt->event_list, FALSE); // leave filter inits

  // if !move_effects we deleted fx in delete_block, here we move them
  if (mt->opts.move_effects) update_filter_events(mt, mt->specific_event, start_tc, end_tc, old_track, new_start_tc,
        mt->current_track);

  remove_end_blank_frames(mt->event_list, TRUE); // remove filter inits
  mt->moving_block = FALSE;
  mt->specific_event = NULL;

  if (new_track != -1) eventbox = (LiVESWidget *)lives_list_nth_data(mt->video_draws, mt->current_track);
  else eventbox = (LiVESWidget *)mt->audio_draws->data;
  block = (track_rect *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "block_last");

  if (block != NULL && (mt->opts.grav_mode == GRAV_MODE_LEFT ||
                        (mt->opts.grav_mode == GRAV_MODE_RIGHT && block->next != NULL)) &&
      !did_backup) {
    double oldr_start = mt->region_start;
    double oldr_end = mt->region_end;
    LiVESList *tracks_sel = NULL;
    track_rect *lblock;
    double rtc = get_event_timecode(block->start_event) / TICKS_PER_SECOND_DBL - 1. / mt->fps, rstart = 0., rend;

    if (mt->opts.grav_mode == GRAV_MODE_LEFT) {
      // gravity left - move left until we hit another block or time 0
      if (rtc >= 0.) {
        lblock = block->prev;
        if (lblock != NULL) rstart = get_event_timecode(lblock->end_event) / TICKS_PER_SECOND_DBL;
      }
      rend = get_event_timecode(block->end_event) / TICKS_PER_SECOND_DBL;
    } else {
      // gravity right - move right until we hit the next block
      lblock = block->next;
      rstart = get_event_timecode(block->start_event) / TICKS_PER_SECOND_DBL;
      rend = get_event_timecode(lblock->start_event) / TICKS_PER_SECOND_DBL;
    }

    mt->region_start = rstart;
    mt->region_end = rend;

    if (new_track > -1) {
      tracks_sel = lives_list_copy(mt->selected_tracks);
      if (mt->selected_tracks != NULL) lives_list_free(mt->selected_tracks);
      mt->selected_tracks = NULL;
      mt->selected_tracks = lives_list_append(mt->selected_tracks, LIVES_INT_TO_POINTER(new_track));
    } else {
      current_track = mt->current_track;
      mt->current_track = old_track;
    }

    remove_first_gaps(NULL, mt);
    if (old_track > -1) {
      lives_list_free(mt->selected_tracks);
      mt->selected_tracks = lives_list_copy(tracks_sel);
      if (tracks_sel != NULL) lives_list_free(tracks_sel);
    } else mt->current_track = current_track;
    mt->region_start = oldr_start;
    mt->region_end = oldr_end;
    mt_sensitise(mt);
  }

  // get this again because it could have moved
  block = (track_rect *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "block_last");

  if (!did_backup) {
    if (mt->avol_fx != -1 && (block == NULL || block->next == NULL) && mt->audio_draws != NULL &&
        mt->audio_draws->data != NULL && get_first_event(mt->event_list) != NULL) {
      apply_avol_filter(mt);
    }
  }

  // apply autotransition
  if (prefs->atrans_fx != -1) {
    // add the insert and autotrans as 2 separate undo events
    mt->did_backup = did_backup;
    mt_do_autotransition(mt, block);
    mt->did_backup = TRUE;
  }

  if (!did_backup && mt->framedraw != NULL && mt->current_rfx != NULL && mt->init_event != NULL &&
      mt->poly_state == POLY_PARAMS && weed_plant_has_leaf(mt->init_event, WEED_LEAF_IN_TRACKS)) {
    weed_timecode_t tc = q_gint64(lives_spin_button_get_value(LIVES_SPIN_BUTTON(mt->node_spinbutton)) * TICKS_PER_SECOND_DBL +
                                  get_event_timecode(mt->init_event), mt->fps);
    get_track_index(mt, tc);
  }

  // give the new block the same uid as the old one
  if (block != NULL) block->uid = uid;

  if (!did_backup) {
    mt->did_backup = FALSE;
    if (needs_idlefunc || (!did_backup && mt->auto_changed)) mt->idlefunc = mt_idle_add(mt);
    if (prefs->mt_auto_back == 0) mt_auto_backup(mt);
  }

  return block;
}


void unselect_all(lives_mt * mt) {
  // unselect all blocks
  LiVESWidget *eventbox;
  LiVESList *list;
  track_rect *trec;

  if (mt->block_selected != NULL) lives_widget_queue_draw(mt->block_selected->eventbox);

  if (cfile->achans > 0) {
    for (list = mt->audio_draws; list != NULL; list = list->next) {
      eventbox = (LiVESWidget *)list->data;
      if (eventbox != NULL) {
        trec = (track_rect *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "blocks");
        while (trec != NULL) {
          trec->state = BLOCK_UNSELECTED;
          trec = trec->next;
	  // *INDENT-OFF*
        }}}}
  // *INDENT-ON*

  for (list = mt->video_draws; list != NULL; list = list->next) {
    eventbox = (LiVESWidget *)list->data;
    if (eventbox != NULL) {
      trec = (track_rect *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "blocks");
      while (trec != NULL) {
        trec->state = BLOCK_UNSELECTED;
        trec = trec->next;
      }
    }
  }
  mt->block_selected = NULL;
  lives_widget_set_sensitive(mt->view_in_out, FALSE);
  lives_widget_set_sensitive(mt->delblock, FALSE);

  lives_widget_set_sensitive(mt->fx_block, FALSE);
  lives_widget_set_sensitive(mt->fx_blocka, FALSE);
  lives_widget_set_sensitive(mt->fx_blockv, FALSE);
  if (!nb_ignore && mt->poly_state != POLY_FX_STACK) polymorph(mt, POLY_CLIPS);
}


void clear_context(lives_mt * mt) {
  if (mt->context_scroll != NULL) {
    lives_widget_destroy(mt->context_scroll);
  }

  mt->context_scroll = lives_scrolled_window_new(NULL, NULL);
  lives_widget_set_hexpand(mt->context_scroll, TRUE);

  lives_container_add(LIVES_CONTAINER(mt->context_frame), mt->context_scroll);

  lives_scrolled_window_set_policy(LIVES_SCROLLED_WINDOW(mt->context_scroll), LIVES_POLICY_NEVER,
                                   LIVES_POLICY_AUTOMATIC);

  mt->context_box = lives_vbox_new(FALSE, 4);
  lives_widget_set_hexpand(mt->context_scroll, TRUE);
  lives_scrolled_window_add_with_viewport(LIVES_SCROLLED_WINDOW(mt->context_scroll), mt->context_box);

  // Apply theme background to scrolled window
  if (palette->style & STYLE_1) {
    lives_widget_set_fg_color(lives_bin_get_child(LIVES_BIN(mt->context_scroll)), LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
    lives_widget_set_bg_color(lives_bin_get_child(LIVES_BIN(mt->context_scroll)), LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
  }

  add_context_label(mt, ("                                          ")); // info box stop from shrinking
  if (mt->opts.show_ctx) lives_widget_show_all(mt->context_frame);
}


void add_context_label(lives_mt * mt, const char *text) {
  // WARNING - do not add > 8 lines of text (including newlines) - otherwise the window can get resized

  LiVESWidget *label;

  widget_opts.justify = LIVES_JUSTIFY_CENTER;
  widget_opts.line_wrap = TRUE;
  label = lives_standard_label_new(NULL);
  lives_label_set_markup(LIVES_LABEL(label), text);
  widget_opts.line_wrap = FALSE;
  widget_opts.justify = LIVES_JUSTIFY_DEFAULT;

  lives_widget_show(label);
  lives_box_pack_start(LIVES_BOX(mt->context_box), label, FALSE, FALSE, 0);

  if (palette->style & STYLE_1) {
    lives_widget_set_bg_color(label, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
    lives_widget_set_fg_color(label, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
  }
}


boolean resize_timeline(lives_mt * mt) {
  double end_secs;

  if (mt->event_list == NULL || get_first_event(mt->event_list) == NULL || mt->tl_fixed_length > 0.) return FALSE;

  end_secs = event_list_get_end_secs(mt->event_list);

  if (end_secs > mt->end_secs) {
    set_timeline_end_secs(mt, end_secs);
    return TRUE;
  }

  redraw_all_event_boxes(mt);

  return FALSE;
}


static void set_in_out_spin_ranges(lives_mt * mt, weed_timecode_t start_tc, weed_timecode_t end_tc) {
  track_rect *block = mt->block_selected;
  weed_timecode_t min_tc = 0, max_tc = -1;
  weed_timecode_t offset_start = get_event_timecode(block->start_event);
  int filenum;
  double in_val = start_tc / TICKS_PER_SECOND_DBL, out_val = end_tc / TICKS_PER_SECOND_DBL, in_start_range = 0.,
         out_start_range = in_val + 1. / mt->fps;
  double out_end_range, real_out_end_range;
  double in_end_range = out_val - 1. / mt->fps, real_in_start_range = in_start_range;
  double avel = 1.;

  int track = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(block->eventbox), "layer_number"));

  lives_signal_handler_block(mt->spinbutton_out, mt->spin_out_func);
  lives_signal_handler_block(mt->spinbutton_in, mt->spin_in_func);

  if (block->prev != NULL) min_tc = get_event_timecode(block->prev->end_event) + (double)(track >= 0) * TICKS_PER_SECOND_DBL /
                                      mt->fps;
  if (block->next != NULL) max_tc = get_event_timecode(block->next->start_event) - (double)(
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


static void update_in_image(lives_mt * mt) {
  LiVESPixbuf *thumb;
  track_rect *block = mt->block_selected;
  int track;
  int filenum;
  int frame_start;
  int width = cfile->hsize;
  int height = cfile->vsize;

  if (block != NULL) {
    track = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(block->eventbox), "layer_number"));
    filenum = get_frame_event_clip(block->start_event, track);
    frame_start = calc_frame_from_time(filenum, block->offset_start / TICKS_PER_SECOND_DBL);
  } else {
    filenum = mt->file_selected;
    frame_start = mainw->files[filenum]->start;
  }

  calc_maxspect(lives_widget_get_allocation_width(mt->in_frame), lives_widget_get_allocation_height(mt->in_frame), &width,
                &height);

  thumb = make_thumb(mt, filenum, width, height, frame_start, LIVES_INTERP_NORMAL, FALSE);
  lives_image_set_from_pixbuf(LIVES_IMAGE(mt->in_image), thumb);
  if (thumb != NULL) lives_widget_object_unref(thumb);
}


static void update_out_image(lives_mt * mt, weed_timecode_t end_tc) {
  LiVESPixbuf *thumb;
  track_rect *block = mt->block_selected;
  int track;
  int filenum;
  int frame_end;
  int width = cfile->hsize;
  int height = cfile->vsize;

  if (block != NULL) {
    track = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(block->eventbox), "layer_number"));
    filenum = get_frame_event_clip(block->start_event, track);
    frame_end = calc_frame_from_time(filenum, end_tc / TICKS_PER_SECOND_DBL - 1. / mt->fps);
  } else {
    filenum = mt->file_selected;
    frame_end = mainw->files[filenum]->end;
  }

  calc_maxspect(lives_widget_get_allocation_width(mt->out_frame), lives_widget_get_allocation_height(mt->out_frame), &width,
                &height);

  thumb = make_thumb(mt, filenum, width, height, frame_end, LIVES_INTERP_NORMAL, FALSE);
  lives_image_set_from_pixbuf(LIVES_IMAGE(mt->out_image), thumb);
  if (thumb != NULL) lives_widget_object_unref(thumb);
}


static boolean show_in_out_images(livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  track_rect *block = mt->block_selected;
  weed_timecode_t end_tc;
  int track;
  if (block == NULL) return FALSE;
  track = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(block->eventbox), "layer_number"));
  if (track < 0) return FALSE;
  end_tc = block->offset_start + (weed_timecode_t)(TICKS_PER_SECOND_DBL / mt->fps) + (get_event_timecode(
             block->end_event) - get_event_timecode(block->start_event));
  update_in_image(mt);
  update_out_image(mt, end_tc);
  return FALSE;
}


void in_out_start_changed(LiVESWidget * widget, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;

  track_rect *block = mt->block_selected, *ablock = NULL;

  weed_plant_t *event;
  weed_plant_t *start_event = NULL, *event_next;

  weed_timecode_t new_start_tc, orig_start_tc, offset_end, tl_start;
  weed_timecode_t new_tl_tc;

  double new_start;
  double avel = 1., aseek = 0.;

  boolean was_moved;
  boolean start_anchored;
  boolean needs_idlefunc = FALSE;
  boolean did_backup = mt->did_backup;

  int track;
  int filenum;
  int aclip = 0;

  if (!mainw->interactive) return;

  if (mt->idlefunc > 0) {
    needs_idlefunc = TRUE;
    lives_source_remove(mt->idlefunc);
    mt->idlefunc = 0;
  }

  if (block == NULL) {
    // if no block selected, set for current clip
    lives_clip_t *sfile = mainw->files[mt->file_selected];
    sfile->start = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(widget));
    set_clip_labels_variable(mt, mt->file_selected);
    update_in_image(mt);

    if (sfile->end < sfile->start) {
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(mt->spinbutton_out), (double)sfile->start);
    }
    if (needs_idlefunc || (!did_backup && mt->auto_changed)) {
      mt->idlefunc = mt_idle_add(mt);
    }
    return;
  }

  new_start = lives_spin_button_get_value(LIVES_SPIN_BUTTON(widget));

  event = block->start_event;
  orig_start_tc = block->offset_start;
  track = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(block->eventbox), "layer_number"));
  new_start_tc = q_dbl(new_start, mt->fps);

  if (new_start_tc == orig_start_tc || !block->ordered) {
    lives_spin_button_set_value(LIVES_SPIN_BUTTON(mt->spinbutton_in), new_start_tc / TICKS_PER_SECOND_DBL);
    if (needs_idlefunc || (!did_backup && mt->auto_changed)) {
      mt->idlefunc = mt_idle_add(mt);
    }
    return;
  }

  tl_start = get_event_timecode(event);

  // get the audio block (if exists)
  if (track >= 0) {
    if (!mt->aud_track_selected) {
      if (mt->opts.pertrack_audio) {
        LiVESWidget *aeventbox = LIVES_WIDGET(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(block->eventbox), "atrack"));
        ablock = get_block_from_time(aeventbox, tl_start / TICKS_PER_SECOND_DBL, mt);
      }
      start_anchored = block->start_anchored;
    } else {
      LiVESWidget *eventbox = LIVES_WIDGET(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(block->eventbox), "owner"));
      ablock = block;
      block = get_block_from_time(eventbox, tl_start / TICKS_PER_SECOND_DBL, mt);
      start_anchored = ablock->start_anchored;
    }
    filenum = get_frame_event_clip(block->start_event, track);
  } else {
    ablock = block;
    start_anchored = block->start_anchored;
    avel = get_audio_frame_vel(ablock->start_event, track);
    filenum = get_audio_frame_clip(ablock->start_event, track);
  }

  if (!start_anchored) {
    if (new_start_tc > block->offset_start) {
      start_event = get_prev_frame_event(block->start_event);

      // start increased, not anchored
      while (event != NULL) {
        if ((get_event_timecode(event) - tl_start) >= (new_start_tc - block->offset_start) / avel) {
          //if tc of event - tc of block start event > new start tc (in source file) - offset tc (in source file)

          // done
          if (ablock != NULL) {
            aclip = get_audio_frame_clip(ablock->start_event, track);
            aseek = get_audio_frame_seek(ablock->start_event, track);

            if (ablock->prev != NULL && ablock->prev->end_event == ablock->start_event) {
              int last_aclip = get_audio_frame_clip(ablock->prev->start_event, track);
              insert_audio_event_at(ablock->start_event, track, last_aclip, 0., 0.);
            } else {
              remove_audio_for_track(ablock->start_event, track);
            }
            aseek += q_gint64(avel * (double)(get_event_timecode(event) - get_event_timecode(ablock->start_event)),
                              mt->fps) / TICKS_PER_SECOND_DBL;
            ablock->start_event = event;
            ablock->offset_start = new_start_tc;
          }
          if (block != ablock) {
            block->start_event = event;
            block->offset_start = new_start_tc;
          }
          break;
        }

        if (event == block->end_event) {
          if (needs_idlefunc || (!did_backup && mt->auto_changed)) {
            mt->idlefunc = mt_idle_add(mt);
          }
          return; // should never happen...
        }
        if (track >= 0) remove_frame_from_event(mt->event_list, event, track);
        event = get_next_frame_event(event);
      }

      if (ablock != NULL) {
        insert_audio_event_at(ablock->start_event, track, aclip, aseek, avel);
        ablock->offset_start = q_dbl(aseek * TICKS_PER_SECOND_DBL, mt->fps) / TICKS_PER_SECOND_DBL;
      }

      // move filter_inits right, and deinits left
      new_tl_tc = get_event_timecode(block->start_event);
      if (start_event == NULL) event = get_first_event(mt->event_list);
      else event = get_next_event(start_event);
      while (event != NULL && get_event_timecode(event) < tl_start) event = get_next_event(event);

      while (event != NULL && get_event_timecode(event) < new_tl_tc) {
        event_next = get_next_event(event);
        was_moved = FALSE;
        if (WEED_EVENT_IS_FILTER_INIT(event) && event != mt->avol_init_event) {
          if (!move_event_right(mt->event_list, event, TRUE, mt->fps)) {
            was_moved = TRUE;
            if (event == start_event) start_event = NULL;
          }
        } else {
          if (WEED_EVENT_IS_FILTER_DEINIT(event)) {
            if (!move_event_left(mt->event_list, event, TRUE, mt->fps)) {
              was_moved = TRUE;
            }
          }
        }
        if (was_moved) {
          if (start_event == NULL) event = get_first_event(mt->event_list);
          else event = get_next_event(start_event);
          while (event != NULL && get_event_timecode(event) < tl_start) event = get_next_event(event);
        } else {
          event = event_next;
          if (WEED_EVENT_IS_FRAME(event)) start_event = event;
        }
      }

    } else {
      // move start left, not anchored
      if (ablock != NULL) {
        aclip = get_audio_frame_clip(ablock->start_event, track);
        aseek = get_audio_frame_seek(ablock->start_event, track);

        remove_audio_for_track(ablock->start_event, track);

        aseek += q_gint64(new_start_tc - ablock->offset_start, mt->fps) / TICKS_PER_SECOND_DBL;
        ablock->start_event = get_frame_event_at_or_before(mt->event_list,
                              q_gint64(tl_start + (new_start_tc - ablock->offset_start) / avel, mt->fps),
                              get_prev_frame_event(ablock->start_event));

        insert_audio_event_at(ablock->start_event, track, aclip, aseek, avel);
        ablock->offset_start = q_dbl(aseek * TICKS_PER_SECOND_DBL, mt->fps) / TICKS_PER_SECOND_DBL;
      }
      if (block != ablock) {
        // do an insert from offset_start down
        insert_frames(filenum, block->offset_start, new_start_tc, tl_start, LIVES_DIRECTION_BACKWARD, block->eventbox, mt, block);
        block->offset_start = new_start_tc;
      }

      // any filter_inits with this track as owner get moved as far left as possible
      new_tl_tc = get_event_timecode(block->start_event);
      event = block->start_event;

      while (event != NULL && get_event_timecode(event) < tl_start) {
        start_event = event;
        event = get_next_event(event);
      }
      while (event != NULL && get_event_timecode(event) == tl_start) {
        if (WEED_EVENT_IS_FILTER_INIT(event) && event != mt->avol_init_event) {
          if (filter_init_has_owner(event, track)) {
            // candidate for moving
            move_filter_init_event(mt->event_list, new_tl_tc, event, mt->fps);
            // account for overlaps
            move_event_right(mt->event_list, event, TRUE, mt->fps);
            event = start_event;
            while (event != NULL && get_event_timecode(event) < tl_start) event = get_next_event(event);
            continue;
          }
        }
        event = get_next_event(event);
      }
    }
  } else {
    // start is anchored, do a re-insert from start to end
    lives_mt_insert_mode_t insert_mode = mt->opts.insert_mode;
    offset_end = q_gint64((block->offset_start = new_start_tc) + (weed_timecode_t)(TICKS_PER_SECOND_DBL / mt->fps) +
                          (get_event_timecode(block->end_event) - get_event_timecode(block->start_event)), mt->fps);
    mt->opts.insert_mode = INSERT_MODE_OVERWRITE;
    if (track >= 0) insert_frames(filenum, new_start_tc, offset_end, tl_start, LIVES_DIRECTION_FORWARD, block->eventbox, mt, block);
    if (ablock != NULL) {
      aclip = get_audio_frame_clip(ablock->start_event, track);
      aseek = get_audio_frame_seek(ablock->start_event, track);
      aseek += q_gint64(new_start_tc - orig_start_tc, mt->fps) / TICKS_PER_SECOND_DBL;
      insert_audio_event_at(ablock->start_event, track, aclip, aseek, avel);
      ablock->offset_start = q_gint64(aseek * TICKS_PER_SECOND_DBL, mt->fps);
    }
    mt->opts.insert_mode = insert_mode;
  }

  offset_end = (new_start_tc = block->offset_start) + (weed_timecode_t)((double)(track >= 0) * TICKS_PER_SECOND_DBL / mt->fps) +
               avel * (get_event_timecode(block->end_event) - get_event_timecode(block->start_event));

  if (mt->poly_state == POLY_IN_OUT) {
    set_in_out_spin_ranges(mt, new_start_tc, offset_end);
    lives_signal_handler_block(mt->spinbutton_out, mt->spin_out_func);
    lives_signal_handler_block(mt->spinbutton_in, mt->spin_in_func);
    lives_spin_button_set_value(LIVES_SPIN_BUTTON(mt->spinbutton_out), offset_end / TICKS_PER_SECOND_DBL);
    lives_spin_button_set_value(LIVES_SPIN_BUTTON(mt->spinbutton_in), new_start_tc / TICKS_PER_SECOND_DBL);
    lives_spin_button_update(LIVES_SPIN_BUTTON(mt->spinbutton_out));
    lives_spin_button_update(LIVES_SPIN_BUTTON(mt->spinbutton_in));
    lives_signal_handler_unblock(mt->spinbutton_out, mt->spin_out_func);
    lives_signal_handler_unblock(mt->spinbutton_in, mt->spin_in_func);

    if (track >= 0) {
      // update images
      update_in_image(mt);
      if (start_anchored) update_out_image(mt, offset_end);
      update_in_image(mt);
      if (start_anchored) update_out_image(mt, offset_end);
    }
  }

  if (!resize_timeline(mt)) {
    redraw_eventbox(mt, block->eventbox);
    paint_lines(mt, mt->ptr_time, TRUE);
  }

  if (prefs->mt_auto_back >= 0) {
    mt->auto_changed = TRUE;
    if (prefs->mt_auto_back < MT_INOUT_TIME) {
      // the user wants us to backup, but it would be tiresome to backup on every spin button change
      // so instead of adding the idle function we will add a timer
      //
      // if the spinbutton is changed again we we reset the timer
      // after any backup we put the normal idlefunc back again
      mt->idlefunc = lives_timer_add(MT_INOUT_TIME, mt_auto_backup, mt);
    } else {
      mt->idlefunc = mt_idle_add(mt);
    }
  }
}


void in_out_end_changed(LiVESWidget * widget, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;

  track_rect *block = mt->block_selected, *ablock = NULL;

  weed_timecode_t offset_end, orig_end_tc;
  weed_timecode_t new_end_tc, tl_end;
  weed_timecode_t new_tl_tc;

  weed_plant_t *event, *prevevent, *shortcut = NULL;
  weed_plant_t *start_event, *event_next, *init_event, *new_end_event;

  double new_end = lives_spin_button_get_value(LIVES_SPIN_BUTTON(widget));
  double start_val;
  double aseek, avel = 1.;

  boolean was_moved;
  boolean end_anchored;
  boolean needs_idlefunc = FALSE;
  boolean did_backup = mt->did_backup;

  int track;
  int filenum;
  int aclip = 0;

  if (!mainw->interactive) return;

  if (mt->idlefunc > 0) {
    needs_idlefunc = TRUE;
    lives_source_remove(mt->idlefunc);
    mt->idlefunc = 0;
  }

  if (block == NULL) {
    lives_clip_t *sfile = mainw->files[mt->file_selected];
    sfile->end = (int)new_end;
    set_clip_labels_variable(mt, mt->file_selected);
    update_out_image(mt, 0);

    if (sfile->end < sfile->start) lives_spin_button_set_value(LIVES_SPIN_BUTTON(mt->spinbutton_in), (double)sfile->end);
    if (needs_idlefunc || (!did_backup && mt->auto_changed)) {
      mt->idlefunc = mt_idle_add(mt);
    }
    return;
  }

  start_val = block->offset_start / TICKS_PER_SECOND_DBL;
  event = block->end_event;
  track = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(block->eventbox), "layer_number"));

  tl_end = get_event_timecode(event);

  // get the audio block (if exists)
  if (track >= 0) {
    if (!mt->aud_track_selected) {
      if (mt->opts.pertrack_audio) {
        LiVESWidget *aeventbox = LIVES_WIDGET(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(block->eventbox), "atrack"));
        ablock = get_block_from_time(aeventbox, tl_end / TICKS_PER_SECOND_DBL - 1. / mt->fps, mt);
      }
      end_anchored = block->end_anchored;
    } else {
      LiVESWidget *eventbox = LIVES_WIDGET(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(block->eventbox), "owner"));
      ablock = block;
      block = get_block_from_time(eventbox, tl_end / TICKS_PER_SECOND_DBL - 1. / mt->fps, mt);
      end_anchored = ablock->end_anchored;
    }
    filenum = get_frame_event_clip(block->start_event, track);
  } else {
    ablock = block;
    end_anchored = block->end_anchored;
    avel = get_audio_frame_vel(ablock->start_event, track);
    filenum = get_audio_frame_clip(ablock->start_event, track);
  }

  // offset_end is timecode of end event within source (scaled for velocity)
  offset_end = q_gint64(block->offset_start + (weed_timecode_t)((double)(track >= 0) * TICKS_PER_SECOND_DBL / mt->fps) +
                        (weed_timecode_t)((double)(get_event_timecode(block->end_event) -
                                          get_event_timecode(block->start_event)) * avel), mt->fps);

  if (track >= 0 &&
      new_end > mainw->files[filenum]->frames / mainw->files[filenum]->fps) new_end = mainw->files[filenum]->frames /
            mainw->files[filenum]->fps;

  new_end_tc = q_gint64(block->offset_start + (new_end - start_val) * TICKS_PER_SECOND_DBL, mt->fps);
  orig_end_tc = offset_end;

#ifdef DEBUG_BL_MOVE
  g_print("pt a %ld %ld %ld %.4f %ld %ld\n", block->offset_start, get_event_timecode(block->end_event),
          get_event_timecode(block->start_event), new_end, orig_end_tc, new_end_tc);
#endif

  if (ABS(new_end_tc - orig_end_tc) < (.5 * TICKS_PER_SECOND_DBL) / mt->fps || !block->ordered) {
    lives_signal_handler_block(mt->spinbutton_out, mt->spin_out_func);
    lives_spin_button_set_value(LIVES_SPIN_BUTTON(mt->spinbutton_out), new_end_tc / TICKS_PER_SECOND_DBL);
    lives_signal_handler_unblock(mt->spinbutton_out, mt->spin_out_func);
    if (needs_idlefunc || (!did_backup && mt->auto_changed)) {
      mt->idlefunc = mt_idle_add(mt);
    }
    return;
  }

  start_event = get_prev_frame_event(event);

  if (!end_anchored) {
    new_tl_tc = q_gint64(get_event_timecode(block->start_event) + (new_end - start_val) * TICKS_PER_SECOND_DBL / avel -
                         (double)(track >= 0) * TICKS_PER_SECOND_DBL / mt->fps, mt->fps);
    if (track < 0) new_tl_tc -= (TICKS_PER_SECOND_DBL / mt->fps);

#ifdef DEBUG_BL_MOVE
    g_print("new tl tc is %ld %ld %.4f %.4f\n", new_tl_tc, tl_end, new_end, start_val);
#endif
    if (tl_end > new_tl_tc) {
      // end decreased, not anchored

      while (event != NULL) {
        if (get_event_timecode(event) <= new_tl_tc) {
          // done
          if (ablock != NULL) {
            if (ablock->next == NULL || ablock->next->start_event != ablock->end_event)
              remove_audio_for_track(ablock->end_event, track);
            aclip = get_audio_frame_clip(ablock->start_event, track);
          }
          block->end_event = event;
          break;
        }
        prevevent = get_prev_frame_event(event);
        if (track >= 0) remove_frame_from_event(mt->event_list, event, track);
        event = prevevent;
      }

      if (ablock != NULL) {
        new_end_event = get_next_frame_event(event);
        if (new_end_event == NULL) {
          weed_plant_t *shortcut = ablock->end_event;
          mt->event_list = insert_blank_frame_event_at(mt->event_list,
                           q_gint64(new_tl_tc + (weed_timecode_t)((double)(track >= 0) *
                                    TICKS_PER_SECOND_DBL / mt->fps), mt->fps),
                           &shortcut);
          ablock->end_event = shortcut;
        } else ablock->end_event = new_end_event;
        insert_audio_event_at(ablock->end_event, track, aclip, 0., 0.);
      }

      // move filter_inits right, and deinits left
      new_tl_tc = get_event_timecode(block->end_event);
      start_event = block->end_event;
      while (event != NULL && get_event_timecode(event) <= new_tl_tc) event = get_next_event(event);

      while (event != NULL && get_event_timecode(event) <= tl_end) {
        event_next = get_next_event(event);
        was_moved = FALSE;
        if (WEED_EVENT_IS_FILTER_INIT(event)) {
          if (!move_event_right(mt->event_list, event, TRUE, mt->fps)) {
            was_moved = TRUE;
            if (event == start_event) start_event = NULL;
          }
        } else {
          if (WEED_EVENT_IS_FILTER_DEINIT(event)) {
            init_event = (weed_plant_t *)weed_get_voidptr_value(event, WEED_LEAF_INIT_EVENT, NULL);
            if (init_event != mt->avol_init_event) {
              if (!move_event_left(mt->event_list, event, TRUE, mt->fps)) {
                was_moved = TRUE;
              }
            }
          }
        }
        if (was_moved) {
          if (start_event == NULL) event = get_first_event(mt->event_list);
          else event = get_next_event(start_event);
          while (event != NULL && get_event_timecode(event) <= new_tl_tc) event = get_next_event(event);
        } else {
          event = event_next;
          if (WEED_EVENT_IS_FRAME(event)) start_event = event;
        }
      }
      remove_end_blank_frames(mt->event_list, TRUE);
    } else {
      // end increased, not anchored
      if (track >= 0) {
        // do an insert from end_tc up, starting with end_frame and finishing at new_end
        insert_frames(filenum, offset_end, new_end_tc, tl_end + (weed_timecode_t)(TICKS_PER_SECOND_DBL / mt->fps),
                      LIVES_DIRECTION_FORWARD,
                      block->eventbox, mt, block);
        block->end_event = get_frame_event_at(mt->event_list, q_gint64(new_end_tc + tl_end - offset_end, mt->fps),
                                              block->end_event, TRUE);
      } else {
        // for backing audio insert blank frames up to end
        weed_plant_t *last_frame_event = get_last_frame_event(mt->event_list);
        weed_timecode_t final_tc = get_event_timecode(last_frame_event);
        shortcut = last_frame_event;
        while (final_tc < new_tl_tc) {
          final_tc = q_gint64(final_tc + TICKS_PER_SECOND_DBL / mt->fps, mt->fps);
          mt->event_list = insert_blank_frame_event_at(mt->event_list, final_tc, &shortcut);
        }
      }
      if (ablock != NULL) {
        new_end_event = get_frame_event_at(mt->event_list, q_gint64(new_tl_tc + TICKS_PER_SECOND_DBL / mt->fps, mt->fps),
                                           ablock->end_event, TRUE);
        if (new_end_event == ablock->end_event) {
          lives_signal_handler_block(mt->spinbutton_out, mt->spin_out_func);
          lives_spin_button_set_value(LIVES_SPIN_BUTTON(mt->spinbutton_out), orig_end_tc / TICKS_PER_SECOND_DBL);
          lives_signal_handler_unblock(mt->spinbutton_out, mt->spin_out_func);
          return;
        }
        remove_audio_for_track(ablock->end_event, track);
        if (new_end_event == NULL) {
          if (shortcut == NULL) shortcut = ablock->end_event;
          mt->event_list = insert_blank_frame_event_at(mt->event_list, q_gint64(new_tl_tc + TICKS_PER_SECOND_DBL / mt->fps, mt->fps),
                           &shortcut);
          ablock->end_event = shortcut;
        } else ablock->end_event = new_end_event;

        if (ablock->next == NULL || ablock->next->start_event != ablock->end_event) {
          aclip = get_audio_frame_clip(ablock->start_event, track);
          insert_audio_event_at(ablock->end_event, track, aclip, 0., 0.);
        }
      }

      new_tl_tc = get_event_timecode(block->end_event);

      start_event = event;

      while (event != NULL && get_event_timecode(event) == tl_end) {
        if (WEED_EVENT_IS_FILTER_DEINIT(event)) {
          init_event = (weed_plant_t *)weed_get_voidptr_value(event, WEED_LEAF_INIT_EVENT, NULL);
          if (init_event != mt->avol_init_event) {
            if (filter_init_has_owner(init_event, track)) {
              // candidate for moving
              move_filter_deinit_event(mt->event_list, new_tl_tc, event, mt->fps, TRUE);
              // account for overlaps
              //move_event_left(mt->event_list,event,TRUE,mt->fps);
              event = start_event;
              continue;
            }
          }
        }
        event = get_next_event(event);
      }
    }
  } else {
    // end is anchored, do a re-insert from end to start
    weed_timecode_t offset_start;
    lives_mt_insert_mode_t insert_mode = mt->opts.insert_mode;

    offset_end = q_gint64((offset_start = block->offset_start + new_end_tc - orig_end_tc) +
                          (weed_timecode_t)((double)(track >= 0) * TICKS_PER_SECOND_DBL / mt->fps) +
                          (get_event_timecode(block->end_event) - get_event_timecode(block->start_event)), mt->fps);

    mt->opts.insert_mode = INSERT_MODE_OVERWRITE;

    // note: audio blocks end at the timecode, video blocks end at tc + TICKS_PER_SECOND_DBL/mt->fps
    if (track >= 0) insert_frames(filenum, offset_end, offset_start, tl_end +
                                    (weed_timecode_t)((double)(track >= 0 && !mt->aud_track_selected)*TICKS_PER_SECOND_DBL / mt->fps),
                                    LIVES_DIRECTION_BACKWARD, block->eventbox, mt, block);

    block->offset_start = q_gint64(offset_start, mt->fps);

    if (ablock != NULL) {
      aclip = get_audio_frame_clip(ablock->start_event, track);
      aseek = get_audio_frame_seek(ablock->start_event, track);
      avel = get_audio_frame_vel(ablock->start_event, track);
      aseek += q_gint64(new_end_tc - orig_end_tc, mt->fps) / TICKS_PER_SECOND_DBL;
      insert_audio_event_at(ablock->start_event, track, aclip, aseek, avel);
      ablock->offset_start = q_gint64(aseek * TICKS_PER_SECOND_DBL, mt->fps);
    }

    mt->opts.insert_mode = insert_mode;
  }

  // get new offset_end
  new_end_tc = (block->offset_start + (weed_timecode_t)((double)(track >= 0) * TICKS_PER_SECOND_DBL / mt->fps) +
                (get_event_timecode(block->end_event) - get_event_timecode(block->start_event)) * avel);

#ifdef DEBUG_BL_MOVE
  g_print("new end tc is %ld %ld %ld %.4f\n", get_event_timecode(block->end_event),
          get_event_timecode(block->start_event), block->offset_start, avel);
#endif

  if (mt->avol_fx != -1 && mt->avol_init_event != NULL && mt->audio_draws != NULL &&
      mt->audio_draws->data != NULL && block->next == NULL) {
    apply_avol_filter(mt);
  }

  if (mt->poly_state == POLY_IN_OUT) {
    set_in_out_spin_ranges(mt, block->offset_start, new_end_tc);
    lives_signal_handler_block(mt->spinbutton_out, mt->spin_out_func);
    lives_signal_handler_block(mt->spinbutton_in, mt->spin_in_func);
    lives_spin_button_set_value(LIVES_SPIN_BUTTON(mt->spinbutton_in), block->offset_start / TICKS_PER_SECOND_DBL);
    lives_spin_button_set_value(LIVES_SPIN_BUTTON(mt->spinbutton_out), new_end_tc / TICKS_PER_SECOND_DBL);
    lives_signal_handler_unblock(mt->spinbutton_out, mt->spin_out_func);
    lives_signal_handler_unblock(mt->spinbutton_in, mt->spin_in_func);

    if (track >= 0) {
      // update image
      update_out_image(mt, new_end_tc);
      if (end_anchored) update_in_image(mt);
    }
  }
#ifdef DEBUG_BL_MOVE
  g_print("pt b %ld\n", q_gint64(new_end_tc / avel, mt->fps));
#endif
  if (!resize_timeline(mt)) {
    redraw_eventbox(mt, block->eventbox);
    if (ablock != NULL && ablock != block) redraw_eventbox(mt, block->eventbox);
    paint_lines(mt, mt->ptr_time, TRUE);
    // TODO - redraw chans ??
  }
  if (prefs->mt_auto_back >= 0) {
    mt->auto_changed = TRUE;
    if (prefs->mt_auto_back < MT_INOUT_TIME) {
      // the user wants us to backup, but it would be tiresome to backup on every spin button change
      // so instead of adding the idle function we will add a timer
      //
      // if the spinbutton is changed again we we reset the timer
      // after any backup we put the normal idlefunc back again
      mt->idlefunc = lives_timer_add(MT_INOUT_TIME, mt_auto_backup, mt);
    } else {
      mt->idlefunc = mt_idle_add(mt);
    }
  }
}


void avel_reverse_toggled(LiVESToggleButton * togglebutton, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  track_rect *block = mt->block_selected;
  int track = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(block->eventbox), "layer_number"));
  double avel = -get_audio_frame_vel(block->start_event, track);
  double aseek = get_audio_frame_seek(block->start_event, track), aseek_end;
  int aclip = get_audio_frame_clip(block->start_event, track);

  double old_in_val = lives_spin_button_get_value(LIVES_SPIN_BUTTON(mt->spinbutton_in));
  double old_out_val = lives_spin_button_get_value(LIVES_SPIN_BUTTON(mt->spinbutton_out));

  // update avel and aseek
  aseek_end = aseek + (get_event_timecode(block->end_event) - get_event_timecode(block->start_event)) / TICKS_PER_SECOND_DBL *
              (-avel);
  insert_audio_event_at(block->start_event, track, aclip, aseek_end, avel);

  if (avel < 0.) set_in_out_spin_ranges(mt, old_in_val * TICKS_PER_SECOND_DBL, old_out_val * TICKS_PER_SECOND_DBL);
  else set_in_out_spin_ranges(mt, old_out_val * TICKS_PER_SECOND_DBL, old_in_val * TICKS_PER_SECOND_DBL);

  lives_signal_handler_block(mt->spinbutton_out, mt->spin_out_func);
  lives_signal_handler_block(mt->spinbutton_in, mt->spin_in_func);

  lives_spin_button_set_value(LIVES_SPIN_BUTTON(mt->spinbutton_in), old_out_val);
  lives_spin_button_set_value(LIVES_SPIN_BUTTON(mt->spinbutton_out), old_in_val);

  lives_signal_handler_unblock(mt->spinbutton_in, mt->spin_in_func);
  lives_signal_handler_unblock(mt->spinbutton_out, mt->spin_out_func);

  if (avel < 0.) {
    lives_widget_set_sensitive(mt->spinbutton_in, FALSE);
    lives_widget_set_sensitive(mt->spinbutton_out, FALSE);
    lives_widget_set_sensitive(mt->spinbutton_avel, FALSE);
    lives_widget_set_sensitive(mt->avel_scale, FALSE);
    lives_widget_set_sensitive(mt->checkbutton_start_anchored, FALSE);
    lives_widget_set_sensitive(mt->checkbutton_end_anchored, FALSE);
  } else {
    lives_widget_set_sensitive(mt->spinbutton_in, TRUE);
    lives_widget_set_sensitive(mt->spinbutton_out, TRUE);
    if (!block->start_anchored || !block->end_anchored) {
      lives_widget_set_sensitive(mt->spinbutton_avel, TRUE);
      lives_widget_set_sensitive(mt->avel_scale, TRUE);
    }
    lives_widget_set_sensitive(mt->checkbutton_start_anchored, TRUE);
    lives_widget_set_sensitive(mt->checkbutton_end_anchored, TRUE);
  }
}


void avel_spin_changed(LiVESSpinButton * spinbutton, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  track_rect *block = mt->block_selected;
  int track = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(block->eventbox), "layer_number"));
  double new_avel = lives_spin_button_get_value(spinbutton);
  double aseek = get_audio_frame_seek(block->start_event, track);
  int aclip = get_audio_frame_clip(block->start_event, track);
  weed_timecode_t new_end_tc, old_tl_tc, start_tc, new_tl_tc, min_tc;
  weed_plant_t *new_end_event, *new_start_event;
  double orig_end_val, orig_start_val;
  boolean was_adjusted = FALSE;

  if (!mainw->interactive) return;

  if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(mt->checkbutton_avel_reverse))) new_avel = -new_avel;

  start_tc = block->offset_start;

  orig_end_val = lives_spin_button_get_value(LIVES_SPIN_BUTTON(mt->spinbutton_out));
  old_tl_tc = get_event_timecode(block->end_event);

  if (!block->end_anchored) {
    new_end_tc = q_gint64(start_tc + ((orig_end_val =
                                         lives_spin_button_get_value(LIVES_SPIN_BUTTON(mt->spinbutton_out))) * TICKS_PER_SECOND_DBL - start_tc)
                          / new_avel, mt->fps);

    insert_audio_event_at(block->start_event, track, aclip, aseek, new_avel);

    new_tl_tc = q_gint64(get_event_timecode(block->start_event) + (orig_end_val * TICKS_PER_SECOND_DBL - start_tc) / new_avel,
                         mt->fps);

    // move end point (if we can)
    if (block->next != NULL && new_tl_tc >= get_event_timecode(block->next->start_event)) {
      new_end_tc = q_gint64((get_event_timecode(block->next->start_event) -
                             get_event_timecode(block->start_event)) * new_avel + block->offset_start, mt->fps);
      set_in_out_spin_ranges(mt, block->offset_start, new_end_tc);
      lives_signal_handler_block(mt->spinbutton_out, mt->spin_out_func);
      lives_signal_handler_block(mt->spinbutton_in, mt->spin_in_func);
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(mt->spinbutton_out), new_end_tc / TICKS_PER_SECOND_DBL);
      lives_signal_handler_unblock(mt->spinbutton_in, mt->spin_in_func);
      lives_signal_handler_unblock(mt->spinbutton_out, mt->spin_out_func);
      return;
    }

    if (new_tl_tc != old_tl_tc) {
      weed_plant_t *shortcut;
      if (new_tl_tc > old_tl_tc) shortcut = block->end_event;
      else shortcut = block->start_event;
      if (block->next == NULL || block->next->start_event != block->end_event) remove_audio_for_track(block->end_event, -1);
      new_end_event = get_frame_event_at(mt->event_list, new_tl_tc, shortcut, TRUE);
      if (new_end_event == block->start_event) return;
      block->end_event = new_end_event;

      if (block->end_event == NULL) {
        weed_plant_t *last_frame_event = get_last_frame_event(mt->event_list);
        add_blank_frames_up_to(mt->event_list, last_frame_event, new_tl_tc, mt->fps);
        block->end_event = get_last_frame_event(mt->event_list);
      }
      if (block->next == NULL || block->next->start_event != block->end_event)
        insert_audio_event_at(block->end_event, -1, aclip, 0., 0.);

      lives_widget_queue_draw((LiVESWidget *)mt->audio_draws->data);
      new_end_tc = start_tc + (get_event_timecode(block->end_event) - get_event_timecode(block->start_event)) * new_avel;
      lives_signal_handler_block(mt->spinbutton_out, mt->spin_out_func);
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(mt->spinbutton_out), new_end_tc / TICKS_PER_SECOND_DBL);
      lives_signal_handler_unblock(mt->spinbutton_out, mt->spin_out_func);
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(mt->spinbutton_out), orig_end_val);

      remove_end_blank_frames(mt->event_list, TRUE);

      if (mt->avol_fx != -1 && block->next == NULL) {
        apply_avol_filter(mt);
      }
    }
    if (!resize_timeline(mt)) {
      redraw_eventbox(mt, block->eventbox);
      paint_lines(mt, mt->ptr_time, TRUE);
    }
    return;
  }

  // move start point (if we can)
  min_tc = 0;
  if (block->prev != NULL) min_tc = get_event_timecode(block->prev->end_event);

  new_tl_tc = q_gint64(get_event_timecode(block->end_event) - (orig_end_val * TICKS_PER_SECOND_DBL - start_tc) / new_avel,
                       mt->fps);
  new_end_tc = orig_end_val * TICKS_PER_SECOND_DBL;

  if (new_tl_tc < min_tc) {
    aseek -= (new_tl_tc - min_tc) / TICKS_PER_SECOND_DBL;
    start_tc = block->offset_start = aseek * TICKS_PER_SECOND_DBL;
    new_tl_tc = min_tc;
    was_adjusted = TRUE;
  }

  orig_start_val = lives_spin_button_get_value(LIVES_SPIN_BUTTON(mt->spinbutton_in));

  if (was_adjusted || new_tl_tc != old_tl_tc) {
    weed_plant_t *shortcut;
    if (new_tl_tc > old_tl_tc) shortcut = block->start_event;
    else {
      if (block->prev != NULL) shortcut = block->prev->end_event;
      else shortcut = NULL;
    }

    new_start_event = get_frame_event_at(mt->event_list, new_tl_tc, shortcut, TRUE);
    if (new_start_event == block->end_event) return;

    if (block->prev == NULL || block->start_event != block->prev->end_event) remove_audio_for_track(block->start_event, -1);
    else insert_audio_event_at(block->start_event, -1, aclip, 0., 0.);
    block->start_event = new_start_event;

    insert_audio_event_at(block->start_event, -1, aclip, aseek, new_avel);

    lives_widget_queue_draw((LiVESWidget *)mt->audio_draws->data);

    set_in_out_spin_ranges(mt, start_tc, new_end_tc);
    lives_signal_handler_block(mt->spinbutton_out, mt->spin_out_func);
    lives_signal_handler_block(mt->spinbutton_in, mt->spin_in_func);

    lives_spin_button_set_value(LIVES_SPIN_BUTTON(mt->spinbutton_in), start_tc / TICKS_PER_SECOND_DBL);

    if (!was_adjusted) lives_spin_button_set_value(LIVES_SPIN_BUTTON(mt->spinbutton_in), orig_start_val);

    lives_signal_handler_unblock(mt->spinbutton_in, mt->spin_in_func);
    lives_signal_handler_unblock(mt->spinbutton_out, mt->spin_out_func);

    if (mt->avol_fx != -1 && block->next == NULL) {
      apply_avol_filter(mt);
    }
  }
}


void in_anchor_toggled(LiVESToggleButton * togglebutton, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  track_rect *block = mt->block_selected;
  weed_timecode_t offset_end;
  double avel = 1.;

  if (!mainw->interactive) return;

  if (mt->current_track < 0) {
    avel = get_audio_frame_vel(block->start_event, mt->current_track);
  }

  offset_end = block->offset_start + (double)(mt->current_track >= 0 && !mt->aud_track_selected) *
               (weed_timecode_t)(TICKS_PER_SECOND_DBL / mt->fps) + ((get_event_timecode(block->end_event) -
                   get_event_timecode(block->start_event))) * avel;

  block->start_anchored = !block->start_anchored;

  set_in_out_spin_ranges(mt, block->offset_start, offset_end);

  if ((block->start_anchored && block->end_anchored) || LIVES_IS_PLAYING) {
    lives_widget_set_sensitive(mt->spinbutton_avel, FALSE);
    lives_widget_set_sensitive(mt->avel_scale, FALSE);
  } else {
    lives_widget_set_sensitive(mt->spinbutton_avel, TRUE);
    lives_widget_set_sensitive(mt->avel_scale, TRUE);
  }

  if (mt->current_track >= 0 && mt->opts.pertrack_audio) {
    LiVESWidget *xeventbox;
    track_rect *xblock;

    // if video, find the audio track, and vice-versa
    if (mt->aud_track_selected) {
      xeventbox = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(block->eventbox), "owner");
    } else {
      xeventbox = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(block->eventbox), "atrack");
    }
    if (xeventbox != NULL) {
      xblock = get_block_from_time(xeventbox, get_event_timecode(block->start_event) / TICKS_PER_SECOND_DBL, mt);
      if (xblock != NULL) xblock->start_anchored = block->start_anchored;
    }
  }
}


void out_anchor_toggled(LiVESToggleButton * togglebutton, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  track_rect *block = mt->block_selected;
  weed_timecode_t offset_end;
  double avel = 1.;

  if (!mainw->interactive) return;

  if (mt->current_track < 0) {
    avel = get_audio_frame_vel(block->start_event, mt->current_track);
  }

  offset_end = block->offset_start + (double)(mt->current_track >= 0 && !mt->aud_track_selected) *
               (weed_timecode_t)(TICKS_PER_SECOND_DBL / mt->fps) + ((get_event_timecode(block->end_event) -
                   get_event_timecode(block->start_event))) * avel;

  block->end_anchored = !block->end_anchored;

  set_in_out_spin_ranges(mt, block->offset_start, offset_end);

  if ((block->start_anchored && block->end_anchored) || LIVES_IS_PLAYING) {
    lives_widget_set_sensitive(mt->spinbutton_avel, FALSE);
    lives_widget_set_sensitive(mt->avel_scale, FALSE);
  } else {
    lives_widget_set_sensitive(mt->spinbutton_avel, TRUE);
    lives_widget_set_sensitive(mt->avel_scale, TRUE);
  }

  if (mt->current_track >= 0 && mt->opts.pertrack_audio) {
    LiVESWidget *xeventbox;
    track_rect *xblock;

    // if video, find the audio track, and vice-versa
    if (mt->aud_track_selected) {
      xeventbox = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(block->eventbox), "owner");
    } else {
      xeventbox = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(block->eventbox), "atrack");
    }
    if (xeventbox != NULL) {
      xblock = get_block_from_time(xeventbox, get_event_timecode(block->start_event) / TICKS_PER_SECOND_DBL, mt);
      if (xblock != NULL) xblock->end_anchored = block->end_anchored;
    }
  }
}


#define POLY_WIDTH_MARGIN 4 // I think this is the frame border width (1 px) * 2 * 2

void polymorph(lives_mt * mt, lives_mt_poly_state_t poly) {
  LiVESPixbuf *thumb;

  weed_timecode_t offset_end = 0;
  weed_timecode_t tc;

  weed_plant_t *filter, *inst;
  weed_plant_t *frame_event, *filter_map = NULL;
  weed_plant_t *init_event;
  weed_plant_t *prev_fm_event, *next_fm_event, *shortcut;

  track_rect *block = mt->block_selected;

  void **init_events;

  int *in_tracks, *out_tracks;

  LiVESWidget *bbox;
  LiVESWidget *eventbox, *xeventbox, *yeventbox, *label, *vbox;

  double secs;
  double out_end_range;
  double avel = 1.;

  char *fhash;
  char *fname, *otrackname, *txt;

  boolean is_input, is_output;
  boolean has_effect = FALSE;
  boolean has_params;
  boolean tab_set = FALSE;
  boolean start_anchored, end_anchored;

  int num_in_tracks, num_out_tracks;
  int def_out_track = 0;
  int num_fx = 0;
  int fidx;
  int olayer;
  int fxcount = 0;
  int nins = 1;
  int width = cfile->hsize;
  int height = cfile->vsize;
  int track, fromtrack;
  int frame_start, frame_end = 0;
  int filenum;

  static int xxwidth = 0, xxheight = 0;

  register int i, j;

  if (mt->in_sensitise) return;

  if (poly == mt->poly_state && poly != POLY_PARAMS && poly != POLY_FX_STACK) {
    return;
  }

  switch (mt->poly_state) {
  case (POLY_CLIPS) :
    lives_container_remove(LIVES_CONTAINER(mt->poly_box), mt->clip_scroll);
    break;
  case (POLY_IN_OUT) :
    lives_signal_handler_block(mt->spinbutton_in, mt->spin_in_func);
    lives_signal_handler_block(mt->spinbutton_out, mt->spin_out_func);
    if (lives_widget_get_parent(mt->in_out_box) != NULL) lives_widget_unparent(mt->in_out_box);
    if (lives_widget_get_parent(mt->avel_box) != NULL) lives_widget_unparent(mt->avel_box);

    break;
  case (POLY_PARAMS) :
    if (mt->framedraw != NULL) {
      special_cleanup(FALSE);
      mt->framedraw = NULL;
    }
    if (mt->current_rfx != NULL) {
      rfx_free(mt->current_rfx);
      lives_free(mt->current_rfx);
    }
    mt->current_rfx = NULL;

    if (mt->fx_box != NULL) {
      lives_widget_destroy(mt->fx_box);
      mt->fx_box = NULL;
      lives_widget_destroy(mt->fx_params_label);
      mt->fx_params_label = NULL;
      lives_container_remove(LIVES_CONTAINER(mt->poly_box), mt->fx_base_box);
    }

    if (mt->mt_frame_preview) {
      // put blank back in preview window
      lives_widget_object_ref(mainw->playarea);
      if (palette->style & STYLE_1) {
        lives_widget_set_bg_color(mt->fd_frame, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
      }
    }
    if (pchain != NULL && poly != POLY_PARAMS) {
      // no freep !
      lives_free(pchain);
      pchain = NULL;
    }
    mouse_mode_context(mt); // reset context box text
    mt->last_fx_type = MT_LAST_FX_NONE;

    lives_widget_set_sensitive(mt->fx_edit, FALSE);
    lives_widget_set_sensitive(mt->fx_delete, FALSE);
    if (poly == POLY_PARAMS) {
      lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET, TRUE);
    } else {
      mt->init_event = NULL;
      mt_show_current_frame(mt, FALSE);
    }

    break;
  case POLY_FX_STACK:
    if (poly != POLY_FX_STACK) {
      mt->selected_init_event = NULL;
      mt->fm_edit_event = NULL;
      mt->context_time = -1.;
    }
    break;
  case POLY_EFFECTS:
  case POLY_TRANS:
  case POLY_COMP:
    free_pkg_list();
    if (mt->fx_list_scroll != NULL) lives_widget_destroy(mt->fx_list_scroll);
    mt->fx_list_scroll = NULL;
    break;
  default:
    break;
  }

  if (mt->fx_list_box != NULL) lives_widget_unparent(mt->fx_list_box);
  mt->fx_list_box = NULL;
  if (mt->nb_label != NULL) lives_widget_destroy(mt->nb_label);
  mt->nb_label = NULL;

  mt->poly_state = poly;

  if (mt->poly_state == POLY_NONE) return; // transitional state

  switch (poly) {
  case (POLY_IN_OUT) :

    set_poly_tab(mt, POLY_IN_OUT);

    while (xxwidth < 1 || xxheight < 1) {
      if (lives_widget_get_allocation_width(mt->poly_box) > 1 && lives_widget_get_allocation_height(mt->poly_box) > 1) {
        calc_maxspect(lives_widget_get_allocation_width(mt->poly_box) / 2 - POLY_WIDTH_MARGIN,
                      lives_widget_get_allocation_height(mt->poly_box) - POLY_WIDTH_MARGIN / 2 - 16 * widget_opts.packing_height -
                      ((block == NULL || block->ordered) ? lives_widget_get_allocation_height(mainw->spinbutton_start) : 0), &width, &height);

        xxwidth = width;
        xxheight = height;
      } else {
        // need to force show the widgets to get sizes
        lives_widget_show(mt->in_hbox);
        lives_widget_context_update();
        lives_usleep(prefs->sleep_time);
      }
    }

    width = xxwidth;
    height = xxheight;

    mt->init_event = NULL;
    if (block == NULL || block->ordered) {
      lives_widget_show(mt->in_hbox);
      lives_widget_show(mt->out_hbox);
      lives_idle_add(show_in_out_images, (livespointer)mt);
    } else {
      lives_widget_hide(mt->in_hbox);
      lives_widget_hide(mt->out_hbox);
    }

    if (block != NULL) {
      track = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(block->eventbox), "layer_number"));

      offset_end = block->offset_start + (weed_timecode_t)((double)(track >= 0) * TICKS_PER_SECOND_DBL / mt->fps) +
                   ((get_event_timecode(block->end_event) - get_event_timecode(block->start_event)) * ABS(avel));

      start_anchored = block->start_anchored;
      end_anchored = block->end_anchored;
    } else {
      track = 0;

      start_anchored = end_anchored = FALSE;

      filenum = mt->file_selected;

      frame_start = mainw->files[filenum]->start;
      frame_end = mainw->files[filenum]->end;
    }

    if (track > -1) {
      LiVESWidget *oeventbox;

      if (block != NULL) {
        secs = lives_ruler_get_value(LIVES_RULER(mt->timeline));
        if (mt->context_time != -1. && mt->use_context) secs = mt->context_time;
        if (is_audio_eventbox(block->eventbox) &&
            (oeventbox = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(block->eventbox),
                         "owner")) != NULL) {
          // if moving an audio block we move the associated video block first
          block = get_block_from_time(oeventbox, secs, mt);
        }
        filenum = get_frame_event_clip(block->start_event, track);

        frame_start = calc_frame_from_time(filenum, block->offset_start / TICKS_PER_SECOND_DBL);
        frame_end = calc_frame_from_time(filenum, offset_end / TICKS_PER_SECOND_DBL - 1. / mt->fps);
      }

      lives_container_set_border_width(LIVES_CONTAINER(mt->poly_box), 0);

      if (mainw->playing_file == filenum) {
        mainw->files[filenum]->event_list = mt->event_list;
      }
      // start image
      thumb = make_thumb(mt, filenum, width, height, frame_start, LIVES_INTERP_NORMAL, FALSE);
      lives_image_set_from_pixbuf(LIVES_IMAGE(mt->in_image), thumb);
      if (thumb != NULL) lives_widget_object_unref(thumb);
    } else {
      lives_container_set_border_width(LIVES_CONTAINER(mt->poly_box), widget_opts.border_width);
      filenum = get_audio_frame_clip(block->start_event, track);
      lives_box_pack_start(LIVES_BOX(mt->poly_box), mt->avel_box, TRUE, TRUE, 0);
      lives_widget_show_all(mt->avel_box);
      avel = get_audio_frame_vel(block->start_event, track);
      offset_end = block->offset_start + q_gint64((weed_timecode_t)((double)(track >= 0) * TICKS_PER_SECOND_DBL / mt->fps) +
                   ((get_event_timecode(block->end_event) -
                     get_event_timecode(block->start_event)) * ABS(avel)), mt->fps);
    }

    if (block == NULL) {
      lives_widget_hide(mt->checkbutton_start_anchored);
      lives_widget_hide(mt->checkbutton_end_anchored);
      lives_spin_button_set_digits(LIVES_SPIN_BUTTON(mt->spinbutton_in), 0);
      lives_spin_button_set_digits(LIVES_SPIN_BUTTON(mt->spinbutton_out), 0);
      lives_spin_button_configure(LIVES_SPIN_BUTTON(mt->spinbutton_in), mainw->files[filenum]->start, 1.,
                                  mainw->files[filenum]->frames, 1., 100.);
      lives_spin_button_configure(LIVES_SPIN_BUTTON(mt->spinbutton_out), mainw->files[filenum]->end, 1.,
                                  mainw->files[filenum]->frames, 1., 100.);
    } else {
      lives_widget_show(mt->checkbutton_start_anchored);
      lives_widget_show(mt->checkbutton_end_anchored);
      lives_spin_button_set_digits(LIVES_SPIN_BUTTON(mt->spinbutton_in), 2);
      lives_spin_button_set_digits(LIVES_SPIN_BUTTON(mt->spinbutton_out), 2);
      lives_spin_button_configure(LIVES_SPIN_BUTTON(mt->spinbutton_in), 0., 0., 0., 1. / mt->fps, 1.);
      lives_spin_button_configure(LIVES_SPIN_BUTTON(mt->spinbutton_out), 0., 0., 0., 1. / mt->fps, 1.);
    }

    if (avel > 0.) {
      if (block != NULL) {
        lives_spin_button_set_range(LIVES_SPIN_BUTTON(mt->spinbutton_in), 0., offset_end / TICKS_PER_SECOND_DBL - 1. / mt->fps);
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(mt->spinbutton_in), block->offset_start / TICKS_PER_SECOND_DBL);

      } else {
        lives_spin_button_set_range(LIVES_SPIN_BUTTON(mt->spinbutton_in), 1., mainw->files[filenum]->frames);
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(mt->spinbutton_in), mainw->files[filenum]->start);
      }
      lives_signal_handler_block(mt->checkbutton_start_anchored, mt->check_start_func);
      lives_signal_handler_block(mt->checkbutton_avel_reverse, mt->check_avel_rev_func);
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(mt->checkbutton_start_anchored), start_anchored);
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(mt->checkbutton_avel_reverse), FALSE);
      lives_signal_handler_unblock(mt->checkbutton_avel_reverse, mt->check_avel_rev_func);
      lives_signal_handler_unblock(mt->checkbutton_start_anchored, mt->check_start_func);
    } else {
      lives_spin_button_set_range(LIVES_SPIN_BUTTON(mt->spinbutton_out), 0., offset_end / TICKS_PER_SECOND_DBL - 1. / mt->fps);
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(mt->spinbutton_out), block->offset_start / TICKS_PER_SECOND_DBL);
      lives_signal_handler_block(mt->checkbutton_start_anchored, mt->check_start_func);
      lives_signal_handler_block(mt->checkbutton_avel_reverse, mt->check_avel_rev_func);
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(mt->checkbutton_start_anchored), start_anchored);
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(mt->checkbutton_avel_reverse), TRUE);
      lives_signal_handler_unblock(mt->checkbutton_avel_reverse, mt->check_avel_rev_func);
      lives_signal_handler_unblock(mt->checkbutton_start_anchored, mt->check_start_func);
    }

    lives_signal_handler_block(mt->spinbutton_avel, mt->spin_avel_func);
    lives_spin_button_set_value(LIVES_SPIN_BUTTON(mt->spinbutton_avel), ABS(avel));
    lives_signal_handler_unblock(mt->spinbutton_avel, mt->spin_avel_func);

    if (track > -1) {
      // end image
      thumb = make_thumb(mt, filenum, width, height, frame_end, LIVES_INTERP_NORMAL, FALSE);
      lives_image_set_from_pixbuf(LIVES_IMAGE(mt->out_image), thumb);
      if (thumb != NULL) lives_widget_object_unref(thumb);
      out_end_range = count_resampled_frames(mainw->files[filenum]->frames, mainw->files[filenum]->fps, mt->fps) / mt->fps;
    } else {
      out_end_range = q_gint64(mainw->files[filenum]->laudio_time * TICKS_PER_SECOND_DBL, mt->fps) / TICKS_PER_SECOND_DBL;
    }
    if (avel > 0.) {
      if (block != NULL) {
        lives_spin_button_set_range(LIVES_SPIN_BUTTON(mt->spinbutton_out), block->offset_start / TICKS_PER_SECOND_DBL + 1.
                                    / mt->fps, out_end_range);
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(mt->spinbutton_out), offset_end / TICKS_PER_SECOND_DBL);
        if (!block->start_anchored || !block->end_anchored) {
          lives_widget_set_sensitive(mt->spinbutton_avel, TRUE);
          lives_widget_set_sensitive(mt->avel_scale, TRUE);
        }
      }
      lives_widget_set_sensitive(mt->spinbutton_in, TRUE);
      lives_widget_set_sensitive(mt->spinbutton_out, TRUE);

      lives_widget_grab_focus(mt->spinbutton_in);
    } else {
      lives_spin_button_set_range(LIVES_SPIN_BUTTON(mt->spinbutton_in), block->offset_start / TICKS_PER_SECOND_DBL + 1. / mt->fps,
                                  out_end_range);
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(mt->spinbutton_in), offset_end / TICKS_PER_SECOND_DBL);
      lives_widget_set_sensitive(mt->spinbutton_in, FALSE);
      lives_widget_set_sensitive(mt->spinbutton_out, FALSE);
      lives_widget_set_sensitive(mt->spinbutton_avel, FALSE);
      lives_widget_set_sensitive(mt->avel_scale, FALSE);
    }

    lives_signal_handler_block(mt->checkbutton_end_anchored, mt->check_end_func);
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(mt->checkbutton_end_anchored), end_anchored);
    lives_signal_handler_unblock(mt->checkbutton_end_anchored, mt->check_end_func);
    lives_box_pack_start(LIVES_BOX(mt->poly_box), mt->in_out_box, TRUE, TRUE, 0);

    lives_widget_show_all(mt->in_out_box);
    if (track > -1) {
      lives_widget_hide(mt->avel_box);
    } else {
      lives_widget_hide(mt->in_image);
      lives_widget_hide(mt->out_image);
    }

    if (block == NULL) {
      lives_widget_hide(mt->checkbutton_start_anchored);
      lives_widget_hide(mt->checkbutton_end_anchored);
    }

    lives_signal_handler_unblock(mt->spinbutton_in, mt->spin_in_func);
    lives_signal_handler_unblock(mt->spinbutton_out, mt->spin_out_func);

    if (LIVES_IS_PLAYING) mt_desensitise(mt);
    else {
      mt_sensitise(mt);
      lives_widget_grab_focus(mt->spinbutton_in);
    }

    lives_widget_set_valign(mt->in_out_box, LIVES_ALIGN_CENTER);
    break;
  case (POLY_CLIPS) :
    set_poly_tab(mt, POLY_CLIPS);
    mt->init_event = NULL;
    lives_box_pack_start(LIVES_BOX(mt->poly_box), mt->clip_scroll, TRUE, TRUE, 0);
    if (mt->is_ready) mouse_mode_context(mt);
    break;

  case (POLY_PARAMS):
    lives_box_pack_start(LIVES_BOX(mt->poly_box), mt->fx_base_box, TRUE, TRUE, 0);

    filter = get_weed_filter(mt->current_fx);

    if (mt->current_rfx != NULL) {
      rfx_free(mt->current_rfx);
      lives_free(mt->current_rfx);
    }

    // init an inst, in case the plugin needs to set anything
    inst = weed_instance_from_filter(filter);
    weed_reinit_effect(inst, TRUE);
    check_string_choice_params(inst);
    weed_instance_unref(inst);
    weed_instance_unref(inst);

    mt->current_rfx = weed_to_rfx(filter, FALSE);

    tc = get_event_timecode(mt->init_event);

    if (fx_dialog[1] != NULL) {
      lives_rfx_t *rfx = fx_dialog[1]->rfx;
      on_paramwindow_button_clicked(NULL, rfx);
      lives_widget_destroy(fx_dialog[1]->dialog);
      lives_freep((void **)&fx_dialog[1]);
    }

    get_track_index(mt, tc);

    mt->prev_fx_time = 0; // force redraw in node_spin_val_changed
    has_params = add_mt_param_box(mt);

    if (has_params && mainw->playing_file < 0) {
      lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET, TRUE);
      mt->block_tl_move = TRUE;
      on_node_spin_value_changed(LIVES_SPIN_BUTTON(mt->node_spinbutton), mt); // force parameter interpolation
      mt->block_tl_move = FALSE;
    }
    clear_context(mt);
    if (has_params) {
      add_context_label(mt, _("Drag the time slider to where you"));
      add_context_label(mt, _("want to set effect parameters"));
      add_context_label(mt, _("Set parameters, then click \"Apply\"\n"));
      add_context_label(mt, _("NODES are points where parameters\nhave been set.\nNodes can be deleted."));
    } else {
      add_context_label(mt, _("Effect has no parameters.\n"));
    }
    lives_widget_show_all(mt->fx_base_box);
    if (!has_params) {
      lives_widget_hide(mt->apply_fx_button);
      lives_widget_hide(mt->resetp_button);
      lives_widget_hide(mt->del_node_button);
      lives_widget_hide(mt->prev_node_button);
      lives_widget_hide(mt->next_node_button);
    }
    set_poly_tab(mt, POLY_PARAMS);
    break;
  case POLY_FX_STACK:
    mt->init_event = NULL;
    if (mt->current_track >= 0) eventbox = (LiVESWidget *)lives_list_nth_data(mt->video_draws, mt->current_track);
    else eventbox = (LiVESWidget *)mt->audio_draws->data;

    if (eventbox == NULL) break; /// < can happen during mt exit

    secs = mt->ptr_time;
    if (mt->context_time != -1. && mt->use_context) secs = mt->context_time;

    block = get_block_from_time(eventbox, secs, mt);
    if (block == NULL) {
      block = get_block_before(eventbox, secs, FALSE);
      if (block != NULL) shortcut = block->end_event;
      else shortcut = NULL;
    } else shortcut = block->start_event;

    tc = q_gint64(secs * TICKS_PER_SECOND_DBL, mt->fps);

    frame_event = get_frame_event_at(mt->event_list, tc, shortcut, TRUE);

    if (frame_event != NULL)
      filter_map = mt->fm_edit_event = get_filter_map_before(frame_event, LIVES_TRACK_ANY, NULL);

    mt->fx_list_box = lives_vbox_new(FALSE, 0);
    lives_widget_apply_theme(mt->fx_list_box, LIVES_WIDGET_STATE_NORMAL);
    mt->fx_list_label = lives_label_new("");
    lives_widget_apply_theme2(mt->fx_list_label, LIVES_WIDGET_STATE_NORMAL, TRUE);
    lives_box_pack_start(LIVES_BOX(mt->fx_list_box), mt->fx_list_label, FALSE, TRUE, widget_opts.packing_height);

    mt->fx_list_scroll = lives_scrolled_window_new(NULL, NULL);
    lives_scrolled_window_set_policy(LIVES_SCROLLED_WINDOW(mt->fx_list_scroll), LIVES_POLICY_AUTOMATIC, LIVES_POLICY_AUTOMATIC);
    lives_box_pack_start(LIVES_BOX(mt->fx_list_box), mt->fx_list_scroll, TRUE, TRUE, 0);

    lives_box_pack_start(LIVES_BOX(mt->poly_box), mt->fx_list_box, TRUE, TRUE, 0);

    mt->fx_list_vbox = lives_vbox_new(FALSE, widget_opts.packing_height);
    lives_container_set_border_width(LIVES_CONTAINER(mt->fx_list_vbox), widget_opts.border_width);
    lives_scrolled_window_add_with_viewport(LIVES_SCROLLED_WINDOW(mt->fx_list_scroll), mt->fx_list_vbox);
    lives_widget_set_bg_color(lives_bin_get_child(LIVES_BIN(mt->fx_list_scroll)), LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);

    if (filter_map != NULL) {
      init_events = weed_get_voidptr_array_counted(filter_map, WEED_LEAF_INIT_EVENTS, &num_fx);
      if (num_fx > 0) {
        for (i = 0; i < num_fx; i++) {
          init_event = (weed_plant_t *)init_events[i];
          if (init_event != NULL) {
            is_input = FALSE;
            fromtrack = -1;
            in_tracks = weed_get_int_array_counted(init_event, WEED_LEAF_IN_TRACKS, &num_in_tracks);
            if (num_in_tracks > 0) {
              for (j = 0; j < num_in_tracks; j++) {
                if (in_tracks[j] == mt->current_track) {
                  is_input = TRUE;
                } else if (num_in_tracks == 2) fromtrack = in_tracks[j];
              }
              lives_free(in_tracks);
            }
            is_output = FALSE;
            out_tracks = weed_get_int_array_counted(init_event, WEED_LEAF_OUT_TRACKS, &num_out_tracks);
            if (num_out_tracks > 0) {
              def_out_track = out_tracks[0];
              for (j = 0; j < num_out_tracks; j++) {
                if (out_tracks[j] == mt->current_track) {
                  is_output = TRUE;
                  break;
                }
              }
              lives_free(out_tracks);
            }

            if (!is_input && !is_output) continue;

            has_effect = TRUE;

            fxcount++;

            fhash = weed_get_string_value(init_event, WEED_LEAF_FILTER, NULL);
            fidx = weed_get_idx_for_hashname(fhash, TRUE);
            lives_free(fhash);
            fname = weed_filter_idx_get_name(fidx, FALSE, FALSE, FALSE);

            if (!is_input) {
              txt = lives_strdup_printf(_("%s output"), fname);
            } else if (!is_output && num_out_tracks > 0) {
              if (def_out_track > -1) {
                yeventbox = (LiVESWidget *)lives_list_nth_data(mt->video_draws, def_out_track);
                olayer = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(yeventbox), "layer_number"));
                otrackname = lives_strdup_printf(_("layer %d"), olayer);
              } else otrackname = lives_strdup(_("audio track"));
              txt = lives_strdup_printf(_("%s to %s"), fname, otrackname);
              lives_free(otrackname);
            } else if (num_in_tracks == 2 && num_out_tracks > 0) {
              if (fromtrack > -1) {
                yeventbox = (LiVESWidget *)lives_list_nth_data(mt->video_draws, fromtrack);
                olayer = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(yeventbox), "layer_number"));
                otrackname = lives_strdup_printf(_("layer %d"), olayer);
              } else otrackname = lives_strdup(_("audio track"));
              txt = lives_strdup_printf(_("%s from %s"), fname, otrackname);
              lives_free(otrackname);
            } else {
              txt = lives_strdup(fname);
            }
            xeventbox = lives_event_box_new();
            lives_widget_object_set_data(LIVES_WIDGET_OBJECT(xeventbox), "init_event", (livespointer)init_event);

            lives_widget_add_events(xeventbox, LIVES_BUTTON_RELEASE_MASK | LIVES_BUTTON_PRESS_MASK);

            vbox = lives_vbox_new(FALSE, 0);

            lives_container_set_border_width(LIVES_CONTAINER(vbox), widget_opts.border_width >> 1);
            lives_container_add(LIVES_CONTAINER(xeventbox), vbox);
            label = lives_label_new(txt);
            lives_free(txt);
            lives_free(fname);

            lives_container_set_border_width(LIVES_CONTAINER(xeventbox), widget_opts.border_width >> 1);
            lives_box_pack_start(LIVES_BOX(vbox), label, FALSE, FALSE, 0);
            lives_box_pack_start(LIVES_BOX(mt->fx_list_vbox), xeventbox, FALSE, FALSE, 0);

            if (init_event == mt->selected_init_event) {
              lives_widget_apply_theme2(xeventbox, LIVES_WIDGET_STATE_NORMAL, TRUE);
              set_child_alt_colour(xeventbox, TRUE);
            } else {
              lives_widget_apply_theme(xeventbox, LIVES_WIDGET_STATE_NORMAL);
              set_child_colour(xeventbox, TRUE);
            }

            lives_signal_connect(LIVES_GUI_OBJECT(xeventbox), LIVES_WIDGET_BUTTON_PRESS_EVENT,
                                 LIVES_GUI_CALLBACK(fx_ebox_pressed),
                                 (livespointer)mt);
          }
        }
        lives_free(init_events);
      }
    }

    add_hsep_to_box(LIVES_BOX(mt->fx_list_box));

    bbox = lives_hbutton_box_new();

    lives_button_box_set_layout(LIVES_BUTTON_BOX(bbox), LIVES_BUTTONBOX_SPREAD);
    lives_box_pack_end(LIVES_BOX(mt->fx_list_box), bbox, FALSE, FALSE, widget_opts.packing_height);
    lives_container_set_border_width(LIVES_CONTAINER(mt->fx_list_box), widget_opts.border_width);

    widget_opts.expand = LIVES_EXPAND_DEFAULT_WIDTH;
    mt->prev_fm_button = lives_standard_button_new_with_label(_("_Prev filter map"));  // Note to translators: previous filter map
    widget_opts.expand = LIVES_EXPAND_DEFAULT;
    lives_box_pack_start(LIVES_BOX(bbox), mt->prev_fm_button, FALSE, FALSE, 0);

    lives_widget_set_sensitive(mt->prev_fm_button, (prev_fm_event = get_prev_fm(mt, mt->current_track, frame_event)) != NULL &&
                               (get_event_timecode(prev_fm_event) != (get_event_timecode(frame_event))));

    lives_signal_connect(LIVES_GUI_OBJECT(mt->prev_fm_button), LIVES_WIDGET_CLICKED_SIGNAL,
                         LIVES_GUI_CALLBACK(on_prev_fm_clicked),
                         (livespointer)mt);

    if (fxcount > 1) {
      widget_opts.expand = LIVES_EXPAND_DEFAULT_WIDTH;
      mt->fx_ibefore_button = lives_standard_button_new_with_label(_("Insert _before"));
      widget_opts.expand = LIVES_EXPAND_DEFAULT;
      lives_box_pack_start(LIVES_BOX(bbox), mt->fx_ibefore_button, FALSE, FALSE, 0);
      lives_widget_set_sensitive(mt->fx_ibefore_button, mt->fx_order == FX_ORD_NONE &&
                                 get_event_timecode(mt->fm_edit_event) == get_event_timecode(frame_event) &&
                                 mt->selected_init_event != NULL);

      lives_signal_connect(LIVES_GUI_OBJECT(mt->fx_ibefore_button), LIVES_WIDGET_CLICKED_SIGNAL,
                           LIVES_GUI_CALLBACK(on_fx_insb_clicked),
                           (livespointer)mt);

      widget_opts.expand = LIVES_EXPAND_DEFAULT_WIDTH;
      mt->fx_iafter_button = lives_standard_button_new_with_label(_("Insert _after"));
      widget_opts.expand = LIVES_EXPAND_DEFAULT;
      lives_box_pack_start(LIVES_BOX(bbox), mt->fx_iafter_button, FALSE, FALSE, 0);
      lives_widget_set_sensitive(mt->fx_iafter_button, mt->fx_order == FX_ORD_NONE &&
                                 get_event_timecode(mt->fm_edit_event) == get_event_timecode(frame_event) &&
                                 mt->selected_init_event != NULL);

      lives_signal_connect(LIVES_GUI_OBJECT(mt->fx_iafter_button), LIVES_WIDGET_CLICKED_SIGNAL,
                           LIVES_GUI_CALLBACK(on_fx_insa_clicked),
                           (livespointer)mt);

    } else {
      mt->fx_ibefore_button = mt->fx_iafter_button = NULL;
    }

    widget_opts.expand = LIVES_EXPAND_DEFAULT_WIDTH;
    mt->next_fm_button = lives_standard_button_new_with_label(_("_Next filter map"));
    widget_opts.expand = LIVES_EXPAND_DEFAULT;
    lives_box_pack_end(LIVES_BOX(bbox), mt->next_fm_button, FALSE, FALSE, 0);

    lives_widget_set_sensitive(mt->next_fm_button, (next_fm_event = get_next_fm(mt, mt->current_track, frame_event)) != NULL &&
                               (get_event_timecode(next_fm_event) > get_event_timecode(frame_event)));

    lives_signal_connect(LIVES_GUI_OBJECT(mt->next_fm_button), LIVES_WIDGET_CLICKED_SIGNAL,
                         LIVES_GUI_CALLBACK(on_next_fm_clicked),
                         (livespointer)mt);

    if (has_effect) {
      do_fx_list_context(mt, fxcount);
    } else {
      widget_opts.justify = LIVES_JUSTIFY_CENTER;
      label = lives_standard_label_new(_("\n\nNo effects at current track,\ncurrent time.\n"));
      widget_opts.justify = LIVES_JUSTIFY_DEFAULT;
      lives_box_pack_start(LIVES_BOX(mt->fx_list_box), label, TRUE, TRUE, 0);
    }

    lives_widget_show_all(mt->fx_list_box);

    if (!has_effect) {
      lives_widget_hide(mt->fx_list_scroll);
    }

    set_fxlist_label(mt);

    set_poly_tab(mt, POLY_FX_STACK);

    break;

  case POLY_COMP:
    set_poly_tab(mt, POLY_COMP);
    clear_context(mt);
    add_context_label(mt, (_("Drag a compositor anywhere\non the timeline\nto apply it to the selected region.")));
    tab_set = TRUE;
    ++nins;
  case POLY_TRANS:
    if (!tab_set) {
      set_poly_tab(mt, POLY_TRANS);
      clear_context(mt);
      add_context_label(mt, (_("Drag a transition anywhere\non the timeline\nto apply it to the selected region.")));
    }
    tab_set = TRUE;
    ++nins;
  case POLY_EFFECTS:
    pkg_list = NULL;
    if (!tab_set) {
      set_poly_tab(mt, POLY_EFFECTS);
      clear_context(mt);
      add_context_label(mt, (_("Effects can be dragged\nonto blocks on the timeline.")));
    }
    mt->fx_list_box = lives_vbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(mt->poly_box), mt->fx_list_box, TRUE, TRUE, 0);
    mt->fx_list_scroll = NULL;

    if (mt->poly_state == POLY_COMP) nins = 1000000;
    populate_filter_box(nins, mt, 0);
    break;

  default:
    break;
  }
  lives_widget_queue_draw(mt->poly_box);
  /*
    if (prefs->open_maximised) {
    lives_window_unmaximize(LIVES_WINDOW(mt->window));
    lives_window_maximize(LIVES_WINDOW(mt->window));
    }*/
}


static void mouse_select_start(LiVESWidget * widget, LiVESXEventButton * event, lives_mt * mt) {
  double timesecs;
  int min_x;

  if (!mainw->interactive || mt->sel_locked) return;

  lives_widget_set_sensitive(mt->mm_menuitem, FALSE);
  lives_widget_set_sensitive(mt->view_sel_events, FALSE);

  lives_widget_get_pointer((LiVESXDevice *)mainw->mgeom[widget_opts.monitor].mouse_device,
                           mt->timeline, &mt->sel_x, &mt->sel_y);
  timesecs = get_time_from_x(mt, mt->sel_x);
  mt->region_start = mt->region_end = mt->region_init = timesecs;

  mt->region_updating = TRUE;
  on_timeline_update(mt->timeline_eb, NULL, mt);
  mt->region_updating = FALSE;

  lives_widget_get_pointer((LiVESXDevice *)mainw->mgeom[widget_opts.monitor].mouse_device,
                           mt->tl_eventbox, &mt->sel_x, &mt->sel_y);
  lives_widget_get_position(mt->timeline_eb, &min_x, NULL);

  if (mt->sel_x < min_x) mt->sel_x = min_x;
  if (mt->sel_y < 0.) mt->sel_y = 0.;

  lives_widget_queue_draw(mt->tl_hbox);
  lives_widget_queue_draw(mt->timeline);

  mt->tl_selecting = TRUE;
}


static void mouse_select_end(LiVESWidget * widget, LiVESXEventButton * event, lives_mt * mt) {
  if (!mainw->interactive) return;

  mt->tl_selecting = FALSE;
  lives_widget_get_pointer((LiVESXDevice *)mainw->mgeom[prefs->gui_monitor > 0 ? prefs->gui_monitor - 1 : 0].mouse_device,
                           mt->timeline, &mt->sel_x, &mt->sel_y);
  lives_widget_set_sensitive(mt->mm_menuitem, TRUE);
  lives_widget_queue_draw(mt->tl_eventbox);
  on_timeline_release(mt->timeline_reg, NULL, mt);
}


static void mouse_select_move(LiVESWidget * widget, LiVESXEventMotion * event, lives_mt * mt) {
  lives_painter_t *cr;

  LiVESWidget *xeventbox;
  LiVESWidget *checkbutton;

  int x, y;
  int start_x, start_y, width, height;
  int current_track = mt->current_track;

  int rel_x, rel_y, min_x;
  int offs_y_start, offs_y_end, xheight;

  register int i;

  if (!mainw->interactive) return;

  if (mt->block_selected != NULL) unselect_all(mt);

  lives_widget_get_pointer((LiVESXDevice *)mainw->mgeom[prefs->gui_monitor > 0 ? prefs->gui_monitor - 1 : 0].mouse_device,
                           mt->tl_eventbox, &x, &y);
  lives_widget_get_position(mt->timeline_eb, &min_x, NULL);

  if (x < min_x) x = min_x;
  if (y < 0.) y = 0.;

  lives_widget_queue_draw(mt->tl_hbox);
  lives_widget_process_updates(mt->tl_eventbox, FALSE);

  if (x >= mt->sel_x) {
    start_x = mt->sel_x;
    width = x - mt->sel_x;
  } else {
    start_x = x;
    width = mt->sel_x - x;
  }
  if (y >= mt->sel_y) {
    start_y = mt->sel_y;
    height = y - mt->sel_y;
  } else {
    start_y = y;
    height = mt->sel_y - y;
  }

  if (start_x < 0) start_x = 0;
  if (start_y < 0) start_y = 0;

  cr = lives_painter_create_from_widget(mt->tl_eventbox);
  lives_painter_set_source_rgb_from_lives_widget_color(cr, &palette->black);

  lives_painter_rectangle(cr, start_x, start_y, width, height);
  lives_painter_fill(cr);

  for (i = 0; i < mt->num_video_tracks; i++) {
    xeventbox = (LiVESWidget *)lives_list_nth_data(mt->video_draws, i);
    if (LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(xeventbox), "hidden")) == 0) {
      xheight = lives_widget_get_allocation_height(xeventbox);
      checkbutton = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(xeventbox), "checkbutton");
      lives_widget_get_position(xeventbox, &rel_x, &rel_y);
      if (start_y > (rel_y + xheight / 2) || (start_y + height) < (rel_y + xheight / 2)) {
#ifdef ENABLE_GIW
        if (!prefs->lamp_buttons) {
#endif
          if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(checkbutton))) {
            lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(checkbutton), FALSE);
            mt->current_track = current_track;
            track_select(mt);
          }
#ifdef ENABLE_GIW
        } else {
          if (giw_led_get_mode(GIW_LED(checkbutton))) {
            giw_led_set_mode(GIW_LED(checkbutton), FALSE);
            mt->current_track = current_track;
            track_select(mt);
          }
        }
#endif
        continue;
      }
      offs_y_start = 0;
      offs_y_end = xheight;

      if (start_y < rel_y + xheight) {
        offs_y_start = start_y - rel_y;
        lives_painter_move_to(cr, start_x - rel_x, offs_y_start);
        lives_painter_line_to(cr, start_x + width - rel_x - 1, offs_y_start);
      }
      if (start_y + height < rel_y + xheight) {
        offs_y_end = start_y - rel_y + height;
        lives_painter_move_to(cr, start_x - rel_x, offs_y_end);
        lives_painter_line_to(cr, start_x + width - rel_x - 1, offs_y_end);
      }

      lives_painter_move_to(cr, start_x - rel_x, offs_y_start);
      lives_painter_line_to(cr, start_x - rel_x, offs_y_end);
      lives_painter_move_to(cr, start_x - rel_x - 1, offs_y_start);
      lives_painter_line_to(cr, start_x - rel_x - 1, offs_y_end);
      lives_painter_stroke(cr);

#ifdef ENABLE_GIW
      if (!prefs->lamp_buttons) {
#endif
        if (!lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(checkbutton))) {
          lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(checkbutton), TRUE);
          mt->current_track = current_track;
          track_select(mt);
        }
#ifdef ENABLE_GIW
      } else {
        if (!giw_led_get_mode(GIW_LED(checkbutton))) {
          giw_led_set_mode(GIW_LED(checkbutton), TRUE);
          mt->current_track = current_track;
          track_select(mt);
        }
      }
#endif
    }
  }

  lives_painter_destroy(cr);

  if (widget != mt->timeline_eb) {
    mt->region_updating = TRUE;
    on_timeline_update(mt->timeline_eb, NULL, mt);
    mt->region_updating = FALSE;
  }
}


void do_block_context(lives_mt * mt, LiVESXEventButton * event, track_rect * block) {
  // pop up a context menu when a selected block is right clicked on

  LiVESWidget *delete_block;
  LiVESWidget *split_here;
  LiVESWidget *list_fx_here;
  LiVESWidget *selblock;
  LiVESWidget *avol;
  LiVESWidget *menu = lives_menu_new();

  double block_start_time, block_end_time;

  //mouse_select_end(NULL,mt);
  if (!mainw->interactive) return;

  lives_menu_set_title(LIVES_MENU(menu), _("Selected Block/Frame"));

  selblock = lives_standard_menu_item_new_with_label(_("_Select this Block"));
  lives_container_add(LIVES_CONTAINER(menu), selblock);

  lives_signal_connect(LIVES_GUI_OBJECT(selblock), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(selblock_cb),
                       (livespointer)mt);

  if (block->ordered) { // TODO
    split_here = lives_standard_menu_item_new_with_label(_("_Split Block At Cursor"));
    lives_container_add(LIVES_CONTAINER(menu), split_here);

    // disable if cursor out block
    block_start_time = get_event_timecode(block->start_event) / TICKS_PER_SECOND_DBL + 1. / cfile->fps;
    block_end_time = get_event_timecode(block->end_event) / TICKS_PER_SECOND_DBL + (double)(!is_audio_eventbox(
                       block->eventbox)) / cfile->fps;
    if (mt->ptr_time < block_start_time || mt->ptr_time >= block_end_time)
      lives_widget_set_sensitive(split_here, FALSE);

    lives_signal_connect(LIVES_GUI_OBJECT(split_here), LIVES_WIDGET_ACTIVATE_SIGNAL,
                         LIVES_GUI_CALLBACK(on_split_activate),
                         (livespointer)mt);
  }

  list_fx_here = lives_standard_menu_item_new_with_label(_("List _Effects Here"));
  lives_container_add(LIVES_CONTAINER(menu), list_fx_here);

  lives_signal_connect(LIVES_GUI_OBJECT(list_fx_here), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(list_fx_here_cb),
                       (livespointer)mt);

  if (is_audio_eventbox(block->eventbox) && mt->avol_init_event != NULL) {
    char *avol_fxname = weed_get_string_value(get_weed_filter(mt->avol_fx), WEED_LEAF_NAME, NULL);
    char *text = lives_strdup_printf(_("_Adjust %s"), avol_fxname);
    avol = lives_standard_menu_item_new_with_label(text);
    lives_free(avol_fxname);
    lives_free(text);
    lives_container_add(LIVES_CONTAINER(menu), avol);

    lives_signal_connect(LIVES_GUI_OBJECT(avol), LIVES_WIDGET_ACTIVATE_SIGNAL,
                         LIVES_GUI_CALLBACK(mt_avol_quick),
                         (livespointer)mt);

    if (mt->event_list == NULL) lives_widget_set_sensitive(avol, FALSE);
  }

  delete_block = lives_standard_menu_item_new_with_label(_("_Delete this Block"));
  lives_container_add(LIVES_CONTAINER(menu), delete_block);
  if (mt->is_rendering) lives_widget_set_sensitive(delete_block, FALSE);

  lives_signal_connect(LIVES_GUI_OBJECT(delete_block), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(delete_block_cb),
                       (livespointer)mt);

  if (palette->style & STYLE_1) {
    set_child_alt_colour(menu, TRUE);
    lives_widget_set_bg_color(menu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
    lives_widget_set_fg_color(menu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars_fore);
  }

  lives_widget_show_all(menu);
  lives_menu_popup(LIVES_MENU(menu), event);
}


void do_track_context(lives_mt * mt, LiVESXEventButton * event, double timesecs, int track) {
  // pop up a context menu when track is right clicked on

  LiVESWidget *insert_here, *avol;
  LiVESWidget *menu = lives_menu_new();

  boolean has_something = FALSE;
  boolean needs_idlefunc = FALSE;
  boolean did_backup = mt->did_backup;

  if (!mainw->interactive) return;

  if (mt->idlefunc > 0) {
    lives_source_remove(mt->idlefunc);
    mt->idlefunc = 0;
    needs_idlefunc = TRUE;
  }

  mouse_select_end(NULL, event, mt);

  lives_menu_set_title(LIVES_MENU(menu), _("Selected Frame"));

  if (mt->file_selected > 0 && ((track < 0 && mainw->files[mt->file_selected]->achans > 0 &&
                                 mainw->files[mt->file_selected]->laudio_time > 0.) ||
                                (track >= 0 && mainw->files[mt->file_selected]->frames > 0))) {
    if (track >= 0) {
      insert_here = lives_standard_menu_item_new_with_label(_("_Insert Here"));
      lives_signal_connect(LIVES_GUI_OBJECT(insert_here), LIVES_WIDGET_ACTIVATE_SIGNAL,
                           LIVES_GUI_CALLBACK(insert_at_ctx_cb),
                           (livespointer)mt);
    } else {
      insert_here = lives_standard_menu_item_new_with_label(_("_Insert Audio Here"));
      lives_signal_connect(LIVES_GUI_OBJECT(insert_here), LIVES_WIDGET_ACTIVATE_SIGNAL,
                           LIVES_GUI_CALLBACK(insert_audio_at_ctx_cb),
                           (livespointer)mt);
    }
    lives_container_add(LIVES_CONTAINER(menu), insert_here);
    has_something = TRUE;
  }

  if (mt->audio_draws != NULL && (track < 0 || mt->opts.pertrack_audio) && mt->event_list != NULL) {
    char *avol_fxname = weed_get_string_value(get_weed_filter(mt->avol_fx), WEED_LEAF_NAME, NULL);
    char *text = lives_strdup_printf(_("_Adjust %s"), avol_fxname);
    avol = lives_standard_menu_item_new_with_label(text);
    lives_free(avol_fxname);
    lives_free(text);
    lives_container_add(LIVES_CONTAINER(menu), avol);

    lives_signal_connect(LIVES_GUI_OBJECT(avol), LIVES_WIDGET_ACTIVATE_SIGNAL,
                         LIVES_GUI_CALLBACK(mt_avol_quick),
                         (livespointer)mt);

    if (mt->event_list == NULL) lives_widget_set_sensitive(avol, FALSE);

    has_something = TRUE;
  }

  if (has_something) {
    if (palette->style & STYLE_1) {
      set_child_alt_colour(menu, TRUE);
      lives_widget_set_bg_color(menu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
      lives_widget_set_fg_color(menu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars_fore);
    }

    lives_widget_show_all(menu);
    lives_menu_popup(LIVES_MENU(menu), event);

    lives_signal_connect(LIVES_GUI_OBJECT(menu), LIVES_WIDGET_UNMAP_SIGNAL,
                         LIVES_GUI_CALLBACK(rdrw_cb),
                         (livespointer)mt);
  } else lives_widget_destroy(menu);

  if (needs_idlefunc || (!did_backup && mt->auto_changed)) {
    mt->idlefunc = mt_idle_add(mt);
  }
}


boolean on_track_release(LiVESWidget * eventbox, LiVESXEventButton * event, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  weed_timecode_t tc, tcpp;
  LiVESList *list;
  LiVESWidget *xeventbox;
  LiVESWidget *oeventbox;
  LiVESWidget *xlabelbox;
  LiVESWidget *xahbox;
  LiVESXWindow *window;

  double timesecs;

  boolean got_track = FALSE;
  boolean needs_idlefunc = FALSE;
  boolean did_backup = mt->did_backup;

  int x, y;
  int track = 0;

  int win_x, win_y;
  int old_track = mt->current_track;

  register int i;

  if (!mainw->interactive) return FALSE;

  if (mt->idlefunc > 0) {
    lives_source_remove(mt->idlefunc);
    mt->idlefunc = 0;
    needs_idlefunc = TRUE;
  }

  lives_set_cursor_style(LIVES_CURSOR_BUSY, NULL);

  lives_widget_get_pointer((LiVESXDevice *)mainw->mgeom[widget_opts.monitor].mouse_device,
                           eventbox, &x, &y);
  timesecs = get_time_from_x(mt, x);
  tc = timesecs * TICKS_PER_SECOND;

  window = lives_display_get_window_at_pointer
           ((LiVESXDevice *)mainw->mgeom[widget_opts.monitor].mouse_device,
            mt->display, &win_x, &win_y);

  if (cfile->achans > 0) {
    i = 0;
    for (list = mt->audio_draws; list != NULL; list = list->next, i++) {
      xeventbox = (LiVESWidget *)list->data;
      oeventbox = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(xeventbox), "owner");
      if (i >= mt->opts.back_audio_tracks &&
          !LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(oeventbox), "expanded"))) continue;
      xlabelbox = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(xeventbox), "labelbox");
      xahbox = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(xeventbox), "ahbox");
      if (lives_widget_get_xwindow(xeventbox) == window || lives_widget_get_xwindow(xlabelbox) == window ||
          lives_widget_get_xwindow(xahbox) == window) {
        track = i - 1;
        got_track = TRUE;
        mt->aud_track_selected = TRUE;
        break;
      }
    }
  }

  if (track != -1) {
    for (list = mt->video_draws; list != NULL; list = list->next) {
      xeventbox = (LiVESWidget *)list->data;
      xlabelbox = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(xeventbox), "labelbox");
      xahbox = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(xeventbox), "ahbox");
      if (lives_widget_get_xwindow(xeventbox) == window || lives_widget_get_xwindow(xlabelbox) == window ||
          lives_widget_get_xwindow(xahbox) == window) {
        track = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(xeventbox), "layer_number"));
        mt->aud_track_selected = FALSE;
        got_track = TRUE;
        break;
      }
    }
  }

  if (mt->opts.mouse_mode == MOUSE_MODE_SELECT) {
    mouse_select_end(eventbox, event, mt);
    lives_signal_handler_block(mt->tl_eventbox, mt->mouse_mot2);
    mt->sel_y -= y + 2;
  } else {
    if (mt->hotspot_x != 0 || mt->hotspot_y != 0) {
      LiVESXScreen *screen;
      int abs_x, abs_y;

      int height = lives_widget_get_allocation_height(LIVES_WIDGET(lives_list_nth_data(mt->video_draws, 0)));
      lives_display_get_pointer((LiVESXDevice *)mainw->mgeom[widget_opts.monitor].mouse_device,
                                mt->display, &screen, &abs_x, &abs_y, NULL);
      lives_display_warp_pointer((LiVESXDevice *)mainw->mgeom[widget_opts.monitor].mouse_device,
                                 mt->display, screen, abs_x + mt->hotspot_x, abs_y + mt->hotspot_y - height / 2);
      mt->hotspot_x = mt->hotspot_y = 0;
      // we need to call this to warp the pointer
      //lives_widget_context_update();
      lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET, TRUE);
    }

    if (doubleclick) {
      // this is a double-click
      mt->putative_block = get_block_from_time(eventbox, timesecs, mt);
      select_block(mt);
      mt->putative_block = NULL;
      doubleclick = FALSE;
      goto track_rel_done;
    }

    if (got_track && !mt->is_rendering && mt->putative_block != NULL && !LIVES_IS_PLAYING &&
        event->button == 1) {
      weed_timecode_t start_tc;

      mt_desensitise(mt);

      start_tc = get_event_timecode(mt->putative_block->start_event);

      // timecodes per pixel
      tcpp = TICKS_PER_SECOND_DBL * ((mt->tl_max - mt->tl_min) /
                                     (double)lives_widget_get_allocation_width(LIVES_WIDGET(lives_list_nth_data(mt->video_draws, 0))));

      // need to move at least 1.5 pixels, or to another track
      if ((track != mt->current_track || (tc - start_tc > (tcpp * 3 / 2)) || (start_tc - tc > (tcpp * 3 / 2))) &&
          ((old_track < 0 && track < 0) || (old_track >= 0 && track >= 0))) {
        move_block(mt, mt->putative_block, timesecs, old_track, track);
        mt->putative_block = NULL;

        lives_widget_get_pointer((LiVESXDevice *)mainw->mgeom[prefs->gui_monitor > 0 ? prefs->gui_monitor - 1 : 0].mouse_device,
                                 eventbox, &x, &y);
        timesecs = get_time_from_x(mt, x);

        mt_tl_move(mt, timesecs);
      }
    }
  }

track_rel_done:

  if (!LIVES_IS_PLAYING) mt_sensitise(mt);
  mt->hotspot_x = mt->hotspot_y = 0;
  mt->putative_block = NULL;

  lives_set_cursor_style(LIVES_CURSOR_NORMAL, NULL);
  lives_widget_set_sensitive(mt->mm_menuitem, TRUE);

  if (needs_idlefunc || (!did_backup && mt->auto_changed)) {
    mt->idlefunc = mt_idle_add(mt);
  }

  return TRUE;
}


boolean on_track_header_click(LiVESWidget * widget, LiVESXEventButton * event, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  if (!mainw->interactive) return FALSE;
  if (mt->opts.mouse_mode == MOUSE_MODE_SELECT) {
    mouse_select_start(widget, event, mt);
    lives_signal_handler_unblock(widget, mt->mouse_mot1);
  }
  return TRUE;
}


boolean on_track_header_release(LiVESWidget * widget, LiVESXEventButton * event, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  if (!mainw->interactive) return FALSE;
  if (mt->opts.mouse_mode == MOUSE_MODE_SELECT) {
    mouse_select_end(widget, event, mt);
    lives_signal_handler_block(widget, mt->mouse_mot1);
  }
  return TRUE;
}


boolean on_track_between_click(LiVESWidget * widget, LiVESXEventButton * event, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  if (!mainw->interactive) return FALSE;
  if (mt->opts.mouse_mode == MOUSE_MODE_SELECT) {
    mouse_select_start(widget, event, mt);
    lives_signal_handler_unblock(mt->tl_eventbox, mt->mouse_mot2);
  }
  return TRUE;
}


boolean on_track_between_release(LiVESWidget * widget, LiVESXEventButton * event, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  if (!mainw->interactive) return FALSE;
  if (mt->opts.mouse_mode == MOUSE_MODE_SELECT) {
    mouse_select_end(widget, event, mt);
    lives_signal_handler_block(mt->tl_eventbox, mt->mouse_mot2);
  }
  return TRUE;
}


boolean on_track_click(LiVESWidget * eventbox, LiVESXEventButton * event, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;

  track_rect *block;

  double timesecs;

  int x, y;
  int track;
  int filenum = -1;

  //doubleclick=FALSE;

  if (!mainw->interactive) return FALSE;

  mt->aud_track_selected = is_audio_eventbox(eventbox);

  lives_widget_set_sensitive(mt->mm_menuitem, FALSE);

  lives_widget_get_pointer((LiVESXDevice *)mainw->mgeom[prefs->gui_monitor > 0 ? prefs->gui_monitor - 1 : 0].mouse_device,
                           eventbox, &x, &y);

  timesecs = get_time_from_x(mt, x + mt->hotspot_x);

  if (cfile->achans == 0 || mt->audio_draws == NULL || (mt->opts.back_audio_tracks == 0 || eventbox != mt->audio_draws->data))
    track = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "layer_number"));
  else track = -1;
  block = mt->putative_block = get_block_from_time(eventbox, timesecs, mt);

  unselect_all(mt); // set all blocks unselected

  if (mt->opts.mouse_mode == MOUSE_MODE_SELECT && event->type == LIVES_BUTTON_PRESS) {
    mouse_select_start(eventbox, event, mt);
    lives_signal_handler_unblock(mt->tl_eventbox, mt->mouse_mot2);
  } else {
    if (lives_event_get_time((LiVESXEvent *)event) - last_press_time < LIVES_DCLICK_TIME) {
      doubleclick = TRUE;
      return TRUE;
    } else {
      // single click, TODO - locate the frame for the track in event_list
      if (event->button == 1) {
        if (!LIVES_IS_PLAYING) {
          mt->fm_edit_event = NULL;
          mt_tl_move(mt, timesecs);
        }
      }

      // for a double click, gdk normally sends 2 single click events,
      // followed by a double click

      // calling mt_tl_move() causes any double click to be triggered during
      // the second single click and then we return here
      // however, this is quite useful as we can skip the next bit

      if (event->time != dclick_time) {
        show_track_info(mt, eventbox, track, timesecs);
        if (block != NULL) {
          if (!is_audio_eventbox(eventbox))
            filenum = get_frame_event_clip(block->start_event, track);
          else filenum = get_audio_frame_clip(block->start_event, -1);
          if (filenum != mainw->scrap_file && filenum != mainw->ascrap_file) {
            mt->clip_selected = mt_clip_from_file(mt, filenum);
            mt_clip_select(mt, TRUE);
          }

          if (!mt->is_rendering) {
            double start_secs, end_secs;

            LiVESXScreen *screen;
            int abs_x, abs_y;

            int ebwidth = lives_widget_get_allocation_width(mt->timeline);

            double width = ((end_secs = (get_event_timecode(block->end_event) / TICKS_PER_SECOND_DBL)) -
                            (start_secs = (get_event_timecode(block->start_event) / TICKS_PER_SECOND_DBL)) + 1. / mt->fps);
            int height;

            // start point must be on timeline to move a block
            if (block != NULL && (mt->tl_min * TICKS_PER_SECOND_DBL > get_event_timecode(block->start_event))) {
              mt->putative_block = NULL;
              return TRUE;
            }
            if (event->button == 1) {
              if (!is_audio_eventbox(eventbox))
                height = lives_widget_get_allocation_height(LIVES_WIDGET(lives_list_nth_data(mt->video_draws, 0)));
              else height = lives_widget_get_allocation_height(LIVES_WIDGET(mt->audio_draws->data));

              width = (width / (mt->tl_max - mt->tl_min) * (double)ebwidth);
              if (width > ebwidth) width = ebwidth;
              if (width < 2) width = 2;

              mt->hotspot_x = x - (int)((ebwidth * ((double)start_secs - mt->tl_min) / (mt->tl_max - mt->tl_min)) + .5);
              mt->hotspot_y = y;
              lives_display_get_pointer((LiVESXDevice *)mainw->mgeom[prefs->gui_monitor > 0 ? prefs->gui_monitor - 1 : 0].mouse_device,
                                        mt->display, &screen, &abs_x, &abs_y, NULL);
              lives_display_warp_pointer((LiVESXDevice *)mainw->mgeom[prefs->gui_monitor > 0 ? prefs->gui_monitor - 1 : 0].mouse_device,
                                         mt->display, screen, abs_x - mt->hotspot_x, abs_y - y + height / 2);
              if (track >= 0 && !mt->aud_track_selected) {
                if (mainw->files[filenum]->clip_type == CLIP_TYPE_FILE) {
                  lives_clip_data_t *cdata = ((lives_decoder_t *)mainw->files[filenum]->ext_src)->cdata;
                  if (cdata != NULL && !(cdata->seek_flag & LIVES_SEEK_FAST)) {
                    mt_set_cursor_style(mt, LIVES_CURSOR_VIDEO_BLOCK, width, height, filenum, 0, height / 2);
                  } else {
                    mt_set_cursor_style(mt, LIVES_CURSOR_BLOCK, width, height, filenum, 0, height / 2);
                  }
                } else {
                  mt_set_cursor_style(mt, LIVES_CURSOR_BLOCK, width, height, filenum, 0, height / 2);
                }
              } else mt_set_cursor_style(mt, LIVES_CURSOR_AUDIO_BLOCK, width, height, filenum, 0, height / 2);
            }
          }
        }
      } else {
        mt->putative_block = NULL; // please don't move the block
      }
    }
  }

  mt->current_track = track;
  track_select(mt);

  if (event->button == 3 && !LIVES_IS_PLAYING) {
    lives_widget_set_sensitive(mt->mm_menuitem, TRUE);
    mt->context_time = timesecs;
    if (block != NULL) {
      // context menu for a selected block
      mt->putative_block = block;
      do_block_context(mt, event, block);
      return TRUE;
    } else {
      do_track_context(mt, event, timesecs, track);
      return TRUE;
    }
  }

  last_press_time = lives_event_get_time((LiVESXEvent *)event);

  return TRUE;
}


boolean on_track_move(LiVESWidget * widget, LiVESXEventMotion * event, livespointer user_data) {
  // used for mouse mode SELECT
  lives_mt *mt = (lives_mt *)user_data;
  if (!mainw->interactive) return FALSE;
  if (mt->opts.mouse_mode == MOUSE_MODE_SELECT) mouse_select_move(widget, event, mt);
  return TRUE;
}


boolean on_track_header_move(LiVESWidget * widget, LiVESXEventMotion * event, livespointer user_data) {
  // used for mouse mode SELECT
  lives_mt *mt = (lives_mt *)user_data;
  if (!mainw->interactive) return FALSE;
  if (mt->opts.mouse_mode == MOUSE_MODE_SELECT) mouse_select_move(widget, event, mt);
  return TRUE;
}


void unpaint_line(lives_mt * mt, LiVESWidget * eventbox) {
  int xoffset;
  int ebwidth;

  if (mt->redraw_block) return; // don't update during expose event, otherwise we might leave lines
  if (!lives_widget_is_visible(eventbox)) return;
  if (!mt->is_ready) return;
  if ((xoffset = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "has_line"))) < 0) return;

  ebwidth = lives_widget_get_allocation_width(mt->timeline);

  if (xoffset < ebwidth) {
    lives_widget_queue_draw_area(eventbox, xoffset - 4, 0, 9, lives_widget_get_allocation_height(eventbox));
    lives_widget_process_updates(eventbox, TRUE);
  }

  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(eventbox), "has_line", LIVES_INT_TO_POINTER(-1));
}


void unpaint_lines(lives_mt * mt) {
  if (!mt->is_ready) return;
  unpaint_line(mt, mt->timeline_table);
  return;
}


static void paint_line(lives_mt * mt, LiVESWidget * eventbox, int offset, double currtime) {
  lives_painter_t *cr;

  /* if (!LIVES_IS_PLAYING) { */
  /*   if (lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "has_line") != NULL) { */
  /*     int xoffset = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "has_line")); */
  /*     if (xoffset >= 0 && xoffset != offset) unpaint_line(mt, eventbox); */
  /*   } */
  /* } */

  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(eventbox), "has_line", LIVES_INT_TO_POINTER(offset));

  cr = lives_painter_create_from_widget(eventbox);

  if (lives_painter_set_operator(cr, LIVES_PAINTER_OPERATOR_DIFFERENCE))
    lives_painter_set_source_rgb(cr, 1., 1., 1.);
  else
    lives_painter_set_source_rgb(cr, 0., 0., 0.);

  lives_painter_set_line_width(cr, 1.);

  lives_painter_rectangle(cr, offset, 0., 1., lives_widget_get_allocation_height(eventbox));

  lives_painter_fill(cr);
  lives_painter_destroy(cr);
}


static void paint_lines(lives_mt * mt, double currtime, boolean unpaint) {
  int ebwidth;
  int offset, off_x;

  if (!mt->is_ready) return;

  ebwidth = lives_widget_get_allocation_width(mt->timeline);

  if (currtime < mt->tl_min || currtime > mt->tl_max) return;

  offset = (currtime - mt->tl_min) / (mt->tl_max - mt->tl_min) * (double)ebwidth;

  if (unpaint) unpaint_line(mt, mt->timeline_table);
  lives_widget_get_position(mt->timeline_eb, &off_x, NULL);
  offset += off_x;

  if (offset > off_x && offset < ebwidth) {
    paint_line(mt, mt->timeline_table, offset, currtime);
  }
}


void animate_multitrack(lives_mt * mt) {
  // update timeline pointer(s)
  double currtime = mainw->currticks / TICKS_PER_SECOND_DBL;
  double tl_page;

  int offset, offset_old;

  int ebwidth = lives_widget_get_allocation_width(mt->timeline);

  update_timecodes(mt, currtime);

  offset = (currtime - mt->tl_min) / (mt->tl_max - mt->tl_min) * (double)ebwidth;
  offset_old = (lives_ruler_get_value(LIVES_RULER(mt->timeline)) - mt->tl_min) / (mt->tl_max - mt->tl_min) * (double)ebwidth;

  mt->ptr_time = lives_ruler_set_value(LIVES_RULER(mt->timeline), currtime);

  if (offset == offset_old) return;

  if (mt->opts.follow_playback) {
    if (currtime > (mt->tl_min + ((tl_page = mt->tl_max - mt->tl_min)) * .85) &&
        event_list_get_end_secs(mt->event_list) > mt->tl_max) {
      // scroll right one page
      mt->tl_min += tl_page * .85;
      mt->tl_max += tl_page * .85;
      mt_zoom(mt, -1.);
    }
  }

  if ((offset < 0. && offset_old < 0.) || (offset > (double)ebwidth && offset_old > (double)ebwidth)) return;

  lives_widget_queue_draw(mt->timeline);
  if (mt->redraw_block) return; // don't update during expose event, otherwise we might leave lines

  paint_lines(mt, currtime, TRUE);
}


////////////////////////////////////////////////////
// menuitem callbacks

static boolean multitrack_end(LiVESMenuItem * menuitem, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  return multitrack_delete(mt, !(prefs->warning_mask & WARN_MASK_EXIT_MT) || menuitem == NULL);
}


// callbacks for future adding to osc.c
void multitrack_end_cb(LiVESMenuItem * menuitem, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  if (mt->is_rendering) return;
  multitrack_end(menuitem, user_data);
}


void insert_here_cb(LiVESMenuItem * menuitem, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  if (mt->is_rendering) return;
  multitrack_insert(NULL, user_data);
}


void insert_at_ctx_cb(LiVESMenuItem * menuitem, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  if (mt->is_rendering) return;
  mt->use_context = TRUE;
  multitrack_insert(NULL, user_data);
}


void edit_start_end_cb(LiVESMenuItem * menuitem, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  if (mt->is_rendering) return;
  multitrack_adj_start_end(NULL, user_data);
}


void close_clip_cb(LiVESMenuItem * menuitem, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  if (mt->is_rendering) return;
  on_close_activate(NULL, NULL);
}


void show_clipinfo_cb(LiVESMenuItem * menuitem, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  int current_file = mainw->current_file;
  if (mt->file_selected != -1) {
    mainw->current_file = mt->file_selected;
    on_show_file_info_activate(NULL, NULL);
    mainw->current_file = current_file;
  }
}


void insert_audio_here_cb(LiVESMenuItem * menuitem, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  if (mt->is_rendering) return;
  multitrack_audio_insert(NULL, user_data);
}


void insert_audio_at_ctx_cb(LiVESMenuItem * menuitem, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  if (mt->is_rendering) return;
  mt->use_context = TRUE;
  multitrack_audio_insert(NULL, user_data);
}


void delete_block_cb(LiVESMenuItem * menuitem, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  if (mt->is_rendering) return;
  on_delblock_activate(NULL, user_data);
}


void selblock_cb(LiVESMenuItem * menuitem, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  select_block(mt);
  mt->putative_block = NULL;
}


void list_fx_here_cb(LiVESMenuItem * menuitem, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  mt_tl_move(mt, mt->context_time);
  mt->context_time = -1.;
  on_mt_list_fx_activate(NULL, user_data);
}


///////////////////////////////////////////////////////////

static void on_move_fx_changed(LiVESMenuItem * menuitem, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  mt->opts.move_effects = !mt->opts.move_effects;
}


static void select_all_time(LiVESMenuItem * menuitem, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  mt->region_start = 0.;
  mt->region_end = get_event_timecode(get_last_event(mt->event_list));
  on_timeline_release(mt->timeline_reg, NULL, mt);
}


static void select_from_zero_time(LiVESMenuItem * menuitem, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  if (mt->region_start == 0. && mt->region_end == 0.) mt->region_end = mt->ptr_time;
  mt->region_start = 0.;
  on_timeline_release(mt->timeline_reg, NULL, mt);
}


static void select_to_end_time(LiVESMenuItem * menuitem, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  if (mt->region_start == 0. && mt->region_end == 0.) mt->region_start = mt->ptr_time;
  mt->region_end = get_event_timecode(get_last_event(mt->event_list));
  on_timeline_release(mt->timeline_reg, NULL, mt);
}


static void select_all_vid(LiVESMenuItem * menuitem, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  LiVESWidget *eventbox, *checkbutton;
  LiVESList *vdr = mt->video_draws;

  int current_track = mt->current_track;
  int i = 0;

  lives_signal_handler_block(mt->select_track, mt->seltrack_func);
  if (!lives_check_menu_item_get_active(LIVES_CHECK_MENU_ITEM(mt->select_track)))
    lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mt->select_track), TRUE);
  lives_signal_handler_unblock(mt->select_track, mt->seltrack_func);

  while (vdr != NULL) {
    eventbox = (LiVESWidget *)vdr->data;
    checkbutton = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "checkbutton");

#ifdef ENABLE_GIW
    if (!prefs->lamp_buttons) {
#endif
      if (!lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(checkbutton)))
        lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(checkbutton), TRUE);
#ifdef ENABLE_GIW
    } else {
      if (!giw_led_get_mode(GIW_LED(checkbutton))) giw_led_set_mode(GIW_LED(checkbutton), TRUE);
    }
#endif
    mt->current_track = i++;
    // we need to call this since it appears that checkbuttons on hidden tracks don't get updated until shown
    on_seltrack_activate(LIVES_MENU_ITEM(mt->select_track), mt);
    vdr = vdr->next;
  }
  mt->current_track = current_track;
}


static void select_no_vid(LiVESMenuItem * menuitem, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  LiVESWidget *eventbox, *checkbutton;
  LiVESList *vdr = mt->video_draws;

  int current_track = mt->current_track;
  int i = 0;

  lives_signal_handler_block(mt->select_track, mt->seltrack_func);
  if (lives_check_menu_item_get_active(LIVES_CHECK_MENU_ITEM(mt->select_track)))
    lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mt->select_track), FALSE);
  lives_signal_handler_unblock(mt->select_track, mt->seltrack_func);

  while (vdr != NULL) {
    eventbox = (LiVESWidget *)vdr->data;
    checkbutton = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "checkbutton");

#ifdef ENABLE_GIW
    if (!prefs->lamp_buttons) {
#endif
      if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(checkbutton)))
        lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(checkbutton), FALSE);
#ifdef ENABLE_GIW
    } else {
      if (giw_led_get_mode(GIW_LED(checkbutton))) giw_led_set_mode(GIW_LED(checkbutton), FALSE);
    }
#endif
    mt->current_track = i++;
    // we need to call this since it appears that checkbuttons on hidden tracks don't get updated until shown
    on_seltrack_activate(LIVES_MENU_ITEM(mt->select_track), mt);
    vdr = vdr->next;
  }
  mt->current_track = current_track;
}


static void mt_fplay_toggled(LiVESMenuItem * menuitem, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  mt->opts.follow_playback = !mt->opts.follow_playback;
  //lives_widget_set_sensitive(mt->follow_play,mt->opts.follow_playback);
}


static void mt_render_vid_toggled(LiVESMenuItem * menuitem, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  mt->opts.render_vidp = !mt->opts.render_vidp;
  lives_widget_set_sensitive(mt->render_aud, mt->opts.render_vidp);
}


static void mt_render_aud_toggled(LiVESMenuItem * menuitem, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  mt->opts.render_audp = !mt->opts.render_audp;
  lives_widget_set_sensitive(mt->render_vid, mt->opts.render_audp);
  lives_widget_set_sensitive(mt->normalise_aud, mt->opts.render_audp);
}


static void mt_norm_aud_toggled(LiVESMenuItem * menuitem, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  mt->opts.normalise_audp = !mt->opts.normalise_audp;
}


static void mt_view_audio_toggled(LiVESMenuItem * menuitem, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  mt->opts.show_audio = lives_check_menu_item_get_active(LIVES_CHECK_MENU_ITEM(menuitem));

  if (!mt->opts.show_audio) lives_widget_object_set_data(LIVES_WIDGET_OBJECT(mt->audio_draws->data), "hidden",
        LIVES_INT_TO_POINTER(TRACK_I_HIDDEN_USER));
  else lives_widget_object_set_data(LIVES_WIDGET_OBJECT(mt->audio_draws->data), "hidden", LIVES_INT_TO_POINTER(0));

  scroll_tracks(mt, mt->top_track, FALSE);
  track_select(mt);
}


static void mt_ign_ins_sel_toggled(LiVESMenuItem * menuitem, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  mt->opts.ign_ins_sel = lives_check_menu_item_get_active(LIVES_CHECK_MENU_ITEM(menuitem));
}


static void remove_gaps_inner(LiVESMenuItem * menuitem, livespointer user_data, boolean only_first) {
  lives_mt *mt = (lives_mt *)user_data;

  weed_timecode_t offset = 0;
  weed_timecode_t tc, new_tc, tc_last, new_tc_last, tc_first, block_tc;

  LiVESList *vsel = mt->selected_tracks;
  LiVESList *track_sel;

  LiVESWidget *eventbox;

  track_rect *block = NULL;

  boolean did_backup = mt->did_backup;
  boolean audio_done = FALSE;
  boolean needs_idlefunc = FALSE;

  int track;
  int filenum;

  if (mt->idlefunc > 0) {
    needs_idlefunc = TRUE;
    lives_source_remove(mt->idlefunc);
    mt->idlefunc = 0;
  }

  mt_backup(mt, MT_UNDO_REMOVE_GAPS, 0);

  //go through selected tracks, move each block as far left as possible

  tc_last = q_gint64(mt->region_end * TICKS_PER_SECOND_DBL, mt->fps);

  while (vsel != NULL || (mt->current_track == -1 && !audio_done)) {
    offset = 0;
    if (mt->current_track > -1) {
      track = LIVES_POINTER_TO_INT(vsel->data);
      eventbox = (LiVESWidget *)lives_list_nth_data(mt->video_draws, track);
    } else {
      track = -1;
      eventbox = (LiVESWidget *)mt->audio_draws->data;
    }
    tc = mt->region_start * TICKS_PER_SECOND_DBL;
    tc = q_gint64(tc, mt->fps);

    if (mt->opts.grav_mode != GRAV_MODE_RIGHT) {
      // adjust the region so it begins after any first partially contained block
      block = get_block_before(eventbox, tc / TICKS_PER_SECOND_DBL, TRUE);
      if (block != NULL) {
        new_tc = q_gint64(get_event_timecode(block->end_event) + (double)(track > -1) * TICKS_PER_SECOND_DBL / mt->fps, mt->fps);
        if (new_tc > tc) tc = new_tc;
      }
    } else {
      // adjust the region so it ends before any last partially contained block
      block = get_block_after(eventbox, tc_last / TICKS_PER_SECOND_DBL, TRUE);
      if (block != NULL) {
        new_tc_last = q_gint64(get_event_timecode(block->start_event) - (double)(track > -1)
                               * TICKS_PER_SECOND_DBL / mt->fps, mt->fps);
        if (new_tc_last < tc_last) tc_last = new_tc_last;
      }
    }

    if (mt->opts.grav_mode != GRAV_MODE_RIGHT) {
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

      while (tc <= tc_last) {
        block = get_block_after(eventbox, tc / TICKS_PER_SECOND_DBL, FALSE);
        if (block == NULL) break;

        new_tc = get_event_timecode(block->start_event);
        if (new_tc > tc_last) break;

        if (tc < new_tc) {
          // move this block to tc
          if (offset > 0) tc = q_gint64(new_tc - offset, mt->fps);
          filenum = get_frame_event_clip(block->start_event, track);
          mt->clip_selected = mt_clip_from_file(mt, filenum);
          mt_clip_select(mt, FALSE);
          if (!mt->did_backup) mt_backup(mt, MT_UNDO_REMOVE_GAPS, 0);

          // save current selected_tracks, move_block may change this
          track_sel = mt->selected_tracks;
          mt->selected_tracks = NULL;
          block = move_block(mt, block, tc / TICKS_PER_SECOND_DBL, track, track);
          if (mt->selected_tracks != NULL) lives_list_free(mt->selected_tracks);
          mt->selected_tracks = track_sel;
          if (only_first && offset == 0) offset = new_tc - tc;
        }
        tc = q_gint64(get_event_timecode(block->end_event) + (double)(track > -1) * TICKS_PER_SECOND_DBL / mt->fps, mt->fps);
      }
      if (mt->current_track > -1) vsel = vsel->next;
      else audio_done = TRUE;
    } else {
      // moving right
      // here we do the reverse:
      // find last block in range. move it right so it ends at tc
      // then we adjust tc to the start of the block + 1 frame
      // and continue until we reach the end of the region

      tc_first = tc;
      tc = tc_last;
      while (tc >= tc_first) {
        block = get_block_before(eventbox, tc / TICKS_PER_SECOND_DBL, FALSE);
        if (block == NULL) break;

        new_tc = get_event_timecode(block->end_event);
        if (new_tc < tc_first) break;

        // subtract the length of the block to get the start point
        block_tc = new_tc - get_event_timecode(block->start_event) + (double)(track > -1) * TICKS_PER_SECOND_DBL / mt->fps;

        if (tc > new_tc) {
          // move this block to tc
          if (offset > 0) tc = q_gint64(new_tc - block_tc + offset, mt->fps);
          else tc = q_gint64(tc - block_tc, mt->fps);
          filenum = get_frame_event_clip(block->start_event, track);
          mt->clip_selected = mt_clip_from_file(mt, filenum);
          mt_clip_select(mt, FALSE);
          if (!mt->did_backup) mt_backup(mt, MT_UNDO_REMOVE_GAPS, 0);

          // save current selected_tracks, move_block may change this
          track_sel = mt->selected_tracks;
          mt->selected_tracks = NULL;
          block = move_block(mt, block, tc / TICKS_PER_SECOND_DBL, track, track);
          if (mt->selected_tracks != NULL) lives_list_free(mt->selected_tracks);
          mt->selected_tracks = track_sel;
          if (only_first && offset == 0) offset = tc - new_tc + block_tc;
        }
        tc = q_gint64(get_event_timecode(block->start_event) - (double)(track > -1) * TICKS_PER_SECOND_DBL / mt->fps, mt->fps);
      }
      if (mt->current_track > -1) vsel = vsel->next;
      else audio_done = TRUE;
    }
  }

  if (!did_backup) {
    if (mt->avol_fx != -1 && (block == NULL || block->next == NULL) && mt->audio_draws != NULL &&
        mt->audio_draws->data != NULL && get_first_event(mt->event_list) != NULL) {
      apply_avol_filter(mt);
    }
  }

  mt->did_backup = did_backup;
  if (!did_backup && mt->framedraw != NULL && mt->current_rfx != NULL && mt->init_event != NULL &&
      mt->poly_state == POLY_PARAMS && weed_plant_has_leaf(mt->init_event, WEED_LEAF_IN_TRACKS)) {
    tc = q_gint64(lives_spin_button_get_value(LIVES_SPIN_BUTTON(mt->node_spinbutton))
                  * TICKS_PER_SECOND_DBL + get_event_timecode(mt->init_event), mt->fps);
    get_track_index(mt, tc);
  }

  if (!did_backup) {
    mt->did_backup = FALSE;
    if (needs_idlefunc || (!did_backup && mt->auto_changed)) mt->idlefunc = mt_idle_add(mt);
    if (prefs->mt_auto_back == 0) mt_auto_backup(mt);
  }
}


void remove_first_gaps(LiVESMenuItem * menuitem, livespointer user_data) {
  // remove first gaps in selected time/tracks
  // if gravity is Right then we remove last gaps instead

  remove_gaps_inner(menuitem, user_data, TRUE);
}


void remove_gaps(LiVESMenuItem * menuitem, livespointer user_data) {
  remove_gaps_inner(menuitem, user_data, FALSE);
}


static void split_block(lives_mt * mt, track_rect * block, weed_timecode_t tc, int track, boolean no_recurse) {
  weed_plant_t *event = block->start_event;
  weed_plant_t *start_event = event;
  weed_plant_t *old_end_event = block->end_event;
  int frame = 0, clip;
  LiVESWidget *eventbox;
  track_rect *new_block;
  weed_timecode_t offset_start;
  double seek, new_seek, vel;

  tc = q_gint64(tc, mt->fps);

  if (block == NULL) return;

  mt->no_expose = TRUE;

  while (get_event_timecode(event) < tc) event = get_next_event(event);
  block->end_event = track >= 0 ? get_prev_event(event) : event;
  if (!WEED_EVENT_IS_FRAME(block->end_event)) block->end_event = get_prev_frame_event(event);

  if (!WEED_EVENT_IS_FRAME(event)) event = get_next_frame_event(event);
  eventbox = block->eventbox;

  if (!is_audio_eventbox(eventbox)) {
    if (!no_recurse) {
      // if we have an audio block, split it too
      LiVESWidget *aeventbox = LIVES_WIDGET(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "atrack"));
      if (aeventbox != NULL) {
        track_rect *ablock = get_block_from_time(aeventbox, tc / TICKS_PER_SECOND_DBL + 1. / mt->fps, mt);
        if (ablock != NULL) split_block(mt, ablock, tc + TICKS_PER_SECOND_DBL / mt->fps, track, TRUE);
      }
    }
    frame = get_frame_event_frame(event, track);
    clip = get_frame_event_clip(event, track);
  } else {
    if (!no_recurse) {
      // if we have a video block, split it too
      LiVESWidget *oeventbox = LIVES_WIDGET(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "owner"));
      if (oeventbox != NULL) split_block(mt, get_block_from_time(oeventbox, tc / TICKS_PER_SECOND_DBL - 1. / mt->fps, mt),
                                           tc - TICKS_PER_SECOND_DBL / mt->fps, track, TRUE);
    }
    clip = get_audio_frame_clip(start_event, track);
    seek = get_audio_frame_seek(start_event, track);
    vel = get_audio_frame_vel(start_event, track);
    event = block->end_event;
    new_seek = seek + (get_event_timecode(event) / TICKS_PER_SECOND_DBL
                       - get_event_timecode(start_event) / TICKS_PER_SECOND_DBL) * vel;
    insert_audio_event_at(event, track, clip, new_seek, vel);
  }

  if (block->ordered ||
      (is_audio_eventbox(eventbox))) offset_start = block->offset_start - get_event_timecode(start_event) + get_event_timecode(event);
  else offset_start = calc_time_from_frame(clip, frame) * TICKS_PER_SECOND_DBL;

  new_block = add_block_start_point(LIVES_WIDGET(eventbox), tc, clip, offset_start, event, block->ordered);
  new_block->end_event = old_end_event;

  mt->no_expose = FALSE;

  redraw_eventbox(mt, eventbox);
  paint_lines(mt, mt->ptr_time, TRUE);
}


static void insgap_inner(lives_mt * mt, int tnum, boolean is_sel, int passnm) {
  // insert a gap in track tnum

  // we will process in 2 passes

  // pass 1

  // if there is a block at start time, we split it
  // then we move the frame events for this track, inserting blanks if necessary, and we update all our blocks

  // pass 2

  // FILTER_INITs and FILTER_DEINITS - we move the filter init/deinit if "move effects with blocks" is selected and all in_tracks are in the tracks to be moved
  // (transitions may have one non-moving track)

  track_rect *sblock, *block, *ablock = NULL;
  LiVESWidget *eventbox;
  LiVESList *slist;
  weed_plant_t *event, *new_event = NULL, *last_frame_event;
  weed_plant_t *init_event;
  weed_timecode_t tc, new_tc;
  weed_timecode_t start_tc, new_init_tc, init_tc;
  double aseek = 0., avel = 0., *audio_seeks;
  double end_secs;
  boolean found;
  int clip, *clips, *new_clips;
  int64_t frame, *frames, *new_frames;
  int xnumclips, numclips, naclips;
  int aclip = 0, *audio_clips;
  int nintracks, *in_tracks;
  int notmatched;
  register int i;

  switch (passnm) {
  case 1:
    // frames and blocks
    if (tnum >= 0) eventbox = (LiVESWidget *)lives_list_nth_data(mt->video_draws, tnum);
    else eventbox = (LiVESWidget *)mt->audio_draws->data;
    tc = q_dbl(mt->region_start, mt->fps);
    sblock = get_block_from_time(eventbox, mt->region_start, mt);

    if (sblock != NULL) {
      split_block(mt, sblock, tc, tnum, FALSE);
      sblock = sblock->next;
    } else {
      sblock = get_block_after(eventbox, mt->region_start, FALSE);
    }

    if (sblock == NULL) return;

    block = sblock;
    while (block->next != NULL) block = block->next;
    event = block->end_event;

    if (tnum >= 0 && mt->opts.pertrack_audio) {
      LiVESWidget *aeventbox = LIVES_WIDGET(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "atrack"));
      if (aeventbox != NULL) {
        ablock = get_block_after(aeventbox, mt->region_start, FALSE);
        if (ablock != NULL) {
          while (ablock->next != NULL) ablock = ablock->next;
          event = ablock->end_event;
        }
      }
    }

    while (event != NULL) {
      if (WEED_EVENT_IS_FRAME(event)) {
        tc = get_event_timecode(event);
        new_tc = q_gint64(tc + (mt->region_end - mt->region_start) * TICKS_PER_SECOND_DBL, mt->fps);
        new_event = event;

        if (tnum >= 0 && tc <= get_event_timecode(block->end_event)) {
          frame = get_frame_event_frame(event, tnum);
          clip = get_frame_event_clip(event, tnum);

          if ((new_event = get_frame_event_at(mt->event_list, new_tc, event, TRUE)) == NULL) {
            last_frame_event = get_last_frame_event(mt->event_list);
            mt->event_list = add_blank_frames_up_to(mt->event_list, last_frame_event, new_tc, mt->fps);
            new_event = get_last_frame_event(mt->event_list);
          }

          remove_frame_from_event(mt->event_list, event, tnum);

          clips = weed_get_int_array_counted(new_event, WEED_LEAF_CLIPS, &numclips);
          xnumclips = numclips;
          if (numclips < tnum + 1) xnumclips = tnum + 1;

          new_clips = (int *)lives_malloc(xnumclips * sizint);
          new_frames = (int64_t *)lives_malloc(xnumclips * 8);

          frames = weed_get_int64_array(new_event, WEED_LEAF_FRAMES, NULL);

          for (i = 0; i < xnumclips; i++) {
            if (i == tnum) {
              new_clips[i] = clip;
              new_frames[i] = frame;
            } else {
              if (i < numclips) {
                new_clips[i] = clips[i];
                new_frames[i] = frames[i];
              } else {
                new_clips[i] = -1;
                new_frames[i] = 0;
              }
            }
          }

          weed_set_int_array(new_event, WEED_LEAF_CLIPS, xnumclips, new_clips);
          weed_set_int64_array(new_event, WEED_LEAF_FRAMES, xnumclips, new_frames);

          lives_free(clips);
          lives_free(frames);
          lives_free(new_clips);
          lives_free(new_frames);
        }

        if (WEED_EVENT_IS_AUDIO_FRAME(event)) {
          if ((new_event = get_frame_event_at(mt->event_list, new_tc, event, TRUE)) == NULL) {
            last_frame_event = get_last_frame_event(mt->event_list);
            mt->event_list = add_blank_frames_up_to(mt->event_list, last_frame_event, q_gint64(new_tc, mt->fps), mt->fps);
            new_event = get_last_frame_event(mt->event_list);
          }

          audio_clips = weed_get_int_array_counted(event, WEED_LEAF_AUDIO_CLIPS, &naclips);
          audio_seeks = weed_get_double_array(event, WEED_LEAF_AUDIO_SEEKS, NULL);

          for (i = 0; i < naclips; i += 2) {
            if (audio_clips[i] == tnum) {
              aclip = audio_clips[i + 1];
              aseek = audio_seeks[i];
              avel = audio_seeks[i + 1];
            }
          }

          lives_free(audio_clips);
          lives_free(audio_seeks);

          remove_audio_for_track(event, tnum);
          insert_audio_event_at(new_event, tnum, aclip, aseek, avel);

          if (mt->avol_fx != -1) {
            apply_avol_filter(mt);
          }

        }

        if (new_event != event) {

          if (ablock != NULL) {
            if (event == ablock->end_event) ablock->end_event = new_event;
            else if (event == ablock->start_event) {
              ablock->start_event = new_event;
            }
          }

          if (event == block->end_event) block->end_event = new_event;
          else if (event == block->start_event) {
            block->start_event = new_event;
            if (block == sblock) {
              if (tnum < 0 || ablock != NULL) {
                if (ablock != NULL) block = ablock;
                if (block->prev != NULL && block->prev->end_event == event) {
                  // audio block was split, need to add a new "audio off" event
                  insert_audio_event_at(event, tnum, aclip, 0., 0.);
                }
                if (mt->avol_fx != -1) {
                  apply_avol_filter(mt);
                }
              }
              mt_fixup_events(mt, event, new_event);
              redraw_eventbox(mt, eventbox);
              paint_lines(mt, mt->ptr_time, TRUE);
              return;
            }
            block = block->prev;
          }
          if (ablock != NULL && ablock->start_event == new_event) {
            ablock = ablock->prev;
            if (ablock != NULL && event == ablock->end_event) ablock->end_event = new_event;
          }
          mt_fixup_events(mt, event, new_event);
        }
      }
      if (tnum >= 0) event = get_prev_event(event);
      else {
        if (new_event == block->end_event) event = block->start_event;
        else event = block->end_event; // we will have moved to the previous block
      }
    }

    break;

  case 2:
    // FILTER_INITs
    start_tc = q_gint64(mt->region_start * TICKS_PER_SECOND_DBL, mt->fps);
    event = get_last_event(mt->event_list);

    while (event != NULL && (tc = get_event_timecode(event)) >= start_tc) {
      if (WEED_EVENT_IS_FILTER_DEINIT(event)) {
        init_event = (weed_plant_t *)weed_get_voidptr_value(event, WEED_LEAF_INIT_EVENT, NULL);

        if (init_event == mt->avol_init_event) {
          event = get_prev_event(event);
          continue;
        }

        // see if all of this filter`s in_tracks were moved
        in_tracks = weed_get_int_array_counted(init_event, WEED_LEAF_IN_TRACKS, &nintracks);

        if (!is_sel) {
          if ((nintracks == 1 && in_tracks[0] != mt->current_track) || (nintracks == 2 && in_tracks[0] != mt->current_track &&
              in_tracks[1] != mt->current_track)) {
            event = get_prev_event(event);
            continue;
          }
        } else {
          for (i = 0; i < nintracks; i++) {
            slist = mt->selected_tracks;
            found = FALSE;
            notmatched = 0;
            while (slist != NULL && !found) {
              if (LIVES_POINTER_TO_INT(slist->data) == in_tracks[i]) found = TRUE;
              slist = slist->next;
            }
            if (!found) {
              if (nintracks != 2 || notmatched > 0) return;
              notmatched = 1;
            }
          }
        }

        lives_free(in_tracks);

        // move filter_deinit
        new_tc = q_gint64(tc + (mt->region_end - mt->region_start) * TICKS_PER_SECOND_DBL, mt->fps);
        move_filter_deinit_event(mt->event_list, new_tc, event, mt->fps, TRUE);

        init_tc = get_event_timecode(init_event);

        if (init_tc >= start_tc) {
          // move filter init
          new_init_tc = q_gint64(init_tc + (mt->region_end - mt->region_start) * TICKS_PER_SECOND_DBL, mt->fps);
          move_filter_init_event(mt->event_list, new_init_tc, init_event, mt->fps);
        }

        // for a transition where only one track moved, pack around the overlap

        if (nintracks == 2) {
          move_event_left(mt->event_list, event, TRUE, mt->fps);
          if (init_tc >= start_tc && init_event != mt->avol_init_event) move_event_right(mt->event_list, init_event, TRUE, mt->fps);
        }
      }
      event = get_prev_event(event);
    }
    break;
  }
  end_secs = event_list_get_end_secs(mt->event_list);
  if (end_secs > mt->end_secs) {
    set_timeline_end_secs(mt, end_secs);
  }
}


void on_insgap_sel_activate(LiVESMenuItem * menuitem, livespointer user_data) {

  lives_mt *mt = (lives_mt *)user_data;
  LiVESList *slist = mt->selected_tracks;
  char *tstart, *tend;
  boolean did_backup = mt->did_backup;
  boolean needs_idlefunc = FALSE;

  int track;

  if (mt->idlefunc > 0) {
    needs_idlefunc = TRUE;
    lives_source_remove(mt->idlefunc);
    mt->idlefunc = 0;
  }

  mt_backup(mt, MT_UNDO_INSERT_GAP, 0);

  while (slist != NULL) {
    track = LIVES_POINTER_TO_INT(slist->data);
    insgap_inner(mt, track, TRUE, 1);
    slist = slist->next;
  }

  if (mt->opts.move_effects) {
    insgap_inner(mt, 0, TRUE, 2);
  }

  mt->did_backup = did_backup;
  mt_show_current_frame(mt, FALSE);

  tstart = time_to_string(QUANT_TIME(mt->region_start));
  tend = time_to_string(QUANT_TIME(mt->region_end));

  d_print(_("Inserted gap in selected tracks from time %s to time %s\n"), tstart, tend);

  lives_free(tstart);
  lives_free(tend);

  if (!did_backup) {
    mt->did_backup = FALSE;
    if (needs_idlefunc || (!did_backup && mt->auto_changed)) mt->idlefunc = mt_idle_add(mt);
    if (prefs->mt_auto_back == 0) mt_auto_backup(mt);
  }
}


void on_insgap_cur_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;

  boolean did_backup = mt->did_backup;
  boolean needs_idlefunc = FALSE;

  char *tstart, *tend;
  char *tname;

  if (mt->idlefunc > 0) {
    needs_idlefunc = TRUE;
    lives_source_remove(mt->idlefunc);
    mt->idlefunc = 0;
  }

  if (!did_backup) mt_backup(mt, MT_UNDO_INSERT_GAP, 0);

  insgap_inner(mt, mt->current_track, FALSE, 1);

  if (mt->opts.move_effects) {
    insgap_inner(mt, mt->current_track, FALSE, 2);
  }

  mt->did_backup = did_backup;
  mt_show_current_frame(mt, FALSE);

  tstart = time_to_string(QUANT_TIME(mt->region_start));
  tend = time_to_string(QUANT_TIME(mt->region_end));

  tname = get_track_name(mt, mt->current_track, FALSE);
  d_print(_("Inserted gap in track %s from time %s to time %s\n"), tname, tstart, tend);

  lives_free(tname);
  lives_free(tstart);
  lives_free(tend);

  if (!did_backup) {
    mt->did_backup = FALSE;
    if (needs_idlefunc || (!did_backup && mt->auto_changed)) mt->idlefunc = mt_idle_add(mt);
    if (prefs->mt_auto_back == 0) mt_auto_backup(mt);
  }
}


void multitrack_undo(LiVESMenuItem * menuitem, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;

  mt_undo *last_undo = (mt_undo *)lives_list_nth_data(mt->undos, lives_list_length(mt->undos) - 1 - mt->undo_offset);
  mt_undo *new_redo = NULL;

  LiVESList *slist;
  LiVESList *label_list = NULL;
  LiVESList *vlist, *llist;
  LiVESList *seltracks = NULL;
  LiVESList *aparam_view_list;

  LiVESWidget *checkbutton, *eventbox, *label;

  unsigned char *memblock, *mem_end;

  size_t space_avail = (size_t)(prefs->mt_undo_buf * 1024 * 1024) - mt->undo_buffer_used;
  size_t space_needed;

  double end_secs;
  double ptr_time;

  char *utxt, *tmp;
  char *txt;

  boolean block_is_selected = FALSE;
  boolean avoid_fx_list = FALSE;

  int current_track;
  int clip_sel;
  int avol_fx;
  int num_tracks;

  register int i;

  if (mt->undo_mem == NULL) return;

  if (mt->idlefunc > 0) {
    lives_source_remove(mt->idlefunc);
    mt->idlefunc = 0;
  }

  mt_desensitise(mt);

  mt->was_undo_redo = TRUE;
  ptr_time = mt->ptr_time;

  if (mt->block_selected != NULL) block_is_selected = TRUE;

  if (last_undo->action != MT_UNDO_NONE) {
    if (mt->undo_offset == 0) {
      add_markers(mt, mt->event_list, TRUE);
      if ((space_needed = estimate_space(mt, last_undo->action) + sizeof(mt_undo)) > space_avail) {
        if (!make_backup_space(mt, space_needed) || mt->undos == NULL) {
          remove_markers(mt->event_list);
          mt->idlefunc = mt_idle_add(mt);
          do_mt_undo_buf_error();
          mt_sensitise(mt);
          return;
        }
      }

      new_redo = (mt_undo *)(mt->undo_mem + mt->undo_buffer_used);
      new_redo->action = last_undo->action;

      memblock = (unsigned char *)(new_redo) + sizeof(mt_undo);
      new_redo->data_len = space_needed;
      save_event_list_inner(NULL, 0, mt->event_list, &memblock);
      mt->undo_buffer_used += space_needed;
      mt->undos = lives_list_append(mt->undos, new_redo);
      mt->undo_offset++;
    }

    current_track = mt->current_track;
    end_secs = mt->end_secs;
    num_tracks = mt->num_video_tracks;
    clip_sel = mt->clip_selected;

    seltracks = lives_list_copy(mt->selected_tracks);

    vlist = mt->video_draws;
    while (vlist != NULL) {
      eventbox = LIVES_WIDGET(vlist->data);
      label = LIVES_WIDGET(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "label"));
      txt = lives_strdup(lives_label_get_text(LIVES_LABEL(label)));
      label_list = lives_list_append(label_list, txt);
      vlist = vlist->next;
    }

    aparam_view_list = lives_list_copy(mt->opts.aparam_view_list);
    avol_fx = mt->avol_fx;
    mt->avol_fx = -1;

    mt->no_expose = TRUE;

    event_list_free(mt->event_list);
    last_undo = (mt_undo *)lives_list_nth_data(mt->undos, lives_list_length(mt->undos) - 1 - mt->undo_offset);
    memblock = (unsigned char *)(last_undo) + sizeof(mt_undo);
    mem_end = memblock + last_undo->data_len - sizeof(mt_undo);
    mt->event_list = load_event_list_inner(mt, -1, FALSE, NULL, &memblock, mem_end);

    if (!event_list_rectify(mt, mt->event_list)) {
      event_list_free(mt->event_list);
      mt->event_list = NULL;
    }

    if (get_first_event(mt->event_list) == NULL) {
      event_list_free(mt->event_list);
      mt->event_list = NULL;
    }

    for (i = 0; i < mt->num_video_tracks; i++) {
      delete_video_track(mt, i, FALSE);
    }
    lives_list_free(mt->video_draws);
    mt->video_draws = NULL;
    mt->num_video_tracks = 0;

    delete_audio_tracks(mt, mt->audio_draws, FALSE);
    mt->audio_draws = NULL;

    mt->fm_edit_event = NULL; // this might have been deleted; etc., c.f. fixup_events
    mt->init_event = NULL;
    mt->selected_init_event = NULL;
    mt->specific_event = NULL;
    mt->avol_init_event = NULL; // we will try to relocate this in mt_init_tracks()

    mt_init_tracks(mt, FALSE);

    if (mt->avol_fx == -1) mt->avol_fx = avol_fx;
    if (mt->avol_fx != -1) mt->opts.aparam_view_list = lives_list_copy(aparam_view_list);
    if (aparam_view_list != NULL) lives_list_free(aparam_view_list);

    add_aparam_menuitems(mt);

    unselect_all(mt);
    for (i = mt->num_video_tracks; i < num_tracks; i++) {
      add_video_track_behind(NULL, mt);
    }

    mt->clip_selected = clip_sel;
    mt_clip_select(mt, FALSE);

    vlist = mt->video_draws;
    llist = label_list;
    while (vlist != NULL) {
      eventbox = LIVES_WIDGET(vlist->data);
      label = LIVES_WIDGET(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "label"));
      widget_opts.mnemonic_label = FALSE;
      lives_label_set_text(LIVES_LABEL(label), (const char *)llist->data);
      widget_opts.mnemonic_label = TRUE;
      vlist = vlist->next;
      llist = llist->next;
    }
    lives_list_free(label_list);

    if (mt->event_list != NULL) remove_markers(mt->event_list);

    mt->selected_tracks = lives_list_copy(seltracks);
    slist = mt->selected_tracks;
    while (slist != NULL) {
      eventbox = (LiVESWidget *)lives_list_nth_data(mt->video_draws, LIVES_POINTER_TO_INT(slist->data));
      checkbutton = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "checkbutton");
#ifdef ENABLE_GIW
      if (!prefs->lamp_buttons) {
#endif
        lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(checkbutton), TRUE);
#ifdef ENABLE_GIW
      } else {
        giw_led_set_mode(GIW_LED(checkbutton), TRUE);
      }
#endif
      slist = slist->next;
    }
    if (seltracks != NULL) lives_list_free(seltracks);

    mt->current_track = current_track;
    track_select(mt);
    if (mt->end_secs != end_secs && event_list_get_end_secs(mt->event_list) <= end_secs) set_timeline_end_secs(mt, end_secs);
  }

  mt->no_expose = FALSE;

  mt->undo_offset++;

  if (mt->undo_offset == lives_list_length(mt->undos)) mt_set_undoable(mt, MT_UNDO_NONE, NULL, FALSE);
  else {
    mt_undo *undo = (mt_undo *)(lives_list_nth_data(mt->undos, lives_list_length(mt->undos) - mt->undo_offset - 1));
    mt_set_undoable(mt, undo->action, undo->extra, TRUE);
  }
  mt_set_redoable(mt, last_undo->action, last_undo->extra, TRUE);
  lives_ruler_set_value(LIVES_RULER(mt->timeline), ptr_time);
  lives_widget_queue_draw(mt->timeline);

  utxt = lives_utf8_strdown((tmp = get_undo_text(last_undo->action, last_undo->extra)), -1);
  lives_free(tmp);

  d_print(_("Undid %s\n"), utxt);
  lives_free(utxt);

  if (last_undo->action <= 1024 && block_is_selected) mt_selblock(LIVES_MENU_ITEM(mt->seldesel_menuitem), (livespointer)mt);

  // TODO - make sure this is the effect which is now deleted/added...
  if (mt->poly_state == POLY_PARAMS) {
    if (mt->last_fx_type == MT_LAST_FX_BLOCK && mt->block_selected != NULL) polymorph(mt, POLY_FX_STACK);
    else polymorph(mt, POLY_CLIPS);
    avoid_fx_list = TRUE;
  }
  if ((last_undo->action == MT_UNDO_FILTER_MAP_CHANGE || mt->poly_state == POLY_FX_STACK) && !avoid_fx_list) {
    if (last_undo->action == MT_UNDO_FILTER_MAP_CHANGE) mt_tl_move(mt, last_undo->tc);
    polymorph(mt, POLY_FX_STACK);
  }
  if (mt->poly_state != POLY_PARAMS) mt_show_current_frame(mt, FALSE);

  mt_desensitise(mt);
  mt_sensitise(mt);

  if (mt->event_list == NULL) recover_layout_cancelled(FALSE);

  mt->idlefunc = mt_idle_add(mt);
  if (prefs->mt_auto_back == 0) mt_auto_backup(mt);
}


void multitrack_redo(LiVESMenuItem * menuitem, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;

  mt_undo *last_redo = (mt_undo *)lives_list_nth_data(mt->undos, lives_list_length(mt->undos) + 1 - mt->undo_offset);

  LiVESWidget *checkbutton, *eventbox, *label;

  LiVESList *slist;
  LiVESList *label_list = NULL;
  LiVESList *vlist, *llist;
  LiVESList *seltracks = NULL;
  LiVESList *aparam_view_list;

  unsigned char *memblock, *mem_end;

  char *txt;
  char *utxt, *tmp;

  double ptr_time;
  double end_secs;

  int current_track;
  int num_tracks;
  int clip_sel;
  int avol_fx;

  register int i;

  if (mt->undo_mem == NULL) return;

  if (mt->idlefunc > 0) {
    lives_source_remove(mt->idlefunc);
    mt->idlefunc = 0;
  }

  mt_desensitise(mt);

  //if (mt->block_selected!=NULL) block_is_selected=TRUE; // TODO *** - need to set track and time

  mt->was_undo_redo = TRUE;
  ptr_time = mt->ptr_time;

  if (last_redo->action != MT_UNDO_NONE) {
    current_track = mt->current_track;
    end_secs = mt->end_secs;
    num_tracks = mt->num_video_tracks;
    clip_sel = mt->clip_selected;

    seltracks = lives_list_copy(mt->selected_tracks);

    vlist = mt->video_draws;
    while (vlist != NULL) {
      eventbox = LIVES_WIDGET(vlist->data);
      label = LIVES_WIDGET(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "label"));
      txt = lives_strdup(lives_label_get_text(LIVES_LABEL(label)));
      label_list = lives_list_append(label_list, txt);
      vlist = vlist->next;
    }

    aparam_view_list = lives_list_copy(mt->opts.aparam_view_list);
    avol_fx = mt->avol_fx;
    mt->avol_fx = -1;

    mt->no_expose = TRUE;

    event_list_free(mt->event_list);

    memblock = (unsigned char *)(last_redo) + sizeof(mt_undo);
    mem_end = memblock + last_redo->data_len - sizeof(mt_undo);
    mt->event_list = load_event_list_inner(mt, -1, FALSE, NULL, &memblock, mem_end);
    if (!event_list_rectify(mt, mt->event_list)) {
      event_list_free(mt->event_list);
      mt->event_list = NULL;
    }

    if (get_first_event(mt->event_list) == NULL) {
      event_list_free(mt->event_list);
      mt->event_list = NULL;
    }

    for (i = 0; i < mt->num_video_tracks; i++) {
      delete_video_track(mt, i, FALSE);
    }
    lives_list_free(mt->video_draws);
    mt->video_draws = NULL;
    mt->num_video_tracks = 0;

    delete_audio_tracks(mt, mt->audio_draws, FALSE);
    mt->audio_draws = NULL;

    mt->fm_edit_event = NULL; // this might have been deleted; etc., c.f. fixup_events
    mt->init_event = NULL;
    mt->selected_init_event = NULL;
    mt->specific_event = NULL;
    mt->avol_init_event = NULL; // we will try to relocate this in mt_init_tracks()

    mt_init_tracks(mt, FALSE);

    if (mt->avol_fx == avol_fx) {
      mt->opts.aparam_view_list = lives_list_copy(aparam_view_list);
    }
    if (aparam_view_list != NULL) lives_list_free(aparam_view_list);

    add_aparam_menuitems(mt);

    unselect_all(mt);
    for (i = mt->num_video_tracks; i < num_tracks; i++) {
      add_video_track_behind(NULL, mt);
    }

    mt->clip_selected = clip_sel;
    mt_clip_select(mt, FALSE);

    vlist = mt->video_draws;
    llist = label_list;
    while (vlist != NULL) {
      eventbox = LIVES_WIDGET(vlist->data);
      label = LIVES_WIDGET(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "label"));
      widget_opts.mnemonic_label = FALSE;
      lives_label_set_text(LIVES_LABEL(label), (const char *)llist->data);
      widget_opts.mnemonic_label = TRUE;
      vlist = vlist->next;
      llist = llist->next;
    }
    lives_list_free(label_list);

    if (mt->event_list != NULL) remove_markers(mt->event_list);

    mt->selected_tracks = lives_list_copy(seltracks);
    slist = mt->selected_tracks;
    while (slist != NULL) {
      eventbox = (LiVESWidget *)lives_list_nth_data(mt->video_draws, LIVES_POINTER_TO_INT(slist->data));
      checkbutton = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "checkbutton");
#ifdef ENABLE_GIW
      if (!prefs->lamp_buttons) {
#endif
        lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(checkbutton), TRUE);
#ifdef ENABLE_GIW
      } else {
        giw_led_set_mode(GIW_LED(checkbutton), TRUE);
      }
#endif
      slist = slist->next;
    }
    if (seltracks != NULL) lives_list_free(seltracks);

    mt->current_track = current_track;
    track_select(mt);
    if (mt->end_secs != end_secs && event_list_get_end_secs(mt->event_list) <= end_secs) set_timeline_end_secs(mt, end_secs);
  }

  mt->no_expose = FALSE;

  mt->undo_offset--;

  if (mt->undo_offset <= 1) mt_set_redoable(mt, MT_UNDO_NONE, NULL, FALSE);
  else {
    mt_undo *redo = (mt_undo *)(lives_list_nth_data(mt->undos, lives_list_length(mt->undos) - mt->undo_offset));
    mt_set_redoable(mt, redo->action, redo->extra, TRUE);
  }
  last_redo = (mt_undo *)lives_list_nth_data(mt->undos, lives_list_length(mt->undos) - 1 - mt->undo_offset);
  mt_set_undoable(mt, last_redo->action, last_redo->extra, TRUE);

  lives_ruler_set_value(LIVES_RULER(mt->timeline), ptr_time);
  lives_widget_queue_draw(mt->timeline);

  // TODO *****
  //if (last_redo->action<1024&&block_is_selected) mt_selblock(NULL, NULL, 0, 0, (livespointer)mt);

  if (last_redo->action == MT_UNDO_FILTER_MAP_CHANGE || mt->poly_state == POLY_FX_STACK) {
    if (last_redo->action == MT_UNDO_FILTER_MAP_CHANGE) mt_tl_move(mt, last_redo->tc);
    polymorph(mt, POLY_FX_STACK);
  }
  if (mt->poly_state != POLY_PARAMS) mt_show_current_frame(mt, FALSE);

  utxt = lives_utf8_strdown((tmp = get_undo_text(last_redo->action, last_redo->extra)), -1);
  lives_free(tmp);

  d_print(_("Redid %s\n"), utxt);
  lives_free(utxt);

  mt_desensitise(mt);
  mt_sensitise(mt);

  if (mt->event_list == NULL) recover_layout_cancelled(FALSE);

  mt->idlefunc = mt_idle_add(mt);
  if (prefs->mt_auto_back == 0) mt_auto_backup(mt);
}


void multitrack_view_details(LiVESMenuItem * menuitem, livespointer user_data) {
  char buff[512];
  lives_clipinfo_t *filew;
  lives_mt *mt = (lives_mt *)user_data;
  lives_clip_t *rfile = mainw->files[mt->render_file];
  uint32_t bsize = 0;
  double time = 0.;
  int num_events = 0;

  filew = create_clip_info_window(cfile->achans, TRUE);

  // type
  lives_snprintf(buff, 512, "\n  Event List");
  lives_text_view_set_text(LIVES_TEXT_VIEW(filew->textview_type), buff, -1);

  // fps
  if (mt->fps > 0) {
    lives_snprintf(buff, 512, "\n  %.3f%s", mt->fps, rfile->ratio_fps ? "..." : "");
  } else {
    lives_snprintf(buff, 512, "%s", _("\n (variable)"));
  }

  lives_text_view_set_text(LIVES_TEXT_VIEW(filew->textview_fps), buff, -1);

  // image size
  lives_snprintf(buff, 512, "\n  %dx%d", rfile->hsize, rfile->vsize);
  lives_text_view_set_text(LIVES_TEXT_VIEW(filew->textview_size), buff, -1);

  // elist time
  if (mt->event_list != NULL) {
    bsize = event_list_get_byte_size(mt, mt->event_list, &num_events);
    time = event_list_get_end_secs(mt->event_list);
  }

  // events
  lives_snprintf(buff, 512, "\n  %d", num_events);
  lives_text_view_set_text(LIVES_TEXT_VIEW(filew->textview_frames), buff, -1);

  lives_snprintf(buff, 512, "\n  %.3f sec", time);
  lives_text_view_set_text(LIVES_TEXT_VIEW(filew->textview_vtime), buff, -1);

  // byte size
  lives_snprintf(buff, 512, "\n  %d bytes", bsize);
  lives_text_view_set_text(LIVES_TEXT_VIEW(filew->textview_fsize), buff, -1);

  if (cfile->achans > 0) {
    lives_snprintf(buff, 512, "\n  %d Hz %d bit", cfile->arate, cfile->asampsize);
    lives_text_view_set_text(LIVES_TEXT_VIEW(filew->textview_lrate), buff, -1);
  }

  if (cfile->achans > 1) {
    lives_snprintf(buff, 512, "\n  %d Hz %d bit", cfile->arate, cfile->asampsize);
    lives_text_view_set_text(LIVES_TEXT_VIEW(filew->textview_rrate), buff, -1);
  }
}


static void add_effect_inner(lives_mt * mt, int num_in_tracks, int *in_tracks, int num_out_tracks, int *out_tracks,
                             weed_plant_t *start_event, weed_plant_t *end_event) {
  void **init_events;

  weed_plant_t *event, *init_event;
  weed_plant_t *filter = get_weed_filter(mt->current_fx);

  double timesecs = mt->ptr_time;

  weed_timecode_t start_tc = get_event_timecode(start_event);
  weed_timecode_t end_tc = get_event_timecode(end_event);

  weed_timecode_t tc = q_gint64(timesecs * TICKS_PER_SECOND_DBL, mt->fps);

  lives_rfx_t *rfx;

  boolean has_params;

  register int i;

  // set track_index (for special widgets)
  mt->track_index = -1;
  for (i = 0; i < num_in_tracks; i++) {
    if (mt->current_track == in_tracks[i]) {
      mt->track_index = i;
      break;
    }
  }

  // add effect_init event
  mt->event_list = append_filter_init_event(mt->event_list, start_tc, mt->current_fx, num_in_tracks, -1, NULL);
  mt->init_event = get_last_event(mt->event_list);
  unlink_event(mt->event_list, mt->init_event);
  weed_set_int_array(mt->init_event, WEED_LEAF_IN_TRACKS, num_in_tracks, in_tracks);
  weed_set_int_array(mt->init_event, WEED_LEAF_OUT_TRACKS, num_out_tracks, out_tracks);
  insert_filter_init_event_at(mt->event_list, start_event, mt->init_event);

  if (pchain != NULL) {
    // no freep !
    lives_free(pchain);
    pchain = NULL;
  }

  if (num_in_params(filter, FALSE, FALSE) > 0)
    pchain = filter_init_add_pchanges(mt->event_list, filter, mt->init_event, num_in_tracks, 0);

  // add effect map event
  init_events = get_init_events_before(start_event, mt->init_event, TRUE);
  mt->event_list = append_filter_map_event(mt->event_list, start_tc, init_events);
  lives_free(init_events);
  event = get_last_event(mt->event_list);
  unlink_event(mt->event_list, event);
  insert_filter_map_event_at(mt->event_list, start_event, event, TRUE);

  // update all effect maps in block, appending init_event
  update_filter_maps(start_event, end_event, mt->init_event);

  // add effect deinit event
  mt->event_list = append_filter_deinit_event(mt->event_list, end_tc, (void *)mt->init_event, pchain);
  event = get_last_event(mt->event_list);
  unlink_event(mt->event_list, event);
  insert_filter_deinit_event_at(mt->event_list, end_event, event);

  // zip forward a bit, in case there is a FILTER_MAP at end_tc after our FILTER_DEINIT (e.g. if adding multiple filters)
  while (event != NULL && get_event_timecode(event) == end_tc) event = get_next_event(event);
  if (event == NULL) event = get_last_event(mt->event_list);
  else event = get_prev_event(event);

  // add effect map event
  init_events = get_init_events_before(event, mt->init_event, FALSE); // also deletes the effect
  mt->event_list = append_filter_map_event(mt->event_list, end_tc, init_events);
  lives_free(init_events);

  event = get_last_event(mt->event_list);
  unlink_event(mt->event_list, event);
  insert_filter_map_event_at(mt->event_list, end_event, event, FALSE);

  if (mt->event_list != NULL) lives_widget_set_sensitive(mt->clear_event_list, TRUE);

  if (mt->current_fx == mt->avol_fx) {
    return;
  }

  if (mt->avol_fx != -1) {
    apply_avol_filter(mt);
  }

  if (mt->is_atrans) {
    return;
  }

  rfx = weed_to_rfx(filter, FALSE);
  get_track_index(mt, tc);

  // here we just check if we have any params to display
  has_params = make_param_box(NULL, rfx);
  rfx_free(rfx);
  lives_free(rfx);

  init_event = mt->init_event;
  mt_tl_move(mt, start_tc / TICKS_PER_SECOND_DBL);
  mt->init_event = init_event;

  if (has_params) {
    polymorph(mt, POLY_PARAMS);
    lives_widget_set_sensitive(mt->apply_fx_button, FALSE);
    lives_widget_set_sensitive(mt->resetp_button, check_can_resetp(mt));
  } else polymorph(mt, POLY_FX_STACK);
}


weed_plant_t *add_blank_frames_up_to(weed_plant_t *event_list, weed_plant_t *start_event, weed_timecode_t end_tc, double fps) {
  // add blank frames from FRAME event (or NULL) start_event up to and including (quantised) end_tc
  // returns updated event_list
  weed_timecode_t tc;
  weed_plant_t *shortcut = NULL;
  weed_timecode_t tl = q_dbl(1. / fps, fps);
  int blank_clip = -1;
  int64_t blank_frame = 0;

  if (start_event != NULL) tc = get_event_timecode(start_event) + tl;
  else tc = 0;

  for (; tc <= end_tc; tc = q_gint64(tc + tl, fps)) {
    event_list = insert_frame_event_at(event_list, tc, 1, &blank_clip, &blank_frame, &shortcut);
  }
  weed_set_double_value(event_list, WEED_LEAF_FPS, fps);
  return event_list;
}


void mt_add_region_effect(LiVESMenuItem * menuitem, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;

  LiVESList *llist;

  weed_plant_t *start_event;
  weed_plant_t *end_event;
  weed_plant_t *last_frame_event = NULL;

  weed_timecode_t start_tc = q_gint64(mt->region_start * TICKS_PER_SECOND_DBL, mt->fps);
  weed_timecode_t end_tc = q_gint64(mt->region_end * TICKS_PER_SECOND_DBL - TICKS_PER_SECOND_DBL / mt->fps, mt->fps);
  weed_timecode_t last_frame_tc = 0;

  char *filter_name;
  char *tname, *track_desc;
  char *tmp, *tmp1;
  char *tstart, *tend;

  boolean did_backup = mt->did_backup;
  boolean needs_idlefunc = FALSE;

  int numtracks = lives_list_length(mt->selected_tracks);
  int tcount = 0, tlast = -1000000, tsmall = -1, ctrack;

  int *tracks = (int *)lives_malloc(numtracks * sizint);

  if (mt->idlefunc > 0) {
    needs_idlefunc = TRUE;
    lives_source_remove(mt->idlefunc);
    mt->idlefunc = 0;
  }

  // sort selected tracks into ascending order
  while (tcount < numtracks) {
    tsmall = -1000000;
    llist = mt->selected_tracks;
    while (llist != NULL) {
      ctrack = LIVES_POINTER_TO_INT(llist->data);
      if ((tsmall == -1000000 || ctrack < tsmall) && ctrack > tlast) tsmall = ctrack;
      llist = llist->next;
    }
    tracks[tcount++] = tlast = tsmall;
  }

  // add blank frames up to region end (if necessary)
  if (mt->event_list != NULL &&
      ((last_frame_event = get_last_frame_event(mt->event_list)) != NULL)) last_frame_tc = get_event_timecode(last_frame_event);
  if (end_tc > last_frame_tc) mt->event_list = add_blank_frames_up_to(mt->event_list, last_frame_event,
        end_tc - (double)(tracks[0] < 0) * TICKS_PER_SECOND_DBL / mt->fps,
        mt->fps);

  if (menuitem != NULL) mt->current_fx = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(menuitem), "idx"));

  start_event = get_frame_event_at(mt->event_list, start_tc, NULL, TRUE);
  end_event = get_frame_event_at(mt->event_list, end_tc, start_event, TRUE);

  add_effect_inner(mt, numtracks, tracks, 1, &tracks[0], start_event, end_event);

  if (menuitem == NULL && !mt->is_atrans) {
    if (!did_backup) {
      if (needs_idlefunc || (!did_backup && mt->auto_changed)) mt->idlefunc = mt_idle_add(mt);
    }
    lives_free(tracks);
    return;
  }

  mt->last_fx_type = MT_LAST_FX_REGION;

  // create user message
  filter_name = weed_filter_idx_get_name(mt->current_fx, FALSE, TRUE, FALSE);
  numtracks = enabled_in_channels(get_weed_filter(mt->current_fx), TRUE); // count repeated channels
  switch (numtracks) {
  case 1:
    tname = lives_fx_cat_to_text(LIVES_FX_CAT_EFFECT, FALSE); // effect
    track_desc = lives_strdup_printf(_("track %s"), (tmp = get_track_name(mt, tracks[0], FALSE)));
    lives_free(tmp);
    break;
  case 2:
    tname = lives_fx_cat_to_text(LIVES_FX_CAT_TRANSITION, FALSE); // transition
    track_desc = lives_strdup_printf(_("tracks %s and %s"), (tmp1 = get_track_name(mt, tracks[0], FALSE)), (tmp = get_track_name(mt,
                                     tracks[1],
                                     FALSE)));
    lives_free(tmp);
    lives_free(tmp1);
    break;
  default:
    tname = lives_fx_cat_to_text(LIVES_FX_CAT_COMPOSITOR, FALSE); // compositor
    track_desc = lives_strdup(_("selected tracks"));
    break;
  }
  lives_free(tracks);

  tstart = time_to_string(QUANT_TICKS(start_tc));
  tend = time_to_string(QUANT_TICKS(end_tc + TICKS_PER_SECOND_DBL / mt->fps));

  d_print(_("Added %s %s to %s from time %s to time %s\n"), tname, filter_name, track_desc, tstart, tend);

  lives_free(tstart);
  lives_free(tend);
  lives_free(filter_name);
  lives_free(tname);
  lives_free(track_desc);

  if (!did_backup) {
    mt->did_backup = FALSE;
    if (needs_idlefunc || (!did_backup && mt->auto_changed)) mt->idlefunc = mt_idle_add(mt);
    if (prefs->mt_auto_back == 0) mt_auto_backup(mt);
  }
}


static boolean mt_add_region_effect_idle(livespointer user_data) {
  mt_add_region_effect(LIVES_MENU_ITEM(dummy_menuitem), user_data);
  lives_widget_object_unref(dummy_menuitem);
  return FALSE;
}


void mt_add_block_effect(LiVESMenuItem * menuitem, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  weed_plant_t *start_event = mt->block_selected->start_event;
  weed_plant_t *end_event = mt->block_selected->end_event;
  weed_timecode_t start_tc = get_event_timecode(start_event);
  weed_timecode_t end_tc = get_event_timecode(end_event);
  char *filter_name;
  char *tstart, *tend;
  char *tmp;
  int selected_track = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(mt->block_selected->eventbox),
                       "layer_number"));
  boolean did_backup = mt->did_backup;
  boolean needs_idlefunc = FALSE;

  if (mt->idlefunc > 0) {
    needs_idlefunc = TRUE;
    lives_source_remove(mt->idlefunc);
    mt->idlefunc = 0;
  }

  if (menuitem != NULL) mt->current_fx
      = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(menuitem), "idx"));

  mt->last_fx_type = MT_LAST_FX_BLOCK;
  add_effect_inner(mt, 1, &selected_track, 1, &selected_track, start_event, end_event);

  filter_name = weed_filter_idx_get_name(mt->current_fx, FALSE, TRUE, FALSE);

  tstart = time_to_string(QUANT_TICKS(start_tc));
  tend = time_to_string(QUANT_TICKS(end_tc + TICKS_PER_SECOND_DBL / mt->fps));

  d_print(_("Added effect %s to track %s from time %s to time %s\n"), filter_name,
          (tmp = get_track_name(mt, selected_track, mt->aud_track_selected)),
          tstart, tend);

  lives_free(tstart);
  lives_free(tend);
  lives_free(tmp);
  lives_free(filter_name);

  if (!did_backup) {
    mt->did_backup = FALSE;
    if (needs_idlefunc || (!did_backup && mt->auto_changed)) mt->idlefunc = mt_idle_add(mt);
    if (prefs->mt_auto_back == 0) mt_auto_backup(mt);
  }
}


static boolean mt_add_block_effect_idle(livespointer user_data) {
  mt_add_block_effect(LIVES_MENU_ITEM(dummy_menuitem), user_data);
  lives_widget_object_unref(dummy_menuitem);
  return FALSE;
}


void on_mt_list_fx_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  // list effects at current frame/track
  lives_mt *mt = (lives_mt *)user_data;
  polymorph(mt, POLY_FX_STACK);
}


void on_mt_delfx_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;

  weed_timecode_t start_tc, end_tc;

  weed_plant_t *deinit_event, *init_event = mt->selected_init_event;

  int *tracks;

  char *fhash, *filter_name;
  char *tname, *track_desc;
  char *tmp, *tmp1;
  char *tstart, *tend;

  boolean did_backup = mt->did_backup;
  boolean needs_idlefunc = FALSE;

  int numtracks;
  int error;

  if (mt->selected_init_event == NULL) return;

  if (mt->is_rendering) return;

  if (mt->idlefunc > 0) {
    needs_idlefunc = TRUE;
    lives_source_remove(mt->idlefunc);
    mt->idlefunc = 0;
  }

  fhash = weed_get_string_value(init_event, WEED_LEAF_FILTER, &error);
  mt->current_fx = weed_get_idx_for_hashname(fhash, TRUE);
  lives_free(fhash);

  deinit_event = weed_get_plantptr_value(init_event, WEED_LEAF_DEINIT_EVENT, &error);
  filter_name = weed_filter_idx_get_name(mt->current_fx, FALSE, FALSE, FALSE);
  start_tc = get_event_timecode(init_event);
  end_tc = get_event_timecode(deinit_event) + TICKS_PER_SECOND_DBL / mt->fps;

  tracks = weed_get_int_array_counted(init_event, WEED_LEAF_IN_TRACKS, &numtracks);

  numtracks = enabled_in_channels(get_weed_filter(mt->current_fx), TRUE); // count repeated channels
  switch (numtracks) {
  case 1:
    tname = lives_fx_cat_to_text(LIVES_FX_CAT_EFFECT, FALSE); // effect
    track_desc = lives_strdup_printf(_("track %s"), (tmp = get_track_name(mt, tracks[0], FALSE)));
    lives_free(tmp);
    break;
  case 2:
    tname = lives_fx_cat_to_text(LIVES_FX_CAT_TRANSITION, FALSE); // transition
    track_desc = lives_strdup_printf(_("tracks %s and %s"), (tmp1 = get_track_name(mt, tracks[0], FALSE)), (tmp = get_track_name(mt,
                                     tracks[1],
                                     FALSE)));
    lives_free(tmp);
    lives_free(tmp1);
    break;
  default:
    tname = lives_fx_cat_to_text(LIVES_FX_CAT_COMPOSITOR, FALSE); // compositor
    track_desc = lives_strdup(_("selected tracks"));
    break;
  }

  lives_free(tracks);

  mt_backup(mt, MT_UNDO_DELETE_FILTER, 0);

  remove_filter_from_event_list(mt->event_list, mt->selected_init_event);
  remove_end_blank_frames(mt->event_list, TRUE);

  tstart = time_to_string(QUANT_TICKS(start_tc));
  tend = time_to_string(QUANT_TICKS(end_tc + TICKS_PER_SECOND_DBL / mt->fps));

  d_print(_("Deleted %s %s from %s from time %s to time %s\n"), tname, filter_name, track_desc, tstart, tend);

  lives_free(tstart);
  lives_free(tend);
  lives_free(filter_name);
  lives_free(track_desc);

  mt->selected_init_event = NULL;
  mt->current_fx = -1;
  if (mt->poly_state == POLY_PARAMS) polymorph(mt, POLY_CLIPS);
  else if (mt->poly_state == POLY_FX_STACK) polymorph(mt, POLY_FX_STACK);
  mt_show_current_frame(mt, FALSE);

  if (!did_backup) {
    mt->did_backup = FALSE;
    if (needs_idlefunc || (!did_backup && mt->auto_changed)) mt->idlefunc = mt_idle_add(mt);
    if (prefs->mt_auto_back == 0) mt_auto_backup(mt);
  }
}


static void mt_jumpto(lives_mt * mt, lives_direction_t dir) {
  track_rect *block;

  weed_timecode_t tc = q_gint64(mt->ptr_time * TICKS_PER_SECOND_DBL, mt->fps);
  weed_timecode_t start_tc, end_tc;

  LiVESWidget *eventbox;

  double secs = tc / TICKS_PER_SECOND_DBL;
  double offs = 1.;

  if (mt->current_track > -1 &&
      !mt->aud_track_selected) eventbox = (LiVESWidget *)lives_list_nth_data(mt->video_draws, mt->current_track);
  else {
    eventbox = (LiVESWidget *)lives_list_nth_data(mt->audio_draws, mt->current_track + mt->opts.back_audio_tracks);
    offs = 0.;
  }
  block = get_block_from_time(eventbox, secs, mt);

  if (block != NULL) {
    if (dir == LIVES_DIRECTION_BACKWARD) {
      if (tc == (start_tc = get_event_timecode(block->start_event))) {
        secs -= 1. / mt->fps;
        block = NULL;
      } else secs = start_tc / TICKS_PER_SECOND_DBL;
    } else {
      if (tc == q_gint64((end_tc = get_event_timecode(block->end_event)) + (offs * TICKS_PER_SECOND_DBL) / mt->fps, mt->fps)) {
        secs += 1. / mt->fps;
        block = NULL;
      } else secs = end_tc / TICKS_PER_SECOND_DBL + offs / mt->fps;
    }
  }
  if (block == NULL) {
    if (dir == LIVES_DIRECTION_BACKWARD) {
      block = get_block_before(eventbox, secs, TRUE);
      if (block == NULL) secs = 0.;
      else {
        if (tc == q_gint64((end_tc = get_event_timecode(block->end_event)) + (offs * TICKS_PER_SECOND_DBL) / mt->fps, mt->fps)) {
          secs = get_event_timecode(block->start_event) / TICKS_PER_SECOND_DBL;
        } else secs = end_tc / TICKS_PER_SECOND_DBL + offs / mt->fps;
      }
    } else {
      block = get_block_after(eventbox, secs, FALSE);
      if (block == NULL) return;
      secs = get_event_timecode(block->start_event) / TICKS_PER_SECOND_DBL;
    }
  }

  if (secs < 0.) secs = 0.;
  if (secs > mt->end_secs) set_timeline_end_secs(mt, secs);
  mt->fm_edit_event = NULL;
  mt_tl_move(mt, secs);
}


void on_jumpback_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  mt_jumpto(mt, LIVES_DIRECTION_BACKWARD);
}


void on_jumpnext_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  mt_jumpto(mt, LIVES_DIRECTION_FORWARD);
}


static void mt_jumpto_mark(lives_mt * mt, lives_direction_t dir) {
  LiVESList *tl_marks = mt->tl_marks;
  double time = -1., marktime = -1.;
  double ptr_time = q_dbl(mt->ptr_time, mt->fps) / TICKS_PER_SECOND_DBL;

  while (tl_marks != NULL) {
    time = q_dbl(strtod((char *)tl_marks->data, NULL), mt->fps) / TICKS_PER_SECOND_DBL;
    if (time > ptr_time) break;
    marktime = time;
    tl_marks = tl_marks->next;
  }
  if (dir == LIVES_DIRECTION_FORWARD) marktime = time;
  if (marktime > 0.)
    mt_tl_move(mt, marktime);
}


void on_jumpback_mark_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  mt_jumpto_mark(mt, LIVES_DIRECTION_BACKWARD);
}


void on_jumpnext_mark_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  mt_jumpto_mark(mt, LIVES_DIRECTION_FORWARD);
}


void on_rename_track_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  _entryw *rnentry;
  LiVESWidget *xeventbox;

  char *cname;

  int response;

  if (mt->current_track < 0) return;

  xeventbox = (LiVESWidget *)lives_list_nth_data(mt->video_draws, mt->current_track);

  cname = (char *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(xeventbox), "track_name");

  rnentry = create_rename_dialog(7);

  response = lives_dialog_run(LIVES_DIALOG(rnentry->dialog));

  if (response == LIVES_RESPONSE_CANCEL) return; // destroyed and freed in a callback

  lives_free(cname);

  cname = lives_strdup(lives_entry_get_text(LIVES_ENTRY(rnentry->entry)));

  lives_widget_destroy(rnentry->dialog);
  lives_free(rnentry);

  lives_widget_object_set_data_auto(LIVES_WIDGET_OBJECT(xeventbox), "track_name", cname);

  set_track_label(LIVES_EVENT_BOX(xeventbox), mt->current_track);
}


void on_cback_audio_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  mt->current_track = -1;
  track_select(mt);
}


boolean on_render_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  LiVESList *list;
  char *com;

  boolean had_audio = FALSE;
  boolean post_reset_ba = FALSE;
  boolean post_reset_ca = FALSE;
  boolean retval = FALSE;

  // save these values, because reget_afilesize() can reset them
  int arate = cfile->arate;
  int arps = cfile->arps;
  int asampsize = cfile->asampsize;
  int achans = cfile->achans;
  int signed_endian = cfile->signed_endian;

  int orig_file;
  register int i;

  if (mt->idlefunc > 0) {
    lives_source_remove(mt->idlefunc);
    mt->idlefunc = 0;
  }

  if (menuitem == NULL) {
    // pre-render audio (not used currently)
    mt->pr_audio = TRUE;
    had_audio = mt->has_audio_file;
    if (had_audio) {
      lives_rm(cfile->info_file);
      mainw->error = FALSE;
      mainw->cancelled = CANCEL_NONE;
      com = lives_strdup_printf("%s backup_audio \"%s\"", prefs->backend_sync, cfile->handle);
      lives_popen(com, TRUE, mainw->msg, MAINW_MSG_SIZE);
      lives_free(com);
      handle_backend_errors(FALSE, get_transient_full());
      if (mainw->error) return FALSE;
    }
    mt->has_audio_file = TRUE;
  } else {
    mt->pr_audio = FALSE;
  }

  mt_desensitise(mt);

  /// disable playback optimisations
  mainw->effort = -EFFORT_RANGE_MAX;
  mainw->event_list = mt->event_list;

  mt->is_rendering = TRUE; // use this to test for rendering from mt (not mainw->is_rendering)
  lives_widget_set_sensitive(mt->render_vid, FALSE);
  lives_widget_set_sensitive(mt->render_aud, FALSE);
  lives_widget_set_sensitive(mt->normalise_aud, FALSE);

  if (mt->poly_state == POLY_PARAMS) polymorph(mt, POLY_FX_STACK);

  mt->pb_start_event = get_first_event(mainw->event_list);

  if (mt->opts.normalise_audp) {
    // Normalise audio (preference)

    // TODO - in future we could also check the pb volume levels and adjust to prevent clipping
    // - although this would be time consuming when clicking or activating "render" in mt

    // auto-adjust mixer levels:
    boolean has_backing_audio = FALSE;
    boolean has_channel_audio = FALSE;

    // -> if we have either but not both: backing audio or channel audio
    list = mt->audio_draws;
    if (mt->opts.back_audio_tracks >= 1) {
      // check backing track(s) for audio blocks
      for (i = 0; i < mt->opts.back_audio_tracks; list = list->next, i++) {
        if (!is_empty_track(LIVES_WIDGET_OBJECT(list->data))) {
          if (get_mixer_track_vol(mt, i) == 0.5) {
            has_backing_audio = TRUE;
	    // *INDENT-OFF*
          }}}}
    // *INDENT-ON*

    list = mt->audio_draws;
    for (i = mt->opts.back_audio_tracks + 1; --i > 0; list = list->next);
    for (; list != NULL; list = list->next, i++) {
      // check channel track(s) for audio blocks
      if (!is_empty_track(LIVES_WIDGET_OBJECT(list->data))) {
        if (get_mixer_track_vol(mt, i) == 0.5) {
          has_channel_audio = TRUE;
        }
      }
    }

    // first checks done ^

    if (has_backing_audio && !has_channel_audio) {
      // backing but no channel audio

      // ->
      // if ALL backing levels are at 0.5, set them to 1.0
      list = mt->audio_draws;
      for (i = 0; i < mt->opts.back_audio_tracks; i++, list = list->next) {
        if (!is_empty_track(LIVES_WIDGET_OBJECT(list->data)));
        if (get_mixer_track_vol(mt, i) != 0.5) {
          has_backing_audio = FALSE;
          break;
        }
      }
    }

    if (has_backing_audio) {
      post_reset_ba = TRUE; // reset levels after rendering
      list = mt->audio_draws;
      for (i = 0; i < mt->opts.back_audio_tracks; i++, list = list->next) {
        if (!is_empty_track(LIVES_WIDGET_OBJECT(list->data))) {
          set_mixer_track_vol(mt, i, 1.0);
        }
      }
    }

    if (!has_backing_audio && has_channel_audio) {
      // channel but no backing audio

      // ->
      // if ALL channel levels are at 0.5, set them all to 1.0
      list = mt->audio_draws;
      for (i = mt->opts.back_audio_tracks + 1; --i > 0; list = list->next);
      // check channel track(s) for audio blocks
      if (!is_empty_track(LIVES_WIDGET_OBJECT(list->data))) {
        if (get_mixer_track_vol(mt, i) != 0.5) {
          has_channel_audio = FALSE;
        }
      }
    }

    if (has_channel_audio) {
      post_reset_ca = TRUE; // reset levels after rendering
      list = mt->audio_draws;
      for (i = 0; i < mt->opts.back_audio_tracks; list = list->next);
      // check channel track(s) for audio blocks
      for (; list != NULL; list = list->next) {
        if (!is_empty_track(LIVES_WIDGET_OBJECT(list->data))) {
          // set to 1.0
          set_mixer_track_vol(mt, i++, 1.0);
	    // *INDENT-OFF*
	  }}}}
    // *INDENT-ON*

  if (render_to_clip(FALSE)) {
    // rendering was successful

#if 0
    if (mt->pr_audio) {
      mt->pr_audio = FALSE;
      d_print_done();
      lives_widget_set_sensitive(mt->render_vid, TRUE);
      lives_widget_set_sensitive(mt->render_aud, TRUE);
      lives_widget_set_sensitive(mt->normalise_aud, TRUE);
      mt->idlefunc = mt_idle_add(mt);
      return FALSE;
    }
#endif

    cfile->start = cfile->frames > 0 ? 1 : 0;
    cfile->end = cfile->frames;
    if (cfile->frames == 0) {
      cfile->hsize = cfile->vsize = 0;
    }
    set_undoable(NULL, FALSE);
    cfile->changed = TRUE;
    add_to_clipmenu();
    mt->file_selected = orig_file = mainw->current_file;
    d_print(_("rendered %d frames to new clip.\n"), cfile->frames);
    if (mainw->scrap_file != -1 || mainw->ascrap_file != -1) mt->changed = FALSE;
    mt->is_rendering = FALSE;

    save_clip_values(orig_file);

    if (prefs->crash_recovery) add_to_recovery_file(cfile->handle);
    reset_clipmenu();

    if (post_reset_ba) {
      // reset after normalising backing audio
      for (i = 0; i < mt->opts.back_audio_tracks; i++) {
        if (!is_empty_track(LIVES_WIDGET_OBJECT(lives_list_nth_data(mt->audio_draws, i)))) {
          if (get_mixer_track_vol(mt, i) == 1.0) {
            set_mixer_track_vol(mt, i, 0.5);
	    // *INDENT-OFF*
          }}}}

    if (post_reset_ca) {
      // reset after normalising channel audio
      list = mt->audio_draws;
      for (i = 0; i < mt->opts.back_audio_tracks; list = list->next);
      // check channel track(s) for audio blocks
      for (; list != NULL; list = list->next) {
	if (!is_empty_track(LIVES_WIDGET_OBJECT(list->data))) {
	  if (get_mixer_track_vol(mt, i) == 1.0) {
	    set_mixer_track_vol(mt, i, 0.5);
	    // *INDENT-OFF*
          }}}}
    // *INDENT-ON*


    mainw->current_file = mainw->first_free_file;

    if (!get_new_handle(mainw->current_file, NULL)) {
      mainw->current_file = orig_file;
      if (!multitrack_end(NULL, user_data)) switch_to_file((mainw->current_file = 0), orig_file);
      mt->idlefunc = mt_idle_add(mt);
      return FALSE;
    }

    cfile->hsize = mainw->files[orig_file]->hsize;
    cfile->vsize = mainw->files[orig_file]->vsize;

    cfile->img_type = mainw->files[orig_file]->img_type;

    cfile->pb_fps = cfile->fps = mainw->files[orig_file]->fps;
    cfile->ratio_fps = mainw->files[orig_file]->ratio_fps;

    cfile->arate = arate;
    cfile->arps = arps;
    cfile->asampsize = asampsize;

    cfile->achans = achans;
    cfile->signed_endian = signed_endian;

    cfile->bpp = cfile->img_type == IMG_TYPE_JPEG ? 24 : 32;
    cfile->changed = TRUE;
    cfile->is_loaded = TRUE;

    cfile->old_frames = cfile->frames;

    mt->render_file = mainw->current_file;

    if (prefs->mt_exit_render) {
      if (multitrack_end(menuitem, user_data)) return TRUE;
    }

    mt_init_clips(mt, orig_file, TRUE);
    if (mt->idlefunc > 0) lives_source_remove(mt->idlefunc);
    mt->idlefunc = 0;
    lives_widget_context_update();
    mt_clip_select(mt, TRUE);

    retval = TRUE;
  } else {
    char *curworkdir;
    // rendering failed - clean up

    cfile->frames = cfile->start = cfile->end = 0;
    mt->is_rendering = FALSE;

    mainw->event_list = NULL;
    if (mt->pr_audio) {
      com = lives_strdup_printf("%s undo_audio \"%s\"", prefs->backend_sync, cfile->handle);
      lives_system(com, FALSE);
      lives_free(com);
      mt->has_audio_file = had_audio;
    } else {
      // remove subdir
      do_threaded_dialog(_("Cleaning up..."), FALSE);
      curworkdir = lives_build_filename(prefs->workdir, cfile->handle, NULL);
      lives_rmdir(curworkdir, TRUE);
      end_threaded_dialog();
    }
  }

  // enable GUI for next rendering
  lives_widget_set_sensitive(mt->render_vid, TRUE);
  lives_widget_set_sensitive(mt->render_aud, TRUE);
  lives_widget_set_sensitive(mt->normalise_aud, TRUE);
  mt_sensitise(mt);

  mt->idlefunc = mt_idle_add(mt);

  return retval;
}


void on_prerender_aud_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  on_render_activate(menuitem, user_data);
  mainw->is_rendering = mainw->internal_messaging = mt->is_rendering = FALSE;
  mt_sensitise(mt);
  lives_widget_set_sensitive(mt->prerender_aud, FALSE);
}


void update_filter_events(lives_mt * mt, weed_plant_t *first_event, weed_timecode_t start_tc, weed_timecode_t end_tc,
                          int track, weed_timecode_t new_start_tc, int new_track) {
  // move/remove filter_inits param_change and filter_deinits after deleting/moving a block

  // first_event: event just before block which is removed

  // start_tc, end_tc start and end timecodes of block on track

  // new_start_tc, new_track: new positions

  //if block is being deleted, or moved and move_effects is FALSE, this is called during block_delete to remove effects

  //if block is being moved and move_effects is TRUE, this is called after block was deleted and reinserted

  // filters which do not have our deleted block as an input, and filters with >2 in channels are not affected

  // filters with 1 in track (ours) are moved to the new block, if the option is set

  // other filters which are affected are deleted if their init/deinit reside entirely in the old block:
  // otherwise,
  // if their init_event is within the old block it moves right until we hit a frame
  // on the same track (+ the second track if applicable). If we pass the deinit_event, the filter is removed.

  // then, if the deinit event is within the old block, it is moved left until we find the frames for it similarly

  LiVESList *moved_events = NULL;

  weed_plant_t *event, *event_next;
  weed_plant_t *init_event, *deinit_event;

  weed_timecode_t last_frame_tc = 0, event_tc;

  boolean was_moved;
  boolean leave_event;

  int nins;
  int error;

  register int i;

  event = get_last_frame_event(mt->event_list);
  if (event != NULL) last_frame_tc = get_event_timecode(event);

  // find first event inside old block
  if (first_event == NULL) event = get_first_event(mt->event_list);
  else event = get_next_event(first_event);
  while (event != NULL && get_event_timecode(event) < start_tc) event = get_next_event(event);

  while (event != NULL && get_event_timecode(event) <= end_tc) {
    // step through all events in old block
    event_next = get_next_event(event);
    was_moved = FALSE;

    if (WEED_EVENT_IS_FILTER_INIT(event)) {
      // filter init event
      if (event == mt->avol_init_event) {
        event = event_next;
        continue;  // we move our audio volume effect using a separate mechanism
      }
      if (mt->opts.move_effects && mt->moving_block) {
        // move effects

        if (weed_plant_has_leaf(event, WEED_LEAF_DEINIT_EVENT) && weed_leaf_num_elements(event, WEED_LEAF_IN_TRACKS) == 1
            && weed_get_int_value(event, WEED_LEAF_IN_TRACKS, &error) == track) {
          // this effect has a deinit_event, it has one in_track, which is this one

          deinit_event = weed_get_plantptr_value(event, WEED_LEAF_DEINIT_EVENT, &error);
          if (get_event_timecode(deinit_event) <= end_tc) {
            //if the effect also ends within the block, we will move it to the new block

            if (lives_list_index(moved_events, event) == -1) {
              // update owners,in_tracks and out_tracks
              weed_set_int_value(event, WEED_LEAF_IN_TRACKS, new_track); // update the in_track to the new one

              if (weed_plant_has_leaf(event, WEED_LEAF_OUT_TRACKS)) {
                int num_tracks;
                int *out_tracks = weed_get_int_array_counted(event, WEED_LEAF_OUT_TRACKS, &num_tracks);
                for (i = 0; i < num_tracks; i++) {
                  // update the out_track to the new one
                  if (out_tracks[i] == track) out_tracks[i] = new_track;
                }
                weed_set_int_array(event, WEED_LEAF_OUT_TRACKS, num_tracks, out_tracks);
                lives_free(out_tracks);
              }

              // move to new position
              if (new_start_tc < start_tc) {
                // if moving earlier, we need to move the init_event first, then the deinit_event
                // this will also update the filter_maps, and param_changes
                move_filter_init_event(mt->event_list, get_event_timecode(event) + new_start_tc - start_tc, event, mt->fps);
                move_filter_deinit_event(mt->event_list, get_event_timecode(deinit_event) + new_start_tc - start_tc,
                                         deinit_event, mt->fps, TRUE);
                if (event == first_event) first_event = NULL;
                was_moved = TRUE;
              } else if (new_start_tc > start_tc) {
                // if moving later, we need to move the deinit_event first, then the init_event
                // this will also update the filter_maps, and param_changes
                move_filter_deinit_event(mt->event_list, get_event_timecode(deinit_event) + new_start_tc - start_tc,
                                         deinit_event, mt->fps, TRUE);
                move_filter_init_event(mt->event_list, get_event_timecode(event) + new_start_tc - start_tc, event, mt->fps);
                if (event == first_event) first_event = NULL;
                was_moved = TRUE;
              }
              // add this effect to our list of moved_events, so we don't end up moving it multiple times
              moved_events = lives_list_prepend(moved_events, event);
	      // *INDENT-OFF*
            }}}}
      // *INDENT-ON*

      if (lives_list_index(moved_events, event) == -1 && event != mt->avol_init_event) {
        if (weed_plant_has_leaf(event, WEED_LEAF_DEINIT_EVENT) && weed_plant_has_leaf(event, WEED_LEAF_IN_TRACKS) &&
            (nins = weed_leaf_num_elements(event, WEED_LEAF_IN_TRACKS)) <= 2) {
          int *in_tracks = weed_get_int_array(event, WEED_LEAF_IN_TRACKS, &error);
          if (in_tracks[0] == track || (nins == 2 && in_tracks[1] == track)) {
            // if the event wasnt moved (either because user chose not to, or block was deleted, or it had 2 tracks)
            // move the init_event to the right until we find frames from all tracks. If we pass the deinit_event then
            // the effect is removed.
            // Effects with one in_track which is other, or effects with >2 in tracks, do not suffer this fate.

            deinit_event = weed_get_plantptr_value(event, WEED_LEAF_DEINIT_EVENT, &error);

            if (get_event_timecode(deinit_event) <= end_tc) {
              remove_filter_from_event_list(mt->event_list, event);
              was_moved = TRUE;
              if (event == first_event) first_event = NULL;
            } else {
              if (!move_event_right(mt->event_list, event, TRUE, mt->fps)) {
                // moved event right until it hits a frame from all tracks, if it passed the deinit_event it is removed
                // param_change events are also scaled in time
                was_moved = TRUE;
                if (event == first_event) first_event = NULL;
              }
            }
          }
          lives_free(in_tracks);
        }
      }
    } else {
      leave_event = TRUE;
      if (WEED_EVENT_IS_FILTER_DEINIT(event)) {
        // check filter deinit
        if (mt->opts.move_effects && mt->moving_block) {
          if (weed_plant_has_leaf(event, WEED_LEAF_INIT_EVENT)) {
            init_event = (weed_plant_t *)weed_get_voidptr_value(event, WEED_LEAF_INIT_EVENT, &error);
            event_tc = get_event_timecode(event);
            if (init_event != mt->avol_init_event &&
                (event_tc > last_frame_tc ||
                 (lives_list_index(moved_events, init_event) == -1 &&
                  weed_plant_has_leaf(event, WEED_LEAF_IN_TRACKS)
                  && (nins = weed_leaf_num_elements(event, WEED_LEAF_IN_TRACKS)) <= 2))) {
              // move it if: it is not avol event, and either it is after all frames or init_event was not moved
              // and it has one or two tracks, one of which is our track
              if (event_tc <= last_frame_tc) {
                int *in_tracks = weed_get_int_array(event, WEED_LEAF_IN_TRACKS, &error);
                if (in_tracks[0] == track || (nins == 2 && in_tracks[1] == track)) {
                  leave_event = FALSE;
                }
                lives_free(in_tracks);
              } else leave_event = FALSE;
            }
          }
        }
        if (!leave_event && !move_event_left(mt->event_list, event, TRUE, mt->fps)) {
          // move the event left until it hits a frame from all tracks
          // if it passes the init_event, it is removed
          // param change events are also scaled in time
          was_moved = TRUE;
        }
      }
    }
    if (was_moved) {
      // if we moved an event, re-scan from the start of the old block
      if (first_event == NULL) event = get_first_event(mt->event_list);
      else event = get_next_event(first_event);
      while (event != NULL && get_event_timecode(event) < start_tc) event = get_next_event(event);
    } else {
      event = event_next;
      if (WEED_EVENT_IS_FRAME(event)) first_event = event;
    }
  }
  if (moved_events != NULL) lives_list_free(moved_events);
}


void on_split_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  // split current block at current time
  lives_mt *mt = (lives_mt *)user_data;

  weed_timecode_t tc;

  double timesecs = mt->ptr_time;

  boolean did_backup = mt->did_backup;
  boolean needs_idlefunc = FALSE;

  if (mt->putative_block == NULL) return;

  if (mt->idlefunc > 0) {
    needs_idlefunc = TRUE;
    lives_source_remove(mt->idlefunc);
    mt->idlefunc = 0;
  }

  if (mt->context_time != -1. && mt->use_context) {
    timesecs = mt->context_time;
    mt->context_time = -1.;
    mt->use_context = FALSE;
  }

  mt_backup(mt, MT_UNDO_SPLIT, 0);

  tc = q_gint64(timesecs * TICKS_PER_SECOND_DBL, mt->fps);

  split_block(mt, mt->putative_block, tc, mt->current_track, FALSE);

  if (!did_backup) {
    mt->did_backup = FALSE;
    if (needs_idlefunc || (!did_backup && mt->auto_changed)) mt->idlefunc = mt_idle_add(mt);
    if (prefs->mt_auto_back == 0) mt_auto_backup(mt);
  }
}


void on_split_curr_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  // split current track at current time
  lives_mt *mt = (lives_mt *)user_data;
  double timesecs = mt->ptr_time;
  boolean did_backup = mt->did_backup;
  boolean needs_idlefunc = FALSE;
  weed_timecode_t tc;
  LiVESWidget *eventbox;
  track_rect *block;

  if (mt->idlefunc > 0) {
    needs_idlefunc = TRUE;
    lives_source_remove(mt->idlefunc);
    mt->idlefunc = 0;
  }

  tc = q_gint64(timesecs * TICKS_PER_SECOND_DBL, mt->fps);

  if (mt->current_track == -1) eventbox = (LiVESWidget *)mt->audio_draws->data;
  else eventbox = (LiVESWidget *)lives_list_nth_data(mt->video_draws, mt->current_track);

  block = get_block_from_time(eventbox, timesecs, mt);

  if (block == NULL) {
    if (!did_backup) {
      if (needs_idlefunc || (!did_backup && mt->auto_changed)) mt->idlefunc = mt_idle_add(mt);
    }
    return;
  }

  mt_backup(mt, MT_UNDO_SPLIT, 0);
  split_block(mt, block, tc, mt->current_track, FALSE);

  if (!did_backup) {
    mt->did_backup = FALSE;
    if (needs_idlefunc || (!did_backup && mt->auto_changed)) mt->idlefunc = mt_idle_add(mt);
    if (prefs->mt_auto_back == 0) mt_auto_backup(mt);
  }
}


void on_split_sel_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  // split selected tracks at current time
  lives_mt *mt = (lives_mt *)user_data;
  LiVESList *selt = mt->selected_tracks;
  LiVESWidget *eventbox;
  int track;
  track_rect *block;
  double timesecs = mt->ptr_time;
  boolean did_backup = mt->did_backup;
  boolean needs_idlefunc = FALSE;

  if (mt->selected_tracks == NULL) return;

  if (mt->idlefunc > 0) {
    needs_idlefunc = TRUE;
    lives_source_remove(mt->idlefunc);
    mt->idlefunc = 0;
  }

  mt_backup(mt, MT_UNDO_SPLIT_MULTI, 0);

  while (selt != NULL) {
    track = LIVES_POINTER_TO_INT(selt->data);
    eventbox = (LiVESWidget *)lives_list_nth_data(mt->video_draws, track);
    block = get_block_from_time(eventbox, timesecs, mt);
    if (block != NULL) split_block(mt, block, timesecs * TICKS_PER_SECOND_DBL, track, FALSE);
    selt = selt->next;
  }

  if (!did_backup) {
    mt->did_backup = FALSE;
    if (needs_idlefunc || (!did_backup && mt->auto_changed)) mt->idlefunc = mt_idle_add(mt);
    if (prefs->mt_auto_back == 0) mt_auto_backup(mt);
  }
}


static void on_delblock_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  weed_timecode_t start_tc, end_tc;
  weed_plant_t *first_event;
  weed_plant_t *event, *prevevent;

  track_rect *block, *blockprev, *blocknext;

  LiVESWidget *eventbox, *aeventbox;

  char *tmp;
  char *tstart, *tend;

  boolean done = FALSE;
  boolean did_backup = mt->did_backup;
  boolean needs_idlefunc = FALSE;

  int track;

  if (mt->is_rendering) return;

  if (mt->idlefunc > 0) {
    needs_idlefunc = TRUE;
    lives_source_remove(mt->idlefunc);
    mt->idlefunc = 0;
  }

  mt->context_time = -1.;

  if (mt->current_track != -1) mt_backup(mt, MT_UNDO_DELETE_BLOCK, 0);
  else mt_backup(mt, MT_UNDO_DELETE_AUDIO_BLOCK, 0);

  if (mt->block_selected == NULL) mt->block_selected = mt->putative_block;
  block = mt->block_selected;
  eventbox = block->eventbox;

  if (mt->current_track != -1) track = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox),
                                         "layer_number"));
  else track = -1;

  if ((aeventbox = LIVES_WIDGET(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "atrack"))) != NULL) {
    int current_track = mt->current_track;
    mt->current_track = track;
    mt->block_selected = get_block_from_time(aeventbox, get_event_timecode(block->start_event) / TICKS_PER_SECOND_DBL, mt);
    if (mt->block_selected != NULL) on_delblock_activate(NULL, user_data);
    mt->block_selected = block;
    mt->current_track = current_track;
  }

  mt_desensitise(mt);

  start_tc = get_event_timecode(block->start_event);
  end_tc = get_event_timecode(block->end_event);

  first_event = get_prev_event(block->start_event);
  while (first_event != NULL && get_event_timecode(first_event) == start_tc) {
    first_event = get_prev_event(first_event);
  }

  event = block->end_event;

  if (mt->current_track != -1 && !is_audio_eventbox(eventbox)) {
    // delete frames
    while (event != NULL && !done) {
      prevevent = get_prev_frame_event(event);
      if (event == block->start_event) done = TRUE;
      remove_frame_from_event(mt->event_list, event, track);
      if (!done) event = prevevent;
    }
  } else {
    // update audio events
    // if last event in block turns audio off, delete it
    if (get_audio_frame_vel(block->end_event, track) == 0.) {
      remove_audio_for_track(block->end_event, track);
    }

    // if first event in block is the end of another block, turn audio off (velocity==0)
    if (block->prev != NULL && block->start_event == block->prev->end_event) {
      insert_audio_event_at(block->start_event, track, 1, 0., 0.);
    }
    // else we'll delete it
    else {
      remove_audio_for_track(block->start_event, track);
    }
  }

  if ((blockprev = block->prev) != NULL) blockprev->next = block->next;
  else lives_widget_object_set_data(LIVES_WIDGET_OBJECT(eventbox), "blocks", (livespointer)block->next);
  if ((blocknext = block->next) != NULL) blocknext->prev = blockprev;

  lives_free(block);

  lives_widget_queue_draw(eventbox);
  if (cfile->achans > 0 && mt->audio_draws != NULL && mt->opts.back_audio_tracks > 0 && eventbox == mt->audio_draws->data &&
      LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "expanded"))) {
    LiVESWidget *xeventbox = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "achan0");
    if (xeventbox != NULL) lives_widget_queue_draw(xeventbox);
    xeventbox = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "achan1");
    if (xeventbox != NULL) lives_widget_queue_draw(xeventbox);
  }

  tmp = get_track_name(mt, mt->current_track, FALSE);

  tstart = time_to_string(QUANT_TICKS(start_tc));
  tend = time_to_string(QUANT_TICKS(end_tc + TICKS_PER_SECOND_DBL / mt->fps));

  if (mt->current_track != -1 && !is_audio_eventbox(eventbox)) {
    d_print(_("Deleted frames from time %s to time %s on track %s\n"), tstart, tend, tmp);
  } else {
    d_print(_("Deleted audio from time %s to time %s on track %s\n"), tstart, tend, tmp);
  }
  lives_free(tmp);
  lives_free(tstart);
  lives_free(tend);

  if ((mt->opts.grav_mode == GRAV_MODE_LEFT || mt->opts.grav_mode == GRAV_MODE_RIGHT) && !mt->moving_block && !did_backup) {
    // gravity left - remove first gap from old block start to end time
    // gravity right - remove last gap from 0 to old block end time

    double oldr_start = mt->region_start;
    double oldr_end = mt->region_end;
    LiVESList *tracks_sel = NULL;
    if (mt->current_track != -1) {
      tracks_sel = lives_list_copy(mt->selected_tracks);
      if (mt->selected_tracks != NULL) lives_list_free(mt->selected_tracks);
      mt->selected_tracks = NULL;
      mt->selected_tracks = lives_list_append(mt->selected_tracks, LIVES_INT_TO_POINTER(mt->current_track));
    }

    if (mt->opts.grav_mode == GRAV_MODE_LEFT) {
      mt->region_start = start_tc / TICKS_PER_SECOND_DBL;
      mt->region_end = mt->end_secs;
    } else {
      mt->region_start = 0.;
      mt->region_end = end_tc / TICKS_PER_SECOND_DBL;
    }

    remove_first_gaps(NULL, mt);
    if (mt->current_track > -1) {
      lives_list_free(mt->selected_tracks);
      mt->selected_tracks = lives_list_copy(tracks_sel);
      if (tracks_sel != NULL) lives_list_free(tracks_sel);
    }
    mt->region_start = oldr_start;
    mt->region_end = oldr_end;
    mt_sensitise(mt);
  }

  remove_end_blank_frames(mt->event_list, FALSE); // leave filter_inits

  if (!mt->opts.move_effects || !mt->moving_block) {
    update_filter_events(mt, first_event, start_tc, end_tc, track, start_tc, track);
    if (mt->block_selected == block) {
      mt->block_selected = NULL;
      unselect_all(mt);
    }
    mt_sensitise(mt);
  }

  remove_end_blank_frames(mt->event_list, TRUE); // remove filter inits

  if ((!mt->moving_block || get_first_frame_event(mt->event_list) == NULL) && mt->avol_fx != -1 && blocknext == NULL &&
      mt->audio_draws != NULL && get_first_event(mt->event_list) != NULL) {
    apply_avol_filter(mt);
  }

  if (!did_backup && mt->framedraw != NULL && mt->current_rfx != NULL && mt->init_event != NULL &&
      mt->poly_state == POLY_PARAMS && weed_plant_has_leaf(mt->init_event, WEED_LEAF_IN_TRACKS)) {
    weed_timecode_t tc = q_gint64(lives_spin_button_get_value(LIVES_SPIN_BUTTON(mt->node_spinbutton)) * TICKS_PER_SECOND_DBL +
                                  get_event_timecode(mt->init_event), mt->fps);
    get_track_index(mt, tc);
  }

  if (!mt->moving_block) {
    redraw_eventbox(mt, eventbox);
    paint_lines(mt, mt->ptr_time, TRUE);
    mt_show_current_frame(mt, FALSE);
  }

  if (!did_backup) {
    mt->did_backup = FALSE;
    if (needs_idlefunc || (!did_backup && mt->auto_changed)) mt->idlefunc = mt_idle_add(mt);
    if (prefs->mt_auto_back == 0) mt_auto_backup(mt);
  }

  mt_desensitise(mt);
  mt_sensitise(mt);

  if (mt->event_list == NULL) recover_layout_cancelled(FALSE);
}


void mt_selection_lock(LiVESMenuItem * menuitem, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  mt->sel_locked = lives_check_menu_item_get_active(LIVES_CHECK_MENU_ITEM(menuitem));
  lives_widget_set_sensitive(mt->spinbutton_start, !mt->sel_locked);
  lives_widget_set_sensitive(mt->spinbutton_end, !mt->sel_locked);
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

  if (mt->selected_tracks != NULL) {
    if (mt->event_list != NULL && get_first_event(mt->event_list) != NULL) {
      lives_widget_set_sensitive(mt->split_sel, mt_selblock(NULL, (livespointer)mt) != NULL);
    }

    if (mt->region_start != mt->region_end) {
      if (mt->event_list != NULL && get_first_event(mt->event_list) != NULL) {
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


void on_seltrack_toggled(LiVESWidget * checkbutton, livespointer user_data) {
  int track = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(checkbutton), "layer_number"));
  lives_mt *mt = (lives_mt *)user_data;

  if (!mainw->interactive) return;

  mt->current_track = track;

  if (track > -1) mt->aud_track_selected = FALSE;
  else mt->aud_track_selected = TRUE;

  // track_select will call on_seltrack_activate, which will set our new state
  track_select(mt);

}


void mt_desensitise(lives_mt * mt) {
  double val;
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
  lives_widget_set_sensitive(mt->save_event_list, FALSE);
  lives_widget_set_sensitive(mt->load_event_list, FALSE);
  lives_widget_set_sensitive(mt->clear_event_list, FALSE);
  lives_widget_set_sensitive(mt->remove_gaps, FALSE);
  lives_widget_set_sensitive(mt->remove_first_gaps, FALSE);
  lives_widget_set_sensitive(mt->undo, FALSE);
  lives_widget_set_sensitive(mt->redo, FALSE);
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

  if (mt->poly_state == POLY_IN_OUT) {
    if (mt->block_selected != NULL) {
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

  if (mt->event_list != NULL && get_first_event(mt->event_list) != NULL) {
    lives_widget_set_sensitive(mt->playall, TRUE);
    lives_widget_set_sensitive(mainw->m_playbutton, TRUE);
    lives_widget_set_sensitive(mt->view_events, TRUE);
    lives_widget_set_sensitive(mt->view_sel_events, mt->region_start != mt->region_end);
    lives_widget_set_sensitive(mt->render, TRUE);
    if (mt->avol_init_event != NULL && mt->opts.pertrack_audio && mainw->files[mt->render_file]->achans > 0)
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
  }

  if (mt->event_list != NULL) lives_widget_set_sensitive(mt->clear_event_list, TRUE);

  lives_widget_set_sensitive(mt->add_vid_behind, TRUE);
  lives_widget_set_sensitive(mt->add_vid_front, TRUE);
  lives_widget_set_sensitive(mt->quit, TRUE);
  lives_widget_set_sensitive(mt->clear_ds, TRUE);
  lives_widget_set_sensitive(mt->open_menu, TRUE);
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
  if (mt->selected_init_event != NULL) lives_widget_set_sensitive(mt->fx_edit, TRUE);
  if (mt->selected_init_event != NULL) lives_widget_set_sensitive(mt->fx_delete, TRUE);
  lives_widget_set_sensitive(mt->checkbutton_avel_reverse, TRUE);

  if (mt->block_selected != NULL && (!mt->block_selected->start_anchored ||
                                     !mt->block_selected->end_anchored) && !lives_toggle_button_get_active
      (LIVES_TOGGLE_BUTTON(mt->checkbutton_avel_reverse))) {
    lives_widget_set_sensitive(mt->spinbutton_avel, TRUE);
    lives_widget_set_sensitive(mt->avel_scale, TRUE);
  }

  lives_widget_set_sensitive(mt->load_event_list, strlen(mainw->set_name) > 0);
  lives_widget_set_sensitive(mt->clipedit, TRUE);
  if (mt->file_selected > -1) {
    if (mainw->files[mt->file_selected]->frames > 0) lives_widget_set_sensitive(mt->insert, TRUE);
    if (mainw->files[mt->file_selected]->achans > 0 && mainw->files[mt->file_selected]->laudio_time > 0.)
      lives_widget_set_sensitive(mt->audio_insert, TRUE);
    lives_widget_set_sensitive(mt->save_set, !mainw->recording_recovered);
    lives_widget_set_sensitive(mt->close, TRUE);
    lives_widget_set_sensitive(mt->adjust_start_end, TRUE);
  }

  if (mt->video_draws != NULL &&
      mt->current_track > -1) eventbox = (LiVESWidget *)lives_list_nth_data(mt->video_draws, mt->current_track);
  else if (mt->audio_draws != NULL) eventbox = (LiVESWidget *)mt->audio_draws->data;

  if (eventbox != NULL) {
    lives_widget_set_sensitive(mt->jumpback, lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "blocks") != NULL);
    lives_widget_set_sensitive(mt->jumpnext, lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "blocks") != NULL);
  }

  if (mt->tl_marks != NULL) {
    lives_widget_set_sensitive(mt->mark_jumpback, TRUE);
    lives_widget_set_sensitive(mt->mark_jumpnext, TRUE);
  }

  lives_widget_set_sensitive(mt->change_vals, TRUE);

  if (mt->block_selected) {
    lives_widget_set_sensitive(mt->delblock, TRUE);
    if (mt->poly_state == POLY_IN_OUT && mt->block_selected->ordered) {
      weed_timecode_t offset_end = mt->block_selected->offset_start + (weed_timecode_t)(TICKS_PER_SECOND_DBL / mt->fps) +
                                   (get_event_timecode(mt->block_selected->end_event) - get_event_timecode(mt->block_selected->start_event));
      set_in_out_spin_ranges(mt, mt->block_selected->offset_start, offset_end);
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

  if (mt->region_end > mt->region_start && mt->event_list != NULL && get_first_event(mt->event_list) != NULL) {
    if (mt->selected_tracks != NULL) {
      lives_widget_set_sensitive(mt->fx_region, TRUE);
      lives_widget_set_sensitive(mt->ins_gap_sel, TRUE);
      lives_widget_set_sensitive(mt->remove_gaps, TRUE);
      lives_widget_set_sensitive(mt->remove_first_gaps, TRUE);
      lives_widget_set_sensitive(mt->fx_region, TRUE);
      if (mt->selected_tracks != NULL && mt->region_end != mt->region_start) {
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

  if (freeze_closure == NULL) freeze_closure = lives_cclosure_new(LIVES_GUI_CALLBACK(freeze_callback), NULL, NULL);

  if (put_pause) {
#if GTK_CHECK_VERSION(2, 6, 0)
    tmp_img = lives_image_new_from_stock(LIVES_STOCK_MEDIA_PAUSE, lives_toolbar_get_icon_size(LIVES_TOOLBAR(mt->btoolbar)));
#endif
    lives_menu_item_set_text(mt->playall, _("_Pause"), TRUE);
    lives_widget_set_tooltip_text(mainw->m_playbutton, _("Pause (p)"));
    lives_widget_set_sensitive(mt->playall, TRUE);
    lives_widget_set_sensitive(mainw->m_playbutton, TRUE);
    lives_accel_group_connect(LIVES_ACCEL_GROUP(mt->accel_group), LIVES_KEY_BackSpace,
                              (LiVESXModifierType)LIVES_CONTROL_MASK,
                              (LiVESAccelFlags)0, freeze_closure);

  } else {
    tmp_img = lives_image_new_from_stock(LIVES_STOCK_MEDIA_PLAY, lives_toolbar_get_icon_size(LIVES_TOOLBAR(mt->btoolbar)));
    lives_menu_item_set_text(mt->playall, _("_Play from Timeline Position"), TRUE);
    lives_widget_set_tooltip_text(mainw->m_playbutton, _("Play all (p)"));
    lives_accel_group_disconnect(LIVES_ACCEL_GROUP(mt->accel_group), freeze_closure);
    freeze_closure = NULL;
  }

  if (tmp_img != NULL) lives_widget_show(tmp_img);
  lives_tool_button_set_icon_widget(LIVES_TOOL_BUTTON(mainw->m_playbutton), tmp_img);
}


void multitrack_preview_clicked(LiVESWidget * button, livespointer user_data) {
  //preview during rendering
  lives_mt *mt = (lives_mt *)user_data;

  if (!LIVES_IS_PLAYING) multitrack_playall(mt);
  else mainw->cancelled = CANCEL_NO_PROPOGATE;
}


void mt_prepare_for_playback(lives_mt * mt) {
  // called from on_preview_clicked

  pb_loop_event = mt->pb_loop_event;
  pb_filter_map = mainw->filter_map; // keep a copy of this, in case we are rendering
  pb_afilter_map = mainw->afilter_map; // keep a copy of this, in case we are rendering
  pb_audio_needs_prerender = lives_widget_is_sensitive(mt->prerender_aud);

  mt_desensitise(mt);

  if (mt->mt_frame_preview) {
    // put blank back in preview window
    if (palette->style & STYLE_1) {
      if (mt->framedraw != NULL) lives_widget_set_bg_color(mt->fd_frame, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
    }
  } else {
    lives_widget_object_ref(mt->play_blank);
    lives_container_remove(LIVES_CONTAINER(mt->play_box), mt->play_blank);
  }

  lives_widget_set_sensitive(mt->stop, TRUE);
  lives_widget_set_sensitive(mt->rewind, FALSE);
  lives_widget_set_sensitive(mainw->m_rewindbutton, FALSE);
  //if (!mt->is_paused&&!mt->playing_sel) mt->ptr_time=lives_ruler_get_value(LIVES_RULER(mt->timeline));
}


void mt_post_playback(lives_mt * mt) {
  // called from on_preview_clicked
  unhide_cursor(lives_widget_get_xwindow(mainw->playarea));

  lives_widget_show(mainw->playarea);

  if (mainw->cancelled != CANCEL_USER_PAUSED && !((mainw->cancelled == CANCEL_NONE ||
      mainw->cancelled == CANCEL_NO_MORE_PREVIEW) && mt->is_paused)) {
    lives_widget_set_sensitive(mt->stop, FALSE);
    mt->no_frame_update = FALSE;

    mt_tl_move(mt, mt->pb_unpaused_start_time);

    if (mt->opts.follow_playback) {
      double currtime = mt->ptr_time;
      if (currtime > mt->tl_max || currtime < mt->tl_min) {
        mt_zoom(mt, 1.);
      }
    }
  } else {
    double curtime;
    mt->is_paused = TRUE;
    if ((curtime = mt->ptr_time) > 0.) {
      lives_widget_set_sensitive(mt->rewind, TRUE);
      lives_widget_set_sensitive(mainw->m_rewindbutton, TRUE);
    }
    paint_lines(mt, curtime, TRUE);
    mt->no_frame_update = FALSE;
    mt_show_current_frame(mt, FALSE);
  }

  mainw->cancelled = CANCEL_NONE;

  if (!mt->is_rendering) {
    if (mt->poly_state == POLY_PARAMS) {
      if (mt->init_event != NULL) {
        lives_spin_button_set_value(LIVES_SPIN_BUTTON(mt->node_spinbutton),
                                    mt->ptr_time - get_event_timecode(mt->init_event) / TICKS_PER_SECOND_DBL);
        lives_widget_set_sensitive(mt->apply_fx_button, FALSE);
        lives_widget_set_sensitive(mt->resetp_button, check_can_resetp(mt));
      }
    }
    if (mt->poly_state == POLY_FX_STACK) {
      polymorph(mt, POLY_FX_STACK);
    }
  }

  if (mt->ptr_time > 0.) {
    lives_widget_set_sensitive(mt->rewind, TRUE);
    lives_widget_set_sensitive(mainw->m_rewindbutton, TRUE);
  }

  mainw->filter_map = pb_filter_map;
  mainw->afilter_map = pb_afilter_map;

  if (mt->is_paused) mt->pb_loop_event = pb_loop_event;
  lives_widget_set_sensitive(mainw->m_playbutton, TRUE);
  paint_lines(mt, mt->ptr_time, FALSE);
}


void multitrack_playall(lives_mt * mt) {
  LiVESWidget *old_context_scroll = mt->context_scroll;
  boolean needs_idlefunc = FALSE;

  if (!CURRENT_CLIP_IS_VALID) return;
  mt->no_expose_frame = TRUE;
  mt->no_frame_update = TRUE;

  if (mt->idlefunc > 0) {
    lives_source_remove(mt->idlefunc);
    mt->idlefunc = 0;
    needs_idlefunc = TRUE;
  }

  lives_widget_set_sensitive(mt->quit, TRUE);

  lives_widget_object_ref(mt->context_scroll); // this allows us to get our old messages back
  lives_container_remove(LIVES_CONTAINER(mt->context_frame), mt->context_scroll);
  mt->context_scroll = NULL;
  clear_context(mt);

  add_context_label(mt, _("Press 'm' during playback"));
  add_context_label(mt, _("to make a mark on the timeline"));

  add_context_label(mt, "\n\n");

  add_context_label(mt, _("Press 't' during playback"));
  add_context_label(mt, _("to toggle timecode overlay"));

  if (mt->opts.follow_playback) {
    double currtime = mt->ptr_time;
    if (currtime > mt->tl_max || currtime < mt->tl_min) {
      double page = mt->tl_max - mt->tl_min;
      mt->tl_min = currtime - page * .25;
      mt->tl_max = currtime + page * .75;
      mt_zoom(mt, -1.);
    }
  }

  if (needs_clear) {
    set_ce_frame_from_pixbuf(LIVES_IMAGE(mainw->play_image), NULL, NULL);
    needs_clear = FALSE;
  }

  if (mt->is_rendering) {
    // preview during rendering
    boolean had_audio = mt->has_audio_file;
    mt->pb_start_event = NULL;
    mt->has_audio_file = TRUE;
    on_preview_clicked(LIVES_BUTTON(cfile->proc_ptr->preview_button), NULL);
    mt->has_audio_file = had_audio;
  } else {
    if (mt->event_list != NULL) {
      mainw->is_rendering = TRUE; // NOTE : mainw->is_rendering is not the same as mt->is_rendering !
      set_play_position(mt);
      if (mainw->cancelled != CANCEL_VID_END) {
        // otherwise jack transport set us out of range

        if (mt->playing_sel)
          mt->pb_loop_event = get_frame_event_at(mt->event_list,
                                                 q_gint64(mt->region_start * TICKS_PER_SECOND_DBL, mt->fps), NULL, TRUE);
        else if (mt->is_paused) mt->pb_loop_event = pb_loop_event;

        on_preview_clicked(NULL, LIVES_INT_TO_POINTER(1));
      }

      mainw->is_rendering = mainw->is_processing = FALSE;
    }
  }

  mt_swap_play_pause(mt, FALSE);

  lives_container_remove(LIVES_CONTAINER(mt->context_frame), mt->context_scroll);

  mt->context_scroll = old_context_scroll;
  lives_container_add(LIVES_CONTAINER(mt->context_frame), mt->context_scroll);

  if (mt->opts.show_ctx) lives_widget_show_all(mt->context_frame);

  lives_widget_object_unref(mt->context_scroll);

  if (!mt->is_rendering) {
    mt_sensitise(mt);
    if (needs_idlefunc) mt->idlefunc = mt_idle_add(mt);
  }

  if (!pb_audio_needs_prerender) lives_widget_set_sensitive(mt->prerender_aud, FALSE);
  mt->no_expose_frame = FALSE;
}


void multitrack_play_sel(LiVESMenuItem * menuitem, livespointer user_data) {
  // get current pointer time; if it is outside the time region jump to start
  double ptr_time;
  lives_mt *mt = (lives_mt *)user_data;

  ptr_time = mt->ptr_time;

  if (ptr_time < mt->region_start || ptr_time >= mt->region_end) {
    mt->ptr_time = lives_ruler_set_value(LIVES_RULER(mt->timeline), mt->region_start);
  }

  // set loop start point to region start, and pb_start to current position
  // set loop end point to region end or tl end, whichever is soonest
  mt->playing_sel = TRUE;

  multitrack_playall(mt);

  // on return here, return the pointer to its original position, unless paused
  if (!mt->is_paused) {
    mt->playing_sel = FALSE;
  }
}


void multitrack_adj_start_end(LiVESMenuItem * menuitem, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  unselect_all(mt);
  polymorph(mt, POLY_IN_OUT);
}


boolean multitrack_insert(LiVESMenuItem * menuitem, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  lives_clip_t *sfile = mainw->files[mt->file_selected];

  double secs = mt->ptr_time;

  LiVESWidget *eventbox;

  weed_timecode_t ins_start = (sfile->start - 1.) / sfile->fps * TICKS_PER_SECOND_DBL;
  weed_timecode_t ins_end = (double)(sfile->end) / sfile->fps * TICKS_PER_SECOND_DBL;

  boolean did_backup = mt->did_backup;
  boolean needs_idlefunc = FALSE;

  track_rect *block;

  if (mt->current_track < 0) return multitrack_audio_insert(menuitem, user_data);

  if (sfile->frames == 0) return FALSE;

  if (mt->idlefunc > 0) {
    needs_idlefunc = TRUE;
    lives_source_remove(mt->idlefunc);
    mt->idlefunc = 0;
  }

  if (mt->context_time != -1. && mt->use_context) {
    secs = mt->context_time;
    mt->context_time = -1.;
    mt->use_context = FALSE;
  }

  eventbox = (LiVESWidget *)lives_list_nth_data(mt->video_draws, mt->current_track);

  if (mt->opts.ign_ins_sel) {
    // ignore selection limits
    ins_start = 0;
    ins_end = (double)(sfile->frames) / sfile->fps * TICKS_PER_SECOND_DBL;
  }

  if (mt->insert_start != -1) {
    // used if we move a block
    ins_start = mt->insert_start;
    ins_end = mt->insert_end;
  }

  if (mt->opts.insert_mode == INSERT_MODE_NORMAL) {
    // first check if there is space to insert the block to, otherwise we will abort the insert
    weed_plant_t *event = NULL;
    weed_timecode_t tc = 0, tcnow;
    weed_timecode_t tclen = ins_end - ins_start;

    while (tc <= tclen) {
      tcnow = q_gint64(tc + secs * TICKS_PER_SECOND_DBL, mt->fps);
      tc += TICKS_PER_SECOND_DBL / mt->fps;
      event = get_frame_event_at(mt->event_list, tcnow, event, TRUE);
      if (event == NULL) break; // must be end of timeline
      // is video track, if we have a non-blank frame, abort
      if (get_frame_event_clip(event, mt->current_track) >= 0) {
        if (!did_backup) {
          if (needs_idlefunc || (!did_backup && mt->auto_changed)) mt->idlefunc = mt_idle_add(mt);
        }
        return FALSE;
      }
    }
  }

  mt_backup(mt, MT_UNDO_INSERT_BLOCK, 0);

  insert_frames(mt->file_selected, ins_start, ins_end, secs * TICKS_PER_SECOND, LIVES_DIRECTION_FORWARD, eventbox, mt, NULL);

  block = (track_rect *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "block_last");

  if (block != NULL && (mt->opts.grav_mode == GRAV_MODE_LEFT || (block->next != NULL
                        && mt->opts.grav_mode == GRAV_MODE_RIGHT)) &&
      !(did_backup || mt->moving_block)) {
    double oldr_start = mt->region_start;
    double oldr_end = mt->region_end;
    LiVESList *tracks_sel;
    track_rect *selblock = NULL;
    if (mt->block_selected != block) selblock = mt->block_selected;
    tracks_sel = lives_list_copy(mt->selected_tracks);
    if (mt->selected_tracks != NULL) lives_list_free(mt->selected_tracks);
    mt->selected_tracks = NULL;
    mt->selected_tracks = lives_list_append(mt->selected_tracks, LIVES_INT_TO_POINTER(mt->current_track));

    if (mt->opts.grav_mode == GRAV_MODE_LEFT) {
      if (block->prev != NULL) mt->region_start = get_event_timecode(block->prev->end_event) / TICKS_PER_SECOND_DBL;
      else mt->region_start = 0.;
      mt->region_end = get_event_timecode(block->start_event) / TICKS_PER_SECOND_DBL;
    } else {
      mt->region_start = get_event_timecode(block->end_event) / TICKS_PER_SECOND_DBL;
      mt->region_end = get_event_timecode(block->next->start_event) / TICKS_PER_SECOND_DBL;
    }

    remove_first_gaps(NULL, mt);
    lives_list_free(mt->selected_tracks);
    mt->selected_tracks = lives_list_copy(tracks_sel);
    if (tracks_sel != NULL) lives_list_free(tracks_sel);
    mt->region_start = oldr_start;
    mt->region_end = oldr_end;
    mt_sensitise(mt);
    if (selblock != NULL) mt->block_selected = selblock;
  }

  // get this again because it could have moved
  block = (track_rect *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "block_last");

  if (!did_backup) {
    if (mt->avol_fx != -1 && block != NULL && block->next == NULL && get_first_event(mt->event_list) != NULL) {
      apply_avol_filter(mt);
    }
  }

  if (!mt->moving_block && prefs->atrans_fx != -1) {
    // add the insert and autotrans as 2 separate undo events
    mt->did_backup = did_backup;
    mt_do_autotransition(mt, block);
    mt->did_backup = TRUE;
  }

  if (block != NULL && !resize_timeline(mt) && !did_backup) {
    lives_painter_surface_t *bgimage = (lives_painter_surface_t *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox),
                                       "bgimg");
    if (bgimage != NULL) {
      draw_block(mt, NULL, bgimage, block, 0, lives_widget_get_allocation_width(eventbox));
      lives_widget_queue_draw(eventbox);
    }
  }

  if (!did_backup && mt->framedraw != NULL && mt->current_rfx != NULL && mt->init_event != NULL &&
      mt->poly_state == POLY_PARAMS &&
      weed_plant_has_leaf(mt->init_event, WEED_LEAF_IN_TRACKS)) {
    weed_timecode_t tc = q_gint64(lives_spin_button_get_value(LIVES_SPIN_BUTTON(mt->node_spinbutton)) *
                                  TICKS_PER_SECOND_DBL + get_event_timecode(mt->init_event), mt->fps);
    get_track_index(mt, tc);
  }

  // expand the play preview as necessary
  set_mt_play_sizes(mt, cfile->hsize, cfile->vsize, TRUE);

  mt_tl_move_relative(mt, 0.);

  if (!did_backup) {
    mt->did_backup = FALSE;
    if (needs_idlefunc || (!did_backup && mt->auto_changed)) mt->idlefunc = mt_idle_add(mt);
    if (prefs->mt_auto_back == 0) mt_auto_backup(mt);
  }
  return TRUE;
}


boolean multitrack_audio_insert(LiVESMenuItem * menuitem, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  lives_clip_t *sfile = mainw->files[mt->file_selected];

  double secs = mt->ptr_time;
  double tstart, tend;

  LiVESWidget *eventbox = (LiVESWidget *)mt->audio_draws->data;

  weed_timecode_t ins_start = q_gint64((sfile->start - 1.) / sfile->fps * TICKS_PER_SECOND_DBL, mt->fps);
  weed_timecode_t ins_end = q_gint64((double)sfile->end / sfile->fps * TICKS_PER_SECOND_DBL, mt->fps);

  boolean did_backup = mt->did_backup;
  boolean needs_idlefunc = FALSE;

  track_rect *block;

  char *tmp;
  char *istart, *iend;

  lives_direction_t dir;

  if (mt->current_track != -1 || sfile->achans == 0) return FALSE;

  if (mt->idlefunc > 0) {
    needs_idlefunc = TRUE;
    lives_source_remove(mt->idlefunc);
    mt->idlefunc = 0;
  }

  if (mt->context_time != -1. && mt->use_context) {
    secs = mt->context_time;
    mt->context_time = -1.;
    mt->use_context = FALSE;
  }

  if (sfile->frames == 0 || mt->opts.ign_ins_sel) {
    ins_start = 0;
    ins_end = q_gint64(sfile->laudio_time * TICKS_PER_SECOND_DBL, mt->fps);
  }

  if (ins_start > q_gint64(sfile->laudio_time * TICKS_PER_SECOND_DBL, mt->fps)) {
    if (!did_backup) {
      if (needs_idlefunc || (!did_backup && mt->auto_changed)) mt->idlefunc = mt_idle_add(mt);
    }
    return FALSE;
  }

  if (ins_end > q_gint64(sfile->laudio_time * TICKS_PER_SECOND_DBL, mt->fps)) {
    ins_end = q_gint64(sfile->laudio_time * TICKS_PER_SECOND_DBL, mt->fps);
  }

  if (mt->insert_start != -1) {
    ins_start = mt->insert_start;
    ins_end = mt->insert_end;
  }

  if (mt->insert_avel > 0.) dir = LIVES_DIRECTION_FORWARD;
  else dir = LIVES_DIRECTION_BACKWARD;

  if (mt->opts.insert_mode == INSERT_MODE_NORMAL) {
    // first check if there is space to insert the block to, otherwise we will abort the insert
    weed_plant_t *event = NULL;
    weed_timecode_t tc = 0, tcnow;
    weed_timecode_t tclen = ins_end - ins_start;

    //if (dir==LIVES_DIRECTION_BACKWARD) tc+=TICKS_PER_SECOND_DBL/mt->fps; // TODO - check if we need this

    while (tc <= tclen) {
      tcnow = q_gint64(tc + secs * TICKS_PER_SECOND_DBL, mt->fps);
      tc += TICKS_PER_SECOND_DBL / mt->fps;
      event = get_frame_event_at(mt->event_list, tcnow, event, TRUE);
      if (event == NULL) break; // must be end of timeline
      // is audio track, see if we are in an audio block
      if ((tc == 0 && get_audio_block_start(mt->event_list, mt->current_track, tcnow, TRUE) != NULL) ||
          (get_audio_block_start(mt->event_list, mt->current_track, tcnow, FALSE) != NULL)) {
        if (!did_backup) {
          if (needs_idlefunc || (!did_backup && mt->auto_changed)) mt->idlefunc = mt_idle_add(mt);
        }
        return FALSE;
      }
    }
  }

  mt_backup(mt, MT_UNDO_INSERT_AUDIO_BLOCK, 0);

  insert_audio(mt->file_selected, ins_start, ins_end, secs * TICKS_PER_SECOND, mt->insert_avel, dir, eventbox, mt, NULL);

  block = (track_rect *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "block_last");

  if (block != NULL && (mt->opts.grav_mode == GRAV_MODE_LEFT ||
                        (mt->opts.grav_mode == GRAV_MODE_RIGHT && block->next != NULL))
      && !(did_backup || mt->moving_block)) {
    double oldr_start = mt->region_start;
    double oldr_end = mt->region_end;
    LiVESList *tracks_sel;
    track_rect *selblock = NULL;
    if (mt->block_selected != block) selblock = mt->block_selected;
    tracks_sel = lives_list_copy(mt->selected_tracks);
    if (mt->selected_tracks != NULL) lives_list_free(mt->selected_tracks);
    mt->selected_tracks = NULL;
    mt->current_track = -1;

    if (mt->opts.grav_mode == GRAV_MODE_LEFT) {
      if (block->prev != NULL) mt->region_start = get_event_timecode(block->prev->end_event) / TICKS_PER_SECOND_DBL;
      else mt->region_start = 0.;
      mt->region_end = get_event_timecode(block->start_event) / TICKS_PER_SECOND_DBL;
    } else {
      mt->region_start = get_event_timecode(block->end_event) / TICKS_PER_SECOND_DBL;
      mt->region_end = get_event_timecode(block->next->start_event) / TICKS_PER_SECOND_DBL;
    }

    remove_first_gaps(NULL, mt);
    lives_list_free(mt->selected_tracks);
    mt->selected_tracks = lives_list_copy(tracks_sel);
    if (tracks_sel != NULL) lives_list_free(tracks_sel);
    mt->region_start = oldr_start;
    mt->region_end = oldr_end;
    if (selblock != NULL) mt->block_selected = selblock;
  }

  mt->did_backup = did_backup;

  tstart = QUANT_TICKS(ins_start);
  tend = QUANT_TICKS(ins_end);
  istart = time_to_string(QUANT_TIME(secs));
  iend = time_to_string(QUANT_TIME(secs + (ins_end - ins_start) / TICKS_PER_SECOND_DBL));

  d_print(_("Inserted audio %.4f to %.4f from clip %s into backing audio from time %s to time %s\n"),
          tstart, tend, (tmp = get_menu_name(sfile, FALSE)), istart, iend);
  lives_free(tmp);

  lives_free(istart);
  lives_free(iend);

  if (!resize_timeline(mt) && !did_backup) {
    lives_painter_surface_t *bgimage = (lives_painter_surface_t *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox),
                                       "bgimg");
    if (bgimage != NULL) {
      draw_block(mt, NULL, bgimage, block, 0, lives_widget_get_allocation_width(eventbox));
      lives_widget_queue_draw(eventbox);
    }

    if (LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "expanded"))) {
      LiVESWidget *xeventbox = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "achan0");
      if (xeventbox != NULL) lives_widget_queue_draw(xeventbox);
      xeventbox = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "achan1");
      if (xeventbox != NULL) lives_widget_queue_draw(xeventbox);
    }
  }

  // get this again because it could have moved
  block = (track_rect *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "block_last");

  if (!did_backup) {
    if (mt->avol_fx != -1 && block != NULL && block->next == NULL && get_first_event(mt->event_list) != NULL) {
      apply_avol_filter(mt);
    }
  }

  mt_tl_move_relative(mt, 0.);

  if (!did_backup && mt->framedraw != NULL && mt->current_rfx != NULL && mt->init_event != NULL &&
      mt->poly_state == POLY_PARAMS && weed_plant_has_leaf(mt->init_event, WEED_LEAF_IN_TRACKS)) {
    weed_timecode_t tc = q_gint64(lives_spin_button_get_value(LIVES_SPIN_BUTTON(mt->node_spinbutton)) *
                                  TICKS_PER_SECOND_DBL + get_event_timecode(mt->init_event), mt->fps);
    get_track_index(mt, tc);
  }

  if (!did_backup) {
    mt->did_backup = FALSE;
    if (needs_idlefunc || (!did_backup && mt->auto_changed)) mt->idlefunc = mt_idle_add(mt);
    if (prefs->mt_auto_back == 0) mt_auto_backup(mt);
  }
  return TRUE;
}


void insert_frames(int filenum, weed_timecode_t offset_start, weed_timecode_t offset_end, weed_timecode_t tc,
                   lives_direction_t direction, LiVESWidget * eventbox, lives_mt * mt, track_rect * in_block) {
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

  lives_clip_t *sfile = mainw->files[filenum];

  weed_timecode_t last_tc = 0, offset_start_tc, start_tc, last_offset;
  weed_timecode_t orig_st = offset_start, orig_end = offset_end;

  int *clips = NULL, *rep_clips;
  int64_t *frames = NULL, *rep_frames;
  weed_plant_t *last_frame_event = NULL;
  weed_plant_t *event, *shortcut1 = NULL, *shortcut2 = NULL;
  weed_error_t error;
  track_rect *new_block = NULL;

  LiVESWidget *aeventbox = NULL;

  double aseek;
  double end_secs;

  boolean isfirst = TRUE;

  int64_t frame = (int64_t)((double)(offset_start / TICKS_PER_SECOND_DBL) * mt->fps + 1.4999);
  int track = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "layer_number"));
  int numframes, i;
  int render_file = mainw->current_file;

  mt_desensitise(mt);

  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(eventbox), "block_last", (livespointer)NULL);
  if ((aeventbox = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "atrack")) != NULL)
    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(aeventbox), "block_last", (livespointer)NULL);

  last_offset = offset_start_tc = q_gint64(offset_start, mt->fps);
  offset_end = q_gint64(offset_end, mt->fps);
  start_tc = q_gint64(tc, mt->fps);
  if (direction == LIVES_DIRECTION_BACKWARD) tc -= TICKS_PER_SECOND_DBL / mt->fps;
  last_tc = q_gint64(tc, mt->fps);

  if (direction == LIVES_DIRECTION_FORWARD) {
    // fill in blank frames in a gap
    if (mt->event_list != NULL) last_frame_event = get_last_frame_event(mt->event_list);
    mt->event_list = add_blank_frames_up_to(mt->event_list, last_frame_event, q_gint64(start_tc - 1. / mt->fps, mt->fps), mt->fps);
  }

  mainw->current_file = filenum;

  if (cfile->fps != mt->fps && cfile->event_list == NULL) {
    // resample clip to render fps
    cfile->undo1_dbl = mt->fps;
    on_resample_vid_ok(NULL, NULL);
  }

  mainw->current_file = render_file;

  while (((direction == LIVES_DIRECTION_FORWARD &&
           (offset_start = q_gint64(last_tc - start_tc + offset_start_tc, mt->fps)) < offset_end)
          || (direction == LIVES_DIRECTION_BACKWARD &&
              (offset_start = q_gint64(last_tc + offset_start_tc - start_tc, mt->fps)) >= offset_end))) {
    numframes = 0;
    clips = rep_clips = NULL;
    frames = rep_frames = NULL;

    if ((event = get_frame_event_at(mt->event_list, last_tc, shortcut1, TRUE)) != NULL) {
      // TODO - memcheck
      clips = weed_get_int_array_counted(event, WEED_LEAF_CLIPS, &numframes);
      frames = weed_get_int64_array(event, WEED_LEAF_FRAMES, &error);
      shortcut1 = event;
    } else if (direction == LIVES_DIRECTION_FORWARD && mt->event_list != NULL) {
      shortcut1 = get_last_event(mt->event_list);
    }

    if (numframes <= track) {
      // TODO - memcheck
      rep_clips = (int *)lives_malloc(track * sizint + sizint);
      rep_frames = (int64_t *)lives_malloc(track * 8 + 8);

      for (i = 0; i < track; i++) {
        if (i < numframes) {
          rep_clips[i] = clips[i];
          rep_frames[i] = frames[i];
        } else {
          rep_clips[i] = -1;
          rep_frames[i] = 0;
        }
      }
      numframes = track + 1;
    } else {
      if (mt->opts.insert_mode == INSERT_MODE_NORMAL && frames[track] > 0) {
        if (in_block == NULL && new_block != NULL) {
          if (direction == LIVES_DIRECTION_FORWARD) {
            shortcut1 = get_prev_frame_event(shortcut1);
          }
        }
        lives_freep((void **)&clips);
        lives_freep((void **)&frames);
        break; // do not allow overwriting in this mode
      }
      rep_clips = clips;
      rep_frames = frames;
    }

    if (sfile->event_list != NULL) event = get_frame_event_at(sfile->event_list, offset_start, shortcut2, TRUE);
    if (sfile->event_list != NULL && event == NULL) {
      if (rep_clips != clips && rep_clips != NULL) lives_free(rep_clips);
      if (rep_frames != frames && rep_frames != NULL) lives_free(rep_frames);
      lives_freep((void **)&clips);
      lives_freep((void **)&frames);
      break; // insert finished: ran out of frames in resampled clip
    }
    last_offset = offset_start;
    if (sfile->event_list != NULL) {
      // frames were resampled, get new frame at the source file timecode
      frame = weed_get_int64_value(event, WEED_LEAF_FRAMES, &error);
      if (direction == LIVES_DIRECTION_FORWARD) shortcut2 = event;
      else shortcut2 = get_prev_frame_event(event); // TODO : this is not optimal for the first frame
    }
    rep_clips[track] = filenum;
    rep_frames[track] = frame;

    // TODO - memcheck
    mt->event_list = insert_frame_event_at(mt->event_list, last_tc, numframes, rep_clips, rep_frames, &shortcut1);

    if (rep_clips != clips && rep_clips != NULL) lives_free(rep_clips);
    if (rep_frames != frames && rep_frames != NULL) lives_free(rep_frames);

    if (isfirst) {
      // TODO - memcheck
      if (in_block == NULL) {
        new_block = add_block_start_point(eventbox, last_tc, filenum, offset_start, shortcut1, TRUE);
        if (aeventbox != NULL) {
          if (cfile->achans > 0 && sfile->achans > 0 && mt->opts.insert_audio) {
            // insert audio start or end
            if (direction == LIVES_DIRECTION_FORWARD) {
              aseek = ((double)frame - 1.) / sfile->fps;

              insert_audio_event_at(shortcut1, track, filenum, aseek, 1.);
              add_block_start_point(aeventbox, last_tc, filenum, offset_start, shortcut1, TRUE);
            } else {
              weed_plant_t *nframe;
              if ((nframe = get_next_frame_event(shortcut1)) == NULL) {
                mt->event_list = insert_blank_frame_event_at(mt->event_list,
                                 q_gint64(last_tc + TICKS_PER_SECOND_DBL / mt->fps, mt->fps), &shortcut1);
                nframe = shortcut1;
              }
              insert_audio_event_at(nframe, track, filenum, 0., 0.);
            }
          }
        }
        isfirst = FALSE;
      }
    }

    lives_freep((void **)&clips);
    lives_freep((void **)&frames);

    if (direction == LIVES_DIRECTION_FORWARD) {
      last_tc += TICKS_PER_SECOND_DBL / mt->fps;
      last_tc = q_gint64(last_tc, mt->fps);
    } else {
      if (last_tc < TICKS_PER_SECOND_DBL / mt->fps) break;
      last_tc -= TICKS_PER_SECOND_DBL / mt->fps;
      last_tc = q_gint64(last_tc, mt->fps);
    }
    if (sfile->event_list == NULL) if ((direction == LIVES_DIRECTION_FORWARD && (++frame > (int64_t)sfile->frames)) ||
                                         (direction == LIVES_DIRECTION_BACKWARD && (--frame < 1))) {
        break;
      }
  }

  if (!isfirst || direction == LIVES_DIRECTION_BACKWARD) {
    if (direction == LIVES_DIRECTION_FORWARD) {
      if (in_block != NULL) lives_widget_object_set_data(LIVES_WIDGET_OBJECT(eventbox), "block_last", (livespointer)in_block);
      add_block_end_point(eventbox, shortcut1);

      if (cfile->achans > 0 && sfile->achans > 0 && mt->opts.insert_audio && mt->opts.pertrack_audio) {
        weed_plant_t *shortcut2 = get_next_frame_event(shortcut1);
        if (shortcut2 == NULL) {
          mt->event_list = insert_blank_frame_event_at(mt->event_list, last_tc, &shortcut1);
        } else shortcut1 = shortcut2;
        insert_audio_event_at(shortcut1, track, filenum, 0., 0.);
        add_block_end_point(aeventbox, shortcut1);
      }
    } else if (in_block != NULL) {
      in_block->offset_start = last_offset;
      in_block->start_event = shortcut1;
      if (cfile->achans > 0 && sfile->achans > 0 && mt->opts.insert_audio && mt->opts.pertrack_audio) {
        weed_plant_t *shortcut2 = get_next_frame_event(shortcut1);
        if (shortcut2 == NULL) {
          mt->event_list = insert_blank_frame_event_at(mt->event_list, last_tc, &shortcut1);
        } else shortcut1 = shortcut2;
      }
    }
  }

  mt->last_direction = direction;

  if (mt->event_list != NULL) {
    weed_set_double_value(mt->event_list, WEED_LEAF_FPS, mainw->files[render_file]->fps);
  }

  if (in_block == NULL) {
    char *tmp, *tmp1, *istart, *iend;

    istart = time_to_string(QUANT_TICKS((orig_st + start_tc)));
    iend = time_to_string(QUANT_TICKS((orig_end + start_tc)));

    d_print(_("Inserted frames %d to %d from clip %s into track %s from time %s to time %s\n"),
            sfile->start, sfile->end, (tmp1 = get_menu_name(sfile, FALSE)),
            (tmp = get_track_name(mt, mt->current_track, FALSE)), istart, iend);
    lives_free(tmp);
    lives_free(tmp1);
    lives_free(istart);
    lives_free(iend);
  }

  end_secs = event_list_get_end_secs(mt->event_list);
  if (end_secs > mt->end_secs) {
    set_timeline_end_secs(mt, end_secs);
  }
  mt_sensitise(mt);
}


void insert_audio(int filenum, weed_timecode_t offset_start, weed_timecode_t offset_end, weed_timecode_t tc,
                  double avel, lives_direction_t direction, LiVESWidget * eventbox,
                  lives_mt * mt, track_rect * in_block) {
  // insert the selected audio from mainw->files[filenum] from source file filenum into mt->event_list starting at timeline timecode tc
  // if in_block is non-NULL, then we extend (existing) in_block with the new frames; otherwise we create a new block and insert it into eventbox
  weed_timecode_t start_tc = q_gint64(tc, mt->fps);
  weed_timecode_t end_tc = q_gint64(start_tc + offset_end - offset_start, mt->fps);
  weed_plant_t *last_frame_event;
  track_rect *block;
  weed_plant_t *shortcut = NULL;
  weed_plant_t *frame_event;

  double end_secs;

  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(eventbox), "block_last", (livespointer)NULL);

  if (direction == LIVES_DIRECTION_BACKWARD) {
    weed_timecode_t tmp_tc = offset_end;
    offset_end = offset_start;
    offset_start = tmp_tc;
  }

  // if already block at tc, return
  if ((block = get_block_from_time((LiVESWidget *)mt->audio_draws->data, start_tc / TICKS_PER_SECOND_DBL, mt)) != NULL &&
      get_event_timecode(block->end_event) > start_tc) return;

  // insert blank frames up to end_tc
  last_frame_event = get_last_frame_event(mt->event_list);
  mt->event_list = add_blank_frames_up_to(mt->event_list, last_frame_event, end_tc, mt->fps);

  block = get_block_before((LiVESWidget *)mt->audio_draws->data, start_tc / TICKS_PER_SECOND_DBL, TRUE);
  if (block != NULL) shortcut = block->end_event;

  block = get_block_after((LiVESWidget *)mt->audio_draws->data, start_tc / TICKS_PER_SECOND_DBL, FALSE);

  // insert audio seek at tc
  frame_event = get_frame_event_at(mt->event_list, start_tc, shortcut, TRUE);

  if (direction == LIVES_DIRECTION_FORWARD) {
    insert_audio_event_at(frame_event, -1, filenum, offset_start / TICKS_PER_SECOND_DBL, avel);
  } else {
    insert_audio_event_at(frame_event, -1, filenum, offset_end / TICKS_PER_SECOND_DBL, avel);
    offset_start = offset_start - offset_end + offset_end * mt->insert_avel;
  }

  add_block_start_point((LiVESWidget *)mt->audio_draws->data, start_tc, filenum, offset_start, frame_event, TRUE);

  if (block == NULL || get_event_timecode(block->start_event) > end_tc) {
    // if no blocks after end point, insert audio off at end point
    frame_event = get_frame_event_at(mt->event_list, end_tc, frame_event, TRUE);
    insert_audio_event_at(frame_event, -1, filenum, 0., 0.);
    add_block_end_point((LiVESWidget *)mt->audio_draws->data, frame_event);
  } else add_block_end_point((LiVESWidget *)mt->audio_draws->data, block->start_event);

  end_secs = event_list_get_end_secs(mt->event_list);
  if (end_secs > mt->end_secs) {
    set_timeline_end_secs(mt, end_secs);
  }

  mt_sensitise(mt);
}


void multitrack_view_events(LiVESMenuItem * menuitem, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  LiVESWidget *elist_dialog;
  if ((prefs->event_window_show_frame_events && count_events(mt->event_list, TRUE, 0, 0) > 1000) ||
      (!prefs->event_window_show_frame_events && ((count_events(mt->event_list, TRUE, 0, 0)
          - count_events(mt->event_list, FALSE, 0, 0)) > 1000)))
    if (!do_event_list_warning()) return;
  mt_desensitise(mt);
  lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET, TRUE);
  elist_dialog = create_event_list_dialog(mt->event_list, 0, 0);
  lives_dialog_run(LIVES_DIALOG(elist_dialog));
  mt_sensitise(mt);
}


void multitrack_view_sel_events(LiVESMenuItem * menuitem, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  LiVESWidget *elist_dialog;

  weed_timecode_t tc_start = q_gint64(mt->region_start * TICKS_PER_SECOND, mt->fps);
  weed_timecode_t tc_end = q_gint64(mt->region_end * TICKS_PER_SECOND, mt->fps);

  if ((prefs->event_window_show_frame_events && count_events(mt->event_list, TRUE, tc_start, tc_end) > 1000) ||
      (!prefs->event_window_show_frame_events && ((count_events(mt->event_list, TRUE, tc_start, tc_end)
          - count_events(mt->event_list, FALSE, tc_start, tc_end)) > 1000)))
    if (!do_event_list_warning()) return;
  mt_desensitise(mt);
  lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET, TRUE);
  elist_dialog = create_event_list_dialog(mt->event_list, tc_start, tc_end);
  mt_sensitise(mt);
  lives_dialog_run(LIVES_DIALOG(elist_dialog));
}


////////////////////////////////////////////////////////
// region functions

void draw_region(lives_mt * mt) {
  lives_painter_t *cr;

  double start, end;
  if (mt->region_start == mt->region_end) {
    cr = lives_painter_create_from_widget(mt->timeline_reg);

    if (palette->style & STYLE_3) {
      lives_painter_set_source_rgb_from_lives_widget_color(cr, &palette->menu_and_bars);
    } else {
      lives_painter_set_source_rgb_from_lives_widget_color(cr, &palette->white);
    }

    lives_painter_rectangle(cr, 0, 0,
                            lives_widget_get_allocation_width(mt->timeline_reg),
                            lives_widget_get_allocation_height(mt->timeline_reg));
    lives_painter_fill(cr);
    lives_painter_destroy(cr);
  }

  if (mt->region_start < mt->region_end) {
    start = mt->region_start;
    end = mt->region_end;
  } else {
    start = mt->region_end;
    end = mt->region_start;
  }

  if (mt->region_start == mt->region_end) {
    lives_widget_set_sensitive(mt->rs_to_tc, FALSE);
    lives_widget_set_sensitive(mt->re_to_tc, FALSE);
  } else {
    lives_widget_set_sensitive(mt->rs_to_tc, TRUE);
    lives_widget_set_sensitive(mt->re_to_tc, TRUE);
  }

  cr = lives_painter_create_from_widget(mt->timeline_reg);

  if (palette->style & STYLE_3) {
    lives_painter_set_source_rgb_from_lives_widget_color(cr, &palette->menu_and_bars);
  } else {
    lives_painter_set_source_rgb_from_lives_widget_color(cr, &palette->white);
  }

  lives_painter_rectangle(cr, 0, 0,
                          lives_widget_get_allocation_width(mt->timeline_reg),
                          lives_widget_get_allocation_height(mt->timeline_reg));
  lives_painter_fill(cr);

  lives_painter_set_source_rgb_from_lives_rgba(cr, &palette->mt_timeline_reg);
  lives_painter_rectangle(cr, (start - mt->tl_min)*lives_widget_get_allocation_width(mt->timeline) / (mt->tl_max - mt->tl_min),
                          0,
                          (end - start)*lives_widget_get_allocation_width(mt->timeline) / (mt->tl_max - mt->tl_min),
                          lives_widget_get_allocation_height(mt->timeline_reg) - 2);
  lives_painter_fill(cr);
  lives_painter_destroy(cr);
}


static EXPOSE_FN_DECL(expose_timeline_reg_event, timeline, user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  LiVESList *tl_marks = mt->tl_marks;

  double time;

  int ebwidth;
  int offset;

  if (mt->no_expose) return TRUE;
  if (event != NULL && event->count > 0) return FALSE;
  if (LIVES_IS_PLAYING || mt->is_rendering) return FALSE;
  draw_region(mt);

  if (event != NULL) cairo = lives_painter_create_from_widget(timeline);

  lives_painter_set_source_rgb_from_lives_rgba(cairo, &palette->mt_mark);

  while (tl_marks != NULL) {
    time = strtod((char *)tl_marks->data, NULL);
    ebwidth = lives_widget_get_allocation_width(mt->timeline);
    offset = (time - mt->tl_min) / (mt->tl_max - mt->tl_min) * (double)ebwidth;

    lives_painter_move_to(cairo, offset, 1);
    lives_painter_line_to(cairo, offset, lives_widget_get_allocation_height(mt->timeline_reg) - 2);
    lives_painter_stroke(cairo);

    tl_marks = tl_marks->next;
  }

  if (event != NULL) lives_painter_destroy(cairo);

  paint_lines(mt, mt->ptr_time, FALSE);

  return TRUE;
}
EXPOSE_FN_END


static void draw_soundwave(LiVESWidget * ebox, lives_painter_surface_t *surf, int chnum, lives_mt * mt) {
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
  double awid;

  int offset_start, offset_end; // pixel values
  int fnum;
  int width = lives_widget_get_allocation_width(ebox);
  int track = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "layer_number"));

  register int i;

  aofile = -1;
  afd = -1;

  mainw->read_failed = FALSE;

  while (block != NULL) {
    event = block->start_event;
    tc = get_event_timecode(event);

    offset_startd = tc / TICKS_PER_SECOND_DBL;
    if (offset_startd > mt->tl_max) {
      if (afd != -1) lives_close_buffered(afd);
      return;
    }

    offset_start = (int)((offset_startd - mt->tl_min) / tl_span * lives_widget_get_allocation_width(ebox) + .5);
    if (width > 0 && offset_start > width) {
      if (afd != -1) lives_close_buffered(afd);
      return;
    }

    offset_endd = get_event_timecode(block->end_event) / TICKS_PER_SECOND_DBL; //+1./cfile->fps;
    offset_end = (offset_endd - mt->tl_min) / tl_span * lives_widget_get_allocation_width(ebox);

    if (offset_end < mt->tl_min) {
      block = block->next;
      continue;
    }

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

    awid = lives_widget_get_allocation_width(ebox) / (tl_span + mt->tl_min - offset_startd * vel);

    if (fnum != aofile) {
      // does not make sense to use buffer reads, as we may read very sparsely from the file
      if (afd != -1) close(afd);
      filename = lives_get_audio_file_name(fnum);
      afd = lives_open_buffered_rdonly(filename);
      lives_free(filename);
      aofile = fnum;
    }

    for (i = offset_start; i <= offset_end; i++) {
      secs = (double)i / awid + seek;
      if (secs > (chnum == 0 ? mainw->files[fnum]->laudio_time : mainw->files[fnum]->raudio_time)) break;

      // seek and read
      if (afd == -1) {
        mainw->read_failed = -2;
        return;
      }
      ypos = get_float_audio_val_at_time(fnum, afd, secs, chnum, cfile->achans) * .5;

      lives_painter_move_to(cr, i, (float)lives_widget_get_allocation_height(ebox) / 2.);
      lives_painter_line_to(cr, i, (.5 - ypos) * (float)lives_widget_get_allocation_height(ebox));
      lives_painter_stroke(cr);
    }
    block = block->next;

    if (mainw->read_failed) {
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

  if (event != NULL) {
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

  hidden = (int)LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(ebox), "hidden"));
  if (hidden != 0) {
    return FALSE;
  }

  if (width > lives_widget_get_allocation_width(ebox) - startx) width = lives_widget_get_allocation_width(ebox) - startx;

  if (event != NULL) cairo = lives_painter_create_from_widget(ebox);
  bgimage = (lives_painter_surface_t *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(ebox), "bgimg");

  if (LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(ebox), "drawn"))) {
    if (bgimage != NULL && lives_painter_image_surface_get_width(bgimage) > 0) {
      lives_painter_set_source_surface(cairo, bgimage, startx, starty);
      lives_painter_rectangle(cairo, startx, starty, width, height);
      lives_painter_fill(cairo);
      if (event != NULL) lives_painter_destroy(cairo);
      return TRUE;
    }
  }

  if (event != NULL) {
    lives_painter_destroy(cairo);
    width = lives_widget_get_allocation_width(ebox);
    height = lives_widget_get_allocation_height(ebox);
  }

  if (bgimage != NULL) lives_painter_surface_destroy(bgimage);

  bgimage = lives_painter_image_surface_create(LIVES_PAINTER_FORMAT_ARGB32,
            width,
            height);

  if (palette->style & STYLE_1) {
    lives_painter_t *crx = lives_painter_create_from_surface(bgimage);
    lives_painter_set_source_rgb_from_lives_rgba(crx, &palette->mt_evbox);
    lives_painter_rectangle(crx, 0., 0., width, height);
    lives_painter_fill(crx);
    lives_painter_paint(crx);
    lives_painter_destroy(crx);
  }

  channum = LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(ebox), "channel"));

  if (bgimage != NULL && lives_painter_image_surface_get_width(bgimage) > 0) {
    draw_soundwave(ebox, bgimage, channum, mt);
    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(ebox), "drawn", LIVES_INT_TO_POINTER(TRUE));
    lives_widget_queue_draw(ebox);
  } else if (bgimage != NULL) {
    lives_painter_surface_destroy(bgimage);
    bgimage = NULL;
  }

  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(ebox), "bgimg", bgimage);

  return TRUE;
}
EXPOSE_FN_END


////////////////////////////////////////////////////

// functions for moving and clicking on the timeline

boolean on_timeline_update(LiVESWidget * widget, LiVESXEventMotion * event, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  int x;
  double pos;

  if (LIVES_IS_PLAYING) return TRUE;

  lives_widget_get_pointer((LiVESXDevice *)mainw->mgeom[prefs->gui_monitor > 0 ? prefs->gui_monitor - 1 : 0].mouse_device,
                           widget, &x, NULL);
  pos = get_time_from_x(mt, x);

  if (!mt->region_updating) {
    if (mt->tl_mouse) {
      mt->fm_edit_event = NULL;
      mt_tl_move(mt, pos);
    }
    return TRUE;
  }

  if (!mt->sel_locked) {
    if (pos > mt->region_init) {
      mt->region_start = mt->region_init;
      mt->region_end = pos;
    } else {
      mt->region_start = pos;
      mt->region_end = mt->region_init;
    }
  }

  lives_spin_button_set_value(LIVES_SPIN_BUTTON(mt->spinbutton_start), mt->region_start);
  lives_spin_button_set_value(LIVES_SPIN_BUTTON(mt->spinbutton_end), mt->region_end);

  if (mt->opts.mouse_mode == MOUSE_MODE_SELECT && mt->tl_selecting && event != NULL) mouse_select_move(widget, event, mt);

  return TRUE;
}


boolean all_present(weed_plant_t *event, LiVESList * sel) {
  int *clips;
  int64_t *frames;
  int numclips, layer;

  // see if we have an actual clip/frame for each layer in sel
  if (event == NULL || sel == NULL) return FALSE;

  clips = weed_get_int_array_counted(event, WEED_LEAF_CLIPS, &numclips);

  if (!numclips) return FALSE;

  frames = weed_get_int64_array(event, WEED_LEAF_FRAMES, NULL);

  while (sel != NULL) {
    layer = LIVES_POINTER_TO_INT(sel->data);
    if (layer >= numclips || clips[layer] < 1 || frames[layer] < 1) {
      lives_free(clips);
      lives_free(frames);
      return FALSE;
    }
    sel = sel->next;
  }
  lives_free(clips);
  lives_free(frames);
  return TRUE;
}


void get_region_overlap(lives_mt * mt) {
  // get region which overlaps all selected tracks
  weed_plant_t *event;
  weed_timecode_t tc;

  if (mt->selected_tracks == NULL || mt->event_list == NULL) {
    mt->region_start = mt->region_end = 0.;
    return;
  }

  tc = q_gint64(mt->region_start * TICKS_PER_SECOND, mt->fps);
  event = get_frame_event_at(mt->event_list, tc, NULL, TRUE);

  while (all_present(event, mt->selected_tracks)) {
    // move start to left
    event = get_prev_frame_event(event);
  }

  if (event == NULL) {
    event = get_first_event(mt->event_list);
    if (!WEED_EVENT_IS_FRAME(event)) event = get_next_frame_event(event);
  }

  while (event != NULL && !all_present(event, mt->selected_tracks)) {
    event = get_next_frame_event(event);
  }

  if (event == NULL) mt->region_start = 0.;
  else mt->region_start = get_event_timecode(event) / TICKS_PER_SECOND_DBL;

  tc = q_gint64(mt->region_end * TICKS_PER_SECOND, mt->fps);
  event = get_frame_event_at(mt->event_list, tc, NULL, TRUE);

  while (all_present(event, mt->selected_tracks)) {
    // move end to right
    event = get_next_frame_event(event);
  }

  if (event == NULL) {
    event = get_last_event(mt->event_list);
    if (!WEED_EVENT_IS_FRAME(event)) event = get_prev_frame_event(event);
  }

  while (event != NULL && !all_present(event, mt->selected_tracks)) {
    event = get_prev_frame_event(event);
  }

  if (event == NULL) mt->region_end = 0.;
  mt->region_end = get_event_timecode(event) / TICKS_PER_SECOND_DBL + 1. / mt->fps;

  if (mt->event_list != NULL && get_first_event(mt->event_list) != NULL) {
    lives_widget_set_sensitive(mt->view_sel_events, mt->region_start != mt->region_end);
  }
}


void do_sel_context(lives_mt * mt) {
  char *msg;
  if (mt->region_start == mt->region_end || mt->did_backup) return;
  clear_context(mt);
  msg = lives_strdup_printf(_("Time region %.3f to %.3f\nselected.\n"), mt->region_start, mt->region_end);
  add_context_label(mt, msg);
  lives_free(msg);
  if (mt->selected_tracks == NULL) {
    msg = lives_strdup_printf(_("select one or more tracks\nto create a region.\n"), lives_list_length(mt->selected_tracks));
  } else msg = lives_strdup_printf(_("%d video tracks selected.\n"), lives_list_length(mt->selected_tracks));
  add_context_label(mt, msg);
  add_context_label(mt, _("Double click on timeline\nto deselect time region."));
  lives_free(msg);
}


void do_fx_list_context(lives_mt * mt, int fxcount) {
  clear_context(mt);
  add_context_label(mt, (_("Single click on an effect\nto select it.")));
  add_context_label(mt, (_("Double click on an effect\nto edit it.")));
  add_context_label(mt, (_("Right click on an effect\nfor context menu.\n")));
  add_context_label(mt, (_("\n\nEffects are apllied in order from top to bottom.\n")));
  if (fxcount > 1) {
    add_context_label(mt, (_("Effect order can be changed at\nFILTER MAPS")));
  }
}


void do_fx_move_context(lives_mt * mt) {
  clear_context(mt);
  add_context_label(mt, (_("You can select an effect,\nthen use the INSERT BEFORE")));
  add_context_label(mt, (_("or INSERT AFTER buttons to move it.")));
}


boolean on_timeline_release(LiVESWidget * eventbox, LiVESXEventButton * event, livespointer user_data) {
  //button release
  lives_mt *mt = (lives_mt *)user_data;

  double pos = mt->region_end;

  lives_mt_poly_state_t statep;

  if (!mainw->interactive || mt->sel_locked) return FALSE;

  if (LIVES_IS_PLAYING) return FALSE;

  mt->tl_mouse = FALSE;

  if (eventbox != mt->timeline_reg || mt->sel_locked) {
    return FALSE;
  }

  if (event != NULL) mt->region_updating = FALSE;

  if (mt->region_start == mt->region_end && eventbox == mt->timeline_reg) {
    mt->region_start = mt->region_end = 0;
    lives_widget_set_sensitive(mt->view_sel_events, FALSE);
    lives_signal_handler_block(mt->spinbutton_start, mt->spin_start_func);
    lives_signal_handler_block(mt->spinbutton_end, mt->spin_end_func);
    lives_spin_button_set_range(LIVES_SPIN_BUTTON(mt->spinbutton_start), 0., mt->end_secs);
    lives_spin_button_set_range(LIVES_SPIN_BUTTON(mt->spinbutton_end), 0., mt->end_secs + 1. / mt->fps);
    lives_spin_button_set_value(LIVES_SPIN_BUTTON(mt->spinbutton_start), 0.);
    lives_spin_button_set_value(LIVES_SPIN_BUTTON(mt->spinbutton_end), 0.);
    lives_signal_handler_unblock(mt->spinbutton_start, mt->spin_start_func);
    lives_signal_handler_unblock(mt->spinbutton_end, mt->spin_end_func);
    lives_widget_queue_draw(mt->timeline_reg);
    lives_widget_process_updates(mt->timeline_reg, FALSE);
    draw_region(mt);
    no_time_selected(mt);
  }

  if ((mt->region_end != mt->region_start) && eventbox == mt->timeline_reg) {
    if (mt->opts.snap_over) get_region_overlap(mt);
    if (mt->region_end < mt->region_start) {
      mt->region_start -= mt->region_end;
      mt->region_end += mt->region_start;
      mt->region_start = mt->region_end - mt->region_start;
    }
    if (mt->region_end > mt->region_start && mt->event_list != NULL && get_first_event(mt->event_list) != NULL) {
      if (mt->selected_tracks != NULL) {
        lives_widget_set_sensitive(mt->fx_region, TRUE);
        lives_widget_set_sensitive(mt->ins_gap_sel, TRUE);
        lives_widget_set_sensitive(mt->remove_gaps, TRUE);
        lives_widget_set_sensitive(mt->remove_first_gaps, TRUE);
      } else {
        lives_widget_set_sensitive(mt->fx_region, FALSE);
      }
      lives_widget_set_sensitive(mt->playsel, TRUE);
      lives_widget_set_sensitive(mt->ins_gap_cur, TRUE);
      lives_widget_set_sensitive(mt->view_sel_events, TRUE);
    } else {
      lives_widget_set_sensitive(mt->playsel, FALSE);
      lives_widget_set_sensitive(mt->fx_region, FALSE);
      lives_widget_set_sensitive(mt->ins_gap_cur, FALSE);
      lives_widget_set_sensitive(mt->ins_gap_sel, FALSE);
      lives_widget_set_sensitive(mt->remove_gaps, FALSE);
      lives_widget_set_sensitive(mt->remove_first_gaps, FALSE);
    }
    if (mt->region_start == mt->region_end) lives_widget_queue_draw(mt->timeline);
  } else {
    if (eventbox != mt->timeline_reg) mt_tl_move(mt, pos);
    lives_widget_set_sensitive(mt->fx_region, FALSE);
    lives_widget_set_sensitive(mt->ins_gap_cur, FALSE);
    lives_widget_set_sensitive(mt->ins_gap_sel, FALSE);
    lives_widget_set_sensitive(mt->playsel, FALSE);
    lives_widget_set_sensitive(mt->remove_gaps, FALSE);
    lives_widget_set_sensitive(mt->remove_first_gaps, FALSE);
    if (mt->init_event != NULL && mt->poly_state == POLY_PARAMS)
      lives_spin_button_set_value(LIVES_SPIN_BUTTON(mt->node_spinbutton),
                                  pos - get_event_timecode(mt->init_event) / TICKS_PER_SECOND_DBL);
  }

  lives_spin_button_set_value(LIVES_SPIN_BUTTON(mt->spinbutton_start), mt->region_start);
  lives_spin_button_set_value(LIVES_SPIN_BUTTON(mt->spinbutton_end), mt->region_end);

  pos = mt->ptr_time;
  if (pos > mt->region_end - 1. / mt->fps) lives_widget_set_sensitive(mt->tc_to_rs, FALSE);
  else lives_widget_set_sensitive(mt->tc_to_rs, TRUE);
  if (pos < mt->region_start + 1. / mt->fps) lives_widget_set_sensitive(mt->tc_to_re, FALSE);
  else lives_widget_set_sensitive(mt->tc_to_re, TRUE);

  if (mt->opts.mouse_mode == MOUSE_MODE_SELECT && event != NULL) mouse_select_end(eventbox, event, mt);

  if (mt->selected_tracks != NULL && mt->region_end != mt->region_start) {
    lives_widget_set_sensitive(mt->fx_region, TRUE);
    switch (lives_list_length(mt->selected_tracks)) {
    case 1:
      lives_widget_set_sensitive(mt->fx_region_v, TRUE);
      if (cfile->achans == 0) lives_widget_set_sensitive(mt->fx_region_a, FALSE);
      break;
    case 2:
      if (!mt->opts.pertrack_audio)
        lives_widget_set_sensitive(mt->fx_region_2a, FALSE);
      lives_widget_set_sensitive(mt->fx_region_v, FALSE);
      lives_widget_set_sensitive(mt->fx_region_a, FALSE);
      break;
    default:
      break;
    }
  } else lives_widget_set_sensitive(mt->fx_region, FALSE);

  // update labels
  statep = get_poly_state_from_page(mt);
  if (statep == POLY_TRANS || statep == POLY_COMP) {
    polymorph(mt, POLY_NONE);
    polymorph(mt, statep);
  }

  return TRUE;
}


boolean on_timeline_press(LiVESWidget * widget, LiVESXEventButton * event, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;

  LiVESXModifierType modmask;
  LiVESXDevice *device;

  double pos;

  int x;

  if (!mainw->interactive) return FALSE;

  if (LIVES_IS_PLAYING) return FALSE;

  lives_widget_get_pointer((LiVESXDevice *)mainw->mgeom[widget_opts.monitor].mouse_device,
                           widget, &x, NULL);
  pos = get_time_from_x(mt, x);
  if (widget == mt->timeline_reg && !mt->sel_locked) {
    mt->region_start = mt->region_end = mt->region_init = pos;
    lives_widget_set_sensitive(mt->view_sel_events, FALSE);
    mt->region_updating = TRUE;
  }

  device = (LiVESXDevice *)mainw->mgeom[widget_opts.monitor].mouse_device;
  lives_widget_get_modmask(device, widget, &modmask);

  if (event->button == 1) {
    if (widget == mt->timeline_eb) {
      mt->fm_edit_event = NULL;
      mt_tl_move(mt, pos);
      mt->tl_mouse = TRUE;
    }

    if (widget == mt->timeline) {
      mt->fm_edit_event = NULL;
      mt_tl_move(mt, pos);
    }

    if (mt->opts.mouse_mode == MOUSE_MODE_SELECT) mouse_select_start(widget, event, mt);
  }

  return TRUE;
}


weed_plant_t *get_prev_fm(lives_mt * mt, int current_track, weed_plant_t *event) {
  weed_plant_t *event2, *event3, *eventx;

  if (event == NULL) return NULL;

  eventx = get_filter_map_before(event, current_track, NULL);

  if (eventx == NULL) return NULL;

  if (get_event_timecode(eventx) == get_event_timecode(event)) {
    // start with a map different from current

    while (1) {
      event2 = get_prev_event(eventx);

      if (event2 == NULL) return NULL;

      event3 = get_filter_map_before(event2, current_track, NULL);

      if (!compare_filter_maps(event3, eventx, current_track)) {
        event = event2 = event3;
        break; // continue with event 3
      }
      eventx = event3;
    }
  } else {
    if ((event2 = get_prev_frame_event(event)) == NULL) return NULL;

    event2 = get_filter_map_before(event2, current_track, NULL);

    if (event2 == NULL) return NULL;
  }

  // now find the earliest which is the same
  while (1) {
    event = event2;

    event3 = get_prev_event(event2);

    if (event3 == NULL) break;

    event2 = get_filter_map_before(event3, current_track, NULL);

    if (event2 == NULL) break;

    if (!compare_filter_maps(event2, event, current_track)) break;
  }

  if (filter_map_after_frame(event)) return get_next_frame_event(event);

  return event;
}


weed_plant_t *get_next_fm(lives_mt * mt, int current_track, weed_plant_t *event) {
  weed_plant_t *event2, *event3;

  if (event == NULL) return NULL;

  if ((event2 = get_filter_map_after(event, current_track)) == NULL) return event;

  event3 = get_filter_map_before(event, LIVES_TRACK_ANY, NULL);

  if (event3 == NULL) return NULL;

  // find the first filter_map which differs from the current
  while (1) {
    if (!compare_filter_maps(event2, event3, current_track)) break;

    event = get_next_event(event2);

    if (event == NULL) return NULL;

    event3 = event2;

    if ((event2 = get_filter_map_after(event, current_track)) == NULL) return NULL;
  }

  if (filter_map_after_frame(event2)) return get_next_frame_event(event2);

  return event2;
}


static void add_mark_at(lives_mt * mt, double time) {
  lives_painter_t *cr;

  char *tstring = lives_strdup_printf("%.6f", time);
  int offset;

  lives_widget_set_sensitive(mt->clear_marks, TRUE);
  mt->tl_marks = lives_list_append(mt->tl_marks, tstring);
  offset = (time - mt->tl_min) / (mt->tl_max - mt->tl_min) * (double)lives_widget_get_allocation_width(mt->timeline);

  cr = lives_painter_create_from_widget(mt->timeline_reg);

  lives_painter_set_source_rgb_from_lives_rgba(cr, &palette->mt_mark);

  lives_painter_move_to(cr, offset, 1);
  lives_painter_line_to(cr, offset, lives_widget_get_allocation_height(mt->timeline_reg) - 2);
  lives_painter_stroke(cr);

  lives_painter_destroy(cr);
}


boolean mt_mark_callback(LiVESAccelGroup * group, LiVESWidgetObject * obj, uint32_t keyval, LiVESXModifierType mod,
                         livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  double cur_time;

  if (!LIVES_IS_PLAYING) return TRUE;

  cur_time = mt->ptr_time;

  add_mark_at(mt, cur_time);
  return TRUE;
}


boolean mt_tcoverlay_callback(LiVESAccelGroup * group, LiVESWidgetObject * obj, uint32_t keyval, LiVESXModifierType mod,
                              livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  if (LIVES_IS_PLAYING) {
    mt->opts.overlay_timecode = !mt->opts.overlay_timecode;
  }
  return FALSE;
}


void on_fx_insa_clicked(LiVESWidget * button, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  mt->fx_order = FX_ORD_AFTER;
  lives_widget_set_sensitive(mt->fx_ibefore_button, FALSE);
  lives_widget_set_sensitive(mt->fx_iafter_button, FALSE);

  clear_context(mt);
  add_context_label(mt, (_("Click on another effect,")));
  add_context_label(mt, (_("and the selected one\nwill be inserted")));
  add_context_label(mt, (_("after it.\n")));
}


void on_fx_insb_clicked(LiVESWidget * button, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  mt->fx_order = FX_ORD_BEFORE;
  lives_widget_set_sensitive(mt->fx_ibefore_button, FALSE);
  lives_widget_set_sensitive(mt->fx_iafter_button, FALSE);

  clear_context(mt);
  add_context_label(mt, (_("Click on another effect,")));
  add_context_label(mt, (_("and the selected one\nwill be inserted")));
  add_context_label(mt, (_("before it.\n")));
}


void on_prev_fm_clicked(LiVESWidget * button, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  weed_timecode_t tc;
  double secs = mt->ptr_time;
  tc = q_gint64(secs * TICKS_PER_SECOND_DBL, mt->fps);
  weed_plant_t *event;

  event = get_frame_event_at(mt->event_list, tc, mt->fm_edit_event, TRUE);

  event = get_prev_fm(mt, mt->current_track, event);

  if (event != NULL) tc = get_event_timecode(event);

  mt_tl_move(mt, tc / TICKS_PER_SECOND_DBL);
}


void on_next_fm_clicked(LiVESWidget * button, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  weed_timecode_t tc;
  weed_plant_t *event;
  double secs = mt->ptr_time;
  tc = q_gint64(secs * TICKS_PER_SECOND_DBL, mt->fps);

  event = get_frame_event_at(mt->event_list, tc, mt->fm_edit_event, TRUE);

  event = get_next_fm(mt, mt->current_track, event);

  if (event != NULL) tc = get_event_timecode(event);

  mt_tl_move(mt, tc / TICKS_PER_SECOND_DBL);
}


static weed_timecode_t get_prev_node_tc(lives_mt * mt, weed_timecode_t tc) {
  int num_params = num_in_params(get_weed_filter(mt->current_fx), FALSE, FALSE);
  int i, error;
  weed_timecode_t prev_tc = -1;
  weed_plant_t *event;
  weed_timecode_t ev_tc;

  if (pchain == NULL) return tc;

  for (i = 0; i < num_params; i++) {
    event = (weed_plant_t *)pchain[i];
    while (event != NULL && (ev_tc = get_event_timecode(event)) < tc) {
      if (ev_tc > prev_tc) prev_tc = ev_tc;
      event = (weed_plant_t *)weed_get_voidptr_value(event, WEED_LEAF_NEXT_CHANGE, &error);
    }
  }
  return prev_tc;
}


static weed_timecode_t get_next_node_tc(lives_mt * mt, weed_timecode_t tc) {
  int num_params = num_in_params(get_weed_filter(mt->current_fx), FALSE, FALSE);
  int i, error;
  weed_timecode_t next_tc = -1;
  weed_plant_t *event;
  weed_timecode_t ev_tc;

  if (pchain == NULL) return tc;

  for (i = 0; i < num_params; i++) {
    event = (weed_plant_t *)pchain[i];
    while (event != NULL && (ev_tc = get_event_timecode(event)) <= tc)
      event = (weed_plant_t *)weed_get_voidptr_value(event, WEED_LEAF_NEXT_CHANGE, &error);
    if (event != NULL) {
      if (next_tc == -1 || ev_tc < next_tc) next_tc = ev_tc;
    }
  }
  return next_tc;
}


static boolean is_node_tc(lives_mt * mt, weed_timecode_t tc) {
  int num_params = num_in_params(get_weed_filter(mt->current_fx), FALSE, FALSE);
  int i, error;
  weed_plant_t *event;
  weed_timecode_t ev_tc;

  for (i = 0; i < num_params; i++) {
    event = (weed_plant_t *)pchain[i];
    ev_tc = -1;
    while (event != NULL &&
           (ev_tc = get_event_timecode(event)) < tc) event = (weed_plant_t *)weed_get_voidptr_value(event, WEED_LEAF_NEXT_CHANGE, &error);
    if (ev_tc == tc && !weed_plant_has_leaf(event, WEED_LEAF_IS_DEF_VALUE)) return TRUE;
  }
  return FALSE;
}


// apply the param changes and update widgets
void on_node_spin_value_changed(LiVESSpinButton * spinbutton, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  weed_timecode_t init_tc = get_event_timecode(mt->init_event);
  weed_timecode_t otc = lives_spin_button_get_value(spinbutton) * TICKS_PER_SECOND_DBL + init_tc;
  weed_timecode_t tc = q_gint64(otc, mt->fps);
  weed_timecode_t pn_tc, nn_tc;
  double timesecs;
  boolean auto_prev = mt->opts.fx_auto_preview;

  lives_signal_handlers_block_by_func(spinbutton, (livespointer)on_node_spin_value_changed, (livespointer)mt);

  if (!mt->block_tl_move) {
    timesecs = otc / TICKS_PER_SECOND_DBL;
    mt->block_node_spin = TRUE;
    if (mt->current_track >= 0 && (mt->opts.fx_auto_preview || mainw->play_window != NULL))
      mt->no_frame_update = TRUE; // we will preview anyway later, so don't do it twice
    mt_tl_move(mt, timesecs);
    mt->no_frame_update = FALSE;
    mt->block_node_spin = FALSE;
  }

  if (mt->prev_fx_time == 0. || tc == init_tc) {
    //add_mt_param_box(mt); // sensitise/desensitise reinit params
  } else mt->prev_fx_time = mt_get_effect_time(mt);

  interpolate_params((weed_plant_t *)mt->current_rfx->source, pchain, tc);

  set_params_unchanged(mt, mt->current_rfx);

  get_track_index(mt, tc);

  mt->opts.fx_auto_preview = FALSE; // we will preview anyway later, so don't do it twice
  mt->current_rfx->needs_reinit = FALSE;
  mainw->block_param_updates = TRUE;
  update_visual_params(mt->current_rfx, FALSE);
  mainw->block_param_updates = FALSE;
  if (mt->current_rfx->needs_reinit) {
    mt->current_rfx->needs_reinit = FALSE;
    weed_reinit_effect(mt->current_rfx->source, TRUE);
  }

  mt->opts.fx_auto_preview = auto_prev;

  // get timecodes of previous and next fx nodes
  pn_tc = get_prev_node_tc(mt, tc);
  nn_tc = get_next_node_tc(mt, tc);

  if (pn_tc > -1) lives_widget_set_sensitive(mt->prev_node_button, TRUE);
  else lives_widget_set_sensitive(mt->prev_node_button, FALSE);

  if (nn_tc > -1) lives_widget_set_sensitive(mt->next_node_button, TRUE);
  else lives_widget_set_sensitive(mt->next_node_button, FALSE);

  if (is_node_tc(mt, tc)) {
    lives_widget_set_sensitive(mt->del_node_button, TRUE);
    lives_widget_set_sensitive(mt->apply_fx_button, FALSE);
  } else lives_widget_set_sensitive(mt->del_node_button, FALSE);

  lives_widget_set_sensitive(mt->resetp_button, check_can_resetp(mt));

  if (mt->current_track >= 0) {
    if (mt->opts.fx_auto_preview || mainw->play_window != NULL) mt_show_current_frame(mt, FALSE);
  }

  lives_signal_handlers_unblock_by_func(spinbutton, (livespointer)on_node_spin_value_changed, (livespointer)mt);
}


static boolean check_can_resetp(lives_mt * mt) {
  int nparams;
  boolean can_reset = FALSE;
  weed_plant_t *filter = get_weed_filter(mt->current_fx);

  /// create new in_params with default vals.
  weed_plant_t **def_params = weed_params_create(filter, TRUE);
  weed_plant_t **in_params = weed_instance_get_in_params(mt->current_rfx->source, &nparams);
  int ninpar, i;

  if (mt->init_event != NULL) {
    int num_in_tracks;
    int *in_tracks = weed_get_int_array_counted(mt->init_event, WEED_LEAF_IN_TRACKS, &num_in_tracks);
    if (num_in_tracks > 0) {
      for (i = 0; i < num_in_tracks; i++) {
        if (in_tracks[i] == mt->current_track) {
          mt->track_index = i;
          break;
	  // *INDENT-OFF*
	}}}}
  // *INDENT-ON*

  /// check for variance
  for (i = 0; i < nparams; i++) {
    if (weed_param_is_hidden(in_params[i])) continue;
    if (is_perchannel_multiw(in_params[i])) {
      /// if param is "value_per_channel", leave the other channels unaltered
      fill_param_vals_to(def_params[i], weed_param_get_template(def_params[i]), mt->track_index);
      if (!weed_leaf_elements_equate(in_params[i], WEED_LEAF_VALUE, def_params[i], WEED_LEAF_VALUE, mt->track_index)) {
        can_reset = TRUE;
        break;
      }
    } else {
      if (!weed_leaf_elements_equate(in_params[i], WEED_LEAF_VALUE, def_params[i], WEED_LEAF_VALUE, -1)) {
        can_reset = TRUE;
        break;
      }
    }
  }
  ninpar = num_in_params(filter, FALSE, FALSE);
  for (i = 0; i < ninpar; i++) {
    weed_plant_free(def_params[i]);
  }
  lives_free(def_params);
  lives_free(in_params);
  return can_reset;
}

void on_resetp_clicked(LiVESWidget * button, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  int nparams;
  boolean can_apply = FALSE;
  boolean aprev = mt->opts.fx_auto_preview;
  weed_plant_t *filter = get_weed_filter(mt->current_fx);

  /// create new in_params with default vals.
  weed_plant_t **def_params = weed_params_create(filter, TRUE);
  weed_plant_t **in_params = weed_instance_get_in_params(mt->current_rfx->source, &nparams);
  int i;

  if (mt->init_event != NULL) {
    int num_in_tracks;
    int *in_tracks = weed_get_int_array_counted(mt->init_event, WEED_LEAF_IN_TRACKS, &num_in_tracks);
    if (num_in_tracks > 0) {
      for (i = 0; i < num_in_tracks; i++) {
        if (in_tracks[i] == mt->current_track) {
          mt->track_index = i;
          break;
	  // *INDENT-OFF*
	}}}}
  // *INDENT-ON*

  /// set params to the default value,
  /// and if any change we'll sensitize the "Apply Button"
  for (i = 0; i < nparams; i++) {
    if (weed_param_is_hidden(in_params[i])) continue;
    if (is_perchannel_multiw(in_params[i])) {
      /// if param is "value_per_channel", leave the other channels unaltered
      fill_param_vals_to(def_params[i], weed_param_get_template(def_params[i]), mt->track_index);
      if (!weed_leaf_elements_equate(in_params[i], WEED_LEAF_VALUE, def_params[i], WEED_LEAF_VALUE, mt->track_index)) {
        weed_leaf_dup_nth(in_params[i], def_params[i], WEED_LEAF_VALUE, mt->track_index);
        can_apply = TRUE;
        mt->current_rfx->params[i].changed = TRUE;
      }
    } else {
      if (!weed_leaf_elements_equate(in_params[i], WEED_LEAF_VALUE, def_params[i], WEED_LEAF_VALUE, -1)) {
        weed_leaf_dup(in_params[i], def_params[i], WEED_LEAF_VALUE);
        mt->current_rfx->params[i].changed = TRUE;
        can_apply = TRUE;
      }
    }
    weed_plant_free(def_params[i]);
  }
  lives_free(def_params);
  lives_free(in_params);

  mt->opts.fx_auto_preview = FALSE;
  mainw->block_param_updates = TRUE;
  mt->current_rfx->needs_reinit = FALSE;
  mt->current_rfx->flags |= RFX_FLAGS_NO_RESET;
  update_visual_params(mt->current_rfx, TRUE);
  mainw->block_param_updates = FALSE;
  if (mt->current_rfx->needs_reinit) {
    weed_reinit_effect(mt->current_rfx->source, TRUE);
    mt->current_rfx->needs_reinit = FALSE;
  }
  mt->current_rfx->flags ^= RFX_FLAGS_NO_RESET;
  mt->opts.fx_auto_preview = aprev;
  lives_widget_set_sensitive(mt->apply_fx_button, can_apply);
  lives_widget_set_sensitive(mt->resetp_button, check_can_resetp(mt));
  activate_mt_preview(mt);
}


// node buttons
void on_next_node_clicked(LiVESWidget * button, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  weed_timecode_t init_tc = get_event_timecode(mt->init_event);
  weed_timecode_t tc = q_gint64(lives_spin_button_get_value(LIVES_SPIN_BUTTON(mt->node_spinbutton)) * TICKS_PER_SECOND_DBL +
                                init_tc,
                                mt->fps);
  weed_timecode_t next_tc = get_next_node_tc(mt, tc);
  lives_spin_button_set_value(LIVES_SPIN_BUTTON(mt->node_spinbutton), (next_tc - init_tc) / TICKS_PER_SECOND_DBL);
  if (mt->current_track >= 0) mt_show_current_frame(mt, FALSE);
  lives_widget_set_sensitive(mt->apply_fx_button, FALSE);
  lives_widget_set_sensitive(mt->resetp_button, check_can_resetp(mt));
}


void on_prev_node_clicked(LiVESWidget * button, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  weed_timecode_t init_tc = get_event_timecode(mt->init_event);
  weed_timecode_t tc = q_gint64(lives_spin_button_get_value(LIVES_SPIN_BUTTON(mt->node_spinbutton)) * TICKS_PER_SECOND_DBL +
                                init_tc,
                                mt->fps);
  weed_timecode_t prev_tc = get_prev_node_tc(mt, tc);
  lives_spin_button_set_value(LIVES_SPIN_BUTTON(mt->node_spinbutton), (prev_tc - init_tc) / TICKS_PER_SECOND_DBL);
  if (mt->current_track >= 0) mt_show_current_frame(mt, FALSE);
  lives_widget_set_sensitive(mt->apply_fx_button, FALSE);
  lives_widget_set_sensitive(mt->resetp_button, check_can_resetp(mt));
}


void on_del_node_clicked(LiVESWidget * button, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;

  int error;

  weed_plant_t **in_params = weed_get_plantptr_array((weed_plant_t *)mt->current_rfx->source, WEED_LEAF_IN_PARAMETERS, &error);

  weed_plant_t *event;
  weed_plant_t *prev_pchange, *next_pchange;

  weed_timecode_t ev_tc;
  weed_timecode_t tc = q_gint64(lives_spin_button_get_value(LIVES_SPIN_BUTTON(mt->node_spinbutton)) * TICKS_PER_SECOND_DBL +
                                get_event_timecode(mt->init_event), mt->fps);

  char *filter_name;

  int num_params = num_in_params((weed_plant_t *)mt->current_rfx->source, FALSE, FALSE);

  register int i;

  if (mt->idlefunc > 0) {
    lives_source_remove(mt->idlefunc);
    mt->idlefunc = 0;
  }

  // TODO - undo: but we need to reinsert the values in pchains...

  for (i = 0; i < num_params; i++) {
    event = (weed_plant_t *)pchain[i];
    ev_tc = -1;
    while (event != NULL && (ev_tc = get_event_timecode(event)) < tc)
      event = (weed_plant_t *)weed_get_voidptr_value(event, WEED_LEAF_NEXT_CHANGE, &error);
    if (ev_tc == tc) {
      prev_pchange = (weed_plant_t *)weed_get_voidptr_value(event, WEED_LEAF_PREV_CHANGE, &error);
      next_pchange = (weed_plant_t *)weed_get_voidptr_value(event, WEED_LEAF_NEXT_CHANGE, &error);
      if (event != pchain[i]) {
        delete_event(mt->event_list, event);
        if (prev_pchange != NULL) weed_set_voidptr_value(prev_pchange, WEED_LEAF_NEXT_CHANGE, next_pchange);
        if (next_pchange != NULL) weed_set_voidptr_value(next_pchange, WEED_LEAF_PREV_CHANGE, prev_pchange);
      } else {
        // is initial pchange, reset to defaults, c.f. paramspecial.c
        weed_plant_t *param = in_params[i];
        weed_plant_t *paramtmpl = weed_param_get_template(param);
        if (weed_plant_has_leaf(paramtmpl, WEED_LEAF_HOST_DEFAULT)) {
          weed_leaf_copy(event, WEED_LEAF_VALUE, paramtmpl, WEED_LEAF_HOST_DEFAULT);
        } else weed_leaf_copy(event, WEED_LEAF_VALUE, paramtmpl, WEED_LEAF_DEFAULT);
        if (is_perchannel_multiw(param)) {
          int num_in_tracks = weed_leaf_num_elements(mt->init_event, WEED_LEAF_IN_TRACKS);
          fill_param_vals_to(event, paramtmpl, num_in_tracks - 1);
        }
        weed_set_boolean_value(event, WEED_LEAF_IS_DEF_VALUE, WEED_TRUE);
      }
    }
  }
  lives_free(in_params);

  if (mt->current_fx == mt->avol_fx && mt->avol_init_event != NULL && mt->opts.aparam_view_list != NULL) {
    LiVESList *slist = mt->audio_draws;
    while (slist != NULL) {
      lives_widget_queue_draw((LiVESWidget *)slist->data);
      slist = slist->next;
    }
  }

  filter_name = weed_filter_idx_get_name(mt->current_fx, FALSE, FALSE, FALSE);

  d_print(_("Removed parameter values for effect %s at time %s\n"), filter_name, mt->timestring);
  lives_free(filter_name);
  mt->block_tl_move = TRUE;
  on_node_spin_value_changed(LIVES_SPIN_BUTTON(mt->node_spinbutton), (livespointer)mt);
  mt->block_tl_move = FALSE;
  lives_widget_set_sensitive(mt->del_node_button, FALSE);
  if (mt->current_track >= 0) {
    mt_show_current_frame(mt, FALSE);
  }
  lives_widget_set_sensitive(mt->apply_fx_button, FALSE);
  lives_widget_set_sensitive(mt->resetp_button, check_can_resetp(mt));

  if (!mt->auto_changed) mt->auto_changed = TRUE;
  if (prefs->mt_auto_back > 0) mt->idlefunc = mt_idle_add(mt);
  else if (prefs->mt_auto_back == 0) mt_auto_backup(mt);
  else lives_widget_set_sensitive(mt->backup, TRUE);
  mt->changed = TRUE;
}


void mt_fixup_events(lives_mt * mt, weed_plant_t *old_event, weed_plant_t *new_event) {
  // if any "notable" events have changed, we should repoint them here

  if (mt == NULL) return;

  if (mt->fm_edit_event == old_event) {
    //g_print("fme event\n");
    mt->fm_edit_event = new_event;
  }
  if (mt->init_event == old_event) {
    //g_print("ie event\n");
    mt->init_event = new_event;
  }
  if (mt->selected_init_event == old_event) {
    //g_print("se event\n");
    mt->selected_init_event = new_event;
  }
  if (mt->avol_init_event == old_event) {
    //g_print("aie event\n");
    mt->avol_init_event = new_event;
  }
  if (mt->specific_event == old_event) {
    //g_print("spec event\n");
    mt->specific_event = new_event;
  }
}


static void combine_ign(weed_plant_t *xnew, weed_plant_t *xold) {
  int num, numo, *nign, *oign, i;

  // combine WEED_LEAF_IGNORE values using NAND
  oign = weed_get_boolean_array_counted(xold, WEED_LEAF_IGNORE, &numo);
  if (!numo) return;
  nign = weed_get_boolean_array_counted(xnew, WEED_LEAF_IGNORE, &num);
  for (i = 0; i < num; i++) if (i >= numo || oign[i] == WEED_FALSE) nign[i] = WEED_FALSE;
  weed_set_boolean_array(xnew, WEED_LEAF_IGNORE, num, nign);
  lives_free(oign);
  lives_free(nign);
}


static void add_to_pchain(weed_plant_t *event_list, weed_plant_t *init_event, weed_plant_t *pchange, int index,
                          weed_timecode_t tc) {
  weed_plant_t *event = (weed_plant_t *)pchain[index];
  weed_plant_t *last_event = NULL;
  int error;

  while (event != NULL && get_event_timecode(event) < tc) {
    last_event = event;
    event = (weed_plant_t *)weed_get_voidptr_value(event, WEED_LEAF_NEXT_CHANGE, &error);
  }

  if (event != NULL && get_event_timecode(event) == tc) {
    // replace an existing change
    weed_plant_t *next_event = (weed_plant_t *)weed_get_voidptr_value(event, WEED_LEAF_NEXT_CHANGE, &error);
    if (next_event != NULL) weed_set_voidptr_value(next_event, WEED_LEAF_PREV_CHANGE, pchange);
    weed_set_voidptr_value(pchange, WEED_LEAF_NEXT_CHANGE, next_event);
    if (event == pchain[index]) weed_leaf_delete(pchange, WEED_LEAF_IGNORE); // never ignore our init pchanges
    if (weed_plant_has_leaf(pchange, WEED_LEAF_IGNORE)) combine_ign(pchange, event);
    delete_event(event_list, event);
  } else {
    weed_set_voidptr_value(pchange, WEED_LEAF_NEXT_CHANGE, event);
    if (event != NULL) weed_set_voidptr_value(event, WEED_LEAF_PREV_CHANGE, pchange);
  }

  if (last_event != NULL) weed_set_voidptr_value(last_event, WEED_LEAF_NEXT_CHANGE, pchange);
  else {
    // update "in_params" for init_event
    int numin;
    void **in_params = weed_get_voidptr_array_counted(init_event, WEED_LEAF_IN_PARAMETERS, &numin);
    in_params[index] = pchain[index] = (void *)pchange;
    weed_set_voidptr_array(init_event, WEED_LEAF_IN_PARAMETERS, numin, in_params);
    lives_free(in_params);
  }
  weed_set_voidptr_value(pchange, WEED_LEAF_PREV_CHANGE, last_event);
}


void activate_mt_preview(lives_mt * mt) {
  // called from paramwindow.c when a parameter changes - show effect with currently unapplied values
  static boolean norecurse = FALSE;
  if (norecurse) return;
  norecurse = TRUE;

  if (mt->poly_state == POLY_PARAMS) {
    if (mt->opts.fx_auto_preview) {
      mainw->no_interp = TRUE; // no interpolation - parameter is in an uncommited state
      mt_show_current_frame(mt, FALSE);
      mainw->no_interp = FALSE;
    }
    if (mt->apply_fx_button != NULL) {
      int nparams = mt->current_rfx->num_params;
      lives_widget_set_sensitive(mt->apply_fx_button, FALSE);
      for (int i = 0; i < nparams; i++) {
        if (mt->current_rfx->params[i].changed) {
          lives_widget_set_sensitive(mt->apply_fx_button, TRUE);
          break;
        }
      }
      lives_widget_set_sensitive(mt->resetp_button, check_can_resetp(mt));
    }
  } else mt_show_current_frame(mt, FALSE);
  norecurse = FALSE;
}


void on_set_pvals_clicked(LiVESWidget * button, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;

  weed_plant_t *inst = (weed_plant_t *)mt->current_rfx->source;
  weed_plant_t *param, *pchange, *at_event;

  int error;

  weed_timecode_t tc = q_gint64(lives_spin_button_get_value(LIVES_SPIN_BUTTON(mt->node_spinbutton)) * TICKS_PER_SECOND_DBL +
                                get_event_timecode(mt->init_event), mt->fps);

  int *tracks;

  char *tmp, *tmp2;
  char *filter_name;
  char *tname, *track_desc;

  boolean has_multi = FALSE;
  boolean was_changed = FALSE;
  boolean needs_idlefunc = FALSE;
  boolean did_backup = mt->did_backup;
  int nparams;
  int numtracks;
  register int i;

  if (mt->current_rfx != NULL && mainw->textwidget_focus != NULL) {
    // make sure text widgets are updated if they activate the default
    LiVESWidget *textwidget = (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(mainw->textwidget_focus),
                              "textwidget");
    after_param_text_changed(textwidget, mt->current_rfx);
  }
  if (mt->framedraw != NULL) {
    if (!check_filewrite_overwrites()) {
      return;
    }
  }

  if (mt->idlefunc > 0) {
    needs_idlefunc = TRUE;
    lives_source_remove(mt->idlefunc);
    mt->idlefunc = 0;
  }

  lives_widget_set_sensitive(mt->apply_fx_button, FALSE);
  lives_widget_set_sensitive(mt->resetp_button, check_can_resetp(mt));

  for (i = 0; ((param = weed_inst_in_param(inst, i, FALSE, FALSE)) != NULL); i++) {
    if (!mt->current_rfx->params[i].changed) continue; // set only user changed parameters
    pchange = weed_plant_new(WEED_PLANT_EVENT);
    weed_set_int_value(pchange, WEED_LEAF_HINT, WEED_EVENT_HINT_PARAM_CHANGE);
    weed_set_int64_value(pchange, WEED_LEAF_TIMECODE, tc);
    weed_set_voidptr_value(pchange, WEED_LEAF_INIT_EVENT, mt->init_event);
    weed_set_int_value(pchange, WEED_LEAF_INDEX, i);
    if (is_perchannel_multiw(param)) {
      has_multi = TRUE;
      if (weed_plant_has_leaf(param, WEED_LEAF_IGNORE)) {
        int j;
        int num_vals;
        int *ign = weed_get_boolean_array_counted(param, WEED_LEAF_IGNORE, &num_vals);
        weed_set_boolean_array(pchange, WEED_LEAF_IGNORE, num_vals, ign);
        for (j = 0; j < num_vals; j++) {
          if (ign[j] == WEED_FALSE) {
            was_changed = TRUE;
            ign[j] = WEED_TRUE;
          }
        }
        if (was_changed)
          weed_set_boolean_array(param, WEED_LEAF_IGNORE, num_vals, ign);
        lives_free(ign);
      }
    } else was_changed = TRUE;

    weed_leaf_copy(pchange, WEED_LEAF_VALUE, param, WEED_LEAF_VALUE);
    //weed_add_plant_flags(pchange, WEED_LEAF_READONLY_PLUGIN);

    // mark the default value as changed, so the user can delete this node (which will reset to defaults)
    if (weed_plant_has_leaf(pchange, WEED_LEAF_IS_DEF_VALUE))
      weed_leaf_delete(pchange, WEED_LEAF_IS_DEF_VALUE);

    // set next_change, prev_change
    add_to_pchain(mt->event_list, mt->init_event, pchange, i, tc);

    at_event = get_frame_event_at(mt->event_list, tc, mt->init_event, TRUE);
    insert_param_change_event_at(mt->event_list, at_event, pchange);
  }

  if (!was_changed) {
    if (needs_idlefunc || (!did_backup && mt->auto_changed))
      mt->idlefunc = mt_idle_add(mt);
    return;
  }

  filter_name = weed_filter_idx_get_name(mt->current_fx, FALSE, FALSE, FALSE);
  tracks = weed_get_int_array(mt->init_event, WEED_LEAF_IN_TRACKS, &error);
  numtracks = enabled_in_channels(get_weed_filter(mt->current_fx), TRUE); // count repeated channels

  nparams = mt->current_rfx->num_params;
  for (i = 0; i < nparams; i++) mt->current_rfx->params[i].changed = FALSE;

  switch (numtracks) {
  case 1:
    tname = lives_fx_cat_to_text(LIVES_FX_CAT_EFFECT, FALSE); // effect
    track_desc = lives_strdup_printf(_("track %s"), (tmp = get_track_name(mt, tracks[0], FALSE)));
    lives_free(tmp);
    break;
  case 2:
    tname = lives_fx_cat_to_text(LIVES_FX_CAT_TRANSITION, FALSE); // transition
    track_desc = lives_strdup_printf(_("tracks %s and %s"), (tmp = get_track_name(mt, tracks[0], FALSE)), (tmp2 = get_track_name(mt,
                                     tracks[1],
                                     FALSE)));
    lives_free(tmp);
    lives_free(tmp2);
    break;
  default:
    tname = lives_fx_cat_to_text(LIVES_FX_CAT_COMPOSITOR, FALSE); // compositor
    if (has_multi) {
      track_desc = lives_strdup_printf(_("track %s"), (tmp = get_track_name(mt, mt->current_track, mt->aud_track_selected)));
      lives_free(tmp);
    } else track_desc = lives_strdup(_("selected tracks"));
    break;
  }
  lives_free(tracks);
  if (mt->current_fx == mt->avol_fx) {
    lives_free(tname);
    tname = lives_strdup(_("audio"));
  }

  d_print(_("Set parameter values for %s %s on %s at time %s\n"), tname, filter_name, track_desc, mt->timestring);
  lives_free(filter_name);
  lives_free(tname);
  lives_free(track_desc);

  lives_widget_set_sensitive(mt->del_node_button, TRUE);

  if (mt->current_fx == mt->avol_fx && mt->avol_init_event != NULL && mt->opts.aparam_view_list != NULL) {
    LiVESList *slist = mt->audio_draws;
    while (slist != NULL) {
      lives_widget_queue_draw((LiVESWidget *)slist->data);
      slist = slist->next;
    }
  }

  if (mt->current_track >= 0) {
    mt_show_current_frame(mt, FALSE); // show full preview in play window
  }

  if (!mt->auto_changed) mt->auto_changed = TRUE;
  else lives_widget_set_sensitive(mt->backup, TRUE);
  if (prefs->mt_auto_back > 0) mt->idlefunc = mt_idle_add(mt);
  else if (prefs->mt_auto_back == 0) mt_auto_backup(mt);
  mt->changed = TRUE;
}

//////////////////////////////////////////////////////////////////////////
static int free_tt_key;
static int elist_errors;

LiVESList *load_layout_map(void) {
  // load in a layout "map" for the set, [create mainw->current_layouts_map]

  // the layout.map file maps clip "unique_id" and "handle" stored in the header.lives file and matches it with
  // the clip numbers in each layout file (.lay) file for that set

  // [thus a layout could be transferred to another set and the unique_id's/handles altered,
  // one could use a layout.map and a layout file as a template for
  // rendering many different sets]

  // this is called from recover_layout_map() in saveplay.c, where the map entries entry->list are assigned
  // to files (clips)

  char **array;
  LiVESList *lmap = NULL;
  layout_map *lmap_entry;
  uint64_t unique_id;
  ssize_t bytes;

  char *lmap_name = lives_build_filename(prefs->workdir, mainw->set_name, LAYOUTS_DIRNAME, LAYOUT_MAP_FILENAME, NULL);
  char *handle;
  char *entry;
  char *string;
  char *name;

  int len, nm, i;
  int fd;
  int retval;

  boolean err = FALSE;

  if (!lives_file_test(lmap_name, LIVES_FILE_TEST_EXISTS)) {
    lives_free(lmap_name);
    return NULL;
  }

  do {
    retval = 0;
    fd = lives_open2(lmap_name, O_RDONLY);
    if (fd < 0) {
      retval = do_read_failed_error_s_with_retry(lmap_name, NULL, NULL);
    } else {
      while (1) {
        bytes = lives_read_le_buffered(fd, &len, 4, TRUE);
        if (bytes < 4) {
          break;
        }
        handle = (char *)lives_malloc(len + 1);
        bytes = lives_read_buffered(fd, handle, len, TRUE);
        if (bytes < len) {
          break;
        }
        lives_memset(handle + len, 0, 1);
        bytes = lives_read_le_buffered(fd, &unique_id, 8, TRUE);
        if (bytes < 8) {
          break;
        }
        bytes = lives_read_le_buffered(fd, &len, 4, TRUE);
        if (bytes < 4) {
          break;
        }
        name = (char *)lives_malloc(len + 1);
        bytes = lives_read_buffered(fd, name, len, TRUE);
        if (bytes < len) {
          break;
        }
        lives_memset(name + len, 0, 1);
        bytes = lives_read_le_buffered(fd, &nm, 4, TRUE);
        if (bytes < 4) {
          break;
        }

        ////////////////////////////////////////////////////////////
        // this is one mapping entry (actually only the unique id matters)
        lmap_entry = (layout_map *)lives_malloc(sizeof(layout_map));
        lmap_entry->handle = handle;
        lmap_entry->unique_id = unique_id;
        lmap_entry->name = name;
        lmap_entry->list = NULL;
        ///////////////////////////////////////////

        //////////////////////////////////////////////
        // now we read in a list of layouts this clip is used in, and create mainw->current_layouts_map

        // format is:
        // layout_file_filename|clip_number|max_frame_used|clip fps|max audio time|audio rate

        // here we only add layout_file_filename

        for (i = 0; i < nm; i++) {
          bytes = lives_read_le_buffered(fd, &len, 4, TRUE);
          if (bytes < sizint) {
            err = TRUE;
            break;
          }
          entry = (char *)lives_malloc(len + 1);
          bytes = lives_read_buffered(fd, entry, len, TRUE);
          if (bytes < len) {
            err = TRUE;
            break;
          }
          lives_memset(entry + len, 0, 1);
          string = repl_workdir(entry, FALSE); // allow relocation of workdir
          lmap_entry->list = lives_list_append(lmap_entry->list, lives_strdup(string));
          array = lives_strsplit(string, "|", -1);
          lives_free(string);
          mainw->current_layouts_map = lives_list_append_unique(mainw->current_layouts_map, array[0]);
          lives_strfreev(array);
          lives_free(entry);
        }
        if (err) break;
        lmap = lives_list_append(lmap, lmap_entry);
        //g_print("add layout %p %p %p\n", lmap, lmap_entry, lmap_entry->list);
      }
    }

    ////////////////////////////////////////////////////////////////////////////

    if (fd >= 0) lives_close_buffered(fd);

    if (err) {
      retval = do_read_failed_error_s_with_retry(lmap_name, NULL, NULL);
    }
  } while (retval == LIVES_RESPONSE_RETRY);

  lives_free(lmap_name);
  return lmap;
}


void save_layout_map(int *lmap, double * lmap_audio, const char *file, const char *dir) {
  // in the file "layout.map", we map each clip used in the set to which layouts (if any) it is used in
  // we also record the highest frame number used and the max audio time; and the current fps of the clip
  // and audio rate;

  // one entry per layout file, per clip

  // map format in memory is:

  // this was knocked together very hastily, so it could probably be improved upon

  // layout_file_filename|clip_number|max_frame_used|clip fps|max audio time|audio rate

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

  // lmap[] and lmap_audio[] hold the highest frame numbers and highest audio time respectively
  // when we save a layout we update these from the current layout

  LiVESList *map, *map_next;

  char *new_entry;
  char *map_name = NULL, *ldir = NULL;
  char *string;

  uint32_t size = 0;

  double max_atime;

  boolean written = FALSE;

  boolean write_to_file = TRUE;

  int fd = 0;
  int len;
  int retval;
  int max_frame;

  register int i;

  if (dir == NULL && strlen(mainw->set_name) == 0) return;

  if (file == NULL) write_to_file = FALSE;
  else {

    if (file != NULL && (mainw->current_layouts_map == NULL ||
                         !lives_list_find(mainw->current_layouts_map, file)))
      mainw->current_layouts_map = lives_list_append(mainw->current_layouts_map, lives_strdup(file));
    if (dir == NULL) ldir = lives_build_filename(prefs->workdir, mainw->set_name, LAYOUTS_DIRNAME, NULL);
    else ldir = lives_strdup(dir);

    map_name = lives_build_filename(ldir, LAYOUT_MAP_FILENAME, NULL);

    lives_mkdir_with_parents(ldir, capable->umask);
  }

  do {
    retval = 0;
    if (write_to_file) fd = lives_create_buffered(map_name, DEF_FILE_PERMS);

    if (fd == -1) {
      retval = do_write_failed_error_s_with_retry(map_name, lives_strerror(errno), NULL);
    } else {
      mainw->write_failed = FALSE;

      for (i = 1; i <= MAX_FILES; i++) {
        // add or update
        if (mainw->files[i] != NULL) {

          if (mainw->files[i]->layout_map != NULL) {
            map = mainw->files[i]->layout_map;
            while (map != NULL) {
              map_next = map->next;
              if (map->data != NULL) {
                char **array = lives_strsplit((char *)map->data, "|", -1);
                if ((file != NULL && !strcmp(array[0], file)) || (file == NULL && dir == NULL &&
                    !lives_file_test(array[0], LIVES_FILE_TEST_EXISTS))) {
                  // remove prior entry
                  lives_free((livespointer)map->data);
                  mainw->files[i]->layout_map = lives_list_delete_link(mainw->files[i]->layout_map, map);
                  break;
                }
                lives_strfreev(array);
              }
              map = map_next;
            }
          }

          if (file != NULL && ((lmap != NULL && lmap[i] != 0) || (lmap_audio != NULL && lmap_audio[i] != 0.))) {
            if (lmap != NULL) max_frame = lmap[i];
            else max_frame = 0;
            if (lmap_audio != NULL) max_atime = lmap_audio[i];
            else max_atime = 0.;

            new_entry = lives_strdup_printf("%s|%d|%d|%.8f|%.8f|%.8f", file, i, max_frame, mainw->files[i]->fps,
                                            max_atime, (double)((int)((double)(mainw->files[i]->arps) /
                                                (double)mainw->files[i]->arate * 10000. + .5)) / 10000.);
            mainw->files[i]->layout_map = lives_list_prepend(mainw->files[i]->layout_map, new_entry);
          }

          if (write_to_file && ((map = mainw->files[i]->layout_map) != NULL)) {
            written = TRUE;
            len = strlen(mainw->files[i]->handle);
            lives_write_le_buffered(fd, &len, 4, TRUE);
            lives_write_buffered(fd, mainw->files[i]->handle, len, TRUE);
            lives_write_le_buffered(fd, &mainw->files[i]->unique_id, 8, TRUE);
            len = strlen(mainw->files[i]->name);
            lives_write_le_buffered(fd, &len, 4, TRUE);
            lives_write_buffered(fd, mainw->files[i]->name, len, TRUE);
            len = lives_list_length(map);
            lives_write_le_buffered(fd, &len, 4, TRUE);
            while (map != NULL) {
              string = repl_workdir((char *)map->data, TRUE); // allow relocation of workdir
              len = strlen(string);
              lives_write_le_buffered(fd, &len, 4, TRUE);
              lives_write_buffered(fd, string, len, TRUE);
              lives_free(string);
              map = map->next;
            }
          }
        }
        if (mainw->write_failed) break;
      }
      if (mainw->write_failed) {
        retval = do_write_failed_error_s_with_retry(map_name, NULL, NULL);
        mainw->write_failed = FALSE;
      }

    }
    if (retval == LIVES_RESPONSE_RETRY && fd >= 0) lives_close_buffered(fd);
  } while (retval == LIVES_RESPONSE_RETRY);

  if (write_to_file && retval != LIVES_RESPONSE_CANCEL) {
    lives_close_buffered(fd);
    size = sget_file_size(map_name);

    if (size == 0 || !written) {
      LIVES_DEBUG("Removing layout map file: ");
      LIVES_DEBUG(map_name);
      lives_rm(map_name);
    }

    LIVES_DEBUG("Removing layout dir: ");
    LIVES_DEBUG(ldir);
    lives_rmdir(ldir, FALSE);
  }

  if (write_to_file) {
    lives_free(ldir);
    lives_free(map_name);
  }
}


void add_markers(lives_mt * mt, weed_plant_t *event_list, boolean add_block_ids) {
  // add "block_start" and "block_unordered" markers to a timeline
  // this is done when we save an event_list (layout file).
  // these markers are removed when the event_list is loaded and displayed

  // if add_block_ids id FALSE, we add block start markers only where blocks are split
  // if it is TRUE, we add block start markers for all blocks along with the block uid.
  // This helps us keep the same block selected for undo/redo. (work in progress)

  // other hosts are not bound to take notice of "marker" events, so these could be absent or misplaced
  // when the layout is reloaded

  LiVESList *track_blocks = NULL;
  LiVESList *tlist = mt->video_draws;
  LiVESList *blist;
  track_rect *block;
  weed_timecode_t tc;
  LiVESWidget *eventbox;
  weed_plant_t *event;
  int track;

  while (tlist != NULL) {
    eventbox = (LiVESWidget *)tlist->data;
    block = (track_rect *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "blocks");
    track_blocks = lives_list_append(track_blocks, (livespointer)block);
    tlist = tlist->next;
  }

  event = get_first_event(event_list);

  while (event != NULL) {
    if (WEED_EVENT_IS_FRAME(event)) {
      tc = get_event_timecode(event);
      blist = track_blocks;
      track = 0;
      while (blist != NULL) {
        block = (track_rect *)blist->data;
        if (block != NULL) {
          if (block->prev != NULL && (get_event_timecode(block->prev->end_event) ==
                                      q_gint64(tc - TICKS_PER_SECOND_DBL / mt->fps, mt->fps)) && (tc == get_event_timecode(block->start_event)) &&
              (get_frame_event_clip(block->prev->end_event, track) == get_frame_event_clip(block->start_event, track))) {

            insert_marker_event_at(mt->event_list, event, EVENT_MARKER_BLOCK_START, LIVES_INT_TO_POINTER(track));

            if (mt->audio_draws != NULL && lives_list_length(mt->audio_draws) >= track + mt->opts.back_audio_tracks) {
              // insert in audio too
              insert_marker_event_at(mt->event_list, event, EVENT_MARKER_BLOCK_START,
                                     LIVES_INT_TO_POINTER(-track - mt->opts.back_audio_tracks - 1));
            }
          }
          if (!block->ordered)  {
            insert_marker_event_at(mt->event_list, event, EVENT_MARKER_BLOCK_UNORDERED, LIVES_INT_TO_POINTER(track));
          }
          if (event == block->end_event) blist->data = block->next;
        }
        track++;
        blist = blist->next;
      }
    }
    event = get_next_event(event);
  }

  if (track_blocks != NULL) lives_list_free(track_blocks);
}


boolean set_new_set_name(lives_mt * mt) {
  char new_set_name[MAX_SET_NAME_LEN];

  char *tmp;

  boolean response;
  boolean needs_idlefunc = FALSE;

  if (mt != NULL && mt->idlefunc > 0) {
    lives_source_remove(mt->idlefunc);
    mt->idlefunc = 0;
    needs_idlefunc = TRUE;
  }

  do {
    // prompt for a set name, advise user to save set
    renamew = create_rename_dialog(4);
    lives_widget_show(renamew->dialog);
    lives_widget_context_update();
    response = lives_dialog_run(LIVES_DIALOG(renamew->dialog));
    if (response == LIVES_RESPONSE_CANCEL) {
      mainw->cancelled = CANCEL_USER;
      if (mt != NULL) {
        if (needs_idlefunc) {
          mt->idlefunc = mt_idle_add(mt);
        }
        mt_sensitise(mt);
      }
      return FALSE;
    }
    lives_snprintf(new_set_name, MAX_SET_NAME_LEN, "%s", (tmp = U82F(lives_entry_get_text(LIVES_ENTRY(renamew->entry)))));
    lives_widget_destroy(renamew->dialog);
    lives_freep((void **)&renamew);
    lives_free(tmp);
    lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET, TRUE);
  } while (!is_legal_set_name(new_set_name, FALSE));

  lives_snprintf(mainw->set_name, MAX_SET_NAME_LEN, "%s", new_set_name);

  if (!mt->auto_changed) mt->auto_changed = TRUE;
  if (prefs->mt_auto_back >= 0) mt_auto_backup(mt);
  else lives_widget_set_sensitive(mt->backup, TRUE);
  return TRUE;
}


boolean on_save_event_list_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  //  here we save a layout list (*.lay) file

  // we dump (serialise) the event_list plant, followed by all of its events
  // serialisation method is described in the weed-docs/weedevents spec.
  // (serialising of event_lists)

  // loading an event list is simply the reverse of this process

  lives_mt *mt = (lives_mt *)user_data;

  char *filt[] = {"*." LIVES_FILE_EXT_LAYOUT, NULL};

  int *layout_map;

  double *layout_map_audio;

  LiVESWidget *ar_checkbutton;
  LiVESWidget *hbox;

  weed_plant_t *event_list;

  char *layout_name;
  char *esave_dir;
  char *esave_file;

  char xlayout_name[PATH_MAX];

  boolean orig_ar_layout = prefs->ar_layout, ar_layout;
  boolean was_set = mainw->was_set;
  boolean retval = TRUE;

  boolean needs_idlefunc = FALSE;
  boolean did_backup;

  int retval2;
  int fd;

  if (mt == NULL) {
    event_list = mainw->stored_event_list;
    layout_name = mainw->stored_layout_name;
  } else {
    did_backup = mt->did_backup;
    mt_desensitise(mt);
    event_list = mt->event_list;
    layout_name = mt->layout_name;
  }

  // update layout map
  layout_map = update_layout_map(event_list);
  layout_map_audio = update_layout_map_audio(event_list);

  if (mainw->scrap_file != -1 && layout_map[mainw->scrap_file] != 0) {
    // can't save if we have generated frames
    do_layout_scrap_file_error();
    lives_freep((void **)&layout_map);
    lives_freep((void **)&layout_map_audio);
    mainw->cancelled = CANCEL_USER;
    if (mt != NULL) mt_sensitise(mt);
    return FALSE;
  }

  if (mainw->ascrap_file != -1 && layout_map_audio[mainw->ascrap_file] != 0) {
    // can't save if we have recorded audio
    do_layout_ascrap_file_error();
    lives_freep((void **)&layout_map);
    lives_freep((void **)&layout_map_audio);
    mainw->cancelled = CANCEL_USER;
    if (mt != NULL) mt_sensitise(mt);
    return FALSE;
  }

  if (mt != NULL && mt->idlefunc > 0) {
    lives_source_remove(mt->idlefunc);
    mt->idlefunc = 0;
    needs_idlefunc = TRUE;
  }

  if (strlen(mainw->set_name) > 0) {
    char *tmp;
    weed_set_string_value(event_list, WEED_LEAF_NEEDS_SET, (tmp = F2U8(mainw->set_name)));
    lives_free(tmp);
  } else if (mt != NULL) {
    if (mainw->interactive) {
      if (!set_new_set_name(mt)) {
        if (needs_idlefunc) {
          mt->idlefunc = mt_idle_add(mt);
        }
        mt_sensitise(mt);
        lives_freep((void **)&layout_map);
        lives_freep((void **)&layout_map_audio);
        return FALSE;
      }
    } else {
      if ((needs_idlefunc || (!did_backup && mt->auto_changed))) {
        mt->idlefunc = mt_idle_add(mt);
      }
      mt_sensitise(mt);
      lives_freep((void **)&layout_map);
      lives_freep((void **)&layout_map_audio);
      return FALSE;
    }
  }

  esave_dir = lives_build_filename(prefs->workdir, mainw->set_name, LAYOUTS_DIRNAME, NULL);
  lives_mkdir_with_parents(esave_dir, capable->umask);

  hbox = lives_hbox_new(FALSE, 0);

  ar_checkbutton = make_autoreload_check(LIVES_HBOX(hbox), prefs->ar_layout);
  lives_signal_connect(LIVES_GUI_OBJECT(ar_checkbutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(toggle_sets_pref),
                       (livespointer)PREF_AR_LAYOUT);

  lives_widget_show_all(hbox);

  if (!strlen(layout_name)) esave_file = choose_file(esave_dir, NULL, filt, LIVES_FILE_CHOOSER_ACTION_SAVE, NULL, hbox);
  else esave_file = choose_file(esave_dir, layout_name, filt, LIVES_FILE_CHOOSER_ACTION_SAVE, NULL, hbox);

  ar_layout = prefs->ar_layout;
  prefs->ar_layout = orig_ar_layout;

  if (esave_file != NULL) {
    lives_free(esave_dir);
    esave_dir = get_dir(esave_file);
  }

  if (esave_file == NULL || !check_storage_space(-1, FALSE)) {
    char *cdir;
    lives_rmdir(esave_dir, FALSE);

    cdir = lives_build_filename(prefs->workdir, mainw->set_name, NULL);
    lives_rmdir(cdir, FALSE);

    lives_freep((void **)&esave_file);
    lives_freep((void **)&esave_dir);
    lives_freep((void **)&layout_map);
    lives_freep((void **)&layout_map_audio);
    mainw->was_set = was_set;
    if (!was_set) lives_memset(mainw->set_name, 0, 1);
    mainw->cancelled = CANCEL_USER;

    if (mt != NULL) {
      mt_sensitise(mt);
      if (needs_idlefunc || (!did_backup && mt->auto_changed)) {
        mt->idlefunc = mt_idle_add(mt);
      }
    }
    return FALSE;
  }

  esave_file = ensure_extension(esave_file, LIVES_FILE_EXT_LAYOUT);

  lives_snprintf(xlayout_name, PATH_MAX, "%s", esave_file);
  get_basename(xlayout_name);

  if (mt != NULL) add_markers(mt, mt->event_list, FALSE);

  do {
    retval2 = 0;
    retval = TRUE;

    fd = lives_create_buffered(esave_file, DEF_FILE_PERMS);
    if (fd >= 0) {
      do_threaded_dialog(_("Saving layout"), FALSE);

      retval = save_event_list_inner(mt, fd, event_list, NULL);
      lives_close_buffered(fd);

      end_threaded_dialog();
    }

    if (!retval || fd < 0) {
      retval2 = do_write_failed_error_s_with_retry(esave_file, (fd < 0) ? lives_strerror(errno) : NULL, NULL);
      if (retval2 == LIVES_RESPONSE_CANCEL) {
        if (mt != NULL) {
          if (needs_idlefunc) {
            mt->idlefunc = mt_idle_add(mt);
          }
          mt_sensitise(mt);
        }
        lives_freep((void **)&esave_file);
        lives_freep((void **)&esave_dir);
        lives_freep((void **)&layout_map);
        lives_freep((void **)&layout_map_audio);
        return FALSE;
      }
    }
  } while (retval2 == LIVES_RESPONSE_RETRY);

  if (retval2 != LIVES_RESPONSE_CANCEL) {
    lives_snprintf(mainw->recent_file, PATH_MAX, "%s", xlayout_name);
    d_print(_("Saved layout to %s\n"), esave_file);
  }

  // save layout map
  save_layout_map(layout_map, layout_map_audio, esave_file, esave_dir);

  if (mt != NULL) mt->changed = FALSE;

  if (!ar_layout) {
    prefs->ar_layout = FALSE;
    set_string_pref(PREF_AR_LAYOUT, "");
    lives_memset(prefs->ar_layout_name, 0, 1);
  } else {
    prefs->ar_layout = TRUE;
    set_string_pref(PREF_AR_LAYOUT, layout_name);
    lives_snprintf(prefs->ar_layout_name, PATH_MAX, "%s", xlayout_name);
  }

  lives_freep((void **)&esave_file);
  lives_freep((void **)&esave_dir);
  lives_freep((void **)&layout_map);
  lives_freep((void **)&layout_map_audio);

  if (mainw->was_set) recover_layout_cancelled(FALSE);

  if (mt != NULL) {
    mt->auto_changed = FALSE;
    lives_widget_set_sensitive(mt->backup, FALSE);
    mt_sensitise(mt);
  }

  return TRUE;
}

// next functions are mainly to do with event_list manipulation

static char *rec_error_add(char *ebuf, char *msg, int num, weed_timecode_t tc) {
  // log an error generated during event_list rectification

  char *tmp;
  char *xnew;

  elist_errors++;

  threaded_dialog_spin(0.);
  if (tc == -1) xnew = lives_strdup(msg); // missing timecode
  else {
    if (num == -1) xnew = lives_strdup_printf("%s at timecode %"PRId64"\n", msg, tc);
    else xnew = lives_strdup_printf("%s %d at timecode %"PRId64"\n", msg, num, tc);
  }
  tmp = lives_strconcat(ebuf, xnew, NULL);
  //#define SILENT_EVENT_LIST_LOAD
#ifndef SILENT_EVENT_LIST_LOAD
  lives_printerr("Rec error: %s", xnew);
#endif
  lives_free(ebuf);
  lives_free(xnew);
  threaded_dialog_spin(0.);
  return tmp;
}


static int get_next_tt_key(ttable * trans_table) {
  int i;
  for (i = free_tt_key; i < FX_KEYS_MAX - FX_KEYS_MAX_VIRTUAL; i++) {
    if (trans_table[i].in == 0) return i;
  }
  return -1;
}


static void *find_init_event_in_ttable(ttable * trans_table, uint64_t in, boolean normal) {
  int i;
  for (i = 0; i < FX_KEYS_MAX - FX_KEYS_MAX_VIRTUAL; i++) {
    if (normal && trans_table[i].in == in) return trans_table[i].out;

    /// reverse lookup for past filter_map check
    if (!normal && (uint64_t)trans_table[i].out == in) return (void *)trans_table[i].in;
    if (trans_table[i].out == NULL) return NULL;
  }
  return NULL;
}


static void **remove_nulls_from_filter_map(void **init_events, int *num_events) {
  // remove NULLs from filter_map init_events

  // old array may be free()d, return value should be freed() unless NULL
  int num_nulls = 0, i, j = 0;
  void **new_init_events;

  if (*num_events == 1) return init_events;

  for (i = 0; i < *num_events; i++) if (init_events[i] == NULL) num_nulls++;
  if (num_nulls == 0) return init_events;

  *num_events -= num_nulls;

  if (*num_events == 0) new_init_events = NULL;

  else new_init_events = (void **)lives_malloc((*num_events) * sizeof(void *));

  for (i = 0; i < *num_events + num_nulls; i++) if (init_events[i] != NULL) new_init_events[j++] = init_events[i];

  lives_free(init_events);

  if (*num_events == 0) *num_events = 1;

  return new_init_events;
}


void move_init_in_filter_map(lives_mt * mt, weed_plant_t *event_list, weed_plant_t *event, weed_plant_t *ifrom,
                             weed_plant_t *ito, int track, boolean after) {
  int error, i, j;
  weed_plant_t *deinit_event = weed_get_plantptr_value(ifrom, WEED_LEAF_DEINIT_EVENT, &error);
  void **events_before = NULL;
  void **events_after = NULL;
  int num_before = 0, j1;
  int num_after = 0, j2;
  boolean got_after;
  void **init_events, **new_init_events;
  int num_inits;

  while (event != deinit_event) {
    if (!WEED_EVENT_IS_FILTER_MAP(event)) {
      event = get_next_event(event);
      continue;
    }
    init_events = weed_get_voidptr_array_counted(event, WEED_LEAF_INIT_EVENTS, &num_inits);
    if (events_before == NULL && events_after == NULL) {
      j = 0;
      for (i = 0; i < num_inits; i++) {
        if (init_events[i] == ifrom) continue;
        if (init_events[i] != ito && !init_event_is_relevant((weed_plant_t *)init_events[i], track)) continue;
        j++;
        if (init_events[i] == ito) {
          num_before = j - 1 + after;
          j = 1;
        }
      }
      num_after = j - after;
      if (num_before > 0) events_before = (void **)lives_malloc(num_before * sizeof(void *));
      if (num_after > 0) events_after = (void **)lives_malloc(num_after * sizeof(void *));
      j1 = j2 = 0;
      for (i = 0; i < num_inits; i++) {
        if (!init_event_is_relevant((weed_plant_t *)init_events[i], track)) continue;
        if (init_events[i] == ifrom) continue;
        if (j1 < num_before) {
          events_before[j1] = init_events[i];
          j1++;
        } else {
          events_after[j2] = init_events[i];
          j2++;
        }
      }
    }
    // check to see if we can move event without problem
    got_after = FALSE;
    for (i = 0; i < num_inits; i++) {
      if (init_events[i] == ifrom) continue;
      if (!init_event_is_relevant((weed_plant_t *)init_events[i], track)) continue;
      if (!got_after && init_event_in_list(events_after, num_after, (weed_plant_t *)init_events[i])) got_after = TRUE;
      if (got_after && init_event_in_list(events_before, num_before, (weed_plant_t *)init_events[i])) {
        lives_free(init_events);
        if (events_before != NULL) lives_free(events_before);
        if (events_after != NULL) lives_free(events_after);
        return; // order has changed, give up
      }
    }
    new_init_events = (void **)lives_malloc(num_inits * sizeof(void *));
    got_after = FALSE;
    j = 0;
    for (i = 0; i < num_inits; i++) {
      if (init_events[i] == ifrom) continue;
      if ((init_event_in_list(events_before, num_before, (weed_plant_t *)init_events[i]) ||
           !init_event_is_relevant((weed_plant_t *)init_events[i], track)) &&
          !init_event_is_process_last((weed_plant_t *)init_events[i]) && !init_event_is_process_last(ifrom))
        new_init_events[j] = init_events[i];
      else {
        if (!got_after) {
          got_after = TRUE;
          new_init_events[j] = ifrom;
          i--;
          j++;
          continue;
        }
        new_init_events[j] = init_events[i];
      }
      j++;
    }
    if (j < num_inits) new_init_events[j] = ifrom;
    weed_set_voidptr_array(event, WEED_LEAF_INIT_EVENTS, num_inits, new_init_events);
    lives_free(new_init_events);
    lives_free(init_events);
    event = get_next_event(event);
  }

  if (events_before != NULL) lives_free(events_before);
  if (events_after != NULL) lives_free(events_after);
}


boolean compare_filter_maps(weed_plant_t *fm1, weed_plant_t *fm2, int ctrack) {
  // return TRUE if the maps match exactly; if ctrack is !=LIVES_TRACK_ANY,
  // then we only compare filter maps where ctrack is an in_track or out_track
  int i1, i2, error, num_events1, num_events2;
  void **inits1, **inits2;

  if (!weed_plant_has_leaf(fm1, WEED_LEAF_INIT_EVENTS) && !weed_plant_has_leaf(fm2, WEED_LEAF_INIT_EVENTS)) return TRUE;
  if (ctrack == LIVES_TRACK_ANY && ((!weed_plant_has_leaf(fm1, WEED_LEAF_INIT_EVENTS) &&
                                     weed_get_voidptr_value(fm2, WEED_LEAF_INIT_EVENTS, &error) != NULL) ||
                                    (!weed_plant_has_leaf(fm2, WEED_LEAF_INIT_EVENTS) &&
                                     weed_get_voidptr_value(fm1, WEED_LEAF_INIT_EVENTS, &error) != NULL))) return FALSE;

  if (ctrack == LIVES_TRACK_ANY && (weed_plant_has_leaf(fm1, WEED_LEAF_INIT_EVENTS)) &&
      weed_plant_has_leaf(fm2, WEED_LEAF_INIT_EVENTS) &&
      ((weed_get_voidptr_value(fm1, WEED_LEAF_INIT_EVENTS, &error) == NULL &&
        weed_get_voidptr_value(fm2, WEED_LEAF_INIT_EVENTS, &error) != NULL) ||
       (weed_get_voidptr_value(fm1, WEED_LEAF_INIT_EVENTS, &error) != NULL &&
        weed_get_voidptr_value(fm2, WEED_LEAF_INIT_EVENTS, &error) == NULL))) return FALSE;

  num_events1 = weed_leaf_num_elements(fm1, WEED_LEAF_INIT_EVENTS);
  num_events2 = weed_leaf_num_elements(fm2, WEED_LEAF_INIT_EVENTS);
  if (ctrack == LIVES_TRACK_ANY && num_events1 != num_events2) return FALSE;

  inits1 = weed_get_voidptr_array(fm1, WEED_LEAF_INIT_EVENTS, &error);
  inits2 = weed_get_voidptr_array(fm2, WEED_LEAF_INIT_EVENTS, &error);

  if (inits1 == NULL && inits2 == NULL) return TRUE;

  i2 = 0;

  for (i1 = 0; i1 < num_events1; i1++) {

    if (i2 < num_events2 && init_event_is_process_last((weed_plant_t *)inits2[i2])) {
      // for process_last we don't care about the exact order
      if (init_event_in_list(inits1, num_events1, (weed_plant_t *)inits2[i2])) {
        i2++;
        i1--;
        continue;
      }
    }

    if (init_event_is_process_last((weed_plant_t *)inits1[i1])) {
      // for process_last we don't care about the exact order
      if (init_event_in_list(inits2, num_events2, (weed_plant_t *)inits1[i1])) {
        continue;
      }
    }

    if (ctrack != LIVES_TRACK_ANY) {
      if (inits1[i1] != NULL) {
        if (init_event_is_relevant((weed_plant_t *)inits1[i1], ctrack)) {
          if (i2 >= num_events2) {
            lives_free(inits1);
            lives_free(inits2);
            return FALSE;
          }
        } else continue; // skip this one, it doesn't involve ctrack
      } else continue; // skip NULLS
    }

    if (i2 < num_events2) {
      if (ctrack != LIVES_TRACK_ANY) {
        if (inits2[i2] == NULL || !init_event_is_relevant((weed_plant_t *)inits2[i2], ctrack)) {
          i2++;
          i1--;
          continue; // skip this one, it doesn't involve ctrack
        }
      }

      if (inits1[i1] != inits2[i2]) {
        lives_free(inits1);
        lives_free(inits2);
        return FALSE;
      }
      i2++;
    }
  }

  if (i2 < num_events2) {
    if (ctrack == LIVES_TRACK_ANY) {
      lives_free(inits1);
      return FALSE;
    }
    for (; i2 < num_events2; i2++) {
      if (inits2[i2] != NULL) {
        if (init_event_is_process_last((weed_plant_t *)inits2[i2])) {
          // for process_last we don't care about the exact order
          if (init_event_in_list(inits1, num_events1, (weed_plant_t *)inits2[i2])) continue;
        }

        if (init_event_is_relevant((weed_plant_t *)inits2[i2], ctrack)) {
          lives_free(inits1);
          lives_free(inits2);
          return FALSE;
        }
      }
    }
  }
  if (inits1 != NULL) lives_free(inits1);
  if (inits2 != NULL) lives_free(inits2);
  return TRUE;
}


static char *filter_map_check(ttable * trans_table, weed_plant_t *filter_map, weed_timecode_t deinit_tc,
                              weed_timecode_t fm_tc, char *ebuf) {
  int num_init_events;
  void **copy_events, **pinit_events;
  int error, i;
  uint64_t *init_events;

  if (!weed_plant_has_leaf(filter_map, WEED_LEAF_INIT_EVENTS)) return ebuf;
  // check no deinited events are active
  num_init_events = weed_leaf_num_elements(filter_map, WEED_LEAF_INIT_EVENTS);

  if (weed_leaf_seed_type(filter_map, WEED_LEAF_INIT_EVENTS) == WEED_SEED_INT64) {
    if (num_init_events == 1 && weed_get_int64_value(filter_map, WEED_LEAF_INIT_EVENTS, &error) == 0) return ebuf;
    init_events = (uint64_t *)(weed_get_int64_array(filter_map, WEED_LEAF_INIT_EVENTS, &error));
  } else {
    if (num_init_events == 1 && weed_get_voidptr_value(filter_map, WEED_LEAF_INIT_EVENTS, &error) == NULL) return ebuf;
    pinit_events = weed_get_voidptr_array(filter_map, WEED_LEAF_INIT_EVENTS, &error);
    init_events = (uint64_t *)lives_malloc(num_init_events * sizeof(uint64_t));
    for (i = 0; i < num_init_events; i++) init_events[i] = (uint64_t)pinit_events[i];
    lives_free(pinit_events);
  }

  copy_events = (void **)lives_malloc(num_init_events * sizeof(weed_plant_t *));
  for (i = 0; i < num_init_events; i++) {
    if (find_init_event_in_ttable(trans_table, init_events[i], FALSE) != NULL) copy_events[i] = (void *)init_events[i]; // !!
    else {
      copy_events[i] = NULL;
      ebuf = rec_error_add(ebuf, "Filter_map points to invalid filter_init", -1, fm_tc);
    }
  }
  if (num_init_events > 1) copy_events = remove_nulls_from_filter_map(copy_events, &num_init_events);

  if (copy_events != NULL) lives_free(copy_events);
  lives_free(init_events);
  return ebuf;
}


static char *add_filter_deinits(weed_plant_t *event_list, ttable * trans_table, void ***pchains,
                                weed_timecode_t tc, char *ebuf) {
  // add filter deinit events for any remaining active filters
  int i, j, error, num_params;
  char *filter_hash;
  int idx;
  weed_plant_t *init_event, *event;
  void **in_pchanges;

  for (i = 0; i < FX_KEYS_MAX - FX_KEYS_MAX_VIRTUAL; i++) {
    if (trans_table[i].out == NULL) continue;
    if (trans_table[i].in != 0) {
      event_list = append_filter_deinit_event(event_list, tc, (init_event = (weed_plant_t *)trans_table[i].out), pchains[i]);
      event = get_last_event(event_list);

      filter_hash = weed_get_string_value(init_event, WEED_LEAF_FILTER, &error);
      if ((idx = weed_get_idx_for_hashname(filter_hash, TRUE)) != -1) {
        if ((num_params = weed_leaf_num_elements(init_event, WEED_LEAF_IN_PARAMETERS)) > 0) {
          in_pchanges = (void **)lives_malloc(num_params * sizeof(void *));
          for (j = 0; j < num_params; j++) {
            if (!WEED_EVENT_IS_FILTER_INIT((weed_plant_t *)pchains[i][j]))
              in_pchanges[j] = (weed_plant_t *)pchains[i][j];
            else in_pchanges[j] = NULL;
          }
          weed_set_voidptr_array(event, WEED_LEAF_IN_PARAMETERS, num_params, in_pchanges); // set array to last param_changes
          lives_free(in_pchanges);
          lives_free(pchains[i]);
        }
      }
      lives_free(filter_hash);
      ebuf = rec_error_add(ebuf, "Added missing filter_deinit", -1, tc);
    }
  }
  return ebuf;
}


static char *add_null_filter_map(weed_plant_t *event_list, weed_plant_t *last_fm, weed_timecode_t tc, char *ebuf) {
  int num_events;
  int error;

  if (!weed_plant_has_leaf(last_fm, WEED_LEAF_INIT_EVENTS)) return ebuf;

  num_events = weed_leaf_num_elements(last_fm, WEED_LEAF_INIT_EVENTS);
  if (num_events == 1 && weed_get_voidptr_value(last_fm, WEED_LEAF_INIT_EVENTS, &error) == NULL) return ebuf;

  event_list = append_filter_map_event(event_list, tc, NULL);

  ebuf = rec_error_add(ebuf, "Added missing empty filter_map", -1, tc);
  return ebuf;
}


static weed_plant_t *duplicate_frame_at(weed_plant_t *event_list, weed_plant_t *src_frame, weed_timecode_t tc) {
  // tc should be > src_frame tc : i.e. copy is forward in time because insert_frame_event_at searches forward
  int *clips;
  int64_t *frames;
  int numframes;

  clips = weed_get_int_array_counted(src_frame, WEED_LEAF_CLIPS, &numframes);
  if (!numframes) return src_frame;

  frames = weed_get_int64_array(src_frame, WEED_LEAF_FRAMES, NULL);

  event_list = insert_frame_event_at(event_list, tc, numframes, clips, frames, &src_frame);

  lives_free(clips);
  lives_free(frames);
  return get_frame_event_at(event_list, tc, src_frame, TRUE);
}


static LiVESList *atrack_list;

static void add_atrack_to_list(int track, int clip) {
  // keep record of audio tracks so we can add closures if missing
  LiVESList *alist = atrack_list;
  char *entry;
  char **array;

  while (alist != NULL) {
    entry = (char *)alist->data;
    array = lives_strsplit(entry, "|", -1);
    if (atoi(array[0]) == track) {
      lives_free((livespointer)alist->data);
      alist->data = lives_strdup_printf("%d|%d", track, clip);
      lives_strfreev(array);
      return;
    }
    lives_strfreev(array);
    alist = alist->next;
  }
  atrack_list = lives_list_append(atrack_list, lives_strdup_printf("%d|%d", track, clip));
}


static void remove_atrack_from_list(int track) {
  // keep record of audio tracks so we can add closures if missing
  LiVESList *alist = atrack_list, *alist_next;
  char *entry;
  char **array;

  while (alist != NULL) {
    alist_next = alist->next;
    entry = (char *)alist->data;
    array = lives_strsplit(entry, "|", -1);
    if (atoi(array[0]) == track) {
      atrack_list = lives_list_remove(atrack_list, entry);
      lives_strfreev(array);
      lives_free(entry);
      return;
    }
    lives_strfreev(array);
    alist = alist_next;
  }
}


static char *add_missing_atrack_closers(weed_plant_t *event_list, double fps, char *ebuf) {
  LiVESList *alist = atrack_list;
  char *entry;
  char **array;
  int i = 0;

  int *aclips;
  double *aseeks;

  weed_plant_t *last_frame;
  int num_atracks;
  weed_timecode_t tc;

  if (atrack_list == NULL) return ebuf;

  num_atracks = lives_list_length(atrack_list) * 2;

  aclips = (int *)lives_malloc(num_atracks * sizint);
  aseeks = (double *)lives_malloc(num_atracks * sizdbl);

  last_frame = get_last_frame_event(event_list);
  tc = get_event_timecode(last_frame);

  if (!is_blank_frame(last_frame, TRUE)) {
    weed_plant_t *shortcut = last_frame;
    event_list = insert_blank_frame_event_at(event_list, q_gint64(tc + 1. / TICKS_PER_SECOND_DBL, fps), &shortcut);
  }

  while (alist != NULL) {
    entry = (char *)alist->data;
    array = lives_strsplit(entry, "|", -1);
    aclips[i] = atoi(array[0]);
    aclips[i + 1] = atoi(array[1]);
    aseeks[i] = 0.;
    aseeks[i + 1] = 0.;
    lives_strfreev(array);
    if (aclips[i] >= 0) ebuf = rec_error_add(ebuf, "Added missing audio closure", aclips[i], tc);
    else ebuf = rec_error_add(ebuf, "Added missing audio closure to backing track", -aclips[i], tc);
    i += 2;
    alist = alist->next;
  }

  weed_set_int_array(last_frame, WEED_LEAF_AUDIO_CLIPS, num_atracks, aclips);
  weed_set_double_array(last_frame, WEED_LEAF_AUDIO_SEEKS, num_atracks, aseeks);

  lives_free(aclips);
  lives_free(aseeks);

  lives_list_free_all(&atrack_list);

  return ebuf;
}


static int64_t *get_int64_array_convert(weed_plant_t *plant, const char *key) {
  if (weed_leaf_seed_type(plant, key) == WEED_SEED_INT) {
    int nvals;
    int *ivals = weed_get_int_array_counted(plant, key, &nvals);
    if (!nvals) return NULL;
    int64_t *i64vals = lives_calloc(nvals, 8);
    for (int i = 0; i < nvals; i++) i64vals[i] = (int64_t)ivals[i];
    lives_free(ivals);
    weed_leaf_delete(plant, key);
    weed_set_int64_array(plant, key, nvals, i64vals);
    return i64vals;
  }
  return weed_get_int64_array(plant, key, NULL);
}


boolean event_list_rectify(lives_mt * mt, weed_plant_t *event_list) {
  // check and reassemble a newly loaded event_list
  // reassemply consists of matching init_event(s) to event_id's
  // we also rebuild our param_change chains (WEED_LEAF_IN_PARAMETERS in filter_init and filter_deinit,
  // and WEED_LEAF_NEXT_CHANGE and WEED_LEAF_PREV_CHANGE
  // in other param_change events)

  // The checking done is quite sophisticated, and can correct many errors in badly-formed event_lists

  weed_plant_t **ptmpls;
  weed_plant_t **ctmpls;

  weed_plant_t *event = get_first_event(event_list), *event_next;
  weed_plant_t *shortcut = NULL;
  weed_plant_t *last_frame_event;
  weed_plant_t *last_filter_map = NULL;
  weed_plant_t *filter = NULL;
  weed_plant_t *last_event;

  weed_timecode_t tc = 0, last_tc = 0;
  weed_timecode_t last_frame_tc = -1;
  weed_timecode_t last_deinit_tc = -1;
  weed_timecode_t last_filter_map_tc = -1;
  weed_timecode_t cur_tc = 0;

  char *ebuf = lives_strdup("");
  char *host_tag_s;
  char *filter_hash;
  char *bit1 = lives_strdup(""), *bit2 = NULL, *bit3 = lives_strdup("."), *msg;
  int64_t *frame_index, *new_frame_index;
  int *inct, *outct;
  int *clip_index, *aclip_index, *new_aclip_index;
  int *new_clip_index;
  weed_error_t error;

  int hint;
  int i, idx, filter_idx, j;
  int host_tag;
  int num_ctmpls, num_inct, num_outct;
  int pnum, thisct;
  int num_init_events;
  int num_params;
  int num_tracks, num_atracks;
  int last_valid_frame;
  int marker_type;
  int ev_count = 0;

  boolean check_filter_map = FALSE;
  boolean was_deleted = FALSE;
  boolean was_moved;
  boolean missing_clips = FALSE, missing_frames = FALSE;

  void *init_event;
  void **new_init_events;
  void **in_pchanges;
  void **pchains[FX_KEYS_MAX - FX_KEYS_MAX_VIRTUAL]; // parameter chains

  double fps = 0.;
  double *aseek_index, *new_aseek_index;

  LiVESWidget *transient;

  uint64_t event_id;

  uint64_t *init_events;

  ttable trans_table[FX_KEYS_MAX - FX_KEYS_MAX_VIRTUAL]; // translation table for init_events

  if (weed_plant_has_leaf(event_list, WEED_LEAF_FPS)) fps = weed_get_double_value(event_list, WEED_LEAF_FPS, &error);

  if (mt != NULL) mt->layout_prompt = FALSE;

  for (i = 0; i < FX_KEYS_MAX - FX_KEYS_MAX_VIRTUAL; i++) {
    trans_table[i].in = 0;
    trans_table[i].out = NULL;
  }

  free_tt_key = 0;

  atrack_list = NULL;

  while (event != NULL) {
    was_deleted = FALSE;
    event_next = get_next_event(event);
    if (!weed_plant_has_leaf(event, WEED_LEAF_TIMECODE)) {
      ebuf = rec_error_add(ebuf, "Event has no timecode", weed_get_plant_type(event), -1);
      delete_event(event_list, event);
      event = event_next;
      continue;
    }
    tc = get_event_timecode(event);
    if (fps != 0.) {
      tc = q_gint64(tc + TICKS_PER_SECOND_DBL / (2. * fps) - 1, fps);
      weed_set_int64_value(event, WEED_LEAF_TIMECODE, tc);
    }
    ev_count++;
    lives_snprintf(mainw->msg, MAINW_MSG_SIZE, "%d|", ev_count);
    if ((ev_count % 100) == 0) threaded_dialog_spin(0.);

    if (weed_get_plant_type(event) != WEED_PLANT_EVENT) {
      ebuf = rec_error_add(ebuf, "Invalid plant type", weed_get_plant_type(event), tc);
      delete_event(event_list, event);
      event = event_next;
      continue;
    }
    if (tc < last_tc) {
      break_me();
      ebuf = rec_error_add(ebuf, "Out of order event", -1, tc);
      delete_event(event_list, event);
      event = event_next;
      continue;
    }
    if (!weed_plant_has_leaf(event, WEED_LEAF_HINT)) {
      ebuf = rec_error_add(ebuf, "Event has no hint", weed_get_plant_type(event), tc);
      delete_event(event_list, event);
      event = event_next;
      continue;
    }

    hint = get_event_hint(event);

    switch (hint) {
    case WEED_EVENT_HINT_FILTER_INIT:
#ifdef DEBUG_TTABLE
      g_print("\n\ngot filter init %p\n", event);
#endif
      // set in table
      if (!weed_plant_has_leaf(event, WEED_LEAF_EVENT_ID)) {
        ebuf = rec_error_add(ebuf, "Filter_init missing event_id", -1, tc);
        /* delete_event(event_list, event); */
        /* was_deleted = TRUE; */
        weed_set_int64_value(event, WEED_LEAF_EVENT_ID, (uint64_t)((void *)event));
      }

      if (1) {
        if (!weed_plant_has_leaf(event, WEED_LEAF_FILTER)) {
          ebuf = rec_error_add(ebuf, "Filter_init missing filter", -1, tc);
          delete_event(event_list, event);
          was_deleted = TRUE;
        } else {
          filter_hash = weed_get_string_value(event, WEED_LEAF_FILTER, &error);
          if ((filter_idx = weed_get_idx_for_hashname(filter_hash, TRUE)) != -1) {
            filter = get_weed_filter(filter_idx);
            if (weed_plant_has_leaf(filter, WEED_LEAF_IN_CHANNEL_TEMPLATES)) {
              if (!weed_plant_has_leaf(event, WEED_LEAF_IN_COUNT)) {
                ebuf = rec_error_add(ebuf, "Filter_init missing filter", -1, tc);
                delete_event(event_list, event);
                was_deleted = TRUE;
              } else {
                num_ctmpls = weed_leaf_num_elements(filter, WEED_LEAF_IN_CHANNEL_TEMPLATES);
                num_inct = weed_leaf_num_elements(event, WEED_LEAF_IN_COUNT);
                if (num_ctmpls != num_inct) {
                  ebuf = rec_error_add(ebuf, "Filter_init has invalid in_count", -1, tc);
                  delete_event(event_list, event);
                  was_deleted = TRUE;
                } else {
                  inct = weed_get_int_array(event, WEED_LEAF_IN_COUNT, &error);
                  ctmpls = weed_get_plantptr_array(filter, WEED_LEAF_IN_CHANNEL_TEMPLATES, &error);
                  for (i = 0; i < num_ctmpls; i++) {
                    thisct = inct[i];
                    if (thisct == 0 && !weed_chantmpl_is_optional(ctmpls[i])) {
                      ebuf = rec_error_add(ebuf, "Filter_init disables a non-optional in channel", i, tc);
                      delete_event(event_list, event);
                      was_deleted = TRUE;
                    } else {
                      if (thisct > 1 && (!weed_plant_has_leaf(ctmpls[i], WEED_LEAF_MAX_REPEATS) ||
                                         (weed_get_int_value(ctmpls[i], WEED_LEAF_MAX_REPEATS, &error) > 0 &&
                                          weed_get_int_value(ctmpls[i], WEED_LEAF_MAX_REPEATS, &error) < thisct))) {
                        ebuf = rec_error_add(ebuf, "Filter_init has too many repeats of in channel", i, tc);
                        delete_event(event_list, event);
                        was_deleted = TRUE;
                      }
                    }
                  }

                  lives_free(inct);
                  lives_free(ctmpls);

                  if (!was_deleted) {
                    num_ctmpls = weed_leaf_num_elements(filter, WEED_LEAF_OUT_CHANNEL_TEMPLATES);
                    num_outct = weed_leaf_num_elements(event, WEED_LEAF_OUT_COUNT);
                    if (num_ctmpls != num_outct) {
                      ebuf = rec_error_add(ebuf, "Filter_init has invalid out_count", -1, tc);
                      delete_event(event_list, event);
                      was_deleted = TRUE;
                    } else {
                      outct = weed_get_int_array(event, WEED_LEAF_OUT_COUNT, &error);
                      ctmpls = weed_get_plantptr_array(filter, WEED_LEAF_OUT_CHANNEL_TEMPLATES, &error);
                      for (i = 0; i < num_ctmpls; i++) {
                        thisct = outct[i];
                        if (thisct == 0 && !weed_chantmpl_is_optional(ctmpls[i])) {
                          ebuf = rec_error_add(ebuf, "Filter_init disables a non-optional out channel", i, tc);
                          delete_event(event_list, event);
                          was_deleted = TRUE;
                        } else {
                          if (thisct > 1 && (!weed_plant_has_leaf(ctmpls[i], WEED_LEAF_MAX_REPEATS) ||
                                             (weed_get_int_value(ctmpls[i], WEED_LEAF_MAX_REPEATS, &error) > 0 &&
                                              weed_get_int_value(ctmpls[i], WEED_LEAF_MAX_REPEATS, &error) < thisct))) {
                            ebuf = rec_error_add(ebuf, "Filter_init has too many repeats of out channel", i, tc);
                            delete_event(event_list, event);
                            was_deleted = TRUE;
                          } else {
                            if (weed_plant_has_leaf(event, WEED_LEAF_IN_TRACKS)) {
                              int ntracks;
                              int *trax = weed_get_int_array_counted(event, WEED_LEAF_IN_TRACKS, &ntracks);
                              for (i = 0; i < ntracks; i++) {
                                if (trax[i] >= 0 && !has_video_chans_in(filter, FALSE)) {
                                  // TODO ** inform user
                                  if (mt != NULL && !mt->opts.pertrack_audio) {
                                    lives_widget_set_sensitive(mt->fx_region_2a, TRUE);
                                    mt->opts.pertrack_audio = TRUE;
                                  } else force_pertrack_audio = TRUE;
                                }

                                if (trax[i] == -1) {
                                  // TODO ** inform user
                                  if (mt != NULL && mt->opts.back_audio_tracks == 0) {
                                    mt->opts.back_audio_tracks = 1;
                                    ebuf = rec_error_add(ebuf, "Adding backing audio", -1, tc);
                                  } else force_backing_tracks = 1;
                                }
                              }

                              lives_free(trax);
                              trax = weed_get_int_array_counted(event, WEED_LEAF_OUT_TRACKS, &ntracks);
                              for (i = 0; i < ntracks; i++) {
                                if (trax[i] >= 0 && !has_video_chans_out(filter, FALSE)) {
                                  // TODO ** inform user
                                  if (mt != NULL && !mt->opts.pertrack_audio) {
                                    lives_widget_set_sensitive(mt->fx_region_2a, TRUE);
                                    mt->opts.pertrack_audio = TRUE;
                                  } else force_pertrack_audio = TRUE;
                                }
                                if (trax[i] == -1) {
                                  // TODO ** inform user
                                  if (mt != NULL && mt->opts.back_audio_tracks == 0) {
                                    mt->opts.back_audio_tracks = 1;
                                  } else force_backing_tracks = 1;
                                }
                              }
                              lives_free(trax);
                            }
                          }
                          // all tests passed
                          if (tc == 0) {
                            if (mt != NULL && mt->avol_fx == -1) {
                              // check if it can be a filter delegate
                              LiVESList *clist = mainw->fx_candidates[FX_CANDIDATE_AUDIO_VOL].list;
                              while (clist != NULL) {
                                if (LIVES_POINTER_TO_INT(clist->data) == filter_idx) {
                                  mt->avol_fx = filter_idx;
                                  mt->avol_init_event = event;
                                  break;
                                }
                                clist = clist->next;
				// *INDENT-OFF*
                              }}}}}
		      // *INDENT-ON*
                      lives_free(outct);
                      lives_free(ctmpls);
		    // *INDENT-OFF*
                    }}}}}
	  // *INDENT-ON*
          } else {
            lives_printerr("Layout contains unknown filter %s\n", filter_hash);
            ebuf = rec_error_add(ebuf, "Layout contains unknown filter", -1, tc);
            delete_event(event_list, event);
            was_deleted = TRUE;
            if (mt != NULL) mt->layout_prompt = TRUE;
          }
          lives_free(filter_hash);
          if (!was_deleted) {
            host_tag = get_next_tt_key(trans_table) + FX_KEYS_MAX_VIRTUAL + 1;
            if (host_tag == -1) {
              ebuf = rec_error_add(ebuf, "Fatal: too many active effects", FX_KEYS_MAX - FX_KEYS_MAX_VIRTUAL, tc);
              end_threaded_dialog();
              return FALSE;
            }
            host_tag_s = lives_strdup_printf("%d", host_tag);
            weed_set_string_value(event, WEED_LEAF_HOST_TAG, host_tag_s);
            lives_free(host_tag_s);

            if (weed_leaf_seed_type(event, WEED_LEAF_EVENT_ID) == WEED_SEED_INT64)
              event_id = (uint64_t)(weed_get_int64_value(event, WEED_LEAF_EVENT_ID, &error));
            else
              event_id = (uint64_t)((weed_plant_t *)weed_get_voidptr_value(event, WEED_LEAF_EVENT_ID, &error));

            trans_table[(idx = host_tag - FX_KEYS_MAX_VIRTUAL - 1)].in = event_id;
            trans_table[idx].out = event;
#ifdef DEBUG_TTABLE
            g_print("adding lookup %"PRIu64" -> %p\n", event_id, event);
#endif

            // use pchain array
            if (weed_plant_has_leaf(event, WEED_LEAF_IN_PARAMETERS)) {
              num_params = weed_leaf_num_elements(event, WEED_LEAF_IN_PARAMETERS);
              pchains[idx] = (void **)lives_malloc((num_params + 1) * sizeof(void *));
              in_pchanges = (void **)lives_malloc(num_params * sizeof(void *));
              for (i = 0; i < num_params; i++) {
                pchains[idx][i] = event;
                in_pchanges[i] = NULL;
              }
              pchains[idx][i] = NULL;
              // set all to NULL, we will re-fill as we go along
              weed_leaf_delete(event, WEED_LEAF_IN_PARAMETERS);
              weed_set_voidptr_array(event, WEED_LEAF_IN_PARAMETERS, num_params, in_pchanges);
              lives_free(in_pchanges);
	      // *INDENT-OFF*
            }}}}
	  // *INDENT-ON*

      break;
    case WEED_EVENT_HINT_FILTER_DEINIT:

      // update "init_event" from table, remove entry; we will check filter_map at end of tc
      if (!weed_plant_has_leaf(event, WEED_LEAF_INIT_EVENT)) {
        ebuf = rec_error_add(ebuf, "Filter_deinit missing init_event", -1, tc);
        delete_event(event_list, event);
        was_deleted = TRUE;
      } else {
        if (weed_leaf_seed_type(event, WEED_LEAF_INIT_EVENT) == WEED_SEED_INT64)
          event_id = (uint64_t)(weed_get_int64_value(event, WEED_LEAF_INIT_EVENT, &error));
        else
          event_id = (uint64_t)((weed_plant_t *)weed_get_voidptr_value(event, WEED_LEAF_INIT_EVENT, &error));

#ifdef DEBUG_TTABLE
        g_print("looking for %"PRIu64" in ttable\n", event_id);
#endif
        init_event = find_init_event_in_ttable(trans_table, event_id, TRUE);

        if (init_event == NULL) {
          ebuf = rec_error_add(ebuf, "Filter_deinit has invalid init_event", -1, tc);
          delete_event(event_list, event);
          was_deleted = TRUE;
        } else {
          weed_leaf_delete((weed_plant_t *)init_event, WEED_LEAF_DEINIT_EVENT);
          weed_set_plantptr_value((weed_plant_t *)init_event, WEED_LEAF_DEINIT_EVENT, event);

          host_tag_s = weed_get_string_value((weed_plant_t *)init_event, WEED_LEAF_HOST_TAG, &error);
          host_tag = atoi(host_tag_s);
          lives_free(host_tag_s);
          trans_table[(idx = host_tag - FX_KEYS_MAX_VIRTUAL - 1)].in = 0;
          if (idx < free_tt_key) free_tt_key = idx;
          weed_leaf_delete(event, WEED_LEAF_INIT_EVENT);
          weed_set_voidptr_value(event, WEED_LEAF_INIT_EVENT, init_event);
          check_filter_map = TRUE;
          last_deinit_tc = tc;

          filter_hash = weed_get_string_value((weed_plant_t *)init_event, WEED_LEAF_FILTER, &error);
          if ((filter_idx = weed_get_idx_for_hashname(filter_hash, TRUE)) != -1) {
            if ((num_params = weed_leaf_num_elements((weed_plant_t *)init_event, WEED_LEAF_IN_PARAMETERS)) > 0) {
              in_pchanges = (void **)lives_malloc(num_params * sizeof(void *));
              for (i = 0; i < num_params; i++) {
                if (!WEED_EVENT_IS_FILTER_INIT((weed_plant_t *)pchains[idx][i]))
                  in_pchanges[i] = (weed_plant_t *)pchains[idx][i];
                else in_pchanges[i] = NULL;
              }
              weed_leaf_delete(event, WEED_LEAF_IN_PARAMETERS);
              weed_set_voidptr_array(event, WEED_LEAF_IN_PARAMETERS, num_params, in_pchanges); // set array to last param_changes
              lives_free(in_pchanges);
              lives_free(pchains[idx]);
            }
          }
          lives_free(filter_hash);
        }
      }
      break;
    case WEED_EVENT_HINT_FILTER_MAP:
      // update "init_events" from table
      if (weed_plant_has_leaf(event, WEED_LEAF_INIT_EVENTS)) {
        num_init_events = weed_leaf_num_elements(event, WEED_LEAF_INIT_EVENTS);
        if (weed_leaf_seed_type(event, WEED_LEAF_INIT_EVENTS) == WEED_SEED_INT64)
          init_events = (uint64_t *)weed_get_int64_array(event, WEED_LEAF_INIT_EVENTS, &error);
        else  {
          void **pinit_events = weed_get_voidptr_array(event, WEED_LEAF_INIT_EVENTS, &error);
          init_events = (uint64_t *)lives_malloc(num_init_events * sizeof(uint64_t));
          for (i = 0; i < num_init_events; i++) init_events[i] = (uint64_t)pinit_events[i];
          lives_free(pinit_events);
        }

        new_init_events = (void **)lives_malloc(num_init_events * sizeof(void *));
        for (i = 0; i < num_init_events; i++) {
          event_id = (uint64_t)init_events[i];
          if (event_id != 0) {

            init_event = find_init_event_in_ttable(trans_table, event_id, TRUE);
#ifdef DEBUG_TTABLE
            g_print("looking for %"PRIu64" in ttable, got %p\n", event_id, init_event);
#endif
            if (init_event == NULL) {
              ebuf = rec_error_add(ebuf, "Filter_map has invalid init_event", -1, tc);
              new_init_events[i] = NULL;
            } else new_init_events[i] = init_event;
          } else new_init_events[i] = NULL;
        }
        new_init_events = remove_nulls_from_filter_map(new_init_events, &num_init_events);

        weed_leaf_delete(event, WEED_LEAF_INIT_EVENTS);

        if (new_init_events == NULL) weed_set_voidptr_value(event, WEED_LEAF_INIT_EVENTS, NULL);
        else {
          weed_set_voidptr_array(event, WEED_LEAF_INIT_EVENTS, num_init_events, new_init_events);

          for (i = 0; i < num_init_events; i++) {
            if (init_event_is_process_last((weed_plant_t *)new_init_events[i])) {
              // reposition process_last events to the end
              add_init_event_to_filter_map(event, (weed_plant_t *)new_init_events[i], NULL);
            }
          }
          lives_free(new_init_events);
        }
        lives_free(init_events);
      } else {
        weed_set_voidptr_value(event, WEED_LEAF_INIT_EVENTS, NULL);
      }
      if (last_filter_map != NULL) {
        if (compare_filter_maps(last_filter_map, event, LIVES_TRACK_ANY)) {
          // filter map is identical to prior one, we can remove this one
          delete_event(event_list, event);
          was_deleted = TRUE;
        }
      } else if (weed_leaf_num_elements(event, WEED_LEAF_INIT_EVENTS) == 1 &&
                 weed_get_voidptr_value(event, WEED_LEAF_INIT_EVENTS, &error) == NULL) {
        delete_event(event_list, event);
        was_deleted = TRUE;
      }
      if (!was_deleted) last_filter_map = event;

      break;
    case WEED_EVENT_HINT_PARAM_CHANGE:
      if (!weed_plant_has_leaf(event, WEED_LEAF_INDEX)) {
        ebuf = rec_error_add(ebuf, "Param_change has no index", -1, tc);
        delete_event(event_list, event);
        was_deleted = TRUE;
      } else {
        if (!weed_plant_has_leaf(event, WEED_LEAF_VALUE)) {
          ebuf = rec_error_add(ebuf, "Param_change has no value", -1, tc);
          delete_event(event_list, event);
          was_deleted = TRUE;
        } else {
          if (!weed_plant_has_leaf(event, WEED_LEAF_INIT_EVENT)) {
            ebuf = rec_error_add(ebuf, "Param_change has no init_event", -1, tc);
            delete_event(event_list, event);
            was_deleted = TRUE;
          } else {
            if (weed_leaf_seed_type(event, WEED_LEAF_INIT_EVENT) == WEED_SEED_INT64)
              event_id = (uint64_t)(weed_get_int64_value(event, WEED_LEAF_INIT_EVENT, &error));
            else
              event_id = (uint64_t)((weed_plant_t *)weed_get_voidptr_value(event, WEED_LEAF_INIT_EVENT, &error));

#ifdef DEBUG_TTABLE
            g_print("pc looking for %"PRIu64" in ttable %d\n", event_id, error);
#endif

            if ((init_event = find_init_event_in_ttable(trans_table, event_id, TRUE)) == NULL) {
              ebuf = rec_error_add(ebuf, "Param_change has invalid init_event", -1, tc);
              delete_event(event_list, event);
              was_deleted = TRUE;
            } else {
              filter_hash = weed_get_string_value((weed_plant_t *)init_event, WEED_LEAF_FILTER, &error);
              if ((filter_idx = weed_get_idx_for_hashname(filter_hash, TRUE)) != -1) {
                filter = get_weed_filter(filter_idx);
                pnum = weed_get_int_value(event, WEED_LEAF_INDEX, &error);
                if (pnum < 0 || pnum >= num_in_params(filter, FALSE, FALSE) ||
                    pnum >= (num_params = weed_leaf_num_elements((weed_plant_t *)init_event, WEED_LEAF_IN_PARAMETERS))) {
                  ebuf = rec_error_add(ebuf, "Param_change has invalid index", pnum, tc);
                  delete_event(event_list, event);
                  was_deleted = TRUE;
                } else {
                  ptmpls = weed_get_plantptr_array(filter, WEED_LEAF_IN_PARAMETER_TEMPLATES, &error);
                  if (!weed_plant_has_leaf(event, WEED_LEAF_VALUE)) {
                    ebuf = rec_error_add(ebuf, "Param_change has no value with index", pnum, tc);
                    delete_event(event_list, event);
                    was_deleted = TRUE;
                  } else {
                    if (weed_leaf_seed_type(event, WEED_LEAF_VALUE) != weed_leaf_seed_type(ptmpls[pnum], WEED_LEAF_DEFAULT)) {
                      ebuf = rec_error_add(ebuf, "Param_change has invalid seed type with index", pnum, tc);
                      delete_event(event_list, event);
                      was_deleted = TRUE;
                    } else {
                      // all checks passed
                      host_tag_s = weed_get_string_value((weed_plant_t *)init_event, WEED_LEAF_HOST_TAG, &error);
                      host_tag = atoi(host_tag_s);
                      lives_free(host_tag_s);
                      idx = host_tag - FX_KEYS_MAX_VIRTUAL - 1;

                      if (pchains[idx][pnum] == init_event) {
                        if (weed_leaf_seed_type((weed_plant_t *)init_event, WEED_LEAF_IN_PARAMETERS) == WEED_SEED_INT64) {
                          // leave as int64_t and we will change afterwards
                          uint64_t *orig_pchanges = (uint64_t *)weed_get_int64_array((weed_plant_t *)init_event,
                                                    WEED_LEAF_IN_PARAMETERS, &error);

                          uint64_t *pin_pchanges = (uint64_t *)lives_malloc(num_params * sizeof(uint64_t));

                          for (i = 0; i < num_params; i++) {
                            if (orig_pchanges[i] == 0 && i == pnum) pin_pchanges[i] = (uint64_t)event;
                            else pin_pchanges[i] = (uint64_t)orig_pchanges[i];
                          }

                          weed_leaf_delete((weed_plant_t *)init_event, WEED_LEAF_IN_PARAMETERS);
                          weed_set_int64_array((weed_plant_t *)init_event, WEED_LEAF_IN_PARAMETERS, num_params,
                                               (int64_t *)pin_pchanges);

                          lives_free(pin_pchanges);
                          lives_free(orig_pchanges);
                        } else {
                          void **orig_pchanges = weed_get_voidptr_array((weed_plant_t *)init_event,
                                                 WEED_LEAF_IN_PARAMETERS, &error);
                          void **pin_pchanges = (void **)lives_malloc(num_params * sizeof(void *));

                          for (i = 0; i < num_params; i++) {
                            if (orig_pchanges[i] == NULL && i == pnum) pin_pchanges[i] = (void *)event;
                            else pin_pchanges[i] = (void *)orig_pchanges[i];
                          }
                          weed_leaf_delete((weed_plant_t *)init_event, WEED_LEAF_IN_PARAMETERS);
                          weed_set_voidptr_array((weed_plant_t *)init_event, WEED_LEAF_IN_PARAMETERS, num_params, pin_pchanges);

                          lives_free(pin_pchanges);
                          lives_free(orig_pchanges);
                        }
                        weed_leaf_delete(event, WEED_LEAF_PREV_CHANGE);
                        weed_set_voidptr_value(event, WEED_LEAF_PREV_CHANGE, NULL);
                      } else {
                        weed_leaf_delete(event, WEED_LEAF_NEXT_CHANGE);
                        weed_set_voidptr_value((weed_plant_t *)pchains[idx][pnum], WEED_LEAF_NEXT_CHANGE, event);
                        weed_leaf_delete(event, WEED_LEAF_PREV_CHANGE);
                        weed_set_voidptr_value(event, WEED_LEAF_PREV_CHANGE, pchains[idx][pnum]);
                      }
                      weed_leaf_delete(event, WEED_LEAF_NEXT_CHANGE);
                      weed_set_voidptr_value(event, WEED_LEAF_NEXT_CHANGE, NULL);
                      weed_leaf_delete(event, WEED_LEAF_INIT_EVENT);
                      weed_set_voidptr_value(event, WEED_LEAF_INIT_EVENT, init_event);
                      pchains[idx][pnum] = event;
                    }
                  }
                  lives_free(ptmpls);
                }
                lives_free(filter_hash);
		// *INDENT-OFF*
              }}}}}
      // *INDENT-ON*
      break;
    case WEED_EVENT_HINT_FRAME:
      if (tc == last_frame_tc) {
        ebuf = rec_error_add(ebuf, "Duplicate frame event", -1, tc);
        delete_event(event_list, event);
        was_deleted = TRUE;
      } else {
        if (!weed_plant_has_leaf(event, WEED_LEAF_CLIPS)) {
          weed_set_int_value(event, WEED_LEAF_CLIPS, -1);
          weed_set_int64_value(event, WEED_LEAF_FRAMES, 0);
          ebuf = rec_error_add(ebuf, "Frame event missing clips at", -1, tc);
        }

        last_frame_tc = tc;

        clip_index = weed_get_int_array_counted(event, WEED_LEAF_CLIPS, &num_tracks);
        frame_index = get_int64_array_convert(event, WEED_LEAF_FRAMES);

        new_clip_index = (int *)lives_malloc(num_tracks * sizint);
        new_frame_index = (int64_t *)lives_malloc(num_tracks * 8);
        last_valid_frame = 0;
        //#define DEBUG_MISSING_CLIPS
#ifdef DEBUG_MISSING_CLIPS
        g_print("pt zzz %d\n", num_tracks);
#endif
        for (i = 0; i < num_tracks; i++) {
          if (clip_index[i] > 0 && (clip_index[i] > MAX_FILES || renumbered_clips[clip_index[i]] < 1 ||
                                    mainw->files[renumbered_clips[clip_index[i]]] == NULL)) {
            // clip has probably been closed, so we remove its frames

            new_clip_index[i] = -1;
            new_frame_index[i] = 0;
            ebuf = rec_error_add(ebuf, "Invalid clip number", clip_index[i], tc);

#ifdef DEBUG_MISSING_CLIPS
            g_print("found invalid clip number %d on track %d, renumbered_clips=%d\n", clip_index[i], i,
                    renumbered_clips[clip_index[i]]);
#endif
            missing_clips = TRUE;
          } else {
            // take into account the fact that clip could have been resampled since layout was saved
            if (clip_index[i] > 0 && frame_index[i] > 0) {
              int rclip = renumbered_clips[clip_index[i]];
              if (lfps[rclip] != 0.) {
                new_frame_index[i] = count_resampled_frames(frame_index[i], lfps[rclip],
                                     mainw->files[rclip]->fps);
              } else new_frame_index[i] = frame_index[i];
              // the scrap_file has no real frames so we allow it to pass
              if (rclip != mainw->scrap_file && new_frame_index[i] > mainw->files[rclip]->frames) {
                ebuf = rec_error_add(ebuf, "Invalid frame number", new_frame_index[i], tc);
                new_clip_index[i] = -1;
                new_frame_index[i] = 0;
                missing_frames = TRUE;
              } else {
                // if recovering a recording we will be rendering from the clip editor and not using renumbered clips
                // so we must adjust the clip number in the layout
                // for multitrack, we leave the original clip number and use our renumbered clips mapping
                if (mainw->recording_recovered) new_clip_index[i] = rclip;
                else new_clip_index[i] = clip_index[i];
                new_frame_index[i] = frame_index[i];
                last_valid_frame = i + 1;
              }
            } else {
              new_clip_index[i] = clip_index[i];
              new_frame_index[i] = frame_index[i];
              last_valid_frame = i + 1;
            }
          }
        }

        if (last_valid_frame == 0) {
          lives_free(new_clip_index);
          lives_free(new_frame_index);
          new_clip_index = (int *)lives_malloc(sizint);
          new_frame_index = (int64_t *)lives_malloc(8);
          *new_clip_index = -1;
          *new_frame_index = 0;
          num_tracks = 1;
          weed_set_int_array(event, WEED_LEAF_CLIPS, num_tracks, new_clip_index);
          weed_set_int64_array(event, WEED_LEAF_FRAMES, num_tracks, new_frame_index);
        } else {
          if (last_valid_frame < num_tracks) {
            lives_free(clip_index);
            lives_free(frame_index);
            clip_index = (int *)lives_malloc(last_valid_frame * sizint);
            frame_index = (int64_t *)lives_malloc(last_valid_frame * 8);
            for (i = 0; i < last_valid_frame; i++) {
              clip_index[i] = new_clip_index[i];
              frame_index[i] = new_frame_index[i];
            }
            num_tracks = last_valid_frame;
            weed_set_int_array(event, WEED_LEAF_CLIPS, num_tracks, clip_index);
            weed_set_int64_array(event, WEED_LEAF_FRAMES, num_tracks, frame_index);
          } else {
            weed_set_int_array(event, WEED_LEAF_CLIPS, num_tracks, new_clip_index);
            weed_set_int64_array(event, WEED_LEAF_FRAMES, num_tracks, new_frame_index);
          }
        }

        lives_free(new_clip_index);
        lives_free(clip_index);
        lives_free(new_frame_index);
        lives_free(frame_index);

        if (WEED_EVENT_IS_AUDIO_FRAME(event)) {
          // check audio clips
          num_atracks = weed_leaf_num_elements(event, WEED_LEAF_AUDIO_CLIPS);
          if ((num_atracks & 1) != 0) {
            ebuf = rec_error_add(ebuf, "Invalid number of audio_clips", -1, tc);
            weed_leaf_delete(event, WEED_LEAF_AUDIO_CLIPS);
            weed_leaf_delete(event, WEED_LEAF_AUDIO_SEEKS);
          } else {
            if (!weed_plant_has_leaf(event, WEED_LEAF_AUDIO_SEEKS) || weed_leaf_num_elements(event,
                WEED_LEAF_AUDIO_SEEKS) != num_atracks) {
              ebuf = rec_error_add(ebuf, "Invalid number of audio_seeks", -1, tc);
              weed_leaf_delete(event, WEED_LEAF_AUDIO_CLIPS);
              weed_leaf_delete(event, WEED_LEAF_AUDIO_SEEKS);
            } else {
              aclip_index = weed_get_int_array(event, WEED_LEAF_AUDIO_CLIPS, &error);
              aseek_index = weed_get_double_array(event, WEED_LEAF_AUDIO_SEEKS, &error);
              new_aclip_index = (int *)lives_malloc(num_atracks * sizint);
              new_aseek_index = (double *)lives_malloc(num_atracks * sizdbl);
              j = 0;
              for (i = 0; i < num_atracks; i += 2) {
                if (aclip_index[i + 1] > 0) {
                  if ((aclip_index[i + 1] > MAX_FILES || renumbered_clips[aclip_index[i + 1]] < 1 ||
                       mainw->files[renumbered_clips[aclip_index[i + 1]]] == NULL) && aseek_index[i + 1] != 0.) {
                    // clip has probably been closed, so we remove its frames
                    ebuf = rec_error_add(ebuf, "Invalid audio clip number", aclip_index[i + 1], tc);
                    missing_clips = TRUE;
                  } else {
                    new_aclip_index[j] = aclip_index[i];
                    new_aclip_index[j + 1] = aclip_index[i + 1];
                    new_aseek_index[j] = aseek_index[i];
                    new_aseek_index[j + 1] = aseek_index[i + 1];
                    if (aseek_index[j + 1] != 0.) add_atrack_to_list(aclip_index[i], aclip_index[i + 1]);
                    else remove_atrack_from_list(aclip_index[i]);
                    j += 2;
                  }
                }
                if (aclip_index[i] > -1) {
                  if (mt != NULL && !mt->opts.pertrack_audio) {
                    mt->opts.pertrack_audio = TRUE;
                    // enable audio transitions
                    lives_widget_set_sensitive(mt->fx_region_2a, TRUE);
                    ebuf = rec_error_add(ebuf, "Adding pertrack audio", -1, tc);
                  } else force_pertrack_audio = TRUE;
                  // TODO ** inform user
                }
                if (aclip_index[i] == -1) {
                  if (mt != NULL && mt->opts.back_audio_tracks == 0) {
                    mt->opts.back_audio_tracks = 1;
                    ebuf = rec_error_add(ebuf, "Adding backing audio", -1, tc);
                  } else force_backing_tracks = 1;
                  // TODO ** inform user
                }
              }
              if (j == 0) {
                weed_leaf_delete(event, WEED_LEAF_AUDIO_CLIPS);
                weed_leaf_delete(event, WEED_LEAF_AUDIO_SEEKS);
              } else {
                weed_set_int_array(event, WEED_LEAF_AUDIO_CLIPS, j, new_aclip_index);
                weed_set_double_array(event, WEED_LEAF_AUDIO_SEEKS, j, new_aseek_index);
              }
              lives_free(aclip_index);
              lives_free(aseek_index);
              lives_free(new_aclip_index);
              lives_free(new_aseek_index);
	      // *INDENT-OFF*
            }}}}
      // *INDENT-ON*
      break;

    case WEED_EVENT_HINT_MARKER:
      // check marker values
      if (!weed_plant_has_leaf(event, WEED_LEAF_LIVES_TYPE)) {
        ebuf = rec_error_add(ebuf, "Unknown marker type", -1, tc);
        delete_event(event_list, event);
        was_deleted = TRUE;
      } else {
        marker_type = weed_get_int_value(event, WEED_LEAF_LIVES_TYPE, &error);
        if (marker_type != EVENT_MARKER_BLOCK_START && marker_type != EVENT_MARKER_BLOCK_UNORDERED &&
            marker_type != EVENT_MARKER_RECORD_END && marker_type != EVENT_MARKER_RECORD_START) {
          ebuf = rec_error_add(ebuf, "Unknown marker type", marker_type, tc);
          delete_event(event_list, event);
          was_deleted = TRUE;
        }
        if (marker_type == EVENT_MARKER_BLOCK_START && !weed_plant_has_leaf(event, WEED_LEAF_TRACKS)) {
          ebuf = rec_error_add(ebuf, "Block start marker has no tracks", -1, tc);
          delete_event(event_list, event);
          was_deleted = TRUE;
        }
      }
      break;
    default:
      ebuf = rec_error_add(ebuf, "Invalid hint", hint, tc);
      delete_event(event_list, event);
      was_deleted = TRUE;
    }
    if (!was_deleted && check_filter_map && last_filter_map != NULL && \
        (event_next == NULL || get_event_timecode(event_next) > last_deinit_tc)) {
      // if our last filter_map refers to filter instances which were deinited, we must add another filter_map here
      ebuf = filter_map_check(trans_table, last_filter_map, last_deinit_tc, tc, ebuf);
      check_filter_map = FALSE;
    }
    event = event_next;

    if (!was_deleted && fps != 0.) {
      while (cur_tc < last_frame_tc) {
        // add blank frames
        if (!has_frame_event_at(event_list, cur_tc, &shortcut)) {
          if (shortcut != NULL) {
            shortcut = duplicate_frame_at(event_list, shortcut, cur_tc);
            ebuf = rec_error_add(ebuf, "Duplicated frame at", -1, cur_tc);
          } else {
            event_list = insert_blank_frame_event_at(event_list, cur_tc, &shortcut);
            ebuf = rec_error_add(ebuf, "Inserted missing blank frame", -1, cur_tc);
          }
        }
        cur_tc += TICKS_PER_SECOND_DBL / fps;
        cur_tc = q_gint64(cur_tc, fps);
      }
    }
    last_tc = tc;
  }

  if (fps == 0.) {
    lives_free(ebuf);
    return TRUE;
  }

  // add any missing filter_deinit events
  ebuf = add_filter_deinits(event_list, trans_table, pchains, last_tc, ebuf);

  // check the last filter map
  if (last_filter_map != NULL) ebuf = add_null_filter_map(event_list, last_filter_map, last_tc, ebuf);

  last_event = get_last_event(event_list);
  remove_end_blank_frames(event_list, TRUE);

  if (get_last_event(event_list) != last_event) {
    last_event = get_last_event(event_list);
    last_tc = get_event_timecode(last_event);
    ebuf = rec_error_add(ebuf, "Removed final blank frames", -1, last_tc);
  }

  // pass 2 - move left any FILTER_DEINITS before the FRAME, move right any FILTER_INITS or PARAM_CHANGES after the FRAME
  // ensure we have at most 1 FILTER_MAP before each FRAME, and 1 FILTER_MAP after a FRAME

  // we do this as a second pass since we may have inserted blank frames
  last_frame_tc = last_filter_map_tc = -1;
  last_frame_event = NULL;

  event = get_first_event(event_list);
  while (event != NULL) {
    was_moved = FALSE;
    event_next = get_next_event(event);
    tc = get_event_timecode(event);
    hint = get_event_hint(event);
    switch (hint) {
    case WEED_EVENT_HINT_FILTER_INIT:
      // if our in_parameters are int64, convert to void *
      if (weed_plant_has_leaf(event, WEED_LEAF_IN_PARAMETERS)) {
        uint64_t *pin_params;
        void **nin_params;
        num_params = weed_leaf_num_elements(event, WEED_LEAF_IN_PARAMETERS);

        if (weed_leaf_seed_type(event, WEED_LEAF_IN_PARAMETERS) == WEED_SEED_INT64) {
          pin_params = (uint64_t *)weed_get_int64_array(event, WEED_LEAF_IN_PARAMETERS, &error);
          nin_params = (void **)lives_malloc(num_params * sizeof(void *));
          for (i = 0; i < num_params; i++) {
            nin_params[i] = (void *)pin_params[i];
          }
          lives_free(pin_params);
          weed_leaf_delete(event, WEED_LEAF_IN_PARAMETERS);
          weed_set_voidptr_array(event, WEED_LEAF_IN_PARAMETERS, num_params, nin_params);
          lives_free(nin_params);
        }

        filter_hash = weed_get_string_value(event, WEED_LEAF_FILTER, &error);
        if ((filter_idx = weed_get_idx_for_hashname(filter_hash, TRUE)) != -1) {
          void **pchain;
          filter = get_weed_filter(filter_idx);
          // fill in any newly added params
          num_tracks = weed_leaf_num_elements(event, WEED_LEAF_IN_TRACKS);
          pchain = filter_init_add_pchanges(event_list, filter, event, num_tracks, num_params);
          lives_free(pchain);
        }
        lives_free(filter_hash);
      }
      if (mt != NULL && event != mt->avol_init_event) {
        if (!move_event_right(event_list, event, last_frame_tc != tc, fps)) was_moved = TRUE;
      }
      break;
    case WEED_EVENT_HINT_PARAM_CHANGE:
      if (last_frame_tc == tc) if (!move_event_right(event_list, event, FALSE, fps)) was_moved = TRUE;
      break;
    case WEED_EVENT_HINT_FILTER_DEINIT:
      if (mt != NULL && weed_get_voidptr_value(event, WEED_LEAF_INIT_EVENT, &error) != mt->avol_init_event) {
        if (!move_event_left(event_list, event, last_frame_tc == tc, fps)) was_moved = TRUE;
      }
      break;
    case WEED_EVENT_HINT_FILTER_MAP:
      if (last_filter_map_tc == tc) {
        // remove last filter_map
        ebuf = rec_error_add(ebuf, "Duplicate filter maps", -1, tc);
        delete_event(event_list, last_filter_map);
      }
      last_filter_map_tc = tc;
      last_filter_map = event;
      break;
    case WEED_EVENT_HINT_FRAME:
      last_frame_tc = tc;
      last_filter_map_tc = -1;
      last_frame_event = event;
      break;
    }
    if (was_moved) {
      if (last_frame_event != NULL) event = last_frame_event;
      else event = get_first_event(event_list);
    } else event = event_next;
  }

  ebuf = add_missing_atrack_closers(event_list, fps, ebuf);

  transient = LIVES_MAIN_WINDOW_WIDGET;

  if (missing_clips && missing_frames) {
    bit2 = lives_strdup(_("clips and frames"));
  } else {
    if (missing_clips) {
      bit2 = lives_strdup(_("clips"));
    } else if (missing_frames) {
      bit2 = lives_strdup(_("frames"));
    }
  }

  end_threaded_dialog();

  if (bit2 != NULL) {
    if (mt != NULL && mt->auto_reloading) {
      lives_free(bit1);
      lives_free(bit3);
      bit1 = lives_strdup(_("\nAuto reload layout.\n"));
      bit3 = lives_strdup_printf("\n%s", prefs->ar_layout_name);
    }
    msg = lives_strdup_printf(_("%s\nSome %s are missing from the layout%s\nTherefore it could not be loaded properly.\n"),
                              bit1, bit2, bit3);
    do_error_dialog_with_check_transient(msg, TRUE, 0, LIVES_WINDOW(transient));
    lives_free(msg);
    lives_free(bit2);
    if (mt != NULL) mt->layout_prompt = TRUE;
  }
  lives_free(bit1);
  lives_free(bit3);

  lives_free(ebuf); // TODO - allow option of viewing/saving this

  return TRUE;
}


char *get_eload_filename(lives_mt * mt, boolean allow_auto_reload) {
  LiVESWidget *hbox;
  LiVESWidget *ar_checkbutton;

  boolean needs_idlefunc = FALSE;
  boolean did_backup = mt->did_backup;

  char *filt[] = {"*." LIVES_FILE_EXT_LAYOUT, NULL};

  char *eload_dir;
  char *eload_file;
  char *startdir = NULL;

  if (!strlen(mainw->set_name)) {
    LIVES_ERROR("Loading event list for unknown set");
    return NULL;
  }

  eload_dir = lives_build_path(prefs->workdir, mainw->set_name, LAYOUTS_DIRNAME, NULL);

  mainw->com_failed = FALSE;
  lives_mkdir_with_parents(eload_dir, capable->umask);

  if (!mainw->recoverable_layout && !lives_file_test(eload_dir, LIVES_FILE_TEST_IS_DIR)) {
    lives_free(eload_dir);
    return NULL;
  }

  startdir = lives_strdup(eload_dir);

  hbox = lives_hbox_new(FALSE, 0);

  if (allow_auto_reload) {
    ar_checkbutton = make_autoreload_check(LIVES_HBOX(hbox), prefs->ar_layout);
    lives_signal_connect(LIVES_GUI_OBJECT(ar_checkbutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                         LIVES_GUI_CALLBACK(toggle_sets_pref),
                         (livespointer)PREF_AR_LAYOUT);
  }

  if (mt->idlefunc > 0) {
    lives_source_remove(mt->idlefunc);
    mt->idlefunc = 0;
    needs_idlefunc = TRUE;
  }

  lives_widget_show_all(hbox);

  eload_file = choose_file(startdir, NULL, filt, LIVES_FILE_CHOOSER_ACTION_OPEN, NULL, hbox);

  lives_free(startdir);

  if (eload_file == NULL) {
    // if the user cancelled see if we can clear the directories
    // this will fail if there are any files in the directories

    char *cdir;
    lives_rmdir(eload_dir, FALSE);

    cdir = lives_build_filename(prefs->workdir, mainw->set_name, NULL);
    lives_rmdir(cdir, FALSE);

    if (needs_idlefunc || (!did_backup && mt->auto_changed))
      mt->idlefunc = mt_idle_add(mt);
  }

  lives_free(eload_dir);

  return eload_file;
}


weed_plant_t *load_event_list(lives_mt * mt, char *eload_file) {
  // load (deserialise) a serialised event_list
  // after loading we perform sophisticated checks on it to detect
  // and try to repair any errors in it
  weed_plant_t *event_list = NULL;

  char *msg;
  char *eload_name;

  boolean free_eload_file = TRUE;
  boolean orig_ar_layout = prefs->ar_layout, ar_layout;
  boolean retval = TRUE;
  boolean needs_idlefunc = FALSE;

  int num_events = 0;
  int retval2;
  int old_avol_fx;
  int fd;

  if (mt != NULL) {
    old_avol_fx  = mt->avol_fx;
    if (mt->idlefunc > 0) {
      lives_source_remove(mt->idlefunc);
      mt->idlefunc = 0;
      needs_idlefunc = TRUE;
    }
  }

  if (eload_file == NULL) {
    eload_file = get_eload_filename(mt, TRUE);
    if (eload_file == NULL) {
      if (needs_idlefunc) mt->idlefunc = mt_idle_add(mt);
      return NULL;
    }
  } else free_eload_file = FALSE;

  ar_layout = prefs->ar_layout;
  prefs->ar_layout = orig_ar_layout;

  if (!mainw->recoverable_layout) eload_name = lives_strdup(eload_file);
  else eload_name = lives_strdup(_("auto backup"));

  if ((fd = lives_open_buffered_rdonly(eload_file)) < 0) {
    if (mt != NULL) {
      msg = lives_strdup_printf(_("\nUnable to load layout file %s\n"), eload_name);
      do_error_dialog_with_check_transient(msg, TRUE, 0, LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET));
      lives_free(msg);
      if (needs_idlefunc) mt->idlefunc = mt_idle_add(mt);
    }
    lives_free(eload_name);
    return NULL;
  }

  lives_buffered_rdonly_slurp(fd, 0);

  if (mt != NULL) {
    event_list_free_undos(mt);

    if (mainw->event_list != NULL) {
      event_list_free(mt->event_list);
      mt->event_list = NULL;
      mt_clear_timeline(mt);
    }

    mainw->no_switch_dprint = TRUE;
    d_print(_("Loading layout from %s..."), eload_name);
    mainw->no_switch_dprint = FALSE;

    mt_desensitise(mt);
  }

  do {
    retval = 0;
    if ((event_list = load_event_list_inner(mt, fd, mt != NULL, &num_events, NULL, NULL)) == NULL) {
      lives_close_buffered(fd);

      if (mainw->read_failed == fd + 1) {
        mainw->read_failed = 0;
        if (mt != NULL) retval = do_read_failed_error_s_with_retry(eload_name, NULL, NULL);
        mainw->read_failed = FALSE;
      }

      if (mt != NULL && retval != LIVES_RESPONSE_RETRY) {
        if (mt->is_ready) mt_sensitise(mt);
        lives_free(eload_name);
        if (needs_idlefunc) mt->idlefunc = mt_idle_add(mt);
        return NULL;
      }
    } else lives_close_buffered(fd);

    if (mt == NULL) {
      lives_free(eload_name);
      renumber_from_backup_layout_numbering(NULL);
      if (!event_list_rectify(NULL, event_list)) {
        event_list_free(event_list);
        event_list = NULL;
      }
      if (get_first_event(event_list) == NULL) {
        event_list_free(event_list);
        event_list = NULL;
      }
      return event_list;
    }
  } while (retval == LIVES_RESPONSE_RETRY);

  lives_free(eload_name);

  d_print_done();

  d_print(_("Got %d events...processing..."), num_events);

  mt->changed = mainw->recoverable_layout;
  lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET, TRUE);

  cfile->progress_start = 1;
  cfile->progress_end = num_events;

  // event list loaded, now we set the pointers for filter_map (init_events), param_change (init_events and param chains),
  // filter_deinit (init_events)
  do_threaded_dialog(_("Checking and rebuilding event list"), FALSE);

  elist_errors = 0;

  if (!mainw->recoverable_layout) {
    // re-map clips so our loaded event_list refers to the correct clips and frames
    rerenumber_clips(eload_file, NULL);
  } else {
    renumber_from_backup_layout_numbering(mt);
  }

  mt->avol_init_event = NULL;
  mt->avol_fx = -1;

  if (!event_list_rectify(mt, event_list)) {
    event_list_free(event_list);
    event_list = NULL;
  }

  if (get_first_event(event_list) == NULL) {
    event_list_free(event_list);
    event_list = NULL;
  }

  if (event_list != NULL) {
    d_print(_("%d errors detected.\n"), elist_errors);
    if (!mt->auto_reloading) {
      if (!mt->layout_prompt || do_mt_rect_prompt()) {
        do {
          retval2 = 0;
          retval = TRUE;

          // resave with corrections/updates
          fd = lives_create_buffered(eload_file, DEF_FILE_PERMS);
          if (fd >= 0) {
            retval = save_event_list_inner(NULL, fd, event_list, NULL);
            lives_close_buffered(fd);
          }

          if (fd < 0 || !retval) {
            retval2 = do_write_failed_error_s_with_retry(eload_file, (fd < 0) ? lives_strerror(errno) : NULL, NULL);
            if (retval2 == LIVES_RESPONSE_CANCEL) d_print_file_error_failed();
          }
        } while (retval2 == LIVES_RESPONSE_RETRY);
      }
    }
  } else d_print_failed();

  mt->layout_prompt = FALSE;

  if (mt->avol_fx == -1 && mainw->fx_candidates[FX_CANDIDATE_AUDIO_VOL].delegate != -1) {
    // user (or system) has delegated an audio volume filter from the candidates
    mt->avol_fx = LIVES_POINTER_TO_INT(lives_list_nth_data(mainw->fx_candidates[FX_CANDIDATE_AUDIO_VOL].list,
                                       mainw->fx_candidates[FX_CANDIDATE_AUDIO_VOL].delegate));
  }

  if (mt->avol_fx != old_avol_fx && mt->opts.aparam_view_list != NULL) {
    // audio volume effect changed, so we reset which parameters are viewed
    lives_list_free(mt->opts.aparam_view_list);
    mt->opts.aparam_view_list = NULL;
  }

  if (event_list != NULL) {
    if (!mainw->recoverable_layout) {
      lives_snprintf(mt->layout_name, PATH_MAX, "%s", eload_file);
      get_basename(mt->layout_name);
    }

    if (mt->layout_set_properties) msg = mt_set_vals_string();
    else msg = lives_strdup_printf(_("Multitrack fps set to %.3f\n"), cfile->fps);
    d_print(msg);
    lives_free(msg);

    set_mt_title(mt);

    if (!ar_layout) {
      prefs->ar_layout = FALSE;
      set_string_pref(PREF_AR_LAYOUT, "");
      lives_memset(prefs->ar_layout_name, 0, 1);
    } else {
      if (!mainw->recoverable_layout) {
        prefs->ar_layout = TRUE;
        set_string_pref(PREF_AR_LAYOUT, mt->layout_name);
        lives_snprintf(prefs->ar_layout_name, 128, "%s", mt->layout_name);
      }
    }
  }

  if (cfile->achans > 0) {
    set_audio_filter_channel_values(mt);
  }

  if (mt->opts.back_audio_tracks > 0) {
    lives_widget_show(mt->view_audio);
  }

  if (free_eload_file) lives_free(eload_file);

  if (!mainw->recoverable_layout) {
    polymorph(mt, POLY_CLIPS);
  }

  return (event_list);
}


void remove_markers(weed_plant_t *event_list) {
  weed_plant_t *event = get_first_event(event_list);
  weed_plant_t *event_next;
  int marker_type;
  int error;

  while (event != NULL) {
    event_next = get_next_event(event);
    if (WEED_EVENT_IS_MARKER(event)) {
      marker_type = weed_get_int_value(event, WEED_LEAF_LIVES_TYPE, &error);
      if (marker_type == EVENT_MARKER_BLOCK_START || marker_type == EVENT_MARKER_BLOCK_UNORDERED) {
        delete_event(event_list, event);
      }
    }
    event = event_next;
  }
}


void wipe_layout(lives_mt * mt) {
  mt_desensitise(mt);

  if (mt->idlefunc > 0) {
    lives_source_remove(mt->idlefunc);
    mt->idlefunc = 0;
  }

  print_layout_wiped();

  close_scrap_file(TRUE);
  close_ascrap_file(TRUE);

  recover_layout_cancelled(FALSE);
  mainw->recording_recovered = FALSE;

  if (strlen(mt->layout_name) > 0 && !strcmp(mt->layout_name, prefs->ar_layout_name)) {
    set_string_pref(PREF_AR_LAYOUT, "");
    lives_memset(prefs->ar_layout_name, 0, 1);
    prefs->ar_layout = FALSE;
  }

  event_list_free(mt->event_list);
  mt->event_list = NULL;

  event_list_free_undos(mt);

  mt_clear_timeline(mt);

  mt_sensitise(mt);

  mt->idlefunc = mt_idle_add(mt);
}


void on_clear_event_list_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  _entryw *cdsw;

  int resp = 2;

  boolean rev_resp = FALSE; // if TRUE, a return value of 2 means save, otherwise it means delete

  if (mt->idlefunc > 0) {
    lives_source_remove(mt->idlefunc);
    mt->idlefunc = 0;
  }

  if (strlen(mt->layout_name) > 0) {
    // delete : 2
    // wipe : 1
    cdsw = create_cds_dialog(2);
    rev_resp = FALSE;
  } else {
    // save: 2
    // wipe: 1
    cdsw = create_cds_dialog(3);
    rev_resp = TRUE;
  }

  do {
    mainw->cancelled = CANCEL_NONE;
    resp = lives_dialog_run(LIVES_DIALOG(cdsw->dialog));

    if (resp == 2 && rev_resp) {
      // save
      on_save_event_list_activate(NULL, mt);
      if (mainw->cancelled == CANCEL_NONE) break;
    }
  } while (resp == 2 && rev_resp);

  lives_widget_destroy(cdsw->dialog);
  lives_free(cdsw);

  if (resp == LIVES_RESPONSE_CANCEL) {
    mt->idlefunc = mt_idle_add(mt);
    return; // cancel
  }

  if (resp == 2 && !rev_resp) {
    // delete from disk
    LiVESList *layout_map = NULL;
    char *lmap_file;
    if (!do_yesno_dialog("\nLayout will be deleted from the disk.\nAre you sure ?\n")) {
      mt->idlefunc = mt_idle_add(mt);
      return;
    }

    lmap_file = lives_build_filename(WORKDIR_LITERAL, mainw->set_name, LAYOUTS_DIRNAME, mt->layout_name, NULL);
    layout_map = lives_list_append(layout_map, lmap_file);
    remove_layout_files(layout_map);
    lives_free(lmap_file);
  } else {
    // wipe
    if (mt->changed) {
      if (!do_yesno_dialog_with_check(
            _("The current layout has changes which have not been saved.\nAre you sure you wish to wipe it ?\n"),
            WARN_MASK_LAYOUT_WIPE)) {
        mt->idlefunc = mt_idle_add(mt);
        return;
      }
    }
  }

  // wipe
  wipe_layout(mt);
}


boolean on_load_event_list_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  int i;
  lives_mt *mt = (lives_mt *)user_data;
  weed_plant_t *new_event_list;

  if (mainw->interactive)
    if (!check_for_layout_del(mt, FALSE)) return FALSE;

  if (mt->idlefunc > 0) {
    lives_source_remove(mt->idlefunc);
    mt->idlefunc = 0;
  }

  new_event_list = load_event_list(mt, mt->force_load_name);

  if (mainw->was_set) recover_layout_cancelled(FALSE);

  if (new_event_list == NULL) {
    mt_sensitise(mt);
    mt->idlefunc = mt_idle_add(mt);
    return FALSE;
  }

  if (mt->event_list != NULL) event_list_free(mt->event_list);
  mt->event_list = NULL;

  mt->undo_buffer_used = 0;
  mt->undo_offset = 0;
  lives_list_free(mt->undos);
  mt->undos = NULL;
  mt_set_undoable(mt, MT_UNDO_NONE, NULL, FALSE);
  mt_set_redoable(mt, MT_UNDO_NONE, NULL, FALSE);

  for (i = 0; i < mt->num_video_tracks; i++) {
    delete_video_track(mt, i, FALSE);
  }
  lives_list_free(mt->video_draws);
  mt->video_draws = NULL;
  mt->num_video_tracks = 0;

  if (mt->amixer != NULL) on_amixer_close_clicked(NULL, mt);

  delete_audio_tracks(mt, mt->audio_draws, FALSE);
  mt->audio_draws = NULL;

  if (mt->audio_vols != NULL) lives_list_free(mt->audio_vols);
  mt->audio_vols = NULL;

  mt->event_list = new_event_list;

  if (mt->selected_tracks != NULL) lives_list_free(mt->selected_tracks);
  mt->selected_tracks = NULL;

  mt_init_tracks(mt, TRUE);

  if (!mt->ignore_load_vals) set_audio_mixer_vols(mt, mt->event_list);

  add_aparam_menuitems(mt);

  unselect_all(mt);
  remove_markers(mt->event_list);
  mt_sensitise(mt);
  mt_show_current_frame(mt, FALSE);

  mt->idlefunc = mt_idle_add(mt);

  return TRUE;
}


void migrate_layouts(const char *old_set_name, const char *new_set_name) {
  // if we change the name of a set, we must also update the layouts - at the very least 2 things need to happen
  // 1) the WEED_LEAF_NEEDS_SET leaf in each layout must be updated
  // 2) the layouts will be physically moved, so if appending we check for name collisions
  // 3) the names of layouts in mainw->affected_layouts_map must be altered

  // here we also update mainw->current_layouts_map and the layout_maps for each clip

  // this last may not be necessary as we are probably closing the set

  // on return from here we physically move the layouts, and we append the layout_map to the new one

  // load each event_list in mainw->current_layouts_map
  LiVESList *map = mainw->current_layouts_map;
  int fd;
  int i;
  int retval2 = 0;
  weed_plant_t *event_list;
  char *tmp;
  boolean retval = TRUE;

  char *changefrom = NULL;
  size_t chlen;

  if (old_set_name != NULL) {
    changefrom = lives_build_filename(prefs->workdir, old_set_name, LAYOUTS_DIRNAME LIVES_DIR_SEP, NULL);
    chlen = strlen(changefrom);
  } else chlen = 0;

  while (map != NULL) {
    if (old_set_name != NULL) {
      // load and save each layout, updating the WEED_LEAF_NEEDS_SET leaf
      do {
        retval2 = 0;
        if ((fd = lives_open_buffered_rdonly((char *)map->data)) > -1) {
          lives_buffered_rdonly_slurp(fd, 0);
          if ((event_list = load_event_list_inner(NULL, fd, FALSE, NULL, NULL, NULL)) != NULL) {
            lives_close_buffered(fd);
            // adjust the value of WEED_LEAF_NEEDS_SET to new_set_name
            weed_set_string_value(event_list, WEED_LEAF_NEEDS_SET, (tmp = F2U8(new_set_name)));
            lives_free(tmp);
            // save the event_list with the same name
            lives_rm((char *)map->data);

            do {
              retval2 = 0;
              fd = lives_create_buffered((char *)map->data, DEF_FILE_PERMS);
              if (fd >= 0) {
                retval = save_event_list_inner(NULL, fd, event_list, NULL);
              }
              if (fd < 0 || !retval) {
                if (fd > 0) lives_close_buffered(fd);
                retval2 = do_write_failed_error_s_with_retry((char *)map->data, (fd < 0) ? lives_strerror(errno) : NULL, NULL);
              }
            } while (retval2 == LIVES_RESPONSE_RETRY);

            event_list_free(event_list);
          }
          if (retval2 == 0) lives_close_buffered(fd);
        } else {
          retval2 = do_read_failed_error_s_with_retry((char *)map->data, NULL, NULL);
        }
      } while (retval2 == LIVES_RESPONSE_RETRY);
    }

    if (old_set_name != NULL && !lives_strncmp((char *)map->data, changefrom, chlen)) {
      // update entries in mainw->current_layouts_map
      tmp = lives_build_filename(prefs->workdir, new_set_name, LAYOUTS_DIRNAME, (char *)map->data + chlen, NULL);
      if (lives_file_test(tmp, LIVES_FILE_TEST_EXISTS)) {
        // prevent duplication of layouts
        lives_free(tmp);
        tmp = lives_strdup_printf("%s" LIVES_DIR_SEP "%s" LIVES_DIR_SEP LAYOUTS_DIRNAME LIVES_DIR_SEP "%s-%s",
                                  prefs->workdir, new_set_name, old_set_name, (char *)map->data + chlen);
        lives_mv((const char *)map->data, tmp);
      }
      lives_free((livespointer)map->data);
      map->data = tmp;
    }
    map = map->next;
  }

  // update layout_map's in mainw->files
  for (i = 1; i <= MAX_FILES; i++) {
    if (mainw->files[i] != NULL) {
      if (mainw->files[i]->layout_map != NULL) {
        map = mainw->files[i]->layout_map;
        while (map != NULL) {
          if (map->data != NULL) {
            if ((old_set_name != NULL && !lives_strncmp((char *)map->data, changefrom, chlen)) ||
                (old_set_name == NULL && (strstr((char *)map->data, new_set_name) == NULL))) {

              char **array = lives_strsplit((char *)map->data, "|", -1);
              size_t origlen = strlen(array[0]);
              char *tmp2 = lives_build_filename(prefs->workdir, new_set_name, LAYOUTS_DIRNAME, array[0] + chlen, NULL);
              if (lives_file_test(tmp2, LIVES_FILE_TEST_EXISTS)) {
                tmp2 = lives_strdup_printf("%s" LIVES_DIR_SEP "%s" LIVES_DIR_SEP LAYOUTS_DIRNAME LIVES_DIR_SEP "%s-%s",
                                           prefs->workdir, new_set_name, old_set_name, array[0] + chlen);
              }
              tmp = lives_strdup_printf("%s%s", tmp2, (char *)map->data + origlen);
              lives_free(tmp2);
              lives_strfreev(array);

              lives_free((livespointer)map->data);
              map->data = tmp;
            }
            map = map->next;
	    // *INDENT-OFF*
          }}}}}
  // *INDENT-ON*

  // update mainw->affected_layouts_map
  map = mainw->affected_layouts_map;
  while (map != NULL) {
    if ((old_set_name != NULL && !lives_strncmp((char *)map->data, changefrom, chlen)) ||
        (old_set_name == NULL && (strstr((char *)map->data, new_set_name) == NULL))) {
      if (strcmp(mainw->string_constants[LIVES_STRING_CONSTANT_CL], (char *)map->data + chlen)) {
        tmp = lives_build_filename(prefs->workdir, new_set_name, LAYOUTS_DIRNAME, (char *)map->data + chlen, NULL);
        if (lives_file_test(tmp, LIVES_FILE_TEST_EXISTS)) {
          lives_free(tmp);
          tmp = lives_strdup_printf("%s" LIVES_DIR_SEP "%s" LIVES_DIR_SEP LAYOUTS_DIRNAME LIVES_DIR_SEP "%s-%s",
                                    prefs->workdir, new_set_name, old_set_name, (char *)map->data + chlen);
        }
        lives_free((livespointer)map->data);
        map->data = tmp;
      }
    }
    map = map->next;
  }
  lives_freep((void **)&changefrom);
}


LiVESList *layout_frame_is_affected(int clipno, int start, int end, LiVESList * xlays) {
  // return list of names of layouts which are affected, or NULL
  // list and list->data should be freed after use

  char **array;
  LiVESList *lmap = mainw->files[clipno]->layout_map;
  double orig_fps;
  int resampled_frame;

  if (mainw->stored_event_list != NULL && mainw->files[clipno]->stored_layout_frame != 0) {
    // see if it affects the current layout
    resampled_frame = count_resampled_frames(mainw->files[clipno]->stored_layout_frame, mainw->files[clipno]->stored_layout_fps,
                      mainw->files[clipno]->fps);
    if (start <= resampled_frame && (end == 0 || end >= resampled_frame))
      xlays = lives_list_append_unique(xlays, mainw->string_constants[LIVES_STRING_CONSTANT_CL]);
  }

  while (lmap != NULL) {
    array = lives_strsplit((char *)lmap->data, "|", -1);
    if (atoi(array[2]) != 0) {
      orig_fps = strtod(array[3], NULL);
      resampled_frame = count_resampled_frames(atoi(array[2]), orig_fps, mainw->files[clipno]->fps);
      if (array[2] == 0) resampled_frame = 0;
      if (start <= resampled_frame && (end == 0 || end >= resampled_frame))
        xlays = lives_list_append_unique(xlays, array[0]);
    }
    lives_strfreev(array);
    lmap = lmap->next;
  }

  return xlays;
}


LiVESList *layout_audio_is_affected(int clipno, double stime, double etime, LiVESList * xlays) {
  char **array;
  LiVESList *lmap = mainw->files[clipno]->layout_map;
  double max_time;

  if (mainw->files[clipno]->arate == 0) return mainw->xlays;

  // adjust time depending on if we have stretched audio
  stime *= mainw->files[clipno]->arps / mainw->files[clipno]->arate;
  etime *= mainw->files[clipno]->arps / mainw->files[clipno]->arate;

  if (mainw->stored_event_list != NULL) {
    // see if it affects the current layout
    if (mainw->files[clipno]->stored_layout_audio > 0. && stime <= mainw->files[clipno]->stored_layout_audio &&
        (etime == 0. || etime <= mainw->files[clipno]->stored_layout_audio))
      xlays = lives_list_append_unique(xlays, mainw->string_constants[LIVES_STRING_CONSTANT_CL]);
  }

  while (lmap != NULL) {
    if (get_token_count((char *)lmap->data, '|') < 5) continue;
    array = lives_strsplit((char *)lmap->data, "|", -1);
    max_time = strtod(array[4], NULL);
    if (max_time > 0. && stime <= max_time && (etime == 0. || etime <= mainw->files[clipno]->stored_layout_audio)) {
      xlays = lives_list_append_unique(xlays, array[0]);
    }
    lives_strfreev(array);
    lmap = lmap->next;
  }

  return xlays;
}


void mt_change_disp_tracks_ok(LiVESButton * button, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  lives_general_button_clicked(button, NULL);
  prefs->max_disp_vtracks = mainw->fx1_val;
  set_int_pref(PREF_MAX_DISP_VTRACKS, prefs->max_disp_vtracks);
  scroll_tracks(mt, mt->top_track, FALSE);
}


///////////////////////////////////////////////

void show_frame_events_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  prefs->event_window_show_frame_events = !prefs->event_window_show_frame_events;
}


void mt_change_max_disp_tracks(LiVESMenuItem * menuitem, livespointer user_data) {
  LiVESWidget *dialog;
  lives_mt *mt = (lives_mt *)user_data;

  mainw->fx1_val = prefs->max_disp_vtracks;
  dialog = create_cdtrack_dialog(LIVES_DEVICE_INTERNAL, mt);
  lives_widget_show(dialog);
}


void mt_load_vals_toggled(LiVESMenuItem * menuitem, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  mt->ignore_load_vals = !mt->ignore_load_vals;
}


static void mt_ac_audio_toggled(LiVESMenuItem * menuitem, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  mt->opts.autocross_audio = !mt->opts.autocross_audio;
}


void mt_change_vals_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  lives_mt *mt = (lives_mt *)user_data;
  boolean response;
  char *msg;

  rdet = create_render_details(4);
  rdet->enc_changed = FALSE;
  do {
    rdet->suggestion_followed = FALSE;
    if ((response = lives_dialog_run(LIVES_DIALOG(rdet->dialog))) == LIVES_RESPONSE_OK) {
      if (rdet->enc_changed) {
        check_encoder_restrictions(FALSE, FALSE, TRUE);
      }
    }
  } while (rdet->suggestion_followed);

  if (resaudw != NULL) {
    xarate = (int)atoi(lives_entry_get_text(LIVES_ENTRY(resaudw->entry_arate)));
    xachans = (int)atoi(lives_entry_get_text(LIVES_ENTRY(resaudw->entry_achans)));
    xasamps = (int)atoi(lives_entry_get_text(LIVES_ENTRY(resaudw->entry_asamps)));

    if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(resaudw->rb_unsigned))) {
      xse = AFORM_UNSIGNED;
    } else xse = 0;
    if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(resaudw->rb_bigend))) {
      xse |= AFORM_BIG_ENDIAN;
    }
  } else {
    xachans = xarate = xasamps = 0;
    xse = cfile->signed_endian;
  }

  if (response == LIVES_RESPONSE_CANCEL) {
    lives_widget_destroy(rdet->dialog);
    lives_free(rdet->encoder_name);
    lives_freep((void **)&rdet);
    lives_freep((void **)&resaudw);
    return;
  }

  if (xachans == 0 && mt->audio_draws != NULL) {
    LiVESList *slist = mt->audio_draws;
    while (slist != NULL) {
      if (lives_widget_object_get_data(LIVES_WIDGET_OBJECT(slist->data), "blocks") != NULL) {
        do_mt_no_audchan_error();
        lives_widget_destroy(rdet->dialog);
        lives_free(rdet->encoder_name);
        lives_freep((void **)&rdet);
        lives_freep((void **)&resaudw);
        return;
      }
      slist = slist->next;
    }
  }

  if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(rdet->always_checkbutton))) {
    prefs->mt_enter_prompt = FALSE;
    set_boolean_pref(PREF_MT_ENTER_PROMPT, prefs->mt_enter_prompt);
    prefs->mt_def_width = rdet->width;
    set_int_pref(PREF_MT_DEF_WIDTH, prefs->mt_def_width);
    prefs->mt_def_height = rdet->height;
    set_int_pref(PREF_MT_DEF_HEIGHT, prefs->mt_def_height);
    prefs->mt_def_fps = rdet->fps;
    set_double_pref(PREF_MT_DEF_FPS, prefs->mt_def_fps);
    prefs->mt_def_arate = xarate;
    set_int_pref(PREF_MT_DEF_ARATE, prefs->mt_def_arate);
    prefs->mt_def_achans = xachans;
    set_int_pref(PREF_MT_DEF_ACHANS, prefs->mt_def_achans);
    prefs->mt_def_asamps = xasamps;
    set_int_pref(PREF_MT_DEF_ASAMPS, prefs->mt_def_asamps);
    prefs->mt_def_signed_endian = xse;
    set_int_pref(PREF_MT_DEF_SIGNED_ENDIAN, prefs->mt_def_signed_endian);
    prefs->mt_pertrack_audio = ptaud;
    set_boolean_pref(PREF_MT_PERTRACK_AUDIO, prefs->mt_pertrack_audio);
    prefs->mt_backaudio = btaud;
    set_int_pref(PREF_MT_BACKAUDIO, prefs->mt_backaudio);
  } else {
    if (!prefs->mt_enter_prompt) {
      prefs->mt_enter_prompt = TRUE;
      set_boolean_pref(PREF_MT_ENTER_PROMPT, prefs->mt_enter_prompt);
    }
  }

  lives_widget_destroy(rdet->dialog);

  mt->user_width = rdet->width;
  mt->user_height = rdet->height;
  mt->user_fps = rdet->fps;
  mt->user_arate = xarate;
  mt->user_achans = xachans;
  mt->user_asamps = xasamps;
  mt->user_signed_endian = xse;

  lives_free(rdet->encoder_name);
  lives_freep((void **)&rdet);
  lives_freep((void **)&resaudw);

  msg = set_values_from_defs(mt, FALSE);
  if (msg != NULL) {
    d_print(msg);
    lives_free(msg);

    set_mt_title(mt);
  }

  if (cfile->achans == 0) {
    delete_audio_tracks(mt, mt->audio_draws, FALSE);
    mt->audio_draws = NULL;

    if (mt->amixer != NULL) on_amixer_close_clicked(NULL, mt);

    if (mt->audio_vols != NULL) lives_list_free(mt->audio_vols);
    mt->audio_vols = NULL;
  }

  set_interactive(mainw->interactive);

  scroll_tracks(mt, mt->top_track, FALSE);
}


uint32_t event_list_get_byte_size(lives_mt * mt, weed_plant_t *event_list, int *num_events) {
  // return serialisation size
  int i, j;
  uint32_t tot = 0;
  weed_plant_t *event = get_first_event(event_list);
  char **leaves;
  int ne;
  int tot_events = 0;

  // write extra bits in event_list
  save_event_list_inner(mt, -1, event_list, NULL);

  while (event != NULL) {
    if (WEED_EVENT_IS_FILTER_INIT(event)) {
      weed_leaf_delete(event, WEED_LEAF_EVENT_ID);
      weed_set_int64_value(event, WEED_LEAF_EVENT_ID, (uint64_t)((void *)event));
    }
    tot_events++;
    leaves = weed_plant_list_leaves(event, NULL);
    tot += sizint; //number of leaves
    for (i = 0; leaves[i] != NULL; i++) {
      tot += sizint * 3 + strlen(leaves[i]); // key_length, seed_type, num_elements
      ne = weed_leaf_num_elements(event, leaves[i]);
      // sum data_len + data
      for (j = 0; j < ne; j++) tot += sizint + weed_leaf_element_size(event, leaves[i], j);
      lives_free(leaves[i]);
    }
    lives_free(leaves);
    event = get_next_event(event);
  }

  event = event_list;
  leaves = weed_plant_list_leaves(event, NULL);
  tot += sizint;
  for (i = 0; leaves[i] != NULL; i++) {
    tot += sizint * 3 + strlen(leaves[i]);
    ne = weed_leaf_num_elements(event, leaves[i]);
    // sum data_len + data
    for (j = 0; j < ne; j++) tot += sizint + weed_leaf_element_size(event, leaves[i], j);
    lives_free(leaves[i]);
  }
  lives_free(leaves);

  if (num_events != NULL) *num_events = tot_events;
  return tot;
}


void on_amixer_close_clicked(LiVESButton * button, lives_mt * mt) {
  lives_amixer_t *amixer = mt->amixer;
  int i;
  double val;

  if (!mainw->interactive) return;

  mt->opts.gang_audio = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(amixer->gang_checkbutton));

  // set vols from slider vals

  for (i = 0; i < amixer->nchans; i++) {
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

  lives_general_button_clicked(button, NULL);
  lives_free(amixer->ch_sliders);
  lives_free(amixer->ch_slider_fns);
  lives_free(amixer);
  mt->amixer = NULL;
  if (mt->audio_vols_back != NULL) lives_list_free(mt->audio_vols_back);
  //lives_widget_set_sensitive(mt->prerender_aud,TRUE);
}


static void on_amixer_reset_clicked(LiVESButton * button, lives_mt * mt) {
  lives_amixer_t *amixer = mt->amixer;
  int i;

  if (!mainw->interactive) return;

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(amixer->inv_checkbutton), FALSE);
  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(amixer->gang_checkbutton), mt->opts.gang_audio);

  // copy vols to slider vals

  for (i = 0; i < amixer->nchans; i++) {
    float val = (float)LIVES_POINTER_TO_INT(lives_list_nth_data(mt->audio_vols_back, i)) / LIVES_AVOL_SCALE;
    if (0)
      val = lives_vol_to_linear(val);
#if ENABLE_GIW
    if (prefs->lamp_buttons) {
      lives_signal_handler_block(giw_vslider_get_adjustment(GIW_VSLIDER(amixer->ch_sliders[i])), amixer->ch_slider_fns[i]);
      giw_vslider_set_value(GIW_VSLIDER(amixer->ch_sliders[i]), val);
      lives_signal_handler_unblock(giw_vslider_get_adjustment(GIW_VSLIDER(amixer->ch_sliders[i])), amixer->ch_slider_fns[i]);
    } else {
#endif
      lives_signal_handler_block(lives_range_get_adjustment(LIVES_RANGE(amixer->ch_sliders[i])), amixer->ch_slider_fns[i]);
      lives_range_set_value(LIVES_RANGE(amixer->ch_sliders[i]), val);
      //lives_scale_add_mark(LIVES_SCALE(amixer->ch_sliders[i]), val, LIVES_POS_LEFT, NULL);
      //lives_scale_add_mark(LIVES_SCALE(amixer->ch_sliders[i]), val, LIVES_POS_RIGHT, NULL);
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
            lives_signal_handler_unblock(giw_vslider_get_adjustment(GIW_VSLIDER(amixer->ch_sliders[i])), amixer->ch_slider_fns[i]);
          } else {
#endif
            lives_signal_handler_block(lives_range_get_adjustment(LIVES_RANGE(amixer->ch_sliders[i])), amixer->ch_slider_fns[i]);
            lives_range_set_value(LIVES_RANGE(amixer->ch_sliders[i]), 1. - val);
            lives_signal_handler_unblock(lives_range_get_adjustment(LIVES_RANGE(amixer->ch_sliders[i])), amixer->ch_slider_fns[i]);
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

  amixer->ch_slider_fns[i] = lives_signal_connect_after(LIVES_GUI_OBJECT(adj), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                             LIVES_GUI_CALLBACK(on_amixer_slider_changed),
                             (livespointer)mt);

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
  gtk_spin_button_set_adjustment(LIVES_SPIN_BUTTON(spinbutton), LIVES_ADJUSTMENT(adj));

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

  int nachans = lives_list_length(mt->audio_draws);

  int winsize_h = GUI_SCREEN_WIDTH * 2. / 3.;
  int winsize_v = GUI_SCREEN_HEIGHT * 2. / 3.;

  int i;

  lives_amixer_t *amixer;

  if (!mainw->interactive) return;

  if (nachans == 0) return;

  mt->audio_vols_back = lives_list_copy(mt->audio_vols);

  amixer = mt->amixer = (lives_amixer_t *)lives_malloc(sizeof(lives_amixer_t));
  amixer->nchans = 0;

  amixer->ch_sliders = (LiVESWidget **)lives_malloc(nachans * sizeof(LiVESWidget *));
  amixer->ch_slider_fns = (ulong *)lives_malloc(nachans * sizeof(ulong));

  amixerw = lives_window_new(LIVES_WINDOW_TOPLEVEL);
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

  /* if (prefs->open_maximised) { */
  /*   lives_window_unmaximize(LIVES_WINDOW(amixerw)); */
  /*   lives_window_maximize(LIVES_WINDOW(amixerw)); */
  /* } else lives_window_set_default_size(LIVES_WINDOW(amixerw), winsize_h, winsize_v); */

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
                               LIVES_KEY_m, LIVES_CONTROL_MASK,
                               (LiVESAccelFlags)0);

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
    lives_signal_connect(LIVES_GUI_OBJECT(amixer->inv_checkbutton), LIVES_WIDGET_EXPOSE_EVENT,
                         LIVES_GUI_CALLBACK(draw_cool_toggle),
                         NULL);
#endif
    lives_widget_set_bg_color(amixer->inv_checkbutton, LIVES_WIDGET_STATE_ACTIVE, &palette->light_green);
    lives_widget_set_bg_color(amixer->inv_checkbutton, LIVES_WIDGET_STATE_NORMAL, &palette->dark_red);

    lives_signal_connect_after(LIVES_GUI_OBJECT(amixer->inv_checkbutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                               LIVES_GUI_CALLBACK(lives_cool_toggled),
                               NULL);

    lives_cool_toggled(amixer->inv_checkbutton, NULL);

  } else amixer->inv_checkbutton = lives_check_button_new();

  if (mt->opts.back_audio_tracks > 0 && mt->opts.pertrack_audio) {
    label = lives_standard_label_new_with_mnemonic_widget(_("_Invert backing audio\nand layer volumes"), amixer->inv_checkbutton);

    lives_widget_set_tooltip_text(amixer->inv_checkbutton, _("Adjust backing and layer audio values so that they sum to 1.0"));
    eventbox = lives_event_box_new();
    lives_tooltips_copy(eventbox, amixer->inv_checkbutton);
    lives_label_set_mnemonic_widget(LIVES_LABEL(label), amixer->inv_checkbutton);

    lives_container_add(LIVES_CONTAINER(eventbox), label);
    lives_signal_connect(LIVES_GUI_OBJECT(eventbox), LIVES_WIDGET_BUTTON_PRESS_EVENT,
                         LIVES_GUI_CALLBACK(label_act_toggle),
                         amixer->inv_checkbutton);

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
    lives_signal_connect(LIVES_GUI_OBJECT(amixer->gang_checkbutton), LIVES_WIDGET_EXPOSE_EVENT,
                         LIVES_GUI_CALLBACK(draw_cool_toggle),
                         NULL);
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
    lives_signal_connect(LIVES_GUI_OBJECT(eventbox), LIVES_WIDGET_BUTTON_PRESS_EVENT,
                         LIVES_GUI_CALLBACK(label_act_toggle),
                         amixer->gang_checkbutton);

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

  for (i = 0; i < nachans - mt->opts.back_audio_tracks; i++) {
    vbox = amixer_add_channel_slider(mt, i);
    lives_box_pack_start(LIVES_BOX(amixer->main_hbox), vbox, FALSE, FALSE, widget_opts.packing_width);
  }

  lives_signal_connect(LIVES_GUI_OBJECT(close_button), LIVES_WIDGET_CLICKED_SIGNAL,
                       LIVES_GUI_CALLBACK(on_amixer_close_clicked),
                       (livespointer)mt);

  lives_signal_connect(LIVES_GUI_OBJECT(reset_button), LIVES_WIDGET_CLICKED_SIGNAL,
                       LIVES_GUI_CALLBACK(on_amixer_reset_clicked),
                       (livespointer)mt);

  lives_widget_add_accelerator(close_button, LIVES_WIDGET_CLICKED_SIGNAL, accel_group,
                               LIVES_KEY_Escape, (LiVESXModifierType)0, (LiVESAccelFlags)0);

  lives_signal_connect_after(LIVES_GUI_OBJECT(amixer->gang_checkbutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                             LIVES_GUI_CALLBACK(after_amixer_gang_toggled),
                             (livespointer)amixer);
  lives_signal_connect_after(LIVES_GUI_OBJECT(amixer->gang_checkbutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                             LIVES_GUI_CALLBACK(lives_cool_toggled),
                             NULL);

  lives_cool_toggled(amixer->gang_checkbutton, NULL);
  after_amixer_gang_toggled(LIVES_TOGGLE_BUTTON(amixer->gang_checkbutton), amixer);

  lives_widget_grab_focus(close_button);

  on_amixer_reset_clicked(NULL, mt);

  lives_widget_show_all(amixerw);
}


void on_mt_showkeys_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  do_mt_keys_window();
}


LiVESWidget *get_eventbox_for_track(lives_mt * mt, int ntrack) {
  LiVESWidget *eventbox = NULL;
  if (mt != NULL) {
    if (mt_track_is_video(mt, ntrack)) {
      eventbox = (LiVESWidget *)lives_list_nth_data(mt->video_draws, ntrack);
    } else if (mt_track_is_audio(mt, ntrack)) {
      eventbox = (LiVESWidget *)lives_list_nth_data(mt->audio_draws, 1 - ntrack);
    }
  }
  return eventbox;
}


static track_rect *get_nth_block_for_track(lives_mt * mt, int itrack, int iblock) {
  int count = 0;
  track_rect *block;
  LiVESWidget *eventbox = get_eventbox_for_track(mt, itrack);
  if (eventbox == NULL) return NULL; //<invalid track
  block = (track_rect *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "blocks");
  while (block != NULL) {
    if (count == iblock) return block;
    block = block->next;
    count++;
  }

  return NULL; ///<invalid block
}


// remote API helpers

track_rect *find_block_by_uid(lives_mt * mt, ulong uid) {
  LiVESList *list;
  track_rect *block;

  if (mt == NULL || uid == 0l) return NULL;

  list = mt->video_draws;

  while (list != NULL) {
    block = (track_rect *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(list->data), "blocks");
    while (block != NULL) {
      if (block->uid == uid) return block;
      block = block->next;
    }
  }

  list = mt->audio_draws;

  while (list != NULL) {
    block = (track_rect *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(list->data), "blocks");
    while (block != NULL) {
      if (block->uid == uid) return block;
      block = block->next;
    }
  }

  return NULL;
}


boolean mt_track_is_video(lives_mt * mt, int ntrack) {
  if (ntrack >= 0 && mt->video_draws != NULL && ntrack < lives_list_length(mt->video_draws)) return TRUE;
  return FALSE;
}


boolean mt_track_is_audio(lives_mt * mt, int ntrack) {
  if (ntrack <= 0 && mt->audio_draws != NULL && ntrack >= -(lives_list_length(mt->audio_draws))) return TRUE;
  return FALSE;
}


ulong mt_get_last_block_uid(lives_mt * mt) {
  int track = mt->current_track;
  track_rect *lastblock;
  LiVESWidget *eventbox = get_eventbox_for_track(mt, track);
  if (eventbox == NULL) return 0l; //<invalid track
  lastblock = (track_rect *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "block_last");
  if (lastblock == NULL) return 0l; ///< no blocks in track
  return lastblock->uid;
}


int mt_get_block_count(lives_mt * mt, int ntrack) {
  int count = 0;
  track_rect *block, *lastblock;
  LiVESWidget *eventbox = get_eventbox_for_track(mt, ntrack);
  if (eventbox == NULL) return -1; //<invalid track
  lastblock = (track_rect *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "block_last");
  if (lastblock == NULL) return -1; ///< no blocks in track
  block = (track_rect *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(eventbox), "blocks");
  while (block != NULL) {
    if (block == lastblock) break;
    block = block->next;
    count++;
  }

  return count;
}


/// return time in seconds of first frame event in block
double mt_get_block_sttime(lives_mt * mt, int ntrack, int iblock) {
  track_rect *block = get_nth_block_for_track(mt, ntrack, iblock);
  if (block == NULL) return -1; ///< invalid track or block number
  return (double)get_event_timecode(block->start_event) / TICKS_PER_SECOND_DBL;
}


/// return time in seconds of last frame event in block, + event duration
double mt_get_block_entime(lives_mt * mt, int ntrack, int iblock) {
  track_rect *block = get_nth_block_for_track(mt, ntrack, iblock);
  if (block == NULL) return -1; ///< invalid track or block number
  return (double)get_event_timecode(block->end_event) / TICKS_PER_SECOND_DBL + 1. / mt->fps;
}


track_rect *get_block_from_track_and_time(lives_mt * mt, int track, double time) {
  LiVESWidget *ebox;
  if (mt == NULL) return NULL;
  ebox = get_eventbox_for_track(mt, track);
  return get_block_from_time(ebox, time, mt);
}


int get_clip_for_block(track_rect * block) {
  int track;
  if (block == NULL) return -1;
  track = get_track_for_block(block);
  return get_frame_event_clip(block->start_event, track);
}

////////////////////////////////////
// autotransitions
//


void mt_do_autotransition(lives_mt * mt, track_rect * block) {
  // prefs->atrans_track0 should be the output track (usually the lower of the two)

  track_rect *oblock = NULL;
  weed_timecode_t sttc, endtc = 0;

  weed_plant_t **ptmpls;
  weed_plant_t **oparams;

  weed_plant_t *stevent, *enevent;
  weed_plant_t *filter;
  weed_plant_t *ptm;
  weed_plant_t *old_mt_init = mt->init_event;

  LiVESList *slist;

  double region_start = mt->region_start;
  double region_end = mt->region_end;

  boolean did_backup = FALSE;

  int nvids = lives_list_length(mt->video_draws);
  int current_fx = mt->current_fx;

  int error;
  int tparam;
  int nparams;
  int param_hint;
  int track;

  register int i;

  if (block == NULL) return; ///<invalid block

  filter = get_weed_filter(prefs->atrans_fx);
  if (num_in_params(filter, TRUE, TRUE) == 0) return; ///<filter has no (visible) in parameters

  tparam = get_transition_param(filter, FALSE);
  if (tparam == -1) return; ///< filter has no transition parameter

  ptmpls = weed_get_plantptr_array(filter, WEED_LEAF_IN_PARAMETER_TEMPLATES, &error);
  ptm = ptmpls[tparam];
  param_hint = weed_get_int_value(ptm, WEED_LEAF_HINT, &error);

  mt->current_fx = prefs->atrans_fx;

  sttc = get_event_timecode(block->start_event);

  track = get_track_for_block(block);

  // part 1 - transition in

  slist = lives_list_copy(mt->selected_tracks);

  if (mt->selected_tracks != NULL) {
    lives_list_free(mt->selected_tracks);
    mt->selected_tracks = NULL;
  }

  for (i = 0; i < nvids; i++) {
    if (i == track) continue; ///< cannot transition with self !
    oblock = get_block_from_time((LiVESWidget *)lives_list_nth_data(mt->video_draws, i),
                                 (double)sttc / TICKS_PER_SECOND_DBL + 0.5 / mt->fps, mt);

    if (oblock != NULL) {
      if (get_event_timecode(oblock->end_event) <= get_event_timecode(block->end_event)) {
        endtc = q_gint64(get_event_timecode(oblock->end_event) + TICKS_PER_SECOND_DBL / mt->fps, mt->fps);
        break;
      } else oblock = NULL;
    }
  }

  mt->is_atrans = TRUE; ///< force some visual changes

  if (oblock != NULL) {
    mt->selected_tracks = lives_list_append(mt->selected_tracks, LIVES_INT_TO_POINTER(track));
    mt->selected_tracks = lives_list_append(mt->selected_tracks, LIVES_INT_TO_POINTER(i));

    mt_backup(mt, MT_UNDO_APPLY_FILTER, 0);

    mt->region_start = sttc / TICKS_PER_SECOND_DBL;
    mt->region_end = endtc / TICKS_PER_SECOND_DBL;
    mt_add_region_effect(NULL, mt);

    oparams = (weed_plant_t **)weed_get_voidptr_array_counted(mt->init_event, WEED_LEAF_IN_PARAMETERS, &nparams);

    for (i = 0; i < nparams; i++) {
      if (weed_get_int_value(oparams[i], WEED_LEAF_INDEX, &error) == tparam) break;
    }

    stevent = oparams[i];

    enevent = weed_plant_new(WEED_PLANT_EVENT);
    weed_set_int_value(enevent, WEED_LEAF_HINT, WEED_EVENT_HINT_PARAM_CHANGE);
    weed_set_int64_value(enevent, WEED_LEAF_TIMECODE, endtc);
    weed_set_int_value(enevent, WEED_LEAF_INDEX, tparam);

    weed_set_voidptr_value(enevent, WEED_LEAF_INIT_EVENT, mt->init_event);
    weed_set_voidptr_value(enevent, WEED_LEAF_NEXT_CHANGE, NULL);
    weed_set_voidptr_value(enevent, WEED_LEAF_PREV_CHANGE, stevent);
    //weed_add_plant_flags(enevent, WEED_LEAF_READONLY_PLUGIN);

    weed_set_voidptr_value(stevent, WEED_LEAF_NEXT_CHANGE, enevent);

    if (param_hint == WEED_HINT_INTEGER) {
      int min = weed_get_int_value(ptm, WEED_LEAF_MIN, &error);
      int max = weed_get_int_value(ptm, WEED_LEAF_MAX, &error);
      weed_set_int_value(stevent, WEED_LEAF_VALUE, i < track ? min : max);
      weed_set_int_value(enevent, WEED_LEAF_VALUE, i < track ? max : min);
    } else {
      double min = weed_get_double_value(ptm, WEED_LEAF_MIN, &error);
      double max = weed_get_double_value(ptm, WEED_LEAF_MAX, &error);
      weed_set_double_value(stevent, WEED_LEAF_VALUE, i < track ? min : max);
      weed_set_double_value(enevent, WEED_LEAF_VALUE, i < track ? max : min);
    }

    insert_param_change_event_at(mt->event_list, oblock->end_event, enevent);
    lives_free(oparams);
  }

  // part 2, check if there is a transition out

  oblock = NULL;
  endtc = q_gint64(get_event_timecode(block->end_event) + TICKS_PER_SECOND_DBL / mt->fps, mt->fps);

  if (mt->selected_tracks != NULL) {
    lives_list_free(mt->selected_tracks);
    mt->selected_tracks = NULL;
  }

  for (i = 0; i < nvids; i++) {
    if (i == track) continue; ///< cannot transition with self !
    oblock = get_block_from_time((LiVESWidget *)lives_list_nth_data(mt->video_draws, i),
                                 (double)endtc / TICKS_PER_SECOND_DBL + 0.5 / mt->fps, mt);

    if (oblock != NULL) {
      sttc = get_event_timecode(oblock->start_event);
      if (sttc < get_event_timecode(block->start_event)) oblock = NULL;
      else break;
    }
  }

  if (oblock != NULL) {
    mt->selected_tracks = lives_list_append(mt->selected_tracks, LIVES_INT_TO_POINTER(track));
    mt->selected_tracks = lives_list_append(mt->selected_tracks, LIVES_INT_TO_POINTER(i));

    mt_backup(mt, MT_UNDO_APPLY_FILTER, 0);

    mt->region_start = sttc / TICKS_PER_SECOND_DBL;
    mt->region_end = endtc / TICKS_PER_SECOND_DBL;
    mt_add_region_effect(NULL, mt);

    oparams = (weed_plant_t **)weed_get_voidptr_array_counted(mt->init_event, WEED_LEAF_IN_PARAMETERS, &nparams);

    for (i = 0; i < nparams; i++) {
      if (weed_get_int_value(oparams[i], WEED_LEAF_INDEX, &error) == tparam) break;
    }

    stevent = oparams[i];

    enevent = weed_plant_new(WEED_PLANT_EVENT);
    weed_set_int_value(enevent, WEED_LEAF_HINT, WEED_EVENT_HINT_PARAM_CHANGE);
    weed_set_int64_value(enevent, WEED_LEAF_TIMECODE, get_event_timecode(block->end_event));
    weed_set_int_value(enevent, WEED_LEAF_INDEX, tparam);

    weed_set_voidptr_value(enevent, WEED_LEAF_INIT_EVENT, mt->init_event);
    weed_set_voidptr_value(enevent, WEED_LEAF_NEXT_CHANGE, NULL);
    weed_set_voidptr_value(enevent, WEED_LEAF_PREV_CHANGE, stevent);
    //weed_add_plant_flags(enevent, WEED_LEAF_READONLY_PLUGIN);

    weed_set_voidptr_value(stevent, WEED_LEAF_NEXT_CHANGE, enevent);

    if (param_hint == WEED_HINT_INTEGER) {
      int min = weed_get_int_value(ptm, WEED_LEAF_MIN, &error);
      int max = weed_get_int_value(ptm, WEED_LEAF_MAX, &error);
      weed_set_int_value(stevent, WEED_LEAF_VALUE, i < track ? max : min);
      weed_set_int_value(enevent, WEED_LEAF_VALUE, i < track ? min : max);
    } else {
      double min = weed_get_double_value(ptm, WEED_LEAF_MIN, &error);
      double max = weed_get_double_value(ptm, WEED_LEAF_MAX, &error);
      weed_set_double_value(stevent, WEED_LEAF_VALUE, i < track ? max : min);
      weed_set_double_value(enevent, WEED_LEAF_VALUE, i < track ? min : max);
    }

    insert_param_change_event_at(mt->event_list, block->end_event, enevent);
    lives_free(oparams);
  }

  // crossfade audio
  if (mt->init_event != NULL && mt->opts.autocross_audio)
    weed_set_boolean_value(mt->init_event, WEED_LEAF_HOST_AUDIO_TRANSITION, WEED_TRUE);

  mt->is_atrans = FALSE;
  mt->region_start = region_start;
  mt->region_end = region_end;
  lives_list_free(mt->selected_tracks);
  mt->selected_tracks = lives_list_copy(slist);
  if (slist != NULL) lives_list_free(slist);
  mt->current_fx = current_fx;
  mt->init_event = old_mt_init;

  lives_free(ptmpls);

  mt->changed = mt->auto_changed = TRUE;
  mt->did_backup = did_backup;
}

