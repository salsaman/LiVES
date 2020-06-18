// pangotext.c
// text handling code
// (c) A. Penkov 2010
// (c) G. Finch 2010 - 2020
// pieces of code taken and modified from scribbler.c
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

#include "main.h"
#include "pangotext.h"
#include "effects-weed.h"

#ifdef GUI_GTK
#include <pango/pangocairo.h>
static int font_cmp(const void *p1, const void *p2);
#endif


static void fill_bckg(lives_painter_t *cr, double x, double y, double dx, double dy) {
  lives_painter_new_path(cr);
  lives_painter_rectangle(cr, x, y, dx, dy);
  lives_painter_fill(cr);
  lives_painter_new_path(cr);
}


static void getxypos(LingoLayout *layout, int *px, int *py, int width, int height, boolean cent, double *pw, double *ph) {
  // calc coords of text, text will fit so it goes to bottom. Set cent to center text.

  // width and height are frame width / height in pixels
  // py, px : return locations for x,y
  // pw, ph : return locations for pango width, height

  int w_, h_;
  double d;

  // get size of layout
  lingo_layout_get_size(layout, &w_, &h_);

  // scale width, height to pixels
  d = ((double)h_) / (double)LINGO_SCALE;
  if (ph) *ph = d;

  // ypos (adjusted so text goes to bottom)
  if (py) *py = height - (int) * ph;

  d = ((double)w_) / (double)LINGO_SCALE;
  if (pw) *pw = d;

  if (px) *px = cent ? (width - (int)d) >> 1 : 0.;
}


static char *rewrap_text(char *text) {
  // find the longest line and move the last word to the following line
  // if there is no following line we add one
  // if there are no spaces in the line we truncate
  size_t maxlen = 0;

  char **lines;
  char *jtext, *tmp;
#ifdef REFLOW_TEXT
  char *first, *second;
  register int j;
#endif
  size_t ll;
  boolean needs_nl = FALSE;
  int numlines, maxline = -1;
  register int i;

  if (!text || !(*text)) return NULL;

  jtext = lives_strdup("");
  numlines = get_token_count(text, '\n');
  lines = lives_strsplit(text, "\n", numlines);

  for (i = 0; i < numlines; i++) {
    if ((ll = lives_strlen(lines[i])) > maxlen) {
      maxlen = ll;
      maxline = i;
    }
  }
  if (maxlen < 2) {
    lives_strfreev(lines);
    return jtext;
  }
  for (i = 0; i < numlines; i++) {
    if (i == maxline) {
#ifdef REFLOW_TEXT
      for (j = maxlen - 1; j > 0; j--) {
        // skip the final character - if it's a space we aren't going to move it yet
        // if it's not a space we aren't going to move it yet
        if (lines[i][j - 1] == ' ') {
          // up to and including space
          first = lives_strndup(lines[i], j);
          // after space
          second = lines[i] + j;
          tmp = lives_strdup_printf("%s%s%s\n%s", jtext, needs_nl ? "\n" : "", first, second);
          lives_free(first);
          lives_free(jtext);
          jtext = tmp;
          needs_nl = FALSE;
          break;
        }
        // no space in line, truncate last char
        lines[i][maxlen - 1] = 0;
        if (maxlen > 3)
          lives_snprintf(&lines[i][maxlen - 4], 4, "%s", "...");
        needs_nl = TRUE;
      }
#endif
      //g_print("maxlen %ld for %s\n", maxlen, lines[i]);
      lines[i][maxlen - 1] = 0;
      if (maxlen > 5)
        lives_snprintf(&lines[i][maxlen - 4], 4, "%s", "...");
      //g_print("Trying with %s\n", lines[i]);
    }
    tmp = lives_strdup_printf("%s%s%s", jtext, needs_nl ? "\n" : "", lines[i]);
    lives_free(jtext);
    jtext = tmp;
    needs_nl = TRUE;
  }
  lives_strfreev(lines);
  return jtext;
}


static char *remove_first_line(char *text) {
  int i;
  size_t tlen = lives_strlen(text);
  for (i = 0; i < tlen; i++) {
    if (text[i] == '\n') return lives_strdup(text + i + 1);
  }
  return NULL;
}


void layout_to_lives_painter(LingoLayout *layout, lives_painter_t *cr, lives_text_mode_t mode, lives_colRGBA64_t *fg,
                             lives_colRGBA64_t *bg, int dwidth, int dheight, double x_bg, double y_bg, double x_text, double y_text) {
  double b_alpha = 1.;
  double f_alpha = 1.;

  if (bg != NULL) b_alpha = (double)bg->alpha / 65535.;
  if (fg != NULL) f_alpha = (double)fg->alpha / 65535.;

  if (cr == NULL) return;

  switch (mode) {
  case LIVES_TEXT_MODE_BACKGROUND_ONLY:
    lingo_layout_set_text(layout, "", -1);
  case LIVES_TEXT_MODE_FOREGROUND_AND_BACKGROUND:
    lives_painter_set_source_rgba(cr, (double)bg->red / 66535., (double)bg->green / 66535., (double)bg->blue / 66535., b_alpha);
    fill_bckg(cr, x_bg, y_bg, dwidth, dheight);
    break;
  default:
    break;
  }

  lives_painter_new_path(cr);
  lives_painter_move_to(cr, x_text, y_text);
  lives_painter_set_source_rgba(cr, (double)fg->red / 66535., (double)fg->green / 66535., (double)fg->blue / 66535., f_alpha);
}


//#define DEBUG_MSGS
LingoLayout *layout_nth_message_at_bottom(int n, int width, int height, LiVESWidget *widget, int *linecount) {
  // create a layout, using text properties for widget
  //
  // nth message in mainw->messages should be at the bottom
  // or if there are insufficient messages then we render from message 0

  // also we want to justify text, splitting on words so that it fits width

#ifdef GUI_GTK
  LingoLayout *layout;
  LingoContext *ctx;
  weed_plant_t *msg;

  char *readytext, *testtext = NULL, *newtext = NULL, *tmp, *xx;
  size_t ll;
  weed_error_t error;

  int w = 0, h = 0, pw;
  int totlines = 0;
  int whint = 0;
  int slen;

  boolean heightdone = FALSE;
  boolean needs_newline = FALSE;

  if (width < 32 || height < MIN_MSGBAR_HEIGHT) return NULL;

  ctx = lives_widget_create_lingo_context(widget);
  if (ctx == NULL || !LINGO_IS_CONTEXT(ctx)) return NULL;

  layout = lingo_layout_new(ctx);
  if (layout == NULL || !LINGO_IS_LAYOUT(layout)) {
    lives_widget_object_unref(ctx);
    return NULL;
  }
  lives_widget_object_ref_sink(layout);

  readytext = lives_strdup("");

  msg = get_nth_info_message(n);
  if (!msg) return NULL;
  newtext = weed_get_string_value(msg, WEED_LEAF_LIVES_MESSAGE_STRING, &error);
  if (error != WEED_SUCCESS) return NULL;
  if (!newtext) return NULL;
  slen = (int)lives_strlen(newtext);
  if (slen > 0 && newtext[slen - 1] == '\n') {
    newtext[--slen] = 0;
  }
  totlines = get_token_count(newtext, '\n') + 1;
#ifdef DEBUG_MSGS
  g_print("Got msg:%s\ntotal is now %d lines\n", newtext, totlines);
#endif
  if (msg == mainw->msg_list) msg = NULL;
  else {
    msg = weed_get_plantptr_value(msg, WEED_LEAF_PREVIOUS, &error);
    if (error != WEED_SUCCESS) return NULL;
  }

#ifdef DEBUG_MSGS
  g_print("Want msg number %d at bottom\n%s", n, weed_get_string_value(msg, WEED_LEAF_LIVES_MESSAGE_STRING, &error));
#endif

  while (1) {
    if (!newtext) {
      if (!msg) break;
      newtext = weed_get_string_value(msg, WEED_LEAF_LIVES_MESSAGE_STRING, &error);
      if (error != WEED_SUCCESS) break;
      if (!newtext) break;
      totlines += get_token_count(newtext, '\n');
#ifdef DEBUG_MSGS
      g_print("Got msg:%s\ntotal is now %d lines\n", newtext, totlines);
#endif
      if (msg == mainw->msg_list) msg = NULL;
      else {
        msg = weed_get_plantptr_value(msg, WEED_LEAF_PREVIOUS, &error);
        if (error != WEED_SUCCESS) break;
      }
    }

    if (testtext) lives_free(testtext);
    testtext = lives_strdup_printf("%s%s%s", newtext, needs_newline ? "\n" : "", readytext);
    needs_newline = TRUE;
    lingo_layout_set_text(layout, "", -1);
    lives_widget_object_unref(layout);
    layout = lingo_layout_new(ctx);
    lingo_layout_set_text(layout, testtext, -1);
    lingo_layout_get_size(layout, &w, &h);

    h /= LINGO_SCALE;
    w /= LINGO_SCALE;

#ifdef DEBUG_MSGS
    g_print("Sizes %d %d window, %d %d layout ()\n", width, height, w, h);
#endif

    if (h > height) {
#ifdef DEBUG_MSGS
      g_print("Too high !\n");
#endif

      //#ifdef MUST_FIT
      while (h > height) {
        // text was too high, start removing lines from the top until it fits
        tmp = remove_first_line(newtext);
        lives_free(newtext);
        newtext = tmp;
        totlines--;
        if (!newtext) break; // no more to remove, we are done !
        //#ifdef DEBUG_MSGS
        g_print("Retry with (%d) |%s|\n", totlines, newtext);
        //#endif
        lives_free(testtext);
        testtext = lives_strdup_printf("%s%s", newtext, readytext);
#ifdef DEBUG_MSGS
        g_print("Testing with:%s:\n", testtext);
#endif
        lingo_layout_set_text(layout, "", -1);
        lives_widget_object_unref(layout);
        layout = lingo_layout_new(ctx);
        lingo_layout_set_width(layout, width * LINGO_SCALE);
        lingo_layout_set_text(layout, testtext, -1);
        lingo_layout_get_size(layout, NULL, &h);
        h /= LINGO_SCALE;
      }
      //#endif
      heightdone = TRUE;
    }

    // height was ok, now let's check the width
    if (w > width) {
      int jumpval = 1, dirn = -1, tjump = 0;
      //double nscale = 2.;
      // text was too wide
#ifdef DEBUG_MSGS
      g_print("Too wide !!!\n");
#endif
      while (1) {
        totlines -= get_token_count(newtext, '\n');
        slen = (int)lives_strlen(newtext);
        // for now we just truncate and elipsise lines
        tjump = dirn * jumpval;
        /* if (tjump >= slen && dirn == -1) { */
        /*   jumpval = slen / 2; */
        /*   tjump = dirn * jumpval; */
        /* } */
        /* g_print("pt b %d %d %d\n", tjump, dirn, jumpval); */
        if (whint == 0 || whint + 4 > slen) {
          xx = lives_strndup(newtext, slen + tjump);
        } else {
          xx = lives_strndup(newtext, whint + 4 + tjump);
        }
        tmp = rewrap_text(xx);
        lives_free(xx);
#ifdef DEBUG_MSGS
        g_print("Retry with (%d) |%s|\n", totlines, xx);
#endif
        if (tmp == NULL) break;
        // check width again, just looking at new part
        lingo_layout_set_text(layout, "", -1);
        lives_widget_object_unref(layout);
        layout = lingo_layout_new(ctx);
        lives_widget_object_ref_sink(layout);
        lingo_layout_set_text(layout, tmp, -1);
        lingo_layout_get_size(layout, &pw, NULL);
        w = pw / LINGO_SCALE;
        if (w >= width) {
          //dirn = -1;
          jumpval++;
          if (whint <= 0 || (ll = (int)lives_strlen(tmp)) < whint) whint = ll;
        } else {
          break;
        }
        /*   if (jumpval == 1) break; */
        /*   dirn = 1; */
        /*   nscale = 0.5; */
        /* } */
        /* if (jumpval > 1) jumpval = (int)(jumpval * nscale + .9); */
        lives_free(newtext);
        newtext = tmp;
      }
#ifdef DEBUG_MSGS
      g_print("Width OK now\n");
#endif
      lives_free(newtext);
      newtext = tmp;
      totlines += get_token_count(newtext, '\n');
      // width is OK again, need to recheck height
      heightdone = FALSE;
    }

    lives_free(newtext);
    newtext = NULL;
    lives_free(readytext);
    readytext = testtext;
    testtext = NULL;
#ifdef DEBUG_MSGS
    g_print("|%s| passed size tests\n", readytext);
#endif
    if (heightdone) break;
    /// height too small; prepend more text
  }

  // result is now in readytext
  //lingo_layout_set_text(layout, readytext, -1);

  if (linecount != NULL) *linecount = totlines;

#ifdef DEBUG_MSGS
  g_print("|%s| FINAL !!\n", readytext);
#endif
  lives_free(readytext);
  lives_widget_object_unref(ctx);
  return layout;
#endif
  return NULL;
}


char **get_font_list(void) {
  register int i;
  char **font_list = NULL;
#ifdef GUI_GTK
  PangoContext *ctx;
  ctx = gdk_pango_context_get();
  if (ctx) {
    PangoFontMap *pfm;
    pfm = pango_context_get_font_map(ctx);
    if (pfm) {
      int num = 0;
      PangoFontFamily **pff = NULL;
      pango_font_map_list_families(pfm, &pff, &num);
      if (num > 0) {
        font_list = (char **)lives_malloc((num + 1) * sizeof(char *));
        if (font_list) {
          for (i = 0; i < num; ++i)
            font_list[i] = lives_strdup(pango_font_family_get_name(pff[i]));
          font_list[num] = NULL;
          qsort(font_list, num, sizeof(char *), font_cmp);
        }
      }
      lives_free(pff);
    }
  }
#endif

#ifdef GUI_QT
  QFontDatabase qfd;
  QStringList qsl = qfd.families();
  font_list = (char **)lives_malloc((qsl.size() + 1) * sizeof(char *));
  for (i = 0; i < qsl.size(); i++) {
    font_list[i] = lives_strdup(qsl.at(i).toUtf8().constData());
  }
#endif

  return font_list;
}


static int font_cmp(const void *p1, const void *p2) {
  const char *s1 = (const char *)(*(char **)p1);
  const char *s2 = (const char *)(*(char **)p2);
  char *u1 = lives_utf8_casefold(s1, -1);
  char *u2 = lives_utf8_casefold(s2, -1);
  int ret = lives_strcmp_ordered(u1, u2);
  lives_free(u1);
  lives_free(u2);
  return ret;
}


LingoLayout *render_text_to_cr(LiVESWidget *widget, lives_painter_t *cr, const char *text, const char *fontname,
                               double size, lives_text_mode_t mode, lives_colRGBA64_t *fg, lives_colRGBA64_t *bg,
                               boolean center, boolean rising, double *top, int *offs_x, int dwidth, int *dheight) {
  // fontname may be eg. "Sans"

  // size is in device units, i.e. pixels

  // ypos:
  // if "rising" is TRUE, text will be aligned to fit to bottom
  // if "rising" is FALSE,  "top" (0.0 -> 1.0) is used

  // xpos:
  // aligned to left (offs_x), unless "center" is TRUE

#ifdef GUI_GTK
  PangoFontDescription *font = NULL;
#endif

  LingoLayout *layout;

  int x_pos = 0, y_pos = 0;
  double lwidth = (double)dwidth, lheight = (double) * dheight;

  if (cr == NULL) return NULL;

#ifdef GUI_GTK
  if (widget != NULL) {
    PangoContext *ctx = gtk_widget_get_pango_context(widget);
    layout = pango_layout_new(ctx);
  } else {
    layout = pango_cairo_create_layout(cr);
    if (layout == NULL) return NULL;

    font = pango_font_description_new();
    pango_font_description_set_family(font, fontname);
    pango_font_description_set_absolute_size(font, size * PANGO_SCALE);

    pango_layout_set_font_description(layout, font);
  }

  pango_layout_set_markup(layout, text, -1);
#endif

#ifdef GUI_QT
  layout = new LingoLayout(text, fontname, size);
#endif

#ifndef GUI_QT
  if (rising || center || mode == LIVES_TEXT_MODE_FOREGROUND_AND_BACKGROUND)
#endif

    getxypos(layout, &x_pos, &y_pos, dwidth, *dheight, center, &lwidth, &lheight);

  if (!rising) y_pos = (double) * dheight * *top;
  if (!center) {
    x_pos += *offs_x;
    *offs_x = x_pos;
  }

  /*  lives_painter_new_path(cr);
    lives_painter_rectangle(cr,offs_x,0,width,height);
    lives_painter_clip(cr);*/

  if (center) lingo_layout_set_alignment(layout, LINGO_ALIGN_CENTER);
  else lingo_layout_set_alignment(layout, LINGO_ALIGN_LEFT);

#ifdef GUI_GTK
  if (font != NULL) {
    pango_font_description_free(font);
  }
#endif

  if (mode == LIVES_TEXT_MODE_PRECALCULATE) {
    *dheight = lheight;
    return layout;
  }

  layout_to_lives_painter(layout, cr, mode, fg, bg, lwidth, lheight, x_pos, y_pos, x_pos, y_pos);

  return layout;
}


LIVES_GLOBAL_INLINE weed_plant_t *render_text_overlay(weed_layer_t *layer, const char *text) {
  if (!text) return layer;
  else {
    lives_colRGBA64_t col_white = lives_rgba_col_new(65535, 65535, 65535, 65535);
    lives_colRGBA64_t col_black_a = lives_rgba_col_new(0, 0, 0, SUB_OPACITY);
    int size = weed_layer_get_width(layer) / 32;
    const char *font = "Sans";
    boolean fake_gamma = FALSE;

    if (prefs->apply_gamma) {
      // leave as linear gamma maybe
      if (weed_layer_get_gamma(layer) == WEED_GAMMA_LINEAR) {
        // stops it getting converted
        weed_layer_set_gamma(layer, WEED_GAMMA_SRGB);
        fake_gamma = TRUE;
      }
    }

    layer =  render_text_to_layer(layer, text, font, size,
                                  LIVES_TEXT_MODE_FOREGROUND_AND_BACKGROUND, &col_white, &col_black_a, TRUE, FALSE, 0.1);
    if (fake_gamma)
      weed_set_int_value(layer, WEED_LEAF_GAMMA_TYPE, WEED_GAMMA_LINEAR);
  }
  return layer;
}


weed_plant_t *render_text_to_layer(weed_layer_t *layer, const char *text, const char *fontname,
                                   double size, lives_text_mode_t mode, lives_colRGBA64_t *fg_col, lives_colRGBA64_t *bg_col,
                                   boolean center, boolean rising, double top) {
  // render text to layer and return a new layer, which may have a new "rowstrides", "width" and/or "current_palette"
  // original layer pixel_data is freed in the process and should not be re-used

  lives_painter_t *cr = NULL;
  LingoLayout *layout;
  weed_layer_t *test_layer, *layer_slice;
  uint8_t *src, *pd;
  int row = weed_layer_get_rowstride(layer);
  double ztop = 0.;
  int pal = weed_layer_get_palette(layer);
  int width = weed_layer_get_width(layer);
  int height = weed_layer_get_height(layer);
  int lheight = height;
  int gamma = WEED_GAMMA_UNKNOWN, offsx = 0;

  if (weed_palette_is_rgb(pal)) {
    // test first to get the layout coords
    gamma = weed_layer_get_gamma(layer);

    lheight = height;
    weed_layer_set_height(layer, 4);

    test_layer = weed_layer_copy(NULL, layer);

    weed_layer_set_height(layer, height);
    cr = layer_to_lives_painter(test_layer);
    layout = render_text_to_cr(NULL, cr, text, fontname, size, LIVES_TEXT_MODE_PRECALCULATE,
                               fg_col, bg_col, center, rising, &top, &offsx, width, &lheight);
    if (LIVES_IS_WIDGET_OBJECT(layout)) lives_widget_object_unref(layout);

    weed_layer_free(test_layer);

    /// if possible just render the slice which contains the text
    if (top * height + lheight < height) {
      src = weed_layer_get_pixel_data_packed(layer);
      weed_layer_set_pixel_data_packed(layer, src + (int)(top * height) * row);
      weed_layer_set_height(layer, lheight);
      layer_slice = weed_layer_copy(NULL, layer);
      weed_layer_set_height(layer, height);
      weed_layer_set_pixel_data_packed(layer, src);

      cr = layer_to_lives_painter(layer_slice);
      layout = render_text_to_cr(NULL, cr, text, fontname, size, mode, fg_col, bg_col, center, FALSE, &ztop, &offsx, width, &height);
      if (layout && LINGO_IS_LAYOUT(layout)) {
        lingo_painter_show_layout(cr, layout);
        lives_widget_object_unref(layout);
      }
      // frees pd
      lives_painter_to_layer(cr, layer_slice);
      /// make sure our slice isnt freed, since it is actually part of the image
      /// which we will overwrite
      weed_leaf_set_flagbits(layer_slice, WEED_LEAF_PIXEL_DATA, LIVES_FLAG_MAINTAIN_VALUE);
      convert_layer_palette(layer_slice, pal, 0);
      weed_leaf_clear_flagbits(layer_slice, WEED_LEAF_PIXEL_DATA, LIVES_FLAG_MAINTAIN_VALUE);
      pd = weed_layer_get_pixel_data_packed(layer_slice);
      lives_memcpy(src + (int)(top * height) * row, pd, lheight * row);
      weed_layer_free(layer_slice);
    }
  }
  if (!cr) {
    cr = layer_to_lives_painter(layer);
    if (!cr) return layer; ///< error occured
    layout = render_text_to_cr(NULL, cr, text, fontname, size, mode, fg_col, bg_col, center, rising, &top, &offsx, width, &height);
    if (layout && LINGO_IS_LAYOUT(layout)) {
      lingo_painter_show_layout(cr, layout);
      if (layout) lives_widget_object_unref(layout);
    }
    lives_painter_to_layer(cr, layer);
  }
  if (gamma != WEED_GAMMA_UNKNOWN)
    weed_layer_set_gamma(layer, gamma);
  return layer;
}

static const char *cr_str = "\x0D";
static const char *lf_str = "\x0A";

//
// read appropriate text for subtitle file (.srt)
//
static char *srt_read_text(int fd, lives_subtitle_t *title) {
  char *poslf = NULL;
  char *poscr = NULL;
  char *ret = NULL;
  char data[32768];

  if (fd < 0 || !title) return NULL;

  lives_lseek_buffered_rdonly_absolute(fd, title->textpos);

  while (lives_read_buffered(fd, data, sizeof(data) - 1, TRUE) > 0) {
    // remove \n \r
    poslf = strstr(data, lf_str);
    if (poslf) *poslf = '\0';
    poscr = strstr(data, cr_str);
    if (poscr) *poscr = '\0';
    if (!(*data)) break;
    if (!ret) ret = lives_strdup(data);
    else {
      char *tmp = lives_strconcat(ret, "\n", data, NULL);
      ret = tmp;
      lives_free(ret);
    }
  }

  return ret;
}


static char *sub_read_text(int fd, lives_subtitle_t *title) {
  char *poslf = NULL;
  char *poscr = NULL;
  char *ret = NULL;
  char *retmore = NULL;
  char data[32768];
  size_t curlen, retlen;

  if (fd < 0 || !title) return NULL;

  lives_lseek_buffered_rdonly_absolute(fd, title->textpos);

  while (lives_read_buffered(fd, data, sizeof(data) - 1, TRUE) > 0) {
    // remove \n \r
    poslf = strstr(data, lf_str);
    if (poslf) *poslf = '\0';
    poscr = strstr(data, cr_str);
    if (poscr) *poscr = '\0';
    curlen = lives_strlen(data);
    if (!curlen) break;
    if (!ret) {
      ret = subst(data, "[br]", "\n");
      if (!ret) return NULL;
      retlen = lives_strlen(ret) + 1;
    } else {
      retmore = subst(data, "[br]", "\n");
      if (!retmore) return NULL;
      curlen = lives_strlen(retmore);
      if (!curlen) break;
      retlen += curlen + 1;
      ret = (char *)lives_realloc(ret, retlen);
      if (ret) {
        strcat(ret, "\n");
        strcat(ret, retmore);
        lives_free(retmore);
      } else {
        lives_free(retmore);
        return NULL;
      }
    }
  }

  return ret;
}


static boolean srt_parse_file(lives_clip_t *sfile) {
  lives_subtitle_t *node = NULL;
  lives_subtitle_t *index_prev = NULL;
  char data[32768];
  int fd = sfile->subt->tfile;

  while (!lives_read_buffered_eof(fd)) {
    char *poslf = NULL, *poscr = NULL;
    double starttime, endtime;
    int hstart, mstart, sstart, fstart;
    int hend, mend, send, fend;
    int i;

    //
    // data contains subtitle number
    //

    if (lives_read_buffered(fd, data, 32768, TRUE) < 12) {
      // EOF
      //lives_freep((void **)&sfile->subt->text);
      //sfile->subt->current = NULL;
      //sub_get_last_time(sfile->subt);
      return FALSE;
    }
    //
    // data contains time range
    //
    // remove \n \r
    poslf = strstr(data, lf_str);
    if (poslf)
      *poslf = '\0';
    poscr = strstr(data, cr_str);
    if (poscr)
      *poscr = '\0';

    // try to parse it (time range)
    i = sscanf(data, "%d:%d:%d,%d --> %d:%d:%d,%d", \
               &hstart, &mstart, &sstart, &fstart, \
               &hend, &mend, &send, &fend);
    if (i == 8) {
      // parsing ok
      starttime = hstart * 3600 + mstart * 60 + sstart + fstart / 1000.;
      endtime = hend * 3600 + mend * 60 + send + fend / 1000.;
      node = (lives_subtitle_t *)lives_malloc(sizeof(lives_subtitle_t));
      if (node) {
        node->start_time = starttime;
        node->end_time = endtime;
        node->style = NULL;
        node->next = NULL;
        node->prev = (lives_subtitle_t *)index_prev;
        node->textpos = lives_buffered_offset(fd);
        if (index_prev)
          index_prev->next = (lives_subtitle_t *)node;
        else
          sfile->subt->first = node;
        sfile->subt->last = index_prev = (lives_subtitle_t *)node;
      }
      while (lives_read_buffered(fd, data, 32768, TRUE) > 0) {
        // read the text and final empty line
        // remove \n \r
        poslf = strstr(data, lf_str);
        if (poslf)
          *poslf = '\0';
        poscr = strstr(data, cr_str);
        if (poscr)
          *poscr = '\0';

        if (!(*data)) break;
      } // end while
    } else return FALSE;
  }
  return TRUE;
}


static boolean sub_parse_file(lives_clip_t *sfile) {
  lives_subtitle_t *node = NULL;
  lives_subtitle_t *index_prev = NULL;
  char data[32768];
  int fd = sfile->subt->tfile;
  boolean starttext = FALSE;

  while (lives_read_buffered(fd, data, 32768, TRUE) > 0) {
    char *poslf = NULL, *poscr = NULL;
    int hstart, mstart, sstart, fstart;
    int hend, mend, send, fend;
    int i;
    double starttime, endtime;

    if (!strncmp(data, "[SUBTITLE]", 10)) {
      starttext = TRUE;
    }

    if (!starttext) {
      if (!strncmp(data, "[DELAY]", 7)) {
        sfile->subt->offset = atoi(data + 7);
      }
      continue;
    }

    //
    // data contains time range
    //
    // remove \n \r
    poslf = strstr(data, lf_str);
    if (poslf)
      *poslf = '\0';
    poscr = strstr(data, cr_str);
    if (poscr)
      *poscr = '\0';

    // try to parse it (time range)
    i = sscanf(data, "%d:%d:%d.%d,%d:%d:%d.%d", \
               &hstart, &mstart, &sstart, &fstart, \
               &hend, &mend, &send, &fend);
    if (i == 8) {
      // parsing ok
      starttime = hstart * 3600 + mstart * 60 + sstart + fstart / 100.;
      endtime = hend * 3600 + mend * 60 + send + fend / 100.;
      node = (lives_subtitle_t *)lives_malloc(sizeof(lives_subtitle_t));
      if (node) {
        node->start_time = starttime;
        node->end_time = endtime;
        node->style = NULL;
        node->next = NULL;
        node->prev = (lives_subtitle_t *)index_prev;
        node->textpos = lives_buffered_offset(fd);
        if (index_prev)
          index_prev->next = (lives_subtitle_t *)node;
        else
          sfile->subt->first = node;
        index_prev = (lives_subtitle_t *)node;
      }
      while (lives_read_buffered(fd, data, 32768, TRUE) > 0) {
        // read the text and final empty line
        // remove \n \r
        poslf = strstr(data, lf_str);
        if (poslf)
          *poslf = '\0';
        poscr = strstr(data, cr_str);
        if (poscr)
          *poscr = '\0';

        if (!(*data)) break;
      } // end while
    } else return FALSE;
  }
  return TRUE;
}


boolean get_subt_text(lives_clip_t *sfile, double xtime) {
  lives_subtitle_t *curr = NULL;

  if (!sfile || !sfile->subt) return FALSE;

  curr = sfile->subt->current;
  if (curr) {
    // continue showing existing text
    if (curr->start_time <= xtime && curr->end_time >= xtime) {
      if (!sfile->subt->text) {
        if (sfile->subt->type == SUBTITLE_TYPE_SRT) {
          char *tmp = srt_read_text(sfile->subt->tfile, curr);
          sfile->subt->text = lives_charset_convert(tmp, LIVES_CHARSET_UTF8, SRT_DEF_CHARSET);
          lives_free(tmp);
        } else if (sfile->subt->type == SUBTITLE_TYPE_SUB) sfile->subt->text = sub_read_text(sfile->subt->tfile, curr);
      }
      return TRUE;
    }
  }

  lives_freep((void **)&sfile->subt->text);

  if (xtime < sfile->subt->first->start_time || xtime > sfile->subt->last->end_time) {
    sfile->subt->current = NULL;
    return TRUE;
  }

  if (!curr) curr = sfile->subt->first;

  if (xtime > curr->end_time) while (curr->end_time < xtime) curr = curr->next;
  if (xtime < curr->start_time && xtime <= curr->prev->end_time) while (curr->start_time > xtime) curr = curr->prev;

  sfile->subt->current = curr;

  if (curr->start_time <= xtime && curr->end_time >= xtime) {
    if (sfile->subt->type == SUBTITLE_TYPE_SRT) {
      char *tmp = srt_read_text(sfile->subt->tfile, curr);
      sfile->subt->text = lives_charset_convert(tmp, LIVES_CHARSET_UTF8, SRT_DEF_CHARSET);
      lives_free(tmp);
    } else if (sfile->subt->type == SUBTITLE_TYPE_SUB) sfile->subt->text = sub_read_text(sfile->subt->tfile, curr);
  }

  return TRUE;
}


void subtitles_free(lives_clip_t *sfile) {
  if (!sfile) return;
  if (!sfile->subt) return;
  if (sfile->subt->tfile >= 0) lives_close_buffered(sfile->subt->tfile);

  // remove subt->first entries
  while (sfile->subt->first) {
    lives_subtitle_t *to_delete = sfile->subt->first;

    sfile->subt->first = (lives_subtitle_t *)sfile->subt->first->next;

    lives_freep((void **)&to_delete->style);
    lives_freep((void **)&to_delete);
  }

  lives_freep((void **)&sfile->subt->text);
  lives_freep((void **)&sfile->subt);
}


boolean subtitles_init(lives_clip_t *sfile, char *fname, lives_subtitle_type_t subtype) {
  // fname is the name of the subtitle file
  int fd;

  if (!sfile) return FALSE;
  if (sfile->subt) subtitles_free(sfile);
  sfile->subt = NULL;

  if ((fd = lives_open_buffered_rdonly(fname)) < 0) return FALSE;

  sfile->subt = (lives_subtitles_t *)lives_malloc(sizeof(lives_subtitles_t));
  sfile->subt->tfile = fd;
  sfile->subt->current = sfile->subt->first = NULL;
  sfile->subt->text = NULL;
  sfile->subt->last_time = -1.;
  sfile->subt->type = subtype;
  sfile->subt->offset = 0;
  if (subtype == SUBTITLE_TYPE_SRT) srt_parse_file(sfile);
  if (subtype == SUBTITLE_TYPE_SUB) sub_parse_file(sfile);
  return TRUE;
}


static void parse_double_time(double tim, int *ph, int *pmin, int *psec, int *pmsec, int fix) {
  int ntim = (int)tim;
  int h, m, s, ms;

  h = ntim / 3600;
  m = (ntim - h * 3600) / 60;
  s = (ntim - h * 3600 - m * 60);
  if (fix == 3) ms = (int)((tim - ntim) * 1000.0 + .5);
  else ms = (int)((tim - ntim) * 100.0 + .5); // hundredths
  if (ph)
    *ph = h;
  if (pmin)
    *pmin = m;
  if (psec)
    *psec = s;
  if (pmsec)
    *pmsec = ms;
}


boolean save_srt_subtitles(lives_clip_t *sfile, double start_time, double end_time, double offset_time, const char *filename) {
  lives_subtitles_t *subt = NULL;
  int64_t savepos = 0;
  int fd, num_saves;
  lives_subtitle_t *ptr = NULL;

  if (!sfile) return FALSE;
  subt = sfile->subt;
  if (!subt) return FALSE;
  if (subt->last_time <= -1.)
    get_subt_text(sfile, end_time);
  if (subt->last_time <= -1.)
    savepos = lives_buffered_offset(subt->tfile);

  // save the contents
  fd = lives_create_buffered(filename, DEF_FILE_PERMS);
  if (fd < 0) return FALSE;
  num_saves = 0;
  ptr = subt->first;
  while (ptr) {
    char *text = NULL;
    if (ptr->start_time < end_time && ptr->end_time >= start_time) {
      text = srt_read_text(subt->tfile, ptr);
      if (text) {
        int h, m, s, ms;
        double dtim;

        if (num_saves > 0) lives_write_buffered(fd, "\n", 1, TRUE);

        dtim = ptr->start_time;
        if (dtim < start_time) dtim = start_time;
        dtim += offset_time;

        parse_double_time(dtim, &h, &m, &s, &ms, 3);
        lives_buffered_write_printf(fd, TRUE, "%02d:%02d:%02d,%03d\n", h, m, s, ms);

        dtim = ptr->end_time;
        if (dtim > end_time) dtim = end_time;
        dtim += offset_time;

        parse_double_time(dtim, &h, &m, &s, &ms, 3);
        lives_buffered_write_printf(fd, TRUE, "%02d:%02d:%02d,%03d\n", h, m, s, ms);

        lives_write_buffered(fd, text, lives_strlen(text), TRUE);
        lives_free(text);
      }
    } else if (ptr->start_time >= end_time) break;
    ptr = (lives_subtitle_t *)ptr->next;
  }

  lives_close_buffered(fd);

  if (!num_saves) // don't keep the empty file
    lives_rm(filename);

  if (subt->last_time <= -1.)
    lives_lseek_buffered_rdonly_absolute(subt->tfile, savepos);

  return TRUE;
}


boolean save_sub_subtitles(lives_clip_t *sfile, double start_time, double end_time, double offset_time, const char *filename) {
  lives_subtitles_t *subt = NULL;
  int64_t savepos = 0;
  int fd, num_saves;
  lives_subtitle_t *ptr = NULL;

  if (!sfile)
    return FALSE;
  subt = sfile->subt;
  if (!subt)
    return FALSE;
  if (subt->last_time <= -1.)
    get_subt_text(sfile, end_time);
  if (subt->last_time <= -1.)
    savepos = lives_buffered_offset(subt->tfile);

  // save the contents
  fd = lives_create_buffered(filename, DEF_FILE_PERMS);
  if (fd < 0) return FALSE;
  num_saves = 0;
  ptr = subt->first;

  lives_buffered_write_printf(fd, TRUE,  "[INFORMATION]\n");
  lives_buffered_write_printf(fd, TRUE,  "[TITLE] %s\n", sfile->title);
  lives_buffered_write_printf(fd, TRUE,  "[AUTHOR] %s\n", sfile->author);
  lives_buffered_write_printf(fd, TRUE,  "[SOURCE]\n");
  lives_buffered_write_printf(fd, TRUE,  "[FILEPATH]\n");
  lives_buffered_write_printf(fd, TRUE,  "[DELAY] 0\n");
  lives_buffered_write_printf(fd, TRUE,  "[COMMENT] %s\n", sfile->comment);
  lives_buffered_write_printf(fd, TRUE,  "[END INFORMATION]\n");
  lives_buffered_write_printf(fd, TRUE,  "[SUBTITLE]\n");

  while (ptr) {
    char *text = NULL;
    char *br_text = NULL;
    if (ptr->start_time < end_time && ptr->end_time >= start_time) {
      text = sub_read_text(subt->tfile, ptr);
      if (text) {
        int h, m, s, ms;
        double dtim;
        size_t ll = lives_strlen(text) - 1;
        if (text[ll] == '\n') text[ll] = 0;

        br_text = subst(text, "\n", "[br]");
        if (br_text) {
          if (num_saves > 0) lives_write_buffered(fd, "\n", 1, TRUE);

          dtim = ptr->start_time;
          if (dtim < start_time) dtim = start_time;
          dtim += offset_time;

          parse_double_time(dtim, &h, &m, &s, &ms, 2);
          lives_buffered_write_printf(fd, TRUE,  "%02d:%02d:%02d.%02d,", h, m, s, ms);

          dtim = ptr->end_time;
          if (dtim > end_time) dtim = end_time;
          dtim += offset_time;

          parse_double_time(dtim, &h, &m, &s, &ms, 2);
          lives_buffered_write_printf(fd, TRUE,  "%02d:%02d:%02d.%02d\n", h, m, s, ms);
          lives_buffered_write_printf(fd, TRUE,  "%s\n", br_text);
          lives_free(br_text);
          num_saves++;
        }
        lives_free(text);
      }
    } else if (ptr->start_time >= end_time) break;
    ptr = (lives_subtitle_t *)ptr->next;
  }

  lives_close_buffered(fd);
  if (!num_saves) // don't keep the empty file
    lives_rm(filename);

  if (subt->last_time <= -1.)
    lives_lseek_buffered_rdonly_absolute(subt->tfile, savepos);

  return TRUE;
}

