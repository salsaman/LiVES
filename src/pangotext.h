// pangotext.h
// text handling code
// (c) A. Penkov 2010
// pieces of code taken and modified from scribbler.c
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

#ifndef LIVES_PANGOTEXT_H
#define LIVES_PANGOTEXT_H


typedef struct {
  FILE *tfile;
  char *text;
  double start_time;
  double end_time;
} lives_subtitle_t;




typedef enum {
  LIVES_TEXT_MODE_FOREGROUND_ONLY,
  LIVES_TEXT_MODE_FOREGROUND_AND_BACKGROUND,
  LIVES_TEXT_MODE_BACKGROUND_ONLY
} lives_text_mode_t;

char **get_font_list(void);

gboolean render_text_to_layer(weed_plant_t *layer, const char *text, const char *fontname,\
  double size, lives_text_mode_t mode, lives_colRGBA32_t *fg_col, lives_colRGBA32_t *bg_col,\
  gboolean center, gboolean rising, double top);

char *get_srt_text(FILE *pf, double xtime);

#endif

