// pangotext.c
// text handling code
// (c) A. Penkov 2010
// (c) G. Finch 2010 - 2016
// pieces of code taken and modified from scribbler.c
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

#include <stdlib.h>
#include <string.h>

#ifdef HAVE_SYSTEM_WEED
#include <weed/weed.h>
#include <weed/weed-palettes.h>
#include <weed/weed-effects.h>
#include <weed/weed-host.h>
#else
#include "../libweed/weed.h"
#include "../libweed/weed-palettes.h"
#include "../libweed/weed-effects.h"
#include "../libweed/weed-host.h"
#endif

#include "main.h"
#include "pangotext.h"

#ifdef GUI_GTK
#include <pango/pangocairo.h>
static int font_cmp(const void *p1, const void *p2);
#endif


//
// code things from scribbler.c
//
static void getxypos(LingoLayout *layout, double *px, double *py, int width, int height, boolean cent, double *pw, double *ph) {
  // calc coords of text, text will fit so it goes to bottom. Set cent to center text.

  // width and height are frame width / height in pixels
  // py, px : return locations for x,y
  // pw, ph : return locations for pango width, height

  int w_, h_;
  double d;

  // get size of layout
  lingo_layout_get_size(layout, &w_, &h_, width, height);

  // scale width, height to pixels
  if (pw)
    *pw = ((double)w_)/LINGO_SCALE;
  if (ph)
    *ph = ((double)h_)/LINGO_SCALE;

  // xpos (left or centered)

  if (cent) {
    d = ((double)w_)/LINGO_SCALE;
    d /= 2.0;
    d = (width>>1) - d;
  } else d = 0.0;

  if (px) *px = d;


  // ypos (adjusted so text goes to bottom)

  d = ((double)h_)/LINGO_SCALE;
  d = height - d;

  if (py) *py = d;
}

static void fill_bckg(lives_painter_t *cr, double x, double y, double dx, double dy) {
  lives_painter_new_path(cr);
  lives_painter_rectangle(cr, x, y, dx, dy);
  lives_painter_fill(cr);
  lives_painter_new_path(cr);
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
        font_list = (char **)lives_malloc((num+1)*sizeof(char *));
        if (font_list) {
          for (i = 0; i < num; ++i)
            font_list[i] = strdup(pango_font_family_get_name(pff[i]));
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
    font_list[i] = strdup(qsl.at(i).toUtf8().constData());
  }
#endif

  return font_list;
}



#ifdef GUI_GTK
static int font_cmp(const void *p1, const void *p2) {
  const char *s1 = (const char *)(*(char **)p1);
  const char *s2 = (const char *)(*(char **)p2);
  char *u1 = g_utf8_casefold(s1, -1);
  char *u2 = g_utf8_casefold(s2, -1);
  int ret = strcmp(u1, u2);
  lives_free(u1);
  lives_free(u2);
  return ret;
}
#endif



LingoLayout *render_text_to_cr(LiVESWidget *widget, lives_painter_t *cr, const char *text, const char *fontname,
                               double size, lives_text_mode_t mode, lives_colRGBA64_t *fg, lives_colRGBA64_t *bg,
                               boolean center, boolean rising, double top, int offs_x, int width, int height) {

  // fontname may be eg. "Sans"

  // ypos:
  // if "rising" is TRUE, text will be aligned to fit to bottom
  // if "rising" is FALSE,  "top" (0.0 -> 1.0) is used

  // xpos:
  // aligned to left (offs_x), unless "center" is TRUE

#ifdef GUI_GTK
  PangoFontDescription *font=NULL;
#endif

  LingoLayout *layout;

  double x_pos, y_pos;
  double x_text, y_text;
  double dwidth, dheight;

  double b_alpha=1.;
  double f_alpha=1.;

  if (bg!=NULL) b_alpha=(double)bg->alpha/65535.;
  if (fg!=NULL) f_alpha=(double)fg->alpha/65535.;

  if (cr==NULL) return NULL;

#ifdef GUI_GTK
  if (widget!=NULL) {
    PangoContext *ctx=gtk_widget_get_pango_context(widget);
    layout=pango_layout_new(ctx);
  } else {
    layout = pango_cairo_create_layout(cr);
    if (layout==NULL) return NULL;

    font = pango_font_description_new();
    pango_font_description_set_family(font, fontname);
    pango_font_description_set_absolute_size(font, size*PANGO_SCALE);

    pango_layout_set_font_description(layout, font);
  }
  pango_layout_set_text(layout, text, -1);
#endif

#ifdef GUI_QT
  layout = new LingoLayout(text, fontname, size);
#endif

  getxypos(layout, &x_pos, &y_pos, width, height, center, &dwidth, &dheight);

  if (!rising) y_pos = y_text = height*top;

  if (!center) x_pos+=offs_x;

  x_text = x_pos;
  y_text = y_pos;

  /*  lives_painter_new_path(cr);
  lives_painter_rectangle(cr,offs_x,0,width,height);
  lives_painter_clip(cr);*/

  if (center) lingo_layout_set_alignment(layout, LINGO_ALIGN_CENTER);
  else lingo_layout_set_alignment(layout, LINGO_ALIGN_LEFT);

  switch (mode) {
  case LIVES_TEXT_MODE_BACKGROUND_ONLY:
    lingo_layout_set_text(layout, "", -1);
  case LIVES_TEXT_MODE_FOREGROUND_AND_BACKGROUND:
    lives_painter_set_source_rgba(cr,(double)bg->red/66535., (double)bg->green/66535., (double)bg->blue/66535., b_alpha);
    fill_bckg(cr, x_pos, y_pos, dwidth, dheight);
    break;
  default:
    break;
  }

  lives_painter_new_path(cr);
  lives_painter_move_to(cr, x_text, y_text);
  lives_painter_set_source_rgba(cr,(double)fg->red/66535., (double)fg->green/66535., (double)fg->blue/66535., f_alpha);

#ifdef GUI_QT
  lingo_layout_set_coords(layout, x_pos, y_pos, dwidth, dheight);
#endif

#ifdef GUI_GTK
  if (font!=NULL) {
    pango_font_description_free(font);
  }
#endif

  return layout;
}





weed_plant_t *render_text_to_layer(weed_plant_t *layer, const char *text, const char *fontname,
                                   double size, lives_text_mode_t mode, lives_colRGBA64_t *fg_col, lives_colRGBA64_t *bg_col,
                                   boolean center, boolean rising, double top) {
  // render text to layer and return a new layer, which may have a new "rowstrides", "width" and/or "current_palette"
  // original layer is freed in the process and should not be used

  lives_painter_t *cr;

  lives_painter_surface_t *surface;

  void *src;

  LingoLayout *layout;

  int width, height, error;

  width=weed_get_int_value(layer,"width",&error);
  height=weed_get_int_value(layer,"height",&error);

  // do cairo and pango things

  cr=layer_to_lives_painter(layer);
  if (cr==NULL) return layer; ///< error occured

  layout = render_text_to_cr(NULL,cr,text,fontname,size,mode,fg_col,bg_col,center,rising,top,0,width,height);

  lingo_painter_show_layout(cr, layout);

  // do not !!
  //lives_painter_paint(cr);

  lives_painter_to_layer(cr,layer);

  if (layout) lives_object_unref(layout);

  surface=lives_painter_get_target(cr);
  src=lives_painter_image_surface_get_data(surface);
  lives_free(src);
  lives_painter_destroy(cr);
  return layer;
}





static const char *cr_str = "\x0D";
static const char *lf_str = "\x0A";

//
// read appropriate text for subtitle file (.srt)
//
static char *srt_read_text(FILE *pf, lives_subtitle_t *title) {
  char *poslf = NULL;
  char *poscr = NULL;
  char *ret = NULL;
  char data[32768];
  size_t curlen;

  int64_t curpos;

  if (!pf || !title)
    return NULL;

  curpos=ftell(pf);

  if (fseek(pf, title->textpos, SEEK_SET) == -1)
    return NULL;

  while (fgets(data, sizeof(data)-1, pf)) {
    // remove \n \r
    poslf = strstr(data, lf_str);
    if (poslf)
      *poslf = '\0';
    poscr = strstr(data, cr_str);
    if (poscr)
      *poscr = '\0';
    curlen = strlen(data);
    if (!curlen)
      break;
    strcat(data,"\n");
    curlen = strlen(data);
    if (!ret) {
      ret = (char *)lives_malloc(curlen+1);
      if (ret)
        strcpy(ret, data);
      else {
        fseek(pf, curpos, SEEK_SET);
        return NULL;
      }
    } else {
      ret = (char *)lives_realloc(ret, curlen+1+strlen(ret));
      if (ret)
        strcat(ret, data);
      else {
        fseek(pf, curpos, SEEK_SET);
        return NULL;
      }
    }
  }

  fseek(pf, curpos, SEEK_SET);
  return ret;
}


static char *sub_read_text(FILE *pf, lives_subtitle_t *title) {
  char *poslf = NULL;
  char *poscr = NULL;
  char *ret = NULL;
  char *retmore = NULL;
  char data[32768];
  size_t curlen;

  int64_t curpos;

  if (!pf || !title)
    return NULL;

  curpos=ftell(pf);

  if (fseek(pf, title->textpos, SEEK_SET) == -1)
    return NULL;

  while (fgets(data, sizeof(data)-1, pf)) {
    // remove \n \r
    poslf = strstr(data, lf_str);
    if (poslf)
      *poslf = '\0';
    poscr = strstr(data, cr_str);
    if (poscr)
      *poscr = '\0';
    curlen = strlen(data);
    if (!curlen)
      break;
    strcat(data,"\n");
    if (!ret) {
      ret = subst(data,"[br]","\n");
      if (!ret) {
        fseek(pf, curpos, SEEK_SET);
        return NULL;
      }
    } else {
      retmore = subst(data,"[br]","\n");
      if (!retmore) {
        fseek(pf, curpos, SEEK_SET);
        return NULL;
      }
      ret = (char *)lives_realloc(ret, strlen(retmore)+1+strlen(ret));
      if (ret)
        strcat(ret, retmore);
      else {
        fseek(pf, curpos, SEEK_SET);
        lives_free(retmore);
        return NULL;
      }
      lives_free(retmore);
    }
  }

  fseek(pf, curpos, SEEK_SET);
  return ret;
}



static void sub_get_last_time(lives_subtitles_t *subt) {
  lives_subtitle_t *xsubt;
  if (subt==NULL) return;

  if (subt->current!=NULL) xsubt=subt->current;
  else xsubt = subt->index;

  while (xsubt!=NULL) {
    subt->last_time=xsubt->end_time;
    xsubt = (lives_subtitle_t *)xsubt->next;
  }
}

// read .srt files
boolean get_srt_text(lives_clip_t *sfile, double xtime) {
  lives_subtitle_t *index = NULL;
  lives_subtitle_t *index_ptr = NULL;
  lives_subtitle_t *index_prev = NULL;
  lives_subtitle_t *node = NULL;
  lives_subtitle_t *curr = NULL;
  FILE *pf = NULL;
  char data[32768];

  if (!sfile)
    return  FALSE;

  if (!sfile->subt)
    return  FALSE;

  curr = sfile->subt->current;

  if (curr && (curr->start_time <= xtime) && (curr->end_time >= xtime))
    return (TRUE);

  if (sfile->subt->last_time!=-1. && xtime>sfile->subt->last_time) {
    // past end of subtitles
    if (sfile->subt->text!=NULL) lives_free(sfile->subt->text);
    sfile->subt->text=NULL;
    sfile->subt->current=NULL;
    return TRUE;
  }


  if (curr && (curr->start_time <= xtime))
    index_ptr = index_prev = index = curr;
  else
    index_ptr = index_prev = index = sfile->subt->index;

  while (index_ptr) {
    if (index_ptr->start_time > xtime) {
      if (sfile->subt->text!=NULL) lives_free(sfile->subt->text);
      sfile->subt->text=NULL;
      sfile->subt->current = NULL;
      return (TRUE);
    }
    if (index_ptr->end_time >= xtime) {
      sfile->subt->current = index_ptr;
      if (sfile->subt->text!=NULL) lives_free(sfile->subt->text);
      sfile->subt->text=srt_read_text(sfile->subt->tfile,sfile->subt->current);
      return (TRUE);
    }
    index_prev = index_ptr;
    index_ptr = (lives_subtitle_t *)index_ptr->next;
  }

  pf = sfile->subt->tfile;

  while (fgets(data,sizeof(data), pf)) {
    char *poslf = NULL, *poscr = NULL;
    int hstart, mstart, sstart, fstart;
    int hend, mend, send, fend;
    int i;
    double starttime, endtime;

    //
    // data contains subtitle number
    //

    if (!fgets(data,sizeof(data), pf)) {
      // EOF
      if (sfile->subt->text!=NULL) lives_free(sfile->subt->text);
      sfile->subt->text=NULL;
      sfile->subt->current=NULL;
      sub_get_last_time(sfile->subt);
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
    i = sscanf(data,"%d:%d:%d,%d --> %d:%d:%d,%d",\
               &hstart, &mstart, &sstart, &fstart,\
               &hend, &mend, &send, &fend);
    if (i == 8) {
      // parsing ok
      starttime = hstart*3600 + mstart*60 + sstart + fstart/1000.;
      endtime = hend*3600 + mend*60 + send + fend/1000.;
      node = (lives_subtitle_t *)lives_malloc(sizeof(lives_subtitle_t));
      if (node) {
        node->start_time = starttime;
        node->end_time = endtime;
        node->style = NULL;
        node->next = NULL;
        node->prev = (lives_subtitle_t *)index_prev;
        node->textpos=ftell(pf);
        if (index_prev)
          index_prev->next = (lives_subtitle_t *)node;
        else
          sfile->subt->index = node;
        index_prev = (lives_subtitle_t *)node;
      }
      while (fgets(data, sizeof(data), pf)) {
        // read the text and final empty line
        // remove \n \r
        poslf = strstr(data, lf_str);
        if (poslf)
          *poslf = '\0';
        poscr = strstr(data, cr_str);
        if (poscr)
          *poscr = '\0';

        if (!strlen(data)) break;
      } // end while

      if (node) {
        if (node->start_time > xtime) {
          if (sfile->subt->text!=NULL) lives_free(sfile->subt->text);
          sfile->subt->text=NULL;
          sfile->subt->current = NULL;
          return TRUE;
        }
        if (node->end_time >= xtime) {
          if (sfile->subt->text!=NULL) lives_free(sfile->subt->text);
          sfile->subt->current = node;
          sfile->subt->text=srt_read_text(sfile->subt->tfile,sfile->subt->current);
          return TRUE;
        }
      }
    } else {
      // What to do here ? Probably wrong format
      continue;
    }
  }

  // EOF
  sfile->subt->current=NULL;
  if (sfile->subt->text!=NULL) lives_free(sfile->subt->text);
  sfile->subt->text=NULL;
  sub_get_last_time(sfile->subt);
  return FALSE;
}



// read .sub files
boolean get_sub_text(lives_clip_t *sfile, double xtime) {
  lives_subtitle_t *index = NULL;
  lives_subtitle_t *index_ptr = NULL;
  lives_subtitle_t *index_prev = NULL;
  lives_subtitle_t *node = NULL;
  lives_subtitle_t *curr = NULL;
  FILE *pf = NULL;
  char data[32768];
  boolean starttext;

  if (!sfile)
    return  FALSE;

  if (!sfile->subt)
    return  FALSE;

  curr = sfile->subt->current;

  if (curr && (curr->start_time <= xtime) && (curr->end_time >= xtime))
    return (TRUE);

  if (sfile->subt->last_time!=-1. && xtime>sfile->subt->last_time) {
    // past end of subtitles
    if (sfile->subt->text!=NULL) lives_free(sfile->subt->text);
    sfile->subt->text=NULL;
    sfile->subt->current=NULL;
    return TRUE;
  }


  if (curr && (curr->start_time <= xtime))
    index_ptr = index_prev = index = curr;
  else
    index_ptr = index_prev = index = sfile->subt->index;

  while (index_ptr) {
    if (index_ptr->start_time > xtime) {
      if (sfile->subt->text!=NULL) lives_free(sfile->subt->text);
      sfile->subt->text=NULL;
      sfile->subt->current = NULL;
      return (TRUE);
    }
    if (index_ptr->end_time >= xtime) {
      sfile->subt->current = index_ptr;
      if (sfile->subt->text!=NULL) lives_free(sfile->subt->text);
      sfile->subt->text=sub_read_text(sfile->subt->tfile,sfile->subt->current);
      return (TRUE);
    }
    index_prev = index_ptr;
    index_ptr = (lives_subtitle_t *)index_ptr->next;
  }

  pf = sfile->subt->tfile;

  starttext = (sfile->subt->index!=NULL);

  while (fgets(data,sizeof(data), pf)) {
    char *poslf = NULL, *poscr = NULL;
    int hstart, mstart, sstart, fstart;
    int hend, mend, send, fend;
    int i;
    double starttime, endtime;

    if (!strncmp(data, "[SUBTITLE]", 10)) {
      starttext = TRUE;
    }

    if (!starttext) {
      if (!strncmp(data,"[DELAY]",7)) {
        sfile->subt->offset=atoi(data+7);
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
    i = sscanf(data,"%d:%d:%d.%d,%d:%d:%d.%d",\
               &hstart, &mstart, &sstart, &fstart,\
               &hend, &mend, &send, &fend);
    if (i == 8) {
      // parsing ok
      starttime = hstart*3600 + mstart*60 + sstart + fstart/100.;
      endtime = hend*3600 + mend*60 + send + fend/100.;
      node = (lives_subtitle_t *)lives_malloc(sizeof(lives_subtitle_t));
      if (node) {
        node->start_time = starttime;
        node->end_time = endtime;
        node->style = NULL;
        node->next = NULL;
        node->prev = (lives_subtitle_t *)index_prev;
        node->textpos=ftell(pf);
        if (index_prev)
          index_prev->next = (lives_subtitle_t *)node;
        else
          sfile->subt->index = node;
        index_prev = (lives_subtitle_t *)node;
      }
      while (fgets(data, sizeof(data), pf)) {
        // read the text and final empty line
        // remove \n \r
        poslf = strstr(data, lf_str);
        if (poslf)
          *poslf = '\0';
        poscr = strstr(data, cr_str);
        if (poscr)
          *poscr = '\0';

        if (!strlen(data)) break;
      } // end while

      if (node) {
        if (node->start_time > xtime) {
          if (sfile->subt->text!=NULL) lives_free(sfile->subt->text);
          sfile->subt->text=NULL;
          sfile->subt->current = NULL;
          return TRUE;
        }
        if (node->end_time >= xtime) {
          if (sfile->subt->text!=NULL) lives_free(sfile->subt->text);
          sfile->subt->current = node;
          sfile->subt->text=sub_read_text(sfile->subt->tfile,sfile->subt->current);
          return TRUE;
        }
      }
    } else {
      continue;
    }
  }

  // EOF
  sfile->subt->current=NULL;
  if (sfile->subt->text!=NULL) lives_free(sfile->subt->text);
  sfile->subt->text=NULL;
  sub_get_last_time(sfile->subt);
  return TRUE;
}

///

void subtitles_free(lives_clip_t *sfile) {
  if (sfile==NULL) return;
  if (sfile->subt==NULL) return;

  if (sfile->subt->tfile!=NULL) fclose(sfile->subt->tfile);

  // remove subt->index entries
  while (sfile->subt->index) {
    lives_subtitle_t *to_delete = sfile->subt->index;

    sfile->subt->index = (lives_subtitle_t *)sfile->subt->index->next;

    if (to_delete->style != NULL) lives_free(to_delete->style);
    lives_free(to_delete);
  }

  if (sfile->subt->text!=NULL) lives_free(sfile->subt->text);

  lives_free(sfile->subt);
  sfile->subt=NULL;
}


boolean subtitles_init(lives_clip_t *sfile, char *fname, lives_subtitle_type_t subtype) {
  // fname is the name of the subtitle file
  FILE *tfile;

  if (sfile==NULL) return FALSE;

  if (sfile->subt!=NULL) subtitles_free(sfile);

  sfile->subt=NULL;

  if ((tfile=fopen(fname,"r"))==NULL) return FALSE;

  sfile->subt=(lives_subtitles_t *)lives_malloc(sizeof(lives_subtitles_t));

  sfile->subt->tfile=tfile;

  sfile->subt->current=sfile->subt->index = NULL;

  sfile->subt->text=NULL;

  sfile->subt->last_time=-1.;

  sfile->subt->type=subtype;

  sfile->subt->offset=0;

  return TRUE;
}

static void parse_double_time(double tim, int *ph, int *pmin, int *psec, int *pmsec, int fix) {
  int ntim = (int)tim;
  int h, m, s, ms;

  h = ntim/3600;
  m = (ntim-h*3600)/60;
  s = (ntim-h*3600-m*60);
  if (fix==3) ms = (int)((tim-ntim)*1000.0+.5);
  else ms = (int)((tim-ntim)*100.0+.5); // hundredths
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
  lives_subtitles_t *subt=NULL;
  int64_t savepos = 0;
  FILE *pf;
  int num_saves;
  lives_subtitle_t *ptr = NULL;

  if (!sfile)
    return FALSE;
  subt = sfile->subt;
  if (!subt)
    return FALSE;
  if (subt->last_time <= -1.)
    get_srt_text(sfile, end_time);
  if (subt->last_time <= -1.)
    savepos = ftell(subt->tfile);

  // save the contents
  pf = fopen(filename, "w");
  if (!pf)
    return FALSE;
  num_saves = 0;
  ptr = subt->index;
  while (ptr) {
    char *text = NULL;
    if (ptr->start_time < end_time && ptr->end_time >= start_time) {
      text = srt_read_text(subt->tfile, ptr);
      if (text) {
        int h, m, s, ms;
        double dtim;

        if (num_saves>0) fprintf(pf,"\n");

        fprintf(pf, "%d\n", ++num_saves);

        dtim = ptr->start_time;
        if (dtim < start_time) dtim=start_time;
        dtim += offset_time;

        parse_double_time(dtim, &h, &m, &s, &ms, 3);
        fprintf(pf, "%02d:%02d:%02d,%03d --> ", h, m, s, ms);

        dtim = ptr->end_time;
        if (dtim > end_time) dtim=end_time;
        dtim += offset_time;

        parse_double_time(dtim, &h, &m, &s, &ms, 3);
        fprintf(pf, "%02d:%02d:%02d,%03d\n", h, m, s, ms);

        fprintf(pf, "%s", text);
        lives_free(text);
      }
    } else if (ptr->start_time>=end_time) break;
    ptr = (lives_subtitle_t *)ptr->next;
  }

  fclose(pf);
  if (!num_saves) // don't keep the empty file
    lives_rm(filename);

  if (subt->last_time <= -1.)
    fseek(subt->tfile, savepos, SEEK_SET);

  return TRUE;
}

boolean save_sub_subtitles(lives_clip_t *sfile, double start_time, double end_time, double offset_time, const char *filename) {
  lives_subtitles_t *subt=NULL;
  int64_t savepos = 0;
  FILE *pf;
  int num_saves;
  lives_subtitle_t *ptr = NULL;

  if (!sfile)
    return FALSE;
  subt = sfile->subt;
  if (!subt)
    return FALSE;
  if (subt->last_time <= -1.)
    get_sub_text(sfile, end_time);
  if (subt->last_time <= -1.)
    savepos = ftell(subt->tfile);

  // save the contents
  pf = fopen(filename, "w");
  if (!pf)
    return FALSE;
  num_saves = 0;
  ptr = subt->index;

  fprintf(pf, "[INFORMATION]\n");
  fprintf(pf, "[TITLE] %s\n", sfile->title);
  fprintf(pf, "[AUTHOR] %s\n", sfile->author);
  fprintf(pf, "[SOURCE]\n");
  fprintf(pf, "[FILEPATH]\n");
  fprintf(pf, "[DELAY] 0\n");
  fprintf(pf, "[COMMENT] %s\n", sfile->comment);
  fprintf(pf, "[END INFORMATION]\n");
  fprintf(pf, "[SUBTITLE]\n");


  while (ptr) {
    char *text = NULL;
    char *br_text = NULL;
    if (ptr->start_time < end_time && ptr->end_time >= start_time) {
      text = sub_read_text(subt->tfile, ptr);
      if (text) {
        int h, m, s, ms;
        double dtim;

        if (!strncmp(text+strlen(text)-1,"\n",1)) memset(text+strlen(text)-1,0,1);

        br_text = subst(text, "\n", "[br]");
        if (br_text) {
          if (num_saves>0) fprintf(pf,"\n");

          dtim = ptr->start_time;
          if (dtim < start_time) dtim=start_time;
          dtim += offset_time;

          parse_double_time(dtim, &h, &m, &s, &ms, 2);
          fprintf(pf, "%02d:%02d:%02d.%02d,", h, m, s, ms);

          dtim = ptr->end_time;
          if (dtim > end_time) dtim=end_time;
          dtim += offset_time;

          parse_double_time(dtim, &h, &m, &s, &ms, 2);
          fprintf(pf, "%02d:%02d:%02d.%02d\n", h, m, s, ms);
          fprintf(pf, "%s\n", br_text);
          lives_free(br_text);
          num_saves++;
        }
        lives_free(text);
      }
    } else if (ptr->start_time>=end_time) break;
    ptr = (lives_subtitle_t *)ptr->next;
  }

  fclose(pf);
  if (!num_saves) // don't keep the empty file
    lives_rm(filename);

  if (subt->last_time <= -1.)
    fseek(subt->tfile, savepos, SEEK_SET);

  return TRUE;

}

