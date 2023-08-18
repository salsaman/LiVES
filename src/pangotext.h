// pangotext.h
// text handling code
// (c) A. Penkov 2010
// pieces of code taken and modified from scribbler.c
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

// (c) G. Finch 2002 - 2015

#ifndef LIVES_PANGOTEXT_H
#define LIVES_PANGOTEXT_H

char *pango_text_strip_markup(const char *text);

#define SUB_OPACITY 20480 // TODO

typedef enum {
  LIVES_TEXT_MODE_FOREGROUND_ONLY,
  LIVES_TEXT_MODE_FOREGROUND_AND_BACKGROUND,
  LIVES_TEXT_MODE_BACKGROUND_ONLY,
  LIVES_TEXT_MODE_PRECALCULATE
} lives_text_mode_t;

#define DEF_OVERLAY_SCALING 48.

void reset_font_size(void);

char **get_font_list(void);
void guess_font_size(LiVESWidget *window, LiVESLabel *xlabel, LiVESLabel *ylabel, double guess);

weed_layer_t *render_text_overlay(weed_layer_t *layer, const char *text, double scaling);

weed_layer_t *render_text_to_layer(weed_layer_t *layer, const char *text, const char *fontname,
                                   double size, lives_text_mode_t mode, lives_colRGBA64_t *fg_col,
                                   lives_colRGBA64_t *bg_col, boolean center, boolean rising, double top);

LingoLayout *render_text_to_cr(LiVESWidget *widget, lives_painter_t *, const char *text, const char *fontname,
                               double size, lives_text_mode_t mode, lives_colRGBA64_t *fg_col, lives_colRGBA64_t *bg_col,
                               boolean center, boolean rising, double *top, int *start, int dwidth, int *dheight);

void layout_to_lives_painter(LingoLayout *layout, lives_painter_t *cr, lives_text_mode_t mode, lives_colRGBA64_t *fg,
                             lives_colRGBA64_t *bg, int dwidth, int dheight, double x_bg, double y_bg, double x_text, double y_text);

LingoLayout *layout_nth_message_at_bottom(int n, int width, int height, LiVESWidget *widget, int *linecount);

/// subtitles

#define SRT_DEF_CHARSET "Windows-1252"
#define LIVES_CHARSET_UTF8 "UTF-8"

typedef struct _lives_clip_t lives_clip_t;

boolean subtitles_init(lives_clip_t *sfile, char *fname, lives_subtitle_type_t);
void subtitles_free(lives_clip_t *sfile);
boolean get_subt_text(lives_clip_t *sfile, double xtime);
boolean save_sub_subtitles(lives_clip_t *sfile, double start_time, double end_time, double offset_time, const char *filename);
boolean save_srt_subtitles(lives_clip_t *sfile, double start_time, double end_time, double offset_time, const char *filename);
void render_subs_from_file(lives_clip_t *, double xtime, lives_layer_t *);

boolean lives_parse_font_string(const char *string, char **font, char **family, int *size, char **stretch,
                                char **style, char **weight);

#endif

