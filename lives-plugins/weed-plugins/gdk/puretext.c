// puretext.c
// weed plugin
// (c) salsaman 2010 (contributions by A. Penkov)
// cloned and modified from livetext.c (author G. Finch aka salsaman)
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

///////////////////////////////////////////////////////////////////

static int package_version = 2; // version of this package

//////////////////////////////////////////////////////////////////

#define NEED_RANDOM
#define NEED_PALETTE_CONVERSIONS
#define NEED_FONT_UTILS

#include <pango/pango-font.h>

#define NEED_PANGO_COMPAT

#ifndef NEED_LOCAL_WEED_PLUGIN
#include <weed/weed-plugin.h>
#include <weed/weed-utils.h> // optional
#include <weed/weed-plugin-utils.h> // optional
#else
#include "../../../libweed/weed-plugin.h"
#include "../../../libweed/weed-utils.h" // optional
#include "../../../libweed/weed-plugin-utils.h" // optional
#endif

#include "../weed-plugin-utils.c" // optional

/////////////////////////////////////////////////////////////

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <fcntl.h>
#include <unistd.h>
#include <wchar.h>
#include <ctype.h>
#include <wctype.h>

#include <gdk/gdk.h>
#include <pango/pangocairo.h>

// defines for configure dialog elements
enum DlgControls {
  P_FILE = 0,
  P_RAND,
  P_MODE,
  P_SPEED,
  P_FONT,
  P_FONTSCALE,
  P_TEXTBUF,
  P_END
};

typedef struct {
  int red;
  int green;
  int blue;
} rgb_t;

typedef enum {
  TEXT_TYPE_ASCII, // 1 byte charset [DEFAULT]
  TEXT_TYPE_UTF8, //  1 - 4 byte charset
  TEXT_TYPE_UTF16, // 2 byte charset
  TEXT_TYPE_UTF32 // 4 byte charset
} pt_ttext_t;

typedef enum {
  PT_LETTER_MODE, // proctext() is called per letter xtext from start -> start+length-1
  PT_WORD_MODE, // proctext() is called with per word xtext from start -> start+length-1
  PT_LINE_MODE, // proctext() is called with per line xtext from start -> start+length-1
  PT_ALL_MODE // proctext called once with xtext pointing to sdata->text (or NULL if length is 0)
} pt_tmode_t;

typedef enum {
  PT_SPIRAL_TEXT = 0,
  PT_SPINNING_LETTERS,
  PT_LETTER_STARFIELD,
  PT_WORD_COALESCE,
  PT_TERMINAL
} pt_op_mode_t;

// for future use
#define NTHREADS 1

typedef struct {
  int size;

  double xpos;
  double ypos;
  double zpos;

  double xvel;
  double yvel;
  double zvel;

  double xaccel;
  double yaccel;
  double zaccel;

  double rot;
  double rotvel;
  double rotaccel;

  rgb_t colour;
  double alpha;
} pt_letter_data_t;

// static data per instance
typedef struct {
  int count; // proctext counter

  double timer; // time in seconds since first process call
  weed_timecode_t last_tc; // timecode of last process call
  double alarm_time; // pre-set alarm timecode [set with pt_set_alarm( this, delta) ]

  gboolean alarm; // event wake up

  off_t start; // start glyph (inclusive) in current string (0 based) for string/word/letter modes
  int64_t length; // length of substring in current string [0 to all] for string/word/letter modes

  pt_ttext_t text_type;

  int nstrings; // number of strings loaded excluding blanks
  int xnstrings; // number of strings loaded including blanks
  int cstring; // currently selected string
  int curstring; // currently selected string (continuation)
  size_t toffs; // offset in curstring (ASCII chars)
  size_t utoffs; // offset in curstring (utf8 chars)
  char **xstrings; // text loaded in from file
  char **strings; // pointers to non-blanks in xstrings
  size_t totalen; // total length in ASCII chars
  size_t totulen; // total length in utf8 chars
  size_t totalenwb; // total length in ASCII chars (with blank lines included)
  size_t totulenwb; // total length in utf8 chars (with blank lines included)
  size_t totalennb; // total length in ASCII chars (with blank lines excluded)
  size_t totulennb; // total length in utf8 chars (with blank lines excluded)
  char *text; // pointer to strings[cstring]
  size_t tlength; // length of current line in characters
  int wlength; // length of current line in words
  gboolean rndorder; // true to select random strings, otherwise they are selected in order
  gboolean allow_blanks; // TRUE to include blanks lines in text file / FALSE to ignore them

  gboolean use_file;

  // offsets of text in layer (0,0) == top left
  int x_text;
  int y_text;

  //
  pt_tmode_t tmode;

  rgb_t fg;

  double fg_alpha;

  int mode;

  double fontscale;
  double velocity;

  // generic variables
  double dbl1, dbl2, dbl3;

  int int1, int2, int3;

  gboolean bool1;

  // reserved for future
  //pthread_t xthread[NTHREADS];
  //pthread_mutex_t xmutex;

  // per glyph/mode private data
  pt_letter_data_t *letter_data;

  guchar *pixel_data;
} sdata_t;

typedef struct {
  size_t start;
  size_t length;
} pt_subst_t;

static int verbosity = WEED_VERBOSITY_ERROR;

static const char **fonts_available = NULL;
static int num_fonts_available = 0;

#define set_font_size(sz) _set_font_size(sdata, layout, font, (sz), (double)width)
#define set_font_family(ffam) _set_font_family(layout, font, ffam)
#define set_text(text) pango_layout_set_text(layout, (text), -1)
#define getlsize(w, h) _getlsize(layout, (w), (h))
#define set_alarm(millis) pt_set_alarm(sdata, (millis))
#define rotate_text(x, y, angle) _rotate_text(sdata, cairo, layout, (x), (y), (angle))
#define do_reset(millis) _do_reset(sdata, (millis))
#define getastring() _getastring(sdata)
#define char_equal(idx, c) _char_equal(sdata, (idx), (c))
#define letter_data_free() _letter_data_free(sdata)

#define xtext (*ztext)

static void configure_fonts(void) {
  PangoContext *ctx;

  num_fonts_available = 0;
  fonts_available = NULL;

  ctx = gdk_pango_context_get();
  if (ctx) {
    PangoFontMap *pfm = pango_context_get_font_map(ctx);
    if (pfm) {
      int num = 0;
      PangoFontFamily **pff = NULL;
      pango_font_map_list_families(pfm, &pff, &num);
      if (num > 0) {
        // we should reserve num+1 for a final NULL pointer
        fonts_available = (const char **)weed_calloc((num + 1), sizeof(char *));
        if (fonts_available) {
          num_fonts_available = num;
          for (int i = 0; i < num; ++i) {
            fonts_available[i] = strdup(pango_font_family_get_name(pff[i]));
          }
          fonts_available[num] = NULL;
        }
      }
      g_free(pff);
    }
    g_object_unref(ctx);
  }
}

static void cleanup_fonts(void) {
  if (num_fonts_available && fonts_available) {
    for (int i = 0; i < num_fonts_available; ++i) {
      weed_free((void *)fonts_available[i]);
    }
    weed_free((void *)fonts_available);
  }
  num_fonts_available = 0;
  fonts_available = NULL;
}

static int font_compare(const void *s1, const void *s2) {
  size_t sz1 = strlen(s1);
  size_t sz2 = strlen(s2);
  if (sz1 == sz2) {
    char *u1 = g_utf8_casefold(s1, sz1);
    char *u2 = g_utf8_casefold(s2, sz1);
    int ret = strcmp(u1, u2);
    g_free(u1); g_free(u2);
    return ret;
  }
  return 1;
}


#define STD_STRFUNCS

static inline char *stringdup(const char *s, size_t n) {
#ifdef STD_STRFUNCS
  return strndup(s, n);
#else
  char *ret;
  size_t len = strlen(s);
  if (len > n) len = n;
  ret = weed_calloc(len + 1, 1);
  weed_memcpy(ret, s, len);
  return ret;
#endif
}


static inline size_t utf8len(const char *text) {
  return mbstowcs(NULL, text, 0);
}


static inline size_t utf8offs(const char *text, size_t xoffs) {
  size_t toffs = 0;
  while (text[toffs] != '\0' && xoffs-- > 0) {
    toffs += mbtowc(NULL, &text[toffs], 4);
  }
  return toffs;
}


static inline gboolean _char_equal(sdata_t *sdata, int idx, const char *c) {
  if (sdata->text_type == TEXT_TYPE_ASCII) return (sdata->text[idx - 1] == *c);
  return !strncmp(&sdata->text[utf8offs(sdata->text, idx)], c, 1);
}


static size_t get_ascii_word_length(const char *text) {
  // get length in words (non-spaces)
  size_t toffs = 0;
  int count = 0;
  gboolean isaspace = TRUE;

  while (text[toffs] != '\0') {
    if (isspace(text[toffs])) isaspace = TRUE;
    else if (isaspace) {
      count++;
      isaspace = FALSE;
    }
    toffs++;
  }
  return count;
}


static size_t get_utf8_word_length(const char *text) {
  // get length in words (non-spaces)
  size_t toffs = 0;
  int count = 0;
  gboolean isaspace = TRUE;

  while (text[toffs] != '\0') {
    wchar_t pwc;
    toffs += mbtowc(&pwc, &text[toffs], 4);
    if (iswspace(pwc)) isaspace = TRUE;
    else if (isaspace) {
      count++;
      isaspace = FALSE;
    }
  }
  return count;
}


static void _getastring(sdata_t *sdata) {
  if (!sdata->rndorder) {
    sdata->cstring++;
    if ((!sdata->allow_blanks && sdata->cstring >= sdata->nstrings)
        || (sdata->allow_blanks && sdata->cstring >= sdata->xnstrings)) sdata->cstring = 0;
  } else {
    if (!sdata->allow_blanks)
      sdata->cstring = fastrand_int(sdata->nstrings - 1);
    else
      sdata->cstring = fastrand_int(sdata->xnstrings - 1);
  }

  if (sdata->allow_blanks) sdata->text = sdata->xstrings[sdata->cstring];
  else sdata->text = sdata->strings[sdata->cstring];

  sdata->curstring = sdata->cstring;
  sdata->toffs = sdata->utoffs = 0;

  if (sdata->text_type == TEXT_TYPE_ASCII) {
    sdata->tlength = strlen(sdata->text);
    sdata->wlength = get_ascii_word_length(sdata->text);
  } else {
    sdata->tlength = utf8len(sdata->text);
    sdata->wlength = get_utf8_word_length(sdata->text);
  }
}


static cairo_t *channel_to_cairo(sdata_t *sdata, weed_plant_t *channel) {
  // convert a weed channel to cairo
  // the channel shares pixel_data with cairo
  // so it should be copied before the cairo is destroyed

  // WEED_LEAF_WIDTH,WEED_LEAF_ROWSTRIDES and WEED_LEAF_CURRENT_PALETTE of channel may all change

  guchar *src, *dst, *pixel_data;

  cairo_surface_t *surf;
  cairo_t *cairo;
  cairo_format_t cform = CAIRO_FORMAT_ARGB32;

  int width = weed_channel_get_width(channel);
  int height = weed_channel_get_height(channel);
  int pal = weed_channel_get_palette(channel);
  int irowstride = weed_channel_get_stride(channel);
  int orowstride = cairo_format_stride_for_width(cform, width);
  int widthx = width * 4;

  src = (guchar *)weed_channel_get_pixel_data(channel);

  sdata->pixel_data = pixel_data = (guchar *)weed_calloc(height, orowstride);
  if (!pixel_data) return NULL;

  if (irowstride == orowstride) {
    weed_memcpy((void *)pixel_data, (void *)src, irowstride * height);
  } else {
    dst = pixel_data;
    for (int i = 0; i < height; i++) {
      weed_memcpy(&dst[orowstride * i], &src[irowstride * i], widthx);
      weed_memset(&dst[orowstride * i + widthx], 0, widthx - orowstride);
    }
  }

  if (weed_get_boolean_value(channel, WEED_LEAF_ALPHA_PREMULTIPLIED, NULL) == WEED_FALSE)
    alpha_premult(pixel_data, widthx, height, orowstride, pal, WEED_TRUE);

  surf = cairo_image_surface_create_for_data(pixel_data, cform, width, height, orowstride);

  if (!surf) {
    weed_free(pixel_data);
    sdata->pixel_data = NULL;
    return NULL;
  }

  cairo = cairo_create(surf);
  cairo_surface_destroy(surf);

  return cairo;
}


static void cairo_to_channel(cairo_t *cairo, weed_plant_t *channel) {
  // updates a weed_channel from a cairo_t
  cairo_surface_t *surface = cairo_get_target(cairo);
  cairo_format_t cform = CAIRO_FORMAT_ARGB32;
  guchar *src, *dst, *pixel_data = (guchar *)weed_channel_get_pixel_data(channel);
  int width = weed_channel_get_width(channel), widthx = width * 4;
  int height = weed_channel_get_height(channel);
  int irowstride, orowstride = weed_channel_get_stride(channel);
  int i;

  // flush to ensure all writing to the image was done
  cairo_surface_flush(surface);

  src = cairo_image_surface_get_data(surface);

  irowstride = cairo_format_stride_for_width(cform, width);

  if (irowstride == orowstride) {
    weed_memcpy((void *)pixel_data, (void *)src, irowstride * height);
  } else {
    dst = pixel_data;
    for (i = 0; i < height; i++) {
      weed_memcpy(&dst[orowstride * i], &src[irowstride * i], widthx);
      weed_memset(&dst[orowstride * i + widthx], 0, orowstride - widthx);
    }
  }

  if (weed_get_boolean_value(channel, WEED_LEAF_ALPHA_PREMULTIPLIED, NULL) == WEED_FALSE) {
    int pal = weed_channel_get_palette(channel);
    // un-premultiply the alpha
    alpha_premult(pixel_data, widthx, height, orowstride, pal, TRUE);
  }
  cairo_surface_finish(surface);
}


static pt_subst_t *get_nth_word_utf8(char *text, int idx) {
  // get nth word, return start (bytes) and length (bytes)
  // idx==0 for first word, etc

  size_t toffs = 0, xtoffs;
  gboolean isaspace = TRUE;

  pt_subst_t *subst = (pt_subst_t *)weed_malloc(sizeof(pt_subst_t));
  subst->start = 0;

  while (text[toffs] != '\0') {
    wchar_t pwc;
    xtoffs = mbtowc(&pwc, &text[toffs], 4);
    if (iswspace(pwc)) {
      if (idx == -1) {
        subst->length = toffs - subst->start;
        return subst;
      }
      isaspace = TRUE;
    } else if (isaspace) {
      if (--idx == -1) subst->start = toffs;
      isaspace = FALSE;
    }
    toffs += xtoffs;
  }

  subst->length = toffs - subst->start;
  return subst;
}


static pt_subst_t *get_nth_word_ascii(char *text, int idx) {
  // get nth word, return start (bytes) and length (bytes)
  // idx==0 for first word, etc

  size_t toffs = 0;
  gboolean isaspace = TRUE;

  pt_subst_t *subst = (pt_subst_t *)weed_malloc(sizeof(pt_subst_t));
  subst->start = 0;

  while (text[toffs] != '\0') {
    if (isspace(text[toffs])) {
      if (idx == -1) {
        subst->length = toffs - subst->start;
        return subst;
      }
      isaspace = TRUE;
    } else if (isaspace) {
      if (--idx == -1) subst->start = toffs;
      isaspace = FALSE;
    }
    toffs++;
  }

  subst->length = toffs - subst->start;
  return subst;
}


static inline void _getlsize(PangoLayout *layout, double *pw, double *ph) {
  // calculate width and height of layout
  PangoRectangle ink, logic;
  pango_layout_get_extents(layout, &ink, &logic);
  if (pw)
    *pw = ((double)logic.width) / PANGO_SCALE;
  if (ph)
    *ph = ((double)logic.height) / PANGO_SCALE;
}


static inline void pt_set_alarm(sdata_t *sdata, int delta) {
  sdata->alarm = FALSE;
  if (delta < 0) sdata->alarm_time = -1.;
  else sdata->alarm_time = sdata->timer + (double)delta / 1000.;
}


static inline void setxypos(double dwidth, double dheight, double x, double y, int *x_text, int *y_text) {
  // set top left corner offset given center point and glyph dimensions
  *x_text = x - dwidth / 2. + .5;
  *y_text = y - dheight / 2. + .5;
}


// set font size
static inline void _set_font_size(sdata_t *sdata, PangoLayout *layout, PangoFontDescription *font,
                                  double font_size, int width) {
  int fnt_size = (int)((font_size * sdata->fontscale * width + 2. * PANGO_SCALE) / (4. * PANGO_SCALE))
                 * (4 * PANGO_SCALE);
  pango_font_description_set_size(font, fnt_size);
  if (layout) {
    pango_layout_set_font_description(layout, font);
    pango_layout_context_changed(layout);
  }
}


static inline void _set_font_family(PangoLayout *layout, PangoFontDescription *font, const char *ffam) {
  pango_font_description_set_family(font, ffam);
  if (layout) {
    pango_layout_set_font_description(layout, font);
    pango_layout_context_changed(layout);
  }
}


static void anim_letter(sdata_t *sdata, pt_letter_data_t *ldt) {
  // update velocities and positions

  ldt->xvel += ldt->xaccel * sdata->velocity;
  ldt->yvel += ldt->yaccel * sdata->velocity;
  ldt->zvel += ldt->zaccel * sdata->velocity;
  ldt->rotvel += ldt->rotaccel * sdata->velocity;

  ldt->xpos += ldt->xvel * sdata->velocity;
  ldt->ypos += ldt->yvel * sdata->velocity;
  ldt->zpos += ldt->zvel * sdata->velocity;
  ldt->rot += ldt->rotvel * sdata->velocity;
}


static inline void colour_copy(rgb_t *col1, rgb_t *col2) {
  // copy col2 to col1
  weed_memcpy(col1, col2, sizeof(rgb_t));
}


static inline double rand_angle(void) {
  // return a random double between 0. and 2*M_PI
  return (double)rand() / (double)RAND_MAX * 2.*M_PI;
}


static inline int getrandi(int min, int max) {
  return rand() % (max - min) + min;
}


static inline pt_letter_data_t *letter_data_create(int len) {
  pt_letter_data_t *ldt = (pt_letter_data_t *)weed_calloc(sizeof(pt_letter_data_t), len);
  return ldt;
}


static inline void _letter_data_free(sdata_t *sdata) {
  weed_free(sdata->letter_data);
  sdata->letter_data = NULL;
}


static void _rotate_text(sdata_t *sdata, cairo_t *cairo, PangoLayout *layout, int x_center, int y_center, double radians) {
  cairo_translate(cairo, x_center, y_center);
  cairo_rotate(cairo, radians);
  cairo_translate(cairo, -x_center, -y_center);

  /* Inform Pango to re-layout the text with the new transformation */
  pango_cairo_update_layout(cairo, layout);
}


static inline void _do_reset(sdata_t *sdata, int millis) {
  sdata->start = sdata->length = 0;
  sdata->dbl1 = sdata->dbl2 = sdata->dbl3 = 0.;
  sdata->int1 = sdata->int2 = sdata->int3 = 0;
  if (millis > -1) set_alarm(millis);
}


static void proctext(sdata_t *sdata, weed_timecode_t tc, char *xtext, cairo_t *cairo, PangoLayout *layout,
                     PangoFontDescription *font, int width, int height) {
  pt_letter_data_t *ldt = NULL;
  double dwidth, dheight;
  double radX, radY;

  // this is called with sdata->count set to zero, then with increasing values of sdata->count
  // until it reaches sdata->length

  // sdata->text contains the entire line of text
  // xtext contains the current letter / word / line

  // useful functions here include:

  // void set_font_size(double size)
  // size (points) is rounded to the nearest multiple of 4, and applied to the layout

  // set_text(char *text) : sets the text to be displayed, eg. set_text(xtext), set_text("")

  // getlsize(double *width, double *height) : gets size in pixels of the current layout

  // void setxypos(double dwidth, double dheight, double x, double y, int *x_text, int *y_text)
  // calculates the top left position for test with pixel size dwidth X dheight centered at x, y
  // result is returned in x_text, y_text

  // set_alarm(millis)
  // sets a countdown timer for the desired number of milliseconds
  // sdata->alarm is set to non-zero ater the elapsed time (approximately)
  // (more accurate relative timings can be obtained from tc which is nominally 10 ^ -8 second units)
  // setting a negative value deactivates the alarm timer

  // getastring()
  // causes the next string (line) to be retrieved from the text file
  // if sdata->rndorder is set non-zero then the order will be randomised, otherwise lines are
  // provided in the same sequence as the text file

  // getrandi(start, end) : returns a pseudo-random integer in the range start, end

  // rotate_text(x, y, angle) : translates the origin x_pos, y_pos = (0, 0) to x, y and rotates the axes
  // rotates (turns) the layout by angle radians around the point x, y


  // setting the following affects the current layout (the current letter or word)

  // sdata->x_text, sdata->y_text // position of centre of token
  // setting it to 0. 0 places the top left of the token at the top left of the screen
  // the screen goes from -width / 2, -height / 2 (top left) to width /2, height / 2 (bottom right)

  // sdata->fg, sdata->fg_alpha set colour of the current token,
  // r, g, b values range from 0 - 255, but alpha range is 0. (fully transparent)
  // to 1. (fully opaque)

  // the process is additive, that is each increment of sdata->count adds to the existing layout

  switch (sdata->mode) {
  case (PT_WORD_COALESCE):

    if (sdata->timer == 0.) {
      sdata->start = -1;
      sdata->length = 1;
      sdata->tmode = PT_WORD_MODE;
    }

    if (sdata->start > -1) {
      set_font_size(128.);

      // get pixel size of word
      getlsize(&dwidth, &dheight);

      // align center of word at center of screen
      setxypos(dwidth, dheight, width / 2, height / 2, &sdata->x_text, &sdata->y_text);

      sdata->dbl1 -= sdata->dbl1 / 4. * sdata->velocity + .5; // blur shrink
      sdata->dbl2 += .07 * sdata->velocity;
      sdata->fg_alpha = sdata->dbl2;
    }

    if (sdata->alarm) {
      pt_subst_t *xsubst;
      char nxlast;

      sdata->bool1 = !sdata->bool1;
      sdata->dbl1 = width / 2.; // blur start
      sdata->fg_alpha = sdata->dbl2 = 0.;

      if (sdata->length > 0) {
        sdata->start++;
      } else {
        sdata->length = 1;
        sdata->bool1 = TRUE;
      }

      if (sdata->length > 0) {
        if (sdata->start == sdata->wlength - 1) {
          // time between each phrase
          do_reset(1000); // milliseconds
          getastring();
        } else {
          // peek at last char of next word
          if (sdata->text_type == TEXT_TYPE_ASCII) {
            xsubst = get_nth_word_ascii(sdata->text, sdata->start);
          } else {
            xsubst = get_nth_word_utf8(sdata->text, sdata->start);
          }

          nxlast = sdata->text[xsubst->start + xsubst->length - 1];

          weed_free(xsubst);

          // hold time
          if (nxlast == '.' || nxlast == '!' || nxlast == '?') set_alarm(4000); // milliseconds
          else if (nxlast == ',') set_alarm(2800); // milliseconds
          else if (nxlast == ';') set_alarm(1600); // milliseconds
          else set_alarm(1000); // milliseconds
        }
      }
    }

    break;

  case (PT_LETTER_STARFIELD):
    // LETTER_MODE:
    // this is called for each letter from sdata->start to sdata->start + sdata->length - 1
    // with sdata->count set to the current letter number

    // in letter mode, each letter has the following interal values:
    // xpos, ypos, zpos, rot, xvel, yvel, zvel, rotvel, xaccel, yaccel, zaccel, rotaccel
    // as well as size, colour and alpha

    // calling anim_letter will update the velocities and positions

    // it is then left to the programmer to map these values

    if (sdata->timer == 0.) {
      // set string start position and length
      sdata->start = 0;
      sdata->length = 0;
      sdata->tmode = PT_LETTER_MODE;

      // init letter data
      sdata->letter_data = letter_data_create(sdata->tlength);
    }

    if (sdata->length > 0) {
      double dist;
      ldt = &sdata->letter_data[sdata->count];
      dist = ldt->zpos;
      if (dist > 1.) {
        set_font_size(2048. / dist);

        // get pixel size of letter
        getlsize(&dwidth, &dheight);

        sdata->x_text = ldt->xpos / dist + (double)width / 2.;
        sdata->y_text = ldt->ypos / dist + (double)height / 2.;

        // position centre of letter
        setxypos(dwidth, dheight, sdata->x_text, sdata->y_text, &sdata->x_text, &sdata->y_text);

        // letters fade out as they approach
        sdata->fg_alpha = dist / 100. + .5;

        // update positions and velocities
        anim_letter(sdata, ldt);

        colour_copy(&sdata->fg, &ldt->colour);
      } else set_text("");
    }

    if (sdata->alarm) {
      // set values for next letter
      double angle = rand_angle();
      ldt = &sdata->letter_data[sdata->length];

      ldt->xpos = ldt->ypos = ldt->rot = ldt->rotvel = 0.;
      ldt->zpos = 80. + sdata->length;

      if (ldt->zpos > 100.) ldt->zpos = 100.;

      ldt->xvel = sin(angle) * (double)width / 20.;
      ldt->yvel = cos(angle) * (double)width / 20.;
      ldt->zvel = -8.;

      ldt->xaccel = ldt->yaccel = ldt->zaccel = ldt->rotaccel = 0.;

      if (sdata->length == 0 || char_equal(sdata->length, " ")) {
        ldt->colour.red = getrandi(60, 255);
        ldt->colour.green = getrandi(60, 255);
        ldt->colour.blue = getrandi(60, 255);
      } else {
        colour_copy(&ldt->colour, &(sdata->letter_data[sdata->length - 1].colour));
      }

      sdata->length++;

      if (sdata->length < sdata->tlength) set_alarm(400); // milliseconds
      else {
        // sets length back to zero
        do_reset(2000);
        getastring();
      }
    }

    break;

  case (PT_TERMINAL):
    // LETTER MODE
    // this is called for each letter from sdata->start to sdata->start + sdata->length - 1
    // letters are placed sequentially, a newline will incerement y and reset x
    // the letters gradually fade the further they are from the end.
    // a cursor is placed after the last letter

    if (sdata->timer == 0.) {
      sdata->tmode = PT_LETTER_MODE;
      sdata->dbl1 = sdata->dbl2 = sdata->dbl3 = 0.;
      sdata->int1 = sdata->int2 = sdata->int3 = 0;
      sdata->length = 0;
    }

    if (!sdata->count) {
      sdata->dbl1 = sdata->dbl2 = sdata->dbl3 = 0.;
    }

    if (sdata->length > 0) {

      set_font_size(10.);
      set_font_family("Monospace");

      sdata->fg.red = sdata->fg.blue = 0;
      sdata->fg.green = 255;

      // text fades as it moves away from cursor
      sdata->fg_alpha = 255. - 8. * (sdata->length - sdata->count);
      if (sdata->fg_alpha < 64.) sdata->fg_alpha = 64.;
      sdata->fg_alpha /= 255.;

      sdata->x_text = sdata->dbl1;
      sdata->y_text = sdata->dbl2;

      getlsize(&dwidth, &dheight);

      if (*xtext == '\n') {
        // newline counter
        if (!sdata->bool1) sdata->int1++;

        if (sdata->int1 > 2) {
          // refresh the "screen"
          // - set new start point at curstring / utoffs / toffs
          sdata->cstring = sdata->curstring;
          if (sdata->text_type == TEXT_TYPE_ASCII)
            sdata->start = sdata->toffs;
          else
            sdata->start = sdata->utoffs;

          // rest string length and text position
          sdata->length = 0;
          sdata->dbl1 = sdata->dbl2 = 0.;

          // int3 adjusts the length to EOF
          sdata->int3 += sdata->int2;
          sdata->int2 = 0;
        } else {
          // go down and back
          sdata->dbl1 = 0.;
          sdata->dbl2 += dheight / 2;
          if (sdata->count == sdata->length - 1) {
            if (sdata->int1 == 1 && !sdata->bool1) {
              sdata->bool1 = TRUE;
              set_alarm(128);
              sdata->utoffs--;
              sdata->toffs--;
            }
          }
        }
      } else {
        // place next letter
        sdata->dbl1 += dwidth;
        sdata->int1 = 0;
      }
      if (sdata->count == sdata->length - 1) {
        // append the cursor after the last char displayed
        size_t toffs = (*xtext == '\n') ? 0 : utf8offs(xtext, 1);
        char *cursor = weed_malloc(toffs + 4);
        if (toffs) weed_memcpy(cursor, xtext, toffs);
        weed_free(xtext);
        // 0x2588 -> 0010 0101 1000 1000 -> 1110 0010  10 010110 10 001000 -> 226 150 136
        cursor[toffs] = 226;
        cursor[toffs + 1] = 150;
        cursor[toffs + 2] = 136;
        cursor[toffs + 3] = 0;
        xtext = cursor;
      }
    }

    if (sdata->alarm) {
      sdata->bool1 = FALSE;

      // increase string length by one
      sdata->length++;

      // counts chars until a refresh
      sdata->int2++;

      if (sdata->text_type == TEXT_TYPE_ASCII) {
        if (sdata->length < sdata->totulen - sdata->int3 - 1) set_alarm(20);
        else if (sdata->length == sdata->totalen - sdata->int3) set_alarm(1000);
        else {
          sdata->count = -1;
          sdata->cstring = 0;
          sdata->toffs = 0;
          do_reset(2000);
        }
      } else {
        //if (sdata->length > sdata->totulen - sdata->int3) sdata->length = 0;
        if (sdata->length < sdata->totalen - sdata->int3) set_alarm(10);
        else if (sdata->length == sdata->totalen - sdata->int3) set_alarm(1000);
        else {
          sdata->count = -1;
          sdata->cstring = 0;
          sdata->utoffs = 0;
          do_reset(2000);
        }
      }
    }
    break;

  case (PT_SPINNING_LETTERS):
    // LETTER_MODE:
    // this is called for each letter from sdata->start to sdata->start + sdata->length - 1
    // with sdata->count set to the current letter number

    if (sdata->timer == 0.) {
      // select all text in line right away
      sdata->length = sdata->tlength;
      sdata->tmode = PT_LETTER_MODE;
      sdata->letter_data = letter_data_create(sdata->tlength);
    }

    ldt = &sdata->letter_data[sdata->count];

    if (!sdata->count) {
      sdata->dbl1 = 0.;
    }

    set_font_size(128.);

    if (sdata->length) {
      // get pixel size of letter/word, note the pango layout is not rotated until we actually draw it
      getlsize(&dwidth, &dheight);

      if (ldt->rotvel == 0.) {
        ldt->xpos = sdata->dbl1;
        ldt->rotvel = .1;
        ldt->xvel = -8.;
        sdata->dbl1 += dwidth + 10.;
      }

      // update positions and velocities
      anim_letter(sdata, ldt);

      if (sdata->count >= sdata->tlength - 1 && ldt->xpos <= -width - dwidth) {
        do_reset(1000);
      } else {
        sdata->x_text = width + ldt->xpos;
        sdata->y_text = height / 2;
        setxypos(0, dheight, sdata->x_text, sdata->y_text, &sdata->x_text, &sdata->y_text);
        rotate_text(sdata->x_text, sdata->y_text, ldt->rot);
      }
    }

    if (sdata->alarm) {
      getastring();
      letter_data_free();
      // must reset after calling getastring
      sdata->length = sdata->tlength;
      sdata->letter_data = letter_data_create(sdata->tlength);
    }
    break;

  case (PT_SPIRAL_TEXT):
    // LETTER_MODE:
    // this is called for each letter from sdata->start to sdata->start + sdata->length - 1
    // with sdata->count set to the current letter number

    if (sdata->timer == 0.) {
      sdata->int1 = 0;
      sdata->start = 0;
      sdata->tmode = PT_LETTER_MODE;
    }

    set_font_size(2560. / (sdata->count + 19.));

    if (sdata->length) {
      // get pixel size of letter/word
      getlsize(&dwidth, &dheight);

      // expansion factor
      if (sdata->int1 < 2) {
        sdata->dbl3 = 1.;
        sdata->bool1 = FALSE;
      } else sdata->dbl3 += .0001 * sdata->velocity;

      // set x_text, y_text
      if (!sdata->count) {
        sdata->dbl1 = radX = width * .45 * sdata->dbl3;
        sdata->dbl2 = radY = height * .45 * sdata->dbl3;
        sdata->bool1 = FALSE;
      } else {
        if (!sdata->bool1) {
          sdata->dbl1 = radX = sdata->dbl1 * .98;
          sdata->dbl2 = radY = sdata->dbl2 * .98;
        } else {
          sdata->dbl1 = radX = sdata->dbl1 * .97;
          sdata->dbl2 = radY = sdata->dbl2 * .97;
        }
      }

      if (sdata->bool1)
        setxypos(dwidth, dheight, width / 2 + sin(sdata->count / 2. + (sdata->dbl3 - 1.) * 16.) * radX,
                 height / 2 - cos(-sdata->count / 4. - (sdata->dbl3 - 1.) * 8.)
                 * radY, &sdata->x_text, &sdata->y_text);

      else
        setxypos(dwidth, dheight, width / 2 + sin(sdata->count / 4. + (sdata->dbl3 - 1.) * 8.) * radX,
                 height / 2 - cos(-sdata->count / 4. - (sdata->dbl3 - 1.) * 8.)
                 * radY, &sdata->x_text, &sdata->y_text);

      // check if word-token starts with "."
      if (*xtext == '.') sdata->int1++;
    }

    if (sdata->alarm) {
      set_alarm(80); // milliseconds to get next letter
      if (sdata->start + sdata->length < sdata->tlength) {
        // add an extra letter
        sdata->length++;
      } else {
        // no more letters...
        sdata->dbl1 = sdata->dbl1 * .97;
        sdata->dbl2 = radY = sdata->dbl2 * .97;
        sdata->dbl3 -= .0002;

        if (!sdata->bool1) {
          sdata->bool1 = TRUE;
          set_alarm(20000);
        } else {
          // shrink from head to tail
          sdata->length -= 2;
          sdata->start += 2;

          if (sdata->length <= 0) {
            // all letters gone - restart cycle
            do_reset(2000);
            getastring();
            sdata->bool1 = FALSE;
          } else set_alarm(40); // milliseconds (string disappearing)
        }
      }
    }
    break;
  } // end switch
}


static weed_error_t puretext_init(weed_plant_t *inst) {
  weed_plant_t **in_params = weed_get_in_params(inst, NULL);
  weed_plant_t *gui;
  sdata_t *sdata;
  char *buff;
  char xbuff[65536];
  char *textbuf = weed_param_get_value_string(in_params[P_TEXTBUF]);
  size_t b_read, ulen;
  gboolean erropen = FALSE;
  gboolean use_file = TRUE;
  int i, ii = 0, j = 0;
  int fd, canstart = 0;
  int mode;

  if (*textbuf) use_file = FALSE;

  // open file and read in text

  if (use_file) {
    char *textfile = weed_param_get_value_string(in_params[P_FILE]);
    weed_free(textbuf);
    if ((fd = open(textfile, O_RDONLY)) == -1) {
      erropen = TRUE;
      if (verbosity >= WEED_VERBOSITY_CRITICAL)
        fprintf(stderr, "Error opening file %s\n", textfile);
      weed_free(textfile);
      weed_free(in_params);
      return WEED_ERROR_FILTER_INVALID;
    }
    weed_free(textfile);
    buff = xbuff;
  } else buff = textbuf;

  sdata = (sdata_t *)weed_calloc(sizeof(sdata_t), 1);
  if (!sdata) {
    if (!use_file) weed_free(textbuf);
    weed_free(in_params);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }

  sdata->use_file = use_file;
  sdata->timer = -1;
  sdata->last_tc = 0;
  sdata->alarm_time = 0.;
  sdata->alarm = FALSE;

  sdata->pixel_data = NULL;

  sdata->text_type = TEXT_TYPE_UTF8;

  if (use_file && !erropen) {
    b_read = read(fd, buff, 65535);
    weed_memset(buff + b_read, 0, 1);
    close(fd);
  }
  if (!use_file) b_read = strlen(textbuf);

  sdata->xnstrings = 0;
  sdata->nstrings = 0;
  sdata->totalenwb = sdata->totalennb = 0;
  sdata->totulenwb = sdata->totulennb = 0;

  // parse the text
  for (i = 0; i < b_read; i++) {
    if ((uint8_t)buff[i] == 0x0A || (uint8_t)buff[i] == 0x0D) {
      sdata->xnstrings++;
      if (canstart < i) sdata->nstrings++;
      canstart = i + 1;
    }
  }
  if (canstart < i) {
    sdata->nstrings++;
    sdata->xnstrings++;
  }
  if (canstart == i) sdata->xnstrings++;

  if (sdata->nstrings == 0) {
    if (verbosity >= WEED_VERBOSITY_CRITICAL)
      fprintf(stderr, "No non-empty strings found in file.\n");
    weed_free(in_params);
    return WEED_ERROR_FILTER_INVALID;
  }

  // only non-empty strings
  sdata->strings = (char **)weed_calloc(sdata->nstrings, sizeof(char *));

  // all including empty
  sdata->xstrings = (char **)weed_calloc(sdata->xnstrings, sizeof(char *));

  canstart = 0;

  for (i = 0; i < sdata->xnstrings - 1; i++) {
    // parse the text
    for (; j < b_read; j++) {
      if ((uint8_t)buff[j] == 0x0A || (uint8_t)buff[i] == 0x0D) {
        if (canstart == j) {
          sdata->xstrings[i] = stringdup("", 0);
          ulen = 1;
        } else {
          sdata->xstrings[i] = stringdup(&buff[canstart], j - canstart);
          ulen = utf8len(sdata->xstrings[i]) + 1;
        }
        sdata->totulenwb += ulen;
        sdata->totalenwb += j - canstart + 1;
        if (canstart < j) {
          sdata->strings[ii++] = sdata->xstrings[i];
          sdata->totulennb += ulen;
          sdata->totalennb += j - canstart + 1;
        }
        canstart = ++j;
        break;
      }
    }
  }
  if (canstart < j) {
    sdata->xstrings[i] = stringdup(&buff[canstart], j - canstart);
    sdata->strings[ii] = sdata->xstrings[i];
    ulen = utf8len(sdata->xstrings[i]);
    sdata->totulenwb += ulen;
    sdata->totalenwb += j - canstart;
    sdata->totulennb += ulen;
    sdata->totalennb += j - canstart;
  } else {
    if (canstart == j) {
      sdata->xstrings[i] = stringdup("", 0);
    }
  }

  if (!use_file) weed_free(textbuf);

  sdata->start = sdata->length = 0;
  sdata->cstring = -1;
  sdata->text = NULL;
  sdata->rndorder = TRUE;

  sdata->int1 = sdata->int2 = sdata->int3 = 0;
  sdata->dbl1 = sdata->dbl2 = sdata->dbl3 = 0.;
  sdata->bool1 = FALSE;

  sdata->letter_data = NULL;

  sdata->mode = weed_param_get_value_int(in_params[P_MODE]);

  gui = weed_param_get_gui(in_params[P_RAND]);

  if (mode == PT_TERMINAL) weed_set_boolean_value(gui, WEED_LEAF_HIDDEN, WEED_TRUE);
  else weed_set_boolean_value(gui, WEED_LEAF_HIDDEN, WEED_FALSE);

  weed_set_voidptr_value(inst, "plugin_internal", sdata);

  weed_free(in_params);
  return WEED_SUCCESS;
}


static weed_error_t puretext_deinit(weed_plant_t *inst) {
  int i;
  sdata_t *sdata = (sdata_t *)weed_get_voidptr_value(inst, "plugin_internal", NULL);

  if (sdata) {
    if (sdata->letter_data) letter_data_free();
    for (i = 0; i < sdata->xnstrings; i++) weed_free(sdata->xstrings[i]);
    weed_free(sdata->xstrings);
    weed_free(sdata->strings);
    weed_free(sdata);
  }
  weed_set_voidptr_value(inst, "plugin_internal", NULL);
  return WEED_SUCCESS;
}


static weed_error_t puretext_process(weed_plant_t *inst, weed_timecode_t tc) {
  weed_plant_t *in_channel = weed_get_in_channel(inst, 0);
  weed_plant_t *out_channel = weed_get_out_channel(inst, 0);
  weed_plant_t **in_params = weed_get_in_params(inst, NULL);
  weed_plant_t *filter;

  sdata_t *sdata = (sdata_t *)weed_get_voidptr_value(inst, "plugin_internal", NULL);

  guchar *bgdata = NULL;

  pt_subst_t *xsubst;

  cairo_t *cairo;

  guchar *dst = (guchar *)weed_channel_get_pixel_data(out_channel);

  char *fontstr;

  int orowstride = weed_channel_get_stride(out_channel);
  int width = weed_channel_get_width(out_channel);
  int height = weed_channel_get_height(out_channel);
  int pal = weed_channel_get_palette(out_channel);
  int mode = weed_param_get_value_int(in_params[P_MODE]);
  int version;
  int i, j;

  //xtext = text = weed_param_get_value_string(in_params[P_TEXT]);

  sdata->fontscale = weed_param_get_value_double(in_params[P_FONTSCALE]);
  sdata->velocity = weed_param_get_value_double(in_params[P_SPEED]);

  filter = weed_instance_get_filter(inst);
  version = weed_filter_get_version(filter);
  if (!version) version = 1;

  if (version == 1) fontstr = strdup("Monospace");
  else fontstr = weed_param_get_value_string(in_params[P_FONT]);

  if (mode != sdata->mode) {
    sdata->timer = -1.;
    sdata->mode = mode;
    sdata->alarm_time = 0.;
    if (sdata->letter_data) letter_data_free();
  }

  if (mode == PT_TERMINAL) {
    // RND_ORDER_OFF
    // ALLOW_BLANKS
    sdata->rndorder = FALSE;
    sdata->allow_blanks = TRUE;
    if (sdata->timer == -1) sdata->cstring = 0;
    sdata->text = sdata->xstrings[sdata->cstring];
  } else {
    sdata->rndorder = (weed_param_get_value_boolean(in_params[P_RAND]) == WEED_TRUE);
    sdata->allow_blanks = FALSE;
  }

  weed_free(in_params); // must weed free because we got an array

  if (!sdata->allow_blanks) {
    sdata->totalen = sdata->totalennb;
    sdata->totulen = sdata->totulennb;
  } else {
    sdata->totalen = sdata->totalenwb;
    sdata->totulen = sdata->totulenwb;
  }

  sdata->curstring = sdata->cstring;

  // set timer data and alarm status
  if (sdata->timer == -1. || tc < sdata->last_tc) {
    sdata->timer = 0.;
    sdata->length = 0;
  } else {
    sdata->timer += (double)(tc - sdata->last_tc) / (double)WEED_TICKS_PER_SECOND * sdata->velocity;
    sdata->alarm = FALSE;
  }

  if (sdata->alarm_time >= 0. && sdata->timer >= sdata->alarm_time) {
    sdata->alarm_time = -1.;
    sdata->alarm = TRUE;
  }

  sdata->last_tc = tc;

  sdata->count = 0;

  if (sdata->mode == PT_WORD_COALESCE) {
    // backup original data
    bgdata = weed_malloc(height * orowstride);
    weed_memcpy(bgdata, dst, height * orowstride);
  }

  // convert channel pixel_data to a cairo surface
  if ((!in_channel) || (in_channel == out_channel))
    cairo = channel_to_cairo(sdata, out_channel);
  else
    cairo = channel_to_cairo(sdata, in_channel);

  if (!sdata->text) getastring();

  if (cairo) {
    PangoLayout *layout = NULL;
    PangoFontDescription *font;
    char *family;
    int fontnum = 0;

    // initialize byte and utf8 offsets
    // (only useful for PT_LETTER_MODE)
    if (sdata->text_type == TEXT_TYPE_ASCII) {
      sdata->toffs = sdata->start;
    } else {
      sdata->toffs = utf8offs(sdata->text, sdata->start);
      sdata->utoffs = sdata->start;
    }

    // default colour - opaque white
    sdata->fg.red = sdata->fg.green = sdata->fg.blue = 255.;
    sdata->fg_alpha = 1.;

    weed_parse_font_string(fontstr, &family, NULL, NULL, NULL, NULL);
    for (i = 0; i < num_fonts_available; ++i) {
      if (!font_compare((const void *)fonts_available[i], (const void *)family)) {
        fontnum = i;
        break;
      }
    }

    if (i == num_fonts_available) fontnum = -1;
    weed_free(family);
    weed_free(fontstr);

    font = pango_font_description_new();

    if (num_fonts_available && fontnum >= 0 && fontnum < num_fonts_available
        && fonts_available[fontnum]) {
      pango_font_description_set_family(font, fonts_available[fontnum]);
    } else pango_font_description_set_family(font, "Monospace");

    set_font_size(32.);

    // loop from start char to end char (note that the procedure can alter sdata->length
    // usually it would increase by 1 on each loop and then be reset to 0)
    for (i = sdata->start; i < sdata->start + (sdata->length == 0 ? 1 : sdata->length); i++) {
      // each letter or word gets its own layout which are combined
      layout = pango_cairo_create_layout(cairo);
      if (layout) {
        char *ztext[1], *otext;
        xtext = NULL; // xtext #defined as *ztext

        pango_layout_set_font_description(layout, font);

        if (!sdata->text) {
          // get a line of text if we don't have any
          getastring();
          sdata->curstring = sdata->cstring;
        }

        if (sdata->length == 0) {
          xtext = weed_malloc(1);
          weed_memset(xtext, 0, 1);
        } else {
          switch (sdata->tmode) {
          case PT_LETTER_MODE:
            // letter mode; the pango layout will contain just a single char (ascii or utf8)
            // we set this in xtext
            if (sdata->text_type == TEXT_TYPE_ASCII) {
              xtext = stringdup(&sdata->text[sdata->toffs], 1);
              sdata->toffs++;
            } else {
              if (sdata->text[sdata->toffs]) {
                int xlen = mbtowc(NULL, &sdata->text[sdata->toffs], 4);
                xtext = stringdup(&sdata->text[sdata->toffs], xlen);
                sdata->toffs += xlen;
                sdata->utoffs++;
              }
            }
            if (!xtext || !*xtext) {
              // here we reached the end of the string
              // we will set a newline and move to the next string in order
              xtext = strdup("\n");
              if (!sdata->allow_blanks) {
                if (++sdata->curstring >= sdata->nstrings) sdata->curstring = 0;
                sdata->text = sdata->strings[sdata->curstring];
              } else {
                if (++sdata->curstring >= sdata->xnstrings) sdata->curstring = 0;
                sdata->text = sdata->xstrings[sdata->curstring];
              }
              sdata->toffs = sdata->utoffs = 0;
            }
            break;

          case PT_WORD_MODE:
            // word mode; herte we pass in a "word" at a time
            if (sdata->text_type == TEXT_TYPE_ASCII) {
              xsubst = get_nth_word_ascii(sdata->text, i);
            } else {
              xsubst = get_nth_word_utf8(sdata->text, i);
            }
            xtext = stringdup(&sdata->text[xsubst->start], xsubst->length);
            weed_free(xsubst);
            break;
          default:
            // TODO - line mode and all mode
            xtext = weed_malloc(1);
            weed_memset(xtext, 0, 1);
            break;
          }
        }

        cairo_save(cairo);

        otext = xtext;
        set_text(xtext);

        // call the procedure to
        proctext(sdata, tc, ztext, cairo, layout, font, width, height);

        if (i < sdata->start + sdata->length) {
          cairo_move_to(cairo, sdata->x_text, sdata->y_text);

          if (pal == WEED_PALETTE_RGBA32) {
            cairo_set_source_rgba(cairo, sdata->fg.blue / 255.0, sdata->fg.green / 255.0,
                                  sdata->fg.red / 255.0, sdata->fg_alpha);
          } else {
            cairo_set_source_rgba(cairo, sdata->fg.red / 255.0, sdata->fg.green / 255.0,
                                  sdata->fg.blue / 255.0, sdata->fg_alpha);
          }
          if (xtext != otext) set_text(xtext);

          pango_cairo_show_layout(cairo, layout);
        }
        cairo_restore(cairo);

        if (layout) g_object_unref(layout);
        layout = NULL;
        if (xtext) weed_free(xtext);
        xtext = NULL;
      }

      sdata->count++;
    } // end loop

    if (font) pango_font_description_free(font);
    font = NULL;

    // now convert cairo surface to channel pixel_data
    cairo_to_channel(cairo, out_channel);
    if (sdata->pixel_data) {
      weed_free(sdata->pixel_data);
      sdata->pixel_data = NULL;
    }
    cairo_destroy(cairo);
  }

  // pixel level adjustments

  if (sdata->mode == PT_WORD_COALESCE) {
    if (sdata->dbl1 > 0.) {
      guchar *b_data = bgdata;
      int width4 = width * 4;
      guchar *dstx = dst = (guchar *)weed_channel_get_pixel_data(out_channel);

      for (i = 0; i < height; i++) {
        for (j = 0; j < width4; j += 4) {
          // find text by comparing current pixel_data with backup
          // if the values for a pixel differ. it must be part of the text
          // we move the pixel in a random direction with decreasing distance
          if (dst[j] != b_data[j] || dst[j + 1] != b_data[j + 1] || dst[j + 2] != b_data[j + 2]) {
            // move point by sdata->dbl1 pixels
            double angle = rand_angle();
            int x = j / 4 + sin(angle) * sdata->dbl1;
            int y = i + cos(angle) * sdata->dbl1;
            if (x > 0 && x < width && y > 0 && y < height) {
              // blur 1 pixel
              weed_memcpy(&dstx[y * orowstride + x * 4], &dst[j], 3);
              // protect blurred pixel
              if (y >= i) weed_memcpy(&bgdata[y * orowstride + x * 4], &dst[j], 3);
            }
            // replace original pixel
            weed_memcpy(&dst[j], &b_data[j], 3);
          }
        }
        dst += orowstride;
        b_data += orowstride;
      }
    }
    weed_free(bgdata);
  }

  return WEED_SUCCESS;
}


WEED_SETUP_START(200, 200) {
  char rfxstring0[256], rfxstring1[256];
  char *rfxstrings[] = {rfxstring0, rfxstring1};

  weed_plant_t *in_params[P_END + 1], *gui;
  weed_plant_t *filter_class;

  const char *modes[] = {"Spiral text", "Spinning letters", "Letter starfield", "Word coalesce",
                         "Terminal", NULL
                        };
  char *deftextfile;

  int palette_list[3];
  weed_plant_t *in_chantmpls[2];
  weed_plant_t *out_chantmpls[2];
  weed_plant_t *host_info = weed_get_host_info(plugin_info);
  int filter_flags = weed_host_supports_premultiplied_alpha(host_info)
                     ? WEED_FILTER_PREF_PREMULTIPLIED_ALPHA : 0;

  verbosity = weed_get_host_verbosity(host_info);

  if (is_big_endian()) {
    palette_list[0] = WEED_PALETTE_ARGB32;
    palette_list[1] = palette_list[2] = WEED_PALETTE_END;
  } else {
    palette_list[0] = WEED_PALETTE_RGBA32;
    palette_list[1] = WEED_PALETTE_BGRA32;
    palette_list[2] = WEED_PALETTE_END;
  }

  in_chantmpls[0] = weed_channel_template_init("in channel 0", 0);
  in_chantmpls[1] = NULL;

  out_chantmpls[0] = weed_channel_template_init("out channel 0", WEED_CHANNEL_CAN_DO_INPLACE);
  out_chantmpls[1] = NULL;

  init_unal();

  configure_fonts();

  deftextfile = g_build_filename(g_get_home_dir(), "livestext.txt", NULL);
  in_params[P_FILE] = weed_text_init("textfile", "_Text file", deftextfile);
  g_free(deftextfile);

  gui = weed_paramtmpl_get_gui(in_params[P_FILE]);
  weed_set_int_value(gui, WEED_LEAF_MAXCHARS, 80); // for display only - fileread will override this

  weed_paramtmpl_set_flags(in_params[P_FILE], weed_paramtmpl_get_flags(in_params[P_FILE])
                           | WEED_PARAMETER_REINIT_ON_VALUE_CHANGE);

  in_params[P_RAND] = weed_switch_init("rand", "Select text lines _randomly", WEED_TRUE);

  in_params[P_MODE] = weed_string_list_init("mode", "Effect _mode", 0, modes);

  weed_paramtmpl_set_flags(in_params[P_MODE], weed_paramtmpl_get_flags(in_params[P_MODE])
                           | WEED_PARAMETER_REINIT_ON_VALUE_CHANGE);

  in_params[P_SPEED] = weed_float_init("velocity", "_Speed multiplier (higher is faster)", 1., .1, 10.);

  in_params[P_FONT] = weed_text_init("fontname", "_Font Name", "");

  in_params[P_FONTSCALE] = weed_float_init("fontscale", "_Font Scale", 1., .1, 10.);

  in_params[P_TEXTBUF] = weed_text_init("textbuffer", "_Text buffer (overrides Text File)", "");

  in_params[P_END] = NULL;

  filter_class = weed_filter_class_init("puretext", "Salsaman & Aleksej Penkov", 1,
                                        filter_flags, palette_list,
                                        puretext_init, puretext_process, NULL,
                                        in_chantmpls, out_chantmpls, in_params, NULL);

  // GUI section

  snprintf(rfxstrings[0], 256, "special|fileread|%d|", P_FILE);
  snprintf(rfxstrings[1], 256, "special|fontchooser|%d|", P_FONT);

  gui = weed_filter_get_gui(filter_class);
  weed_set_string_value(gui, WEED_LEAF_LAYOUT_SCHEME, "RFX");
  weed_set_string_value(gui, "layout_rfx_delim", "|");
  weed_set_string_array(gui, "layout_rfx_strings", 2, rfxstrings);

  ////

  weed_plugin_info_add_filter_class(plugin_info, filter_class);

  weed_plugin_set_package_version(plugin_info, package_version);
}
WEED_SETUP_END;


WEED_DESETUP_START {
  // clean up what we reserve for font family names
  cleanup_fonts();
}
WEED_DESETUP_END;
