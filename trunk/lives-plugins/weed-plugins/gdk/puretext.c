// puretext.c
// weed plugin
// (c) salsaman 2010 (contributions by A. Penkov)
// cloned and modified from livetext.c (author G. Finch aka salsaman)
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

#ifdef HAVE_SYSTEM_WEED
#include <weed/weed.h>
#include <weed/weed-palettes.h>
#include <weed/weed-effects.h>
#else
#include "../../../libweed/weed.h"
#include "../../../libweed/weed-palettes.h"
#include "../../../libweed/weed-effects.h"
#endif

///////////////////////////////////////////////////////////////////

static int num_versions=2; // number of different weed api versions supported
static int api_versions[]= {131,100}; // array of weed api versions supported in plugin, in order of preference (most preferred first)

static int package_version=2; // version of this package

//////////////////////////////////////////////////////////////////

#ifdef HAVE_SYSTEM_WEED_PLUGIN_H
#include <weed/weed-plugin.h> // optional
#else
#include "../../../libweed/weed-plugin.h" // optional
#endif

#include "../weed-utils-code.c" // optional
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

static gboolean is_big_endian() {
  int32_t testint = 0x12345678;
  char *pMem;
  pMem = (char *) &testint;
  if (pMem[0] == 0x78) return FALSE;
  return TRUE;
}


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
  PT_LINE_MODE, // proctext() is called with per line xtext from start -> start+length-1
  PT_ALL_MODE // proctext called once with NULL xtext pointing to sdata->text (or NULL if length is 0)
} pt_tmode_t;


typedef enum {
  PT_SPIRAL_TEXT=0,
  PT_SPINNING_LETTERS,
  PT_LETTER_STARFIELD,
  PT_WORD_COALESCE
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

  off_t start; // start glyph (inclusive) in text (0 based) for string/word/letter modes
  int64_t length; // length of string in text [0 to all] for string/word/letter modes

  pt_ttext_t text_type;
  char *text; // text loaded in from file
  size_t tlength; // length of text in characters
  int wlength; // length in words

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
  pt_letter_data_t *letter_data;
} sdata_t;




typedef struct {
  size_t start;
  size_t length;
} pt_subst_t;


/////////////////////////////////////////////
static int unal[256][256];
static int al[256][256];

static void init_unal(void) {
  // premult to postmult and vice-versa

  register int i,j;

  for (i=0; i<256; i++) { //alpha val
    for (j=0; j<256; j++) { // val to be converted
      unal[i][j]=(float)j*255./(float)i;
      al[i][j]=(float)j*(float)i/255.;
    }
  }
}

static void alpha_unpremult(guchar *ptr, int width, int height, int rowstride, int pal, int un) {
  int aoffs,coffs,psizel;
  int alpha;
  int psize=4;

  register int i,j,p;

  switch (pal) {
  case WEED_PALETTE_BGRA32:
    psizel=3;
    coffs=0;
    aoffs=3;
    break;
  case WEED_PALETTE_ARGB32:
    psizel=4;
    coffs=1;
    aoffs=0;
    break;
  default:
    return;
  }

  if (un) {
    for (i=0; i<height; i++) {
      for (j=0; j<width; j+=psize) {
        alpha=ptr[j+aoffs];
        for (p=coffs; p<psizel; p++) {
          ptr[j+p]=unal[alpha][ptr[j+p]];
        }
      }
      ptr+=rowstride;
    }
  } else {
    for (i=0; i<height; i++) {
      for (j=0; j<width; j+=psize) {
        alpha=ptr[j+aoffs];
        for (p=coffs; p<psizel; p++) {
          ptr[j+p]=al[alpha][ptr[j+p]];
        }
      }
      ptr+=rowstride;
    }
  }

}



static cairo_t *channel_to_cairo(weed_plant_t *channel) {
  // convert a weed channel to cairo
  // the channel shares pixel_data with cairo
  // so it should be copied before the cairo is destroyed

  // "width","rowstrides" and "current_palette" of channel may all change

  int irowstride,orowstride;
  int width,widthx;
  int height;
  int pal;
  int error;
  int flags=0;

  register int i;

  guchar *src,*dst,*pixel_data;

  cairo_surface_t *surf;
  cairo_t *cairo;
  cairo_format_t cform=CAIRO_FORMAT_ARGB32;

  width=weed_get_int_value(channel,"width",&error);
  height=weed_get_int_value(channel,"height",&error);
  pal=weed_get_int_value(channel,"current_palette",&error);
  irowstride=weed_get_int_value(channel,"rowstrides",&error);

  widthx=width*4;

  orowstride=cairo_format_stride_for_width(cform,width);

  src=(guchar *)weed_get_voidptr_value(channel,"pixel_data",&error);

  pixel_data=(guchar *)weed_malloc(height*orowstride);

  if (pixel_data==NULL) return NULL;

  if (irowstride==orowstride) {
    weed_memcpy((void *)pixel_data,(void *)src,irowstride*height);
  } else {
    dst=pixel_data;
    for (i=0; i<height; i++) {
      weed_memcpy((void *)dst,(void *)src,widthx);
      weed_memset((void *)dst+widthx,0,widthx-orowstride);
      dst+=orowstride;
      src+=irowstride;
    }
  }

  if (weed_plant_has_leaf(channel,"flags")) flags=weed_get_int_value(channel,"flags",&error);
  if (!(flags&WEED_CHANNEL_ALPHA_PREMULT)) {
    // if we have post-multiplied alpha, pre multiply
    alpha_unpremult(pixel_data,widthx,height,orowstride,pal,FALSE);
  }

  surf=cairo_image_surface_create_for_data(pixel_data,
       cform,
       width, height,
       orowstride);

  if (surf==NULL) {
    weed_free(pixel_data);
    return NULL;
  }

  cairo=cairo_create(surf);
  cairo_surface_destroy(surf);
  weed_free(pixel_data);

  return cairo;
}



static void cairo_to_channel(cairo_t *cairo, weed_plant_t *channel) {
  // updates a weed_channel from a cairo_t
  cairo_surface_t *surface=cairo_get_target(cairo);

  cairo_format_t cform=CAIRO_FORMAT_ARGB32;

  int error;

  guchar *src,*dst,*pixel_data=(guchar *)weed_get_voidptr_value(channel,"pixel_data",&error);

  int flags=0;

  int height=weed_get_int_value(channel,"height",&error);
  int irowstride,orowstride=weed_get_int_value(channel,"rowstrides",&error);
  int width=weed_get_int_value(channel,"width",&error),widthx=width*4;

  register int i;

  // flush to ensure all writing to the image was done
  cairo_surface_flush(surface);

  src = cairo_image_surface_get_data(surface);

  irowstride=cairo_format_stride_for_width(cform,width);

  if (irowstride==orowstride) {
    weed_memcpy((void *)pixel_data,(void *)src,irowstride*height);
  } else {
    dst=pixel_data;
    for (i=0; i<height; i++) {
      weed_memcpy((void *)dst,(void *)src,widthx);
      weed_memset((void *)(dst+widthx),0,widthx-orowstride);
      dst+=orowstride;
      src+=irowstride;
    }
  }


  if (weed_plant_has_leaf(channel,"flags")) flags=weed_get_int_value(channel,"flags",&error);
  if (!(flags&WEED_CHANNEL_ALPHA_PREMULT)) {
    int pal=weed_get_int_value(channel,"current_palette",&error);

    // un-premultiply the alpha
    alpha_unpremult(pixel_data,widthx,height,orowstride,pal,TRUE);
  }


}


static char **fonts_available = NULL;
static int num_fonts_available = 0;

// work around some **weirdness** with strndup
static char *stringdup(const char *s, size_t n) {
  char *ret;
  size_t len=strlen(s);
  if (len>=n) len=n-1;
  ret=weed_malloc(len+1);
  weed_memcpy(ret,s,len);
  weed_memset(ret+len,0,1);
  return ret;
}



static size_t utf8len(char *text) {
  register size_t toffs=0;
  while (text[toffs]!='\0') {
    toffs+=mbtowc(NULL,&text[toffs],4);
  }
  return toffs;
}


static size_t utf8offs(char *text, int xoffs) {
  register size_t toffs=0;
  while (text[toffs]!='\0'&&xoffs>0) {
    toffs+=mbtowc(NULL,&text[toffs],4);
    xoffs--;
  }
  return toffs;
}


static size_t get_ascii_word_length(char *text) {
  // get length in words (non-spaces)
  register size_t toffs=0;
  int count=0;
  gboolean isaspace=TRUE;

  while (text[toffs]!='\0') {
    if (isspace(text[toffs])) isaspace=TRUE;
    else if (isaspace) {
      count++;
      isaspace=FALSE;
    }
    toffs++;
  }
  return count;
}


static size_t get_utf8_word_length(char *text) {
  // get length in words (non-spaces)
  register size_t toffs=0;
  int count=0;
  gboolean isaspace=TRUE;

  while (text[toffs]!='\0') {
    wchar_t pwc;
    toffs+=mbtowc(&pwc,&text[toffs],4);
    if (iswspace(pwc)) isaspace=TRUE;
    else if (isaspace) {
      count++;
      isaspace=FALSE;
    }
  }
  return count;
}


static pt_subst_t *get_nth_word_utf8(char *text, int idx) {
  // get nth word, return start (bytes) and length (bytes)
  // idx==0 for first word, etc

  register size_t toffs=0,xtoffs;
  gboolean isaspace=TRUE;

  pt_subst_t *subst=(pt_subst_t *)weed_malloc(sizeof(pt_subst_t));
  subst->start=0;

  while (text[toffs]!='\0') {
    wchar_t pwc;
    xtoffs=mbtowc(&pwc,&text[toffs],4);
    if (iswspace(pwc)) {
      if (idx==-1) {
        subst->length=toffs-subst->start;
        return subst;
      }
      isaspace=TRUE;
    } else if (isaspace) {
      if (--idx==-1) subst->start=toffs;
      isaspace=FALSE;
    }
    toffs+=xtoffs;
  }

  subst->length=toffs-subst->start;
  return subst;
}


static pt_subst_t *get_nth_word_ascii(char *text, int idx) {
  // get nth word, return start (bytes) and length (bytes)
  // idx==0 for first word, etc

  register size_t toffs=0;
  gboolean isaspace=TRUE;

  pt_subst_t *subst=(pt_subst_t *)weed_malloc(sizeof(pt_subst_t));
  subst->start=0;

  while (text[toffs]!='\0') {
    if (isspace(text[toffs])) {
      if (idx==-1) {
        subst->length=toffs-subst->start;
        return subst;
      }
      isaspace=TRUE;
    } else if (isaspace) {
      if (--idx==-1) subst->start=toffs;
      isaspace=FALSE;
    }
    toffs++;
  }

  subst->length=toffs-subst->start;
  return subst;
}




static void getlsize(PangoLayout *layout, double *pw, double *ph) {
  // calculate width and height of layout
  int w_, h_;
  pango_layout_get_size(layout, &w_, &h_);
  if (pw)
    *pw = ((double)w_)/PANGO_SCALE;
  if (ph)
    *ph = ((double)h_)/PANGO_SCALE;
}


static inline void pt_set_alarm(sdata_t *sdata, int delta) {
  sdata->alarm=FALSE;
  if (delta<0) sdata->alarm_time=-1.;
  else sdata->alarm_time=sdata->timer+(float)delta/1000.;
}


static void setxypos(double dwidth, double dheight, double x, double y, int *x_text, int *y_text) {
  // set top left corner offset given center point and glyph dimensions
  *x_text=x-dwidth/2.+.5;
  *y_text=y-dheight/2.+.5;
}




// set font size
static void set_font_size(PangoLayout *layout, PangoFontDescription *font, int font_size) {
  pango_font_description_set_size(font, font_size*PANGO_SCALE);
  pango_layout_set_font_description(layout, font);
}


static void anim_letter(pt_letter_data_t *ldt) {
  // update velocities and positions

  ldt->xvel+=ldt->xaccel;
  ldt->yvel+=ldt->yaccel;
  ldt->zvel+=ldt->zaccel;
  ldt->rotvel+=ldt->rotaccel;

  ldt->xpos+=ldt->xvel;
  ldt->ypos+=ldt->yvel;
  ldt->zpos+=ldt->zvel;
  ldt->rot+=ldt->rotvel;
}


static void colour_copy(rgb_t *col1, rgb_t *col2) {
  // copy col2 to col1
  weed_memcpy(col1,col2,sizeof(rgb_t));
}

static inline double rand_angle(void) {
  // return a random double between 0. and 2*M_PI
  return (double)rand()/(double)RAND_MAX*2.*M_PI;
}


static int getrandi(int min, int max) {
  return rand()%(max-min)+min;
}


static pt_letter_data_t *letter_data_create(int len) {
  pt_letter_data_t *ldt=(pt_letter_data_t *)weed_malloc(sizeof(pt_letter_data_t)*len);
  return ldt;
}


static void letter_data_free(sdata_t *sdata) {
  weed_free(sdata->letter_data);
  sdata->letter_data=NULL;
}

static void rotate_text(cairo_t *cairo, PangoLayout *layout, int x_center, int y_center, double radians) {
  cairo_translate(cairo, x_center, y_center);
  cairo_rotate(cairo, radians);

  /* Inform Pango to re-layout the text with the new transformation */
  pango_cairo_update_layout(cairo, layout);
}


static void proctext(sdata_t *sdata, weed_timecode_t tc, char *xtext, cairo_t *cairo, PangoLayout *layout,
                     PangoFontDescription *font, int width, int height) {
  double dwidth,dheight;
  double radX,radY;

  switch (sdata->mode) {
  case (PT_WORD_COALESCE):

    if (sdata->timer==0.) {
      sdata->start=-1;
      sdata->length=1;
      sdata->tmode=PT_WORD_MODE;
    }

    if (sdata->start>-1) {
      set_font_size(layout,font,64);
      // get pixel size of word
      getlsize(layout, &dwidth, &dheight);

      setxypos(dwidth,dheight,width/2,height/2,&sdata->x_text,&sdata->y_text);

      sdata->dbl1-=sdata->dbl1/3.5+.5; // blur shrink
      sdata->dbl2+=.07;
      sdata->fg_alpha=sdata->dbl2;
    }

    if (sdata->alarm) {
      pt_subst_t *xsubst;
      char nxlast;

      sdata->bool1=!sdata->bool1;
      sdata->dbl1=300.; // blur start
      sdata->fg_alpha=sdata->dbl2=0.;

      if (sdata->length>0) {
        sdata->start++;
        if (sdata->start>=sdata->wlength) {
          sdata->start=0;
          sdata->length=0;
          sdata->dbl1=0.;
          pt_set_alarm(sdata,1000); // milliseconds
        }
      } else {
        sdata->length=1;
        sdata->bool1=TRUE;
      }

      if (sdata->length>0) {
        if (sdata->start==sdata->wlength-1) {
          pt_set_alarm(sdata,1000); // milliseconds
        } else {
          // peek at last char of next word
          if (sdata->text_type==TEXT_TYPE_ASCII) {
            xsubst=get_nth_word_ascii(sdata->text,sdata->start);
          } else {
            xsubst=get_nth_word_utf8(sdata->text,sdata->start);
          }

          nxlast=sdata->text[xsubst->start+xsubst->length-1];

          weed_free(xsubst);

          if (nxlast=='.'||nxlast=='!'||nxlast=='?') pt_set_alarm(sdata,1000); // milliseconds
          else if (nxlast==',') pt_set_alarm(sdata,700); // milliseconds
          else if (nxlast==';') pt_set_alarm(sdata,800); // milliseconds
          else pt_set_alarm(sdata,500); // milliseconds
        }
      }
    }

    break;

  case (PT_LETTER_STARFIELD):
    if (sdata->timer==0.) {
      sdata->start=0;
      sdata->length=0;

      sdata->tmode=PT_LETTER_MODE;

      sdata->letter_data=letter_data_create(sdata->tlength);
    }

    if (sdata->length>0) {
      pt_letter_data_t *ldt=&sdata->letter_data[sdata->count];
      double dist=ldt->zpos;
      if (dist>1.) {
        set_font_size(layout,font,1024./dist);

        // get pixel size of letter/word
        getlsize(layout, &dwidth, &dheight);

        sdata->x_text=ldt->xpos/dist+(double)width/2.;
        sdata->y_text=ldt->ypos/dist+(double)height/2.;

        setxypos(dwidth,dheight,sdata->x_text,sdata->y_text,&sdata->x_text,&sdata->y_text);

        sdata->fg_alpha=dist/100.+.5;

        anim_letter(ldt);

        colour_copy(&sdata->fg,&ldt->colour);

      } else pango_layout_set_text(layout, "", -1);

    }

    if (sdata->alarm) {
      // set values for next letter
      pt_letter_data_t *ldt=&sdata->letter_data[sdata->length];
      double angle=rand_angle();

      ldt->xpos=ldt->ypos=ldt->rot=ldt->rotvel=0.;
      ldt->zpos=80.+sdata->length;

      if (ldt->zpos>100.) ldt->zpos=100.;

      ldt->xvel=sin(angle)*16.;
      ldt->yvel=cos(angle)*16.;
      ldt->zvel=-2.;

      ldt->xaccel=ldt->yaccel=ldt->zaccel=ldt->rotaccel=0.;

      if (sdata->length==0||!strncmp(&sdata->text[utf8offs(sdata->text,sdata->length)]," ",1)) {
        ldt->colour.red=getrandi(60,255);
        ldt->colour.green=getrandi(60,255);
        ldt->colour.blue=getrandi(60,255);
      } else {
        colour_copy(&ldt->colour,&(sdata->letter_data[sdata->length-1].colour));
      }

      sdata->length++;

      if (sdata->length<sdata->tlength) pt_set_alarm(sdata,400); // milliseconds
      else pt_set_alarm(sdata,-1);
    }


    break;

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

    set_font_size(layout,font,64);

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

    set_font_size(layout,font,2560./(sdata->count+19.));

    // get pixel size of letter/word
    getlsize(layout, &dwidth, &dheight);

    // expansion factor
    if (sdata->int1<2) {
      sdata->dbl3=1.;
      sdata->bool1=FALSE;
    } else sdata->dbl3+=.0001;

    // set x_text, y_text
    if (!sdata->count) {
      sdata->dbl1=radX=width*.45*sdata->dbl3;
      sdata->dbl2=radY=height*.45*sdata->dbl3;
    } else {
      if (!sdata->bool1) {
        sdata->dbl1=radX=sdata->dbl1*.98;
        sdata->dbl2=radY=sdata->dbl2*.98;
      } else {
        sdata->dbl1=radX=sdata->dbl1*.97;
        sdata->dbl2=radY=sdata->dbl2*.97;
      }
    }

    if (sdata->bool1)
      setxypos(dwidth,dheight,width/2+sin(sdata->count/4.+(sdata->dbl3-1.)*9.)*radX,
               height/2-cos(-sdata->count/4.-(sdata->dbl3-1.)*8.)*radY,&sdata->x_text,&sdata->y_text);

    else
      setxypos(dwidth,dheight,width/2+sin(sdata->count/4.+(sdata->dbl3-1.)*8.)*radX,
               height/2-cos(-sdata->count/4.-(sdata->dbl3-1.)*8.)*radY,&sdata->x_text,&sdata->y_text);

    if (!strncmp(xtext,".",1)) sdata->int1++;

    if (sdata->alarm) {
      pt_set_alarm(sdata,250); // milliseconds
      if (sdata->start+sdata->length<sdata->tlength) {
        // add an extra letter
        sdata->length++;
      } else {
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
        } else pt_set_alarm(sdata,50); // milliseconds

      }

    }
    break;

  } // end switch

}


int puretext_init(weed_plant_t *inst) {
  int error,fd;
  gboolean erropen=FALSE;
  weed_plant_t **in_params=weed_get_plantptr_array(inst,"in_parameters",&error);
  sdata_t *sdata;

  char buff[65536];
  size_t b_read;

  char *textfile=weed_get_string_value(in_params[P_TEXT],"value",&error);

  // open file and read in text

  if ((fd=open(textfile,O_RDONLY))==-1) {
    erropen=TRUE;
    g_snprintf(buff,512,"Error opening file %s",textfile);
  }

  weed_free(textfile);
  weed_free(in_params);

  sdata=(sdata_t *)weed_malloc(sizeof(sdata_t));

  if (sdata==NULL) return WEED_ERROR_MEMORY_ALLOCATION;

  sdata->timer=-1;
  sdata->last_tc=0;
  sdata->alarm_time=0.;
  sdata->alarm=FALSE;

  sdata->text_type=TEXT_TYPE_UTF8;

  if (!erropen) {
    b_read=read(fd,buff,65535);
    memset(buff+b_read,0,1);
    close(fd);
  }

  sdata->text=strdup(buff);

  sdata->start=sdata->length=0;

  if (sdata->text_type==TEXT_TYPE_ASCII) {
    sdata->tlength=strlen(sdata->text);
    sdata->wlength=get_ascii_word_length(sdata->text);
  } else {
    sdata->tlength=utf8len(sdata->text);
    sdata->wlength=get_utf8_word_length(sdata->text);
  }

  sdata->int1=0;
  sdata->dbl1=sdata->dbl2=sdata->dbl3=-1.;
  sdata->bool1=FALSE;

  sdata->letter_data=NULL;

  sdata->mode=1;

  weed_set_voidptr_value(inst,"plugin_internal",sdata);

  return WEED_NO_ERROR;
}



int puretext_deinit(weed_plant_t *inst) {
  int error;
  sdata_t *sdata=(sdata_t *)weed_get_voidptr_value(inst,"plugin_internal",&error);

  if (sdata!=NULL) {
    if (sdata->letter_data!=NULL) letter_data_free(sdata);
    if (sdata->text!=NULL) free(sdata->text);
    free(sdata);
  }

  return WEED_NO_ERROR;

}



int puretext_process(weed_plant_t *inst, weed_timecode_t tc) {
  int error;

  weed_plant_t *in_channel=weed_get_plantptr_value(inst,"in_channels",&error);
  weed_plant_t *out_channel=weed_get_plantptr_value(inst,"out_channels",&error);
  weed_plant_t **in_params=weed_get_plantptr_array(inst,"in_parameters",&error);

  sdata_t *sdata=(sdata_t *)weed_get_voidptr_value(inst,"plugin_internal",&error);

  guchar *bgdata=NULL;

  pt_subst_t *xsubst;

  cairo_t *cairo;

  guchar *dst=weed_get_voidptr_value(out_channel,"pixel_data",&error);

  size_t toffs;

  //int alpha_threshold = 0;

  int orowstride=weed_get_int_value(out_channel,"rowstrides",&error);

  int width=weed_get_int_value(out_channel,"width",&error);
  int height=weed_get_int_value(out_channel,"height",&error);

  int mode=weed_get_int_value(in_params[P_MODE],"value",&error);

  register int i,j;


  weed_free(in_params); // must weed free because we got an array

  if (mode!=sdata->mode) {
    sdata->timer=-1.;
    sdata->mode=mode;
    sdata->alarm_time=0.;
    if (sdata->letter_data!=NULL) letter_data_free(sdata);
  }

  // set timer data and alarm status
  if (sdata->timer==-1.||tc<sdata->last_tc) {
    sdata->timer=0.;
    sdata->length=0;
  } else {
    sdata->timer+=(double)(tc-sdata->last_tc)/100000000.;
    sdata->alarm=FALSE;
  }

  if (sdata->alarm_time>-1.&&sdata->timer>=sdata->alarm_time) {
    sdata->alarm_time=-1.;
    sdata->alarm=TRUE;
  }

  sdata->last_tc=tc;

  sdata->count=0;

  if (sdata->mode==PT_WORD_COALESCE) {
    // backup original data
    bgdata=weed_malloc(height*orowstride);
    weed_memcpy(bgdata,dst,height*orowstride);
  }



  // THINGS TO TO WITH TEXTS AND PANGO
  if ((!in_channel) || (in_channel == out_channel))
    cairo = channel_to_cairo(out_channel);
  else
    cairo = channel_to_cairo(in_channel);

  if (cairo) {
    // TODO - get real offset of start in bytes

    if (sdata->text_type==TEXT_TYPE_ASCII) {
      toffs=sdata->start;
    } else {
      toffs=utf8offs(sdata->text,sdata->start);
    }

    // loop from start char to end char
    for (i=sdata->start; i<sdata->start+(sdata->length==0?1:sdata->length); i++) {
      PangoLayout *layout = pango_cairo_create_layout(cairo);
      if (layout) {
        PangoFontDescription *font;
        char *xtext;

        font = pango_font_description_new();

        // send letter or word to proctext

        //if((num_fonts_available) && (fontnum >= 0) && (fontnum < num_fonts_available) && (fonts_available[fontnum]))
        pango_font_description_set_family(font, "Serif");

        if (sdata->length==0) {
          xtext=weed_malloc(1);
          weed_memset(xtext,0,1);
        } else {
          switch (sdata->tmode) {
          case PT_LETTER_MODE:
            // letter mode
            if (sdata->text_type==TEXT_TYPE_ASCII) {
              xtext=stringdup(&sdata->text[toffs],1);
              toffs++;
            } else {
              int xlen=mbtowc(NULL,&sdata->text[toffs],4);
              xtext=stringdup(&sdata->text[toffs],xlen);
              toffs+=xlen;
            }
            break;

          case PT_WORD_MODE:
            // word mode
            if (sdata->text_type==TEXT_TYPE_ASCII) {
              xsubst=get_nth_word_ascii(sdata->text,i);
            } else {
              xsubst=get_nth_word_utf8(sdata->text,i);
            }
            xtext=stringdup(&sdata->text[xsubst->start],xsubst->length);

            weed_free(xsubst);
            break;
          default:
            // TODO - line mode and all mode
            xtext=weed_malloc(1);
            weed_memset(xtext,0,1);
            break;
          }
        }
        pango_layout_set_font_description(layout, font);
        pango_layout_set_text(layout, (char *)xtext, -1);

        // default colour - opaque white
        sdata->fg.red=sdata->fg.green=sdata->fg.blue=255.;
        sdata->fg_alpha=1.;

        cairo_save(cairo);

        // get size, position, and colour
        proctext(sdata,tc,(char *)xtext,cairo,layout,font,width,height);

        free(xtext);

        cairo_move_to(cairo, sdata->x_text, sdata->y_text);

        cairo_set_source_rgba(cairo,sdata->fg.red/255.0, sdata->fg.green/255.0, sdata->fg.blue/255.0,
                              sdata->fg_alpha);

        pango_cairo_show_layout(cairo, layout);

        cairo_restore(cairo);

        pango_font_description_free(font);
        g_object_unref(layout);
      }

      sdata->count++;

    } // end loop

    cairo_to_channel(cairo,out_channel);
    cairo_destroy(cairo);
  }


  if (sdata->mode==PT_WORD_COALESCE) {
    if (sdata->dbl1>0.) {
      guchar *b_data=bgdata;
      int width3=width*3;
      guchar *dstx=dst=weed_get_voidptr_value(out_channel,"pixel_data",&error);

      for (i=0; i<height; i++) {
        for (j=0; j<width3; j+=3) {
          if (dst[j]!=b_data[j]||dst[j+1]!=b_data[j+1]||dst[j+2]!=b_data[j+2]) {
            // move point by sdata->dbl1 pixels
            double angle=rand_angle();
            int x=j/3+sin(angle)*sdata->dbl1;
            int y=i+cos(angle)*sdata->dbl1;
            if (x>0&&x<width&&y>0&&y<height) {
              // blur 1 pixel
              memcpy(&dstx[y*orowstride+x*3],&dst[j],3);
              // protect blurred pixel
              if (y>=i) memcpy(&bgdata[y*orowstride+x*3],&dst[j],3);
            }
            // replace original pixel
            memcpy(&dst[j],&b_data[j],3);
          }
        }
        dst+=orowstride;
        b_data+=orowstride;
      }
    }
    weed_free(bgdata);
  }

  return WEED_NO_ERROR;
}





/*
static int font_compare(const void *p1, const void *p2) {
  const char *s1 = (const char *)(*(char **)p1);
  const char *s2 = (const char *)(*(char **)p2);
  char *u1 = g_utf8_casefold(s1, -1);
  char *u2 = g_utf8_casefold(s2, -1);
  int ret = strcmp(u1, u2);
  g_free(u1);
  g_free(u2);
  return ret;
  }*/

weed_plant_t *weed_setup(weed_bootstrap_f weed_boot) {
  weed_plant_t *plugin_info=weed_plugin_info_init(weed_boot,num_versions,api_versions);

  if (plugin_info!=NULL) {
    weed_plant_t *in_params[P_END+1],*gui;
    weed_plant_t *filter_class;
    PangoContext *ctx;

    const char *modes[]= {"Spiral text","Spinning letters","Letter starfield","Word coalesce",NULL};
    char *rfx_strings[]= {"special|fileread|0|"};

    char *deftextfile;

    int palette_list[2];
    weed_plant_t *in_chantmpls[2];
    weed_plant_t *out_chantmpls[2];

    int flags,error;

    if (is_big_endian())
      palette_list[0]=WEED_PALETTE_ARGB32;
    else
      palette_list[0]=WEED_PALETTE_BGRA32;

    palette_list[1]=WEED_PALETTE_END;

    in_chantmpls[0]=weed_channel_template_init("in channel 0",0,palette_list);
    in_chantmpls[1]=NULL;

    out_chantmpls[0]=weed_channel_template_init("out channel 0",WEED_CHANNEL_CAN_DO_INPLACE,palette_list);
    out_chantmpls[1]=NULL;

    init_unal();


    // this section contains code
    // for configure fonts available
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
          fonts_available = (char **)weed_malloc((num+1)*sizeof(char *));
          if (fonts_available) {
            register int i;
            num_fonts_available = num;
            for (i = 0; i < num; ++i) {
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

    deftextfile=g_build_filename(g_get_home_dir(), "livestext.txt", NULL);

    in_params[P_TEXT]=weed_text_init("textfile","_Text file",deftextfile);
    gui=weed_parameter_template_get_gui(in_params[P_TEXT]);
    weed_set_int_value(gui,"maxchars",80); // for display only - fileread will override this
    flags=0;
    if (weed_plant_has_leaf(in_params[P_TEXT],"flags"))
      flags=weed_get_int_value(in_params[P_TEXT],"flags",&error);
    flags|=WEED_PARAMETER_REINIT_ON_VALUE_CHANGE;
    weed_set_int_value(in_params[P_TEXT],"flags",flags);

    in_params[P_MODE]=weed_string_list_init("mode","Effect _mode",0,modes);
    flags=0;
    if (weed_plant_has_leaf(in_params[P_MODE],"flags"))
      flags=weed_get_int_value(in_params[P_MODE],"flags",&error);
    flags|=WEED_PARAMETER_REINIT_ON_VALUE_CHANGE;
    weed_set_int_value(in_params[P_MODE],"flags",flags);
    in_params[P_END]=NULL;

    g_free(deftextfile);

    filter_class=weed_filter_class_init("puretext","Salsaman & Aleksej Penkov",1,0,&puretext_init,&puretext_process,NULL,
                                        in_chantmpls,out_chantmpls,in_params,NULL);

    gui=weed_filter_class_get_gui(filter_class);
    weed_set_string_value(gui,"layout_scheme","RFX");
    weed_set_string_value(gui,"rfx_delim","|");
    weed_set_string_array(gui,"rfx_strings",1,rfx_strings);

    weed_plugin_info_add_filter_class(plugin_info,filter_class);

    weed_set_int_value(plugin_info,"version",package_version);

  }

  return plugin_info;
}



void weed_desetup(void) {
  // clean up what we reserve for font family names
  if (num_fonts_available && fonts_available) {
    int i;
    for (i = 0; i < num_fonts_available; ++i) {
      free((void *)fonts_available[i]);
    }
    weed_free((void *)fonts_available);
  }
  num_fonts_available = 0;
  fonts_available = NULL;
}


