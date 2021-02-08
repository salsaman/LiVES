// puretext.c
// weed plugin
// (c) salsaman 2010 (contributions by A. Penkov)
// cloned and modified from livetext.c (author G. Finch aka salsaman)
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

///////////////////////////////////////////////////////////////////

static int package_version = 3; // version of this package

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

#define MAX_BUFFLEN 65536

// defines for configure dialog elements
enum {
  P_FILE = 0,
  P_RANDLINES,
  P_MODE,
  P_TRIGGER,
  P_SPEED,
  P_FONT,
  P_FONTSCALE,
  P_USEBUF,
  P_TEXTBUF,
  P_RANDMODE,
  P_END,
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
  PT_TERMINAL,
  PT_WORD_SLIDE,
  PT_BOUNCE,
  PT_END
} pt_op_mode_t;

typedef struct _letter_data_t pt_letter_data_t;

struct _letter_data_t {
  char *text;
  double width, height;

  int group;
  int phase;

  gboolean setup;
  gboolean inactive;

  double fontsize;
  double fontsize_change;

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

  double orbit, orbitvel, orbitaccel, orbitstart;  // angular part
  double orbitx, orbity; // center

  double targetx, targety, targetmass;

  rgb_t colour;
  double alpha;
  double alpha_change;

  pt_letter_data_t *prev;
  pt_letter_data_t *next;
};

// static data per instance
typedef struct {
  int count; // proctext counter

  gboolean init; // set to TRUE for the first call

  // timer internal
  double timer; // time in seconds since first process call
  weed_timecode_t last_tc; // timecode of last process call
  double alarm_time; // pre-set alarm timecode [set with pt_SET_ALARM( this, delta) ]
  gboolean alarm; // event wake up

  int randmode;

  pt_ttext_t text_type; // current charset

  char *textbuf; // holds additional text
  char *ltext; // current layout text (may be NULL until needed to get size)

  int nstrings; // number of strings loaded excluding blanks
  int xnstrings; // number of strings loaded including blanks
  int cstring; // currently selected string
  int curstring; // currently selected string (continuation)
  size_t toffs; // offset in curstring (ASCII chars)
  size_t utoffs; // offset in curstring (utf8 chars)
  char **xstrings; // text loaded in from file
  char **strings; // pointers to non-blanks in xstrings
  size_t totlen; // total length in current charset
  size_t totalen; // total length in ASCII chars
  size_t totulen; // total length in utf8 chars
  size_t totalenwb; // total length in ASCII chars (with blank lines included)
  size_t totulenwb; // total length in utf8 chars (with blank lines included)
  size_t totalennb; // total length in ASCII chars (with blank lines excluded)
  size_t totulennb; // total length in utf8 chars (with blank lines excluded)
  char *text; // pointer to strings[cstring]
  size_t tlength; // length of current line in characters
  int wlength; // length of current line in words

  gboolean use_file;

  gboolean needsmore;
  gboolean refresh;

  //
  pt_tmode_t tmode;

  // readonly
  int mode;
  double fontscale;
  double velocity;
  guchar *pixel_data;

  gboolean trigger, last_trigger, autotrigger;

  // read / write state variables

  gboolean rndorder; // true to select random strings, otherwise they are selected in order
  gboolean rndorder_set; // whether rhe module set it off
  gboolean allow_blanks; // TRUE to include blanks lines in text file / FALSE to ignore them

  off_t start; // start glyph (inclusive) in current string (0 based) for string/word/letter modes
  int64_t length; // length of substring in current string [0 to all] for string/word/letter modes

  // generic state variables
  char string1[512], string2[512], string3[512], string4[512], string5[512];
  double dbl1, dbl2, dbl3, dbl4, dbl5;
  int int1, int2, int3, int4, int5;
  gboolean bool1, bool2, bool3;

  // per glyph/mode private data
  pt_letter_data_t *letter_data;
  int nldt;

  // offsets of text in layer (0,0) == top left
  int x_text, y_text;

  rgb_t fg;
  double fg_alpha;

  cairo_antialias_t antialias;

  double dissolve; // pixel op

  int variant; // mini modes within main mode

  int phase; // for sequencing

  int funmode;

  // store the random seed at init time, then when we select a new mode we can
  // repeat consistently
  uint64_t orig_seed;
} sdata_t;

typedef struct {
  size_t start;
  size_t length;
  size_t ustart;
  size_t ulength;
} pt_subst_t;

static int verbosity = WEED_VERBOSITY_ERROR;

static const char **fonts_available = NULL;
static int num_fonts_available = 0;

#define TWO_PI (M_PI * 2.)

#define DEF_FONT "Monospace"

// internal
#define set_ptext(text) pango_layout_set_text(layout, (text), -1)

// module accessible functions
#define IS_A_SPACE(c) _is_a_space(sdata, (c))
#define IS_A_SPACE_AT(idx) _is_a_space_at(sdata, (idx))

#define SET_FONT_FAMILY(ffam) _set_font_family(sdata, layout, font, (ffam))
#define SET_FONT_SIZE(sz) _set_font_size(inst, sdata, layout, font, (sz), (double)width)
#define SET_FONT_WEIGHT(weight) _set_font_weight(sdata, layout, font, PANGO_WEIGHT_##weight)
#define SET_OPERATOR(op) cairo_set_operator(cairo, CAIRO_OPERATOR_##op)
#define SET_TEXT(txt) _set_text(sdata, layout, (txt), ztext)
#define GETLSIZE(w, h) _getlsize(sdata, layout, (w), (h), ztext)

#define SET_ALARM(millis) _set_alarm(sdata, (millis))

#define DO_RESET() _do_reset(inst, sdata, ztext, layout)
#define GETASTRING(perm) _getastring(inst, sdata, (perm), (!sdata->rndorder && !sdata->init), \
				     (sdata->init == 1))
#define RESTART_AT_STRING(idx) _restart_at_string(inst, sdata, (idx), ztext)

#define CHAR_EQUAL(tok, c) _char_equal(sdata, (tok), (c))

#define LETTER_DATA_INIT(len) _letter_data_create(sdata, (len))
#define LETTER_DATA(idx) (sdata->letter_data && sdata->nldt > idx && idx >= 0 ? \
			  &sdata->letter_data[(idx)] : NULL)
#define LETTER_DATA_FREE() _letter_data_free(sdata)
#define LETTER_DATA_EXTEND(len) _letter_data_extend(sdata, (len))

#define ANIM_LETTER(ldt) _anim_letter(sdata, (ldt))
#define ROTATE_TEXT(x, y, angle) _rotate_text(sdata, cairo, layout, (x), (y), (angle))

#define GET_LAST_CHAR(strg) (_get_last_char(sdata, (strg)))
#define FIT_SIZE(max, width, tw, height, th) _fit_size(inst, sdata, ztext, layout, font, \
						       (max), (width), (tw), (height), (th), width)

#define IS_OFFSCREEN(x, y, dw, dh) (_is_offscreen((x), (y), (dw), (dh), width, height))
#define IS_ALL_ONSCREEN(x, y, dw, dh) (_is_all_onscreen((x), (y), (dw), (dh), width, height))

#define SET_STRING(str, txt) snprintf((str), 512, "%s", (txt))

#define SET_ANTIALIAS(type) _set_antialias(sdata, (type))
#define SET_RANDOM_ORDER(val) _set_random_order(sdata, (val))
#define SET_MODE(mode) sdata->tmode = (mode)
#define SET_ALLOW_BLANKS(mode) sdata->allow_blanks = (mode)

#define FASTRAND_DBL(drange) (fastrand_dbl_re((drange), inst, WEED_LEAF_PLUGIN_RANDOM_SEED))
#define FASTRAND_INT(irange) (fastrand_int_re((irange), inst, WEED_LEAF_PLUGIN_RANDOM_SEED))
#define GETRANDI(min, max) (_getrandi(inst, (min), (max)))
#define RAND_COL(ldt) _rand_col(inst, (ldt))
#define RAND_ANGLE() _rand_angle(inst)

#define ADJUST_RGBA(ldt, rmin, gmin, bmin, amin, amt) _adjust_rgba(inst, sdata, ldt, (rmin), \
								   (gmin), (bmin), (amin), (amt))

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


static int HAS_FONT_FAMILY(const char *family) {
  int fontnum = -1;
  for (int i = 0; i < num_fonts_available; ++i) {
    if (!font_compare((const void *)fonts_available[i], (const void *)family)) {
      fontnum = i;
      break;
    }
  }
  return fontnum;
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


static inline int utf8reset(void) {
  return mbtowc(NULL, NULL, 0);
}


static inline size_t utf8offs(const char *text, size_t xoffs) {
  size_t toffs = 0;
  utf8reset();
  while (text[toffs] != '\0' && xoffs-- > 0) {
    toffs += mbtowc(NULL, &text[toffs], 4);
  }
  return toffs;
}


static inline gboolean _char_equal(sdata_t *sdata, const char *s, const char *c) {
  if (sdata->text_type == TEXT_TYPE_ASCII) return (*s == *c);
  wchar_t pwc1, pwc2;
  mbtowc(&pwc1, s, 4);
  mbtowc(&pwc2, c, 4);
  return pwc1 == pwc2;
}


static gboolean _is_a_space(sdata_t *sdata, const char *txt) {
  if (sdata->text_type == TEXT_TYPE_ASCII) return isspace(txt);
  else {
    wchar_t pwc;
    mbtowc(&pwc, txt, 4);
    return (iswspace(pwc));
  }
}


static gboolean _is_a_space_at(sdata_t *sdata, int idx) {
  if (sdata->text_type == TEXT_TYPE_ASCII) {
    return isspace(sdata->text[idx]);
  } else {
    wchar_t pwc;
    size_t utoffs = utf8offs(sdata->text, idx);
    mbtowc(&pwc, &sdata->text[utoffs], 4);
    return (iswspace(pwc));
  }
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


static void _getastring(weed_plant_t *inst, sdata_t *sdata, gboolean perm,
                        gboolean sequential, gboolean reset) {
  int cstring = sdata->cstring;
  if (!perm) sdata->cstring = sdata->curstring;

  if ((!sdata->allow_blanks && sdata->cstring >= sdata->nstrings)
      || (sdata->allow_blanks && sdata->cstring >= sdata->xnstrings)) {
    sdata->cstring = 0;
    if (!sdata->use_file) {
      sdata->refresh = TRUE;
      return;
    }
  }

  if (!reset) {
    if (sequential) {
      sdata->cstring++;
      if ((!sdata->allow_blanks && sdata->cstring >= sdata->nstrings)
          || (sdata->allow_blanks && sdata->cstring >= sdata->xnstrings)) {
        sdata->cstring = 0;
        if (!sdata->use_file) {
          sdata->refresh = TRUE;
          return;
        }
      }
    } else {
      if (sdata->rndorder) {
        if (!sdata->allow_blanks)
          sdata->cstring = fastrand_int_re(sdata->nstrings - 1, inst, WEED_LEAF_PLUGIN_RANDOM_SEED);
        else
          sdata->cstring = fastrand_int_re(sdata->xnstrings - 1, inst, WEED_LEAF_PLUGIN_RANDOM_SEED);
      }
    }
  }

  if (sdata->allow_blanks) sdata->text = sdata->xstrings[sdata->cstring];
  else sdata->text = sdata->strings[sdata->cstring];

  sdata->curstring = sdata->cstring;
  sdata->toffs = sdata->utoffs = 0;

  if (sdata->text_type == TEXT_TYPE_ASCII) {
    sdata->tlength = strlen(sdata->text);
    sdata->wlength = get_ascii_word_length(sdata->text);
  } else {
    utf8reset();
    sdata->tlength = utf8len(sdata->text);
    utf8reset();
    sdata->wlength = get_utf8_word_length(sdata->text);
    utf8reset();
  }
  if (sdata->allow_blanks) sdata->tlength++;
  if (!perm) sdata->cstring = cstring;
}


static void _set_random_order(sdata_t *sdata, gboolean val) {
  if (!val) {
    sdata->cstring = 0;
    sdata->rndorder_set = TRUE;
    sdata->rndorder = val;
  } else sdata->rndorder_set = FALSE;
}


static void _restart_at_string(weed_plant_t *inst, sdata_t *sdata, int strnum, char **ztext) {
  sdata->cstring = strnum;
  xtext = strdup("");
  sdata->count = sdata->length + 1;
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
  cairo_font_options_t *ftopts = cairo_font_options_create();

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
  cairo_get_font_options(cairo, ftopts);
  cairo_font_options_set_antialias(ftopts, sdata->antialias);
  cairo_set_font_options(cairo, ftopts);
  cairo_surface_destroy(surf);

  return cairo;
}


static inline double _rand_angle(weed_plant_t *inst) {
  // return a random double between 0. and 2 * M_PI
  return fastrand_dbl_re(TWO_PI, inst, WEED_LEAF_PLUGIN_RANDOM_SEED);
}


#define AUTO_TRG_DISSOLVE 15.

static gboolean do_pixel_ops(weed_plant_t *inst, sdata_t *sdata, int width, int height, int orowstride,
                             int irowstride, int irowstride2, guchar *dst,
                             guchar *src, guchar *src2) {
  gboolean add = FALSE;
  double dissolve;
  // pixel level adjustments
  if (!sdata->dissolve && sdata->trigger && sdata->autotrigger) {
    dissolve = AUTO_TRG_DISSOLVE;
    add = TRUE;
  } else dissolve = sdata->dissolve;

  if (dissolve > 0.) {
    int width4 = width * 4;
    int i, j;

    // backup original data
    if (irowstride2 == orowstride) weed_memcpy(dst, src2, orowstride * height);
    else {
      for (i = 0; i < height; i++) {
        weed_memcpy(&dst[orowstride * i], &src2[irowstride2 * i], width4);
      }
    }

    for (i = 0; i < height; i++) {
      for (j = 0; j < width4; j += 4) {
        // find text by comparing src pixel_data with backup
        // if the values for a pixel differ. it must be part of the text
        // we move the pixel in a random direction in dst
        if (src[irowstride * i + j] != src2[irowstride2 * i + j] ||
            src[irowstride * i + j + 1] != src2[irowstride2 * i + j + 1]
            || src[irowstride * i + j + 2] != src2[irowstride2 * i + j + 2]) {
          // move point by sdata->dissolve pixels
          double angle = RAND_ANGLE();
          int x = j / 4 + sin(angle) * dissolve;
          int y = i + cos(angle) * dissolve;
          if (x > 0 && x < width && y > 0 && y < height) {
            // blur 1 pixel
            // this is not quite correct due to alpha blending
            weed_memcpy(&dst[orowstride * y + x * 4], &src[irowstride * i + j], 3);
            if (add) weed_memcpy(&dst[orowstride * i + j], &src[irowstride * i + j], 4);
          }
        }
      }
    }
    return TRUE;
  }
  return FALSE;
}


static void cairo_to_channel(weed_plant_t *inst, sdata_t *sdata, cairo_t *cairo, weed_plant_t *channel,
                             weed_plant_t *in_channel) {
  // updates a weed_channel from a cairo_t
  cairo_surface_t *surface = cairo_get_target(cairo);
  cairo_format_t cform = CAIRO_FORMAT_ARGB32;
  guchar *src, *dst = (guchar *)weed_channel_get_pixel_data(channel);
  guchar *src2 = (guchar *)weed_channel_get_pixel_data(in_channel);
  int width = weed_channel_get_width(channel), widthx = width * 4;
  int height = weed_channel_get_height(channel);
  int irowstride, orowstride = weed_channel_get_stride(channel);
  int irowstride2 = weed_channel_get_stride(in_channel);

  // flush to ensure all writing to the image was done
  cairo_surface_flush(surface);

  src = cairo_image_surface_get_data(surface);

  irowstride = cairo_format_stride_for_width(cform, width);

  if (!do_pixel_ops(inst, sdata, width, height, orowstride, irowstride, irowstride2, dst, src, src2)) {
    if (irowstride == orowstride) {
      weed_memcpy((void *)dst, (void *)src, irowstride * height);
    } else {
      for (int i = 0; i < height; i++) {
        weed_memcpy(&dst[orowstride * i], &src[irowstride * i], widthx);
        weed_memset(&dst[orowstride * i + widthx], 0, orowstride - widthx);
      }
    }
  }

  if (weed_get_boolean_value(channel, WEED_LEAF_ALPHA_PREMULTIPLIED, NULL) == WEED_FALSE) {
    int pal = weed_channel_get_palette(channel);
    // un-premultiply the alpha
    alpha_premult(dst, widthx, height, orowstride, pal, TRUE);
  }

  cairo_surface_finish(surface);
}


static pt_subst_t *get_nth_word_utf8(sdata_t *sdata, char *text, int idx) {
  // get nth word, return start (bytes) and length (bytes)
  // idx==0 for first word, etc

  // if EOS is reached, length will be zero

  size_t toffs = 0, xtoffs, utoffs = 0;
  gboolean isaspace = TRUE;

  pt_subst_t *subst = (pt_subst_t *)weed_malloc(sizeof(pt_subst_t));
  utf8reset();

  if (idx < 0) {
    idx = -idx - 1;
    toffs = sdata->toffs;
    utoffs = sdata->utoffs;
  }
  subst->start = toffs;
  subst->ustart = utoffs;

  while (text[toffs]) {
    wchar_t pwc;
    xtoffs = mbtowc(&pwc, &text[toffs], 4);
    if (iswspace(pwc)) {
      if (idx == -1) {
        subst->length = toffs - subst->start;
        subst->ulength = utoffs - subst->ustart;
        return subst;
      }
      isaspace = TRUE;
    } else if (isaspace) {
      if (--idx == -1) {
        subst->ustart = utoffs;
        subst->start = toffs;
      }
      isaspace = FALSE;
    }
    utoffs++;
    toffs += xtoffs;
  }
  subst->ulength = utoffs - subst->ustart;
  subst->length = toffs - subst->start;
  if (subst->ulength < 0) subst->ulength = 0;
  if (subst->length < 0) subst->length = 0;
  return subst;
}


static pt_subst_t *get_nth_word_ascii(sdata_t *sdata, char *text, int idx) {
  // get nth word, return start (bytes) and length (bytes)
  // idx==0 for first word, etc

  size_t toffs = 0;
  gboolean isaspace = TRUE;

  pt_subst_t *subst = (pt_subst_t *)weed_malloc(sizeof(pt_subst_t));

  if (idx < 0) {
    idx = -idx - 1;
    toffs = sdata->toffs;
  }

  subst->start = toffs;

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
  if (subst->length < 0) subst->length = 0;
  return subst;
}


static inline void _set_text(sdata_t *sdata, PangoLayout *layout, const char *txt, char **ztext) {
  char *ntext = strdup(txt);
  weed_free(xtext);
  xtext = ntext;
  if (sdata->ltext) {
    weed_free(sdata->ltext);
    sdata->ltext = NULL;
  }
}


static inline void _getlsize(sdata_t *sdata, PangoLayout *layout, double *pw, double *ph,
                             char **ztext) {
  // calculate width and height of layout
  PangoRectangle ink, logic;
  if (!sdata->ltext) {
    set_ptext(xtext);
    sdata->ltext = strdup(xtext);
  }
  pango_layout_get_extents(layout, &ink, &logic);
  if (pw) {
    *pw = (double)logic.width / PANGO_SCALE;
  }
  if (ph) {
    *ph = (double)ink.height;
    if (*ph == 0.) *ph = (double)logic.height;
    else {
      // for some reason, the logic height is doubled sometimes
      if ((double)logic.height > *ph * 200.) *ph = (double)logic.height / 2.;
      else if ((double)logic.height > *ph) *ph = (double)logic.height;
    }
    *ph /= PANGO_SCALE;
    *ph = *ph - 1;
  }
}


static inline void _set_alarm(sdata_t *sdata, int delta) {
  sdata->alarm = FALSE;
  if (delta < 0) sdata->alarm_time = -1.;
  else sdata->alarm_time = sdata->timer + (double)delta / 1000.;
}


static inline void SET_CENTER(double dwidth, double dheight, double x, double y, int *x_text, int *y_text) {
  // set top left corner offset given center point and glyph dimensions
  *x_text = x - dwidth / 2. + .5;
  *y_text = y - dheight / 2. + .5;
}


static inline gboolean _is_offscreen(double xpos, double ypos, double dwidth, double dheight,
                                     double width, double height) {
  return (xpos + (dwidth == 0. ? 1. : dwidth) <= 0. || xpos >= width
          || ypos + dheight <= 0. || ypos >= height);
}


static inline gboolean _is_all_onscreen(double xpos, double ypos, double dwidth, double dheight,
                                        double width, double height) {
  return (xpos >= 0. && xpos + dwidth < width && ypos >= 0. && ypos + dheight < height);
}


static inline void _set_font_weight(sdata_t *sdata, PangoLayout *layout, PangoFontDescription *font,
                                    PangoWeight weight) {
  pango_font_description_set_weight(font, weight);
  if (layout) {
    pango_layout_set_font_description(layout, font);
  }
  if (sdata->ltext) {
    weed_free(sdata->ltext);
    sdata->ltext = NULL;
  }
}


// set font size (scaled for canvas width
static inline void _set_font_size(weed_plant_t *inst, sdata_t *sdata, PangoLayout *layout,
                                  PangoFontDescription *font,
                                  double font_size, int width) {
  int fnt_size;
  if (sdata->trigger && sdata->autotrigger)
    fnt_size *= (fastrand_dbl_re(.1, inst, WEED_LEAF_PLUGIN_RANDOM_SEED) + .95);
  fnt_size = (int)((font_size * sdata->fontscale * width + 2. * PANGO_SCALE) / (4. * PANGO_SCALE) + .5)
             * (4 * PANGO_SCALE);
  pango_font_description_set_size(font, fnt_size);
  if (sdata->trigger && sdata->autotrigger)
    pango_font_description_set_weight(font, PANGO_WEIGHT_BOLD);
  else
    pango_font_description_set_weight(font, PANGO_WEIGHT_NORMAL);
  if (layout) {
    pango_layout_set_font_description(layout, font);
  }
  if (sdata->ltext) {
    weed_free(sdata->ltext);
    sdata->ltext = NULL;
  }
}


static inline void _set_font_family(sdata_t *sdata, PangoLayout *layout, PangoFontDescription *font, const char *ffam) {
  pango_font_description_set_family(font, ffam);
  if (layout) {
    pango_layout_set_font_description(layout, font);
  }
  if (sdata->ltext) {
    weed_free(sdata->ltext);
    sdata->ltext = NULL;
  }
}


static void _anim_letter(sdata_t *sdata, pt_letter_data_t *ldt) {
  // update velocities and positions

  //if (sdata->trigger && sdata->autotrigger) sdata->velocity = -sdata->velocity;
  if (ldt->targetmass) {
    double dx = ldt->targetx - ldt->xpos;
    double dy = ldt->targety - ldt->ypos;
    //double dist = sqrt(dx * dx + dy * dy);
    double dist = dx * dx + dy * dy;
    if (dist > 1.) {
      double force = ldt->targetmass / dist;
      ldt->xaccel = force * dx;
      ldt->yaccel = force * dy;
    } else {
      ldt->xaccel = ldt->yaccel = 0.;
    }
  }

  ldt->xvel += ldt->xaccel * sdata->velocity;
  ldt->yvel += ldt->yaccel * sdata->velocity;
  ldt->zvel += ldt->zaccel * sdata->velocity;
  ldt->rotvel += ldt->rotaccel * sdata->velocity;
  ldt->orbitvel += ldt->orbitaccel * sdata->velocity;

  ldt->xpos += ldt->xvel * sdata->velocity;
  ldt->ypos += ldt->yvel * sdata->velocity;
  ldt->zpos += ldt->zvel * sdata->velocity;
  ldt->rot += ldt->rotvel * sdata->velocity;
  ldt->orbit += ldt->orbitvel * sdata->velocity;

  ldt->fontsize += ldt->fontsize_change * sdata->velocity;
  ldt->alpha += ldt->alpha_change * sdata->velocity;
  if (ldt->alpha > 1.) ldt->alpha = 1.;
  if (ldt->alpha < 0.) ldt->alpha = 0.;

  //if (sdata->trigger && sdata->autotrigger) sdata->velocity = -sdata->velocity;
}


static inline void COLOUR_COPY(rgb_t *col1, rgb_t *col2) {
  // copy col2 to col1
  weed_memcpy(col1, col2, sizeof(rgb_t));
}


static inline int _getrandi(weed_plant_t *inst, int min, int max) {
  return fastrand_int_re(max - min, inst, WEED_LEAF_PLUGIN_RANDOM_SEED) + min;
}


#define AA(type) CAIRO_ANTIALIAS_##type

static inline void _set_antialias(sdata_t *sdata, const char *level) {
  if (!strcmp(level, "NONE"))
    sdata->antialias = AA(NONE);
  else if (!strcmp(level, "FAST"))
    sdata->antialias = AA(FAST);
  else
    sdata->antialias = AA(GOOD);
}


#define CLAMP_BOUNCE(min, max, val) ((val) > (max) ? ((max) - ((val) - (max)) % ((max) - (min) + 1)) \
				     : (val) < (min) ? ((min) + ((min) - (val)) % ((max) - (min) + 1)) \
				     : (val))

static inline void _adjust_rgba(weed_plant_t *inst, sdata_t *sdata, pt_letter_data_t *ldt,
                                int rmin, int gmin,
                                int bmin, double alphamin, double amt) {
  double rup = amt * 2.;
  double rndd;
  amt = 1. - amt;
  rndd = FASTRAND_DBL(rup) + amt;
  ldt->colour.red *= rndd;
  ldt->colour.red = CLAMP_BOUNCE(rmin, 255, ldt->colour.red);
  rndd = FASTRAND_DBL(rup) + amt;
  ldt->colour.green *= rndd;
  ldt->colour.green = CLAMP_BOUNCE(gmin, 255, ldt->colour.green);
  rndd = FASTRAND_DBL(rup) + amt;
  ldt->colour.blue *= rndd;
  ldt->colour.blue = CLAMP_BOUNCE(bmin, 255, ldt->colour.blue);
  rndd = FASTRAND_DBL(.01) - .005 + 1.;
  ldt->alpha *= rndd * 1000.;
  ldt->alpha = (double)CLAMP_BOUNCE((int)(alphamin * 1000.), 1000, (int)ldt->alpha) / 1000.;
  COLOUR_COPY(&sdata->fg, &ldt->colour);
  sdata->fg_alpha = ldt->alpha;
}


static inline pt_letter_data_t *_letter_data_extend(sdata_t *sdata, int len) {
  int extnd = sdata->nldt + len;
  sdata->letter_data = (pt_letter_data_t *)weed_realloc(sdata->letter_data,
                       sizeof(pt_letter_data_t) * extnd);
  weed_memset(&sdata->letter_data[sdata->nldt], 0, len * sizeof(pt_letter_data_t));
  sdata->nldt += len;
  return sdata->letter_data;
}


static inline void _letter_data_free(sdata_t *sdata) {
  pt_letter_data_t *ldt;
  if ((ldt = sdata->letter_data) != NULL) {
    for (int i = 0; i < sdata->nldt; i++) if (ldt[i].text) weed_free(ldt[i].text);
    weed_free(sdata->letter_data);
    sdata->letter_data = NULL;
    sdata->nldt = 0;
  }
}


static inline pt_letter_data_t *_letter_data_create(sdata_t *sdata, int len) {
  //fprintf(stderr, "DOING %d X %ld\n",  len, sizeof(pt_letter_data_t));
  if (sdata->letter_data) _letter_data_free(sdata);
  sdata->letter_data = (pt_letter_data_t *)weed_calloc(sizeof(pt_letter_data_t),  len);
  sdata->nldt = len;
  return sdata->letter_data;
}


static void _rotate_text(sdata_t *sdata, cairo_t *cairo, PangoLayout *layout, int x_center, int y_center, double radians) {
  cairo_translate(cairo, x_center, y_center);
  cairo_rotate(cairo, radians);
  cairo_translate(cairo, -x_center, -y_center);
}



static inline void _do_reset(weed_plant_t *inst, sdata_t *sdata, char **ztext,
                             PangoLayout *layout) {
  sdata->start = 0;
  sdata->length = 1;
  sdata->dbl1 = sdata->dbl2 = sdata->dbl3 = sdata->dbl4 = sdata->dbl5 = 0.;
  sdata->int1 = sdata->int2 = sdata->int3 = sdata->int4 = sdata->int5 = 0;
  sdata->bool1 = sdata->bool2 = sdata->bool3 = FALSE;
  *sdata->string1 = *sdata->string2 = *sdata->string3 = *sdata->string4 = *sdata->string5 = 0;
  sdata->init = -1;
  sdata->phase = 0;
  if (!ztext) {
    if (sdata->randmode) {
      do {
        sdata->funmode = FASTRAND_INT(PT_END - 1);
      } while (sdata->funmode == PT_TERMINAL);
    }
  } else {
    _getastring(inst, sdata, TRUE, !sdata->rndorder, FALSE);
    if (sdata->randmode) {
      sdata->funmode = sdata->mode;
    }
  }
}

static const char *_get_last_char(sdata_t *sdata, const char *strg) {
  if (sdata->text_type != TEXT_TYPE_ASCII) {
    size_t lc = utf8len(strg);
    return strg + utf8offs(strg, lc - 1);
  }
  return strg + strlen(strg) - 1;
}


static void _rand_col(weed_plant_t *inst, pt_letter_data_t *ldt) {
  ldt->colour.red = _getrandi(inst, 60, 255);
  ldt->colour.green = _getrandi(inst, 60, 255);
  ldt->colour.blue = _getrandi(inst, 60, 255);
}


static double _fit_size(weed_plant_t *inst, sdata_t *sdata, char **ztext,
                        PangoLayout *layout, PangoFontDescription *font,
                        double maxsize, double width,
                        double tol_w, double height, double tol_h, int scr_width) {
  double fontsize = maxsize;
  double txdif;
  double dwidth, dheight;
  double est = strlen(xtext) * fontsize / scr_width;

  while (est > 4.) {
    fontsize /= 2.;
    est /= 2.;
  }

  txdif = fontsize / 2.;

  while (txdif >= 2.) {
    _set_font_size(inst, sdata, layout, font, fontsize, (double)scr_width);
    // get pixel size of word
    GETLSIZE(&dwidth, &dheight);
    if (dwidth > width || dheight > height) fontsize -= txdif;
    else {
      if (fontsize < maxsize && dwidth < width * .9 && dheight < height * .9)
        fontsize += txdif;
      else break;
    }
    txdif /= 2.;
  }
  return fontsize;
}


static void proctext(weed_plant_t *inst, sdata_t *sdata, weed_timecode_t tc,
                     char *xtext, cairo_t *cairo, PangoLayout *layout,
                     PangoFontDescription *font, int width, int height) {

  pt_letter_data_t *ldt = NULL, *old_ldt = NULL;

  // this is called initally with a special condition:
  // sdata->count set to -1, sdata->start = 0, sdata->length = 1
  // sdata->init = 1
  //
  // in the more general case, the first call will be with sdata->count = sdata->start
  // then with increasing values of sdata->count until count reaches sdata->length - 1
  // (LETTER mode)

  /// sdata->start and sdata->length may be adjusted by the module at any time,
  // however only changes to length will have immediate effect

  // (* sdata->length = 0 is a special condition, which causes the loop to be called once only,
  // with sdata->count == -1 and an empty sting in xtext)

  // caller loop supplies a single LAYOUT each time, by default this will be used to render
  // the text (ASCII or UTF8 depending on the text mode - this is outside of control of the module)
  // contained in xtext
  // this can be a 'WORD' (a sequence of glyphs) or a  LETTER' (single glyph) depending on the
  // mode of operation of the module

  // the TEXT is derived from the CURRENT LINE (sdata->curstring),
  // and has index: sdata->start + sdata->count

  // if start + count >= length (LETTER MODE) or if count >= length (WORD mode)
  // on return then control is relinquished and the module will be
  // called a little later with sdata->count at zero again

  // the total length of CURRENT LINE in TOKENS (letters) is sdata->tlength, and in WORDS sdata->wlength

  // should start + count reach tlength or wlength (as appropriate), then the following happens
  // - the CURENT LINE is increased, and tlength / wlength are recalculated
  // however this is only a temporary increase in CURRENT LINE (sdata->curstirng)
  // to change CURRENT LINE permanently (sdata->cstring) it is necessary to call GETASTRING()
  // if the end of text is reached then CURRENT LINE will loop back to the beginning (if the input is
  // a file). If input is from the rextbuff parameter and the end of text is reached,
  // this will be signalled to the host via an out_param, and (TODO)...

  // by default, empty lines in the input are ignored as are newlines at the end of a line

  // if SET_ALLOW_BLANKS(TRUE) is called then empty lines are no onger ignored
  // and a newline is counted as the last TOKEN of each line

  // Generally the host determines wheher lines are processed in squence or in random order, with a
  // couple of exceptions: a module can override this with SET_RANDOM_ORDER(FALSE)
  // and when reading from the textbuffer param rather than a file, random order is ignored.

  // start and length may be adjusted with caution, however count, tlength and wlength
  // should be considered as readonly and never altered by a module
  // for letter based modules, normally start is left at zero and length increased
  // until it reaches tlength
  // for word based modules, normally length is set to 1 and start increased to wlength - 1
  // although this is left to the module
  // - smoe modules may set sdata->length to sdata->tlength or sdata->wlength straight away


  // in addition, sdata->count only ever increases monotonically, changing the value of
  // start has no immediate effect on this

  /// thus we can consider the input text to be a string of infinite length, subdivided into
  /// phrases with length tlength or wlength
  /// (Actually this is not quite the case; when reading from a file there is a limit of 64kB,
  /// and when reading from the textbuff parameter, the text will not wrap around).

  // it is important to relinquish control as soon as possible (by allowing start + count to
  // reach length). There is practically no limit to how high length can be set, however the
  // larger the value, generally the slower the filter will run.
  // Future optimisations may allow for imrpoved handling of large amounts of text.

  // Timers: a timer can be set with SET_ALARM(milliseconds), however the time is only checked
  // after control is relinquished. sdata->alarm will be set after at least the elapsed time,
  // The alarm will be cleared automatically, but it may be set again straight away.
  // If the alarm time is set again before it is tirggered then the time is simply updated.
  // Setting a negative time disables the alarm. Zero is also a valid value.
  //

  // initialization: the first itme this is called after an init, sdata->init is set to TRUE
  // then reset automatically to FALSE. The same thing occurs after calling DO_RESET().

  // letter_data: a helper array that may be created to aid animation
  // although it is called letter_data, the same array may be used for words in word mode.
  // calling ANIM_LETTER() will update the state of one element

  // state variables

  // sdata->text contains the entire line of text
  // xtext contains the current TOKEN or WORD

  // useful functions here include:

  // SET_FONT_SIZE(double size)
  // size (points) is rounded to the nearest multiple of 4, scaled according to frame width,
  // and applied to the layout

  // SET_TEXT(const char *text) : replaces the text to be displayed (xtext)
  // setting it to "" empty string will display nothing

  // GETLSIZE(double *width, double *height) : gets size in pixels of the current layout

  // SET_CENTER(double dwidth, double dheight, double x, double y, int *x_text, int *y_text)
  // calculates the top left position for test with pixel size dwidth X dheight centered at x, y
  // result is returned in x_text, y_text

  // SET_ALARM(millis)
  // sets a countdown timer for the desired number of milliseconds
  // sdata->alarm is set to TRUE ater the elapsed time (approximately)
  // (more accurate relative timings can be obtained from sdata->tc
  // which is nominally 10 ^ -8 second units)

  // DO_RESET()
  // reset back to initial state, sets sdata->init. relinquish control

  // GETASTRING()
  // causes the next string (line) to be retrieved from the text file
  // if sdata->rndorder is set non-zero then the order will be randomised, otherwise lines are
  // provided in the same sequence as the text file

  // GETRANDI(start, end) : returns a pseudo-random integer in the range start, end (inclusive)

  // ROTATE_TEXT(x, y, angle) : rotates layout about the point x,y with angle (radians)


  // setting the following affects the current layout (the current letter or word)

  // sdata->x_text, sdata->y_text // position of centre of token
  // setting it to 0. 0 places the top left of the token at the top left of the screen
  // the screen goes from 0, 0 (top left) to width, height (bottom right)

  // sdata->fg, sdata->fg_alpha set colour of the current token,
  // r, g, b values range from 0 - 255, but alpha range is 0. (fully transparent)
  // to 1. (fully opaque)

  // the process is additive, that is each increment of sdata->count adds (overlays)
  // to the existing layout

  switch (sdata->mode) {
  case PT_WORD_COALESCE:
    // design features:
    // WORD MODE / single / line based
    // each word in the line fades / dissolves in, pauses, then fade dissolves out
    // into the next word. After a short pause, the nxt line is loaded.

    // state variables:
    // sdata->dbl1 : fade amount (0, = unfaded, 1. = filly faded)

    // phases:
    // -1 - between lines, 0 - fading in, 1, hold, 2 - fading out

    // trigger effects : default

    // new phrase - wait for alarm
    if (sdata->phase == -1 && !sdata->alarm) break;

    if (sdata->init) {
      SET_ANTIALIAS("GOOD");
      LETTER_DATA_INIT(1);
      SET_MODE(PT_WORD_MODE);
      break;
    }

    if (sdata->alarm) {
      // SEQUENCING
      switch (sdata->phase) {
      case -1:
        sdata->phase = 0;
        //GETASTRING(TRUE);
        sdata->length = 1;
        break;
      case 2:
        if (++sdata->start == sdata->wlength) {
          // end of line; get a new string and wait 1 second
          DO_RESET();
          sdata->phase = -1;
          sdata->length = 0;
          SET_ALARM(500);
        } else sdata->phase = 0;
        break;
      case 1:
        sdata->phase = 2;
        break;
      default:
        break;
      }
    }

    if (sdata->length == 0) break;

    // ACTION SECTION

    ldt = LETTER_DATA(0);

    // align center of word at center of screen
    FIT_SIZE(128., width, .9, height, .9);
    GETLSIZE(&ldt->width, &ldt->height);
    SET_CENTER(ldt->width, ldt->height, width / 2, height / 2, &sdata->x_text, &sdata->y_text);

    if (sdata->phase == 0) {
      // blur in phase
      if (sdata->dbl1 >= 1.) {
        // DONE - switch to holding phase
        const char *nxlast = GET_LAST_CHAR(xtext);

        sdata->dbl1 = 1.;
        sdata->dissolve = 0.;

        // hold phase
        sdata->phase = 1;

        // hold time (then switch to phase 2)
        if (CHAR_EQUAL(nxlast, ".") || CHAR_EQUAL(nxlast, "!")
            || CHAR_EQUAL(nxlast, "?")) SET_ALARM(1000);
        else if (CHAR_EQUAL(nxlast, ",")) SET_ALARM(800);
        else if (CHAR_EQUAL(nxlast, ";")) SET_ALARM(400);
        else SET_ALARM(250);
      } else {
        sdata->dissolve = width / 4. * (1. - sdata->dbl1);
        sdata->fg_alpha = sdata->dbl1 * sdata->dbl1;
        sdata->dbl1 += .05 * sdata->velocity;
      }
    } else if (sdata->phase == 2)  {
      // blur out phase
      if (sdata->dbl1 <= 0.) {
        // get next word / phrase
        SET_TEXT("");
        SET_ALARM(0);
      } else {
        sdata->dissolve = width / 4. * (1. - sdata->dbl1);
        sdata->fg_alpha = sdata->dbl1 * sdata->dbl1;
        sdata->dbl1 -= .05 * sdata->velocity;
      }
    }
    SET_OPERATOR(DIFFERENCE);
    break;

  case PT_WORD_SLIDE:
    // design features:
    // WORD MODE / multi / line based
    // each word in the line slides in from a random edge
    // fun can be had as they slide around the screen

    // state variables:
    // int1 - counts how many offscreen

    // phases:
    // 0 - normal, 1 - reset

    // trigger effects : default

    if (sdata->init) {
      SET_ANTIALIAS("GOOD");
      SET_MODE(PT_WORD_MODE);
      LETTER_DATA_INIT(sdata->wlength);
      break;
    }

    if (sdata->alarm) {
      // SEQUENCING
      if (sdata->phase == 1) {
        // reset and get a new string
        DO_RESET();
        break;
      }
      // adding in phase
      sdata->length++;
    }

    // ACTION SECTION
    ldt = LETTER_DATA(sdata->count);
    if (!ldt->setup) {
      // set defaults
      // pick a slide direction
      ldt->group = GETRANDI(0, 3);
      ldt->fontsize = 32;
      SET_FONT_SIZE(ldt->fontsize);
      RAND_COL(ldt);
      GETLSIZE(&ldt->width, &ldt->height);
      switch (ldt->group) {
      case 0:
        // right to left
        if (ldt->height < height)
          ldt->ypos = GETRANDI(0, height - 1. - ldt->height);
        ldt->xpos = width;
        ldt->xvel = -10. - FASTRAND_DBL(10);
        break;
      case 1:
        // left to right
        if (ldt->height < height)
          ldt->ypos = GETRANDI(0, height - 1. - ldt->height);
        ldt->xpos = -ldt->width;
        ldt->xvel = 10. + FASTRAND_DBL(10.);
        break;
      case 2:
        // top to bottom
        // pango_context_get_matrix
        // pango_matrix_rotate
        // pango_context_set_base_gravity
        if (ldt->width < width)
          ldt->xpos = GETRANDI(0, width - 1. - ldt->width);
        ldt->ypos = -ldt->height;
        ldt->yvel = 10. + FASTRAND_DBL(10.);
        break;
      default:
        // bottom to top
        if (ldt->width < width)
          ldt->xpos = GETRANDI(0, width - 1. - ldt->width);
        ldt->ypos = height;
        ldt->yvel = -10. - FASTRAND_DBL(10.);
        break;
      }
      ldt->setup = TRUE;
      // load another
      if (sdata->count < sdata->wlength - 1) SET_ALARM(500);
    }

    if (sdata->count == 0) sdata->int1 = 0;

    if (ldt->inactive) {
      SET_TEXT("");
      if (++sdata->int1 == sdata->wlength) {
        sdata->phase = 1;
        SET_ALARM(0);
      }
      break;
    }
    ANIM_LETTER(ldt);
    SET_FONT_SIZE(ldt->fontsize);
    if (IS_OFFSCREEN(ldt->xpos, ldt->ypos, ldt->width, ldt->height)) {
      SET_TEXT("");
      if (++sdata->int1 == sdata->wlength) {
        sdata->phase = 1;
        SET_ALARM(0);
        break;
      }
      ldt->inactive = TRUE;
      break;
    }
    COLOUR_COPY(&sdata->fg, &ldt->colour);
    sdata->x_text = ldt->xpos;
    sdata->y_text = ldt->ypos;
    break;

  case PT_BOUNCE:
    // design features:
    // LETTER MODE / multi / line based
    // the phrase appears, then the letters start to fall one by one to the bottom of the screen where
    // they bounce

    // state variables:
    // int1 - count of remaining letters
    // int2 - count of spaces
    // int3 - index of letter to drop next
    // int4 current index of ramining letters
    // bool1 - set when we have counted spaces

    // dbl1 - horiz. posn
    // dbl2 - fontsize

    // phases:
    // -1, get the correct text size, 0 - start, 1 - all letters dropping now

    // trigger effects : default

    if (sdata->init) {
      SET_MODE(PT_LINE_MODE);
      sdata->phase = -1;
      break;
    }

    if (sdata->phase == -1) {
      // sacrifice 1 rpund to get the text size
      sdata->phase = 0;
      sdata->dbl2 = FIT_SIZE(256., width, .9, height, .9);
      SET_MODE(PT_LETTER_MODE);
      // relinquish control so we can come back in letter mode
      sdata->int1 = sdata->count = sdata->length = sdata->tlength;
      sdata->int3 = -1;
      LETTER_DATA_INIT(sdata->tlength);
      SET_TEXT("");
      break;
    }

    if (sdata->alarm) {
      if (sdata->phase == 1) {
        DO_RESET();
        break;
      }
      // pick a letter to drop
      sdata->int3 = GETRANDI(0, sdata->int1 - sdata->int2 - 1);
    }

    if (!sdata->count) {
      sdata->int4 = 0;
      sdata->dbl1 = 0.;
      sdata->int5 = 0;
    }

    ldt = LETTER_DATA(sdata->count);
    if (ldt->inactive) break;

    if (IS_A_SPACE(xtext)) {
      // count spaces, or ignore
      if (!sdata->bool1) {
        SET_FONT_SIZE(sdata->dbl2);
        GETLSIZE(&ldt->width, &ldt->height);
        sdata->dbl1 += ldt->width;
        sdata->int2++;
        ldt->inactive = TRUE;
        if (sdata->count == sdata->length - 1) {
          sdata->bool1 = TRUE;
          SET_ALARM(0);
        }
      }
      break;
    }

    // ACTION SECTION
    if (!sdata->bool1) {
      GETLSIZE(&ldt->width, &ldt->height);
      ldt->xpos = sdata->dbl1;
      ldt->ypos = (height - ldt->height) / 2.;
      sdata->dbl1 += ldt->width;

      if (sdata->count == sdata->length - 1) {
        sdata->bool1 = TRUE;
        SET_ALARM(0);
      }
    }

    if (!ldt->phase) {
      if (sdata->int4++ == sdata->int3) {
        // drop this one next
        ldt->yaccel = .2;
        ldt->phase = 1;
        sdata->int3 = -1;
        if (--sdata->int1 - sdata->int2 == 0) {
          // all dropping
          sdata->phase = 1;
        } else SET_ALARM(200);
      }
    }

    ANIM_LETTER(ldt);

    if (ldt->ypos + ldt->height >= height) {
      if (fabs(ldt->yvel) <= ldt->yaccel * 8.) {
        double dwidth, dheight;
        ldt->yaccel = ldt->yvel = 0.;
        if (ldt->rotvel != 0.) {
          ldt->rotvel = 0.;
          GETLSIZE(&ldt->width, &ldt->height);
        }
        if (ldt->group < 3) {
          if (ldt->group < 2) {
            ldt->rot -= .05;
            GETLSIZE(&dwidth, &dheight);
            if (dheight < ldt->height) {
              ldt->height = dheight;
              ldt->group = 1;
            } else {
              ldt->rot += .05;
              if (ldt->group == 1) ldt->group = 3;
            }
          }
          if (ldt->group == 0 || ldt->group == 2) {
            ldt->rot += .05;
            GETLSIZE(&dwidth, &dheight);
            if (dheight < ldt->height) {
              ldt->height = dheight;
              ldt->group = 2;
            } else {
              ldt->rot -= .05;
              ldt->group = 3;
            }
          }
        }
      } else {
        ldt->yvel = -fabs(ldt->yvel) * .6;
        ldt->rotvel += FASTRAND_DBL(.02) - .01;
      }
      if (sdata->phase == 1) {
        if (ldt->yvel == 0.) {
          if (++sdata->int5 == sdata->length - 1 - sdata->int2) {
            //GETASTRING(TRUE);
            SET_ALARM(0);
          }
        }
      }
      ANIM_LETTER(ldt);
    }

    ROTATE_TEXT(ldt->xpos + ldt->width / 2., ldt->ypos + ldt->height / 2., ldt->rot);
    sdata->x_text = ldt->xpos;
    sdata->y_text = ldt->ypos;

    if (sdata->count) {
      old_ldt = LETTER_DATA(sdata->count - 1);
      COLOUR_COPY(&ldt->colour, &old_ldt->colour);
    }
    ADJUST_RGBA(ldt, 128, 64, 128, 1., .2);

    break;

  case PT_LETTER_STARFIELD:
    // LETTER_MODE:
    // this is called for each letter from sdata->start to sdata->start + sdata->length - 1
    // with sdata->count set to the current letter number

    // in letter mode, each letter has the following interal values:
    // xpos, ypos, zpos, rot, xvel, yvel, zvel, rotvel, xaccel, yaccel, zaccel, rotaccel
    // orbit, orbitvel, orbitaccel
    // as well as size, colour and alpha

    // calling ANIM_LETTER will update the velocities and positions

    // it is then left to the programmer to map these values

    if (sdata->init) {
      // set string start position and length
      SET_ANTIALIAS("GOOD");
      SET_MODE(PT_LETTER_MODE);
      // init letter data
      LETTER_DATA_INIT(sdata->tlength);
      sdata->variant = GETRANDI(0, 10);
      SET_ALARM(0);
      break;
    }

    if (sdata->alarm) {
      // set values for next letter, increase string length
      gboolean wordstart = FALSE;

      if (sdata->phase == 0) {
        sdata->length--;
        sdata->phase = 1;
      }

      old_ldt = sdata->length > 0 ? LETTER_DATA(sdata->length - 1) : NULL;
      if (sdata->length == 0) wordstart = TRUE;

      while (sdata->length < sdata->tlength - 1 && IS_A_SPACE_AT(sdata->length)) {
        wordstart = TRUE;
        sdata->length++;
      }

      if (sdata->length < sdata->tlength) sdata->length++;
      if (sdata->length < sdata->tlength) {
        if (sdata->variant == 1) SET_ALARM(0); // burst mode
        else SET_ALARM(800); // milliseconds after letter
      } else SET_ALARM(-1);

      ldt = LETTER_DATA(sdata->length - 1);

      ldt->fontsize = 2048. * (.8 + FASTRAND_DBL(.4));
      ldt->orbit = ldt->orbitvel = ldt->orbitaccel = 0.;

      if (wordstart) {
        sdata->variant = GETRANDI(0, 10);
        ldt->orbitstart = RAND_ANGLE();
        if (sdata->variant == 8) ldt->orbitvel = FASTRAND_DBL(.5) - .25;
      } else {
        if (sdata->variant == 6 && sdata->wlength) {
          ldt->orbitstart = old_ldt->orbitstart + TWO_PI / sdata->wlength;
        } else if (sdata->variant == 7 && sdata->wlength) {
          ldt->orbitstart = old_ldt->orbitstart - TWO_PI / sdata->wlength;
        } else if (sdata->variant == 8) {
          ldt->orbitstart = old_ldt->orbitstart;
        } else ldt->orbitstart = RAND_ANGLE();
      }

      ldt->xpos = ldt->ypos = ldt->rot = ldt->rotvel = 0.;
      ldt->zpos = 128. + sdata->length;

      if (ldt->zpos > 150.) ldt->zpos = 150.;

      ldt->xvel = sin(ldt->orbitstart) * (double)width / 32.;
      ldt->yvel = cos(ldt->orbitstart) * (double)height / 32.;
      ldt->zvel = -1.5;

      ldt->xaccel = ldt->yaccel = ldt->zaccel = ldt->rotaccel = 0.;

      if (wordstart) {
        if (sdata->variant == 2) {
          ldt->rotvel = FASTRAND_DBL(.5);
        } else if (sdata->variant == 3) {
          ldt->rotvel = -FASTRAND_DBL(.5);
        }
        if (sdata->variant == 4) {
          ldt->orbitvel = FASTRAND_DBL(.5);
        } else if (sdata->variant == 5) {
          ldt->orbitvel = -FASTRAND_DBL(.5);
        }
        RAND_COL(ldt);
      } else {
        ldt->rotvel = old_ldt->rotvel;
        ldt->orbitvel = old_ldt->orbitvel;
        COLOUR_COPY(&ldt->colour, &old_ldt->colour);
      }
    }

    // deal with current letter (sdata->count)
    ldt = LETTER_DATA(sdata->count);
    if (ldt->zpos >= 1.) {
      SET_FONT_SIZE(ldt->fontsize / ldt->zpos);

      // get pixel size of letter
      GETLSIZE(&ldt->width, &ldt->height);

      sdata->x_text = ldt->xpos / ldt->zpos + (double)width / 2.;
      sdata->y_text = ldt->ypos / ldt->zpos + (double)height / 2.;

      // position centre of letter
      SET_CENTER(ldt->width, ldt->height, sdata->x_text, sdata->y_text, &sdata->x_text, &sdata->y_text);

      // letters fade out as they approach
      sdata->fg_alpha = ldt->zpos / 200. + .5;

      // update positions and velocities
      ANIM_LETTER(ldt);
      ROTATE_TEXT(width / 2., height / 2., ldt->orbit);
      ROTATE_TEXT(sdata->x_text, sdata->y_text, ldt->rot - ldt->orbit);
      COLOUR_COPY(&sdata->fg, &ldt->colour);
    } else {
      SET_TEXT("");
      if (sdata->count == sdata->tlength - 1) {
        // end of string
        // sets start, length back to zero
        DO_RESET();
        // init next string
        SET_ALARM(0);
      }
    }

    break;

  case PT_TERMINAL:
    // design features:
    // LETTER MODE
    // letters are placed sequentially, a newline will increment y and reset x
    // the letters gradually fade the further they are from the end.
    // a cursor is placed after the last letter

    // state variables:
    // bool1 - ensures we only pause once on a newline
    // bool2 - set if we can use a custom font
    // dbl1, dbl2 - x,y cordinates of sdata->count element
    // dbl3 - fontsize
    // dbl4 - max height of line
    // string1 - fontname
    // int1 - count of successive newlines
    // int2 - count between refreshes
    // int3 - amount to subtract from total length

    // int4, int5 - for variants

    // phases:

    // trigger effects : default

#define FONTFAM1 "Borgs"
#define FONTFAM2 "Flipside Bak"
#define FONTFAM3 "Atari"
#define FONTFAM4 "Technique BRK"
#define FONTFAM5 "Ethnocentric"

    if (sdata->init) {
      SET_MODE(PT_LETTER_MODE);
      SET_RANDOM_ORDER(FALSE);
      SET_ALLOW_BLANKS(TRUE);
      //GETASTRING(TRUE);
      LETTER_DATA_INIT(sdata->totlen);
      //sdata->variant = 1;
      if (HAS_FONT_FAMILY(FONTFAM1)) {
        SET_STRING(sdata->string1, FONTFAM1);
        sdata->bool2 = TRUE;
      }
      SET_ALARM(0);
      break;
    }

    if (!sdata->count) {
      sdata->dbl1 = sdata->dbl2 = sdata->dbl3 = 0.;
      sdata->dbl4 = 0.;
    }

    if (sdata->alarm) {
      sdata->bool1 = FALSE;

      ldt = LETTER_DATA(sdata->length - 1);
      ldt->alpha = 1.;

      sdata->length++;

      // counts chars until a refresh
      sdata->int2++;

      if (sdata->length <= sdata->totlen - sdata->int3) SET_ALARM(10);
      else if (sdata->length == sdata->totlen - sdata->int3 + 1) SET_ALARM(1000);
      else {
        sdata->cstring = -1;
        DO_RESET();
        SET_ALARM(0);
      }
    }

    ldt = LETTER_DATA(sdata->count);
    if (ldt->inactive) {
      SET_TEXT("");
      break;
    }

    if (sdata->bool2) {
      SET_FONT_FAMILY(sdata->string1);
      SET_FONT_WEIGHT(BOLD);
      if (ldt->fontsize == 0.) ldt->fontsize = 8.;
    } else if (ldt->fontsize == 0.) ldt->fontsize = 10.;

    SET_FONT_SIZE(ldt->fontsize);

    if (!ldt->width) GETLSIZE(&ldt->width, &ldt->height);
    if (ldt->height > sdata->dbl4) sdata->dbl4 = ldt->height;

    sdata->fg.red = FASTRAND_INT(32);
    sdata->fg.blue = FASTRAND_INT(64);
    sdata->fg.green = 235 + FASTRAND_INT(20);

    //text fades as it moves away from cursor
    ldt->alpha = 255. - 8. * (sdata->length - sdata->count);
    if (ldt->alpha < 128.) ldt->alpha = 128.;
    ldt->alpha -= FASTRAND_DBL(16.);
    ldt->alpha /= 255.;

    if (sdata->variant == 1) {
      ANIM_LETTER(ldt);

      // FADE OUT ALL IN GROUP 0
      if (sdata->phase == 1 && ldt->group == 0) {
        if (ldt->phase == 0) {
          ldt->alpha_change = -.002;
          ldt->phase = 1;
        }
        if (ldt->alpha <= .2) {
          ldt->inactive = TRUE;
          SET_TEXT("");
          break;
        }
      }
    }

    sdata->fg_alpha = ldt->alpha;

    ldt->xpos = sdata->x_text = sdata->dbl1;
    ldt->ypos = sdata->y_text = sdata->dbl2;

    if (sdata->dbl1 == 0. && !IS_ALL_ONSCREEN(sdata->dbl1, sdata->dbl2, ldt->width, ldt->height)) {
      RESTART_AT_STRING(sdata->cstring + 1);
      break;
    }

    if (CHAR_EQUAL(xtext, "\n")) {
      // newline counter
      sdata->int1++;
      if (sdata->variant == 0 && sdata->int1 > 2) {
        // SCREEN REFRESH

        // reset the length
        sdata->length = 1;

        // int3 adjusts the length to EOF
        sdata->int3 += sdata->int2 + 1;
        sdata->int2 = 0;

        // set string base at current + 1
        RESTART_AT_STRING(sdata->curstring + 1);
      } else {
        // go down and back
        sdata->dbl1 = 0.;
        sdata->dbl2 += sdata->dbl4 * 1.2;
        if (sdata->count == sdata->length - 1) {
          if (sdata->int1 == 1 && !sdata->bool1) {
            sdata->bool1 = TRUE;
            SET_ALARM(200);
          }
        }
      }
      sdata->dbl4 = 0.;
    } else {
      // place next letter
      sdata->dbl1 += ldt->width * 1.2;
      sdata->int1 = 0;
    }
    if (sdata->count == sdata->length - 1) {
      // append the 'cursor'
      if (sdata->text_type == TEXT_TYPE_ASCII) {
        size_t toffs = 0;
        char *cursor = weed_malloc(3);
        if (xtext[0] != '\n') {
          weed_memcpy(cursor, xtext, 1);
          toffs++;
        }
        cursor[toffs++] = 178;
        cursor[toffs] = 0;
        SET_TEXT(cursor);
      } else {
        // append the cursor after the last char displayed
        size_t toffs = (*xtext == '\n') ? 0 : utf8offs(xtext, 1);
        char *cursor = weed_malloc(toffs + 4);
        if (toffs) weed_memcpy(cursor, xtext, toffs);
        else {
          // make cursor wait at next line ?
          //sdata->x_text = sdata->dbl1;
          //sdata->y_text = sdata->dbl2;
        }
        // 0x2588 -> 0010 0101 1000 1000 -> 1110 0010  10 010110 10 001000 -> 226 150 136
        cursor[toffs] = 226;
        cursor[toffs + 1] = 150;
        cursor[toffs + 2] = 136;
        cursor[toffs + 3] = 0;
        SET_TEXT(cursor);
      }
    }

    if (sdata->variant == 1 && sdata->phase == 0) {
      // PICK GLYPHS WHICH MATCH TEXTBUFFER in sequence (20+ chars between succesive matches)
      ldt = LETTER_DATA(sdata->count);
      if (*sdata->textbuf && sdata->count > sdata->int4 + 20 && sdata->textbuf[sdata->int5]
          && ldt->fontsize_change == 0. &&
          CHAR_EQUAL(xtext, &sdata->textbuf[sdata->int5])) {
        ldt->fontsize_change = .2;
        sdata->int5++; // textbuff pointer
        sdata->int4 = sdata->count; // match position
        if (!sdata->textbuf[sdata->int5]) {
          sdata->phase = 1;
        }
        ldt->group = 1;
        ldt->alpha_change = .01;
      }
    }

    break;

  case PT_SPINNING_LETTERS:
    // LETTER_MODE:
    // this is called for each letter from sdata->start to sdata->start + sdata->length - 1
    // with sdata->count set to the current letter number

    if (sdata->init) {
      // select all text in line right away
      SET_MODE(PT_LETTER_MODE);
      sdata->length = sdata->tlength;
      LETTER_DATA_INIT(sdata->length);
      //SET_ALARM(0);
      break;
    }

    /* if (sdata->alarm) { */
    /*   if (sdata->bool1) { */
    /*     if (!sdata->bool2) { */
    /*       // get a second line */
    /*       sdata->int1 += sdata->tlength; */
    /*       LETTER_DATA_FREE(); */
    /*       GETASTRING(); */
    /*       LETTER_DATA_INIT(sdata->tlength); */
    /*       //LETTER_DATA_EXTEND(sdata->tlength); */
    /*       sdata->bool2 = TRUE; */
    /*       sdata->dbl1 = 0.; */
    /*     } */
    /*   } */
    /*   // must reset after calling GETASTRING */
    /*   sdata->bool1 = TRUE; */
    /*   SET_ALARM(-1); */
    /* } */

    ldt = LETTER_DATA(sdata->count + sdata->int1);

    if (!ldt->setup) {
      if (sdata->bool2) ldt->group = 2;
      else ldt->group = 1;
      ldt->fontsize = 128.;
      ldt->setup = TRUE;
      ldt->xpos = sdata->dbl1;
      SET_FONT_SIZE(ldt->fontsize);
      GETLSIZE(&ldt->width, &ldt->height);
      sdata->dbl1 += ldt->width + 10.;
      ldt->rotvel = .1;
      ldt->xvel = -width / 80.;
      ldt->alpha = 1.;
      if (!sdata->count) RAND_COL(ldt);
      else {
        old_ldt = LETTER_DATA(sdata->count - 1);
        COLOUR_COPY(&ldt->colour, &old_ldt->colour);
        ADJUST_RGBA(ldt, 128, 64, 128, 1., .05);
      }
      COLOUR_COPY(&sdata->fg, &ldt->colour);
      ldt->text = strdup(xtext);
    }

    if (sdata->count == sdata->length - 1 && ldt->xpos + ldt->width + width < 0) {
      DO_RESET();
      break;
    }

    SET_FONT_SIZE(ldt->fontsize);

    /* if (sdata->trigger && sdata->autotrigger) { */
    /*   double rota = ldt->rotaccel; */
    /*   ldt->rotaccel = 0.; */
    /*   ANIM_LETTER(ldt); */
    /*   ldt->rotaccel = rota; */
    /*   sdata->dissolve += 4.; */
    /* } */
    /* else { */


    /* if (sdata->count == sdata->length - 1 && ldt->xpos <= (-width - dwidth) / 2) { */
    /*   if (ldt->phase == 1) { */
    /* 	ldt->phase = 2; */
    /* 	ldt->targetx = FASTRAND_DBL(width); */
    /* 	ldt->targety = FASTRAND_DBL(height); */
    /* 	ldt->targetmass = 10.; */
    /*   } */
    /* } */

    // update positions and velocities
    ANIM_LETTER(ldt);

    //fprintf(stderr, "posx %d is %f\n", sdata->count, ldt->xpos);
    sdata->x_text = width + ldt->xpos;
    sdata->y_text = height / 2.;

    ROTATE_TEXT(sdata->x_text, sdata->y_text, ldt->rot);
    SET_CENTER(ldt->width, ldt->height, sdata->x_text, sdata->y_text, &sdata->x_text, &sdata->y_text);

    COLOUR_COPY(&sdata->fg, &ldt->colour);

    break;

  case PT_SPIRAL_TEXT:
    // design features:
    // spinning, whirling letters /

    // Mode: phrase based / letter mode

    // state variables:
    // dbl1, dbl2, dbl2 - path variables

    // int1 - counts '.'s in phrase

    // phases: initial 0, when all of phrase loaded, swicthes to phase 1, 4 second pause
    // the begins shrinking the text length

    // trigger effects : default

    if (sdata->init) {
      sdata->tmode = PT_LETTER_MODE;
      LETTER_DATA_INIT(sdata->tlength);
      SET_ALARM(0);
      break;
    }

    if (!sdata->count) {
      sdata->dbl3 -= .0001 * sdata->velocity;
      sdata->dbl1 = width * .45 * (1. + sdata->dbl3);
      sdata->dbl2 = height * .45 * (1. + sdata->dbl3);
      sdata->int2 = 0;
    }

    if (sdata->alarm) {
      // SEQUENCING
      if (sdata->start + sdata->length < sdata->tlength) {
        // add an extra letter
        sdata->length++;
        SET_ALARM(80); // milliseconds to get next letter
      } else {
        // no more letters...
        if (sdata->phase == 0) {
          sdata->phase = 1;
          SET_ALARM(4000);
        } else {
          // shrink from head to tail
          sdata->length--;
          sdata->start++;

          if (sdata->start >= sdata->length) {
            // all letters gone - restart cycle
            DO_RESET();
            break;
          }

          else SET_ALARM(80); // milliseconds (string disappearing)
        }
      }
    }

    ldt = LETTER_DATA(sdata->count);

    ldt->fontsize = 2560. / (sdata->count + 19.);
    if (sdata->phase == 1) {
      ldt->fontsize *= (FASTRAND_DBL(1.) + .5);
    }

    SET_FONT_SIZE(ldt->fontsize);

    // get pixel size of letter/word
    GETLSIZE(&ldt->width, &ldt->height);

    if (sdata->phase == 0) {
      sdata->dbl1 *= .97;
      sdata->dbl2 *= .97;
      ldt->colour.red = ldt->colour.green = ldt->colour.blue = 255;
      ldt->alpha = 1.;
      SET_OPERATOR(DIFFERENCE);
    } else {
      if (!ldt->phase) {
        ldt->rotaccel = FASTRAND_DBL(.02) - .01;
        ldt->phase = 1;
      }
      ADJUST_RGBA(ldt, 128, 64, 128, .4, .05);
      sdata->dbl3 -= .001 * sdata->velocity;

      sdata->dbl1 *= .99;
      sdata->dbl2 *= .99;
    }

    //sdata->dbl3 += .02;
    SET_CENTER(ldt->width, ldt->height, width / 2 + sin(sdata->count / 4. - (1. - sdata->dbl3) * 8.
               + sdata->dbl4)
               * sdata->dbl1,
               height / 2 - cos(-sdata->count / 4. + (1. - sdata->dbl3) * 8.
                                + sdata->dbl4)
               * sdata->dbl2, &sdata->x_text, &sdata->y_text);

    if (IS_OFFSCREEN(sdata->x_text, sdata->y_text, ldt->width, ldt->height)) {
      sdata->int2++;
      if (sdata->int2 == sdata->tlength - 1) {
        DO_RESET();
        break;
      }
    }

    if (sdata->phase > 0 && ldt->phase == 1) {
      ANIM_LETTER(ldt);
      ROTATE_TEXT(sdata->x_text, sdata->y_text, ldt->rot);
    }
    // check if word-token starts with "."
    if (CHAR_EQUAL(xtext, ".")) sdata->int1++;

    break;
  } // end switch
}


static void badstrings(int v) {
  if (verbosity >= v)
    fprintf(stderr, "No non-empty strings found in input text\n");
}


static void reparse_text(sdata_t *sdata, const char *buff, size_t b_read) {
  // parse the text
  size_t ulen;
  int i, ii = 0, j = 0;
  int canstart = 0;

  for (i = 0; i < b_read; i++) {
    if ((uint8_t)buff[i] == 0x0A || (uint8_t)buff[i] == 0x0D) {
      sdata->xnstrings++;
      if (canstart < i) {
        sdata->nstrings++;
      }
      canstart = i + 1;
    }
  }
  if (canstart < i) {
    sdata->xnstrings++;
    sdata->nstrings++;
  }

  if (sdata->nstrings) {
    // only non-empty strings
    sdata->strings = (char **)weed_calloc(sdata->nstrings, sizeof(char *));

    // all including empty
    sdata->xstrings = (char **)weed_calloc(sdata->xnstrings, sizeof(char *));

    canstart = 0;

    for (i = 0; i < sdata->xnstrings; i++) {
      sdata->xstrings[i] = NULL;
      // parse the text
      for (; j < b_read; j++) {
        if ((uint8_t)buff[j] == 0x0A || (uint8_t)buff[i] == 0x0D) {
          if (canstart == j) {
            sdata->xstrings[i] = stringdup("", 0);
            ulen = 1;
          } else {
            sdata->strings[ii++] = sdata->xstrings[i] = stringdup(&buff[canstart], j - canstart);
            ulen = utf8len(sdata->xstrings[i]);
            sdata->totulennb += ulen;
            sdata->totalennb += j - canstart;
          }
          sdata->totulenwb += ulen + 1;
          sdata->totalenwb += j - canstart + 1;
          canstart = ++j;
          break;
        }
      }
    }
    if (canstart < j) {
      sdata->xstrings[--i] = stringdup(&buff[canstart], j - canstart);
      sdata->strings[ii] = sdata->xstrings[i];
      ulen = utf8len(sdata->xstrings[i]);
      sdata->totulennb += ulen;
      sdata->totalennb += j - canstart;
      sdata->totulenwb += ulen + 1;
      sdata->totalenwb += j - canstart + 1;
    }
  } else sdata->xnstrings = 0;
}


static weed_error_t puretext_init(weed_plant_t *inst) {
  weed_plant_t **in_params = weed_get_in_params(inst, NULL);
  sdata_t *sdata;
  int mode;
  int hidden = WEED_FALSE;
  gboolean use_file = (weed_param_get_value_boolean(in_params[P_USEBUF]) == WEED_FALSE);

  if (!(weed_instance_get_flags(inst) & WEED_INSTANCE_UPDATE_GUI_ONLY)) {
    char *buff;
    char xbuff[MAX_BUFFLEN];
    size_t b_read;
    gboolean erropen = FALSE;
    int fd;

    // open file and read in text
    if (use_file) {
      char *textfile = weed_param_get_value_string(in_params[P_FILE]);
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
    }

    sdata = (sdata_t *)weed_calloc(sizeof(sdata_t), 1);

    if (!sdata) {
      weed_free(in_params);
      return WEED_ERROR_MEMORY_ALLOCATION;
    }

    // enable repeatable randomness
    sdata->orig_seed = weed_get_int64_value(inst, WEED_LEAF_RANDOM_SEED, NULL);
    weed_set_int64_value(inst, WEED_LEAF_PLUGIN_RANDOM_SEED, sdata->orig_seed);

    sdata->use_file = use_file;

    if (!use_file) buff = sdata->textbuf = weed_param_get_value_string(in_params[P_TEXTBUF]);

    sdata->text_type = TEXT_TYPE_UTF8;
    //sdata->text_type = TEXT_TYPE_ASCII;

    sdata->tmode = PT_LETTER_MODE;

    if (use_file && !erropen) {
      b_read = read(fd, buff, MAX_BUFFLEN - 1);
      buff[b_read] = 0;
      close(fd);
    }
    if (!use_file) b_read = strlen(buff);

    reparse_text(sdata, buff, b_read);

    if (!sdata->nstrings) {
      if (use_file) {
        badstrings(WEED_VERBOSITY_CRITICAL);
        weed_free(in_params);
        return WEED_ERROR_NOT_READY;
      } else {
        weed_plant_t **out_params = weed_get_out_params(inst, NULL);
        // signal host for more text
        weed_set_boolean_value(out_params[0], WEED_LEAF_VALUE, WEED_TRUE);
        weed_free(out_params);
      }
    }

    sdata->mode = -1; // force update
    sdata->timer = -1;
    sdata->funmode = -1;
    weed_set_voidptr_value(inst, "plugin_internal", sdata);
  } else sdata = weed_get_voidptr_value(inst, "plugin_internal", NULL);

  sdata->randmode = weed_param_get_value_boolean(in_params[P_RANDMODE]);
  if (sdata->randmode) {
    weed_param_set_hidden(in_params[P_MODE], WEED_TRUE);
  } else {
    weed_param_set_hidden(in_params[P_MODE], WEED_FALSE);
    mode = weed_param_get_value_int(in_params[P_MODE]);
    if (mode == PT_TERMINAL) hidden = WEED_TRUE;
  }

  weed_param_set_hidden(in_params[P_FONT], hidden);

  if (!sdata->use_file) {
    hidden = TRUE;
    weed_param_set_hidden(in_params[P_FILE], WEED_TRUE);
    weed_param_set_hidden(in_params[P_TEXTBUF], WEED_FALSE);
  } else {
    weed_param_set_hidden(in_params[P_FILE], WEED_FALSE);
    weed_param_set_hidden(in_params[P_TEXTBUF], WEED_TRUE);
  }

  weed_param_set_hidden(in_params[P_RANDLINES], hidden);

  weed_free(in_params);
  return WEED_SUCCESS;
}


static weed_error_t puretext_deinit(weed_plant_t *inst) {
  int i;
  sdata_t *sdata = (sdata_t *)weed_get_voidptr_value(inst, "plugin_internal", NULL);

  if (sdata) {
    if (sdata->letter_data) _letter_data_free(sdata);
    for (i = 0; i < sdata->xnstrings; i++) if (sdata->xstrings[i]) weed_free(sdata->xstrings[i]);
    if (sdata->xstrings) weed_free(sdata->xstrings);
    if (sdata->strings) weed_free(sdata->strings);
    weed_free(sdata);
  }
  weed_set_voidptr_value(inst, "plugin_internal", NULL);
  return WEED_SUCCESS;
}


static weed_error_t puretext_process(weed_plant_t *inst, weed_timecode_t tc) {
  sdata_t *sdata = (sdata_t *)weed_get_voidptr_value(inst, "plugin_internal", NULL);
  weed_plant_t **in_params = weed_get_in_params(inst, NULL);
  weed_plant_t **out_params = weed_get_out_params(inst, NULL);

  if (!sdata->use_file) {
    if (sdata->refresh || sdata->needsmore
        || (sdata->allow_blanks && sdata->cstring >= sdata->xnstrings)
        || (!sdata->allow_blanks && sdata->cstring >= sdata->nstrings)) {
      char *buff = weed_param_get_value_string(in_params[P_TEXTBUF]);
      if (strcmp(buff, sdata->textbuf)) {
        // reparse textbuff
        size_t tblen = strlen(buff);
        if (!sdata->needsmore) {
          if (sdata->textbuf) weed_free(sdata->textbuf);
          for (int i = 0; i < sdata->xnstrings; i++)
            if (sdata->xstrings[i]) weed_free(sdata->xstrings[i]);
          if (sdata->xstrings) weed_free(sdata->xstrings);
          if (sdata->strings) weed_free(sdata->strings);
          sdata->nstrings = sdata->xnstrings = 0;
          sdata->totulenwb = sdata->totalenwb = 0;
          sdata->totulennb = sdata->totalennb = 0;
          sdata->strings = sdata->xstrings = NULL;
          sdata->textbuf = buff;
        } else {
          size_t tblen2 = strlen(sdata->textbuf);
          if (tblen + tblen2 >= MAX_BUFFLEN) tblen = MAX_BUFFLEN - 1 - tblen2;
          sdata->textbuf = weed_realloc(sdata->textbuf, tblen + tblen2 + 1);
          weed_memcpy(sdata->textbuf + tblen2, buff, tblen);
          tblen += tblen2;
          sdata->textbuf[tblen] = 0;
          weed_free(buff);
        }

        reparse_text(sdata, sdata->textbuf, tblen);
        sdata->refresh = sdata->needsmore = FALSE;
      }
    }
  }
  if (!sdata->nstrings) {
    weed_set_boolean_value(out_params[0], WEED_LEAF_VALUE, WEED_TRUE);
    weed_free(in_params);
    weed_free(out_params);
    return WEED_ERROR_NOT_READY;
  } else {
    weed_plant_t *in_channel = weed_get_in_channel(inst, 0);
    weed_plant_t *out_channel = weed_get_out_channel(inst, 0);
    weed_plant_t *filter;

    pt_subst_t *xsubst;
    cairo_t *cairo;
    char *fontstr;

    int width = weed_channel_get_width(out_channel);
    int height = weed_channel_get_height(out_channel);
    int pal = weed_channel_get_palette(out_channel);
    int mode = weed_param_get_value_int(in_params[P_MODE]);
    int trigger = weed_param_get_value_boolean(in_params[P_TRIGGER]);
    int version, i;

    if (trigger == WEED_TRUE && !sdata->last_trigger) {
      sdata->trigger = TRUE;
    } else sdata->trigger = FALSE;
    sdata->last_trigger = (trigger == WEED_TRUE);

    if (sdata->randmode && sdata->mode != -1) mode = sdata->mode;

    if (mode != sdata->mode || sdata->funmode != -1) {
      // enable repeatable randomness
      sdata->orig_seed = fastrand(sdata->orig_seed);
      weed_set_int64_value(inst, WEED_LEAF_PLUGIN_RANDOM_SEED, sdata->orig_seed);

      sdata->tmode = PT_LETTER_MODE;
      sdata->timer = -1.;
      if (sdata->funmode == -1) sdata->cstring = 0;
      sdata->autotrigger = TRUE;
      sdata->variant = 0;
      sdata->dissolve = 0;
      sdata->antialias = CAIRO_ANTIALIAS_FAST;
      sdata->allow_blanks = FALSE;
      sdata->rndorder_set = FALSE;
      _do_reset(inst, sdata, NULL,  NULL);
      if (sdata->funmode != -1) {
        mode = sdata->funmode;
        sdata->funmode = -1;
      }
      sdata->mode = mode;
    }

    // set timer data and alarm status
    if (sdata->timer == -1. || tc < sdata->last_tc) {
      sdata->alarm_time = -1.;
      sdata->timer = 0.;
    } else {
      sdata->timer += (double)(tc - sdata->last_tc) / (double)WEED_TICKS_PER_SECOND * sdata->velocity;
    }

    if (!sdata->use_file) sdata->rndorder = FALSE;
    else {
      if (!sdata->rndorder_set)
        sdata->rndorder = (weed_param_get_value_boolean(in_params[P_RANDLINES]) == WEED_TRUE);
    }

    if (sdata->alarm_time >= 0. && sdata->timer > sdata->alarm_time) {
      sdata->alarm_time = -1.;
      sdata->alarm = TRUE;
    } else sdata->alarm = FALSE;

    sdata->last_tc = tc;

    //xtext = text = weed_param_get_value_string(in_params[P_TEXT]);

    sdata->fontscale = weed_param_get_value_double(in_params[P_FONTSCALE]);
    sdata->velocity = weed_param_get_value_double(in_params[P_SPEED]);

    filter = weed_instance_get_filter(inst);
    version = weed_filter_get_version(filter);
    if (!version) version = 1;

    if (version == 1) fontstr = strdup(DEF_FONT);
    else fontstr = weed_param_get_value_string(in_params[P_FONT]);

    weed_free(in_params); // must weed free because we got an array

    if (!sdata->allow_blanks) {
      sdata->totalen = sdata->totalennb;
      sdata->totulen = sdata->totulennb;
    } else {
      sdata->totalen = sdata->totalenwb;
      sdata->totulen = sdata->totulenwb;
    }

    sdata->curstring = sdata->cstring;

    // convert channel pixel_data to a cairo surface
    if ((!in_channel) || (in_channel == out_channel))
      cairo = channel_to_cairo(sdata, out_channel);
    else
      cairo = channel_to_cairo(sdata, in_channel);

    if (!sdata->text) GETASTRING(TRUE);

    if (cairo) {
      PangoLayout *layout = NULL;
      PangoFontDescription *font;
      char *family;
      size_t offset;
      int fontnum = 0;

      // initialize byte and utf8 offsets
      // (only useful for PT_LETTER_MODE)
      if (sdata->text_type == TEXT_TYPE_ASCII) {
        sdata->totlen = sdata->totalen;
        sdata->toffs = sdata->start;
      } else {
        sdata->totlen = sdata->totulen;
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
      } else pango_font_description_set_family(font, DEF_FONT);

      SET_FONT_SIZE(32.);

      _getastring(inst, sdata, TRUE, FALSE, TRUE);
      offset = sdata->start;

      if (sdata->init == -1) sdata->init = 1;
      if (sdata->init || sdata->length == 0) {
        sdata->count = -1;
      } else sdata->count = 0;

      for (i = sdata->start;
           (sdata->tmode == PT_LETTER_MODE && sdata->start + sdata->count < sdata->length)
           || (sdata->tmode == PT_WORD_MODE && sdata->count < sdata->length)
           || (sdata->tmode == PT_LINE_MODE && sdata->count < sdata->length)
           ; i++) {
        PangoFontDescription *xfont;
        char *ztext[1];
        xtext = NULL; // xtext #defined as *ztext

        // each letter or word gets its own layout which are combined
        if (!layout) layout = pango_cairo_create_layout(cairo);

        xfont = pango_font_description_copy(font);
        pango_layout_set_font_description(layout, xfont);

        if (sdata->count < 0 || sdata->length == 0) {
          xtext = strdup("");
        } else {
          if (!sdata->text) {
            // get a line of text if we don't have any
            GETASTRING(TRUE);
          }

          switch (sdata->tmode) {
          case PT_LETTER_MODE:
            // letter mode; the pango layout will contain just a single char (ascii or utf8)
            // we set this in xtext

            if (sdata->count == 0 || offset >= sdata->tlength) {
              int xcstring = sdata->curstring;
              size_t xoffset = offset;
              while (offset >= sdata->tlength) {
                offset -= sdata->tlength;
                _getastring(inst, sdata, FALSE, TRUE, FALSE);
                if (sdata->curstring == 0 && !sdata->use_file) {
                  sdata->needsmore = TRUE;
                  break;
                }
                if (sdata->curstring == xcstring && offset == xoffset) {
                  xtext = strdup("");
                  badstrings(WEED_VERBOSITY_WARN);
                  break;
                }
              }

              if (sdata->text_type == TEXT_TYPE_ASCII) {
                sdata->toffs = offset;
              } else {
                sdata->utoffs = offset;
                sdata->toffs = utf8offs(sdata->text, sdata->utoffs);
              }
            }

            if (sdata->allow_blanks && offset == sdata->tlength - 1) {
              xtext = strdup("\n");
              sdata->toffs++;
              sdata->utoffs++;
              offset++;
              break;
            }
            if (sdata->text_type == TEXT_TYPE_ASCII) {
              xtext = stringdup(&sdata->text[sdata->toffs], 1);
              sdata->toffs++;
            } else {
              int xlen;
              if (sdata->count == 0) {
                sdata->toffs = utf8offs(sdata->text, sdata->start);
              }
              xlen = mbtowc(NULL, &sdata->text[sdata->toffs], 4);
              if (xlen == -1) xlen = 1;
              xtext = stringdup(&sdata->text[sdata->toffs], xlen);
              //fprintf(stderr, "GOT STR (%c),  %s\n%s:%ld:%d:\n",
              //	sdata->text[sdata->toffs], xtext, sdata->text, sdata->toffs, xlen);
              sdata->toffs += xlen;
              sdata->utoffs++;
            }
            offset++;
            break;

          case PT_WORD_MODE:
            // word mode; herte we pass in a "word" at a time
            if (sdata->count == 0 || offset >= sdata->wlength) {
              int xcstring = sdata->curstring;
              size_t xoffset = offset;
              while (offset >= sdata->wlength) {
                offset -= sdata->wlength;
                _getastring(inst, sdata, FALSE, TRUE, FALSE);
                if (sdata->curstring == 0 && !sdata->use_file) {
                  sdata->needsmore = TRUE;
                  break;
                }
                if (sdata->curstring == xcstring && offset == xoffset) {
                  xtext = strdup("");
                  badstrings(WEED_VERBOSITY_WARN);
                  break;
                }
              }

              if (sdata->text_type == TEXT_TYPE_ASCII) {
                xsubst = get_nth_word_ascii(sdata, sdata->text, offset);
              } else {
                xsubst = get_nth_word_utf8(sdata, sdata->text, offset);
                sdata->utoffs = xsubst->ustart + xsubst->ulength - 1;
              }
            } else {
              if (sdata->text_type == TEXT_TYPE_ASCII) {
                xsubst = get_nth_word_ascii(sdata, sdata->text, -1);
              } else {
                xsubst = get_nth_word_utf8(sdata, sdata->text, -1);
                sdata->utoffs = xsubst->ustart + xsubst->ulength;
              }
            }
            sdata->toffs = xsubst->start + xsubst->length;
            xtext = stringdup(&sdata->text[xsubst->start], xsubst->length);
            weed_free(xsubst);
            break;

          case PT_LINE_MODE:
            // word mode; herte we pass in a "word" at a time
            if (offset > 0) {
              int xcstring = sdata->curstring;
              size_t xoffset = offset;
              while (offset--) {
                _getastring(inst, sdata, FALSE, TRUE, FALSE);
                if (sdata->curstring == 0 && !sdata->use_file) {
                  sdata->needsmore = TRUE;
                  break;
                }
                if (sdata->curstring == xcstring && offset == xoffset) {
                  xtext = strdup("");
                  badstrings(WEED_VERBOSITY_WARN);
                  break;
                }
              }
            }
            sdata->toffs = sdata->utoffs = 0;
            xtext = strdup(sdata->text);
            break;
          default:
            // TODO - all mode
            break;
          }
        }

        sdata->ltext = NULL;

        if (!sdata->needsmore) {
          //fprintf(stderr, "set the text to %s\nline: %s\n", xtext, sdata->text);
          // call the procedure
          proctext(inst, sdata, tc, ztext, cairo, layout, xfont, width, height);

          //fprintf(stderr, "xtext is %s %f\n", xtext, sdata->dissolve);

          sdata->alarm = FALSE;

          if (sdata->length < 0) sdata->length = 0;
          if (sdata->init == 1) {
            sdata->init = 0;
            if (sdata->alarm_time == sdata->timer) sdata->alarm = TRUE;
          }

          if (xtext && *xtext && !sdata->init
              && i < sdata->start + (sdata->length == 0 ? 1 : sdata->length)
              && !IS_A_SPACE(xtext) && *xtext != '\n') {
            if (!sdata->ltext || strcmp(xtext, sdata->ltext)) set_ptext(xtext);

            cairo_save(cairo);

            cairo_move_to(cairo, sdata->x_text, sdata->y_text);

            if (pal == WEED_PALETTE_RGBA32) {
              cairo_set_source_rgba(cairo, sdata->fg.blue / 255.0, sdata->fg.green / 255.0,
                                    sdata->fg.red / 255.0, sdata->fg_alpha);
            } else {
              cairo_set_source_rgba(cairo, sdata->fg.red / 255.0, sdata->fg.green / 255.0,
                                    sdata->fg.blue / 255.0, sdata->fg_alpha);
            }
            pango_cairo_show_layout(cairo, layout);
            cairo_restore(cairo);
          }

          if (sdata->ltext) {
            weed_free(sdata->ltext);
            sdata->ltext = NULL;
          }

          cairo_identity_matrix(cairo);
          cairo_set_operator(cairo, CAIRO_OPERATOR_OVER);
        }

        if (xfont) {
          pango_font_description_free(xfont);
          xfont = NULL;
        }

        if (layout) {
          g_object_unref(layout);
          layout = NULL;
        }
        if (xtext) {
          weed_free(xtext);
          xtext = NULL;
        }
        i = sdata->start +
            sdata->count++;
        if (sdata->count == 0) {
          if (sdata->length > 0) {
            i = sdata->start - 1;
            continue;
          }
        }
        if (sdata->needsmore) break;
        if (sdata->init == -1) break;
      } // end loop

      if (sdata->needsmore || sdata->refresh) {
        // signal host for more text
        weed_set_boolean_value(out_params[0], WEED_LEAF_VALUE, WEED_TRUE);
      } else weed_set_boolean_value(out_params[0], WEED_LEAF_VALUE, WEED_FALSE);

      weed_free(out_params); // must weed free because we got an array

      if (font) {
        pango_font_description_free(font);
        font = NULL;
      }

      // now convert cairo surface to channel pixel_data
      cairo_to_channel(inst, sdata, cairo, out_channel, in_channel);
      if (sdata->pixel_data) {
        weed_free(sdata->pixel_data);
        sdata->pixel_data = NULL;
      }
      cairo_destroy(cairo);
    }
  }
  return WEED_SUCCESS;
}



static weed_error_t pt_disp(weed_plant_t *inst, weed_plant_t *param, int inverse) {
  int stype = weed_leaf_seed_type(param, WEED_LEAF_VALUE);
  if (stype == WEED_SEED_INVALID) return WEED_ERROR_NOSUCH_LEAF;
  else {
    weed_plant_t *gui;
    double dval = weed_param_get_value_double(param);
    if (inverse == WEED_TRUE) {
      if (dval <= 0.) return WEED_ERROR_NOT_READY;
      dval = log(dval) * 5.;
    } else dval = exp(dval) / 5.;
    gui = weed_param_get_gui(param);
    return weed_set_double_value(gui, WEED_LEAF_DISPLAY_VALUE, dval);
  }
}


#define N_RFX_STRINGS 8

WEED_SETUP_START(200, 201) {
  char *rfxstrings[N_RFX_STRINGS];

  weed_plant_t *in_params[P_END + 1], *gui;
  weed_plant_t *out_params[2];
  weed_plant_t *filter_class;

  const char *modes[] = {"Spiral text", "Spinning letters", "Letter starfield", "Word coalesce",
                         "Terminal", "Word slide", "Bouncing letters", NULL
                        };
  char *deftextfile;

  int palette_list[3];
  weed_plant_t *in_chantmpls[2];
  weed_plant_t *out_chantmpls[2];
  weed_plant_t *host_info = weed_get_host_info(plugin_info);
  int filter_flags = weed_host_supports_premultiplied_alpha(host_info)
                     ? WEED_FILTER_PREF_PREMULTIPLIED_ALPHA : 0;
  int i;

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

  out_chantmpls[0] = weed_channel_template_init("out channel 0", 0);
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

  in_params[P_RANDLINES] = weed_switch_init("rand", "Select text lines _randomly", WEED_TRUE);

  in_params[P_USEBUF] = weed_switch_init("usebuff", "Use textbuffer instead of file", WEED_FALSE);
  weed_paramtmpl_set_flags(in_params[P_USEBUF], weed_paramtmpl_get_flags(in_params[P_USEBUF])
                           | WEED_PARAMETER_REINIT_ON_VALUE_CHANGE);

  in_params[P_MODE] = weed_string_list_init("mode", "Effect _mode", 0, modes);

  weed_paramtmpl_set_flags(in_params[P_MODE], weed_paramtmpl_get_flags(in_params[P_MODE])
                           | WEED_PARAMETER_REINIT_ON_VALUE_CHANGE);

  gui = weed_paramtmpl_get_gui(in_params[P_MODE]);
  weed_gui_set_flags(gui, weed_gui_get_flags(gui) | WEED_GUI_REINIT_ON_VALUE_CHANGE);

  in_params[P_SPEED] = weed_float_init("velocity", "_Speed multiplier (higher is faster)", 1.61, .2, 4.);
  gui = weed_paramtmpl_get_gui(in_params[P_SPEED]);
  if (gui) weed_set_funcptr_value(gui, WEED_LEAF_DISPLAY_VALUE_FUNC, (weed_funcptr_t)pt_disp);

  in_params[P_FONT] = weed_text_init("fontname", "_Font Name", "");

  in_params[P_FONTSCALE] = weed_float_init("fontscale", "_Font Scale", 1., .1, 10.);

  in_params[P_TEXTBUF] = weed_text_init("textbuffer", "", "");

  in_params[P_TRIGGER] = weed_switch_init("trigger", "Beat Trigger", WEED_FALSE);
  weed_paramtmpl_set_hidden(in_params[P_TRIGGER], WEED_TRUE);

  in_params[P_RANDMODE] = weed_switch_init("randmode", "Switch mode randomly", WEED_FALSE);
  gui = weed_paramtmpl_get_gui(in_params[P_RANDMODE]);
  weed_gui_set_flags(gui, weed_gui_get_flags(gui) | WEED_GUI_REINIT_ON_VALUE_CHANGE);

  in_params[P_END] = NULL;

  out_params[0] = weed_out_param_switch_init("ready_for_text", WEED_FALSE);
  out_params[1] = NULL;

  filter_class = weed_filter_class_init("puretext", "Salsaman & Aleksej Penkov", 1,
                                        filter_flags, palette_list,
                                        puretext_init, puretext_process, NULL,
                                        in_chantmpls, out_chantmpls, in_params, out_params);

  // GUI section
  for (i = 0; i < N_RFX_STRINGS; i++) rfxstrings[i] = weed_malloc(256);

  snprintf(rfxstrings[0], 256, "special|fileread|%d|", P_FILE);
  snprintf(rfxstrings[1], 256, "special|fontchooser|%d|", P_FONT);

  snprintf(rfxstrings[2], 256, "layout|p%d|p%d|", P_MODE, P_RANDMODE);
  snprintf(rfxstrings[3], 256, "layout|p%d|p%d|", P_FONT, P_FONTSCALE);
  snprintf(rfxstrings[4], 256, "layout|p%d|", P_FILE);
  snprintf(rfxstrings[5], 256, "layout|p%d|", P_RANDLINES);
  snprintf(rfxstrings[6], 256, "layout|p%d|", P_USEBUF);
  snprintf(rfxstrings[7], 256, "layout|p%d|", P_TEXTBUF);

  gui = weed_filter_get_gui(filter_class);
  weed_set_string_value(gui, WEED_LEAF_LAYOUT_SCHEME, "RFX");
  weed_set_string_value(gui, "layout_rfx_delim", "|");
  weed_set_string_array(gui, "layout_rfx_strings", N_RFX_STRINGS, rfxstrings);

  for (i = 0; i < N_RFX_STRINGS; i++) weed_free(rfxstrings[i]);

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
