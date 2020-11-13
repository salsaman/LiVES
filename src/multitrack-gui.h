// multitrack-gui.h
// LiVES
// (c) G. Finch 2005 - 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

// multitrack window

LiVESPixbuf *mt_make_thumb(lives_mt *, int file, int width, int height, frames_t frame, LiVESInterpType interp,
                           boolean noblanks);

void reset_mt_play_sizes(lives_mt *);

void mt_set_cursor_style(lives_mt *, lives_cursor_t cstyle, int width, int height, int clip, int hsx, int hsy);

void mt_draw_block(lives_mt *, lives_painter_t *, lives_painter_surface_t *, track_rect *, int x1, int x2);

void mt_draw_aparams(lives_mt *, LiVESWidget *eventbox, lives_painter_t *, LiVESList *param_list,
                     weed_event_t *init_event, int startx, int width);

void mt_redraw_eventbox(lives_mt *, LiVESWidget *eventbox);

void mt_redraw_all_event_boxes(lives_mt *);

void mt_tl_move_relative(lives_mt *, double pos_rel);

int get_top_track_for(lives_mt *, int track);

void mt_set_time_scrollbar(lives_mt *);

void mt_paint_line(lives_mt *);
void mt_paint_lines(lives_mt *, double currtime, boolean unpaint, lives_painter_t *);

//void unpaint_line(lives_mt *, LiVESWidget *eventbox);
void mt_unpaint_lines(lives_mt *);

boolean on_mt_timeline_scroll(LiVESWidget *, LiVESXEventScroll *, livespointer user_data);

double get_time_from_x(lives_mt *mt, int x);

void mt_update_timecodes(lives_mt *, double dtime);

void on_seltrack_activate(LiVESMenuItem *, livespointer mt);

void mt_set_in_out_spin_ranges(lives_mt *mt, weed_timecode_t start_tc, weed_timecode_t end_tc);

