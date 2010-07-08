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

#include "../libweed/weed.h"
#include "../libweed/weed-palettes.h"
#include "../libweed/weed-effects.h"
#include "../libweed/weed-host.h"

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
        font_list = (char **)malloc((num+1)*sizeof(char*));
        if(font_list) {
          int i;
          for(i = 0; i < num; ++i)
            font_list[i] = strdup(pango_font_family_get_name(pff[i]));
          font_list[num] = NULL;
          qsort(font_list, num, sizeof(char *), font_cmp);
        }
      }
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
  double f_alpha, b_alpha;

  int width, height, palette;

  double dwidth, dheight;
  lives_colRGBA32_t *fg, *bg;

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

  f_alpha = fg->alpha/255.0;
  b_alpha = bg->alpha/255.0;


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

          if(!layer) {
            x_pos = y_pos = 0;
            dwidth = width;
            dheight = height;
            b_alpha = 1.0;
          }

          x_text = x_pos;
          y_text = y_pos;
          switch(cent) {
          case 1:
                 pango_layout_set_alignment(layout, PANGO_ALIGN_CENTER);
                 break;
          case 0:
          default:
                 pango_layout_set_alignment(layout, PANGO_ALIGN_LEFT);
          }

          cairo_move_to(cairo, x_text, y_text);

          switch(mode) {
          case 1:
                 cairo_set_source_rgba(cairo,bg->red/255.0, bg->green/255.0, bg->blue/255.0, b_alpha);
                 fill_bckg(cairo, x_pos, y_pos, dwidth, dheight);
                 cairo_move_to(cairo, x_text, y_text);
                 cairo_set_source_rgba(cairo,fg->red/255.0, fg->green/255.0, fg->blue/255.0, f_alpha);
                 pango_layout_set_text(layout, text, -1);
                 break;
          case 2:
                 cairo_set_source_rgba(cairo,bg->red/255.0, bg->green/255.0, bg->blue/255.0, b_alpha);
                 fill_bckg(cairo, x_pos, y_pos, dwidth, dheight);
                 cairo_move_to(cairo, x_pos, y_pos);
                 cairo_set_source_rgba(cairo,fg->red/255.0, fg->green/255.0, fg->blue/255.0, f_alpha);
                 pango_layout_set_text(layout, "", -1);
                 break;
          case 0:
          default:
                 cairo_set_source_rgba(cairo,fg->red/255.0, fg->green/255.0, fg->blue/255.0, f_alpha);
                 break;
          }

          pango_cairo_show_layout(cairo, layout);

          pixbuf_new = gdk_pixbuf_get_from_drawable(pixbuf, pixmap, NULL,\
              0, 0,\
              0, 0,\
              -1, -1);
          result = pixbuf_to_layer(layer, pixbuf_new);
          if(result)
            g_object_unref(pixbuf_new);
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

//
// return string must be freed with free() after usage
//
char *get_srt_text(FILE *pf, double xtime) {
  char *ret = NULL;
  if(!pf)
    return(NULL);

  int section = 0;

  while(!feof(pf)) {
    char data[256], *posnl, *poscr;
    int hstart, mstart, sstart, fstart;
    int hend, mend, send, fend;
    int i;
    double starttime, endtime;

    if (!fgets(data, sizeof(data), pf)) return NULL;
    // remove \n \r
    posnl = strstr(data, "\n");
    if(posnl)
      *posnl = '\0';
    poscr = strstr(data, "\r");
    if(poscr)
      *poscr = '\0';

    if(!section) {
      i = sscanf(data,"%d:%d:%d,%d --> %d:%d:%d,%d",\
                 &hstart, &mstart, &sstart, &fstart,\
                 &hend, &mend, &send, &fend);
      if(i == 8) {
        // this is a time tag
        starttime = hstart*3600 + mstart*60 + sstart + fstart/1000.;
        endtime = hend*3600 + mend*60 + send + fend/1000.;
        if(xtime >= starttime && xtime <= endtime) {
          section = 1;
        }
      }
    }
    else {
      int len = strlen(data);
      if(!len)
        break;
      //restore skipped symbols
      if(poscr)
        *poscr = '\r';
      if(posnl)
        *posnl = '\n';
      len = strlen(data);
      //add the line to the text
      if(!ret) {
        ret = (char *)malloc(len+1);
        if(ret)
         strcpy(ret,data);
      }
      else {
        ret = (char *)realloc(ret, strlen(ret)+len+1);
        if(ret)
          strcpy(&ret[strlen(ret)], data);
      }
    }
  }
  return ret;
}










///

void subtitles_free(file *sfile) {
  if (sfile==NULL) return;

  // in future we will free the list sfile->subt->index

  if (sfile->subt!=NULL && sfile->subt->current!=NULL) {
    if (sfile->subt->current->text!=NULL) free(sfile->subt->current->text);
    free (sfile->subt->current);
  }

  if (sfile->subt->tfile!=NULL) fclose(sfile->subt->tfile);

  free (sfile->subt);
  sfile->subt=NULL;
}


gboolean subtitles_init(file *sfile, char * fname) {
  // fname is the name of the subtitle file
  FILE *tfile;

  if (sfile==NULL) return FALSE;

  if (sfile->subt!=NULL) subtitles_free(sfile);

  sfile->subt=NULL;

  if ((tfile=fopen(fname,"r"))==NULL) return FALSE;

  sfile->subt->tfile=tfile;


  // in future we will add stuff to our index and just point current to the current entry
  sfile->subt->current=g_malloc(sizeof(lives_subtitle_t));

  sfile->subt->current->text=NULL;
  sfile->subt->current->start_time=sfile->subt->current->end_time=-1.;
  sfile->subt->current->style=NULL;
  sfile->subt->current->prev=sfile->subt->current->next=NULL;

  return TRUE;
}

