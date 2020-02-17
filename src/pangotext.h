// pangotext.h
// text handling code
// (c) A. Penkov 2010
// pieces of code taken and modified from scribbler.c
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

// (c) G. Finch 2002 - 2015

#ifndef LIVES_PANGOTEXT_H
#define LIVES_PANGOTEXT_H

#define SUB_OPACITY 20480 // TODO

typedef enum {
  SUBTITLE_TYPE_NONE = 0,
  SUBTITLE_TYPE_SRT,
  SUBTITLE_TYPE_SUB
} lives_subtitle_type_t;

// for future use
typedef struct {
  lives_colRGB48_t fg;
  lives_colRGB48_t bg;
} lives_subtitle_style_t;

typedef struct _lives_subtitle_t xlives_subtitle_t;

typedef struct _lives_subtitle_t {
  double start_time;
  double end_time;
  lives_subtitle_style_t *style; ///< for future use
  long textpos;
  xlives_subtitle_t *prev; ///< for future use
  xlives_subtitle_t *next;
} lives_subtitle_t;

typedef struct {
  lives_subtitle_type_t type;
  FILE *tfile;
  char *text;
  double last_time;
  lives_subtitle_t *index;
  lives_subtitle_t *current; ///< pointer to current entry in index
  int offset; ///< offset in frames (default 0)
} lives_subtitles_t;

typedef enum {
  LIVES_TEXT_MODE_FOREGROUND_ONLY,
  LIVES_TEXT_MODE_FOREGROUND_AND_BACKGROUND,
  LIVES_TEXT_MODE_BACKGROUND_ONLY
} lives_text_mode_t;

char **get_font_list(void);

weed_plant_t *render_text_overlay(weed_plant_t *layer, const char *text);

weed_plant_t *render_text_to_layer(weed_plant_t *layer, const char *text, const char *fontname,
                                   double size, lives_text_mode_t mode, lives_colRGBA64_t *fg_col,
                                   lives_colRGBA64_t *bg_col, boolean center, boolean rising, double top);

LingoLayout *render_text_to_cr(LiVESWidget *widget, lives_painter_t *, const char *text, const char *fontname,
                               double size, lives_text_mode_t mode, lives_colRGBA64_t *fg_col, lives_colRGBA64_t *bg_col,
                               boolean center, boolean rising, double top, int start, double dwidth, double dheight);

void layout_to_lives_painter(LingoLayout *layout, lives_painter_t *cr, lives_text_mode_t mode, lives_colRGBA64_t *fg,
                             lives_colRGBA64_t *bg, int dwidth, int dheight, double x_bg, double y_bg, double x_text, double y_text);

LingoLayout *layout_nth_message_at_bottom(int n, int width, int height, LiVESWidget *widget, int *linecount);

/// subtitles

typedef struct _lives_clip_t lives_clip_t;

boolean subtitles_init(lives_clip_t *sfile, char *fname, lives_subtitle_type_t);
void subtitles_free(lives_clip_t *sfile);
boolean get_srt_text(lives_clip_t *sfile, double xtime);
boolean get_sub_text(lives_clip_t *sfile, double xtime);
boolean save_sub_subtitles(lives_clip_t *sfile, double start_time, double end_time, double offset_time, const char *filename);
boolean save_srt_subtitles(lives_clip_t *sfile, double start_time, double end_time, double offset_time, const char *filename);

#endif

