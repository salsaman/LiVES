// negate.c
// weed plugin
// (c) G. Finch (salsaman) 2012
//
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

static int num_versions=1; // number of different weed api versions supported
static int api_versions[]= {131}; // array of weed api versions supported in plugin, in order of preference (most preferred first)

static int package_version=1; // version of this package

//////////////////////////////////////////////////////////////////

#ifdef HAVE_SYSTEM_WEED_PLUGIN_H
#include <weed/weed-plugin.h> // optional
#else
#include "../../../libweed/weed-plugin.h" // optional
#endif

#include "../weed-utils-code.c" // optional
#include "../weed-plugin-utils.c" // optional

/////////////////////////////////////////////////////////////

#include <gdk/gdk.h>

#include <stdio.h>
#include <string.h>
#include <math.h>

#define MAX_ELEMS 500

typedef struct {
  float len;
  int j;
  int i;
  float x;
  float y;
} list_ent;


// TODO - make non-static
static list_ent xlist[MAX_ELEMS];
static cairo_user_data_key_t crkey;



static gboolean is_big_endian() {
  int testint = 0x12345678;
  char *pMem=&testint;

  if (pMem[0] == 0x78) {
    return FALSE;
  }
  return TRUE;
}



static int unal[256][256];
static int al[256][256];

static gboolean unal_inited=FALSE;

static void init_unal(void) {
  // premult to postmult and vice-versa

  register int i,j;

  for (i=0; i<256; i++) { //alpha val
    for (j=0; j<256; j++) { // val to be converted
      unal[i][j]=(float)j*255./(float)i;
      al[i][j]=(float)j*(float)i/255.;
    }
  }
  unal_inited=TRUE;
}



void clear_xlist(void) {
  register int i;

  for (i=0; i<MAX_ELEMS; i++) {
    xlist[i].len=0.;
  }
}



void add_to_list(float len, int i, int j, float x, float y) {
  register int k,l;

  for (k=0; k<MAX_ELEMS; k++) {
    if (len>xlist[k].len) {
      // shift existing elements
      for (l=MAX_ELEMS-1; l<k; l--) {
        if (xlist[l-1].len>0.) {
          xlist[l].len=xlist[l-1].len;
          xlist[l].i=xlist[l-1].i;
          xlist[l].j=xlist[l-1].j;
          xlist[l].x=xlist[l-1].x;
          xlist[l].y=xlist[l-1].y;
        }
      }
      xlist[k].len=len;
      xlist[k].i=i;
      xlist[k].j=j;
      xlist[k].x=x;
      xlist[k].y=y;
      break;
    }
  }
}



void alpha_premult(weed_plant_t *channel) {
  // premultply alpha - this only occurs when going from palette with alpha to one without

  int error;
  int widthx;
  int alpha;
  int flags=0;
  int width=weed_get_int_value(channel,"width",&error);
  int height=weed_get_int_value(channel,"height",&error);
  int rowstride=weed_get_int_value(channel,"rowstrides",&error);

  unsigned char *ptr;

  register int i,j,p;

  if (!unal_inited) init_unal();

  widthx=width*4;

  ptr=(unsigned char *)weed_get_voidptr_value(channel,"pixel_data",&error);

  for (i=0; i<height; i++) {
    for (j=0; j<widthx; j+=4) {
      alpha=ptr[j];
      for (p=1; p<4; p++) {
        ptr[j+p]=al[alpha][ptr[j+p]];
      }
    }
    ptr+=rowstride;
  }

  if (weed_plant_has_leaf(channel,"flags"))
    flags=weed_get_int_value(channel,"flags",&error);

  flags|=WEED_CHANNEL_ALPHA_PREMULT;
  weed_set_int_value(channel,"flags",flags);
}



static void pdfree(void *data) {
  weed_free(data);
}

static cairo_t *channel_to_cairo(weed_plant_t *channel) {
  // convert a weed channel to cairo

  int irowstride,orowstride;
  int width,widthx;
  int height,pal;
  int error;

  register int i;

  guchar *src,*dst;

  cairo_surface_t *surf;
  cairo_t *cairo=NULL;
  cairo_format_t cform;

  void *pixel_data;

  width=weed_get_int_value(channel,"width",&error);

  pal=weed_get_int_value(channel,"current_palette",&error);
  if (pal==WEED_PALETTE_A8) {
    cform=CAIRO_FORMAT_A8;
    widthx=width;
  } else if (pal==WEED_PALETTE_A1) {
    cform=CAIRO_FORMAT_A1;
    widthx=width>>3;
  } else {
    cform=CAIRO_FORMAT_ARGB32;
    widthx=width<<2;
  }

  height=weed_get_int_value(channel,"height",&error);

  irowstride=weed_get_int_value(channel,"rowstrides",&error);

  orowstride=cairo_format_stride_for_width(cform,width);

  src=(guchar *)weed_get_voidptr_value(channel,"pixel_data",&error);

  dst=pixel_data=(guchar *)weed_malloc(height*orowstride);
  if (pixel_data==NULL) return NULL;

  if (irowstride==orowstride) {
    weed_memcpy(dst,src,height*irowstride);
  } else {
    for (i=0; i<height; i++) {
      weed_memcpy(dst,src,widthx);
      weed_memset(dst+widthx,0,widthx-orowstride);
      dst+=orowstride;
      src+=irowstride;
    }
  }

  if (cform==CAIRO_FORMAT_ARGB32) {
    int flags=0;
    if (weed_plant_has_leaf(channel,"flags")) flags=weed_get_int_value(channel,"flags",&error);
    if (!(flags&WEED_CHANNEL_ALPHA_PREMULT)) {
      // if we have post-multiplied alpha, pre multiply
      alpha_premult(channel);
      flags|=WEED_CHANNEL_ALPHA_PREMULT;
      weed_set_int_value(channel,"flags",flags);
    }
  }


  surf=cairo_image_surface_create_for_data(pixel_data,
       cform,
       width, height,
       orowstride);

  cairo=cairo_create(surf);
  cairo_surface_destroy(surf);

  cairo_set_user_data(cairo, &crkey, pixel_data, pdfree);

  return cairo;
}






static gboolean cairo_to_channel(cairo_t *cairo, weed_plant_t *channel) {
  // updates a weed_channel from a cairo_t
  // unlike doing this the other way around
  // the cairo is not destroyed (data is copied)
  void *dst,*src;

  int width,height,irowstride,orowstride,widthx,pal;
  int flags=0,error;

  cairo_surface_t *surface=cairo_get_target(cairo);

  register int i;

  // flush to ensure all writing to the image was done
  cairo_surface_flush(surface);

  dst=weed_get_voidptr_value(channel,"pixel_data",&error);
  if (dst==NULL) return FALSE;

  src = cairo_image_surface_get_data(surface);
  height = cairo_image_surface_get_height(surface);
  width = cairo_image_surface_get_width(surface);
  irowstride = cairo_image_surface_get_stride(surface);

  orowstride=weed_get_int_value(channel,"rowstrides",&error);
  pal=weed_get_int_value(channel,"current_palette",&error);

  if (irowstride==orowstride) {
    weed_memcpy(dst,src,height*orowstride);
  } else {
    widthx=width*4;
    if (pal==WEED_PALETTE_A8) widthx=width;
    else if (pal==WEED_PALETTE_A1) widthx=width>>3;

    for (i=0; i<height; i++) {
      weed_memcpy(dst,src,widthx);
      weed_memset(dst+widthx,0,widthx-orowstride);
      dst+=orowstride;
      src+=irowstride;
    }
  }

  if (pal!=WEED_PALETTE_A8 && pal!=WEED_PALETTE_A1) {
    if (weed_plant_has_leaf(channel,"flags"))
      flags=weed_get_int_value(channel,"flags",&error);

    flags|=WEED_CHANNEL_ALPHA_PREMULT;
    weed_set_int_value(channel,"flags",flags);
  }

  return TRUE;
}




/////////////////////////////////////////////////////////////

enum {
  MD_GRID,
  MD_LARGEST
};



static void draw_arrow(cairo_t *cr, int i, int j, float x, float y) {
  // draw arrow from point i-x,j-y to i, j
  int stx=i-(x+.5);
  int sty=j-(y+.5);

  int len=sqrt(x*x+y*y);

  cairo_set_line_width(cr,4.);
  cairo_set_source_rgb(cr,1.,0.,0.);

  cairo_move_to(cr,stx,sty);
  cairo_line_to(cr,i,j);

  cairo_arc(cr,i,j,len/4.,0,M_PI*2.);
  cairo_stroke(cr);


}


int vector_visualiser_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  cairo_t *cr;

  int error;

  weed_plant_t **in_channels=weed_get_plantptr_array(inst,"in_channels",&error);
  weed_plant_t **in_params=weed_get_plantptr_array(inst,"in_parameters",&error);

  weed_plant_t *out_channel=weed_get_plantptr_value(inst,"out_channels",&error);

  float *alpha0=(float *)weed_get_voidptr_value(in_channels[1],"pixel_data",&error);
  float *alpha1=(float *)weed_get_voidptr_value(in_channels[2],"pixel_data",&error);


  float x,y,scale=1.;

  int mode=MD_GRID;

  int irow0=weed_get_int_value(in_channels[1],"rowstrides",&error)>>2;
  int irow1=weed_get_int_value(in_channels[2],"rowstrides",&error)>>2;

  int width=weed_get_int_value(out_channel,"width",&error);
  int height=weed_get_int_value(out_channel,"height",&error);


  int enabled=weed_get_boolean_value(in_params[0],"value",&error);


  register int i,j;

  weed_free(in_params);

  if (enabled==WEED_FALSE) {
    return WEED_NO_ERROR;
  }

  cr=channel_to_cairo(in_channels[0]);

  switch (mode) {
  case MD_GRID: {
    int smwidth=width/20;
    int smheight=height/20;

    if (smwidth<1) smwidth=1;
    if (smheight<1) smheight=1;

    for (i=smheight; i<height; i+=smheight*2) {
      for (j=smwidth; j<width; j+=smwidth*2) {
        x=alpha0[i*irow0+j];
        y=alpha1[i*irow1+j];
        draw_arrow(cr,j,i,x*scale,y*scale);
      }
    }
  }
  break;

  case MD_LARGEST: {
    float len;
    clear_xlist();

    for (i=0; i<height; i++) {
      for (j=0; j<width; j++) {
        x=alpha0[i*irow0+j];
        y=alpha1[i*irow1+j];
        len=sqrt(x*x+y*y);
        if (len>xlist[MAX_ELEMS-1].len) add_to_list(len,i,j,x,y);
      }
    }

    for (i=0; i<MAX_ELEMS; i++) {
      if (xlist[i].len>0.) draw_arrow(cr,xlist[i].j,xlist[i].i,xlist[i].x*scale,xlist[i].y*scale);
    }

  }
  break;

  default:
    break;


  }

  cairo_to_channel(cr,out_channel);
  cairo_destroy(cr);

  weed_free(in_channels);

  return WEED_NO_ERROR;
}




weed_plant_t *weed_setup(weed_bootstrap_f weed_boot) {
  weed_plant_t *plugin_info=weed_plugin_info_init(weed_boot,num_versions,api_versions);
  if (plugin_info!=NULL) {

    int apalette_list[]= {WEED_PALETTE_AFLOAT,WEED_PALETTE_END};

    int vpalette_list[]= {WEED_PALETTE_BGRA32,WEED_PALETTE_END};

    weed_plant_t *in_chantmpls[]= {
      weed_channel_template_init("video in",0,vpalette_list),
      weed_channel_template_init("X-plane",0,apalette_list),
      weed_channel_template_init("Y-plane",0,apalette_list),NULL
    };

    weed_plant_t *out_chantmpls[]= {weed_channel_template_init("video out",WEED_CHANNEL_CAN_DO_INPLACE,vpalette_list),NULL};

    weed_plant_t *in_params[]= {weed_switch_init("enabled","_Enabled",WEED_TRUE),NULL};

    weed_plant_t *filter_class=weed_filter_class_init("cairo vector visualiser","salsaman",1,0,
                               NULL,&vector_visualiser_process,NULL,
                               in_chantmpls,out_chantmpls,
                               in_params,NULL);

    weed_plant_t *gui=weed_parameter_template_get_gui(in_params[0]);
    weed_set_boolean_value(gui,"hidden",WEED_TRUE);

    weed_plugin_info_add_filter_class(plugin_info,filter_class);

    if (is_big_endian()) {
      weed_set_int_value(in_chantmpls[0],"palette_list",WEED_PALETTE_ARGB32);
      weed_set_int_value(out_chantmpls[0],"palette_list",WEED_PALETTE_ARGB32);
    }

    weed_set_int_value(plugin_info,"version",package_version);

  }

  return plugin_info;
}

