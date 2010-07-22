// puretext.c
// weed plugin
// (c) A. Penkov & salsaman 2010
// cloned and modified from livetext.c (author G. Finch aka salsaman)
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details


#include "../../libweed/weed.h"
#include "../../libweed/weed-palettes.h"
#include "../../libweed/weed-effects.h"
#include "../../libweed/weed-plugin.h"

///////////////////////////////////////////////////////////////////

static int num_versions=2; // number of different weed api versions supported
static int api_versions[]={131,100}; // array of weed api versions supported in plugin, in order of preference (most preferred first)

static int package_version=2; // version of this package

//////////////////////////////////////////////////////////////////

#include "../../libweed/weed-utils.h" // optional
#include "../../libweed/weed-plugin-utils.h" // optional

/////////////////////////////////////////////////////////////

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <fcntl.h>
#include <unistd.h>
#include <wchar.h>

#include <gdk/gdk.h>
#include <pango/pangocairo.h>

// defines for configure dialog elements
enum DlgControls {
  P_TEXT=0,
  P_MODE,
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
  PT_ALL_MODE // proctext called once with NULL xtext pointing to sdata->text (or NULL if length is 0)
} pt_tmode_t;


typedef enum {
  PT_SPIRAL_TEXT=0,
  PT_SPINNING_LETTERS
} pt_op_mode_t;


// for future use
#define NTHREADS 1

// static data per instance
typedef struct {
  int count; // proctext counter

  double timer; // time in seconds since first process call
  weed_timecode_t last_tc; // timecode of last process call
  double alarm_time; // pre-set alarm timecode [set with pt_set_alarm( this, delta) ]

  gboolean alarm; // event wake up

  off_t start; // start glyph (inclusive) in text (0 based) for string/word/letter modes
  int64_t length; // length of string in text [0 to all] for string/word/letter modes

  pt_ttext_t text_type;
  char *text; // text loaded in from file
  size_t tlength; // length of text in characters
  

  // offsets of text in layer (0,0) == top left
  int x_text;
  int y_text;

  pt_tmode_t tmode;

  rgb_t fg;

  double fg_alpha;

  int mode;

  // generic variables
  double dbl1;
  double dbl2;
  double dbl3;

  int int1;

  gboolean bool1;

  // reserved for future
  //pthread_t xthread[NTHREADS];
  //pthread_mutex_t xmutex;

  // per glyph/mode private data
  void **priv;
} sdata_t;




/////////////////////////////////////////////


static GdkPixbuf *pl_channel_to_pixbuf (weed_plant_t *channel);
static gboolean pl_pixbuf_to_channel(weed_plant_t *channel, GdkPixbuf *pixbuf);
static GdkPixbuf *pl_gdk_pixbuf_cheat(GdkColorspace colorspace, gboolean has_alpha, int bits_per_sample, int width, int height, guchar *buf);

inline G_GNUC_CONST int pl_gdk_rowstride_value (int rowstride) {
  // from gdk-pixbuf.c
  /* Always align rows to 32-bit boundaries */
  return (rowstride + 3) & ~3;
}

inline int G_GNUC_CONST pl_gdk_last_rowstride_value (int width, int nchans) {
  // from gdk pixbuf docs
  return width*(((nchans<<3)+7)>>3);
}

static void plugin_free_buffer (guchar *pixels, gpointer data) {
  return;
}

static char **fonts_available = NULL;
static int num_fonts_available = 0;


static size_t utf8len(char *text) {
  size_t toffs=0;
  while (text[toffs]!='\0') {
    toffs+=mbtowc(NULL,&text[toffs],4);
  }
  return toffs;
}


static size_t utf8offs(char *text, int xoffs) {
  size_t toffs=0;
  while (text[toffs]!='\0'&&xoffs>0) {
    toffs+=mbtowc(NULL,&text[toffs],4);
    xoffs--;
  }
  return toffs;
}


static void getlsize(PangoLayout *layout, double *pw, double *ph) {
  // calculate width and height of layout
  int w_, h_;
  pango_layout_get_size(layout, &w_, &h_);
  if(pw)
    *pw = ((double)w_)/PANGO_SCALE;
  if(ph)
    *ph = ((double)h_)/PANGO_SCALE;
}


static inline void pt_set_alarm(sdata_t *sdata, int delta) {
  sdata->alarm=FALSE;
  sdata->alarm_time=sdata->timer+(float)delta/1000.;
}


static void setxypos(double dwidth, double dheight, double x, double y, int *x_text, int *y_text) {
  // set top left corner offset given center point and glyph dimensions
  *x_text=x-dwidth/2.+.5;
  *y_text=y-dheight/2.+.5;
}


static void rotate_text(cairo_t *cairo, PangoLayout *layout, int x_center, int y_center, double radians) {
  cairo_translate (cairo, x_center, y_center);
  cairo_rotate (cairo, radians);
  
  /* Inform Pango to re-layout the text with the new transformation */
  pango_cairo_update_layout (cairo, layout);
}


static void proctext(sdata_t *sdata, weed_timecode_t tc, char *xtext, cairo_t *cairo, PangoLayout *layout, 
		     PangoFontDescription *font, int width, int height) {
  double dwidth,dheight;
  double font_size;
  double radX,radY;

  switch (sdata->mode) {
  case (PT_SPINNING_LETTERS):
    if (sdata->timer==0.) {
      sdata->int1=0;

      // select all text
      sdata->start=0;
      sdata->length=sdata->tlength;

      sdata->tmode=PT_LETTER_MODE;
      sdata->dbl3=0.;
    }

    if (!sdata->count) {
      sdata->dbl1=0.;
      sdata->dbl2=sdata->dbl3;
      sdata->dbl3+=.1;
    }

    font_size=64;

    // set font size
    pango_font_description_set_absolute_size(font, font_size*PANGO_SCALE);
    pango_layout_set_font_description(layout, font);

    sdata->x_text=width-sdata->int1+sdata->dbl1;
    sdata->y_text=height/2;

    // rotate letter
    rotate_text(cairo,layout,sdata->x_text,sdata->y_text,sdata->dbl2);

    // must re-center after rotating
    sdata->x_text=sdata->y_text=0;

    // get pixel size of letter/word
    getlsize(layout, &dwidth, &dheight);

    sdata->dbl1+=dwidth+10.;

    setxypos(0,dheight,sdata->x_text,sdata->y_text,&sdata->x_text,&sdata->y_text);

    // colours
    sdata->fg.red=sdata->fg.green=sdata->fg.blue=65535;
    sdata->fg_alpha=1.;

    if (sdata->alarm) {
      pt_set_alarm(sdata,25); // milliseconds
      // shift rate
      sdata->int1+=8;
    }

    // spin rate
    sdata->dbl2+=.1;

    break;

  case (PT_SPIRAL_TEXT):

    if (sdata->timer==0.) {
      sdata->int1=0;
      sdata->start=0;
      sdata->tmode=PT_LETTER_MODE;
    }

    font_size=2560/(sdata->count+19.);

    // set font size
    pango_font_description_set_absolute_size(font, font_size*PANGO_SCALE);
    // if we set size we must also reset font_description
    pango_layout_set_font_description(layout, font);
      
    // get pixel size of letter/word
    getlsize(layout, &dwidth, &dheight);
      
    // expansion factor
    if (sdata->int1<2) {
      sdata->dbl3=1.;
      sdata->bool1=FALSE;
    }
    else sdata->dbl3+=.0001;
      
    // set x_text, y_text
    if (!sdata->count) {
      sdata->dbl1=radX=width*.45*sdata->dbl3;
      sdata->dbl2=radY=height*.45*sdata->dbl3;
    }
    else {
      if (!sdata->bool1) {
	sdata->dbl1=radX=sdata->dbl1*.98;
	sdata->dbl2=radY=sdata->dbl2*.98;
      }
      else {
	sdata->dbl1=radX=sdata->dbl1*.97;
	sdata->dbl2=radY=sdata->dbl2*.97;
      }
    }
      
    if (sdata->bool1) 
      setxypos(dwidth,dheight,width/2+sin(sdata->count/4.+(sdata->dbl3-1.)*9.)*radX,height/2-cos(-sdata->count/4.-(sdata->dbl3-1.)*8.)*radY,&sdata->x_text,&sdata->y_text);
      
    else
      setxypos(dwidth,dheight,width/2+sin(sdata->count/4.+(sdata->dbl3-1.)*8.)*radX,height/2-cos(-sdata->count/4.-(sdata->dbl3-1.)*8.)*radY,&sdata->x_text,&sdata->y_text);
      
    if (!strncmp(xtext,".",1)) sdata->int1++;
      
    // colours
    sdata->fg.red=sdata->fg.green=sdata->fg.blue=65535;
    sdata->fg_alpha=1.;
      
    if (sdata->alarm) {
      pt_set_alarm(sdata,250); // milliseconds
      if (sdata->start+sdata->length<sdata->tlength) {
	// add an extra letter
	sdata->length++;
      }
      else {
	sdata->length-=2;
	sdata->start+=2;
	sdata->dbl3-=.0002;

	sdata->dbl1=sdata->dbl1*.97;
	sdata->dbl2=radY=sdata->dbl2*.97;
	  
	// trip to lissajou
	if (!sdata->bool1) sdata->bool1=TRUE;
	  
	if (sdata->length<=0) {
	  sdata->length=1;
	  sdata->start=0;
	  sdata->bool1=FALSE;
	  sdata->dbl3=-sdata->dbl3/12.;
	}
	else pt_set_alarm(sdata,50); // milliseconds
	  
      }
	
    }
    break;
      
  } // end switch

}


int puretext_init(weed_plant_t *inst) {
  int error,fd;
  weed_plant_t **in_params=weed_get_plantptr_array(inst,"in_parameters",&error);
  sdata_t *sdata;

  char buff[65536];
  size_t b_read;

  char *textfile=weed_get_string_value(in_params[P_TEXT],"value",&error);

  // open file and read in text

  if ((fd=open(textfile,O_RDONLY))==-1) return WEED_ERROR_INIT_ERROR;

  weed_free(textfile);     
  weed_free(in_params);

  sdata=(sdata_t *)weed_malloc(sizeof(sdata_t));

  if (sdata==NULL) return WEED_ERROR_MEMORY_ALLOCATION;

  sdata->timer=-1;
  sdata->last_tc=0;
  sdata->alarm_time=0.;
  sdata->alarm=FALSE;

  sdata->text_type=TEXT_TYPE_UTF8;

  b_read=read(fd,buff,65535);

  memset(buff+b_read,0,1);

  sdata->text=strdup(buff);

  sdata->start=sdata->length=0;

  if (sdata->text_type==TEXT_TYPE_ASCII) {
    sdata->tlength=strlen((char *)sdata->text);
  }
  else {
    sdata->tlength=utf8len(sdata->text);
  }

  sdata->int1=0;
  sdata->dbl1=sdata->dbl2=sdata->dbl3=-1.;
  sdata->bool1=FALSE;


  weed_set_voidptr_value(inst,"plugin_internal",sdata);

  return WEED_NO_ERROR;
}



int puretext_deinit(weed_plant_t *inst) {
  int error;
  sdata_t *sdata=(sdata_t *)weed_get_voidptr_value(inst,"plugin_internal",&error);

  if (sdata!=NULL) {
    if (sdata->text!=NULL) free(sdata->text);
    free(sdata);
  }

  return WEED_NO_ERROR;

}



int puretext_process (weed_plant_t *inst, weed_timecode_t tc) {
  int error;

  int width, height, palette;

  weed_plant_t *in_channel=NULL;

  GdkPixbuf *pixbuf = NULL;
  GdkPixbuf *pixbuf_new = NULL;
  int alpha_threshold = 0;

  int mode;

  int i;
  size_t toffs;

  weed_plant_t *out_channel=weed_get_plantptr_value(inst,"out_channels",&error);
  weed_plant_t **in_params=weed_get_plantptr_array(inst,"in_parameters",&error);

  sdata_t *sdata=(sdata_t *)weed_get_voidptr_value(inst,"plugin_internal",&error);

  width=weed_get_int_value(out_channel,"width",&error);
  height=weed_get_int_value(out_channel,"height",&error);

  palette=weed_get_int_value(out_channel,"current_palette",&error);

  in_channel=weed_get_plantptr_value(inst,"in_channels",&error);

  mode=weed_get_int_value(in_params[P_MODE],"value",&error);

  if (mode!=sdata->mode) {
    sdata->timer=-1.;
    sdata->mode=mode;
    sdata->alarm_time=0.;
  }

  weed_free(in_params); // must weed free because we got an array


  // set timer data and alarm status
  if (sdata->timer==-1.||tc<sdata->last_tc) {
    sdata->timer=0.;
    sdata->length=0;
  }
  else {
    sdata->timer+=(double)(tc-sdata->last_tc)/100000000.;
    sdata->alarm=FALSE;
  }

  if (sdata->alarm_time>-1.&&sdata->timer>=sdata->alarm_time) {
    sdata->alarm_time=-1.;
    sdata->alarm=TRUE;
  }

  sdata->last_tc=tc;

  sdata->count=0;

  // THINGS TO TO WITH TEXTS AND PANGO
  if((!in_channel) || (in_channel == out_channel))
    pixbuf = pl_channel_to_pixbuf(out_channel);
  else
    pixbuf = pl_channel_to_pixbuf(in_channel);

  if(pixbuf) {
    // do cairo and pango things
    GdkPixmap *pixmap = NULL;
    gdk_pixbuf_render_pixmap_and_mask(pixbuf, &pixmap, NULL, alpha_threshold);
    if(pixmap) {
      cairo_t *cairo = gdk_cairo_create(pixmap);
      gboolean result;
      if(cairo) {

	// TODO - get real offset of start in bytes

	if (sdata->text_type==TEXT_TYPE_ASCII) {
	  toffs=sdata->start;
	}
	else {
	  toffs=utf8offs(sdata->text,sdata->start);
	}

	// loop from start char to end char
	for (i=sdata->start;i<sdata->start+(sdata->length==0?1:sdata->length);i++) {
	  PangoLayout *layout = pango_cairo_create_layout(cairo);
	  if(layout) { 
	    PangoFontDescription *font;
	    char *xtext;

	    font = pango_font_description_new();

	    // send letter or word to proctext

	    //if((num_fonts_available) && (fontnum >= 0) && (fontnum < num_fonts_available) && (fonts_available[fontnum]))
	    pango_font_description_set_family(font, "Serif");

	    if (sdata->length==0) {
	      xtext=weed_malloc(1);
	      weed_memset(xtext,0,1);
	    }
	    else {
	      // letter mode
	      if (sdata->text_type==TEXT_TYPE_ASCII) {
		xtext=strndup(&sdata->text[toffs],1);
		toffs++;
	      }
	      else {
		int xlen=mbtowc(NULL,&sdata->text[toffs],4);
		xtext=strndup(&sdata->text[toffs],xlen);
		toffs+=xlen;
	      }
	    }

	    pango_layout_set_font_description(layout, font);
	    pango_layout_set_text(layout, (char *)xtext, -1);

	    cairo_save(cairo);

	    // get size, position, and colour
	    proctext(sdata,tc,(char *)xtext,cairo,layout,font,width,height);

	    free(xtext);

	    if (palette==WEED_PALETTE_BGR24) {
	      int tmp=sdata->fg.red;
	      sdata->fg.red=sdata->fg.blue;
	      sdata->fg.blue=tmp;
	    }

	    cairo_move_to(cairo, sdata->x_text, sdata->y_text);

	    cairo_set_source_rgba(cairo,sdata->fg.red/255.0, sdata->fg.green/255.0, sdata->fg.blue/255.0, sdata->fg_alpha);

	    pango_cairo_show_layout(cairo, layout);


	    cairo_restore(cairo);

	    pango_font_description_free(font);
	    g_object_unref(layout);
	  }

	  sdata->count++;

	} // end loop

	// and finally convert backwards
	pixbuf_new = gdk_pixbuf_get_from_drawable(pixbuf, pixmap, NULL,	\
						  0, 0,			\
						  0, 0,			\
						  -1, -1);
	result = pl_pixbuf_to_channel(out_channel, pixbuf_new);
	g_object_unref(pixbuf_new);
      }
      cairo_destroy(cairo);
    }
    g_object_unref(pixmap);
  }

  return WEED_NO_ERROR;
}






inline static int font_compare(const void *p1, const void *p2) {  
  const char *s1 = (const char *)(*(char **)p1);
  const char *s2 = (const char *)(*(char **)p2);
  return(strcasecmp(s1, s2));
}

weed_plant_t *weed_setup (weed_bootstrap_f weed_boot) {
  weed_plant_t *plugin_info=weed_plugin_info_init(weed_boot,num_versions,api_versions);

  if (plugin_info!=NULL) {
    int palette_list[]={WEED_PALETTE_BGR24,WEED_PALETTE_RGB24,WEED_PALETTE_END};
    weed_plant_t *in_chantmpls[]={weed_channel_template_init("in channel 0",0,palette_list),NULL};
    weed_plant_t *out_chantmpls[]={weed_channel_template_init("out channel 0",WEED_CHANNEL_CAN_DO_INPLACE,palette_list),NULL};
    weed_plant_t *in_params[P_END+1],*gui;
    weed_plant_t *filter_class;
    PangoContext *ctx;

    char *modes[]={"Spiral text","Spinning letters",NULL};
    char *rfx_strings[]={"special|fileread|0|"};

    // this section contains code
    // for configure fonts available
    num_fonts_available = 0;
    fonts_available = NULL;

    ctx = gdk_pango_context_get();
    if(ctx) {
      PangoFontMap *pfm = pango_context_get_font_map(ctx);
      if(pfm) {
        int num = 0;
        PangoFontFamily **pff = NULL;
        pango_font_map_list_families(pfm, &pff, &num);
        if(num > 0) {
          // we should reserve num+1 for a final NULL pointer
          fonts_available = (char **)malloc((num+1)*sizeof(char *));
          if(fonts_available) {
            register int i;
            num_fonts_available = num;
            for(i = 0; i < num; ++i) {
              fonts_available[i] = strdup(pango_font_family_get_name(pff[i]));
            }
            // don't forget this thing
            fonts_available[num] = NULL;
          }
        }
        g_free(pff);
      } 
      g_object_unref(ctx);
    }

    in_params[P_TEXT]=weed_text_init("textfile","_Text file","");
    in_params[P_MODE]=weed_string_list_init("mode","Effect _mode",0,modes);
    in_params[P_END]=NULL;

    filter_class=weed_filter_class_init("puretext","Salsaman & Aleksej Penkov",1,0,&puretext_init,&puretext_process,NULL,in_chantmpls,out_chantmpls,in_params,NULL);

    gui=weed_filter_class_get_gui(filter_class);
    weed_set_string_value(gui,"layout_scheme","RFX");
    weed_set_string_value(gui,"rfx_delim","|");
    weed_set_string_array(gui,"rfx_strings",1,rfx_strings);

    weed_plugin_info_add_filter_class (plugin_info,filter_class);

    weed_set_int_value(plugin_info,"version",package_version);

  }

  return plugin_info;
}



void weed_desetup(void) {
  // clean up what we reserve for font family names
  if(num_fonts_available && fonts_available) {
    int i;
    for(i = 0; i < num_fonts_available; ++i) {
      free((void *)fonts_available[i]);
    }
    free((void *)fonts_available);
  }    
  num_fonts_available = 0;
  fonts_available = NULL;
}



static GdkPixbuf *pl_channel_to_pixbuf (weed_plant_t *channel) {
  int error;
  GdkPixbuf *pixbuf;
  int palette=weed_get_int_value(channel,"current_palette",&error);
  int width=weed_get_int_value(channel,"width",&error);
  int height=weed_get_int_value(channel,"height",&error);
  int irowstride=weed_get_int_value(channel,"rowstrides",&error);
  int rowstride,orowstride;
  guchar *pixel_data=(guchar *)weed_get_voidptr_value(channel,"pixel_data",&error),*pixels,*end;
  gboolean cheat=FALSE;
  gint n_channels;

  switch (palette) {
  case WEED_PALETTE_RGB24:
  case WEED_PALETTE_BGR24:
  case WEED_PALETTE_YUV888:
    if (irowstride==pl_gdk_rowstride_value(width*3)) {
      pixbuf=pl_gdk_pixbuf_cheat(GDK_COLORSPACE_RGB, FALSE, 8, width, height, pixel_data);
      cheat=TRUE;
    }
    else pixbuf=gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, width, height);
    n_channels=3;
    break;
  case WEED_PALETTE_RGBA32:
  case WEED_PALETTE_BGRA32:
  case WEED_PALETTE_ARGB32:
  case WEED_PALETTE_YUVA8888:
    if (irowstride==pl_gdk_rowstride_value(width*4)) {
      pixbuf=pl_gdk_pixbuf_cheat(GDK_COLORSPACE_RGB, TRUE, 8, width, height, pixel_data);
      cheat=TRUE;
    }
    else pixbuf=gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, width, height);
    n_channels=4;
    break;
  default:
    return NULL;
  }
  pixels=gdk_pixbuf_get_pixels (pixbuf);
  orowstride=gdk_pixbuf_get_rowstride(pixbuf);
  
  if (irowstride>orowstride) rowstride=orowstride;
  else rowstride=irowstride;
  end=pixels+orowstride*height;

  if (!cheat) {
    gboolean done=FALSE;
    for (;pixels<end&&!done;pixels+=orowstride) {
      if (pixels+orowstride>=end) {
	orowstride=rowstride=pl_gdk_last_rowstride_value(width,n_channels);
	done=TRUE;
      }
      weed_memcpy(pixels,pixel_data,rowstride);
      if (rowstride<orowstride) weed_memset (pixels+rowstride,0,orowstride-rowstride);
      pixel_data+=irowstride;
    }
  }
  return pixbuf;
}



static gboolean pl_pixbuf_to_channel(weed_plant_t *channel, GdkPixbuf *pixbuf) {
  // return TRUE if we can use the original pixbuf pixels
  
  int error;
  int rowstride=gdk_pixbuf_get_rowstride(pixbuf);
  int width=gdk_pixbuf_get_width(pixbuf);
  int height=gdk_pixbuf_get_height(pixbuf);
  int n_channels=gdk_pixbuf_get_n_channels(pixbuf);
  guchar *in_pixel_data=(guchar *)gdk_pixbuf_get_pixels(pixbuf);
  int out_rowstride=weed_get_int_value(channel,"rowstrides",&error);
  guchar *dst=weed_get_voidptr_value(channel,"pixel_data",&error);

  register int i;

  if(dst == in_pixel_data)
    return TRUE;

  if (rowstride==pl_gdk_last_rowstride_value(width,n_channels)&&rowstride==out_rowstride) {
    weed_memcpy(dst,in_pixel_data,rowstride*height);
    return FALSE;
  }

  for (i=0;i<height;i++) {
    if (i==height-1) rowstride=pl_gdk_last_rowstride_value(width,n_channels);
    weed_memcpy(dst,in_pixel_data,rowstride);
    in_pixel_data+=rowstride;
    dst+=out_rowstride;
  }

  return FALSE;
}

static GdkPixbuf *pl_gdk_pixbuf_cheat(GdkColorspace colorspace, gboolean has_alpha, int bits_per_sample, int width, int height, guchar *buf) {
  // we can cheat if our buffer is correctly sized
  int channels=has_alpha?4:3;
  int rowstride=pl_gdk_rowstride_value(width*channels);
  return gdk_pixbuf_new_from_data (buf, colorspace, has_alpha, bits_per_sample, width, height, rowstride, plugin_free_buffer, NULL);
}

