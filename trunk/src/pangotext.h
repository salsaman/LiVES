// pangotext.h
// text handling code
// (c) A. Penkov 2010
// pieces of code taken and modified from scribbler.c
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

// (c) G. Finch 2002 - 2015

#ifndef LIVES_PANGOTEXT_H
#define LIVES_PANGOTEXT_H

typedef enum {
  SUBTITLE_TYPE_NONE=0,
  SUBTITLE_TYPE_SRT,
  SUBTITLE_TYPE_SUB
} lives_subtitle_type_t;


// for future use
typedef struct {
  lives_colRGB24_t fg;
  lives_colRGB24_t bg;
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

weed_plant_t *render_text_to_layer(weed_plant_t *layer, const char *text, const char *fontname,
                                   double size, lives_text_mode_t mode, lives_colRGBA32_t *fg_col,
                                   lives_colRGBA32_t *bg_col, boolean center, boolean rising, double top);

LingoLayout *render_text_to_cr(LiVESWidget *widget, lives_painter_t *, const char *text, const char *fontname,
                               double size, lives_text_mode_t mode, lives_colRGBA32_t *fg_col, lives_colRGBA32_t *bg_col,
                               boolean center, boolean rising, double top, int start, int width, int height);

#endif

