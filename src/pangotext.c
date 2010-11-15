// pangotext.c
// text handling code
// (c) A. Penkov 2010
// pieces of code taken and modified from scribbler.c
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

#include <stdlib.h>
#include <string.h>

#include <pango/pangocairo.h>

#include "support.h"
#include "main.h"
#include "pangotext.h"

#ifdef HAVE_SYSTEM_WEED
#include "weed/weed.h"
#include "weed/weed-palettes.h"
#include "weed/weed-effects.h"
#include "weed/weed-host.h"
#else
#include "../libweed/weed.h"
#include "../libweed/weed-palettes.h"
#include "../libweed/weed-effects.h"
#include "../libweed/weed-host.h"
#endif

static int font_cmp(const void *p1, const void *p2); 

//
// code things from scribbler.c
//
static void getxypos(PangoLayout *layout, double *px, double *py, int weight, int height, int cent, double *pw, double *ph)
{
  int w_, h_;
  double d;
  pango_layout_get_size(layout, &w_, &h_);
  if(pw)
    *pw = ((double)w_)/PANGO_SCALE;
  if(ph)
    *ph = ((double)h_)/PANGO_SCALE;

  if(cent) {
    d = ((double)w_)/PANGO_SCALE;
    d /= 2.0;
    d = (weight>>1) - d;
  }
  else
    d = 0.0;
  if(px)
    *px = d;
  
  d = ((double)h_)/PANGO_SCALE;
  d = height - d;
  if(py)
    *py = d;
}

static void fill_bckg(cairo_t *cr, double x, double y, double dx, double dy) {
  cairo_rectangle(cr, x, y, dx, dy);
  cairo_fill(cr);
}


char **get_font_list(void) {
  char **font_list = NULL;
  PangoContext *ctx;
  ctx = gdk_pango_context_get();
  if(ctx) {
    PangoFontMap *pfm;
    pfm = pango_context_get_font_map(ctx);
    if(pfm) {
      int num = 0;
      PangoFontFamily **pff = NULL;
      pango_font_map_list_families(pfm, &pff, &num);
      if(num > 0) {
        font_list = (char **)g_malloc((num+1)*sizeof(char*));
        if(font_list) {
          int i;
          for(i = 0; i < num; ++i)
            font_list[i] = strdup(pango_font_family_get_name(pff[i]));
          font_list[num] = NULL;
          qsort(font_list, num, sizeof(char *), font_cmp);
        }
      }
      g_free(pff);
    }
  }
  return font_list;
}

int font_cmp(const void *p1, const void *p2) {  
  const char *s1 = (const char *)(*(char **)p1);
  const char *s2 = (const char *)(*(char **)p2);
  return(strcasecmp(s1, s2));
}


gboolean render_text_to_layer(weed_plant_t *layer, const char *text, const char *fontname,\
  double size, lives_text_mode_t mode, lives_colRGBA32_t *fg_col, lives_colRGBA32_t *bg_col,\
  gboolean center, gboolean rising, double top) {
  int error;
  gboolean ret = FALSE;

  int cent,rise;

  int width, height, palette;

  double dwidth, dheight;
  lives_colRGBA32_t *fg, *bg;

  double b_alpha=(double)bg_col->alpha/255.;
  double f_alpha=(double)fg_col->alpha/255.;

  void **pixel_data = NULL;

  GdkPixbuf *pixbuf = NULL;
  GdkPixbuf *pixbuf_new = NULL;
  int alpha_threshold = 0;

  if(weed_plant_has_leaf(layer, "pixel_data"))
    pixel_data = weed_get_voidptr_array(layer, "pixel_data", &error);

  width=weed_get_int_value(layer,"width",&error);
  height=weed_get_int_value(layer,"height",&error);

  palette=weed_get_int_value(layer,"current_palette",&error);

 
  fg = fg_col;
  bg = bg_col;
  if (palette==WEED_PALETTE_BGR24 || palette==WEED_PALETTE_BGRA32) {
    int tmp=fg->red;
    fg->red=fg->blue;
    fg->blue=tmp;

    tmp=bg->red;
    bg->red=bg->blue;
    bg->blue=tmp;
  }


  // THINGS TO TO WITH TEXTS AND PANGO
  pixbuf = layer_to_pixbuf(layer);

  cent = center ? 1 : 0;
  rise = rising ? 1 : 0;

  if(pixbuf) {
    // do cairo and pango things
    GdkPixmap *pixmap = NULL;
    gdk_pixbuf_render_pixmap_and_mask(pixbuf, &pixmap, NULL, alpha_threshold);
    if(pixmap) {
      cairo_t *cairo;
      cairo = gdk_cairo_create(pixmap);
      if(cairo) {
        PangoLayout *layout;
        layout = pango_cairo_create_layout(cairo);
        if(layout) { 
          PangoFontDescription *font;
          double x_pos, y_pos;
          double x_text, y_text;
          gboolean result;

          font = pango_font_description_new();
          pango_font_description_set_family(font, fontname);
          pango_font_description_set_absolute_size(font, size*PANGO_SCALE);

          pango_layout_set_font_description(layout, font);
          pango_layout_set_text(layout, text, -1);
          getxypos(layout, &x_pos, &y_pos, width, height, cent, &dwidth, &dheight);

          if(!rise)
            y_pos = y_text = height*top;

          x_text = x_pos;
          y_text = y_pos;

          if (cent) pango_layout_set_alignment(layout, PANGO_ALIGN_CENTER);
          else pango_layout_set_alignment(layout, PANGO_ALIGN_LEFT);

          cairo_move_to(cairo, x_text, y_text);

          switch(mode) {
          case LIVES_TEXT_MODE_FOREGROUND_AND_BACKGROUND:
                 cairo_set_source_rgba(cairo,bg->red, bg->green, bg->blue, b_alpha);
                 fill_bckg(cairo, x_pos, y_pos, dwidth, dheight);
                 cairo_move_to(cairo, x_text, y_text);
                 cairo_set_source_rgba(cairo,fg->red, fg->green, fg->blue, f_alpha);
                 break;
          case LIVES_TEXT_MODE_BACKGROUND_ONLY:
                 cairo_set_source_rgba(cairo,bg->red, bg->green, bg->blue, b_alpha);
                 fill_bckg(cairo, x_pos, y_pos, dwidth, dheight);
                 cairo_move_to(cairo, x_pos, y_pos);
                 cairo_set_source_rgba(cairo,fg->red, fg->green, fg->blue, f_alpha);
                 pango_layout_set_text(layout, "", -1);
                 break;
          case LIVES_TEXT_MODE_FOREGROUND_ONLY:
          default:
                 cairo_set_source_rgba(cairo,fg->red, fg->green, fg->blue, f_alpha);
                 break;
          }

          pango_cairo_show_layout(cairo, layout);

          pixbuf_new = gdk_pixbuf_get_from_drawable(pixbuf, pixmap, NULL,\
              0, 0,\
              0, 0,\
              -1, -1);
          result = pixbuf_to_layer(layer, pixbuf_new);

	  // nasty cleanup because of the way we handle pixel_data
	  if (result) {
	    mainw->do_not_free=gdk_pixbuf_get_pixels(pixbuf_new);
	    mainw->free_fn=lives_free_with_check;
	  }
	  g_object_unref(pixbuf_new);
	  mainw->do_not_free=NULL;
	  mainw->free_fn=free;
	  ///////////////////////////////////////////

          g_object_unref(layout);
          pango_font_description_free(font);
          ret = TRUE;
        }
        cairo_destroy(cairo);
      }
      g_object_unref(pixmap);
    }
  }
  
  weed_free(pixel_data);
  return ret;
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

  long curpos;

  if(!pf || !title)
    return(FALSE);

  curpos=ftell(pf);

  if(fseek(pf, title->textpos, SEEK_SET) == -1)
    return(FALSE);

  while(fgets(data, sizeof(data)-1, pf)) {
    // remove \n \r
    poslf = strstr(data, lf_str);
    if(poslf)
      *poslf = '\0';
    poscr = strstr(data, cr_str);
    if(poscr)
      *poscr = '\0';
    curlen = strlen(data);
    if(!curlen)
      break;
    strcat(data,"\n");
    curlen = strlen(data);
    if(!ret) {
      ret = (char *)g_malloc(curlen+1);
      if(ret)
        strcpy(ret, data);
      else {
	fseek(pf, curpos, SEEK_SET);
        return(FALSE);
      }
    }
    else {
      ret = (char *)g_realloc(ret, curlen+1+strlen(ret));
      if(ret)
        strcat(ret, data);
      else {
	fseek(pf, curpos, SEEK_SET);
        return(FALSE);
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

  long curpos;

  if(!pf || !title)
    return(FALSE);

  curpos=ftell(pf);

  if(fseek(pf, title->textpos, SEEK_SET) == -1)
    return(FALSE);

  while(fgets(data, sizeof(data)-1, pf)) {
    // remove \n \r
    poslf = strstr(data, lf_str);
    if(poslf)
      *poslf = '\0';
    poscr = strstr(data, cr_str);
    if(poscr)
      *poscr = '\0';
    curlen = strlen(data);
    if(!curlen)
      break;
    strcat(data,"\n");
    if(!ret) {
      ret = subst(data,"[br]","\n");
      if(!ret) {
	fseek(pf, curpos, SEEK_SET);
        return(FALSE);
      }
    }
    else {
      retmore = subst(data,"[br]","\n");
      if(!retmore) {
	fseek(pf, curpos, SEEK_SET);
        return(FALSE);
      }
      ret = (char *)g_realloc(ret, strlen(retmore)+1+strlen(ret));
      if(ret)
        strcat(ret, retmore);
      else {
	fseek(pf, curpos, SEEK_SET);
        g_free(retmore);
        return(FALSE);
      }
      g_free(retmore);
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
gboolean get_srt_text(file *sfile, double xtime) {
  lives_subtitle_t *index = NULL;
  lives_subtitle_t *index_ptr = NULL;
  lives_subtitle_t *index_prev = NULL;
  lives_subtitle_t *node = NULL;
  lives_subtitle_t *curr = NULL;
  FILE *pf = NULL;
  char data[32768];

  if(!sfile)
    return (FALSE);

  if(!sfile->subt)
    return (FALSE);

  curr = sfile->subt->current;

  if(curr && (curr->start_time <= xtime) && (curr->end_time >= xtime))
    return (TRUE);

  if (sfile->subt->last_time!=-1. && xtime>sfile->subt->last_time) {
    // past end of subtitles
    if (sfile->subt->text!=NULL) g_free(sfile->subt->text);
    sfile->subt->text=NULL;
    sfile->subt->current=NULL;
    return TRUE;
  }


  if(curr && (curr->start_time <= xtime))
    index_ptr = index_prev = index = curr;
  else
    index_ptr = index_prev = index = sfile->subt->index;

  while(index_ptr) {
    if(index_ptr->start_time > xtime) {
      if (sfile->subt->text!=NULL) g_free(sfile->subt->text);
      sfile->subt->text=NULL;
      sfile->subt->current = NULL;
      return (TRUE);
    }
    if(index_ptr->end_time >= xtime) {
      sfile->subt->current = index_ptr;
      if (sfile->subt->text!=NULL) g_free(sfile->subt->text);
      sfile->subt->text=srt_read_text(sfile->subt->tfile,sfile->subt->current);
      return (TRUE);
    }
    index_prev = index_ptr;
    index_ptr = (lives_subtitle_t *)index_ptr->next;
  }

  pf = sfile->subt->tfile;

  while(fgets(data,sizeof(data), pf)) {
    char *poslf = NULL, *poscr = NULL;
    int hstart, mstart, sstart, fstart;
    int hend, mend, send, fend;
    int i;
    double starttime, endtime;

    //
    // data contains subtitle number
    //

    if(!fgets(data,sizeof(data), pf)) {
      // EOF
      if (sfile->subt->text!=NULL) g_free(sfile->subt->text);
      sfile->subt->text=NULL;
      sfile->subt->current=NULL;
      sub_get_last_time(sfile->subt);
      return(FALSE);
    }
    //
    // data contains time range
    //
    // remove \n \r
    poslf = strstr(data, lf_str);
    if(poslf)
      *poslf = '\0';
    poscr = strstr(data, cr_str);
    if(poscr)
      *poscr = '\0';

    // try to parse it (time range)
    i = sscanf(data,"%d:%d:%d,%d --> %d:%d:%d,%d",\
               &hstart, &mstart, &sstart, &fstart,\
               &hend, &mend, &send, &fend);
    if(i == 8) {
      // parsing ok
        starttime = hstart*3600 + mstart*60 + sstart + fstart/1000.;
        endtime = hend*3600 + mend*60 + send + fend/1000.;
        node = (lives_subtitle_t *)g_malloc(sizeof(lives_subtitle_t));
        if(node) {
          node->start_time = starttime;
          node->end_time = endtime;
          node->style = NULL;
          node->next = NULL;
          node->prev = (_lives_subtitle_t *)index_prev;
	  node->textpos=ftell(pf);
          if(index_prev)
            index_prev->next = (_lives_subtitle_t *)node;
          else
            sfile->subt->index = node;
          index_prev = (lives_subtitle_t *)node;
        }
        while(fgets(data, sizeof(data), pf)) {
          // read the text and final empty line
          // remove \n \r
          poslf = strstr(data, lf_str);
          if(poslf)
          *poslf = '\0';
          poscr = strstr(data, cr_str);
          if(poscr)
           *poscr = '\0';

          if (!strlen(data)) break;
	} // end while

       if(node) {
         if(node->start_time > xtime) {
	   if (sfile->subt->text!=NULL) g_free(sfile->subt->text);
	   sfile->subt->text=NULL;
           sfile->subt->current = NULL;
	   return TRUE;
	 }
         if(node->end_time >= xtime) {
	   if (sfile->subt->text!=NULL) g_free(sfile->subt->text);
           sfile->subt->current = node;
	   sfile->subt->text=srt_read_text(sfile->subt->tfile,sfile->subt->current);
	   return TRUE;
	 }
       }
    }
    else {
      // What to do here ? Probably wrong format
      continue;
    }
  }

  // EOF
  sfile->subt->current=NULL;
  if (sfile->subt->text!=NULL) g_free(sfile->subt->text);
  sfile->subt->text=NULL;
  sub_get_last_time(sfile->subt);
  return (FALSE);
}



// read .sub files
gboolean get_sub_text(file *sfile, double xtime) {
  lives_subtitle_t *index = NULL;
  lives_subtitle_t *index_ptr = NULL;
  lives_subtitle_t *index_prev = NULL;
  lives_subtitle_t *node = NULL;
  lives_subtitle_t *curr = NULL;
  FILE *pf = NULL;
  char data[32768];
  gboolean starttext;

  if(!sfile)
    return (FALSE);

  if(!sfile->subt)
    return (FALSE);

  curr = sfile->subt->current;

  if(curr && (curr->start_time <= xtime) && (curr->end_time >= xtime))
    return (TRUE);

  if (sfile->subt->last_time!=-1. && xtime>sfile->subt->last_time) {
    // past end of subtitles
    if (sfile->subt->text!=NULL) g_free(sfile->subt->text);
    sfile->subt->text=NULL;
    sfile->subt->current=NULL;
    return TRUE;
  }


  if(curr && (curr->start_time <= xtime))
    index_ptr = index_prev = index = curr;
  else
    index_ptr = index_prev = index = sfile->subt->index;

  while(index_ptr) {
    if(index_ptr->start_time > xtime) {
      if (sfile->subt->text!=NULL) g_free(sfile->subt->text);
      sfile->subt->text=NULL;
      sfile->subt->current = NULL;
      return (TRUE);
    }
    if(index_ptr->end_time >= xtime) {
      sfile->subt->current = index_ptr;
      if (sfile->subt->text!=NULL) g_free(sfile->subt->text);
      sfile->subt->text=sub_read_text(sfile->subt->tfile,sfile->subt->current);
      return (TRUE);
    }
    index_prev = index_ptr;
    index_ptr = (lives_subtitle_t *)index_ptr->next;
  }

  pf = sfile->subt->tfile;

  starttext = (sfile->subt->index!=NULL);

  while(fgets(data,sizeof(data), pf)) {
    char *poslf = NULL, *poscr = NULL;
    int hstart, mstart, sstart, fstart;
    int hend, mend, send, fend;
    int i;
    double starttime, endtime;

    if(!strncmp(data, "[SUBTITLE]", 10)) {
      starttext = TRUE;
    }

    if(!starttext) {
      if(!strncmp(data,"[DELAY]",7)) {
	sfile->subt->offset=atoi(data+7);
      }
      continue;
    }

    //
    // data contains time range
    //
    // remove \n \r
    poslf = strstr(data, lf_str);
    if(poslf)
      *poslf = '\0';
    poscr = strstr(data, cr_str);
    if(poscr)
      *poscr = '\0';

    // try to parse it (time range)
    i = sscanf(data,"%d:%d:%d.%d,%d:%d:%d.%d",\
               &hstart, &mstart, &sstart, &fstart,\
               &hend, &mend, &send, &fend);
    if(i == 8) {
      // parsing ok
        starttime = hstart*3600 + mstart*60 + sstart + fstart/100.;
        endtime = hend*3600 + mend*60 + send + fend/100.;
        node = (lives_subtitle_t *)g_malloc(sizeof(lives_subtitle_t));
        if(node) {
          node->start_time = starttime;
          node->end_time = endtime;
          node->style = NULL;
          node->next = NULL;
          node->prev = (_lives_subtitle_t *)index_prev;
	  node->textpos=ftell(pf);
          if(index_prev)
            index_prev->next = (_lives_subtitle_t *)node;
          else
            sfile->subt->index = node;
          index_prev = (lives_subtitle_t *)node;
        }
        while(fgets(data, sizeof(data), pf)) {
          // read the text and final empty line
          // remove \n \r
          poslf = strstr(data, lf_str);
          if(poslf)
          *poslf = '\0';
          poscr = strstr(data, cr_str);
          if(poscr)
           *poscr = '\0';

          if (!strlen(data)) break;
	} // end while

       if(node) {
         if(node->start_time > xtime) {
	   if (sfile->subt->text!=NULL) g_free(sfile->subt->text);
	   sfile->subt->text=NULL;
           sfile->subt->current = NULL;
	   return TRUE;
	 }
         if(node->end_time >= xtime) {
	   if (sfile->subt->text!=NULL) g_free(sfile->subt->text);
           sfile->subt->current = node;
	   sfile->subt->text=sub_read_text(sfile->subt->tfile,sfile->subt->current);
	   return TRUE;
	 }
       }
    }
    else {
      continue;
    }
  }

  // EOF
  sfile->subt->current=NULL;
  if (sfile->subt->text!=NULL) g_free(sfile->subt->text);
  sfile->subt->text=NULL;
  sub_get_last_time(sfile->subt);
  return (FALSE);
}

///

void subtitles_free(file *sfile) {
  if (sfile==NULL) return;
  if (sfile->subt==NULL) return;

  if (sfile->subt->tfile!=NULL) fclose(sfile->subt->tfile);
 
  // remove subt->index entries
  while(sfile->subt->index) {
    lives_subtitle_t *to_delete = sfile->subt->index;

    sfile->subt->index = (lives_subtitle_t *)sfile->subt->index->next;

    if(to_delete->style != NULL) g_free(to_delete->style);
    g_free(to_delete);    
  } 

  if (sfile->subt->text!=NULL) g_free(sfile->subt->text);

  g_free (sfile->subt);
  sfile->subt=NULL;
}


gboolean subtitles_init(file *sfile, char * fname, lives_subtitle_type_t subtype) {
  // fname is the name of the subtitle file
  FILE *tfile;

  if (sfile==NULL) return FALSE;

  if (sfile->subt!=NULL) subtitles_free(sfile);

  sfile->subt=NULL;

  if ((tfile=fopen(fname,"r"))==NULL) return FALSE;

  sfile->subt=(lives_subtitles_t *)g_malloc(sizeof(lives_subtitles_t));

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
  if(ph)
    *ph = h;
  if(pmin)
    *pmin = m;
  if(psec)
    *psec = s;
  if(pmsec)
    *pmsec = ms;
}

gboolean save_srt_subtitles(file *sfile, double start_time, double end_time, double offset_time, const char *filename) {
  lives_subtitles_t *subt=NULL;
  long savepos = 0;
  FILE *pf;
  int num_saves;
  lives_subtitle_t *ptr = NULL;
 
  if(!sfile)
    return(FALSE);
  subt = sfile->subt;
  if(!subt)
    return(FALSE);
  if(subt->last_time <= -1.)
    get_srt_text(sfile, end_time);
  if(subt->last_time <= -1.)
    savepos = ftell(subt->tfile);

  // save the contents
  pf = fopen(filename, "w");
  if(!pf)
    return(FALSE);
  num_saves = 0;
  ptr = subt->index;
  while(ptr) {
    char *text = NULL;
    if(ptr->start_time < end_time && ptr->end_time >= start_time) {
      text = srt_read_text(subt->tfile, ptr);
      if(text) {
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
        g_free(text);
      }
    }
    else if (ptr->start_time>=end_time) break;
    ptr = (lives_subtitle_t *)ptr->next;
  }

  fclose(pf);
  if(!num_saves) // don't keep the empty file
    unlink(filename);

  if(subt->last_time <= -1.)
    fseek(subt->tfile, savepos, SEEK_SET);

  return(TRUE);
}

gboolean save_sub_subtitles(file *sfile, double start_time, double end_time, double offset_time, const char *filename) {
  lives_subtitles_t *subt=NULL;
  long savepos = 0;
  FILE *pf;
  int num_saves;
  lives_subtitle_t *ptr = NULL;
 
  if(!sfile)
    return(FALSE);
  subt = sfile->subt;
  if(!subt)
    return(FALSE);
  if(subt->last_time <= -1.)
    get_sub_text(sfile, end_time);
  if(subt->last_time <= -1.)
    savepos = ftell(subt->tfile);

  // save the contents
  pf = fopen(filename, "w");
  if(!pf)
    return(FALSE);
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


  while(ptr) {
    char *text = NULL;
    char *br_text = NULL;
    if(ptr->start_time < end_time && ptr->end_time >= start_time) {
      text = sub_read_text(subt->tfile, ptr);
      if(text) {
        int h, m, s, ms;
        double dtim;

	if (!strncmp(text+strlen(text)-1,"\n",1)) memset(text+strlen(text)-1,0,1);

        br_text = subst(text, "\n", "[br]");
        if(br_text) {
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
          g_free(br_text);
          num_saves++;
        }
        g_free(text);
      }
    }
    else if (ptr->start_time>=end_time) break;
    ptr = (lives_subtitle_t *)ptr->next;
  }

  fclose(pf);
  if(!num_saves) // don't keep the empty file
    unlink(filename);

  if(subt->last_time <= -1.)
    fseek(subt->tfile, savepos, SEEK_SET);

  return(TRUE);

}

