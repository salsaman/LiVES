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


gboolean get_srt_text(file *sfile, double xtime) {
  lives_subtitle_t *index = NULL;
  lives_subtitle_t *index_ptr = NULL;
  lives_subtitle_t *index_prev = NULL;
  lives_subtitle_t *node = NULL;
  lives_subtitle_t *curr = NULL;
  char *ret = NULL;
  FILE *pf = NULL;
  char data[32768];

  if(!sfile)
    return (FALSE);

  if(!sfile->subt)
    return (FALSE);

  curr = sfile->subt->current;

  if(curr && (curr->start_time <= xtime) && (curr->end_time >= xtime))
    return (TRUE);

  if(curr && (curr->start_time <= xtime))
    index_ptr = index_prev = index = curr;
  else
    index_ptr = index_prev = index = sfile->subt->index;

  while(index_ptr) {
    if(index_ptr->start_time > xtime) {
      sfile->subt->current = NULL;
      return (TRUE);
    }
    if(index_ptr->end_time >= xtime) {
      sfile->subt->current = index_ptr;
      return (TRUE);
    }
    index_prev = index_ptr;
    index_ptr = (lives_subtitle_t *)index_ptr->next;
  }

  pf = sfile->subt->tfile;

  while(fgets(data,sizeof(data), pf)) {
    char *posnl = NULL, *poscr = NULL;
    int hstart, mstart, sstart, fstart;
    int hend, mend, send, fend;
    int i;
    double starttime, endtime;

    //
    // data contains subtitle number
    //

    if(!fgets(data,sizeof(data), pf)) {
      sfile->subt->current=NULL;
      return(FALSE);
    }
    //
    // data contains time range
    //
    // remove \n \r
    posnl = strstr(data, "\n");
    if(posnl)
      *posnl = '\0';
    poscr = strstr(data, "\r");
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
        node = (lives_subtitle_t *)malloc(sizeof(lives_subtitle_t));
        if(node) {
          node->start_time = starttime;
          node->end_time = endtime;
          node->text = NULL;
          node->style = NULL;
          node->next = NULL;
          node->prev = (_lives_subtitle_t *)index_prev;
          if(index_prev)
            index_prev->next = (_lives_subtitle_t *)node;
          else
            sfile->subt->index = node;
          index_prev = (lives_subtitle_t *)node;
        }
        while(fgets(data, sizeof(data), pf)) {
          // read the text and final empty line
          // remove \n \r
          posnl = strstr(data, "\n");
          if(posnl)
          *posnl = '\0';
          poscr = strstr(data, "\r");
          if(poscr)
           *poscr = '\0';

          if(strlen(data) > 0) {
            if(posnl) *posnl = '\n';
            if(poscr) *poscr = '\r';
            if(ret == NULL) {
              ret = (char *)malloc(strlen(data) + 1);
              if(ret) strcpy(ret, data);
            }
            else {
              ret = (char *)realloc(ret, strlen(ret) + strlen(data) + 1);
              if(ret) strcat(ret, data);
            }
          }
          else { // strlen(data) == 0
            break;
          }
       } // end while

	if(node) {
	  node->text = ret;
	  if(node->start_time > xtime) {
	    sfile->subt->current = NULL;
	  }
	  if(node->end_time >= xtime) {
	    sfile->subt->current = node;
	  }            
	  return TRUE;
	}
	else
	  if(ret) free(ret);
	ret = NULL;

    }
    else {
      // What to do here ? Probably wrong format
      sfile->subt->current=NULL;
      return (FALSE);
    }
  }

  // EOF
  sfile->subt->current=NULL;
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

    if(to_delete->text != NULL) free(to_delete->text);
    if(to_delete->style != NULL) free(to_delete->style);
    free(to_delete);    
  } 

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

  sfile->subt=(lives_subtitles_t *)g_malloc(sizeof(lives_subtitles_t));

  sfile->subt->tfile=tfile;

  sfile->subt->current=sfile->subt->index = NULL;

  return TRUE;
}

